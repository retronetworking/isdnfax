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
#include <ifax/misc/readconfig.h>


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

  for ( t = fieldsize; t > 0; t-- ) {
    assignbit(start, bitpos+t-1, fieldvalue);
    fieldvalue >>= 1;
  }
}


/* Fill the fax->DIS and fax->DISsize with proper values here */

void fax_setup_outgoing_DIS(void)
{
  /* The DIS is 'Digital Identification Signal' identifying the
   * capabilities of the remote fax machine (size of paper,
   * resolution, speed of paper, modulation supported etc.)
   */
  int start = 16;
  memset(fax->DIS,0,10);                           /* Default to 0's */
  assignfield(fax->DIS,1,8,FAX_CNTL_LAST_FRAME);   /* Last frame is sequence */
  assignfield(fax->DIS,9,8,FAX_FCF_DIS);           /* This is DIS */
  assignbit(fax->DIS,start+10,1);        /* We can receive faxes (no? :-) */
  assignfield(fax->DIS,start+11,4,0x08); /* Data signaling rate: V.29 */
  assignbit(fax->DIS,start+15,1);        /* We can do 200 dpi */
  assignfield(fax->DIS,start+19,2,0x01); /* Unlimited paper length */
  assignfield(fax->DIS,start+21,3,0x07); /* Very fast reception (0.0ms/line) */
  assignbit(fax->DIS,start+24,1);        /* Extend field, bits 17-24 */

  fax->DISsize = 6;
}

void fax_setup_outgoing_CSI(void)
{
  /* The CSI is 'Called Subscriber Identifier' which is shown on the
   * calling fax-machine and in its log.  It is a 20 characters string
   * containing digits and '+'.
   */
  memset(fax->CSI,' ',22);
  assignfield(fax->CSI,1,8,FAX_CNTL_NONLAST_FRAME);
  assignfield(fax->CSI,9,8,FAX_FCF_CSI);
  strncpy(((char*)fax->CSI)+2,subscriber_id,strlen(subscriber_id));
  fax->CSIsize = 22;
}

void fax_setup_outgoing_NSF(void)
{
  fax->NSFsize = 0;
}
