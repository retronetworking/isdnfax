/* $Id$
******************************************************************************

   Fax program for ISDN.
   Main state-machine for G3-fax handeling.

   Copyright (C) 1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
   Copyright (C) 1999 Thomas Reinemannn [tom.reinemann@gmx.net]

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

#include <stdio.h>

#include <ifax/types.h>
#include <ifax/G3/fax.h>
#include <ifax/G3/kernel.h>
#include <ifax/G3/g3-timers.h>
#include <ifax/G3/commandframes.h>
#include <ifax/misc/timers.h>
#include <ifax/misc/softsignals.h>
#include <ifax/misc/statemachine.h>
#include <ifax/modules/hdlc-framing.h>


/* All timing is done in units of 1/8000 seconds (one ISDN-sample) */
// How about tolerances?

#define SEVENTYFIVEMILLISECONDS                600
#define ZEROPOINTTWOSECONDS                   1600
#define ZEROPOINTFIFESECONDS		      4000
#define ONESECOND                             8000
#define THREESECONDS			     24000
#define THREEPOINTEIGHTSECONDS               30400
#define SIXSECONDS			     48000
#define TENSECONDS			     80000
#define THIRTYFIVESECONDS		    280000
#define SIXTYSECONDS			    480000

#define T1_TIME                        THIRTYFIVESECONDS
#define T2_TIME			       SIXSECONDS
#define T3_TIME  		       TENSECONDS
#define T5_TIME			       SIXTYSECONDS

/* Global states that may get called from other files as well as from
 * this one.
 */

FSM_DEFSTATE(fsm_wait_softsignal)
FSM_DEFSTATE(do_hard_exit)


/* List states here that is called in a forward fashion (most of them) */

FSM_DEFSTATE(start_answer_incomming)
FSM_DEFSTATE(do_CED)
FSM_DEFSTATE(done_CED)
FSM_DEFSTATE(start_DIS)
FSM_DEFSTATE(do_DIS)
FSM_DEFSTATE(done_DIS)
FSM_DEFSTATE(hunt_for_DCS_or_DTC)


/* Jump to 'start_answer_incomming' when an incomming call is
 * accepted (answered) and we identify ourselves as a fax-machine.
 * The initialize_fsm_incomming() function is used to initialize the fsm
 * and signal-chain to handle an incomming call and jump to this state.
 */

void fax_initialize_fsm_incomming()
{
	/* Prepare data-frames in 'fax' structure */
	fax_setup_outgoing_DIS();
	fax_setup_outgoing_NSF();
	fax_setup_outgoing_CSI();

	fsm_init(fax->statemachines,0,start_answer_incomming,100,fax);

	ifax_connect(fax->silence,fax->rateconv7k2to8k0);  /* Start silent */
}

#define NEEDS_none	/* No state-machine variables needed */

FSM_STATE(NEEDS_none,start_answer_incomming)
	/* Stay silent for 0.2 seconds before outputing the CED */
	FSMWAITJUMP(TIMER_AUX,ZEROPOINTTWOSECONDS,do_CED);
FSM_END

FSM_STATE(NEEDS_none,do_CED)
	/* Output the CED sinus signal for 3.8 sec */
	ifax_connect(fax->sinusCED,fax->rateconv7k2to8k0);
	FSMWAITJUMP(TIMER_AUX,THREEPOINTEIGHTSECONDS,done_CED);
FSM_END

FSM_STATE(NEEDS_none,done_CED)
	/* After the CED, wait 75ms and do the DIS */
	ifax_connect(fax->silence,fax->rateconv7k2to8k0);
	FSMWAITJUMP(TIMER_AUX,SEVENTYFIVEMILLISECONDS,start_DIS);
FSM_END

FSM_STATE(NEEDS_none,start_DIS)
	/* When we hook up the HDLC+V.21 they go online and send FLAGs */
	ifax_connect(fax->modulatorV21,fax->rateconv7k2to8k0);
	ifax_connect(fax->encoderHDLC,fax->modulatorV21);

	/* Keep sending FLAGs for one second before proceeding with frames */
	FSMWAITJUMP(TIMER_AUX,ONESECOND,do_DIS);
FSM_END

FSM_STATE(NEEDS_none,do_DIS)
	/* After one second of FLAGs, we transmit the DIS etc. */

	if ( fax->NSFsize ) {
		/* There is a NSF */
		ifax_command(fax->encoderHDLC,CMD_HDLC_FRAMING_TXFRAME,
			     fax->NSF,fax->NSFsize,255);
	}

	if ( fax->CSIsize ) {
		/* There is a CSI */
		ifax_command(fax->encoderHDLC,CMD_HDLC_FRAMING_TXFRAME,
			     fax->CSI,fax->CSIsize,255);
	}

	/* Assume there is a DIS (it better...) */
	ifax_command(fax->encoderHDLC,CMD_HDLC_FRAMING_TXFRAME,
		     fax->DIS,fax->DISsize,255);

	FSMJUMP(done_DIS);
