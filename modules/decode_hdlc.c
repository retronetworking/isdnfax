/* $Id$
******************************************************************************

   Fax program for ISDN.
   Decoder for hdlc data input. Expects a datastream of alternating
   0/1,confidence bytes at SAMPLES_PER_SECOND.
   Outputs decoded bytes that came in without any error.
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

#define HDLC_FLAG (0x7e)
#define HDLC_STUFFMASK (0x3f)
#define HDLC_STUFF     (0x3e)

typedef struct {

	int	baud;

	int 	lastsamp;
	int 	bitnum;
	int 	sampcount;
	int	syncbitcnt;
	int	bits;
	int 	have_stuffed;

} decode_hdlc_private;

/* Free the private data
 */
void	decode_hdlc_destroy(ifax_modp self)
{
	decode_hdlc_private *priv=(decode_hdlc_private *)self->private;

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
	int currbit,currconf;
	int x,result,handled,parity;
	char hlpres;
	
	decode_hdlc_private *priv=(decode_hdlc_private *)self->private;

	handled=0;
	while(length--) {

		currbit =*dat++;
		currconf=*dat++;
		priv->sampcount++;
		
//		printf("%d,%d\n",currbit,currconf);

		if (currconf<10) continue;
		
		if (priv->lastsamp!=currbit) {
//			printf("synchronizing due to %d->%d at %d\n",priv->lastsamp,currbit,priv->sampcount);
			priv->lastsamp=currbit;
			priv->sampcount=-SAMPLES_PER_SECOND/2/priv->baud;
			priv->bitnum=0;
		} else
		if ( priv->sampcount >= priv->bitnum*
					SAMPLES_PER_SECOND/priv->baud)
		{
//			printf("Bit %d(%d) is %d\n",priv->syncbitcnt,priv->bitnum,currbit);
			priv->bits<<=1;
			priv->bits|=!!currbit;
			priv->syncbitcnt++;
			if ((priv->bits&0xff)==HDLC_FLAG)
			{
				printf("FLAG !\n");
				priv->syncbitcnt=0;
			} else if (!priv->have_stuffed &&(priv->bits&HDLC_STUFFMASK)==HDLC_STUFF)
			{
				printf("Stuff !\n");
				priv->bits>>=1;
				priv->syncbitcnt--;
				priv->have_stuffed=1;
			} else if ((priv->syncbitcnt&7)==0)
			{
				priv->have_stuffed=0;
				printf("HDLC %x\n",priv->bits&0xff);
			}
			priv->bitnum++;
		}
		handled++;
	}
	return handled;
}

int	decode_hdlc_construct(ifax_modp self,va_list args)
{
	decode_hdlc_private *priv;
	char *encode;
	
	if (NULL==(priv=self->private=malloc(sizeof(decode_hdlc_private)))) 
		return 1;
	self->destroy		=decode_hdlc_destroy;
	self->handle_input	=decode_hdlc_handle;
	self->command		=decode_hdlc_command;

	priv->baud=va_arg(args,int);

	priv->bitnum=-1;	/* Init to "wait for sync" */
	priv->lastsamp=2;	/* Make sure we init */
	priv->syncbitcnt=0;
	priv->bits=0;
	priv->have_stuffed=0;

	return 0;
}
