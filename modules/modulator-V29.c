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
#include <ifax/types.h>
#include <ifax/misc/malloc.h>
#include <ifax/modules/generic.h>
#include <ifax/modules/modulator-V29.h>

/* The following defines are closely related and must be modified with
 * great care.  The sample-rate must be matched to the rate-convert filter
 * and its initialization.  With SAMPLERATE=7200, an interpolation of
 * 10 and a decimation of 9 will give a rate of 8000.  The relationship
 * between SAMPLERATE and SAMPLESPERSYMBOL is:
 *           SAMPLERATE = 2400 * SAMPLESPERSYMBOL
 * where 2400 is the number of symbols per second.  The PHASEINC1700HZ
 * is used to generate a 1700Hz carrier (0x10000 / 1700).  The only
 * relevant alternative setup is SAMPLERATE=9600 and SAMPLESPERSYMBOL=4
 * but this should not be needed with V.29
 */
#define SAMPLERATE 7200
#define SAMPLESPERSYMBOL 3
#define PHASEINC1700HZ 15474

/* The Re/Im signals are low-pass filtered before they are mixed with
 * the carrier.  The LP-filter has the following size:
 */
#define FILTSIZELP2400 5

/* Buffering capacity in samples */
#define BUFFERSIZE 128


/* Synchronizing symbol start positions */

#define SYNCHRONIZE_START_SEG1  0
#define SYNCHRONIZE_START_SEG2  48
#define SYNCHRONIZE_START_SEG3  176
#define SYNCHRONIZE_START_SEG4  560
#define SYNCHRONIZE_START_DATA  608


/* The following datastructure defines the state and
 * working variables needed by the modulator.  It is
 * worth mentioning that the ReIm[...] array contains
 * two arrays.  The reason why they are combined into
 * the same array is to help the optimizer to use
 * offset indexing and perform well.  The two arrays
 * are offset by the PRIVREOFFSET and PRIVIMOFFSET
 * defines.
 *
 * It remains to be seen if the compiler likes
 * this code, or if it should be changed.
 */