FSM_END

FSM_STATE(NEEDS_none,done_DIS)
	/* Wait until the HDLC-frames has been transmitted before receiving */
	if ( ifax_command(fax->encoderHDLC,CMD_HDLC_FRAMING_IDLE) > 2 ) {
		ifax_connect(fax->silence,fax->rateconv7k2to8k0);
		FSMJUMP(hunt_for_DCS_or_DTC);
	}
FSM_END

FSM_STATE(NEEDS_none,hunt_for_DCS_or_DTC)
	/* Stay online a little to flush buffers, then exit */
	FSMWAITJUMP(TIMER_AUX,ONESECOND,do_hard_exit);
FSM_END


/**********************************************************************
 *
 * Utility functions/states/subroutines.
 */

/* The 'fsm_wait_softsignal' is a subroutine called by the 'FSMWAIT' macro
 * to perform the actual waiting... May be used to wait for other signals
 * as well.
 */

FSM_GLOBAL_STATE(NEEDS_none,fsm_wait_softsignal)
	if ( softsignaled_clr(FSMGETARG) ) {
		FSMRETURN(0);
	}
FSM_END

FSM_GLOBAL_STATE(NEEDS_none,do_hard_exit)
	exit(17);
FSM_END

#if 0

/* The following code can't be used yet, because so many other modules
 * lack the softsignaling functionality when certain events occour.
 * Some other modules required has not been written yet.
 */



/**********************************************************************
 * SENDING A FAX 
 * Jump to 'start_sending_fax' when an user wishes to send a fax
 * The initialize_fsm_sending() function is used to initialize the fsm
 * and signal-chain to handle sending a fax.
 */


DEFSTATE(start_sending_fax)
DEFSTATE(con_established)
DEFSTATE(A_CNG_sound)
DEFSTATE(A_CNG_silence)
DEFSTATE(entry_PhaseB)

void fax_initialize_fsm_outgoing()
{

  initialize_statemachines();                /* Reset all state-machines for a fresh start */
  init_fsm(FSM_FAX_MAIN,start_sending_fax);  /* Get the main one going */
  ifax_connect(fax->silence,fax->rateconv7k2to8k0);  /* Start off silent of CNG*/

}

FSM_STATE (start_sending_fax)
{ 
  one_shot_timer(TIMER_DIAL, THIRTYFIVESECONDS);
  one_shot_timer(TIMER_AUX, THREESECONDS);
  fsmself->state = A_CNG_sound;
}

FSM_STATE (A_CNG_silence)
{
  if ( softsignaled(TIMER_AUX) ) {
    ifax_connect(fax->sinusCNG,fax->rateconv7k2to8k0);
    one_shot_timer(TIMER_AUX, ZEROPOINTFIFESECONDS);
    fsmself->state = A_CNG_sound;
    return;
  }
  if (softsignaled (CED)) {
    fsmself->state = entry_PhaseD;  // should entry_PhaseB
    return;
  }
  if (softsignaled(TIMER_DIAL)) {
    softsignal(ACTION_HANGUP);
    return_from_subroutine (fax);
    return;
  }
}

FSM_STATE (A_CNG_sound)
{
  if ( softsignaled(TIMER_AUX) ) {
    ifax_connect(fax->silence,fax->rateconv7k2to8k0);
    one_shot_timer(TIMER_AUX, THREESECONDS);
    fsmself->state = A_CNG_silence;
    return;
  }
  if (softsignaled (CED)) {
    fsmself->state = entry_PhaseD;  // should entry_PhaseB
    return;
  }
  if (softsignaled(TIMER_DIAL)) {
    softsignal(ACTION_HANGUP);
    return_from_subroutine (fax);
    return;
  }
}


/****************************************************************************
 *
 *  Phase B, FIGURE 5-2a/T.30
 *
 *  State-machine for phase B of transmitting terminal
 */

DEFSTATE(B_CNG_sound)
DEFSTATE(B_CNG_silence)


FSM_STATE (entry_PhaseB) {
  one_shot_timer(TIMER_T1, T1_TIME);  
}

