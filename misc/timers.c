/* $Id$
******************************************************************************

   Fax program for ISDN.
   Simple timers for controlling the state-machine(s).

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

/* The timers are very simple stuff:  Decrease the value of the active
 * timers by the specified ammount (unit of samples, given by the main
 * loop), and set of a software signal when it reaches zero.
 * If the timer is one-shot, it is disabled after the first signal.
 *
 * NOTE: A timer is identified by an integer value (index).  All signals
 * are also identified by an integer value, and they are related such that
 * when a timer 'n' reaces zero, the corresponding signal number 'n' is
 * signaled.
 *
 * BUG: This implementation is not very efficient, a sorted queue should
 * be used so we don't have to decrement all the timers all the time, but
 * this task is left as an exercise for the annoyed purist.
 */

#include <ifax/misc/timers.h>
#include <ifax/misc/softsignals.h>

static struct {
	ifax_sint32 current_value;
	ifax_sint32 initial_value;
} timers[MAX_TIMERS];

void one_shot_timer(int timer_num, ifax_sint32 delay)
{
	timers[timer_num].current_value = delay;
	timers[timer_num].initial_value = 0;
	softsignaled_clr(timer_num);
}

void reset_timers(void)
{
	int t;

	for ( t=0; t < MAX_TIMERS; t++ ) {
		timers[t].current_value = 0;
		timers[t].initial_value = 0;
	}
}

void decrease_timers(ifax_sint32 dec)
{
	int t;
	for ( t=0; t < MAX_TIMERS; t++ ) {
		if ( timers[t].current_value > 0 ) {
			timers[t].current_value -= dec;
			if ( timers[t].current_value <= 0 ) {
				softsignal(t);
				if ( timers[t].initial_value > 0 )
					timers[t].current_value +=
						timers[t].initial_value;
			}
		}
	}
}

void hard_timer_init(hard_timer_t *ht, ifax_uint32 sec, ifax_uint32 usec)
{
	gettimeofday(&ht->expires,(struct timezone *)0);
	ht->expires.tv_sec += sec;
	ht->expires.tv_usec += usec;
	if ( ht->expires.tv_usec > 1000000 ) {
		ht->expires.tv_sec++;
		ht->expires.tv_usec -= 1000000;
	}
}

int hard_timer_expired(hard_timer_t *ht)
{
	struct timeval now;

	gettimeofday(&now,(struct timezone *)0);

	if ( now.tv_sec < ht->expires.tv_sec )
		return 0;

	if ( now.tv_sec > ht->expires.tv_sec )
		return 1;

	if ( now.tv_usec >= ht->expires.tv_usec )
		return 1;

	return 0;
}
