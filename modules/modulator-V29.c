/* $Id$
******************************************************************************

   Fax program for ISDN.
   ITU-T Recommendation V.29 modulator.

   Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
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

/* Modulate a bitstream into 9600, 7200 or 4800 bit/s according
 * to ITU-T Recommendation V.29
 *
 * The output of the modulater needs to be rate-converted to
 * match the phone system sample rate.
 */

#include <stdio.h>
#include <stdarg.h>

#include <ifax/ifax.h>
#include <ifax/modules/modulator-V29.h>

/* The following defines are closely related */
#define SAMPLERATE 7200
#define SAMPLESPERSYMBOL 3
#define PHASEINC1700HZ 15474


#define FILTSIZELP2400 5

#define MAXBUFFERING 64


/* The following datastructure defines the state and
 * working variables needed by the modulator.  It is
 * worth mentioning that the ReIm[...] array contains
 * two arrays.  The reason why they are combined into
 * the same array is to help the optimizer to use
 * offset indexing and perform well.  The two arrays
 * are offset by the PRIVREOFFSET and PRIVIMOFFSET
 * defines.
 *
 * It remains to bee seen if the compiler likes
 * this code, or if it should be changed.
 */

#define PRIVREOFFSET   0
#define PRIVIMOFFSET   (2*FILTSIZELP2400)
#define PRIVREIMOFFSET (2*FILTSIZELP2400)

typedef struct {

  signed short buffer[MAXBUFFERING+2*SAMPLESPERSYMBOL];
  signed short ReIm[FILTSIZELP2400*4];
  int ReImInsert;
  int bitstore_size;
  unsigned short bitstore;
  unsigned short w;
  int bits_per_symbol;
  unsigned int status;
  unsigned int buffer_used;
  unsigned char phase, randseq;
  signed short M_Re, M_Im, N_Re, N_Im;
  int sync_count;

} modulator_V29_private;


/* Use the 'phaseamp' array to translate an absolute
 * phase and amplitude into a corresponding set
 * of Re/Im values.  The indexing is as follows:
 *
 *     P2 P1 P0 Q0
 *
 * Where P2-P0 is the phase, and Q0 is the
 * amplitude.  Notice that this table has no
 * connection with TABLE 1/V.29.
 */

static struct { signed short Re, Im; } phaseamp[16] = 
{
  {   4634,      0 },     /* P=000 Q=0  (0)   */
  {  13902,      0 },     /* P=000 Q=1  (0)   */
  {   6951,   6951 },     /* P=001 Q=0  (45)  */
  {  11585,  11585 },     /* P=001 Q=1  (45)  */
  {      0,   4634 },     /* P=010 Q=0  (90)  */
  {      0,  13902 },     /* P=010 Q=1  (90)  */
  {  -6951,   6951 },     /* P=011 Q=0  (135) */
  { -11585,  11585 },     /* P=011 Q=1  (135) */
  {  -4634,      0 },     /* P=100 Q=0  (180) */
  { -13902,      0 },     /* P=100 Q=1  (180) */
  {  -6951,  -6951 },     /* P=101 Q=0  (225) */
  { -11585, -11585 },     /* P=101 Q=1  (225) */
  {      0,  -4634 },     /* P=110 Q=0  (270) */
  {      0, -13902 },     /* P=110 Q=1  (270) */
  {   6951,  -6951 },     /* P=111 Q=0  (315) */
  {  11585, -11585 }      /* P=111 Q=1  (315) */
};


/* The 'bits2phase' array is used to translate a given set of
 * bits, Q2, Q3 and Q4 from TABLE 1/V.29 into a corresponding relative
 * phase shift.  The indexing is as follows:
 *
 *       Q4 Q3 Q2 Q1
 *
 * The least significant bit of the index, Q1, is a don't care as it
 * modulates the amplitude.  Notice the bit-reversal of the index as
 * compared to TABLE 1/V.29 in the ITU-T Recommendation. The values
 * in the array is a relative phase shift encoded as:
 *
 *       P2 P1 P0 0
 *
 * The last significant bit is zero in order to match the 'phaseamp'
 * indexing.
 */

