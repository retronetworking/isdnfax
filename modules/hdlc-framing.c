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
#include <ifax/bitreverse.h>
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
 * bit-stuffing should be applied.  The HDLC protocol specifies that
 * 6 bits in a row with value '1' is reserved for the FLAG sequence.
 * A flag is used to start and stop a frame.
 * When there are 5 bits in a row with value '1' in the body of
 * a frame, they have to be bit-stuffed, so that there is a '0'
 * inserted after the first 5 bits.  The bit-stream is expanded as
 * a result of this, but the advantage is that frame synchronisation
 * is rapidly established.
 *
 * The 'bitstufftbl' is used to answer the following two questions:
 *
 *   1) How many consecutive '1' is there from LSB and up
 *   2) Should a '0' be inserted in bit-positions 5-8 (0 means no stuffing)
 *
 * The index to the table is the 8 bit unsigned intger in question:
 *
 *       x7 x6 x5 x4 x3 x2 x1 x0
 *      MSB                   LSB
 *
 * The lookup-value is a packed character, whith the following fields:
 *
 *       v7 v6 v5 v4   | v3 v2 v1 v0
 *       --------------+------------
 *       Position      | Number of
 *       of needed     | '1' from
 *       stuffing      | LSB and up
 *       0 => none     |
 *       5-8 => x5-x8  |
 */

