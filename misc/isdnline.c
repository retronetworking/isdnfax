/* $Id$
******************************************************************************

   Fax program for ISDN.
   Communicate with the /dev/ttyI* devices to transfer audio over ISDN

   Copyright (C) 1999-2000 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************************
*/

/* Utility-functions to administrate the ISDN-audio subsystem and
 * general transfering of audio over ISDN.  The /dev/ttyI* devices
 * are used to access the underlying device-drivers.  Audio over
 * the ISDN-ttys must be supported by the low-level ISDN-driver used.
 * The set of functions defined in this file are designed to co-exist
 * and work with the hardware abstraction layer in 'hardware-driver.c'
 */

#define _GNU_SOURCE

#define FSM_DEBUG_STATES

#include <stdio.h>
#include <time.h>
#include <termios.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ifax/ifax.h>
#include <ifax/alaw.h>
#include <ifax/misc/isdnline.h>
#include <ifax/misc/malloc.h>
#include <ifax/misc/statemachine.h>
#include <ifax/misc/hardware-driver.h>
#include <ifax/misc/iobuffer.h>
#include <ifax/misc/timers.h>

/* These are some standard sequences we are searching for among AT commands
 * and their responses.  Some of these sequences are known as 'async'
 * messages, since thay can appear seemingly from nowhere (RING indication).
 */
#define ISDN_MESSAGE_MASK				0x7fffff00
#define ISDN_MESSAGE_OK					0x00000100
#define ISDN_MESSAGE_CRLF				0x00000200
#define ISDN_MESSAGE_ERROR				0x00000400
#define ISDN_MESSAGE_VCON				0x00000800
#define ISDN_MESSAGE_BUSY				0x00001000
#define ISDN_MESSAGE_NOCARRIER				0x00002000
#define ISDN_MESSAGE_RING				0x00004000
#define ISDN_MESSAGE_RING2				0x00008000


/* Initialize an 'fd_set' to work with the ISDN devices.  We only need to
 * know when there are anything to read.  When there is something to read,
 * we will write tha same ammount.  This works since ISDN has input and
 * output samples clocked by the same clock.  We still have to handle
 * all the fd-sets since other device types may need them.
 */

static void isdn_prepare_select(struct HardwareHandle *hh, int *maxfd,
				fd_set *rfd, fd_set *wfd, fd_set *efd)
{
	struct IsdnHandle *ih = hh->private;

	if ( ih->fd < 0 )
		return;

	if ( ih->fd > *maxfd )
		*maxfd = ih->fd;

	FD_SET(ih->fd,rfd);
}


/* When the isdn device has been configured, the following function is called
 * to carry out the initialization itself.  This function opens the device
 * and starts the state machine which will complete the initialization and
 * handle all further actions on the isdn device.
 */

static void isdn_initialize(struct HardwareHandle *hh)
{
	struct termios isdnsetting;
	struct IsdnHandle *ih = hh->private;

	if ( (ih->fd=open(ih->device,O_RDWR|O_NONBLOCK)) < 0 ) {
		ih->fd = -1;
		hh->error = 1;
		sprintf(hh->errormsg,"Can't open '%s': %s",ih->device,
			strerror(errno));
		return;
	}

	if ( tcgetattr(ih->fd,&isdnsetting) < 0 ) {
		hh->error = 1;
		sprintf(hh->errormsg,"Unable to tcgetattr for '%s'",
			ih->device);
		close(ih->fd);
		ih->fd = -1;
		return;
	}

	isdnsetting.c_iflag = 0;
	isdnsetting.c_oflag = 0;
	isdnsetting.c_lflag = 0;

	isdnsetting.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CLOCAL);
	isdnsetting.c_cflag |=  (CS8 | CREAD | HUPCL | CLOCAL | CRTSCTS);

	if ( tcsetattr(ih->fd, TCSANOW, &isdnsetting) < 0 ) {
		hh->error = 1;
		sprintf(hh->errormsg,"Unable to tcsetattr for '%s'",
			ih->device);
		close(ih->fd);
		ih->fd = -1;
		return;
	}
}


