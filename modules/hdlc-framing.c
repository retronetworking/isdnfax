/* $Id$
******************************************************************************

   Fax program for ISDN.
   Encoder and decoder for HDLC-framing.

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

/* Translate a block of binary data into a HDLC frame and output it
 * as a stream of bits, usable for feeding the scrambler or V.21
 * modulator.
 *
 * The block to be transmitted is presented to this module through the
 * command interface.  With no data to transmit, an idle-pattern is
 * transmitted.
 */

#include <stdio.h>
#include <stdarg.h>

#include <ifax/ifax.h>
#include <ifax/types.h>
#include <ifax/misc/malloc.h>
#include <ifax/modules/hdlc-framing.h>

#define MAXBUFFER 512
#define QUEUESIZE 128

#define FLAG_SEQUENCE 0x7e

typedef struct {

  ifax_uint32 bitslide;
  int bitslide_size;

  enum { ADDRESS, PAYLOAD, FCS, IDLE } phase;
  ifax_uint8 *src;
  size_t remaining_bytes;
  ifax_uint16 fcs;
  ifax_uint8 fcs_tx[2];

  int new_frame;
  int current_frame;

  struct {
    size_t size;
    ifax_uint8 *start;
    ifax_uint8 address;
  } framequeue[QUEUESIZE];

  ifax_uint8 buffer[MAXBUFFER];

} encoder_hdlc_private;


/* The 'bitstufftbl' array is used to help determine when and where
 * bit-stuffing should be used.  The HDLC protocol specifies that
 * 6 bits in a row with value '1' is reserved for the FLAG sequence.
 * A flag is used to start and stop a frame.
 * When there are 6 bits in a row with value '1' in the body of
 * a frame, they have to be bit-stuffed, so that there is a '0'
 * inserted after the first 5 bits.  The bit-stream is expanded as
 * a result of this, but the advantage is that frame synchronisation
 * is rapidly established.
 *
 * The 'bitstufftbl' is used to answer the following three questions:
 *
 *   1) How many consecutive '1' is there from LSB and up
 *   2) How many consecutive '1' is there from MSB and down
 *   3) Should a '0' be inserted in bit-positions 5, 6 or 7 (or none)
 *
 * The index to the table is the 8 bit unsigned intger in question:
 *
 *       x7 x6 x5 x4 x3 x2 x1 x0
 *      MSB                   LSB
 *
 * The lookup-value is a packed character, whith the following fields:
 *
 *       v7 v6         |  v5 v4 v3     | v2 v1 v0
 *       --------------+---------------+---------
 *       Position      |  Number of    | Number of
 *       of needed     |  '1' from     | '1' from
 *       stuffing      |  MSB and      | LSB and up
 *       0 => none     |  down, max    |
 *       1-3 => x5-x7  |  value of 5   |
 */

static ifax_uint8 bitstufftbl[256] = {
  0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x03,0x00,0x01,0x00,0x02,
  0x00,0x01,0x00,0x04,0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x03,
  0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x05,0x00,0x01,0x00,0x02,
  0x00,0x01,0x00,0x03,0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x04,
  0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x03,0x00,0x01,0x00,0x02,
  0x00,0x01,0x00,0x46,0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x03,
  0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x04,0x00,0x01,0x00,0x02,
  0x00,0x01,0x00,0x03,0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x05,
  0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x03,0x00,0x01,0x00,0x02,
  0x00,0x01,0x00,0x04,0x00,0x01,0x00,0x02,0x00,0x01,0x00,0x03,
  0x00,0x01,0x00,0x02,0x00,0x01,0x80,0x47,0x08,0x09,0x08,0x0a,
  0x08,0x09,0x08,0x0b,0x08,0x09,0x08,0x0a,0x08,0x09,0x08,0x0c,
  0x08,0x09,0x08,0x0a,0x08,0x09,0x08,0x0b,0x08,0x09,0x08,0x0a,
  0x08,0x09,0x08,0x0d,0x08,0x09,0x08,0x0a,0x08,0x09,0x08,0x0b,
  0x08,0x09,0x08,0x0a,0x08,0x09,0x08,0x0c,0x08,0x09,0x08,0x0a,
  0x08,0x09,0x08,0x0b,0x08,0x09,0x08,0x0a,0x08,0x09,0x08,0x4e,
  0x10,0x11,0x10,0x12,0x10,0x11,0x10,0x13,0x10,0x11,0x10,0x12,
  0x10,0x11,0x10,0x14,0x10,0x11,0x10,0x12,0x10,0x11,0x10,0x13,
  0x10,0x11,0x10,0x12,0x10,0x11,0x10,0x15,0x18,0x19,0x18,0x1a,
  0x18,0x19,0x18,0x1b,0x18,0x19,0x18,0x1a,0x18,0x19,0x18,0x1c,
  0x20,0x21,0x20,0x22,0x20,0x21,0x20,0x23,0x28,0x29,0x28,0x2a,
  0xe8,0xe9,0xa8,0x6f
};

