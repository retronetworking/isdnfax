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
 *
 * State machines are grouped together into "state machine groups".
 * Before a state machine can be used, it has to be allocated, along
 * with its possible co-working state-machines, and initialized.
 * This is done like:
 *
 *      smh = allocate_statemachines(number_of_statemachines,stacksize);
 *      allocate_statemachine_stack(smh, 0, 1024);
 *	allocate_statemachine_stack(smh, 2, 128);
 *      fsm_init(smh, 0, init_state_fsm0, 100, &globals[x]);
 *      fsm_init(smh, 1, init_state_fsm1,  10, some_handle);
 *      fsm_init(smh, 2, init_state_fsm2,  25, private_data);
 *        ...
 *      run_statemachines(smh);
 *
 */

#include <string.h>

#include <ifax/types.h>
#include <ifax/debug.h>
#include <ifax/misc/malloc.h>
#include <ifax/misc/statemachine.h>


struct StateMachinesHandle *fsm_allocate(int count)
{
	int t;
	struct StateMachinesHandle *smh;

	smh = ifax_malloc(sizeof(*smh), "State machine set instance");
	smh->running = 0;
	smh->fsm = ifax_malloc(count * sizeof(*smh->fsm),
			       "State machine instances");

	for ( t = 0; t < count; t++ ) {
		smh->fsm[t].state = 0;
		smh->fsm[t].next = 0;
		smh->fsm[t].stackdata = 0;
	}

	return smh;
}

void fsm_setup(struct StateMachinesHandle *smh, int machineid, int size)
{
	if ( smh->fsm[machineid].stackdata != 0 )
		free(smh->fsm[machineid].stackdata);

	smh->fsm[machineid].stackdata = ifax_malloc(size,"FSM stack data");
}


void fsm_init(struct StateMachinesHandle *smh, int machineid,
	      fsm_t init_state, int maxloops, void *private)
{
	/* Link into 'running' chain if previously not active */
	if ( smh->fsm[machineid].state == 0 ) {
		smh->fsm[machineid].next = smh->running;
		smh->running = &smh->fsm[machineid];
	}

	/* Setup stack */
	smh->fsm[machineid].sp = 0;
	smh->fsm[machineid].private = private;
	smh->fsm[machineid].stackframe[0] = smh->fsm[machineid].stackdata;
	smh->fsm[machineid].stackframesize[0] = 0;

	/* Set state to wake up in */
	smh->fsm[machineid].state = init_state;
	strcpy(smh->fsm[machineid].lastfunc,"(init)");
	smh->fsm[machineid].lastfunc_count = 0;

	/* Misc */
	smh->fsm[machineid].debug = 1;
	smh->fsm[machineid].maxloops = maxloops;
}

void fsm_kill(struct StateMachinesHandle *smh, int machineid)
{
	struct statemachine *previous, *kill;

	kill = &smh->fsm[machineid];
	smh->fsm[machineid].state = 0;

	if ( smh->running == kill ) {
		smh->running = smh->running->next;
		kill->next = 0;
		return;
	}

	previous = smh->running;
	for (;;) {
		if ( previous->next == kill ) {
			previous->next = previous->next->next;
			kill->next = 0;
			return;
		}
		/* Trying to kill a fsm not running will cause SEGV here... */
		previous = previous->next;
	}
}

void fsm_run(struct StateMachinesHandle *smh)
{
	fsm_t prev;

	struct statemachine *run;
	int t;

	for ( run = smh->running; run != 0; run = run->next ) {

		for ( t=0; t < run->maxloops; t++ ) {
			prev = run->state;
			run->state(run);
			if ( run->state == prev )
				goto next_fsm;
		}

		/* Something probably has gone very wrong, print a warning */
		ifax_dprintf(DEBUG_SEVERE,"State-machine run-away\n");

	next_fsm: ;

	}
}

void *_fsm_debug_entry(struct statemachine *fsmself, char *function_name)
{
	int t;
	char spaces[80];

	if ( !fsmself->debug )
		return 0;

	strcpy(fsmself->stack_function[fsmself->sp],function_name);
	if ( strcmp(fsmself->lastfunc, function_name) ) {
		/* We have jumped to a new function */
		spaces[0] = '\0';
		for ( t=0; t < 8 && t < fsmself->lastsp; t++ )
			strcat(spaces,"    ");
		ifax_dprintf(DEBUG_DEBUG,"FSM %s%s (%d) -> %s\n",spaces,
			     fsmself->lastfunc, fsmself->lastfunc_count,
			     function_name);
		strcpy(fsmself->lastfunc, function_name);
		fsmself->lastfunc_count = 1;
		fsmself->lastsp = fsmself->sp;
		return 0;
	}

	if ( fsmself->lastfunc_count++ > 10000 ) {
		ifax_dprintf(DEBUG_WARNING,"FSM stuck at %s?\n",
			     fsmself->lastfunc);
	}

	return 0;
}

void _fsm_debug_exit(void *dummy)
{
}
