/* $Id$
******************************************************************************

   Fax program for ISDN.
   ITU-T Recommendation V.21 modulator.

   Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
   Copyright (C) 1998 Andreas Beck   [becka@ggi-project.org]
   Copyright (C) 1998 Oliver Eichler [Oliver.Eichler@regensburg.netsurf.de]
  
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

/* Modulate a bitstream into 300 bit/s according to ITU-T Recommendation V.21
 *
 * The output of the modulator needs to be rate-converted to
 * match the phone system sample rate.
 *
 * The module interface is:
 *
 *    Input:
 *      - Bits packed in bytes (8 bits to a byte)
 *      - length specifies number of bits to process
 *      - First bit is in LSB of first byte.
 *
 *    Output:
 *      - 16-bit signed samples
 *      - length specifies number of samples
 *
 *    Commands supported:
 *      None
 *
 *    Parameters:
 *      Channel number to modulate (1 or 2).
 */

#include <stdio.h>
#include <stdarg.h>

#include <ifax/ifax.h>
#include <ifax/modules/modulator-V21.h>

/* The following two defines are closely related, and must not be changed */
#define SAMPLERATE 7200
#define SAMPLESPERBIT 24


#define MAXBUFFER 256


typedef struct {

  ifax_sint16 buffer[MAXBUFFER];
  ifax_uint16 w;
  ifax_uint8 prevbit;
  int channel;

} modulator_V21_private;


/* The basic idea behind V.21 is to transmit a tone where the
 * frequency depends directly on the bit to be transmitted.
 * V.21 is full duplex, which means information can be
 * exchanged in both directions at the same time.  This is
 * made possible by using different frequencies for each
 * direction.  The V.21 Recommendation specifies the following:
 *
 *    Channel 1:
 *        Used to transfer information from the caller to
 *        the called party.
 *           980 Hz => '1'
 *          1180 Hz => '0'
 *
 *    Channel 2:
 *        Used to transfer information from the called party
 *        to the caller.
 *          1650 Hz => '1'
 *          1850 Hz => '0'
 *
 * When the signal has to change frequency because a different bit is
 * to be transmitted, the frequency change must be done gradualy to
 * reduce the bandwidth of the transmitted signal.  The array
 * 'phaseinc' is used to change the speed of a rotating vector.
 * One full period is 65536 which equals the sampling frequency.
 * The values 8920 and 10741 of channel 1 gives 980 and 1180 Hz.
 * The values in between makes the transition smooth.  The shape of the
 * transition is that of a cosine function.
 */

static unsigned short phaseinc[2][SAMPLESPERBIT] = {
  {
    /* Channel 1 - 980 to 1180 Hz */
    8920,8928,8954,8995,9053,9124,9209,9305,9412,9526,
    9645,9768,9893,10016,10135,10249,10356,10452,10537,
    10608,10666,10707,10733,10741
  },{
    /* Channel 2 - 1650 to 1850 Hz */
    15019,15027,15053,15094,15151,15223,15308,15404,
    15510,15624,15744,15867,15991,16114,16234,16348,
    16454,16550,16635,16707,16764,16805,16831,16839
  }
};


static int modulator_V21_handle(ifax_modp self, void *data, size_t length)
{
  modulator_V21_private *priv = self->private;
  size_t remaining = length;
  ifax_uint8 v, *src = data;
  int fillbuffer, idx, delta, n, t;
  ifax_sint32 sp;
  ifax_uint32 up;

  fillbuffer = 0;

  while ( remaining > 0 ) {
    v = *src++;
    for ( n=0; n < 8; n++ ) {

      if ( v & 1 ) {
	/* Logical '1' */
	if ( priv->prevbit & 1 ) {
	  idx = 0;
	  delta = 0;
	} else {
	  idx = SAMPLESPERBIT-1;
	  delta = -1;
	}
      } else {
	/* Logical '0' */
	if ( priv->prevbit & 1 ) {
	  idx = 0;
	  delta = 1;
	} else {
	  idx = SAMPLESPERBIT-1;
	  delta = 0;
	}
      }

      priv->prevbit = v;
      v >>= 1;

      for ( t=0; t < SAMPLESPERBIT; t++ ) {
	sp = intsin(priv->w) * 0x0A000;
	up = sp;
	up >>= 16;
	priv->buffer[fillbuffer++] = up;
	priv->w += phaseinc[priv->channel][idx];
	idx += delta;
      }

      if ( fillbuffer >= (MAXBUFFER-SAMPLESPERBIT-2) ) {
	ifax_handle_input(self->sendto,priv->buffer,fillbuffer);
	fillbuffer = 0;
      }

      remaining--;
      if ( ! remaining )
	break;
    }
  }

  if ( fillbuffer )
    ifax_handle_input(self->sendto,priv->buffer,fillbuffer);

  return length;
}

static void modulator_V21_demand(ifax_modp self, size_t demand)
{
  int needed;

  needed = ((unsigned int)(demand * (0x10000 / 24))) >> 16;

  if ( needed < 1 )
    needed = 1;

  ifax_handle_demand(self->recvfrom,needed);
}

static void modulator_V21_destroy(ifax_modp self)
{
  free(self->private);
}

static int modulator_V21_command(ifax_modp self, int cmd, va_list cmds)
{
  return 0;
}

int modulator_V21_construct(ifax_modp self,va_list args)
{
  modulator_V21_private *priv;

  if ( (priv = self->private = malloc(sizeof(modulator_V21_private))) == 0 )
    return 1;

  self->destroy = modulator_V21_destroy;
  self->handle_input = modulator_V21_handle;
  self->handle_demand = modulator_V21_demand;
  self->command = modulator_V21_command;

  priv->w = 0;
  priv->channel = (va_arg(args,int)) - 1;
  priv->prevbit = 1;

  return 0;
}
