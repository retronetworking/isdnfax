/* $Id$
******************************************************************************

   Fax program for ISDN.
   Scrambler for ITU-T Recommenedation V.17 and V.29

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

#include <stdarg.h>
#include <ifax/ifax.h>
#include <ifax/modules/scrambler.h>

#define MAXBUFFER 128

typedef struct {

  unsigned int state;
  void (*mode)(unsigned char *,unsigned char *, unsigned int *, int);
  unsigned char buffer[MAXBUFFER];

} scrambler_private;


/* Function 'scramble_V29' is used by V.29 and V.17 modulation.
 *
 * Scramble a series of bytes from source (src) to destination (dst)
 * buffer.  The state is used and updated, and buffer size is
 * in bytes.  First bit to pass through the scrambling is bit 0
 * of src[0], and the last is bit 7 of src[size-1].
 * First bit to pop out of the scrambler is bit 0 of dst[0].
 */

static void scramble_V29(unsigned char *src, unsigned char *dst,
			 unsigned int *state, int size)
{
  unsigned char next;
  unsigned int st;

  st = *state;
  while ( size-- ) {
    next = (st&0xff) ^ ((st>>5)&0xff) ^ (*src++);
    *dst++ = next;
    st = (next<<15) | (st>>8);
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

static void descramble_V29(unsigned char *src, unsigned char *dst,
			   unsigned int *state, int size)
{
  unsigned char xor, input;
  unsigned int st;

  st = *state;
  while ( size-- ) {
    xor = (st&0xff) ^ ((st>>5)&0xff);
    input = *src++;
    *dst++ = input ^ xor;
    st = (input<<15) | (st>>8);
  }
  *state = st;
}


/* The scrambler modules supports initialization and
 * selection of mode (scramble/descramble).
 * The command interface takes care of this.
 */

int scrambler_command(ifax_modp self,int cmd,va_list cmds)
{
  scrambler_private *priv = self->private;

  switch ( cmd ) {

    case CMD_SCRAMBLER_INIT:
      priv->state = 0;
      break;

    case CMD_SCRAMBLER_SCRAM_V29:
      priv->mode = scramble_V29;
      break;

    case CMD_SCRAMBLER_DESCR_V29:
      priv->mode = descramble_V29;
      break;
  }

  return 0;
}

int scrambler_handle(ifax_modp self, void *data, size_t length)
{
  scrambler_private *priv = self->private;
  int chunk, remaining = length;
  unsigned char *src = data;

  while ( remaining > 0 ) {
    chunk = remaining;
    if ( chunk > MAXBUFFER )
      chunk = MAXBUFFER;
    (*priv->mode)(src,priv->buffer,&priv->state,chunk);
    ifax_handle_input(self->sendto,priv->buffer,chunk);
    src += chunk;
    remaining -= chunk;
  }

  return length;
}


void scrambler_destroy(ifax_modp self)
{
  free(self->private);
  return;
}

int scrambler_construct(ifax_modp self,va_list args)
{
  scrambler_private *priv;

  if (NULL==(priv=self->private=malloc(sizeof(scrambler_private)))) 
    return 1;

  self->destroy		= scrambler_destroy;
  self->handle_input	= scrambler_handle;
  self->command		= scrambler_command;

  priv->state=0;
  priv->mode = scramble_V29;

  return 0;
}
