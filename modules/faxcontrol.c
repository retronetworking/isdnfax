/* $Id$
******************************************************************************

   Fax program for ISDN.
   Decoder for fax control commands. Expects a datastream of short ints
   as produced by the decode_hdlc module. 

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
#include <ifax/modules/faxcontrol.h>

#define MAXLENGTH	1024

typedef struct {

	int	length;

	unsigned char	data[MAXLENGTH];

} faxcontrol_private;

#define FXCTRL_ADDRESS(x)		(x[0])
#define FXCTRL_ADDRESS_OK		(0xff)

#define FXCTRL_CONTROL(x)		(x[1])
#define FXCTRL_CONTROL_NONFINAL		(0xc0)
#define FXCTRL_CONTROL_FINAL		(0xc8)

#define FXCTRL_FCF(x)			(x[2])
#define FXCTRL_FCF_DIS			(0x01)
#define FXCTRL_FCF_CSI			(0x02)
#define FXCTRL_FCF_NSF			(0x04)

static const unsigned char CSI_table[32]=
{
	' ',/* 00000 */
	'0',/* 00001 */
	'?',/* 00010 */
	'8',/* 00011 */
	'?',/* 00100 */
	'4',/* 00101 */
	'?',/* 00110 */
	'?',/* 00111 */
	'?',/* 01000 */
	'2',/* 01001 */
	'*',/* 01010 */
	'?',/* 01011 */
	'?',/* 01100 */
	'6',/* 01101 */
	'?',/* 01110 */
	'?',/* 01111 */
	'?',/* 10000 */
	'1',/* 10001 */
	'?',/* 10010 */
	'9',/* 10011 */
	'?',/* 10100 */
	'5',/* 10101 */
	'?',/* 10110 */
	'?',/* 10111 */
	'#',/* 11000 */
	'3',/* 11001 */
	'+',/* 11010 */
	'?',/* 11011 */
	'?',/* 11100 */
	'7',/* 11101 */
	'?',/* 11110 */
	'?',/* 11111 */
};

static unsigned char CSI_decode(unsigned char number)
{
	if ((number&7)!=4)
		ifax_dprintf(DEBUG_WARNING,"Illegal CSI number.\n");

	return(CSI_table[number>>3]);
}

#define HAS_V8(caps)		(caps[0]&0x20)
#define WANT_64OCTETS(caps)	(caps[0]&0x40)
#define HAS_POLL(caps)		(caps[1]&0x01)
#define IS_RECEIVER(caps)	(caps[1]&0x02)
#define HAS_HIGHRES(caps)	(caps[1]&0x40)
#define HAS_2DCODING(caps)	(caps[1]&0x80)

static void show_caps(unsigned char *caps,int length) 
{
	if (length<3)
		ifax_dprintf(DEBUG_WARNING,"Illegal short capability field.\n");

	if (HAS_V8(caps))
		ifax_dprintf(DEBUG_WARNING,"Have V.8 functionality.\n");
	if (WANT_64OCTETS(caps))
		ifax_dprintf(DEBUG_WARNING,"Want 64 octets.\n");
	if (HAS_POLL(caps))
		ifax_dprintf(DEBUG_WARNING,"Have a pollable document.\n");
	if (IS_RECEIVER(caps))
		ifax_dprintf(DEBUG_WARNING,"I'm a receiver.\n");
	if (HAS_HIGHRES(caps))
		ifax_dprintf(DEBUG_WARNING,"Can do 200x200dpi.\n");
	if (HAS_2DCODING(caps))
		ifax_dprintf(DEBUG_WARNING,"Can do 2D coding.\n");
}

static void interpret(faxcontrol_private *priv)
{
	int x;

	if (priv->length<3) {
		ifax_dprintf(DEBUG_WARNING,"Illegal short command frame.\n");
		return;
	}

	if (FXCTRL_ADDRESS(priv->data)!=FXCTRL_ADDRESS_OK) {
		ifax_dprintf(DEBUG_WARNING,"Illegal address 0x%x selected.\n",
				FXCTRL_ADDRESS(priv->data));
		return;
	}

	switch(FXCTRL_CONTROL(priv->data)) {
		case FXCTRL_CONTROL_NONFINAL:
			ifax_dprintf(DEBUG_WARNING,"Non-Final Frame:\n");
			break;
		case FXCTRL_CONTROL_FINAL:
			ifax_dprintf(DEBUG_WARNING,"Final Frame:\n");
			break;
		default:
			ifax_dprintf(DEBUG_WARNING,"Illegal control field 0x%x.\n",
					FXCTRL_CONTROL(priv->data));
		return;
	}

	switch(FXCTRL_FCF(priv->data)) {
		case FXCTRL_FCF_DIS:
			ifax_dprintf(DEBUG_WARNING,"DIS:\n");
			show_caps(&priv->data[3],priv->length-3);
			break;
		case FXCTRL_FCF_CSI:
			ifax_dprintf(DEBUG_WARNING,"CSI:\n");
			for(x=priv->length-1;x>=3;x--) {
				unsigned char number;
				number=CSI_decode(priv->data[x]);
				ifax_dprintf(DEBUG_WARNING,"%c",number);
			}
			ifax_dprintf(DEBUG_WARNING,"\n");
			break;
		case FXCTRL_FCF_NSF:
			ifax_dprintf(DEBUG_WARNING,"NSF:\n");
			break;
		default:
			ifax_dprintf(DEBUG_WARNING,"Illegal FCF 0x%x.\n",
					FXCTRL_CONTROL(priv->data));
		return;
	}
	
	for(x=0;x<priv->length;x++)
		fprintf(stderr,"%x ",priv->data[x]);
	fprintf(stderr,"\n");
}

/* Free the private data
 */
void	faxcontrol_destroy(ifax_modp self)
{
	faxcontrol_private *priv=(faxcontrol_private *)self->private;

	free(self->private);

	return;
}

int	faxcontrol_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	faxcontrol_handle(ifax_modp self, void *data, size_t length)
{
	ifax_uint16 *dat=data;
	ifax_uint16 curr;
	int handled;
	
	faxcontrol_private *priv=(faxcontrol_private *)self->private;

	handled=0;
	while(length--) {
		curr=*dat++;
		if (curr==HDLC_FLAG) {
			if (priv->length) 
				ifax_dprintf(DEBUG_WARNING,"HDLC_FLAG occurred unexpectedly.\n");
			priv->length=0;
			continue;
		}
		if (curr==HDLC_CRC_OK) {
			priv->length-=2;	/* skip the CRC bytes */
			interpret(priv);
			priv->length=0;
			continue;
		}
		if (curr==HDLC_CRC_ERR) {
			if (priv->length) {
				ifax_dprintf(DEBUG_WARNING,"HDLC_CRC_ERROR occurred.\n");
				priv->length=0;
			}
			continue;
		}
		if (priv->length==MAXLENGTH) continue;	/* Huh ? */
		priv->data[priv->length++]=(unsigned char)curr;
		handled++;
	}
	return handled;
}

int	faxcontrol_construct(ifax_modp self,va_list args)
{
	faxcontrol_private *priv;
	
	if (NULL==(priv=self->private=malloc(sizeof(faxcontrol_private)))) 
		return 1;
	self->destroy		=faxcontrol_destroy;
	self->handle_input	=faxcontrol_handle;
	self->command		=faxcontrol_command;

	/* priv->baud=va_arg(args,int); */

	priv->length=0;	/* Init to 0 bytes in queue */
	return 0;
}
