/* $Id$
******************************************************************************

   Fax program for ISDN.
   Line driver interface for ISDN/soundcard and timing/sequencing control.

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

/* This is a generator module only, it does not take any input from
 * a signal chain.  Specifying how this module is to operate is done
 * through the command interface.
 *
 * The module interface is:
 *
 *    Input:
 *       NOT SUPPORTED
 *
 *    Output:
 *       - 16-bit signed samples
 *       - length specifies number of samples
 *    or
 *       - Byte-packed bitstream
 *       - length specifies number of bits
 *
 *    Commands supported:
 *       CMD_SIGNALGEN_SINUS,rate,freq,scale
 *       CMD_SIGNALGEN_RNDBITS
 *
 *    Scale: 0x10000 is full-scale output.
 *
 *    Parameters:
 *       None
 */

#define BUFFERSIZE 128

#include <stdarg.h>
#include <stdlib.h>
#include <ifax/ifax.h>
#include <ifax/sincos.h>
#include <ifax/misc/malloc.h>
#include <ifax/modules/signalgen.h>

typedef struct {

  union {
    ifax_sint16 s16[BUFFERSIZE];
    ifax_uint8  u8[BUFFERSIZE];
  } buffer;

  int mode;

  ifax_uint16 phaseinc, w;
  ifax_sint32 scale;

} signalgen_private;


static void make_signal(ifax_modp self, size_t demand)
{
  signalgen_private *priv = self->private;
  size_t remaining = demand;
  int chunk, chunkbits, t;
  signed int sp;
  unsigned int up;

  switch ( priv->mode ) {

    case CMD_SIGNALGEN_SINUS:
      while ( remaining > 0 ) {
	chunk = remaining;
	if ( chunk > BUFFERSIZE )
	  chunk = BUFFERSIZE;
	for ( t=0; t < chunk; t++ ) {
	  sp = intsin(priv->w) * priv->scale;
	  priv->w += priv->phaseinc;
	  up = sp;
	  up >>= 16;
	  priv->buffer.s16[t] = up;
	}
	ifax_handle_input(self->sendto,priv->buffer.s16,chunk);
	remaining -= chunk;
      }
      break;

    case CMD_SIGNALGEN_RNDBITS:
      while ( remaining > 0 ) {
	  chunk = (remaining+7) >> 3;
	  chunkbits = remaining;
	  if ( chunk > BUFFERSIZE ) {
	    chunk = BUFFERSIZE;
	    chunkbits = chunk * 8;
	  }

	  for ( t=0; t < chunk; t++ ) {
	    priv->buffer.u8[t] = rand() & 0xff;
	  }

	  ifax_handle_input(self->sendto,priv->buffer.s16,chunkbits);
	  remaining -= chunkbits;
      }
      break;
  }
}
    

static void signalgen_demand(ifax_modp self, size_t demand)
{
  make_signal(self,demand);
}

static int signalgen_command(ifax_modp self, int cmd, va_list cmds)
{
  signalgen_private *priv = self->private;
  ifax_sint32 rate, freq, scle;

  switch ( cmd ) {

    case CMD_SIGNALGEN_SINUS:
      rate = va_arg(cmds,int);
      freq = va_arg(cmds,int);
      scle = va_arg(cmds,int);
      priv->mode = CMD_SIGNALGEN_SINUS;
      priv->phaseinc = ( 0x10000 * freq ) / rate;
      priv->scale = scle;
      break;

    case CMD_SIGNALGEN_RNDBITS:
      priv->mode = CMD_SIGNALGEN_RNDBITS;
      break;

    default:
      return 1;
  }

  return 0;
}

static void signalgen_destroy(ifax_modp self)
{
  free(self->private);
}

int signalgen_construct(ifax_modp self, va_list args )
{
  signalgen_private *priv;
  int mode;

  priv = ifax_malloc(sizeof(signalgen_private),"Signalgen instance");
  self->private = priv;

  self->destroy = signalgen_destroy;
  self->handle_input = 0;
  self->handle_demand = signalgen_demand;
  self->command = signalgen_command;

  priv->w = 0;

  mode=va_arg(args,int);
  printf("initial mode is %x.\n",mode);
          
  switch(mode) {
    case CMD_SIGNALGEN_SINUS:
    { int rate,freq,scle;
      rate = va_arg(args,int);
      freq = va_arg(args,int);
      scle = va_arg(args,int);
      priv->phaseinc = ( 0x10000 * freq ) / rate;
      priv->scale = scle;
      priv->mode = CMD_SIGNALGEN_SINUS;
      printf("parms are %d %d %x.\n",rate,freq,scle);
    } break;
    case CMD_SIGNALGEN_RNDBITS:
      priv->mode = CMD_SIGNALGEN_RNDBITS;
      break;
    default: /* Huh ? default to something safe. */
      ifax_command(self,CMD_SIGNALGEN_SINUS,8000,1000,0x10000);
  }

  return 0;
}