FSM_STATE (B_CNG_silence)
{
  if ( softsignaled(TIMER_AUX) ) {
    ifax_connect(fax->sinusCNG,fax->rateconv7k2to8k0);
    one_shot_timer(TIMER_AUX, ZEROPOINTFIFESECONDS);
    fsmself->state = B_CNG_sound;
    return;
  }
  if (softsignaled_clr(DIS_RECEIVED) || softsignaled_clr (DTC_RECEIVED)) {
    fsmself->state = entry_PhaseB;
    return;
  }
  if (softsignaled(T1)) {
    softsignal(ACTION_HANGUP);
    return_from_subroutine (fax);
    return;
  }
}

FSM_STATE (B_CNG_sound)
{
  if ( softsignaled(TIMER_AUX) ) {
    ifax_connect(fax->silence,fax->rateconv7k2to8k0);
    one_shot_timer(TIMER_AUX, THREESECONDS);
    fsmself->state = B_CNG_silence;
    return;
  }
  if (softsignaled_clr(DIS_RECEIVED) || softsignaled_clr (DTC_RECEIVED)) {
    fsmself->state = entry_PhaseB;
    return;
  }
  if (softsignaled(T1)) {
    softsignal(ACTION_HANGUP);
    return_from_subroutine (fax);
    return;
  }
}



/****************************************************************************
 *
 *  Phase E; Call Release, FIGURE 5-2d/T.30
 *
 *  This is the call release part. It contains only the entry point to
 *  disconnect the line.
 *
 */

DEFSTATE (Entry_B)


FSM_STATE(NEEDS_none,Entry_B)
{
  softsignal(ACTION_HANGUP);
  return_from_subroutine(fax);
}


/****************************************************************************
 *
 *  Response Received, FIGURE 5-2s/T.30
 *
 *  This state-machine subroutine checks if a valid response has
 *  arrived within the time-limits.  If an illegal situation
 *  occours, like the remote transmitting more than it is allowed to,
 *  a hangup is performed.
 *
 *  By doing 'call_subroutine' on the state 'response_received',
 *  the answer will be available in RESPONSERECEIVED_RESULT .
 */

DEFSTATE(response_received_flag)
DEFSTATE(response_received_raf)
DEFSTATE(response_received_sg1)
DEFSTATE(response_received_sg2)
DEFSTATE(response_received_sg2_helper)
DEFSTATE(response_received_tdcn)
DEFSTATE(response_received_disconnect)

FSM_STATE(NEEDS_none,response_received)
{
  one_shot_timer(TIMER_T4,THREESECONDS);
  fsmself->state = response_received_flag;
}

FSM_STATE(NEEDS_none,response_received_flag)
{
  if ( softsignaled_clr(HDLCFLAGDETECTED) ) {
    one_shot_timer(TIMER_SHUTUP,THREESECONDS);
    fsmself->state = response_received_RAF;
  }
  if ( softsignaled(TIMER_T4) ) {
    softsignal_set(RESPONSERECEIVED_RESULT,0);
    return_from_subroutine(fax);
  }
}

FSM_STATE(NEEDS_none,response_received_raf)
{
  if ( softsignaled_clr(HDLCFRAMERECEIVED) ) {

    fax_decode_controlmsg(fax);

    if ( fax->FCSerror || fax->ctrlmsg == MSG_CRP ) {
      fsmself->state = response_received_sg2;
      return;
    }

    if ( fax->ctrlmsg == MSG_DCN ) {
      fsmself->state = response_received_disconnect;
      return;
    }

    if ( process_optional_response(fax) ) {
      fsmself->state = response_received_flag;
      return;
    }

    softsignal(RESPONSERECEIVED_RESULT);
    return_from_subroutine(fax);
    return;
  }

  /* We don't have a frame with data, Check for 200 ms silence (end of
   * transmission) or a runaway remote fax that keeps sending.
   */

  if ( softsignaled(V21CARRIEROK) ) {
    if ( softsignaled(TIMER_SHUTUP) ) {
      fsmself->state = response_received_tdcn;
    }
  }

  one_shot_timer(TIMER_AUX,ZEROPOINTTWOSECONDS);
  fsmself->state = response_received_sg1;
}

FSM_STATE(NEEDS_none,response_received_sg1)
{
  if ( softsignaled(V21CARRIEROK ) ) {
    fsmself->state = response_received_raf;
    return;
  }

  if ( softsignaled(TIMER_AUX) ) {
    softsignal_clr(RESPONSERECEIVED_RESULT);
    return_from_subroutine(fax);
  }
}

