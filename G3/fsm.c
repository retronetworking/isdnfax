/* $Id$
******************************************************************************

   Fax program for ISDN.
   Main state-machine for G3-fax handeling.

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

/* This file contains all the states and their related logic needed to
 * implement a G3-fax encoder/decoder.  It is important to notice that
 * the states of this state-machine may not always map directly to
 * the G3-fax specifications.  The reason for this is the fact that
 * everything is handeled in software in this implementation, and that
 * extra states and logic may be needed to handle problems usually
 * handled by hardware directely.
 */

#include <ifax/types.h>
#include <ifax/G3/fax.h>
#include <ifax/G3/kernel.h>
#include <ifax/G3/g3-timers.h>
#include <ifax/misc/timers.h>
#include <ifax/misc/softsignals.h>


/* All timing is done in units of 1/8000 seconds (one ISDN-sample) */

#define SEVENTYFIVEMILLISECONDS                600
#define ZEROPOINTTWOSECONDS                   1600
#define THREEPOINTEIGHTSECONDS               30400


/* Use the following macros when defining the states */

#define STATE(x) static void x(struct G3fax *fax)
#define DEFSTATE(x) static void x(struct G3fax *);


/* When waiting for a specified ammount of time, and doing nothing
 * else, the simple_wait() function can be used.  It simply inserts
 * a "waiting-state" into the flow of things, using the TIMER_AUX
 * timer, and jumps to the specified "next-state" once the wait
 * is over.
 */

STATE(simple_wait_state)
{
  if ( softsignaled(TIMER_AUX) )
    fax->fsm = fax->simple_wait_next_state;
}

void simple_wait(struct G3fax *fax, ifax_sint32 delay,
		 void (*state)(struct G3fax *))
{
  fax->simple_wait_next_state = state;
  fax->fsm = simple_wait_state;
  one_shot_timer(TIMER_AUX,delay);
}

/* The following states are subroutines that must be called by means
 * of the 'invoke_subroutine' function.
 */

/* DEFSTATE(response_received) */




/* List states here that is called in a forward fashion (most of them) */

DEFSTATE(start_answer_incomming)
DEFSTATE(do_CED)
DEFSTATE(done_CED)
DEFSTATE(do_DIS)


/* Jump to 'start_answering_call' when an incomming call is
 * accepted (answered) and we identify ourselves as a fax-machine.
 * The initialize_fsm_incomming() function is used to initialize the fsm
 * and signal-chain to handle an incomming call and jump to this state.
 */

void initialize_fsm_incomming(struct G3fax *fax)
{
  fax->fsm = start_answer_incomming;
  fax_run_internals(fax);
}

STATE(start_answer_incomming)
{
  /* Stay silent for 0.2 seconds before outputing the CED */
  ifax_connect(fax->silence,fax->rateconv7k2to8k0);
  simple_wait(fax,ZEROPOINTTWOSECONDS,do_CED);
}

STATE(do_CED)
{
  /* Output the CED sinus signal for 3.8 sec */
  ifax_connect(fax->sinusCED,fax->rateconv7k2to8k0);
  simple_wait(fax,THREEPOINTEIGHTSECONDS,done_CED);
}

STATE(done_CED)
{
  /* After the CED, wait 75ms and do the DIS */
  ifax_connect(fax->silence,fax->rateconv7k2to8k0);
  simple_wait(fax,SEVENTYFIVEMILLISECONDS,do_DIS);
}

STATE(do_DIS)
{
  /* Naah, we need some more modules first... */
}




#if 0

/* The following code can't be used yet, because so many other modules
 * lack the softsignaling functionality when certain events occour.
 * Some other modules required has not been written yet.
 */

/****************************************************************************
 *
 *  Response Received, FIGURE 5-2s/T.30
 *
 *  This state-machine subroutine checks if a valid response has
 *  arrived within the time-limits.  If an illegal situation
 *  occours, like the remote transmitting more than it is allowed to,
 *  a hangup is performed.
 *
 *  By calling the 'invoke_subroutine' on the state 'response_received',
 *  the answer will be available in RESPONSERECEIVED_RESULT .
 */

DEFSTATE(response_received_flag)
DEFSTATE(response_received_raf)
DEFSTATE(response_received_sg1)
DEFSTATE(response_received_sg2)
DEFSTATE(response_received_sg2_helper)
DEFSTATE(response_received_tdcn)
DEFSTATE(response_received_disconnect)

STATE(response_received)
{
  one_shot_timer(TIMER_T4,THREESECONDS);
  fax->fsm = response_received_flag;
}

STATE(response_received_flag)
{
  if ( softsignaled_clr(HDLCFLAGDETECTED) ) {
    one_shot_timer(TIMER_SHUTUP,THREESECONDS);
    fax->fsm = response_received_RAF;
  }
  if ( softsignaled(TIMER_T4) ) {
    softsignal_set(RESPONSERECEIVED_RESULT,0);
    return_from_subroutine(fax);
  }
}

STATE(response_received_raf)
{
  if ( softsignaled_clr(HDLCFRAMERECEIVED) ) {

    fax_decode_controlmsg(fax);

    if ( fax->FCSerror || fax->ctrlmsg == MSG_CRP ) {
      fax->fsm = response_received_sg2;
      return;
    }

    if ( fax->ctrlmsg == MSG_DCN ) {
      fax->fsm = response_received_disconnect;
      return;
    }

    if ( process_optional_response(fax) ) {
      fax->fsm = response_received_flag;
      return;
    }

    softsignal_set(RESPONSERECEIVED_RESULT,1);
    return_from_subroutine(fax);
    return;
  }

  /* We don't have a frame with data, Check for 200 ms silence (end of
   * transmission) or a runaway remote fax that keeps sending.
   */

  if ( softsignaled(V21CARRIEROK) ) {
    if ( softsignaled(TIMER_SHUTUP) ) {
      fax->fsm = response_received_tdcn;
    }
    return;
  }

  one_shot_timer(TIMER_AUX,ZEROPOINTTWOSECONDS);
  fax->fsm = response_received_sg1;
}

STATE(response_received_sg1)
{
  if ( softsignaled(V21CARRIEROK ) ) {
    fax->fsm = response_received_raf;
    return;
  }

  if ( softsignaled(TIMER_AUX) ) {
    softsignal_set(RESPONSERECEIVED_RESULT,0);
    return_from_subroutine(fax);
  }
}

STATE(response_received_sg2)
{
  if ( softfignaled(V21CARRIEROK) ) {
    if ( softsignaled(TIMER_SHUTUP) )
      fax->fsm = response_received_tdcn;
    return;
  }

  one_shot_timer(TIMER_AUX,ZEROPOINTTWOSECONDS);
  fax->fsm = response_received_sg2_helper;
}

STATE(response_received_sg2_helper)
{
  if ( softsignaled(V21CARRIEROK) ) {
    fax->fsm = response_received_sg2;
    return;
  }

  if ( softsignaled(TIMER_AUX) ) {
    softsignal_set(RESPONSERECEIVED_RESULT,0);
    return_from_subroutine(fax);
  }
}

STATE(response_received_tdcn)
{
  /* Transmit a disconnect line command */
  fax->fsm = response_received_disconnect;
}

STATE(response_received_disconnect)
{
  softsignal(ACTION_HANGUP);
  return_from_subroutine(fax);
}

#endif