/* Read as much as possible from the ISDN-device and queue up in the
 * IsdnHandle structure.  The queue is in raw-format: interpretation
 * of DLE etc. is not done here.  If the fd_sets are supplied, they
 * are used to speed things up if possible (return immediately if no
 * data).
 */

static void isdn_service_select(struct HardwareHandle *hh, fd_set *rfd,
				fd_set *wfd, fd_set *efd)
{
	struct IsdnHandle *ih = hh->private;

	static hardware_state_t last_state = 999999;	/* none */

	static char *device_state[9] = {
		"UNKNOWN",
		"INITIALIZING",
		"FAILED",
		"IDLE",
		"RINGING",
		"CALLING",
		"ONLINE",
		"BUSY",
		"NOANSWER"
	};

	if ( hh->state != last_state ) {
		ifax_dprintf(DEBUG_DEBUG,"Changed state to %s\n",
			     device_state[hh->state]);
		last_state = hh->state;
	}

	if ( (rfd == 0 || FD_ISSET(ih->fd,rfd)) && !hh->error )
		iobuffer_read(ih->incomming_buffer,ih->fd);

	fsm_run(ih->smh);
}


/*
 * Write a set of voice samples to the ISDN device.  The samples are in the
 * form of an array, containing 16-bit signed, linear values.
 * The array is *modified* by this function to the linear values actually
 * transmitted.  This is to help an echo cancel function to work on the true
 * values as sent over the ISDN line to the remote end.
 */

#define ISDN_MAX_WRITE	4096

static void isdn_write_samples(struct HardwareHandle *hh,
			       ifax_sint16 *src, int cnt)
{
	struct IsdnHandle *ih = hh->private;
	ifax_uint8 *dst, wala;
	ifax_uint16 linear;
	ifax_uint8 tmp[2 * ISDN_MAX_WRITE];

	if ( cnt > ISDN_MAX_WRITE ) {
	  ifax_dprintf(DEBUG_ERROR,"Dropping %d samples in isdn_write_samples",
		       cnt - ISDN_MAX_WRITE);
	  cnt = ISDN_MAX_WRITE;
	}

	dst = &tmp[0];

	while ( cnt-- > 0 ) {

		linear = (ifax_uint16) *src;
		wala = sint2wala[linear>>4];
		*src++ = 8 * wala2sint[wala];	/* fill with quantized value */

		if ( wala == ISDNTTY_DLE )
			*dst++ = wala;
		*dst++ = wala;
	}

	iobuffer_fill(ih->outgoing_buffer, &tmp[0], dst - &tmp[0]);
}


/* To configure an isdn device, we need to know the isdn tty to use, and
 * what MSN (message subscriber number) that it should use.
 */

static void isdn_configure(struct HardwareHandle *hh, char *param, char *value)
{
	struct IsdnHandle *ih = hh->private;
	int len = strlen(value);

	if ( !strcmp(param,"msn") ) {
		if ( len >= ISDNMSN_SIZE ) {
			hh->error = 1;
			strcpy(hh->errormsg,"isdn-msn is too long");
			return;
		}
		strcpy(ih->ourmsn,value);
		return;
	}

	if ( !strcmp(param,"device") ) {
		if ( len >= ISDNDEVNME_SIZE ) {
			hh->error = 1;
			strcpy(hh->errormsg,"isdn-device is too long");
			return;
		}
		strcpy(ih->device,value);
		return;
	}

	hh->error = 1;
	sprintf(hh->errormsg,"Unrecognized configuration option '%s'",param);
}


/* The following function needs to be called before going to sleep on a
 * select, so that we fill the ISDN output buffer so that will not
 * underrun anytime soon.
 */

void isdn_service_readwrite(struct HardwareHandle *hh)
{
	struct IsdnHandle *ih = hh->private;
	
	iobuffer_write(ih->outgoing_buffer,ih->fd);
}


/* Trigger a dial to a certain phone number.  This function can only be
 * called when the hardware driver is in the IDLE state.
 */

