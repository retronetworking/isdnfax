/* $Id$
******************************************************************************

   Fax program for ISDN.
   Statemachine management functions

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

/* How deep the stack trace in a state machine can be */
#define FSM_CALL_DEPTH 16

/* Max number of direct (immediate) state-jumps can be allowed
 * before we have to pre-empt the state-machine to avoid lock-ups.
 */
#define MAXSEQSTATEJUMPS   64

/* Maximum size of (source-code) function names and filename for states */
#define FSM_FUNCIDENT_MAX	64

/* Defines for each single state machine */
struct statemachine;
typedef void (*fsm_t)(struct statemachine *);

struct statemachine {
	fsm_t state;			/* Next state to call */
	void *private;			/* Pointer a private memory segment */
	struct statemachine *next;	/* Next statemachine on run-chain */
	char lastfunc[128];		/* Name of last state/function */
	int lastfunc_count;		/* Number of wakeups for last state */
	int lastsp;		/* Last value of stackpointer (for debug) */
	int debug;		/* Nonzero if debugging wanted */
	int maxloops;		/* Max number of states without yielding */

	/* We need a stack and local variables to be flexible */
	int sp, return_value;
	fsm_t stack[FSM_CALL_DEPTH];
	int arg[FSM_CALL_DEPTH];
	char stack_file[FSM_CALL_DEPTH][FSM_FUNCIDENT_MAX];
	char stack_function[FSM_CALL_DEPTH][FSM_FUNCIDENT_MAX];
	void *stackdata;
	void *stackframe[FSM_CALL_DEPTH];
	int stackframesize[FSM_CALL_DEPTH];
};

/* A collection of state machines are contained in the following
 * handle, allocated by allocate_statemachines(count);
 */
struct StateMachinesHandle {
	struct statemachine *fsm, *running;
};

/* The following macros may be defined for state-call tracking.  Define
 * FSM_DEBUG_STATES to get the debugging info.
 */
#undef _FSMDECL_DEBUG
#undef _FSMEXIT_DEBUG

#ifdef FSM_DEBUG_STATES
#define _FSMDECL_DEBUG void *_fsmdebug = \
	_fsm_debug_entry(fsmself,__FUNCTION__);
#define _FSMEXIT_DEBUG _fsm_debug_exit(_fsmdebug);
#else
#define _FSMDECL_DEBUG
#define _FSMEXIT_DEBUG
#endif


/* Define the macros used to define and manipulate the states of the
 * statemachines.
 */

#define FSM_GLOBAL_STATE(decl,state)					\
	void state(struct statemachine *fsmself)			\
	{								\
		decl _FSMDECL_DEBUG

#define FSM_STATE(decl,state) static FSM_GLOBAL_STATE(decl,state)

#define FSM_END _FSMEXIT_DEBUG }

#define FSM_DEFSTATE(x) \
	static void x(struct statemachine *fsmself);
#define FSM_DEFSTATE_GLOBAL(x) \
	extern void x(struct statemachine *fsmself);


#define FSM_SETUPFRAME (fsmself->stackframe[fsmself->sp])
#define FSM_ALLOCFRAME(size)						\
	do {								\
		if ( fsmself->stackframesize[fsmself->sp] == 0 ) {	\
			fsmself->stackframesize[fsmself->sp] = size;	\
		}							\
	} while (0)

/* These are for backward compatibility only
#define DEFSTATE(x) FSM_DEFSTATE(x)
#define DEFGLOBALSTATE(x) FSMDEF_DEFSTATE_GLOBAL(x)
*/

#define FSMJUMP(x)							\
	do {								\
		fsmself->state = x;					\
		return;							\
	} while (0)

#define _FSMSETPOS							\
	do {								\
		sprintf(fsmself->stack_file[fsmself->sp],"%s:%d",	\
			__FILE__,__LINE__);				\
	} while (0)

#define FSMCALLJUMP(c,j,a)						\
	do {								\
		fsmself->state = c;					\
		fsmself->stack[fsmself->sp] = j;			\
		fsmself->arg[fsmself->sp+1] = (a);			\
		fsmself->stackframe[fsmself->sp+1] = (void *)		\
			(((char *)fsmself->stackframe[fsmself->sp])	\
			 + fsmself->stackframesize[fsmself->sp]);	\
		_FSMSETPOS;						\
		fsmself->sp++;						\
		fsmself->stackframesize[fsmself->sp] = 0;		\
		return;							\
	} while (0)

#define FSMRETURN(val)							\
	do {								\
		fsmself->sp--;						\
		fsmself->state = fsmself->stack[fsmself->sp];		\
		fsmself->return_value = val;				\
		return;							\
	} while (0)

#define FSMRETVAL (fsmself->return_value)

#define FSMGETARG (fsmself->arg[fsmself->sp])

#define FSMYIELD  return

#define FSMWAITJUMP(timer,delay,state)					\
	do {								\
		one_shot_timer(timer,delay);				\
		FSMCALLJUMP(fsm_wait_softsignal,state,timer);		\
	} while (0)

/* Help debug state machine problems by dumping the state-machine stack */

#define FSM_DUMPSTACK							\
	do {								\
		int t;							\
		_FSMSETPOS;						\
		fsmself->stack[fsmself->sp] = fsmself->state;		\
		for ( t=fsmself->sp; t >= 0; t-- ) {			\
			printf("#%d %p in %s at %s\n",			\
			       fsmself->sp - t,				\
			       fsmself->stack[t],			\
			       fsmself->stack_function[t],		\
			       fsmself->stack_file[t]);			\
		}							\
	} while (0)



struct StateMachinesHandle *fsm_allocate(int count);
void fsm_init(struct StateMachinesHandle *smh, int num, fsm_t state,
	      int maxloops, void *p);
void fsm_setup(struct StateMachinesHandle *smh, int fsmnum, int stacksize);
void fsm_kill(struct StateMachinesHandle *smh, int fsmnum);
void fsm_run(struct StateMachinesHandle *smh);

/* Internal to FSM system (debugging) */
void *_fsm_debug_entry(struct statemachine *fsmself, char *function_name);
void _fsm_debug_exit(void *dummy);


/* FIXME: With the advent of independent sets of state machines, the
 * following defines and their uses should be moved outside of this file,
 * and split up into several independent state machine systems:
 */

/* All running state-machines are identified by an integer number.
 * Allocation of state-machines are as follows:
 */

/* FAX Group 3 subsystem */
#define FSM_FAX_START                        0
#define FSM_FAX_TOTAL                        2
#define FSM_FAX_MAIN                         FSM_FAX_START+0
#define FSM_FAX_CNG                          FSM_FAX_START+1

/* V.21 modem 300 bit/s subsystem */
#define FSM_V21MODEM_START                   FSM_FAX_START+FSM_FAX_TOTAL
#define FSM_V21MODEM_TOTAL                   1
#define FSM_V21MODEM_MAIN                    FSM_V21MODEM_START+0
