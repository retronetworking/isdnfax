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
 * When registered, the IFAX_ handles can be used to reference them.
 */

#include <ifax/module.h>
#include <ifax/misc/malloc.h>

extern ifax_module_id IFAX_TOAUDIO;
extern ifax_module_id IFAX_PULSEGEN;
extern ifax_module_id IFAX_SINEGEN;
extern ifax_module_id IFAX_REPLICATE;
extern ifax_module_id IFAX_SCRAMBLER;
extern ifax_module_id IFAX_MODULATORV29;
extern ifax_module_id IFAX_MODULATORV21;
extern ifax_module_id IFAX_RATECONVERT;
extern ifax_module_id IFAX_FSKDEMOD;
extern ifax_module_id IFAX_FSKMOD;
extern ifax_module_id IFAX_DECODE_SERIAL;
extern ifax_module_id IFAX_ENCODE_SERIAL;
extern ifax_module_id IFAX_DECODE_HDLC;
extern ifax_module_id IFAX_DEBUG;
extern ifax_module_id IFAX_FAXCONTROL;
extern ifax_module_id IFAX_LINEDRIVER;
extern ifax_module_id IFAX_SIGNALGEN;
extern ifax_module_id IFAX_V29DEMOD;

extern void register_modules(void);
