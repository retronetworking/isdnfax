/* $Id$
******************************************************************************

   Fax program for ISDN.
   Initial startup of daemon 'amodemd' - contains main() etc.

   Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]

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

#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>

#include <ifax/misc/globals.h>
#include <ifax/misc/readconfig.h>
#include <ifax/misc/environment.h>
#include <ifax/misc/watchdog.h>


/* Parse command-line arguments and bail out with an error message if
 * something is wrong.
 */

static void usage(void)
{
  fprintf(stderr,"Usage: %s [-d] [-c <config-file>]\n",progname);
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

    if ( !strcmp("-d",argv[t]) ) {
      t++;
      run_as_daemon = 1;
    }
  }
}


/* Startup point for 'amodemd' - parse args, initialize and off we go */

void main(int argc, char **argv)
{
  int t;

  parse_arguments(argc,argv);
  read_configuration_file();

  if ( run_as_daemon )
    start_daemon();

  if ( watchdog_timeout > 0 ) {
    initialize_watchdog_timer();
    reset_watchdog_timer();
  }

  /* Next up is:
   *      initialize_fax();
   * and later maybe:
   *      initialize_v34();
   * ...
   */

  initialize_realtime();

  /*  main_loop(); */

  /* Waste some cycles to test watchdog timer.... */
  for ( t=0; t < 1000000000; t++ )
    ;

  check_stack_usage();

  exit(0);
}
