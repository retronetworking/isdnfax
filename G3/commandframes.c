/* $Id$
******************************************************************************

   Fax program for ISDN.
   Assemble frames used to command/respond remote party.

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

#include <string.h>
#include <ifax/types.h>
#include <ifax/G3/fax.h>
#include <ifax/G3/commandframes.h>


/* Helper function to assign individual bits in a command frame.  The
 * bit-ordering translation is defined here.
 */

static void assignbit(void *start, int bitpos, int bitval)
{
  ifax_uint32 bytebitnum, bytenum;
  ifax_uint8 *bytearray, bitmask;

  bitpos--;
  bytebitnum = bitpos & 7;
  bytenum = bitpos >> 3;
  bytearray = start;

  if ( bytebitnum ) {
    bitmask = 1 << bytebitnum;
  } else {
    bitmask = 1;
  }

  bytearray[bytenum] = (bytearray[bytenum] & (0xFF ^ bitmask));
  if ( bitval & 1 )
    bytearray[bytenum] |= bitmask;
}

void assignfield(void *start, int bitpos, int fieldsize, int fieldvalue)
{
  int t;

  for ( t = fieldsize; t > 0 ; t-- )
    assignbit(start, bitpos+t-1, fieldvalue & 1);
}


/* Fill the fax->DIS and fax->DISsize with proper values here */

void fax_setup_outgoing_DIS(void)
{
  memset(fax->DIS,0,10);    /* Start with 0's, most unused values are zero */
  assignfield(fax->DIS,11,4,0x08); /* Data signaling rate = V.29 */
  assignbit(fax->DIS,16,1); /* Two dimentional coding capability */

  fax->DISsize = 10;

  /* Very preliminary DIS... but it may suffice for now */
}

void fax_setup_outgoing_CSI(void)
{
  fax->CSIsize = 0;
}

void fax_setup_outgoing_NSF(void)
{
  fax->NSFsize = 0;
}