#define PRIVREOFFSET   0
#define PRIVIMOFFSET   (2*FILTSIZELP2400)
#define PRIVREIMOFFSET (2*FILTSIZELP2400)

   typedef struct {
   
      unsigned int ReImInsert;
      unsigned int  bitstore_size;
      ifax_uint16 bitstore;
      ifax_uint16 w;
      unsigned int bits_per_symbol;
      unsigned int buffer_size;
      ifax_uint8 phase, randseq;
      int syncseq;
      ifax_sint16 ReIm[FILTSIZELP2400*4];
      ifax_sint16 buffer[BUFFERSIZE+2*SAMPLESPERSYMBOL];
   
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

   static struct { ifax_sint16 Re, Im; } phaseamp[16] = 
   {
   {  0x4CCC, 0x0000 },     /* P=000 Q=0  (0)   */
   {  0x7FFF, 0x0000 },     /* P=000 Q=1  (0)   */
   {  0x1999, 0x1999 },     /* P=001 Q=0  (45)  */
   {  0x4CCC, 0x4CCC },     /* P=001 Q=1  (45)  */
   {  0x0000, 0x4CCC },     /* P=010 Q=0  (90)  */
   {  0x0000, 0x7FFF },     /* P=010 Q=1  (90)  */
   {  0xE666, 0x1999 },     /* P=011 Q=0  (135) */
   {  0xB333, 0x4CCC },     /* P=011 Q=1  (135) */
   {  0xB333, 0x0000 },     /* P=100 Q=0  (180) */
   {  0x8000, 0x0000 },     /* P=100 Q=1  (180) */
   {  0xE666, 0xE666 },     /* P=101 Q=0  (225) */
   {  0xB333, 0xB333 },     /* P=101 Q=1  (225) */
   {  0x0000, 0xB333 },     /* P=110 Q=0  (270) */
   {  0x0000, 0x8000 },     /* P=110 Q=1  (270) */
   {  0x1999, 0xE666 },     /* P=111 Q=0  (315) */
   {  0x4CCC, 0xB333 },     /* P=111 Q=1  (315) */
   };


/* 'signalpoint_*' holds the signal/space points used to generate
 * synchronizing segment 2 and 3.
 */

   static struct { ifax_sint16 Re, Im; } signalpoint_A = { 0xB333, 0 };

   static struct { ifax_sint16 Re, Im; } signalpoint_C = { 0x4CCC, 0 };

   static struct { ifax_sint16 Re, Im; } signalpoint_B[5] =
   {
   { 0x0000, 0x0000 },
   { 0x0000, 0x0000 },
   { 0x0000, 0xB333 },    /* B(4800) */
   { 0x1999, 0xE666 },    /* B(7200) */
   { 0x4CCC, 0xB333 }     /* B(9600) */
   };

   static struct { ifax_sint16 Re, Im; } signalpoint_D[5] =
   {
   { 0x0000, 0x0000 },
   { 0x0000, 0x0000 },
   { 0x0000, 0x4CCC },    /* D(4800) */
   { 0xE666, 0x1999 },    /* D(7200) */
   { 0xB333, 0x4CCC }     /* D(9600) */
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
 * The least significant bit is zero in order to match the 'phaseamp'
 * indexing.
 */

   static ifax_uint8 bits2phase[16] = {
   
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


/* This short low-pass filter is used to smooth the Re and Im
 * signals before they are mixed with the carrier.
 */

   ifax_sint16 lowpass2400[FILTSIZELP2400] =
   {0xFB96, 0x01A76, 0x05555, 0x01A76, 0xFB96};

/* Empty the output buffer by sending it on */

   static void send_buffer(ifax_modp self, modulator_V29_private *priv)
   {
      if ( priv->buffer_size > 0 ) {
         ifax_handle_input(self->sendto,priv->buffer,priv->buffer_size);
      }
      priv->buffer_size = 0;
   }


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

   static void modulate_single_symbol(ifax_modp self, modulator_V29_private *priv,
   ifax_sint16 Re, ifax_sint16 Im)
   {
      ifax_sint16 *dp, *cp, *term, *dst, tmp;
      ifax_sint32 Re_sum, Im_sum, sum;
      int s;
   
      static int cnt = 0;
   
      dst = &priv->buffer[priv->buffer_size];
   
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
         /* switched lpf off, olli*/
         sum = intcos(priv->w) * Re;
      
         tmp = Im_sum >> 15;
      	/* switched lpf off, olli*/
         sum += intsin(priv->w) * Im;
      
         tmp = sum >> 15;
         *dst++ = tmp;
      
         priv->w += PHASEINC1700HZ;
      }
   	
      priv->buffer_size += SAMPLESPERSYMBOL;
      if ( priv->buffer_size >= BUFFERSIZE )
         send_buffer(self,priv);
   }

/* The 'modulate_encoded_symbol' reads the required number of bits
 * from the bitstore, and encodes the bits according to the current
 * encoding standard (4800, 7200 or 9600 bit/s, specified by the
 * priv->bits_per_symbol variable taking on the values 2,3 or 4).
 * There must be enough bits in the bitstore when calling this
 * function.  The resulting samples are stored in array 'dst'.
 */

   static void modulate_encoded_symbol(ifax_modp self,modulator_V29_private *priv)
   {
      ifax_uint8 bits, phase;
      ifax_sint16 Re, Im;
   
      static ifax_uint8 databits2quadbits4800[4] = { 8,2,4,14 };
   
      bits = priv->bitstore;
      priv->bitstore >>= priv->bits_per_symbol;
      priv->bitstore_size -= priv->bits_per_symbol;
   
      switch ( priv->bits_per_symbol ) {
      
         case 4:
         /* 9600 bit/s */
            phase = (priv->phase + bits2phase[bits&0xF]) &0xE;
            priv->phase = phase;
            phase = phase | (bits&1);
            Re = phaseamp[phase].Re;
            Im = phaseamp[phase].Im;
            modulate_single_symbol(self,priv,Re,Im);
            break;
      
         case 3:
         /* 7200 bit/s */
            phase = (priv->phase + bits2phase[(bits&0x7)<<1]) & 0xE;
            priv->phase = phase;
            Re = phaseamp[phase].Re;
            Im = phaseamp[phase].Im;
            modulate_single_symbol(self,priv,Re,Im);
            break;
      
         case 2:
         /* 4800 bit/s */
            bits = databits2quadbits4800[bits&0x3];
            phase = (priv->phase + bits2phase[bits]) & 0xE;
            priv->phase = phase;
            Re = phaseamp[phase].Re;
            Im = phaseamp[phase].Im;
            modulate_single_symbol(self,priv,Re,Im);
            break;
      }
   }


   int modulator_V29_handle(ifax_modp self, void *data, size_t length)
   {
      modulator_V29_private *priv = self->private;
      size_t remaining = length;
      ifax_uint8 *dp = data;
      ifax_uint16 new_bits;
      int new_bits_size;
   
      while ( remaining > 0 || priv->bitstore_size >= priv->bits_per_symbol ) {
      
      /* Refill the bitstore if needed */
      
         if ( priv->bitstore_size < priv->bits_per_symbol ) {
         
            if ( remaining >= 8 ) {
            /* Can get a full byte */
               new_bits = (*dp++) & 0x00ff;
               new_bits_size = 8;
               remaining -= 8;
            } 
            else {
            /* Only partially full byte left */
               new_bits = (*dp++) & ((1<<remaining)-1);
               new_bits_size = remaining;
               remaining = 0;
            }
         
            if ( priv->bitstore_size == 0 ) {
               priv->bitstore = new_bits;
               priv->bitstore_size = new_bits_size;
            } 
            else {
               priv->bitstore |= new_bits << priv->bitstore_size;
               priv->bitstore_size += new_bits_size;
            }
         }
      
      /* Modulate a symbol if we have enough bits */
      
         if ( priv->bitstore_size >= priv->bits_per_symbol )
            modulate_encoded_symbol(self,priv);
      }
   
      send_buffer(self,priv);
   
      return length;
   }

   static void modulator_V29_demand(ifax_modp self, size_t demand)
   {
      modulator_V29_private *priv = self->private;
      int symbols_needed, do_symbols, bits_needed;
      ifax_sint16 Re, Im;
   
      symbols_needed = (((0x10000/SAMPLESPERSYMBOL) * demand) >> 16) + 1;
   
      if ( priv->syncseq < SYNCHRONIZE_START_SEG4 ) {
      
      /* We are synchronizing; segments 1-3 generates data directly, segment 4
      * depends on the output of the previous scrambler module.
      */
      
         while ( symbols_needed > 0 ) {
         
            if ( priv->syncseq < SYNCHRONIZE_START_SEG2 ) {
            
            /* Segment 1 - Silence */
            
               modulate_single_symbol(self,priv,0,0);
               priv->syncseq++;
               symbols_needed--;
            
            } 
            else if ( priv->syncseq < SYNCHRONIZE_START_SEG3 ) {
            
            /* Segment 2 - Alternations */
            
               if ( priv->syncseq & 1 ) {
                  Re = signalpoint_B[priv->bits_per_symbol].Re;
                  Im = signalpoint_B[priv->bits_per_symbol].Im;	
               } 
               else {
                  Re = signalpoint_A.Re;
                  Im = signalpoint_A.Im;
               }
            
               modulate_single_symbol(self,priv,Re,Im);
               priv->syncseq++;
/*
               if(priv->syncseq == SYNCHRONIZE_START_SEG2 + 2)
                  priv->syncseq = SYNCHRONIZE_START_SEG2;
*/
               symbols_needed--;
            
            } 
            else if ( priv->syncseq < SYNCHRONIZE_START_SEG4 ) {
            
            /* Segment 3 - Equalizer conditioning pattern */
            
               if ( priv->randseq & 1 ) {
                  Re = signalpoint_D[priv->bits_per_symbol].Re;
                  Im = signalpoint_D[priv->bits_per_symbol].Im;
               } 
               else {
                  Re = signalpoint_C.Re;
                  Im = signalpoint_C.Im;
               }
            
               modulate_single_symbol(self,priv,Re,Im);
               priv->syncseq++;
               symbols_needed--;
            
               priv->randseq = (((priv->randseq<<6) ^ (priv->randseq<<5)) & 0x40)
                  | (priv->randseq>>1);
            
            } 
            else if ( priv->syncseq == SYNCHRONIZE_START_SEG4 ) {
            
            /* Segment 4 is about to start, initialize the scrambler that
            * is (hopefully!) the previous module.
            */
            
               ifax_command(self->recvfrom,CMD_GENERIC_INITIALIZE);
               ifax_command(self->recvfrom,CMD_GENERIC_SCRAMBLEONES);
               priv->bitstore_size = 0;
               priv->bitstore = 0;
               break;
            }
         }
      }
   
      if ( symbols_needed && priv->syncseq < SYNCHRONIZE_START_DATA ) {
      
      /* Segment 4 is on - demand data (ones) from the scrambler one step
      * up the signal chain.
      */
      
         do_symbols = SYNCHRONIZE_START_DATA - priv->syncseq;
         if ( do_symbols > symbols_needed )
            do_symbols = symbols_needed;
      
         bits_needed = do_symbols * priv->bits_per_symbol - priv->bitstore_size;
      
         ifax_handle_demand(self->recvfrom,bits_needed);
         symbols_needed -= do_symbols;
         priv->syncseq += do_symbols;
         if ( priv->syncseq >= SYNCHRONIZE_START_DATA )
            ifax_command(self->recvfrom,CMD_GENERIC_STARTPAYLOAD);
      }
   
      if ( symbols_needed && priv->syncseq >= SYNCHRONIZE_START_DATA ) {
         bits_needed = symbols_needed * priv->bits_per_symbol - priv->bitstore_size;
         ifax_handle_demand(self->recvfrom,bits_needed);
      }
   
      send_buffer(self,priv);
   }



   static void modulator_V29_destroy(ifax_modp self)
   {
      free(self->private);
   }

   static int modulator_V29_command(ifax_modp self, int cmd, va_list cmds)
   {
      modulator_V29_private *priv = self->private;
   
      switch ( cmd ) {
      
         case CMD_GENERIC_INITIALIZE:
            priv->syncseq = SYNCHRONIZE_START_SEG1;
            priv->randseq = 0x2A;
            break;
      
         default:
            return 1;
      }
   
      return 0;
   }

   int modulator_V29_construct(ifax_modp self,va_list args)
   {
      modulator_V29_private *priv;
      int t;
   
      priv = ifax_malloc(sizeof(modulator_V29_private),
			 "V.29 modulator instance");
      self->private = priv;
   
      self->destroy = modulator_V29_destroy;
      self->handle_input = modulator_V29_handle;
      self->handle_demand = modulator_V29_demand;
      self->command = modulator_V29_command;
   
      priv->w = 0;
      priv->bitstore_size = 0;
      priv->bitstore = 0;
      priv->bits_per_symbol = 4;
      priv->phase = 0;
      priv->randseq = 0;
      priv->buffer_size = 0;
   
      priv->ReImInsert = FILTSIZELP2400;
      for ( t=0; t < (4*FILTSIZELP2400); t++ )
         priv->ReIm[t] = 0;
   
      return 0;
   }