FSM_STATE(NEEDS_none,response_received_sg2)
{
  if ( softfignaled(V21CARRIEROK) ) {
    if ( softsignaled(TIMER_SHUTUP) )
      fsmself->state = response_received_tdcn;
    return;
  }

  one_shot_timer(TIMER_AUX,ZEROPOINTTWOSECONDS);
  fsmself->state = response_received_sg2_helper;
}

FSM_STATE(NEEDS_none,response_received_sg2_helper)
{
  if ( softsignaled(V21CARRIEROK) ) {
    fsmself->state = response_received_sg2;
    return;
  }

  if ( softsignaled(TIMER_AUX) ) {
    softsignal_clr(RESPONSERECEIVED_RESULT);
    return_from_subroutine(fax);
  }
}

FSM_STATE(NEEDS_none,response_received_tdcn)
{
  /* Transmit a disconnect line command */
  fsmself->state = response_received_disconnect;
}

FSM_STATE(NEEDS_none,response_received_disconnect)
{
  softsignal(ACTION_HANGUP);
  return_from_subroutine(fax);
}



/****************************************************************************
 *
 *  Command Received, FIGURE 5-2t/T.30
 *
 *  This state-machine subroutine checks if a valid command has
 *  arrived within the time-limits.  If an illegal situation
 *  occours, like the remote transmitting more than it is allowed to,
 *  a hangup is performed.
 *
 *  By doing 'call_subroutine' on the state 'command_received',
 *  the answer will be available in COMMANDRECEIVED_RESULT .
 */


DEFSTATE(command_received_flag)
DEFSTATE(command_received_raf)
DEFSTATE(command_received_sg1)
DEFSTATE(command_received_sg2)
DEFSTATE(command_received_sg2_helper)
DEFSTATE(command_received_disconnect)


FSM_STATE (command_received){
  fsmself->state = command_received_flag;
}

FSM_STATE (command_received_flag)
{
  if ( softsignaled_clr(HDLCFLAGDETECTED) ) {
    one_shot_timer(TIMER_T2,SIXSECONDS); ?????
    fsmself->state = command_received_RAF;
  }
  else {
    softsignal_clr(COMMANDRECEIVED_RESULT);
    return_from_subroutine(fax);
  }
}


FSM_STATE (command_received_raf) {

  if ( softsignaled_clr(HDLCFRAMERECEIVED) ) {

    fax_decode_controlmsg(fax);

    if ( fax->FCSerror) {
      one_shot_timer(TIMER_SHUTUP,THREESECONDS);
      one_shot_timer(TIMER_AUX,ZEROPOINTTWOSECONDS);
      fsmself->state = command_received_sg2;
      return;
    }

    if ( fax->ctrlmsg == MSG_DCN ) {
      fsmself->state = command_received_disconnect;
      return;
    }

    if ( process_optional_command(fax) ) {
      fsmself->state = command_received_flag;
      return;
    }

    softsignal(COMMANDRECEIVED_RESULT);
    return_from_subroutine(fax);
    return;
  }
  else {
    one_shot_timer(TIMER_SHUTUP,THREESECONDS);
    one_shot_timer(TIMER_AUX,ZEROPOINTTWOSECONDS);
    fsmself->state = command_received_sg1;
  }
}

  /* We don't have a frame with data, Check for 200 ms silence (end of
   * transmission) or a runaway remote fax that keeps sending.
   */

FSM_STATE(NEEDS_none,command_received_sg1) {

  if ( softsignaled(V21CARRIEROK) ) {
    if ( softsignaled(TIMER_AUX)  ) {  // 0.2 seconds
      softsignal_clr(COMMANDRECEIVED_RESULT);
      return_from_subroutine(fax);
    }
  }
  else
    if ( softsignaled (TIMER_SHUTUP)) {
      softsignal_clr(COMMANDRECEIVED_RESULT);
      return_from_subroutine(fax);
    }
    else {
      fsmself->state = command_received_raf;
    }
  }
}


FSM_STATE(NEEDS_none,command_received_sg2) {

  if ( softsignaled(V21CARRIEROK) ) {
    if ( softsignaled(TIMER_AUX) ) {  // 0.2 seconds
      if (CRP option)
        Response CRP;
      fsmself->state = command_received_raf;
    }
  }
  else
    if ( softsignaled (TIMER_SHUTUP)) {
      fsmself->state = command_received_disconnect;
    }
  }
}



FSM_STATE(NEEDS_none,command_received_disconnect)
{
  softsignal(ACTION_HANGUP);
  return_from_subroutine(fax);
}


/****************************************************************************
 *
 *  Phase D, FIGURE 5-2c/T.30
 *
 *  State-machine for phase D of transmitting terminal
 */


#endif
