/* $Id$
******************************************************************************

   Fax program for ISDN.
   Software only signals to control state machines

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

/* BUG: Should probably be handled by macros, leave it alone for now */

#include <ifax/misc/softsignals.h>

ifax_uint8 softsignal_tbl[MAX_SOFTSIGNALS];

void softsignal(int signum)
{
  softsignal_tbl[signum] = 1;
}

void reset_softsignals(void)
{
  int t;

  for ( t=0; t < MAX_SOFTSIGNALS; t++ )
    softsignal_tbl[t] = 0;
}

int softsignaled(int signum)
{
  return softsignal_tbl[signum];
}

int softsignaled_clr(int signum)
{
  int ret;

  ret = softsignal_tbl[signum];
  softsignal_tbl[signum] = 0;

  return ret;
}