static unsigned char bits2phase[16] = {

               /* Q2  Q3  Q4  Phase change */
               /* ------------------------ */
  2,2,         /*  0   0   0       45      */
  12,12,       /*  1   0   0      270      */
  4,4,         /*  0   1   0       90      */
  10,10,       /*  1   1   0      225      */
  0,0,         /*  0   0   1        0      */
  14,14,       /*  1   0   1      315      */
  6,6,         /*  0   1   1      135      */
  8,8          /*  1   1   1      180      */
};


/* This short low-pass filter is used to smoth the Re and Im
 * signals before they are mixed with the carrier.
 */

static signed short lowpass2400[FILTSIZELP2400] =
    { -1130, 6774, 21845, 6774, -1130 };


/* The 'modulate_single_symbol' function uses the absolute phase
 * vector and the supplied Re/Im to output a single symbol consisting
 * of 3 samples.
 * The symbol rate is 2400 symbols/s, giving a sample rate of 7200
 * samples/s.
 *
 * The single symbol modulator state consists of a rotating
 * vector (the carrier), and a history of Re and Im values
 * for low-pass filtering before mixing and transmission.
 *
 * Output is full-scale, so the signal should be trimmed in the
 * rateconverter or elsewhere to arrive at the correct
 * output energy.
 */

static void modulate_single_symbol(modulator_V29_private *priv,
			    signed short *dst,
			    signed short Re, signed short Im)
{
  int s;
  signed short *dp, *cp, *term, tmp;
  signed int Re_sum, Im_sum, sum;

  for ( s=0; s < SAMPLESPERSYMBOL; s++ ) {

    /* Lowpass the Re/Im signals.  Do this in a tailed circular buffer */
    priv->ReImInsert--;
    priv->ReIm[priv->ReImInsert + PRIVREOFFSET] = Re;
    priv->ReIm[priv->ReImInsert + PRIVIMOFFSET] = Im;
    priv->ReIm[priv->ReImInsert + PRIVREOFFSET + FILTSIZELP2400] = Re;
    priv->ReIm[priv->ReImInsert + PRIVIMOFFSET + FILTSIZELP2400] = Im;

    Re_sum = 0;
    Im_sum = 0;
    cp = lowpass2400;
    dp = &priv->ReIm[priv->ReImInsert + PRIVREOFFSET];
    term = dp + FILTSIZELP2400;
    while ( dp < term ) {
      Re_sum += (*cp) * (*dp);
      Im_sum += (*cp) * (*(dp+PRIVREIMOFFSET));
      cp++, dp++;
    }

    if ( ! priv->ReImInsert )
        priv->ReImInsert = FILTSIZELP2400;

    /* Mix with the carrier and sum */

    tmp = Re_sum >> 15;
    sum = intcos(priv->w) * tmp;

    tmp = Im_sum >> 15;
    sum += intsin(priv->w) * tmp;

    tmp = sum >> 15;
    *dst++ = tmp;

    priv->w += PHASEINC1700HZ;
  }
}

/* Empty the output buffer by sending it on */

static void send_buffer(ifax_modp self, modulator_V29_private *priv)
{
  if ( priv->buffer_used > 0 ) {
    ifax_handle_input(self->sendto,priv->buffer,priv->buffer_used);
  }
  priv->buffer_used = 0;
}

/* The 'modulate_encoded_symbol' reads the required number of bits
 * from the bitstore, and encodes the bits according to the current
 * encoding standard (4800, 7200 or 9600 bit/s, specified by the
 * priv->bits_per_symbol variable taking on the values 2,3 or 4).
 * There must be enough bits in the bitstore when calling this
 * function.  The resulting samples are stored in array 'dst'.
 */

