/* $Id$
******************************************************************************

   Fax program for ISDN.
   Initialization of G3-fax subsystem

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

#include <ifax/module.h>
#include <ifax/types.h>
#include <ifax/misc/regmodules.h>
#include <ifax/G3/fax.h>
#include <ifax/G3/fsm.h>
#include <ifax/modules/signalgen.h>

/* Import rate-converter filter */
extern ifax_sint16 rate_7k2_8k_1[250];

struct G3fax *initialize_G3fax(ifax_modp linedriver)
{
  struct G3fax *fax;

  fax = ifax_malloc(sizeof(*fax),"G3-fax handle");

  /* The linedriver is responsible for reading samples from the phone-line
   * and pushing them into the demodulator(s), and ask the modulators for
   * samples to be transmitted.
   */
  /* NOTE: Global linedriver used now.
     fax->linedriver = ifax_create_module(IFAX_LINEDRIVER); */

  /* The modulators output with a sampeling rate of 7200 Hz.  This
   * rateconverter is used to upsample to 8000 Hz (phone-line rate).
   * The reason for this is that 7200 is a multiple of 2400 and 300 which
   * is the symbol-rates used by V.29 and V.21 (and easy to work with).
   */
  fax->rateconv7k2to8k0 = ifax_create_module(IFAX_RATECONVERT,10,9,250,
					     rate_7k2_8k_1,0x10000);

  /* Create the "binary coded signal" modulator, which is simply a
   * 300 bit/s modulator just like channel 2 of the V.21 standard.
   * The control messages are transmitted using this slower (but more
   * robust) modulation standard.
   */
  fax->modulatorV21 = ifax_create_module(IFAX_MODULATORV21,2);

  /* The modulation used for the fax-messages is a lot faster and
   * more complicated.  The V.29 offers 9600 and 7200 bit/s for fax
   * transfers.  There are other standards that can be used to get
   * faster transfers, but V.29 is the one that *must* be available.
   */
  fax->modulatorV29 = ifax_create_module(IFAX_MODULATORV29);

  /* When using V.29 the synchronous bitstream must be scrambled before
   * it is modulated.  The scrambler is located in a separate module
   * since it is common to several modulation standards.
   */
  fax->scrambler = ifax_create_module(IFAX_SCRAMBLER);

  /* The initial simple establishment phase (no V.8 involved) uses just
   * a few sinus signals.  The sinus-signal generators are initialized
   * onse and for all and brought online when needed for the CNG and CED
   * signals.  The silence generator is used during periods of silence.
   */

  fax->sinusCNG = ifax_create_module(IFAX_SIGNALGEN);
  ifax_command(fax->sinusCNG,CMD_SIGNALGEN_SINUS,7200,1100,0xA000);

  fax->sinusCED = ifax_create_module(IFAX_SIGNALGEN);
  ifax_command(fax->sinusCED,CMD_SIGNALGEN_SINUS,7200,2100,0xA000);

  fax->silence = ifax_create_module(IFAX_SIGNALGEN);
  ifax_command(fax->silence,CMD_SIGNALGEN_SINUS,7200,440,0);

  /* The HDLC-encode is used both by the "binary coded signal"
   * and high-speed fax transfers.
   */

  fax->encoderHDLC = ifax_create_module(IFAX_ENCODER_HDLC);


  ifax_connect(fax->rateconv7k2to8k0,linedriver);

  return fax;
}


/* This function is called when there is an accepted incomming  call
 * on the global 'linedriver' that needs to be serviced.  It is
 * responsible for setting up the state-machines etc.
 */

void fax_prepare_incomming(struct G3fax *fax)
{
  initialize_fsm_incomming(fax);
}
