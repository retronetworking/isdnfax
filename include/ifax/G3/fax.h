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
#include <ifax/types.h>

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
	ifax_modp encoderHDLC;

	ifax_modp fskd, dehdlc, faxctrl;

	struct StateMachinesHandle *statemachines;

	ifax_uint8 DIS[32], CSI[32], NSF[32];
	int DISsize, CSIsize, NSFsize;
};

extern struct G3fax *fax;

/* Control octet; 1st octet of frame */
#define FAX_CNTL_NONLAST_FRAME       0xC0
#define FAX_CNTL_LAST_FRAME          0xC8

/* Facsimile Control Field (FCF); 2nd octet of frame */
#define FAX_FCF_DIRECTION            0x80
#define FAX_FCF_DIS                  0x01
#define FAX_FCF_CSI                  0x02

#endif