/* An important part of HDLC is frame cheacksum computation and checking.
 * The checksum is computed using a shift register with XOR'ed feedback
 * of (data xor bit 0) into bits 15, 10 and 3.  The following small perl
 * script illustrates how it works, and how the lookuptable below is
 * generated.
 * 
 *   for ( $t=0; $t < 256; $t++ ) {
 *     $x = $t;
 *     for ( $b=0; $b < 8; $b++ ) {
 *       if ( $x & 1 ) {
 *         $x = ($x>>1) ^ 0x8408;
 *       } else {
 *         $x >>= 1;
 *       }
 *     }
 *     print sprintf("0x%04x,",$x);
 *   }
 */

static ifax_uint16 hdlc_fcs_lookup[256] = {
  0x0000,0x1189,0x2312,0x329b,0x4624,0x57ad,0x6536,0x74bf,0x8c48,0x9dc1,
  0xaf5a,0xbed3,0xca6c,0xdbe5,0xe97e,0xf8f7,0x1081,0x0108,0x3393,0x221a,
  0x56a5,0x472c,0x75b7,0x643e,0x9cc9,0x8d40,0xbfdb,0xae52,0xdaed,0xcb64,
  0xf9ff,0xe876,0x2102,0x308b,0x0210,0x1399,0x6726,0x76af,0x4434,0x55bd,
  0xad4a,0xbcc3,0x8e58,0x9fd1,0xeb6e,0xfae7,0xc87c,0xd9f5,0x3183,0x200a,
  0x1291,0x0318,0x77a7,0x662e,0x54b5,0x453c,0xbdcb,0xac42,0x9ed9,0x8f50,
  0xfbef,0xea66,0xd8fd,0xc974,0x4204,0x538d,0x6116,0x709f,0x0420,0x15a9,
  0x2732,0x36bb,0xce4c,0xdfc5,0xed5e,0xfcd7,0x8868,0x99e1,0xab7a,0xbaf3,
  0x5285,0x430c,0x7197,0x601e,0x14a1,0x0528,0x37b3,0x263a,0xdecd,0xcf44,
  0xfddf,0xec56,0x98e9,0x8960,0xbbfb,0xaa72,0x6306,0x728f,0x4014,0x519d,
  0x2522,0x34ab,0x0630,0x17b9,0xef4e,0xfec7,0xcc5c,0xddd5,0xa96a,0xb8e3,
  0x8a78,0x9bf1,0x7387,0x620e,0x5095,0x411c,0x35a3,0x242a,0x16b1,0x0738,
  0xffcf,0xee46,0xdcdd,0xcd54,0xb9eb,0xa862,0x9af9,0x8b70,0x8408,0x9581,
  0xa71a,0xb693,0xc22c,0xd3a5,0xe13e,0xf0b7,0x0840,0x19c9,0x2b52,0x3adb,
  0x4e64,0x5fed,0x6d76,0x7cff,0x9489,0x8500,0xb79b,0xa612,0xd2ad,0xc324,
  0xf1bf,0xe036,0x18c1,0x0948,0x3bd3,0x2a5a,0x5ee5,0x4f6c,0x7df7,0x6c7e,
  0xa50a,0xb483,0x8618,0x9791,0xe32e,0xf2a7,0xc03c,0xd1b5,0x2942,0x38cb,
  0x0a50,0x1bd9,0x6f66,0x7eef,0x4c74,0x5dfd,0xb58b,0xa402,0x9699,0x8710,
  0xf3af,0xe226,0xd0bd,0xc134,0x39c3,0x284a,0x1ad1,0x0b58,0x7fe7,0x6e6e,
  0x5cf5,0x4d7c,0xc60c,0xd785,0xe51e,0xf497,0x8028,0x91a1,0xa33a,0xb2b3,
  0x4a44,0x5bcd,0x6956,0x78df,0x0c60,0x1de9,0x2f72,0x3efb,0xd68d,0xc704,
  0xf59f,0xe416,0x90a9,0x8120,0xb3bb,0xa232,0x5ac5,0x4b4c,0x79d7,0x685e,
  0x1ce1,0x0d68,0x3ff3,0x2e7a,0xe70e,0xf687,0xc41c,0xd595,0xa12a,0xb0a3,
  0x8238,0x93b1,0x6b46,0x7acf,0x4854,0x59dd,0x2d62,0x3ceb,0x0e70,0x1ff9,
  0xf78f,0xe606,0xd49d,0xc514,0xb1ab,0xa022,0x92b9,0x8330,0x7bc7,0x6a4e,
  0x58d5,0x495c,0x3de3,0x2c6a,0x1ef1,0x0f78
};