void isdn_dial(struct HardwareHandle *hh, char *number)
{
	struct IsdnHandle *ih = hh->private;

	ifax_dprintf(DEBUG_INFO,"Dialing triggered for %s\n",number);

	if ( hh->state != IDLE ) {
		ifax_dprintf(DEBUG_ERROR,"Dialing out of place!\n");
		return;
	}

	if ( ih->request_dial )
		return;

	strcpy(ih->dialmsn,number);
	ih->request_dial = 1;
}


/* Allocate the Isdn-device and prepare for configuration and intialization
 * at a later stage.  If the variable 'error' in the IsdnHandle struct is
 * nonzero, the operation failed, and a description of the problem will be
 * in the 'errormsg' array. The ISDN-device is opened in NONBLOCK mode to
 * make things run smooth with multiple input sources (needs to use select
 * to do this).
 */

FSM_DEFSTATE(isdn_start_state)

void isdn_allocate(struct HardwareHandle *hh)
{
	struct IsdnHandle *ih;

	ih = ifax_malloc(sizeof(*ih),"Isdn hardware driver handle");
	hh->private = ih;

	ih->smh = fsm_allocate(1);			/* Needs one FSM */
	fsm_setup(ih->smh,0,512);			/* ... stack=512 */
	fsm_init(ih->smh,0,isdn_start_state,100,hh);	/* Initialize */

	ih->fd = -1;
	ih->device[0] = '\0';
	ih->ourmsn[0] = '\0';
	ih->remotemsn[0] = '\0';

	ih->incomming_buffer = iobuffer_allocate(512,"ISDN/I");
	ih->outgoing_buffer = iobuffer_allocate(512,"ISDN/O");

	hh->configure = isdn_configure;
	hh->initialize = isdn_initialize;

	hh->prepare_select = isdn_prepare_select;
	hh->service_select = isdn_service_select;
	/*	hh->read = ___; */
	hh->write = isdn_write_samples;
	hh->service_readwrite = isdn_service_readwrite;
	hh->dial = isdn_dial;
	/*	hh->hangup = ___; */
}

/***************************************************************************
 *
 * The following functions are helper functions called from the state
 * machine below, but not as a proper state.
 */

static void debug_message(unsigned int msg, int terminate)
{
	int level = DEBUG_DEBUG;

	if ( terminate )
		level = DEBUG_LAST;

	if ( msg & ISDN_MESSAGE_OK )
		ifax_dprintf(level,"Message 'OK' received\n");
	if ( msg & ISDN_MESSAGE_CRLF )
		ifax_dprintf(level,"Message 'CRLF' received\n");
	if ( msg & ISDN_MESSAGE_ERROR )
		ifax_dprintf(level,"Message 'ERROR' received\n");
	if ( msg & ISDN_MESSAGE_VCON )
		ifax_dprintf(level,"Message 'VCON' received\n");
	if ( msg & ISDN_MESSAGE_BUSY )
		ifax_dprintf(level,"Message 'BUSY' received\n");
	if ( msg & ISDN_MESSAGE_NOCARRIER )
		ifax_dprintf(level,"Message 'NOCARRIER' received\n");
	if ( msg & ISDN_MESSAGE_RING )
		ifax_dprintf(level,"Message 'RING' received\n");
	if ( msg & ISDN_MESSAGE_RING2 )
		ifax_dprintf(level,"Message 'RING2' received\n");

	if ( terminate && (msg & 0xffffff00) ) {
		ifax_dprintf(level,"Previous messages came unexpected\n");
		exit(1);
	}
}


/***************************************************************************
 *
 * The rest of this file is a Finite State Machine for administrating
 * the ISDN /dev/ttyI* interface.  These are the functions that do the main
 * work, the isdn entery functions defined above basically sets up a few
 * variables and waits for the state-machine to carry out the work.
 */

/*
 * The statemachine used here has the main user pointer pointing to the
 * hardware handle structure.  This structure has a private pointer that
 * points to the ISDN structure/variables.  These structures are
 * accessed as hh->... and ih->... in the statemachine.
 * In order to be able to access these variables, they need to be
 * declared and initialized.  This is done by the following macros, which
 * are activated when defining a state by using the appropriate NEEDS_* macro.
 */

