/* $Id$
******************************************************************************

   Fax program for ISDN.

   Copyright (C) 1998 Andreas Beck   [becka@ggi-project.org]
   Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]

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
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <ifax/ifax.h>

#include <ifax/modules/generic.h>
#include <ifax/modules/debug.h>
#include <ifax/modules/faxcontrol.h>
#include <ifax/modules/scrambler.h>
#include <ifax/modules/modulator-V29.h>
#include <ifax/modules/modulator-V21.h>
#include <ifax/modules/signalgen.h>
#include <ifax/modules/linedriver.h>
#include <ifax/modules/V.29_demod.h>


int send_to_audio_construct (ifax_modp self, va_list args);
int pulsegen_construct (ifax_modp self, va_list args);
int sinegen_construct (ifax_modp self, va_list args);
int rateconvert_construct (ifax_modp self, va_list args);
int fskdemod_construct (ifax_modp self, va_list args);
int fskmod_construct (ifax_modp self, va_list args);
int decode_serial_construct (ifax_modp self, va_list args);
int decode_hdlc_construct (ifax_modp self, va_list args);
int encode_serial_construct (ifax_modp self, va_list args);
int debug_construct (ifax_modp self, va_list args);
int V29demod_construct (ifax_modp self, va_list args);
int syncbit_construct(ifax_modp self,va_list args);

extern ifax_sint16 rate_7k2_8k_1[250];
extern ifax_sint16 rate_8k_7k2_1[252];

#include <ifax/modules/replicate.h>

ifax_module_id IFAX_TOAUDIO;
ifax_module_id IFAX_PULSEGEN;
ifax_module_id IFAX_SINEGEN;
ifax_module_id IFAX_REPLICATE;
ifax_module_id IFAX_SCRAMBLER;
ifax_module_id IFAX_MODULATORV29;
ifax_module_id IFAX_MODULATORV21;
ifax_module_id IFAX_RATECONVERT;
ifax_module_id IFAX_FSKDEMOD;
ifax_module_id IFAX_FSKMOD;
ifax_module_id IFAX_DECODE_SERIAL;
ifax_module_id IFAX_ENCODE_SERIAL;
ifax_module_id IFAX_DECODE_HDLC;
ifax_module_id IFAX_DEBUG;
ifax_module_id IFAX_FAXCONTROL;
ifax_module_id IFAX_LINEDRIVER;
ifax_module_id IFAX_SIGNALGEN;
ifax_module_id IFAX_V29DEMOD;
ifax_module_id IFAX_SYNCBIT;

void
setup_all_modules (void)
{
  IFAX_TOAUDIO = ifax_register_module_class ("Audio output", send_to_audio_construct);
  IFAX_PULSEGEN = ifax_register_module_class ("Square wave generator", pulsegen_construct);
  IFAX_SINEGEN = ifax_register_module_class ("Sine wave generator", sinegen_construct);
  IFAX_REPLICATE = ifax_register_module_class ("Replicator", replicate_construct);
  IFAX_FSKDEMOD = ifax_register_module_class ("FSK demodulator", fskdemod_construct);
  IFAX_FSKMOD = ifax_register_module_class ("FSK modulator", fskmod_construct);
  IFAX_DECODE_SERIAL = ifax_register_module_class ("Serializer", decode_serial_construct);
  IFAX_ENCODE_SERIAL = ifax_register_module_class ("Serial encoder", encode_serial_construct);
  IFAX_DECODE_HDLC = ifax_register_module_class ("HDLC decoder", decode_hdlc_construct);
  IFAX_SCRAMBLER = ifax_register_module_class ("Bitstream scrambler", scrambler_construct);
  IFAX_MODULATORV29 = ifax_register_module_class ("V.29 Modulator", modulator_V29_construct);
  IFAX_MODULATORV21 = ifax_register_module_class ("V.21 Modulator", modulator_V21_construct);
  IFAX_RATECONVERT = ifax_register_module_class ("Sample-rate converter", rateconvert_construct);
  IFAX_DEBUG = ifax_register_module_class ("Debugger", debug_construct);
  IFAX_FAXCONTROL = ifax_register_module_class ("Fax control", faxcontrol_construct);
  IFAX_SIGNALGEN = ifax_register_module_class ("Simple signal generator", signalgen_construct);
  IFAX_LINEDRIVER = ifax_register_module_class ("Linedriver", linedriver_construct);
  IFAX_V29DEMOD = ifax_register_module_class ("V.29 Demodulator", V29demod_construct);
  IFAX_SYNCBIT = ifax_register_module_class("Bit syncronization",syncbit_construct);
}

void
transmit_carrier (void)
{
  ifax_modp toaudio, square, sine, replicate;
  char data;
  int cnt;

  /* Replicate the incoming signal. */
  replicate = ifax_create_module (IFAX_REPLICATE);

  /* Generate an square wave of 0.5s on, 3s off - CNG */
  square = ifax_create_module (IFAX_PULSEGEN, SAMPLES_PER_SECOND / 2,
			       3 * SAMPLES_PER_SECOND);

  /* Generate 1100Hz */
  sine = ifax_create_module (IFAX_SINEGEN, 1100);

  /* Test - go to stdout */
  toaudio = ifax_create_module (IFAX_TOAUDIO, write, 1);
  ifax_command (replicate, CMD_REPLICATE_ADD, square);
  square->sendto = sine;
  sine->sendto = toaudio;

  /* Feed in dummy input data. */
  for (cnt = 0; cnt < 10 * SAMPLES_PER_SECOND; cnt++)
    ifax_handle_input (replicate, &data, 1);

}

