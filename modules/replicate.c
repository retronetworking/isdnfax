/* $Id$
******************************************************************************

   Fax program for ISDN.
   Replicator module. Gives incoming data to all attached modules.

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
#include <stdarg.h>
#include <math.h>
#include <ifax/ifax.h>
#include <ifax/modules/replicate.h>

#define MAXSUB 10

typedef struct {

	int subnum;
	ifax_modp subs[MAXSUB];

} replicate_private;

/* Free the private data
 */
void	replicate_destroy(ifax_modp self)
{
	free(self->private);
	return;
}

int	replicate_command(ifax_modp self,int cmd,va_list cmds)
{
	replicate_private *priv=(replicate_private *)self->private;

	switch(cmd)
	{ case CMD_REPLICATE_ADD:
		if (priv->subnum>=MAXSUB) return -1;
		priv->subs[priv->subnum++]=va_arg(cmds,ifax_modp);
		break;
	  case CMD_REPLICATE_DEL:
	  	/* FIXME ! Search and destroy references */
	  	{};
	}
	return 0;	/* Not yet used. */
}

int	replicate_handle(ifax_modp self, void *data, size_t length)
{
	replicate_private *priv=(replicate_private *)self->private;

	int rc,x;

	rc=0;
	for(x=0;x<priv->subnum;x++)	
		rc=ifax_handle_input(priv->subs[x],data,length);
	if (self->sendto)
		rc=ifax_handle_input(self->sendto,data,length);

	return rc;
}

int	replicate_construct(ifax_modp self,va_list args)
{
	replicate_private *priv;

	if (NULL==(priv=self->private=malloc(sizeof(replicate_private)))) 
		return 1;
	self->destroy		=replicate_destroy;
	self->handle_input	=replicate_handle;
	self->command		=replicate_command;

	priv->subnum=0;

	return 0;
}