#define ISDN_FSM_DECL_hh struct HardwareHandle *hh = fsmself->private;
#define ISDN_FSM_DECL_ih struct IsdnHandle *ih = hh->private;

#define NEEDS_none	/* No state-machine related variables needed */
#define NEEDS_hh	ISDN_FSM_DECL_hh
#define NEEDS_ih	ISDN_FSM_DECL_hh ISDN_FSM_DECL_ih
#define NEEDS_hh_ih	ISDN_FSM_DECL_hh ISDN_FSM_DECL_ih

/*
 * Variables can be allocated on the statemachine stack inside of a subroutine
 * so there can be persistent variables across several states of a subroutine.
 */
struct generic_isdn_locals {
	hard_timer_t htimer1;
	hard_timer_t htimer2;
	int misc1;
	int misc2;
};

#define ISDN_FSM_DECL_local \
	struct generic_isdn_locals *local = FSM_SETUPFRAME;

#define NEEDS_local	ISDN_FSM_DECL_local
#define NEEDS_local_hh	ISDN_FSM_DECL_local ISDN_FSM_DECL_hh
#define NEEDS_local_ih	ISDN_FSM_DECL_local ISDN_FSM_DECL_hh ISDN_FSM_DECL_ih
#define NEEDS_local_hh_ih NEEDS_local_ih

FSM_DEFSTATE(isdn_junk_incomming)

FSM_DEFSTATE(start_initialize_isdn)
FSM_DEFSTATE(isdn_enter_idle_state)
FSM_DEFSTATE(do_at_command_with_OK)
FSM_DEFSTATE(do_at_command)
FSM_DEFSTATE(do_at_command_get_result)


/*************************************************************************
 *
 * This state is the initial state of the FSM used to get everything
 * started.  It will simply wait until the physical device is opened,
 * and run a simple check on the ISDN device and continue (or fail).
 */

FSM_DEFSTATE(check_flushing_isdn_ok)

FSM_STATE(NEEDS_hh_ih,isdn_start_state)
	if ( ih->fd >= 0 ) {
		/* When device is physically opened, initialize it */
		hh->state = INITIALIZING;
		FSMCALLJUMP(isdn_junk_incomming,check_flushing_isdn_ok,0);
	}
FSM_END

FSM_STATE(NEEDS_hh,check_flushing_isdn_ok)
	if ( FSMRETVAL ) {
		/* ISDN system seems to behave well, continue init */
		ifax_dprintf(DEBUG_JUNK,"Device opened\n");
		FSMCALLJUMP(start_initialize_isdn,isdn_enter_idle_state,0);
	}
	/* The device was not "silent", continuing would be pointless */
	hh->state = FAILED;
FSM_END


/**************************************************************************
 *
 * Main operational states of the ISDN line driver subsystem.  State
 * 'isdn_enter_idle_state' is called after initialization, and everything
 * is taken from there.  Depending on what state the system is in,
 * the API functions will try to alter variables to initiate various
 * operations or responses.  The state machine is totally in control as
 * to what happens to the ISDN layer, so the API functions can only
 * request certain operations, and monitor their progress.
 */

FSM_DEFSTATE(isdn_main_loop_idle)
FSM_DEFSTATE(isdn_start_dial)
FSM_DEFSTATE(isdn_start_dial2)

FSM_STATE(NEEDS_hh_ih,isdn_enter_idle_state)
	/* Set everything up before entering the main loop */
	hh->state = IDLE;
	ih->request_hangup = 0;		/* Hangup is a NOP when idle */
	ih->request_answer = 0;		/* Answer when idle is confused */
	ih->request_dial = 0;		/* Not yet ... */
	FSMJUMP(isdn_main_loop_idle);
FSM_END


FSM_STATE(NEEDS_hh_ih,isdn_main_loop_idle)
	/* This is where we loop on an unused system with no ISDN
	 * activity.
	 */
	if ( ih->request_dial ) {
		ih->request_dial = 0;
		if ( hh->state == IDLE )
			FSMJUMP(isdn_start_dial);
	}
FSM_END