static void modulate_encoded_symbol(modulator_V29_private *priv,
				    signed short *dst)
{
  unsigned char bits, phase;
  signed short Re, Im;

  static unsigned char databits2quadbits4800[4] = { 8,2,4,14 };

  switch ( priv->bits_per_symbol ) {

    case 4:
      /* 9600 bit/s */
      bits = priv->bitstore & 0xf;
      priv->bitstore >>= 4;
      priv->bitstore_size -= 4;
      phase = (priv->phase + bits2phase[bits]) &0xe;
      priv->phase = phase;
      phase = phase | (bits&1);
      Re = phaseamp[phase].Re;
      Im = phaseamp[phase].Im;
      modulate_single_symbol(priv,dst,Re,Im);
      break;

    case 3:
      /* 7200 bit/s */
      bits = (priv->bitstore & 0x7) << 1;
      priv->bitstore >>= 3;
      priv->bitstore_size -= 3;
      phase = (priv->phase + bits2phase[bits]) & 0xe;
      priv->phase = phase;
      Re = phaseamp[phase].Re;
      Im = phaseamp[phase].Im;
      modulate_single_symbol(priv,dst,Re,Im);
      break;

    case 2:
      /* 4800 bit/s */
      bits = databits2quadbits4800[priv->bitstore & 0x3];
      priv->bitstore >>= 2;
      priv->bitstore_size -= 2;
      phase = (priv->phase + bits2phase[bits]) & 0xe;
      priv->phase = phase;
      Re = phaseamp[phase].Re;
      Im = phaseamp[phase].Im;
      modulate_single_symbol(priv,dst,Re,Im);
      break;
  }
}

/* Generate the first three segments of the syncronization
 * sequence.  The last segment, segment 4, relies on the
 * bigger scrambler which is a module by itself.
 * Segment 4 is implemented using the ordinary encoding
 * channel (input -> encoding -> output), controlled
 * from the outside.  Segments 1-3 are generated by calling
 * the maintenance command of this encoder.  No input is
 * used until segment 4 is encoded.
 */

static void modulate_synchronization(modulator_V29_private *priv)
{
  signed short *dst;

  while ( priv->buffer_used < MAXBUFFERING ) {

    dst = &priv->buffer[priv->buffer_used];

    if ( priv->status & MODULATORV29_STATUS_SEGMENT1 ) {

      /* Segment 1 - Silence */
      modulate_single_symbol(priv,dst,0,0);
      priv->buffer_used += SAMPLESPERSYMBOL;
      priv->sync_count++;

      /* Advance to segment 2 when ready */
      if ( priv->sync_count == 48 ) {
	priv->status = MODULATORV29_STATUS_SEGMENT2;
	priv->sync_count = 0;
	priv->M_Re = phaseamp[8].Re;
	priv->M_Im = phaseamp[8].Im;
	switch ( priv->bits_per_symbol ) {

	  case 4:
	    priv->N_Re = phaseamp[15].Re;
	    priv->N_Im = phaseamp[15].Im;
	    break;

          case 3:
	    priv->N_Re = phaseamp[14].Re;
	    priv->N_Im = phaseamp[14].Im;
	    break;

	  default:
	    priv->N_Re = phaseamp[12].Re;
	    priv->N_Im = phaseamp[12].Im;
	    break;
	}
      }
    } else if ( priv->status & MODULATORV29_STATUS_SEGMENT2 ) {

      /* Segment 2 - Alternations */
      modulate_single_symbol(priv,dst,priv->M_Re,priv->M_Im);
      dst += SAMPLESPERSYMBOL;
      modulate_single_symbol(priv,dst,priv->N_Re,priv->N_Im);
      priv->buffer_used += (2*SAMPLESPERSYMBOL);
      priv->sync_count += 2;

      /* Advance to segment 3 when ready */
      if ( priv->sync_count == 128 ) {
	priv->status = MODULATORV29_STATUS_SEGMENT3;
	priv->sync_count = 0;
	priv->randseq = 0x2A;
	priv->M_Re = phaseamp[0].Re;
	priv->M_Im = phaseamp[0].Im;

	switch ( priv->bits_per_symbol ) {

	  case 4:
	    priv->N_Re = phaseamp[7].Re;
	    priv->N_Im = phaseamp[7].Im;
	    break;

          case 3:
	    priv->N_Re = phaseamp[6].Re;
	    priv->N_Im = phaseamp[6].Im;
	    break;

	  default:
	    priv->N_Re = phaseamp[4].Re;
	    priv->N_Im = phaseamp[4].Im;
	    break;
	}
      }
    } else if ( priv->status & MODULATORV29_STATUS_SEGMENT3 ) {

      /* Segment 3 - Equalizer conditioning pattern */
      if ( priv->randseq & 1 ) {
	modulate_single_symbol(priv,dst,priv->N_Re,priv->N_Im);
      } else {
	modulate_single_symbol(priv,dst,priv->M_Re,priv->M_Im);
      }
      priv->buffer_used += SAMPLESPERSYMBOL;
      priv->sync_count++;
      priv->randseq = (((priv->randseq<<6) ^ (priv->randseq<<5)) & 0x40)
	                   | (priv->randseq>>1);

      /* Advance to segment 4 when ready */
      if ( priv->sync_count == 384 ) {
	priv->status = MODULATORV29_STATUS_SEGMENT4;
	priv->sync_count = 0;
	return;
      }
    } else if ( priv->status & MODULATORV29_STATUS_SEGMENT4 ) {

      /* Segment 4 - Scrambled all binary ones */
      return;
    }
  }
}