static ifax_uint8 bitstufftbl[256] = {
  0x00,    /* 00000000 => 0 bits                    */
  0x01,    /* 00000001 => 1 bits                    */
  0x00,    /* 00000010 => 0 bits                    */
  0x02,    /* 00000011 => 2 bits                    */
  0x00,    /* 00000100 => 0 bits                    */
  0x01,    /* 00000101 => 1 bits                    */
  0x00,    /* 00000110 => 0 bits                    */
  0x03,    /* 00000111 => 3 bits                    */
  0x00,    /* 00001000 => 0 bits                    */
  0x01,    /* 00001001 => 1 bits                    */
  0x00,    /* 00001010 => 0 bits                    */
  0x02,    /* 00001011 => 2 bits                    */
  0x00,    /* 00001100 => 0 bits                    */
  0x01,    /* 00001101 => 1 bits                    */
  0x00,    /* 00001110 => 0 bits                    */
  0x04,    /* 00001111 => 4 bits                    */
  0x00,    /* 00010000 => 0 bits                    */
  0x01,    /* 00010001 => 1 bits                    */
  0x00,    /* 00010010 => 0 bits                    */
  0x02,    /* 00010011 => 2 bits                    */
  0x00,    /* 00010100 => 0 bits                    */
  0x01,    /* 00010101 => 1 bits                    */
  0x00,    /* 00010110 => 0 bits                    */
  0x03,    /* 00010111 => 3 bits                    */
  0x00,    /* 00011000 => 0 bits                    */
  0x01,    /* 00011001 => 1 bits                    */
  0x00,    /* 00011010 => 0 bits                    */
  0x02,    /* 00011011 => 2 bits                    */
  0x00,    /* 00011100 => 0 bits                    */
  0x01,    /* 00011101 => 1 bits                    */
  0x00,    /* 00011110 => 0 bits                    */
  0x55,    /* 00011111 => 5 bits   (Stuff at bit 5) */
  0x00,    /* 00100000 => 0 bits                    */
  0x01,    /* 00100001 => 1 bits                    */
  0x00,    /* 00100010 => 0 bits                    */
  0x02,    /* 00100011 => 2 bits                    */
  0x00,    /* 00100100 => 0 bits                    */
  0x01,    /* 00100101 => 1 bits                    */
  0x00,    /* 00100110 => 0 bits                    */
  0x03,    /* 00100111 => 3 bits                    */
  0x00,    /* 00101000 => 0 bits                    */
  0x01,    /* 00101001 => 1 bits                    */
  0x00,    /* 00101010 => 0 bits                    */
  0x02,    /* 00101011 => 2 bits                    */
  0x00,    /* 00101100 => 0 bits                    */
  0x01,    /* 00101101 => 1 bits                    */
  0x00,    /* 00101110 => 0 bits                    */
  0x04,    /* 00101111 => 4 bits                    */
  0x00,    /* 00110000 => 0 bits                    */
  0x01,    /* 00110001 => 1 bits                    */
  0x00,    /* 00110010 => 0 bits                    */
  0x02,    /* 00110011 => 2 bits                    */
  0x00,    /* 00110100 => 0 bits                    */
  0x01,    /* 00110101 => 1 bits                    */
  0x00,    /* 00110110 => 0 bits                    */
  0x03,    /* 00110111 => 3 bits                    */
  0x00,    /* 00111000 => 0 bits                    */
  0x01,    /* 00111001 => 1 bits                    */
  0x00,    /* 00111010 => 0 bits                    */
  0x02,    /* 00111011 => 2 bits                    */
  0x00,    /* 00111100 => 0 bits                    */
  0x01,    /* 00111101 => 1 bits                    */
  0x60,    /* 00111110 => 0 bits   (Stuff at bit 6) */
  0x56,    /* 00111111 => 6 bits   (Stuff at bit 5) */
  0x00,    /* 01000000 => 0 bits                    */
  0x01,    /* 01000001 => 1 bits                    */
  0x00,    /* 01000010 => 0 bits                    */
  0x02,    /* 01000011 => 2 bits                    */
  0x00,    /* 01000100 => 0 bits                    */
  0x01,    /* 01000101 => 1 bits                    */
  0x00,    /* 01000110 => 0 bits                    */
  0x03,    /* 01000111 => 3 bits                    */
  0x00,    /* 01001000 => 0 bits                    */
  0x01,    /* 01001001 => 1 bits                    */
  0x00,    /* 01001010 => 0 bits                    */
  0x02,    /* 01001011 => 2 bits                    */
  0x00,    /* 01001100 => 0 bits                    */
  0x01,    /* 01001101 => 1 bits                    */
  0x00,    /* 01001110 => 0 bits                    */
  0x04,    /* 01001111 => 4 bits                    */
  0x00,    /* 01010000 => 0 bits                    */
  0x01,    /* 01010001 => 1 bits                    */
  0x00,    /* 01010010 => 0 bits                    */
  0x02,    /* 01010011 => 2 bits                    */
  0x00,    /* 01010100 => 0 bits                    */
  0x01,    /* 01010101 => 1 bits                    */
  0x00,    /* 01010110 => 0 bits                    */
  0x03,    /* 01010111 => 3 bits                    */
  0x00,    /* 01011000 => 0 bits                    */
  0x01,    /* 01011001 => 1 bits                    */
  0x00,    /* 01011010 => 0 bits                    */
  0x02,    /* 01011011 => 2 bits                    */
  0x00,    /* 01011100 => 0 bits                    */
  0x01,    /* 01011101 => 1 bits                    */
  0x00,    /* 01011110 => 0 bits                    */
  0x55,    /* 01011111 => 5 bits   (Stuff at bit 5) */
  0x00,    /* 01100000 => 0 bits                    */
  0x01,    /* 01100001 => 1 bits                    */
  0x00,    /* 01100010 => 0 bits                    */
  0x02,    /* 01100011 => 2 bits                    */
  0x00,    /* 01100100 => 0 bits                    */
  0x01,    /* 01100101 => 1 bits                    */
  0x00,    /* 01100110 => 0 bits                    */
  0x03,    /* 01100111 => 3 bits                    */
  0x00,    /* 01101000 => 0 bits                    */
  0x01,    /* 01101001 => 1 bits                    */
  0x00,    /* 01101010 => 0 bits                    */
  0x02,    /* 01101011 => 2 bits                    */
  0x00,    /* 01101100 => 0 bits                    */
  0x01,    /* 01101101 => 1 bits                    */
  0x00,    /* 01101110 => 0 bits                    */
  0x04,    /* 01101111 => 4 bits                    */
  0x00,    /* 01110000 => 0 bits                    */
  0x01,    /* 01110001 => 1 bits                    */
  0x00,    /* 01110010 => 0 bits                    */
  0x02,    /* 01110011 => 2 bits                    */
  0x00,    /* 01110100 => 0 bits                    */
  0x01,    /* 01110101 => 1 bits                    */
  0x00,    /* 01110110 => 0 bits                    */
  0x03,    /* 01110111 => 3 bits                    */
  0x00,    /* 01111000 => 0 bits                    */
  0x01,    /* 01111001 => 1 bits                    */
  0x00,    /* 01111010 => 0 bits                    */
  0x02,    /* 01111011 => 2 bits                    */
  0x70,    /* 01111100 => 0 bits   (Stuff at bit 7) */
  0x71,    /* 01111101 => 1 bits   (Stuff at bit 7) */
  0x60,    /* 01111110 => 0 bits   (Stuff at bit 6) */
  0x57,    /* 01111111 => 7 bits   (Stuff at bit 5) */
  0x00,    /* 10000000 => 0 bits                    */
  0x01,    /* 10000001 => 1 bits                    */
  0x00,    /* 10000010 => 0 bits                    */
  0x02,    /* 10000011 => 2 bits                    */
  0x00,    /* 10000100 => 0 bits                    */
  0x01,    /* 10000101 => 1 bits                    */
  0x00,    /* 10000110 => 0 bits                    */
  0x03,    /* 10000111 => 3 bits                    */
  0x00,    /* 10001000 => 0 bits                    */
  0x01,    /* 10001001 => 1 bits                    */
  0x00,    /* 10001010 => 0 bits                    */
  0x02,    /* 10001011 => 2 bits                    */
  0x00,    /* 10001100 => 0 bits                    */
  0x01,    /* 10001101 => 1 bits                    */
  0x00,    /* 10001110 => 0 bits                    */
  0x04,    /* 10001111 => 4 bits                    */
  0x00,    /* 10010000 => 0 bits                    */
  0x01,    /* 10010001 => 1 bits                    */
  0x00,    /* 10010010 => 0 bits                    */
  0x02,    /* 10010011 => 2 bits                    */
  0x00,    /* 10010100 => 0 bits                    */
  0x01,    /* 10010101 => 1 bits                    */
  0x00,    /* 10010110 => 0 bits                    */
  0x03,    /* 10010111 => 3 bits                    */
  0x00,    /* 10011000 => 0 bits                    */
  0x01,    /* 10011001 => 1 bits                    */
  0x00,    /* 10011010 => 0 bits                    */
  0x02,    /* 10011011 => 2 bits                    */
  0x00,    /* 10011100 => 0 bits                    */
  0x01,    /* 10011101 => 1 bits                    */
  0x00,    /* 10011110 => 0 bits                    */
  0x55,    /* 10011111 => 5 bits   (Stuff at bit 5) */
  0x00,    /* 10100000 => 0 bits                    */
  0x01,    /* 10100001 => 1 bits                    */
  0x00,    /* 10100010 => 0 bits                    */
  0x02,    /* 10100011 => 2 bits                    */
  0x00,    /* 10100100 => 0 bits                    */
  0x01,    /* 10100101 => 1 bits                    */
  0x00,    /* 10100110 => 0 bits                    */
  0x03,    /* 10100111 => 3 bits                    */
  0x00,    /* 10101000 => 0 bits                    */
  0x01,    /* 10101001 => 1 bits                    */
  0x00,    /* 10101010 => 0 bits                    */
  0x02,    /* 10101011 => 2 bits                    */
  0x00,    /* 10101100 => 0 bits                    */
  0x01,    /* 10101101 => 1 bits                    */
  0x00,    /* 10101110 => 0 bits                    */
  0x04,    /* 10101111 => 4 bits                    */
  0x00,    /* 10110000 => 0 bits                    */
  0x01,    /* 10110001 => 1 bits                    */
  0x00,    /* 10110010 => 0 bits                    */
  0x02,    /* 10110011 => 2 bits                    */
  0x00,    /* 10110100 => 0 bits                    */
  0x01,    /* 10110101 => 1 bits                    */
  0x00,    /* 10110110 => 0 bits                    */
  0x03,    /* 10110111 => 3 bits                    */
  0x00,    /* 10111000 => 0 bits                    */
  0x01,    /* 10111001 => 1 bits                    */
  0x00,    /* 10111010 => 0 bits                    */
  0x02,    /* 10111011 => 2 bits                    */
  0x00,    /* 10111100 => 0 bits                    */
  0x01,    /* 10111101 => 1 bits                    */
  0x60,    /* 10111110 => 0 bits   (Stuff at bit 6) */
  0x56,    /* 10111111 => 6 bits   (Stuff at bit 5) */
  0x00,    /* 11000000 => 0 bits                    */
  0x01,    /* 11000001 => 1 bits                    */
  0x00,    /* 11000010 => 0 bits                    */
  0x02,    /* 11000011 => 2 bits                    */
  0x00,    /* 11000100 => 0 bits                    */
  0x01,    /* 11000101 => 1 bits                    */
  0x00,    /* 11000110 => 0 bits                    */
  0x03,    /* 11000111 => 3 bits                    */
  0x00,    /* 11001000 => 0 bits                    */
  0x01,    /* 11001001 => 1 bits                    */
  0x00,    /* 11001010 => 0 bits                    */
  0x02,    /* 11001011 => 2 bits                    */
  0x00,    /* 11001100 => 0 bits                    */
  0x01,    /* 11001101 => 1 bits                    */
  0x00,    /* 11001110 => 0 bits                    */
  0x04,    /* 11001111 => 4 bits                    */
  0x00,    /* 11010000 => 0 bits                    */
  0x01,    /* 11010001 => 1 bits                    */
  0x00,    /* 11010010 => 0 bits                    */
  0x02,    /* 11010011 => 2 bits                    */
  0x00,    /* 11010100 => 0 bits                    */
  0x01,    /* 11010101 => 1 bits                    */
  0x00,    /* 11010110 => 0 bits                    */
  0x03,    /* 11010111 => 3 bits                    */
  0x00,    /* 11011000 => 0 bits                    */
  0x01,    /* 11011001 => 1 bits                    */
  0x00,    /* 11011010 => 0 bits                    */
  0x02,    /* 11011011 => 2 bits                    */
  0x00,    /* 11011100 => 0 bits                    */
  0x01,    /* 11011101 => 1 bits                    */
  0x00,    /* 11011110 => 0 bits                    */
  0x55,    /* 11011111 => 5 bits   (Stuff at bit 5) */
  0x00,    /* 11100000 => 0 bits                    */
  0x01,    /* 11100001 => 1 bits                    */
  0x00,    /* 11100010 => 0 bits                    */
  0x02,    /* 11100011 => 2 bits                    */
  0x00,    /* 11100100 => 0 bits                    */
  0x01,    /* 11100101 => 1 bits                    */
  0x00,    /* 11100110 => 0 bits                    */
  0x03,    /* 11100111 => 3 bits                    */
  0x00,    /* 11101000 => 0 bits                    */
  0x01,    /* 11101001 => 1 bits                    */
  0x00,    /* 11101010 => 0 bits                    */
  0x02,    /* 11101011 => 2 bits                    */
  0x00,    /* 11101100 => 0 bits                    */
  0x01,    /* 11101101 => 1 bits                    */
  0x00,    /* 11101110 => 0 bits                    */
  0x04,    /* 11101111 => 4 bits                    */
  0x00,    /* 11110000 => 0 bits                    */
  0x01,    /* 11110001 => 1 bits                    */
  0x00,    /* 11110010 => 0 bits                    */
  0x02,    /* 11110011 => 2 bits                    */
  0x00,    /* 11110100 => 0 bits                    */
  0x01,    /* 11110101 => 1 bits                    */
  0x00,    /* 11110110 => 0 bits                    */
  0x03,    /* 11110111 => 3 bits                    */
  0x80,    /* 11111000 => 0 bits   (Stuff at bit 8) */
  0x81,    /* 11111001 => 1 bits   (Stuff at bit 8) */
  0x80,    /* 11111010 => 0 bits   (Stuff at bit 8) */
  0x82,    /* 11111011 => 2 bits   (Stuff at bit 8) */
  0x70,    /* 11111100 => 0 bits   (Stuff at bit 7) */
  0x71,    /* 11111101 => 1 bits   (Stuff at bit 7) */
  0x60,    /* 11111110 => 0 bits   (Stuff at bit 6) */
  0x58,    /* 11111111 => 8 bits   (Stuff at bit 5) */
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
 *
 * A simple schetch to illustrate when bit-stuffing is
 * performed (I got it wrong the first time...:-):
 *
 *  Pos   Stuffed  Previous
 *  -----------------------
 *   0     ???????+ 11111???
 *   1     ??????+1 11110???
 *   2     ?????+11 1110????
 *   3     ????+111 110?????
 *   4     ???+1111 10??????
 *   5     ??+11111 0???????
 *   6   ? ?+111110 ????????
 *   7   ? +111110? ????????
 *   8   + 111110?? ????????
 *   9  +1 11110??? ????????
 *
 * Notice:   ?  Don't care
 *           +  This is where bit-stuffing must be applied
 *
 * When bit-stuffing is applied, all the bits including the '+' and up
 * is shifted one bit to the left, replacing the '+' with a zero.
 * When bit-stuffing is applied in position 0-3, a second stuffing may
 * have to be performed in position 6-9.
 */

void produce_bits(encoder_hdlc_private *priv)
{
  ifax_uint16 next, LSB_mask, LSB_bits;
  ifax_uint8 n, p, previous;
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
  next = (*priv->src++ & 0x00ff);
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
	priv->src = &priv->fcs_tx[0];
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

  previous = priv->bitslide>>(priv->bitslide_size-8);
  p = bitstufftbl[bitreverse[previous]&0xF] & 0xF;
  n = bitstufftbl[next] & 0xF;

  if ( p+n >= 5 ) {
    /* Need bit-stuffing somewhere in position 1-5 */
    stuffpos = 5 - p;
    LSB_mask = (1<<stuffpos) - 1;
    LSB_bits = next & LSB_mask;
    next &= ~LSB_mask;
    next <<= 1;
    next_size++;
    next |= LSB_bits;
  }

  /* The table-lookup will tell us if we have a string of '1' completely
   * inside the 'next' word (which is 8 or 9 bits large at this point)
   * which needs bit-stuffing.  Since any string of 1s occupying
   * bit-position 0 is taken care of in the previous test and stuffing,
   * we can now consentrate on bits 1-8 (we assume bit 8 is used - it
   * will be 0 and not cause bit-stuffing if it is not).
   */

  stuffpos = bitstufftbl[next>>1]>>4;

  if ( stuffpos ) {
    LSB_mask = (2<<stuffpos) - 1;
    LSB_bits = next & LSB_mask;
    next &= ~LSB_mask;
    next <<= 1;
    next_size++;
    next |= LSB_bits;
  }

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
