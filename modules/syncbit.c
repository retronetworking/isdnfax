/* $Id$
******************************************************************************

   Fax program for ISDN.

   Bit sychronization. Expects a datastream of alternating 0/1,confidence 
   bytes at samprate.
   Outputs bits as 0/1 bytes at the given sample speed.

   Copyright (C) 1999 Andreas Beck	[becka@ggi-project.org]
  
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

typedef struct {

	int	samprate,baud;

	int 	lastsamp;
	int 	bitnum;
	int 	sampcount;

} syncbit_private;

/* Free the private data
 */
void	syncbit_destroy(ifax_modp self)
{
	/* syncbit_private *priv=(syncbit_private *)self->private; */

	free(self->private);

	return;
}

int	syncbit_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	syncbit_handle(ifax_modp self, void *data, size_t length)
{
	char *dat=data;
	int currbit,currconf;
	int handled;
	char result;
	
	syncbit_private *priv=(syncbit_private *)self->private;

	handled=0;
	while(length--) {

		currbit =*dat++;
		currconf=*dat++;
		priv->sampcount++;
		
		ifax_dprintf(DEBUG_JUNK,"bit in: %d,%d\n",currbit,currconf);

		if (currconf<10) continue;
		
		/* _Very_ simple bit synchronizer */
		if (priv->lastsamp!=currbit) {
			ifax_dprintf(DEBUG_JUNK,"synchronizing due to %d->%d at %d\n",priv->lastsamp,currbit,priv->sampcount);
			priv->lastsamp=currbit;
			priv->sampcount=-priv->samprate/2/priv->baud;
			priv->bitnum=0;
		} else
		if ( priv->sampcount >= priv->bitnum*
					priv->samprate/priv->baud)
		{
			ifax_dprintf(DEBUG_DEBUG,"Bit %d is %d\n",priv->bitnum,currbit);
			result=!!currbit;
			if (self->sendto)
				ifax_handle_input(self->sendto,&result,1);
			ifax_dprintf(DEBUG_DEBUG,"HDLC FLAG\n");
			priv->bitnum++;
		}
		handled++;
	}
	return handled;
}

int	syncbit_construct(ifax_modp self,va_list args)
{
	syncbit_private *priv;
	
	if (NULL==(priv=self->private=malloc(sizeof(syncbit_private)))) 
		return 1;
	self->destroy		=syncbit_destroy;
	self->handle_input	=syncbit_handle;
	self->command		=syncbit_command;

	priv->samprate	=va_arg(args,int);
	priv->baud	=va_arg(args,int);

	priv->bitnum=-1;	/* Init to "wait for sync" */
	priv->lastsamp=2;	/* Make sure we init */

	return 0;
}
