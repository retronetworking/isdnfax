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

#define STATEMACHINESTACKSIZE 16
#define MAXSTATEMACHINES      64

struct statemachine {
  void (*state)(struct statemachine *);
  void (*stack[STATEMACHINESTACKSIZE])(struct statemachine *);
  int stackpointer, arg;
  struct statemachine *next;
};

#define STATE(x) \
static void x(struct statemachine *fsmself)

#define GLOBALSTATE(x) \
void x(struct statemachine *fsmself)

#define DEFSTATE(x) \
static void x(struct statemachine *fsmself);

#define DEFGLOBALSTATE(x) \
extern void x(struct statemachine *fsmself);

#define FSMJUMP(x) \
{ fsmself->state = x; return; }

#define FSMCALLJUMP(c,j) \
{ fsmself->state=c; fsmself->stack[fsmself->stackpointer++]=j; return; }

#define FSMRETURN \
{ fsmself->state=fsmself->stack[--fsmself->stackpointer]; return; }

#define FSMWAITJUMP(t,d,s) \
{ one_shot_timer(t,d); fsmself->arg=t; FSMCALLJUMP(fsm_wait_softsignal,s) }

extern void initialize_statemachines(void);
extern void init_fsm(int, void (*)(struct statemachine *));
extern void kill_fsm(int);
extern void run_statemachines(void);


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
