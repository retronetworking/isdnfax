/* $Id$
******************************************************************************

   Fax program for ISDN.
   Serial bits encoder. Takes bytes as input and generates 0/1 bytes at 
   SAMPLES_PER_SECOND. Take care not to overflow the input buffer.

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
#include <string.h>
#include <sys/times.h>
#include <ifax/ifax.h>

enum parity {
	PAR_NONE,
	PAR_ODD,
	PAR_EVEN,
	PAR_ONE,
	PAR_ZERO
};

#define MAXBUFLEN 256

typedef struct {

	int		baud;
	int		bits;
	int 		stopbits;
	enum parity	parity;

	char lastbits[11];
	int bitnum;
	int sampcount;

	char buffer[MAXBUFLEN];
	int bufferlen;

} encode_serial_private;

/* Free the private data
 */
void	encode_serial_destroy(ifax_modp self)
{
	encode_serial_private *priv=(encode_serial_private *)self->private;

	free(self->private);

	return;
}

int	encode_serial_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	encode_serial_handle(ifax_modp self, void *data, size_t length)
{
	char *dat=data;
	int x,handled,parity;
	char hlpres;
	
	encode_serial_private *priv=(encode_serial_private *)self->private;

	handled=0;
	while(length--) {
		if (priv->bufferlen<MAXBUFLEN) {
			priv->buffer[priv->bufferlen++]=*dat++;
			handled++;
		} else
			fprintf(stderr,"encode_serial: buffer overrun.");
	}
	if (priv->bufferlen && priv->bitnum==-1)
	{
		priv->lastbits[0]=0;	/* START BIT */
		parity=0;
		for(x=0;x<priv->bits;x++)
		{
			priv->lastbits[x+1]=((priv->buffer[0]&(1<<x))!=0);
			parity^=priv->lastbits[x+1];
		}
		switch(priv->parity) {
			case PAR_NONE: break;
			case PAR_ODD:  priv->lastbits[8]=!parity;break;
			case PAR_EVEN: priv->lastbits[8]= parity;break;
			case PAR_ONE:  priv->lastbits[8]= 1     ;break;
			case PAR_ZERO: priv->lastbits[8]= 0     ;break;
		}
		priv->lastbits[9]=1;	/* STOP BIT */
		priv->lastbits[10]=1;	/* STOP BIT - maybe not used */
		memmove(priv->buffer,priv->buffer+1,--priv->bufferlen);
		priv->bitnum=0;
		priv->sampcount=0;
	}

	hlpres=1;	/* constant 1 for no activity */

	if (priv->bitnum>=0)
	{
		hlpres=priv->lastbits[priv->bitnum];
		
		priv->sampcount++;
		
		if ( priv->sampcount >= (priv->bitnum+1)*
					SAMPLES_PER_SECOND/priv->baud)
		{
			priv->bitnum++;
			if (priv->bitnum>= 1+8+priv->stopbits)
				priv->bitnum=-1;
		}
	}
	ifax_handle_input(self->sendto,&hlpres,1);

	return handled;
}

int	encode_serial_construct(ifax_modp self,va_list args)
{
	encode_serial_private *priv;
	char *encode;
	
	if (NULL==(priv=self->private=malloc(sizeof(encode_serial_private)))) 
		return 1;
	self->destroy		=encode_serial_destroy;
	self->handle_input	=encode_serial_handle;
	self->command		=encode_serial_command;

	priv->baud=va_arg(args,int);
	encode    =va_arg(args,char *);

	switch(encode[0])
	{
		case '7': priv->bits=7;break;
		default : fprintf(stderr,"not 7/8 bits !\n");
		case '8': priv->bits=8;break;
	}

	switch(encode[1])
	{
		default : fprintf(stderr,"invalid parity specified !\n");
		case 'N': case 'n': priv->parity=PAR_NONE;break;
		case 'O': case 'o': priv->parity=PAR_ODD ;break;
		case 'E': case 'e': priv->parity=PAR_EVEN;break;
		case '1':           priv->parity=PAR_ONE ;break;
		case '0':           priv->parity=PAR_ZERO;break;
	}

	switch(encode[2])
	{
		default : fprintf(stderr,"not 1/2 stopbits !\n");
		case '1': priv->stopbits=1;break;
		case '2': priv->stopbits=2;break;
	}

	priv->bitnum=-1;	/* Init to "wait for 0->1" */
	priv->sampcount=0;
	priv->bufferlen=0;

	return 0;
}
