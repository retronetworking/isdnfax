/* $Id$
******************************************************************************

   Fax program for ISDN.
   Register all modules available so they can be referenced/used elsewhere

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

/* All the modules used in the link/DSP-signaling chain must be registered
 * before they can be used.  This file is responsible for registering
 * all modules needed through the 'register_modules()' function.
 */

#include <ifax/ifax.h>
#include <ifax/misc/regmodules.h>

#include <ifax/modules/generic.h>
#include <ifax/modules/debug.h>
#include <ifax/modules/faxcontrol.h>
#include <ifax/modules/scrambler.h>
#include <ifax/modules/modulator-V29.h>
#include <ifax/modules/modulator-V21.h>
#include <ifax/modules/signalgen.h>
#include <ifax/modules/linedriver.h>
#include <ifax/modules/V.29_demod.h>
#include <ifax/modules/replicate.h>

/* FIXME: The following should be in header-files */
extern int send_to_audio_construct (ifax_modp self, va_list args);
extern int pulsegen_construct (ifax_modp self, va_list args);
extern int sinegen_construct (ifax_modp self, va_list args);
extern int rateconvert_construct (ifax_modp self, va_list args);
extern int fskdemod_construct (ifax_modp self, va_list args);
extern int fskmod_construct (ifax_modp self, va_list args);
extern int decode_serial_construct (ifax_modp self, va_list args);
extern int decode_hdlc_construct (ifax_modp self, va_list args);
extern int encode_serial_construct (ifax_modp self, va_list args);
extern int debug_construct (ifax_modp self, va_list args);
extern int V29demod_construct (ifax_modp self, va_list args);


/* These are the actual handles used when refering to a given module */

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


#define REGMODULE(m,d,c) m=ifax_register_module_class(d,c)

void register_modules(void)
{
  REGMODULE(IFAX_TOAUDIO,"Audio output",send_to_audio_construct);
  REGMODULE(IFAX_PULSEGEN,"Square wave generator",pulsegen_construct);
  REGMODULE(IFAX_SINEGEN,"Sine wave generator",sinegen_construct);
  REGMODULE(IFAX_REPLICATE,"Replicator",replicate_construct);
  REGMODULE(IFAX_FSKDEMOD,"FSK demodulator",fskdemod_construct);
  REGMODULE(IFAX_FSKMOD,"FSK modulator",fskmod_construct);
  REGMODULE(IFAX_DECODE_SERIAL,"Serializer",decode_serial_construct);
  REGMODULE(IFAX_ENCODE_SERIAL,"Serial encoder",encode_serial_construct);
  REGMODULE(IFAX_DECODE_HDLC,"HDLC decoder",decode_hdlc_construct);
  REGMODULE(IFAX_SCRAMBLER,"Bitstream scrambler",scrambler_construct);
  REGMODULE(IFAX_MODULATORV29,"V.29 Modulator",modulator_V29_construct);
  REGMODULE(IFAX_MODULATORV21,"V.21 Modulator",modulator_V21_construct);
  REGMODULE(IFAX_RATECONVERT,"Samplerate converter",rateconvert_construct);
  REGMODULE(IFAX_DEBUG,"Debugger",debug_construct);
  REGMODULE(IFAX_FAXCONTROL,"Fax control",faxcontrol_construct);
  REGMODULE(IFAX_SIGNALGEN,"Misc signal generator",signalgen_construct);
  REGMODULE(IFAX_LINEDRIVER,"Linedriver",linedriver_construct);
  REGMODULE(IFAX_V29DEMOD,"V.29 Demodulator",V29demod_construct);
}
