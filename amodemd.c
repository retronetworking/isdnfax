/* $Id$
******************************************************************************

   Fax program for ISDN.
   Initial startup of daemon 'amodemd' - contains main() etc.

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

/* This file holds the 'main()' function for the 'amodemd' daemon program
 * that enables communication with analog modems and faxes without the
 * use of a dedicated modem.  This is made possible by collecting all the
 * digital signal processing algorithms usually found in modems inside
 * this daemon program.  This daemon will as a result of this, consume
 * significant resources when communication is commencing.
 */

/* Defining this avoids dialing, and only logs */
#define TESTING

#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <ifax/ifax.h>
#include <ifax/misc/globals.h>
#include <ifax/misc/readconfig.h>
#include <ifax/misc/environment.h>
#include <ifax/misc/watchdog.h>
#include <ifax/misc/regmodules.h>
#include <ifax/misc/isdnline.h>
#include <ifax/misc/timers.h>
#include <ifax/misc/softsignals.h>
#include <ifax/modules/linedriver.h>
#include <ifax/G3/initialize.h>
#include <ifax/G3/kernel.h>
#include <ifax/G3/fax.h>

#include <ifax/misc/pty.h>
#include <ifax/highlevel/commandparse.h>

static struct IsdnHandle *ih;
static struct PtyHandle *ph;
static struct ModemHandle *mh;

/* Parse command-line arguments and bail out with an error message if
 * something is wrong.
 */

static void usage(void)
{
	fprintf(stderr,"Usage: %s [-D] [-c <config-file>]\n",progname);
	exit(1);
}

static void parse_arguments(int argc, char **argv)
{
	int t;

	if ( argc > 0 && argv[0] != 0 ) {
		progname = rindex(argv[0],'/');
		if ( progname != 0 ) {
			progname++;
		} else {
			progname = argv[0];
		}
	}

	t = 1;
	while ( t < argc ) {

		if ( !strcmp("-c",argv[t]) ) {
			t++;
			if ( t >= argc )
				usage();
			config_file = argv[t];
			t++;
		}

		if ( !strcmp("-D",argv[t]) ) {
			t++;
			run_as_daemon = 1;
		}
	}
}

static void main_loop(void)
{
	fd_set rfd, wfd;
	int maxfd, rc;
	struct timeval delay;

	printf("Main loop....\n");

	for (;;) {
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);

		/* Watch the ISDN-line for more incomming samples */
		FD_SET(ih->fd,&rfd);
		maxfd = ih->fd + 1;

		/* Watch the pty for incomming AT-commands or data */
		pty_prepare_select(ph,&maxfd,&rfd,&wfd);

		delay.tv_sec = 0;
		delay.tv_usec = 500000;

		/* Do the actual waiting (sleeping).  When this select
		 * system call is made, the rest of the system can run when
		 * using the real-time scheduler.
		 */
		rc = select(maxfd, &rfd, (fd_set *)0, (fd_set *)0, &delay);

		pty_service_read(ph);

		rc = IsdnService(ih);
		if ( rc == ISDN_RINGING ) {
			pty_write(ph,"\nRING\n\n",7);
#if 0
			/* Not gotten far enough for testing this yet... */
			if ( IsdnAnswer(ih) ) {
				setup_incomming();
				handle_call();
			}
#endif
		}

		modeminput(mh,ph);

		pty_service_write(ph);
	}
}


/* Startup point for 'amodemd' - parse args, initialize and off we go */

void main(int argc, char **argv)
{
	parse_arguments(argc,argv);
	read_configuration_file();

	if ( run_as_daemon )
		start_daemon();

	if ( watchdog_timeout > 0 ) {
		initialize_watchdog_timer();
		reset_watchdog_timer();
	}

	register_modules();

	/* Start slave I/O and interface process here when available... */

	ph = pty_initialize("/dev/ptyp9");
	mh = modem_initialize();
	ih = IsdnInit(isdn_device,isdn_msn);

	linedriver = ifax_create_module(IFAX_LINEDRIVER);
	ifax_command(linedriver,CMD_LINEDRIVER_ISDN,ih);
	/* ifax_command(linedriver,CMD_LINEDRIVER_AUDIO); */
	/* ifax_command(linedriver,CMD_LINEDRIVER_RECORD,"modem.dat"); */

	initialize_G3fax(linedriver);

	initialize_realtime();

	main_loop();

	/* Should never get here */
	ifax_command(linedriver,CMD_LINEDRIVER_RECORD,(char *)0);
	check_stack_usage();

	exit(0);
}