int modulator_V29_handle(ifax_modp self, void *data, size_t length)
{
  modulator_V29_private *priv=(modulator_V29_private *)self->private;
  int remaining=length;
  unsigned char *dp = data;
  int symbols;

  symbols = 0;

  while ( remaining > 0 || priv->bitstore_size >= priv->bits_per_symbol ) {

    if ( priv->bitstore_size < priv->bits_per_symbol ) {
      if ( priv->bitstore_size == 0 ) {
	priv->bitstore = *dp++;
      } else {
	priv->bitstore |= (*dp++) << priv->bitstore_size;
      }
      priv->bitstore_size += 8;
      remaining--;
    }

    modulate_encoded_symbol(priv,&priv->buffer[priv->buffer_used]);
    priv->buffer_used += SAMPLESPERSYMBOL;
    symbols++;

    if ( priv->buffer_used >= MAXBUFFERING )
      send_buffer(self,priv);
  }

  send_buffer(self,priv);

  if ( priv->status & MODULATORV29_STATUS_SEGMENT4 ) {
    priv->sync_count += symbols;
    if ( priv->sync_count >= 48 ) {
      priv->status = MODULATORV29_STATUS_DATA;
    }
  }

  return length;
}

void modulator_V29_destroy(ifax_modp self)
{
  free(self->private);
  return;
}

int modulator_V29_command(ifax_modp self, int cmd, va_list cmds)
{
  modulator_V29_private *priv = self->private;
  unsigned int *statusp;

  switch ( cmd ) {

    case CMD_MODULATORV29_STARTSYNC:
      priv->status = MODULATORV29_STATUS_SEGMENT1;
      priv->sync_count = 0;
      break;

    case CMD_MODULATORV29_MAINTAIN:
      if ( priv->status & MODULATORV29_STATUS_SEGMENT1234 ) {
	modulate_synchronization(priv);
	send_buffer(self,priv);
      }
      statusp = va_arg(cmds,unsigned int *);
      *statusp = priv->status;
      break;
  }

  return 0;
}

int modulator_V29_construct(ifax_modp self,va_list args)
{
  modulator_V29_private *priv;
  int t;

  if (NULL==(priv=self->private=malloc(sizeof(modulator_V29_private)))) 
    return 1;

  self->destroy         = modulator_V29_destroy;
  self->handle_input    = modulator_V29_handle;
  self->command         = modulator_V29_command;

  priv->w = 0;
  priv->bitstore_size = 0;
  priv->bitstore = 0;
  priv->bits_per_symbol = 4;    /* Default 9600, should be option */
  priv->phase = 0;
  priv->randseq = 0;
  priv->buffer_used = 0;

  priv->ReImInsert = FILTSIZELP2400;
  for ( t=0; t < (4*FILTSIZELP2400); t++ )
    priv->ReIm[t] = 0;

  return 0;
}
