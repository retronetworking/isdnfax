/* $Id$
******************************************************************************

   Fax program for ISDN.
   Decoder for hdlc data input. Expects a datastream 0/1 bytes for the
   incoming bits at the bitrate (not samplerate).
   Outputs decoded bytes in a short int. This allows for giving a few extra
   codes for signaling start flags, checksum tests and error conditions.
   Automatically locks onto the start flag and undoes bit stuffing.

   Copyright (C) 1998 Andreas Beck	[becka@ggi-project.org]
  
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
 
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/times.h>
#include <ifax/ifax.h>
#include <ifax/modules/decode_hdlc.h>

#define _HDLC_FLAG (0x7e)
#define _HDLC_STUFFMASK (0x3f)
#define _HDLC_STUFF     (0x3e)

#define _CRC_INIT    0xffff
#define _CRC_TOPMASK 0x8000
#define _CRC_POLY    0x1021
#define _CRC_GOOD    0x1d0f

typedef struct {

	int	syncbitcnt;
	int	bits;
	int 	have_stuffed;

	ifax_uint16	crc;

} decode_hdlc_private;

/* Free the private data
 */
void	decode_hdlc_destroy(ifax_modp self)
{
	/*decode_hdlc_private *priv=(decode_hdlc_private *)self->private;*/

	free(self->private);

	return;
}

int	decode_hdlc_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	decode_hdlc_handle(ifax_modp self, void *data, size_t length)
{
	char *dat=data;
	int currbit;
	int handled,x;
	ifax_uint16 result;
	
	decode_hdlc_private *priv=(decode_hdlc_private *)self->private;

	handled=0;
	while(length--) {

		currbit =*dat++;
		ifax_dprintf(DEBUG_DEBUG,"Bit %d is %d\n",priv->syncbitcnt,currbit);

		/* Shift buffer up, place bit into buffer. 
		 */
		priv->bits<<=1;
		priv->bits|=!!currbit;
		priv->syncbitcnt++;

		/* Check if we have a flag sequence. If yes, check CRC of the
		 * previous block, send the appropriate code, and then a FLAG
		 * code. Reset bitcounter and CRC calc field.
		 */
		if ((priv->bits&0xff)==_HDLC_FLAG)
		{
			result= (priv->crc == _CRC_GOOD) ? 
				HDLC_CRC_OK : HDLC_CRC_ERR;
			if (self->sendto)
				ifax_handle_input(self->sendto,&result,1);
			ifax_dprintf(DEBUG_DEBUG,"HDLC CRC %s.\n",result==HDLC_CRC_OK ? "good" : "error");

			result=HDLC_FLAG;
			if (self->sendto)
				ifax_handle_input(self->sendto,&result,1);
			ifax_dprintf(DEBUG_DEBUG,"HDLC FLAG\n");

			priv->syncbitcnt=0;
			priv->crc=_CRC_INIT;
			priv->have_stuffed=0;
		} 
		/* Check, if bit-stuffing occured. If we received a pattern of
		 * 111110, the 0 is "stuffed" in. we remove it and flag that
		 * we have done it to avoid doing that over and over again if
		 * more zeroes follow.
		 */
		else if (!priv->have_stuffed &&(priv->bits&_HDLC_STUFFMASK)==_HDLC_STUFF)
		{
			/* We don't mention this on the stream. */
			ifax_dprintf(DEBUG_DEBUG,"Stuffbit removed !\n");
			priv->bits>>=1;
			priv->syncbitcnt--;
			priv->have_stuffed=1;
		} 
		/* Do we have a complete byte (don't check this, if stuffing 
		 * occured - we are still in the same byte, then.
		 */
		else if ((priv->syncbitcnt&7)==0)
		{
			/* Calculate the CRC.
			 */
			for(x=7;x>=0;x--)
			{
				if (!(priv->crc&_CRC_TOPMASK) !=
				    !(priv->bits&(1<<x))) 
				{ 
					priv->crc<<=1;
					priv->crc^=_CRC_POLY;
				}
				else
					priv->crc<<=1;
				ifax_dprintf(DEBUG_JUNK,"CRC %04x\n",priv->crc);
			}
			/* We have to reset the bitstuffing flag in all 
			 * non-stuffing cases ...
			 */
			priv->have_stuffed=0;
			/* mask out the result and transmit it.
			 */
			result=priv->bits&0xff;
			if (self->sendto)
				ifax_handle_input(self->sendto,&result,1);
			ifax_dprintf(DEBUG_DEBUG,"HDLC %x, %x\n",priv->bits&0xff,priv->crc);
		} else	priv->have_stuffed=0;
		handled++;
	}
	return handled;
}

int	decode_hdlc_construct(ifax_modp self,va_list args)
{
	decode_hdlc_private *priv;
	
	if (NULL==(priv=self->private=malloc(sizeof(decode_hdlc_private)))) 
		return 1;
	self->destroy		=decode_hdlc_destroy;
	self->handle_input	=decode_hdlc_handle;
	self->command		=decode_hdlc_command;

	priv->syncbitcnt=0;
	priv->bits=0;
	priv->have_stuffed=0;

	return 0;
}