/***************************************************************************
 *
 * The 'isdn_start_dial' takes care of setting up an outgoing call.
 * Phone number to call must already be loaded into the ih->dialmsn[] array.
 */

FSM_DEFSTATE(endless_loop)
FSM_DEFSTATE(isdn_start_dial2)
FSM_DEFSTATE(isdn_start_dial3)

FSM_STATE(NEEDS_hh_ih,isdn_start_dial)
	hh->state = CALLING;
	ih->start_time = time(0);
	sprintf(ih->at_cmd,"ATD%s",ih->dialmsn);
	FSMCALLJUMP(do_at_command,isdn_start_dial2,0);
FSM_END

FSM_STATE(NEEDS_none,isdn_start_dial2)
	/* Wait for response from tty after dialing */
	FSMCALLJUMP(do_at_command_get_result,isdn_start_dial3, 0
		    | ISDN_MESSAGE_VCON
		    | ISDN_MESSAGE_BUSY
		    | ISDN_MESSAGE_NOCARRIER);

FSM_END

FSM_STATE(NEEDS_none,isdn_start_dial3)
	ifax_dprintf(DEBUG_JUNK,"Dialing done: Message=%d\n",FSMRETVAL);
	FSMJUMP(endless_loop);
FSM_END

FSM_STATE(NEEDS_none,endless_loop)
	/* Keep spending time here ... */
FSM_END


/***************************************************************************
 *
 * Subroutines to read any junk that may come from the ISDN device
 * and flush any unwritten output to it.
 */

#define JUNK_INCOMMING_MAX_TIME		1,0		/* One second */
#define JUNK_INCOMMING_SILENCE_TIME	0,100000	/* 1/10 second */

FSM_DEFSTATE(isdn_junk_incomming_swallow)

/*
 * Read all that comes from the ISDN device for up to one second and throw
 * the received data away.  If we expirience 0.1 second of silence, assume
 * device has turned quiet and has no more data queued up.  If data keeps
 * arriving for more than one second, an error is returned.
 *
 * Returns: 0 when error
 *          1 when ok
 */
FSM_STATE(NEEDS_local,isdn_junk_incomming)
	FSM_ALLOCFRAME(sizeof(struct generic_isdn_locals));
	hard_timer_init(&local->htimer1,JUNK_INCOMMING_MAX_TIME);
	hard_timer_init(&local->htimer2,JUNK_INCOMMING_SILENCE_TIME);
     	FSMJUMP(isdn_junk_incomming_swallow);
FSM_END

FSM_STATE(NEEDS_local_ih,isdn_junk_incomming_swallow)
	/* Just read everything from the ISDN device until one of the
	 * timers expire.
	 */
	if ( ih->incomming_buffer->size > 0 ) {
		/* There is some 'junk' - throw it away and reset timer */
		iobuffer_reset(ih->incomming_buffer);
		hard_timer_init(&local->htimer2,JUNK_INCOMMING_SILENCE_TIME);
		FSMYIELD;
	}

	/* If MAX_TIME has elapsed, give up and report failure */
	if ( hard_timer_expired(&local->htimer1) )
		FSMRETURN(0);

	/* If SILENCE timer expired, we have (some) silence and report ok */
	if ( hard_timer_expired(&local->htimer2) )
		FSMRETURN(1);
FSM_END

/*
 * Flush the outgoing ISDN queue.  This is used during initialization, and
 * possibly at other times when samples needs to be flushed before ioctl,
 * close/open etc.  No return value, it hangs until output buffer is
 * emptied.
 */

FSM_STATE(NEEDS_hh_ih,flush_outgoing)
	if ( ih->outgoing_buffer->size == 0 ) {
		/* Outgoing buffer has been emptied, our job is done */
		FSMRETURN(0);
	}
FSM_END


/****************************************************************************
 *
 * Initialize the ISDN /dev/ttyI* interface by issuing AT-commands to
 * prepare everything for voice transfers.
 */

FSM_DEFSTATE(start_initialize_isdn2)
FSM_DEFSTATE(start_initialize_isdn3)
FSM_DEFSTATE(start_initialize_isdn4)
FSM_DEFSTATE(start_initialize_isdn5)