#if 0
void
test_modulator_V29 (void)
{
  ifax_modp scrambler, modulator, rateconvert, debug;
  unsigned char data;
  unsigned int status;

  /* Test scrambler as a module */
  scrambler = ifax_create_module (IFAX_SCRAMBLER);

  /* Modulate into signed shorts */
  modulator = ifax_create_module (IFAX_MODULATORV29);

  /* Rateconvert from 7200 Hz to 8000 Hz */
  rateconvert = ifax_create_module (IFAX_RATECONVERT, 10, 9, 250,
				    rate_7k2_8k_1, 0x10000);

  /* Print samples of standard output for analysis */
  debug = ifax_create_module (IFAX_DEBUG, 0, DEBUG_FORMAT_SIGNED16BIT,
			      DEBUG_METHOD_STDOUT);

  scrambler->sendto = modulator;
  modulator->sendto = rateconvert;
  rateconvert->sendto = debug;


  /* Initialize the scrambler, and start a V.29 * synchronization sequence.
     When we reach  * segment 4, we let the scrambler take over * and feed it 
     with 1's until the modulator * enters the DATA phase. */

  ifax_command (scrambler, CMD_SCRAMBLER_INIT);
  ifax_command (modulator, CMD_MODULATORV29_STARTSYNC);

  for (;;)
    {
      ifax_command (modulator, CMD_MODULATORV29_MAINTAIN, &status);
      if (status & MODULATORV29_STATUS_SEGMENT4)
	break;
    }

  data = 255;
  for (;;)
    {
      ifax_handle_input (scrambler, &data, 1);
      ifax_command (modulator, CMD_MODULATORV29_MAINTAIN, &status);
      if (status & MODULATORV29_STATUS_DATA)
	break;
    }

  /* The synchronization sequence has now completed, * and we may continue
     with payload data. */
}
#endif

void
test_scrambler (void)
{
  /* The new scrambler/descrambler in modules/scrambler.c should be *
     compliant with V.29.  Run a small empirical test here of *
     encding/decoding. */

  int t;
  ifax_modp scrambler, descrambler, debug;
  unsigned char source[200];

  debug = ifax_create_module (IFAX_DEBUG, 1);
  scrambler = ifax_create_module (IFAX_SCRAMBLER);
  descrambler = ifax_create_module (IFAX_SCRAMBLER);

  ifax_command (descrambler, CMD_SCRAMBLER_DESCR_V29);

  scrambler->sendto = descrambler;
  descrambler->sendto = debug;

  for (t = 0; t < 200; t++)
    source[t] = t;

  ifax_handle_input (scrambler, source, 200);
}


