/* $Id$
******************************************************************************

   Fax program for ISDN.
   V.29 modulator.

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
/* modulator-V29.c
 *
 * Modulate a bitstream into 9600 bit/s according to V.29
 *
 * (C) 1998 Morten Rolland
 */

/* Bugs: Can only do 9600 */

#include <stdio.h>
#include <stdarg.h>
#include <ifax/ifax.h>

/* Samplerate and samples/symbol is closely related here */
#define SAMPLERATE 9600
#define SAMPLESPERSYMBOL 4

/* Carrier frequency */
#define CARRIERFREQ 1700

/* This is the delta-phase for the carrier */
#define SAMPLEPHASEINC ((0x10000*CARRIERFREQ)/SAMPLERATE)

#define MAXBUFFERING 128

typedef struct {

  unsigned int prev_phase;
  unsigned short w;
  signed short buffer[MAXBUFFERING+2*SAMPLESPERSYMBOL];
  /*  unsigned char watingbits;    (For use when symbol != 4 bits
      int waitingbits_count; */

} modulator_V29_private;

unsigned char bits2phase[] = { 1,0,2,3,6,7,5,4 };

/* Free the private data
 */
void modulator_V29_destroy(ifax_modp self)
{
  free(self->private);
  return;
}

int modulator_V29_command(ifax_modp self,int cmd,va_list cmds)
{
  return 0;
}

void modulate_symbol(unsigned char bits, signed short *dst,
		     unsigned int *prev_phase, unsigned short *w)
{
  unsigned int p;
  signed short wave, amp;
  signed int sample;
  int t;

  p = (*prev_phase + bits2phase[bits>>1]) & 0x7;
  *prev_phase = p;

  fprintf(stderr,"%d\n",p*45);

  if ( p & 1 ) {
    /* 45 + 90*n */
    if ( bits & 1 ) {
      amp = 27798;
    } else {
      amp = 9266;
    }
  } else {
    /* 0 + 90*n */
    if ( bits & 1 ) {
      amp = 32760;
    } else {
      amp = 19656;
    }
  }

  p *= 0x2000;

  for ( t=0; t < SAMPLESPERSYMBOL; t++ ) {
    wave = intsin(*w + p);
    sample = wave * amp;
    sample /= 0x10000;
    printf("%d\n",sample);
    *dst++ = sample;
    *w += SAMPLEPHASEINC;
  }
}

int modulator_V29_handle(ifax_modp self, void *data, size_t length)
{
  modulator_V29_private *priv=(modulator_V29_private *)self->private;
  size_t chunk, remaining=length;
  signed short *bp = priv->buffer;
  unsigned char *dp = data;
  int t;

  while ( remaining > 0 ) {

    chunk = remaining;
    if ( chunk > MAXBUFFERING )
      chunk = MAXBUFFERING;
    for ( t=0; t < chunk; t++ ) {
      modulate_symbol(dp[t]&0xf,bp,&priv->prev_phase,&priv->w);
      bp += SAMPLESPERSYMBOL;
      modulate_symbol(dp[t]>>4,bp,&priv->prev_phase,&priv->w);
      bp += SAMPLESPERSYMBOL;
    }
    /* ifax_handle_input(self->sendto,priv->buffer,chunk); */
    remaining -= chunk;    
  }

  return length;
}

int modulator_V29_construct(ifax_modp self,va_list args)
{
  modulator_V29_private *priv;
  if (NULL==(priv=self->private=malloc(sizeof(modulator_V29_private)))) 
    return 1;
  self->destroy         = modulator_V29_destroy;
  self->handle_input    = modulator_V29_handle;
  self->command         = modulator_V29_command;

  priv->w = 0;
  priv->prev_phase = 0;

  return 0;
}