FSM_STATE(NEEDS_hh_ih,start_initialize_isdn)
	/* Feed all the 'AT-commands' needed to initialize to the
	 * isdn device and monitor their responses.  The very first
	 * initialization will just write a series of AT-commands
	 * to stabalize the tty and simply discard the echo/results.
	 */
	static char *cold_cmd =
		"\rATS3=13\r\nATS3=13\n"	/* S3=13 (CR=carrige return) */
		"\rATS4=10\r\nATS4=10\n"	/* S4=10 (LF=Line feed) */
		"\rATE0\r"			/* Echo disabled */
		"ATV1\r";			/* English responses */

	iobuffer_fill(ih->outgoing_buffer,(ifax_uint8 *)cold_cmd,
		      strlen(cold_cmd));
	FSMCALLJUMP(flush_outgoing,start_initialize_isdn2,0);
FSM_END

FSM_STATE(NEEDS_none,start_initialize_isdn2)
	/* Discard any output from the ISDN device that may have been
	 * caused by the AT commands in the previous state.  This is
	 * to complete the "initialize from unknown state", and all
	 * programming of the ISDN device is done properly after this.
	 */
	FSMCALLJUMP(isdn_junk_incomming,start_initialize_isdn3,0);
FSM_END

FSM_STATE(NEEDS_hh_ih,start_initialize_isdn3)
	/* Start sending the 'proper' AT-commands to the ISDN driver.
	 * Check if device is indeed ISDN first.  Also, initialize the
	 * "response" flag array, which is updated with bit-masks for
	 * all known fixed or handled responses, like RING, VCON, OK etc.
	 * Some of these fixed messages may be delivered asynchronously,
	 * which is handeled and logged at a lower level.  Indications
	 * of which fixed messages have been received is flagged in the
	 * ih->message variable.
	 */
	ih->messages = 0;
	strcpy(ih->at_cmd,"ATI");
	FSMCALLJUMP(do_at_command_with_OK,start_initialize_isdn4,0);
FSM_END

FSM_STATE(NEEDS_hh_ih,start_initialize_isdn4)
	if ( FSMRETVAL >= 0x100 )
		debug_message(FSMRETVAL,1);

	if ( strcmp(ih->at_result,"Linux ISDN") ) {
		/* Ooops, this seems to be something else */
		ifax_dprintf(DEBUG_LAST,"Device not recognized as ISDN (%s)\n",
			     ih->at_result);
		exit(1);
	}
	sprintf(ih->at_cmd,"AT&E%s+FCLASS=8+VSM=5+VLS=2S18=1S14=4",ih->ourmsn);
	FSMCALLJUMP(do_at_command_with_OK,start_initialize_isdn5,0);
FSM_END

FSM_STATE(NEEDS_none,start_initialize_isdn5)
	if ( FSMRETVAL >= 0x100 )
		debug_message(FSMRETVAL,1);

	ifax_dprintf(DEBUG_INFO,"ISDN device initialized OK\n");

	FSMRETURN(0);
FSM_END



/***************************************************************************
 *
 * Check if the current character stream from the ISDN device has any
 * fixed messages inside (RING etc.) that needs special attention.
 *
 * If the character(s) in the buffer may be part of an async message,
 * a short timer is activated and it is checked if more incomming
 * characters are in the pipeline.  If so, they are read and checked
 * against the async messages that we expect.
 *
 * Return values:
 *     0		- Nothing available
 *     1-255		- A single character available (no message)
 *     ISDN_MESSAGE_*	- Message available (value 256-2G)
 */

static struct {
	int message;
	char *string;
	int length;
} isdn_message[] = {
	{ 0, 0, 0 },			/* Unused; Index 0 used for "false" */

	{ ISDN_MESSAGE_OK,		"\r\nOK\r\n", 6 },
	{ ISDN_MESSAGE_ERROR,		"\r\nERROR\r\n", 9 },
	{ ISDN_MESSAGE_VCON,		"\r\nVCON\r\n", 8 },
	{ ISDN_MESSAGE_BUSY,		"\r\nBUSY\r\n", 8 },
	{ ISDN_MESSAGE_NOCARRIER,	"\r\nNO CARRIER\r\n", 14 },
	{ ISDN_MESSAGE_RING,		"\r\nRING\r\n", 8 },
	{ ISDN_MESSAGE_RING2,		"\r\nCALLER NUMBER: ", 17 },
	{ ISDN_MESSAGE_CRLF,		"\r\n", 2 },

	{ 0, 0, 0 }			/* Terminate list */
};

