/* $Id$
******************************************************************************

   Fax program for ISDN.
   Initial startup of daemon 'amodemd' - contains main() etc.

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

static struct PtyHandle *ph;
static struct ModemHandle *mh;


/*
 * Parse command-line arguments and bail out with an error message if
 * something is wrong.  Variables 'argc' and 'argv' must be set prior
 * to calling these functions (e.g. initialized by 'main').
 */

static int argc, argn;
static char **argv, **arg;

static void usage(void)
{
	fprintf(stderr,"Usage: %s [-D] [-c <config-file>]\n",progname);
	exit(1);
}

static int option(const char *option, int num)
{
	if ( strcmp(argv[argn],option) )
		return 0;

	if ( argn + num <= argc ) {
		arg = argv + argn;
		return 1;
	}

	usage();
	return 0;	/* Silence compiler */
}

static void parse_arguments(void)
{
	if ( argc > 0 && argv[0] != 0 ) {
		progname = rindex(argv[0],'/');
		if ( progname != 0 ) {
			progname++;
		} else {
			progname = argv[0];
		}
	}

	argn = 1;
	while ( argn < argc ) {

		if ( option("-c",2) ) {
			config_file = arg[1];
			continue;
		}

		if ( option("-D",1) ) {
			run_as_daemon = 1;
			continue;
		}

		usage();
	}
}

/*
 * The main loop of the entire program.  All concurrent operations are
 * scheduled from here.  The inner select function takes care of waiting
 * for input/output, and all other parts of the system is designed to
 * cooperate with this inner loop.
 *
 * No part of the system can be allowed to block, as this will stall the
 * main loop, and thus possible stall some other important part of the
 * system (like the DSP chain or AT command parser).
 *
 * The main loop will only exit in case of an unrecoverable error.
 */

static void main_loop(void)
{
	fd_set rfd, wfd, efd;
	int maxfd, rc;
	struct timeval delay;

	ifax_dprintf(DEBUG_INFO,"Entering main loop\n");

	for (;;) {
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_ZERO(&efd);
		maxfd = -1;

		hh->prepare_select(hh,&maxfd,&rfd,&wfd,&efd);
		pty_prepare_select(ph,&maxfd,&rfd,&wfd);

		delay.tv_sec = 0;
		delay.tv_usec = 500000;

		rc = select(maxfd, &rfd, &wfd, &efd, &delay);

		pty_service_read(ph);
		hh->service_select(hh,&rfd, &wfd, &efd);

		modeminput(mh,ph);

		pty_service_write(ph);
	}
}


/* Startup point for 'amodemd' - parse args, initialize and off we go */

void main(int ac, char **av)
{
	argc = ac;
	argv = av;

	parse_arguments();
	read_configuration_file();

	if ( hh == 0 ) {
		ifax_dprintf(DEBUG_LAST,"No hardware device configured\n");
		exit(1);
	}
	
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

	linedriver = ifax_create_module(IFAX_LINEDRIVER);
	ifax_command(linedriver,CMD_LINEDRIVER_ISDN,hh);

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
