/* $Id$
******************************************************************************

   Fax program for ISDN.
   Common defines for the G3-fax subsystem

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

#ifndef _G3_FAX_H
#define _G3_FAX_H

struct G3fax {
  ifax_modp linedriver;
  ifax_modp rateconv7k2to8k0;
  ifax_modp sinusCED;
  ifax_modp sinusCNG;
  ifax_modp silence;
  ifax_modp scrambler;
  ifax_modp modulatorV21;
  ifax_modp modulatorV29;
  void (*fsm)(struct G3fax *);
  void (*simple_wait_next_state)(struct G3fax *);
};

#endif