/* This function fills up the 'bitslide' variable in the encoder.
 * Based on the internal state of the encoder, and the phase it is
 * in, a proper bit-sequence is appended to the bitslide variable.
 * This function is called whenever the bitslide is low on bits.
 * FLAGs are generated if no frame is available for transmission.
 * This function is rather large, but many of its large blocks
 * only gets invoked once in a while (bitstuffing once in 32 calls
 * for random binary data).
 */

void produce_bits(encoder_hdlc_private *priv)
{
  ifax_uint16 next, LSB_mask, LSB_bits;
  ifax_uint8 n, p;
  int next_size, stuffpos;

  if ( priv->phase == IDLE ) {
    /* Send idle-FLAGS, and possibly initiate a new transfer */
    if ( priv->current_frame != priv->new_frame ) {
      /* There is a frame queued up, prepare for its transmission */
      priv->phase = ADDRESS;
      priv->fcs = 0xffff;
      priv->src = &priv->framequeue[priv->current_frame].address;
      priv->remaining_bytes = 1;
    }
    /* Transmit a FLAG-sequence, no bit-stuffing applied.  This flag is
     * either an idle-channel pattern, or the initial flag of a frame.
     */
    priv->bitslide |= (FLAG_SEQUENCE<<priv->bitslide_size);
    priv->bitslide_size += 8;
    return;
  }

  /* Fetch the next byte to be transmitted, and compute FCS */
  next = (*priv->src++ && 0x00ff);
  next_size = 8;
  priv->remaining_bytes--;
  priv->fcs = (priv->fcs>>8) ^ hdlc_fcs_lookup[(priv->fcs&0xff)^next];

  /* Advance to next phase if current phase exhausted */
  if ( priv->remaining_bytes == 0 ) {
    switch ( priv->phase ) {
      case ADDRESS:
	priv->phase = PAYLOAD;
	priv->src = priv->framequeue[priv->current_frame].start;
	priv->remaining_bytes = priv->framequeue[priv->current_frame].size;
	break;
      case PAYLOAD:
	priv->phase = FCS;
	priv->src = priv->fcs_tx;
	priv->remaining_bytes = 2;
	priv->fcs_tx[0] = ~(priv->fcs & 0xff);
	priv->fcs_tx[1] = ~((priv->fcs>>8) & 0xff);
	break;
      case FCS:
	priv->phase = IDLE;
	priv->current_frame++;
	if ( priv->current_frame >= QUEUESIZE )
	  priv->current_frame = 0;
	break;
      case IDLE:
	/* Should never get here, but these lines removes a compile warning */
	break;
    }
  }

  /* insert 'next' into bitstream with bitstuffing active.  First
   * take a look at the previous completed byte, and see how many '1'
   * there is in MSB (variable p, previous).  Then look up the
   * number of '1' from LSB of the byte to be inserted into the output-
   * stream (and store in variable n, next).
   * If p+n is 6 or larger, we have a string of 1s of length 6 or
   * larger possibly spanning the previous byte and the one we prepare
   * to output.  If so, do bit stuffing.
   */

  p = (bitstufftbl[priv->bitslide>>(priv->bitslide_size-8)]>>3) & 7;
  n = bitstufftbl[next] & 7;

  if ( p+n > 5 ) {
    /* Need bit-stuffing in new-bit 0,1,..,5 */
    stuffpos = 5 - p;
    if ( stuffpos == 0 ) {
      /* Easy stuffing, just shift left... Another reason for doing this
       * test is that I don't trust the expression (1<<x) when x = 0 on
       * all architectures.  But I'm probably just paranoid.
       */
      next <<= 1;
      next_size++;
    } else {
      /* Not so easy stuffing... keep 'stuffpos' LSBs */
      LSB_mask = (1<<stuffpos) - 1;
      LSB_bits = next & LSB_mask;
      next &= ~LSB_mask;
      next <<= 1;
      next_size++;
      next |= LSB_bits;
      if ( stuffpos > 2 )
	/* Double bit-stuffing not needed, so skip the test */
	goto no_more_stuffing;
    }
  }

  /* The table-lookup will tell us if we have a string of '1' completely
   * inside the 'next' word (which is 8 or 9 bits large at this point)
   * which needs bit-stuffing.  Since any string of 1s occupying
   * bit-position 0 is taken care of in the previous test and stuffing,
   * we can now consentrate on bits 1-8 (we assume bit 8 is used - it
   * will be 0 and not cause bit-stuffing if it is not).
   */

  stuffpos = bitstufftbl[next>>1]>>6;

  if ( stuffpos ) {
    stuffpos += 6;

    LSB_mask = (1<<stuffpos) - 1;
    LSB_bits = next & LSB_mask;
    next &= ~LSB_mask;
    next <<= 1;
    next_size++;
    next |= LSB_bits;
  }

 no_more_stuffing:

  /* Finaly insert the (bit-stuffed) string of bits (8-10 bits) into
   * the output-buffer
   */
  priv->bitslide |= (next<<priv->bitslide_size);
  priv->bitslide_size += next_size;
}


