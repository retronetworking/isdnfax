/* $Id$
******************************************************************************

   Fax program for ISDN.
   Communicate with the /dev/ttyI* devices to transfer audio over ISDN

   Copyright (C) 1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
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

/* Utility-functions to administrate ISDN-audio transfers.
 */

#include <time.h>
#include <ifax/misc/hardware-driver.h>
#include <ifax/misc/iobuffer.h>
#include <ifax/misc/timers.h>

#define ISDNERRMSG_SIZE    128
#define ISDNDEVNME_SIZE     64
#define ISDNMSN_SIZE        64
#define ISDNWRITE_TMP      512

#define ISDNATCMD_SIZE     256
#define ISDNATRESULT_SIZE  512
#define ISDNATPARSE_SIZE   128
#define ISDNATPARSE_SIZE_MIN   32

#define ISDNTTY_ETX       0x03
#define ISDNTTY_DLE       0x10
#define ISDNTTY_DC4       0x14

#if 0
#define ISDNTXBUF_SIZE     512
#define ISDNRECBUF_SIZE    512
#define ISDNMAXLNE_SIZE    256

#define ISDN_SLEEPING       -1
#define ISDN_RINGING        -2
#define ISDN_FAILED         -3
#define ISDN_ANSWER         -4
#define ISDN_BUSY           -5
#define ISDN_NOANSWER       -6

#define TTYIMODE_CMDOFFLINE  0
#define TTYIMODE_CMDONLINE   1
#define TTYIMODE_AUDIO       2
#endif

struct IsdnHandle {
	int fd;					/* ISDN device */
	struct StateMachinesHandle *smh;	/* Statemachine */
	struct IOBuffer *incomming_buffer;
	struct IOBuffer *outgoing_buffer;
	int at_idx;				/* Current char of at_cmd */
	unsigned int messages;			/* Accumulated messages */
	/* int at_cmd_ok;	*/		/* nonzero => AT command ok */
	time_t start_time, end_time;		/* Call timing info */
	int request_dial;			/* Nonzero => Start dialing */
	int request_hangup;			/* Force hangup of call */
	int request_answer;			/* Accept incomming call */
	int indication_ringing;			/* Inncomming call detected */
	char device[ISDNDEVNME_SIZE];		/* Name of ISDN-device */
	char ourmsn[ISDNMSN_SIZE];		/* Our MSN */
	char dialmsn[ISDNMSN_SIZE];		/* Diel this number */
	char remotemsn[ISDNMSN_SIZE];		/* Incomming call MSN */
	char at_cmd[ISDNATCMD_SIZE];		/* Current AT-command */
	char at_result[ISDNATRESULT_SIZE];	/* Result of AT-command */
	char at_tmp[ISDNATRESULT_SIZE];		/* Temp storage for result */
	ifax_uint8 tmp_write[ISDNWRITE_TMP];	/* Use for DLE insertion */
};

void isdn_allocate(struct HardwareHandle *hh);
