/* $Id$
******************************************************************************

   Fax program for ISDN.
   Scrambler for ITU-T Recommenedation V.17 and V.29

   Copyright (C) 1998-1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
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

/* Input and Output:
 *      - Packed bits, 8 bits/ifax_uint8
 *      - First bit in LSB of bytes
 *      - length specifies number of bits
 *
 * Parameters are:
 *      None
 *
 * Commands supported:
 *      CMD_GENERIC_INITIALIZE
 *      CMD_GENERIC_SCRAMBLEONES
 *      CMD_GENERIC_STARTPAYLOAD
 *      CMD_SCRAMBLER_SCRAM_V29
 *      CMD_SCRAMBLER_DESCR_V29
 */


#include <stdarg.h>
#include <ifax/ifax.h>
#include <ifax/types.h>
#include <ifax/modules/generic.h>
#include <ifax/modules/scrambler.h>

#define MAXBUFFER 128

typedef struct {

  int scramble_ones;
  ifax_uint32 state;
  void (*mode)(ifax_uint8 *, ifax_uint8 *, ifax_uint32 *, size_t);
  ifax_uint8 buffer[MAXBUFFER];

} scrambler_private;


/* Function 'scramble_V29' is used by V.29 and V.17 modulation.
 *
 * Scramble a series of bits from source (src) to destination (dst)
 * buffer.  The state is used and updated, the size is specified
 * in bits.  First bit to pass through the scrambling is bit 0
 * of src[0], and the first bit to pop out of the scrambler is
 * bit 0 of dst[0].
 */

static void scramble_V29(ifax_uint8 *src, ifax_uint8 *dst,
			 ifax_uint32 *state, size_t size)
{
  ifax_uint8 next, src_data, dst_data, mask;
  ifax_uint32 st;

  st = *state;

  /* Scramble bytewise first, since it is so easy */

  while ( size >= 8 ) {
    size -= 8;
    next = (st ^ (st>>5) ^ (*src++)) & 0xff;
    *dst++ = next;
    st = (next<<15) | (st>>8);
  }

  /* Scramble remaining few bits, one bit at a time */

  if ( size ) {
    src_data = *src;
    dst_data = 0;
    mask = 1;
    while ( size-- ) {
      next = (st ^ (st>>5) ^ src_data) & 1;
      st = (next<<22) | (st>>1);
      if ( next )
	dst_data |= mask;
      mask <<= 1;
    }
    *dst = dst_data;
  }   

  *state = st;
}


/* Function 'descramble_V29' is opposite of above.
 *
 * A self-syncronizing scrambler is such that, even when the
 * scrambler has a state dependent on all previous values,
 * the descrambler is only dependent on a sliding window the
 * size of the scrambler state.  An error in transmission will
 * damage a few bits while the replicated state in the descrambler
 * is wrong.  When the channel is OK again, a correct replicated
 * state will be established and communication continues.
 */

static void descramble_V29(ifax_uint8 *src, ifax_uint8 *dst,
			   ifax_uint32 *state, size_t size)
{
  ifax_uint8 xor, input, src_data, dst_data, mask;
  ifax_uint32 st;

  st = *state;

  /* Descramble bytewise first */

  while ( size >= 8 ) {
    size -= 8;
    xor = (st ^ (st>>5)) & 0xff;
    input = *src++;
    *dst++ = input ^ xor;
    st = (input<<15) | (st>>8);
  }

  /* Descramble leftovers */

  if ( size ) {
    src_data = *src;
    dst_data = 0;
    mask = 1;
    while ( size-- ) {
      xor = (st ^ (st>>5)) & 1;
      input = src_data & 1;
      st = (input<<22) | (st>>1);
      if ( input ^ xor )
	dst_data |= mask;
      mask <<= 1;
    }
    *dst = dst_data;
  }

  *state = st;
}


/* The scrambler modules supports initialization and selection of
 * mode (scramble/descramble).  The command interface takes care of this.
 */

int scrambler_command(ifax_modp self,int cmd,va_list cmds)
{
  scrambler_private *priv = self->private;

  switch ( cmd ) {

    case CMD_GENERIC_INITIALIZE:
      priv->state = 0;
      break;

    case CMD_SCRAMBLER_SCRAM_V29:
      priv->mode = scramble_V29;
      break;

    case CMD_SCRAMBLER_DESCR_V29:
      priv->mode = descramble_V29;
      break;

    case CMD_GENERIC_SCRAMBLEONES:
      priv->scramble_ones = 1;
      break;

    case CMD_GENERIC_STARTPAYLOAD:
      priv->scramble_ones = 0;
      break;

    default:
      return 1;
  }

  return 0;
}

int scrambler_handle(ifax_modp self, void *data, size_t length)
{
  scrambler_private *priv = self->private;
  size_t chunk, remaining = length;
  ifax_uint8 *src = data;

  while ( remaining > 0 ) {
    chunk = remaining;
    if ( chunk > (MAXBUFFER*8) )
      chunk = (MAXBUFFER*8);
    (*priv->mode)(src,priv->buffer,&priv->state,chunk);
    ifax_handle_input(self->sendto,priv->buffer,chunk);
    src += MAXBUFFER;
    remaining -= chunk;
  }

  return length;
}

static void scrambler_demand(ifax_modp self, size_t demand)
{
  scrambler_private *priv = self->private;
  size_t do_bits, remaining = demand;

  static ifax_uint8 ones[] = { 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };

  if ( priv->scramble_ones ) {
    while ( remaining > 0 ) {
      do_bits = 64;
      if ( do_bits > remaining )
	do_bits = remaining;
      (*priv->mode)(ones,priv->buffer,&priv->state,do_bits);
      ifax_handle_input(self->sendto,priv->buffer,do_bits);
      remaining -= do_bits;
    }
  } else {
    ifax_handle_demand(self->recvfrom,remaining);
  }
}

void scrambler_destroy(ifax_modp self)
{
  free(self->private);
  return;
}

int scrambler_construct(ifax_modp self,va_list args)
{
  scrambler_private *priv;
  
  if ( (priv = self->private = malloc(sizeof(scrambler_private))) == 0 )
    return 1;

  self->destroy	=  scrambler_destroy;
  self->handle_input = scrambler_handle;
  self->handle_demand = scrambler_demand;
  self->command	= scrambler_command;

  priv->state=0;
  priv->scramble_ones = 0;
  priv->mode = scramble_V29;

  return 0;
}