void encoder_hdlc_demand(ifax_modp self, size_t demand)
{
  encoder_hdlc_private *priv;
  ifax_uint8 *dst;
  ifax_uint32 remaining_bits, bytes;
  size_t chunk_bits, do_bits;

  priv = self->private;
  remaining_bits = demand;

  while ( remaining_bits > 0 ) {

    dst = priv->buffer;
    chunk_bits = 0;

    bytes = remaining_bits>>3;
    if ( bytes > (MAXBUFFER-1) )
      bytes = (MAXBUFFER-1);

    if ( bytes ) {
      /* Fill buffer with a whole number of bytes first */
      chunk_bits = 8 * bytes;
      remaining_bits -= chunk_bits;
      while ( bytes-- ) {
	if ( priv->bitslide_size < 17 )
	  produce_bits(priv);
	*dst++ = priv->bitslide & 0xff;
	priv->bitslide >>= 8;
	priv->bitslide_size -= 8;
      }
    }

    /* Finnish off with a possibly half-filled byte */
    if ( remaining_bits > 0 ) {
      if ( priv->bitslide_size < 17 )
	produce_bits(priv);
      do_bits = remaining_bits;
      if ( do_bits > 8 )
	do_bits = 8;
      *dst++ = priv->bitslide & 0xff;
      priv->bitslide >>= do_bits;
      priv->bitslide_size -= do_bits;
      chunk_bits += do_bits;
      remaining_bits -= do_bits;
    }

    ifax_handle_input(self->sendto,priv->buffer,chunk_bits);
  }
}

static int frame_queue_size(encoder_hdlc_private *priv)
{
  if ( priv->new_frame < priv->current_frame )
    return QUEUESIZE + priv->new_frame - priv->current_frame;

  return priv->new_frame - priv->current_frame;
}

static void tx_frame(encoder_hdlc_private *priv, ifax_uint8 *start,
		     size_t size, ifax_uint8 address)
{
  if ( frame_queue_size(priv) >= (QUEUESIZE-1) )
    return;
  priv->framequeue[priv->new_frame].size = size;
  priv->framequeue[priv->new_frame].start = start;
  priv->framequeue[priv->new_frame].address = address;
  priv->new_frame++;
  if ( priv->new_frame >= QUEUESIZE )
    priv->new_frame = 0;
}

int encoder_hdlc_command(ifax_modp self, int cmd, va_list cmds)
{
  encoder_hdlc_private *priv = self->private;
  ifax_uint8 address, *frame_start;
  size_t frame_size;

  switch ( cmd ) {

    case CMD_FRAMING_HDLC_TXFRAME:
      frame_start = va_arg(cmds,ifax_uint8 *);
      frame_size = va_arg(cmds,int);
      address = va_arg(cmds,ifax_uint8);
      tx_frame(priv,frame_start,frame_size,address);
      break;
  }

  return 0;
}

void encoder_hdlc_destroy(ifax_modp self)
{
  free(self->private);
}

int encoder_hdlc_construct(ifax_modp self,va_list args)
{
  encoder_hdlc_private *priv;
  
  priv = ifax_malloc(sizeof(encoder_hdlc_private),"HDLC encoder instance");
  self->private = priv;

  self->destroy =  encoder_hdlc_destroy;
  self->handle_input = 0;
  self->handle_demand = encoder_hdlc_demand;
  self->command = encoder_hdlc_command;

  priv->bitslide = (FLAG_SEQUENCE<<8) | FLAG_SEQUENCE;
  priv->bitslide_size = 16;
  priv->phase = IDLE;
  priv->new_frame = 0;
  priv->current_frame = 0;

  return 0;
}