/* Test new V.21 modulator */

void
test_modulator_V21 (void)
{
  ifax_modp modulator, rateconvert, debug;
  unsigned char data;
  int t, v;

  /* Modulate into signed shorts */
  modulator = ifax_create_module (IFAX_MODULATORV21, 2);

  /* Rateconvert from 7200 Hz to 8000 Hz */
  rateconvert = ifax_create_module (IFAX_RATECONVERT, 10, 9, 250,
				    rate_7k2_8k_1, 0x10000);

  if (rateconvert == 0)
    exit (42);

  /* Print samples of standard output for analysis */
  debug = ifax_create_module (IFAX_DEBUG, 0, DEBUG_FORMAT_SIGNED16BIT,
			      DEBUG_METHOD_STDOUT);

  if (debug == 0)
    exit (43);

  modulator->sendto = rateconvert;
  rateconvert->sendto = debug;

  v = 0xFFF407F;

  for (t = 0; t < 20000; t++)
    {
      data = t;
      ifax_handle_input (modulator, &data, 1);
    }
}

/* HDLC testing code
 */
void
test_hdlc (void)
{
  /* module handles for all used modules. */
  ifax_modp fskd, debug, dehdlc, faxcntrl;

  /* helper for data in-/output */
  unsigned char data;

  /* Print samples of standard output for analysis */
  debug = ifax_create_module (IFAX_DEBUG, 0, DEBUG_FORMAT_16BIT_HEX,
			      DEBUG_METHOD_STDOUT);

  faxcntrl = ifax_create_module (IFAX_FAXCONTROL);

  /* The deserializer synchronizes on the startbits and decodes * from the
     0/1 stream from the demodulator to the bytes. * Its output is sent to
     the totty module. See above. */
  dehdlc = ifax_create_module (IFAX_DECODE_HDLC, 300);
  dehdlc->sendto = faxcntrl;

  /* The FSK demodulator. Takes the aLaw input stream and sends the * decoded 
     version to the deserializer. See above.  */
  fskd = ifax_create_module (IFAX_FSKDEMOD, 1650, 1850, 300);
  fskd->sendto = dehdlc;

  /* Run until explicitly terminated. */
  while (1)
    {

      if (read (0, &data, 1) != 1)
	break;

      /* Send it to the demodulator. */
      ifax_handle_input (fskd, &data, 1);
    }
}

void
test_linedriver (void)
{
  ifax_modp signalgen, scrambler, modulator, rateconvert, linedriver;
  int remaining;

  /* Generate a test-signal */
  signalgen = ifax_create_module (IFAX_SIGNALGEN);
  ifax_command (signalgen, CMD_SIGNALGEN_RNDBITS);

  /* Set up the V.29 scrambler */
  scrambler = ifax_create_module (IFAX_SCRAMBLER);
  ifax_command (scrambler, CMD_SCRAMBLER_SCRAM_V29);

  /* Modulate into signed shorts */
  modulator = ifax_create_module (IFAX_MODULATORV29);

  /* Rateconvert from 7200 Hz to 8000 Hz */
  rateconvert = ifax_create_module (IFAX_RATECONVERT, 10, 9, 250,
				    rate_7k2_8k_1, 0x10000);

  /* Feed the signal to the line-driver for transmission */
  linedriver = ifax_create_module (IFAX_LINEDRIVER);
  ifax_command (linedriver, CMD_LINEDRIVER_AUDIO);

  signalgen->sendto = scrambler;
  scrambler->recvfrom = signalgen;

  scrambler->sendto = modulator;
  modulator->recvfrom = scrambler;

  modulator->sendto = rateconvert;
  rateconvert->recvfrom = modulator;

  rateconvert->sendto = linedriver;
  linedriver->recvfrom = rateconvert;

  ifax_command (modulator, CMD_GENERIC_INITIALIZE);
  remaining = 20260;
  while (remaining > 0)
    {
      remaining -= ifax_command (linedriver, CMD_LINEDRIVER_WORK);
    }
}


