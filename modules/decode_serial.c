/* $Id$
******************************************************************************

   Fax program for ISDN.
   Decoder for serial data input. Expects a datastream of alternating
   0/1,confidence bytes at SAMPLES_PER_SECOND.
   Outputs decoded bytes that came in without any error.
   Automatically locks onto the start/stop bits.

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

enum parity {
	PAR_NONE,
	PAR_ODD,
	PAR_EVEN,
	PAR_ONE,
	PAR_ZERO
};

typedef struct {

	int		baud;
	int		bits;
	int 		stopbits;
	enum parity	parity;

	char lastbits[11];
	int bitnum;
	int sampcount;

} decode_serial_private;

/* Free the private data
 */
void	decode_serial_destroy(ifax_modp self)
{
	/* decode_serial_private *priv=(decode_serial_private *)self->private; */

	free(self->private);

	return;
}

int	decode_serial_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	decode_serial_handle(ifax_modp self, void *data, size_t length)
{
	char *dat=data;
	int currbit,currconf;
	int x,result,handled,parity;
	char hlpres;
	
	decode_serial_private *priv=(decode_serial_private *)self->private;

	handled=0;
	while(length--) {

		currbit =*dat++;
		currconf=*dat++;
		priv->sampcount++;
		
		/* printf("%d,%d\n",currbit,currconf); */

		if (currconf<10) continue;
		
		if (priv->bitnum==-1) {
			if (currbit==1) continue;
			priv->bitnum=0;
			priv->sampcount=-SAMPLES_PER_SECOND/2/priv->baud;
		} else
		if ( priv->sampcount >= priv->bitnum*
					SAMPLES_PER_SECOND/priv->baud)
		{
			if (priv->bitnum==0 && currbit!=0) {
				priv->bitnum=-1;
				continue;	/* Garbage */
			}
			if (priv->bitnum==9 && currbit!=1) {
				priv->bitnum=-1;
				continue;	/* Garbage */
			}

			priv->lastbits[priv->bitnum++]=currbit;

			if (priv->bitnum >= 1+8+priv->stopbits) {
				result=parity=0;
				for (x=0;x<priv->bits;x++)
				{
					result|=priv->lastbits[x+1]<<x;
					parity^=priv->lastbits[x+1];
				}
				/* for (x=0;x<priv->bitnum;x++)
				   printf("%d%s",priv->lastbits[x], (x==0 || x==8) ? " " : ""); */
				priv->bitnum=-1;
				switch(priv->parity) {
					case PAR_NONE: parity=0;break;
					case PAR_ODD:  parity=(parity==priv->lastbits[8]);break;
					case PAR_EVEN: parity=(parity!=priv->lastbits[8]);break;
					case PAR_ONE:  parity=(1!=priv->lastbits[8]);break;
					case PAR_ZERO: parity=(0!=priv->lastbits[8]);break;
				}
				hlpres=result;
				/* printf(" %c\n",result); */
				if (parity==0 && self->sendto) 
					ifax_handle_input(self->sendto,&hlpres,1);
			}
		}
		handled++;
	}
	return handled;
}

int	decode_serial_construct(ifax_modp self,va_list args)
{
	decode_serial_private *priv;
	char *encode;
	
	if (NULL==(priv=self->private=malloc(sizeof(decode_serial_private)))) 
		return 1;
	self->destroy		=decode_serial_destroy;
	self->handle_input	=decode_serial_handle;
	self->command		=decode_serial_command;

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

	return 0;
}