FSM_DEFSTATE(isdn_get_next_msg2)
FSM_DEFSTATE(isdn_get_next_msg3)
FSM_DEFSTATE(isdn_get_next_msg4)


/*
 * Subroutine to get the next character or message from the ISDN device.
 */

FSM_STATE(NEEDS_none,isdn_get_next_msg)
	FSMCALLJUMP(isdn_get_next_msg3,isdn_get_next_msg2,0);
FSM_END

FSM_STATE(NEEDS_none,isdn_get_next_msg2)
	int arg;
	char chr[16];
	arg = FSMRETVAL;
	if ( arg != 0 ) {
		if ( arg >= 32 && arg < 127 )
			sprintf(chr," '%c'",arg);
		else
			chr[0] = '\0';
		ifax_dprintf(DEBUG_JUNK,"ISDN Message: %08x%s\n",arg,chr);
	}
	/* Act depending on the message; if there are more info like
	 * a phone number comming, read this as well and store it.
	 */
	FSMRETURN(arg);
FSM_END

FSM_STATE(NEEDS_local,isdn_get_next_msg3)
     	FSM_ALLOCFRAME(sizeof(struct generic_isdn_locals));
	hard_timer_init(&local->htimer1,0,100000);
	FSMJUMP(isdn_get_next_msg4);
FSM_END

FSM_STATE(NEEDS_local_ih,isdn_get_next_msg4)
	char *s1, *s2;
	int t, length, x, partial_match, complete_match;
	unsigned char tmp[128];

	x = iobuffer_drain(ih->incomming_buffer,(unsigned char *)&tmp[0],100);
	
	/* Return if no data available after the delay */
	if ( x == 0 ) {
		if ( hard_timer_expired(&local->htimer1) )
			FSMRETURN(0);
		FSMYIELD;
	}

	/* Check if there is a message, or if there might be one
	 * building up (only parts of it read so far).
	 */
	complete_match = partial_match = 0;
	for ( t=1; isdn_message[t].length != 0; t++ ) {
		length = isdn_message[t].length;
		s1 = (char *)&tmp[0];
		s2 = isdn_message[t].string;
		if ( !complete_match && x >= length && !strncmp(s1,s2,length) )
			complete_match = t;
		
		if ( x < length && !strncmp(s1,s2,x) ) {
			partial_match = t;
			break;
		}
	}

	/* If we have partial matches, wait the timeout period for more */
	if ( partial_match && !hard_timer_expired(&local->htimer1) )
		FSMYIELD;

	if ( complete_match ) {
		length = isdn_message[complete_match].length;
		iobuffer_drain_update(ih->incomming_buffer,length);
		FSMRETURN(isdn_message[complete_match].message);
	}

	iobuffer_drain_update(ih->incomming_buffer,1);
	FSMRETURN(tmp[0]);
FSM_END

#if 0
FSM_STATE(NEEDS_ih,register_async_callerid)
	ifax_dprintf(DEBUG_JUNK,"\n\n@@@@@@@@ CALLER-ID: %s\n\n\n",
		     ih->at_tmp);
	FSMRETURN(0);
FSM_END

/* This state is called whenever an async message is received to register
 * the event.
 */

FSM_STATE(NEEDS_ih,register_async_event)
	switch ( FSMGETARG ) {
		case ASYNC_RING1:
			ih->indication_ringing = 1;
			FSMRETURN(0);
			break;
		case ASYNC_RING2:
			ih->indication_ringing = 1;
			FSMCALLJUMP(do_at_command_get_result,
				    register_async_callerid,0);
			break;
	}
FSM_END
#endif


