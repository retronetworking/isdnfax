/* $Id$
******************************************************************************

   Fax program for ISDN.
   Watchdog timer in case of misbehaving real-time process.

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

/* Set up a signal-handeler for the 'profiling' signal: On Linux, this
 * signal is called when a certain ammount of CPU-power has been used
 * by the process.  It is used for profiling programs, but we use it
 * to bail out in case of an infinite loop.  This is needed for real-
 * time applications.
 */

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#include <ifax/misc/globals.h>
#include <ifax/misc/readconfig.h>
#include <ifax/misc/watchdog.h>


static struct itimerval timer;


static void watchdog_trigger(int signum)
{
  fprintf(stderr,"\n\n%s: Watchdog timer caused termination\n",progname);
  exit(17);
}

void initialize_watchdog_timer(void)
{
  if ( signal(SIGPROF,watchdog_trigger) == SIG_ERR ) {
    fprintf(stderr,"%s: Can't install watchdog signal handeler\n",progname);
    exit(1);
  }
}

void reset_watchdog_timer(void)
{
  timer.it_value.tv_sec = watchdog_timeout;
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  if ( setitimer(ITIMER_PROF, &timer, (struct itimerval *)0) < 0 ) {
    fprintf(stderr,"%s: Can't reset watchdog timer\n",progname);
    exit(1);
  }
}
