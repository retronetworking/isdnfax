/* $Id$
******************************************************************************

   Fax program for ISDN.
   Set up environment for a real-time daemon process like the 'amodemd'

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

#include <stdio.h>
#include <sys/mman.h>
#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <ifax/misc/globals.h>
#include <ifax/misc/environment.h>
#include <ifax/misc/readconfig.h>

char *bottom_unused_stack = 0;  /* This part of the stack shouldn't be used */
char *top_used_stack = 0;       /* Top of stack, used a lot */

/* The following function puts this daemon into a mode of operation
 * where it will get the CPU-power needed to carry out its task.
 * This is needed since generating and interpreting the audio-
 * signals must be carried out "live", and with no pauses.
 * The remaining CPU-resources is available to other programs.
 *
 * This function should be called after all allocations have completed,
 * and if possible, initialized (so all memory is mapped).
 */

void initialize_realtime(void)
{
  char stack_initialization[TOTAL_STACK_USAGE];
  struct sched_param schedparams;

  /* In order to get predictable response-times, we lock this daemon and
   * its data into RAM, and tell the OS to avoid paging.  The assumed
   * stack-usage is 1MB, which is probably way to much, but RAM is cheap.
   * We have to initialize the stack-segment to make sure all pages
   * making up the stack is mapped.
   */

  if ( do_lock_memory ) {
    memset(stack_initialization,TOTAL_STACK_USAGE,0);

    if ( mlockall(MCL_CURRENT|MCL_FUTURE) != 0 ) {
      fprintf(stderr,"Can't lock memory\n");
      exit(1);
    }

    bottom_unused_stack = &stack_initialization[0];
    top_used_stack = &stack_initialization[TOTAL_STACK_USAGE-1];
  }

  /* Next up is requesting the full attention of the CPU.  This
   * basically means that our program will start to run when data
   * is available, no matter how badly other standard programs wants
   * to run.  NOTE: This will work since this daemon waits for
   * data regularly.  During these waits, other processes are
   * allowed to run, so the system can operate normally.
   * However: If this daemon fails to wait, the entire system will
   * become useless, since all other programs will be blocked.
   * A watchdog timer is implemented to try avoid a hard reset.
   */

  if ( realtime_priority > 0 ) {
    schedparams.sched_priority = realtime_priority;
    if ( sched_setscheduler((pid_t)0,SCHED_FIFO,&schedparams) < 0 ) {
      fprintf(stderr,"Couldn't enable real-time scheduling\n");
      exit(1);
    }
  }
}


/* When running as daemon, there are a few things we need to do.
 * Most important is disconnecting the tty.
 */

void start_daemon(void)
{
  pid_t child_pid;

  if ( (child_pid=fork()) < 0 ) {
    fprintf(stderr,"%s: Unable to fork a daemon\n",progname);
    exit(1);
  }

  if ( child_pid )
    exit(0);

  setsid();
}

void check_stack_usage(void)
{
  char *p;
  int unused_stack;

  if ( bottom_unused_stack == 0 )
    return;

  for ( p = bottom_unused_stack; p < top_used_stack; p++ )
    if ( *p != 0 )
      break;

  unused_stack = p - bottom_unused_stack;

  if ( unused_stack < MINIMUM_FREE_STACK_SAFETY_MARGIN ) {
    fprintf(stderr,"%s: Internal error - only %d bytes of headroom on stack\n",
	    progname,unused_stack);
    exit(1);
  }

  fprintf(stderr,"%s: INFO - There is %d bytes of headroom on stack\n",
	  progname,unused_stack);
}
