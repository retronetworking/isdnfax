/* $Id$
******************************************************************************

   Fax program for ISDN.
   Take care of the main housekeeping-work in the fax-system

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

// handling the FSM

#include <ifax/G3/fax.h>

void fax_run_internals(struct G3fax *fax)
{
  void (*prev)(struct G3fax *);

  do {
    prev = fax->fsm;
    (fax->fsm)(fax);
  } while ( prev != fax->fsm );
}

void call_subroutine(struct G3fax *fax) {

  printf ("I'm empty: call_subroutine\n");

}

void return_from_subroutine(struct G3fax *fax) {

  printf ("I'm empty: return_from_subroutine\n");

}