void
test_v29demod (void)
{
  ifax_modp signalgen, scrambler, modulator, rateconvert;
  ifax_modp debug, linedriver, v29demod;
  int remaining;

  /* Generate a test-signal */
  signalgen = ifax_create_module (IFAX_SIGNALGEN);
  ifax_command (signalgen, CMD_SIGNALGEN_RNDBITS);

  /* Set up the V.29 scrambler */
  scrambler = ifax_create_module (IFAX_SCRAMBLER);
  ifax_command (scrambler, CMD_SCRAMBLER_SCRAM_V29);

  /* Modulate into signed shorts */
  modulator = ifax_create_module (IFAX_MODULATORV29);

  /* Rateconvert from 7200 Hz to 8000 Hz */
  rateconvert = ifax_create_module (IFAX_RATECONVERT, 10, 9, 250,
				    rate_7k2_8k_1, 0x10000);

  debug = ifax_create_module (IFAX_DEBUG, 0, DEBUG_FORMAT_SIGNED16BIT,
			      DEBUG_METHOD_STDOUT);

  /* Feed the signal to the line-driver for transmission */
  linedriver = ifax_create_module (IFAX_LINEDRIVER);
  ifax_command (linedriver, CMD_LINEDRIVER_AUDIO);

  /* V.29 Demodulation */
  v29demod = ifax_create_module (IFAX_V29DEMOD);


  signalgen->sendto = scrambler;
  scrambler->recvfrom = signalgen;

  scrambler->sendto = modulator;
  modulator->recvfrom = scrambler;

  modulator->sendto = linedriver;
  /* modulator->sendto = rateconvert; rateconvert->recvfrom = modulator;  
     rateconvert->sendto = linedriver; linedriver->recvfrom = rateconvert; */
  linedriver->recvfrom = modulator;
  linedriver->sendto = v29demod;

  ifax_command (modulator, CMD_GENERIC_INITIALIZE);
  remaining = 1680;
/*  remaining = 528;*/
  while (remaining > 0)
    {
      remaining -= ifax_command (linedriver, CMD_LINEDRIVER_WORK);
    }
}


void test_new_v21_demod(void)
{
  ifax_modp v21mod, rateconvert, v21demod, sync;
  ifax_modp debug;

  /* V.21 channel 1 modulator to make a clean signal at 7200 samples/s */
  v21mod = ifax_create_module(IFAX_MODULATORV21,1);
  assert(v21mod!=0);

  /* Rateconvert from 7200 Hz to 8000 Hz */
  rateconvert = ifax_create_module(IFAX_RATECONVERT, 10, 9, 250,
				   rate_7k2_8k_1, 0x10000);
  assert(rateconvert!=0);

  /* V.21 demodulator */
  v21demod = ifax_create_module(IFAX_FSKDEMOD,8000,980,1180,300);
  assert(v21demod!=0);

  /* Syncronization circuit */
  sync = ifax_create_module(IFAX_SYNCBIT,8000,300);
  assert(sync!=0);

  /* Debug result ... */
  debug = ifax_create_module(IFAX_DEBUG, 0, DEBUG_FORMAT_CONFIDENCE,
			     DEBUG_METHOD_STDOUT);
  assert(debug!=0);

  /* Line up */
  ifax_connect((ifax_modp)0,v21mod);
  ifax_connect(v21mod,rateconvert);
  ifax_connect(rateconvert,v21demod);
  /* ifax_connect(v21demod,sync); */
  ifax_connect(v21demod,debug);
  ifax_connect(debug,(ifax_modp)0);

  ifax_handle_input(v21mod,"\0\0\0\0\0\0\0\0",64);
}


void main (int argc, char **argv)
{

  ifax_debugsetlevel (DEBUG_INFO);
  setup_all_modules ();

  /* transmit_carrier(); */
  /* test_modulator_V29(); */
  /* test_modulator_V21(); */
  /* test_scrambler(); */
  /* test_hdlc(); */
  /* test_linedriver(); */
  /* test_v29demod (); */
  test_new_v21_demod();

  exit (0);
}