/***************************************************************************
 *
 * Send an AT-command to the ISDN interface.
 *
 * Entery points for subroutine calls are:
 *     - do_at_command_with_OK			for simple commands
 *     - do_at_command				for more complex commands
 *     - do_at_command_get_result		to read result of complex cmd.
 *
 * When calling 'do_at_command_with_OK' or 'do_at_command' the command
 * string to be executed must be in ih->at_cmd (NUL terminated string)
 *
 * When do_at_command_with_OK or do_at_command_get_result are called, they
 * will return the results in
 *     - ih->at_cmd_result	First line of characters
 *     - return value		Logical OR of all messages other than prime
 *
 * When calling 'do_at_command_get_result' the prime messages should be
 * specified by logical ORing them as the argument.  The subroutine will
 * scan for one of these messages and not return until found.  The
 * 'do_at_command_with_OK' subroutine calls 'do_at_command_get_result'
 * with ISDN_MESSAGE_OK as argument.
 */

FSM_DEFSTATE(do_at_command_send)
FSM_DEFSTATE(do_at_command_with_OK2)
FSM_DEFSTATE(do_at_command_with_OK3)
FSM_DEFSTATE(do_at_command_get_result)
FSM_DEFSTATE(do_at_command_get_result2)

/*
 * Execute a command that should return OK
 */
FSM_STATE(NEEDS_none,do_at_command_with_OK)
     	FSMCALLJUMP(do_at_command_send,do_at_command_with_OK2,0);
FSM_END

FSM_STATE(NEEDS_none,do_at_command)
	FSMJUMP(do_at_command_send);
FSM_END

FSM_STATE(NEEDS_hh_ih,do_at_command_send)
	char *s, *d;
	s = d = &ih->at_cmd[0];
	while ( *s ) {
		if ( *s != ' ' )
			*d++ = *s;
		s++;
	}
	*d++ = '\r';
	*d = '\0';
	iobuffer_fill(ih->outgoing_buffer,(ifax_uint8 *)ih->at_cmd,
		      strlen(ih->at_cmd));
	FSMRETURN(0);
FSM_END

FSM_STATE(NEEDS_none,do_at_command_with_OK2)
	FSMCALLJUMP(do_at_command_get_result,do_at_command_with_OK3,
		    ISDN_MESSAGE_OK);
FSM_END

FSM_STATE(NEEDS_none,do_at_command_with_OK3)
	unsigned int msg = FSMRETVAL;

	msg &= ~(ISDN_MESSAGE_OK | ISDN_MESSAGE_CRLF);
	debug_message(msg,1);

	FSMRETURN(msg);
FSM_END

FSM_STATE(NEEDS_local_ih,do_at_command_get_result)
	FSM_ALLOCFRAME(sizeof(struct generic_isdn_locals));
	ih->at_idx = 0;
	local->misc1 = 0;
	local->misc2 = FSMGETARG;
	FSMCALLJUMP(isdn_get_next_msg,do_at_command_get_result2,0);
FSM_END

FSM_STATE(NEEDS_local_ih,do_at_command_get_result2)
	int c = FSMRETVAL;

	/* Should check if c == 0 and have a timeout to give a serious error */

	if ( c > 0 && c < 0x100 ) {
		/* Single character */
		if ( ih->at_idx < ISDNATRESULT_SIZE-4 ) {
			/* There is space in the result buffer yet */
			ih->at_result[ih->at_idx++] = c;
			ih->at_result[ih->at_idx] = '\0';
		} else {
			/* No space, or CR+LF allready found, log something */
			local->misc1 |= c;
		}
	} else {
		/* We have a 'message' - record it, chack for return cond. */
		local->misc1 |= c;
		if ( local->misc1 & local->misc2 ) {
			/* One of the prime messages found, return */
			FSMRETURN(local->misc1);
		}
	}

	if ( c == ISDN_MESSAGE_CRLF && ih->at_idx > 0 ) {
		/* Terminating CR+LF for first line reply, stop reading  */
		ih->at_idx = ISDNATRESULT_SIZE;
	}

	/* Get another message or character, and restart this state */
	FSMCALLJUMP(isdn_get_next_msg,do_at_command_get_result2,0);
FSM_END
