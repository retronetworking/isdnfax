/* $Id$
******************************************************************************

   Fax program for ISDN.
   Statemachine management functions

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

/* Handeling of 'state machines' is done in this file.  A state-machine
 * as viewed/defined by this file is simply a collection of functions
 * that gets called in a certain way, and that manipulates its state
 * through the macros in the corresponding include-file.
 */

#include <ifax/misc/statemachine.h>
#include <ifax/types.h>
#include <ifax/debug.h>


/* Max number of direct (immediate) state-jumps can be allowed
 * before we have to pre-empt the state-machine to avoid lock-ups.
 */
#define MAXSEQSTATEJUMPS   16


static struct statemachine fsm_state[MAXSTATEMACHINES], *running;

void initialize_statemachines(void)
{
  int t;

  running = 0;
  for ( t = 0; t < MAXSTATEMACHINES; t++ ) {
    fsm_state[t].state = 0;
  }
}

void init_fsm(int machineid, void (*startstate)(struct statemachine *))
{
  /* Link into 'running' chain if previously not active */
  if ( fsm_state[machineid].state == 0 ) {
    fsm_state[machineid].next = running;
    running = &fsm_state[machineid];
  }
  fsm_state[machineid].stackpointer = 0;
  fsm_state[machineid].state = startstate;
}

void kill_fsm(int machineid)
{
  struct statemachine *previous, *kill = &fsm_state[machineid];

  fsm_state[machineid].state = 0;

  if ( running == kill ) {
    running = running->next;
    return;
  }

  previous = running;
  while ( previous->next != 0 ) {
    if ( previous->next == kill ) {
      previous->next = previous->next->next;
      return;
    }
  }

  return;  /* Shouldn't get here, actually */
}

void run_statemachines(void)
{
  void (*prev)(struct statemachine *);
  struct statemachine *current;
  int t;

  for ( current = running; current != 0; current = current->next ) {

    for ( t=0; t < MAXSEQSTATEJUMPS; t++ ) {
      prev = current->state;
      (current->state)(current);
      if ( current->state == prev )
	goto next_fsm;
    }

    /* Something probably has gone very wrong, print a warning */
    ifax_dprintf(DEBUG_SEVERE,"State-machine %d run-away\n",
		 current-fsm_state);

  next_fsm: ;

  }
}
