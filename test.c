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

#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <ifax/ifax.h>

#include <ifax/modules/debug.h>
#include <ifax/modules/scrambler.h>
#include <ifax/modules/modulator-V29.h>


int     send_to_audio_construct(ifax_modp self,va_list args);
int     pulsegen_construct(ifax_modp self,va_list args);
int     sinegen_construct(ifax_modp self,va_list args);
int     rateconvert_construct(ifax_modp self, va_list args);
int     fskdemod_construct(ifax_modp self, va_list args);
int     fskmod_construct(ifax_modp self, va_list args);
int     decode_serial_construct(ifax_modp self, va_list args);
int     encode_serial_construct(ifax_modp self, va_list args);
int     debug_construct(ifax_modp self, va_list args);

extern signed short rate_7k2_8k_1[250];

#include <ifax/modules/replicate.h>

ifax_module_id	IFAX_TOAUDIO;
ifax_module_id	IFAX_PULSEGEN;
ifax_module_id	IFAX_SINEGEN;
ifax_module_id	IFAX_REPLICATE;
ifax_module_id  IFAX_SCRAMBLER;
ifax_module_id  IFAX_MODULATORV29;
ifax_module_id  IFAX_RATECONVERT;
ifax_module_id  IFAX_FSKDEMOD;
ifax_module_id  IFAX_FSKMOD;
ifax_module_id  IFAX_DECODE_SERIAL;
ifax_module_id  IFAX_ENCODE_SERIAL;
ifax_module_id  IFAX_DEBUG;

void setup_all_modules(void)
{
	IFAX_TOAUDIO	  = ifax_register_module_class("Audio output",send_to_audio_construct);
	IFAX_PULSEGEN	  = ifax_register_module_class("Square wave generator",pulsegen_construct);
	IFAX_SINEGEN	  = ifax_register_module_class("Sine wave generator",sinegen_construct);
	IFAX_REPLICATE	  = ifax_register_module_class("Replicator",replicate_construct);
	IFAX_FSKDEMOD	  = ifax_register_module_class("FSK demodulator",fskdemod_construct);
	IFAX_FSKMOD	  = ifax_register_module_class("FSK modulator",fskmod_construct);
	IFAX_DECODE_SERIAL= ifax_register_module_class("Serializer",decode_serial_construct);
	IFAX_ENCODE_SERIAL= ifax_register_module_class("Serial encoder",encode_serial_construct);
	IFAX_SCRAMBLER    = ifax_register_module_class("Bitstream scrambler",scrambler_construct);
	IFAX_MODULATORV29 = ifax_register_module_class("V.29 Modulator",modulator_V29_construct);
	IFAX_RATECONVERT  = ifax_register_module_class("Sample-rate converter",rateconvert_construct);
	IFAX_DEBUG        = ifax_register_module_class("Debugger",debug_construct);
}

void transmit_carrier(void)
{
	ifax_modp	toaudio,square,sine,replicate;
	char data;
	int cnt;
	
	/* Replicate the incoming signal. */
	replicate=ifax_create_module(IFAX_REPLICATE);

	/* Generate an square wave of 0.5s on, 3s off - CNG */
	square=ifax_create_module(IFAX_PULSEGEN, SAMPLES_PER_SECOND/2, 
						 3*SAMPLES_PER_SECOND);

	/* Generate 1100Hz */
	sine=ifax_create_module(IFAX_SINEGEN, 1100);

	/* Test - go to stdout */
	toaudio=ifax_create_module(IFAX_TOAUDIO, write, 1);
	ifax_command(replicate,CMD_REPLICATE_ADD,square);
	square->sendto=sine;
	sine  ->sendto=toaudio;
	
	/* Feed in dummy input data. */
	for(cnt=0;cnt<10*SAMPLES_PER_SECOND;cnt++)
		ifax_handle_input(replicate,&data,1);

}

void test_modulator_V29(void)
{
  ifax_modp  scrambler, modulator, rateconvert, debug;
  unsigned char data;
  unsigned int status;

  /* Test scrambler as a module */
  scrambler = ifax_create_module(IFAX_SCRAMBLER);

  /* Modulate into signed shorts */
  modulator = ifax_create_module(IFAX_MODULATORV29);

  /* Rateconvert from 7200 Hz to 8000 Hz */
  rateconvert = ifax_create_module(IFAX_RATECONVERT,10,9,250,
				   rate_7k2_8k_1,0x10000);

  /* Print samples of standard output for analysis */
  debug = ifax_create_module(IFAX_DEBUG,0,DEBUG_FORMAT_SIGNED16BIT,
			     DEBUG_METHOD_STDOUT);

  scrambler->sendto = modulator;
  modulator->sendto = rateconvert;
  rateconvert->sendto = debug;


  /* Initialize the scrambler, and start a V.29
   * synchronization sequence.  When we reach 
   * segment 4, we let the scrambler take over
   * and feed it with 1's until the modulator
   * enters the DATA phase.
   */

  ifax_command(scrambler,CMD_SCRAMBLER_INIT);
  ifax_command(modulator,CMD_MODULATORV29_STARTSYNC);

  for (;;) {
    ifax_command(modulator,CMD_MODULATORV29_MAINTAIN,&status);
    if ( status & MODULATORV29_STATUS_SEGMENT4 )
      break;
  }

  data = 255;
  for (;;) {
    ifax_handle_input(scrambler,&data,1);
    ifax_command(modulator,CMD_MODULATORV29_MAINTAIN,&status);
    if ( status & MODULATORV29_STATUS_DATA )
      break;
  }

  /* The synchronization sequence has now completed,
   * and we may continue with payload data.
   */
}    


void main(int argc,char **argv)
{
	setup_all_modules();

	/* transmit_carrier(); */
	test_modulator_V29();
}
