/* $Id$
******************************************************************************

   Fax program for ISDN.
   Sine generator. Create an aLaw encoded sine pattern.

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

typedef struct {

	double step;
	double currphase;

} sinegen_private;

void	sinegen_destroy(ifax_modp self)
{
	free(self->private);
	return;
}

int	sinegen_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	sinegen_handle(ifax_modp self, void *data, size_t length)
{
	unsigned char *dat=data;
	unsigned char out;
	
	int handled=0;
	
	sinegen_private *priv=(sinegen_private *)self->private;

	while(length--) {

		out=int2alaw(2147483647*(*dat/255)*sin(priv->currphase));
		priv->currphase+=priv->step;
		
		ifax_handle_input(self->sendto,&out,1);
		handled++;
	}
	return handled;
}

int	sinegen_construct(ifax_modp self,va_list args)
{
	sinegen_private *priv;
	int frequency;

	if (NULL==(priv=self->private=malloc(sizeof(sinegen_private)))) 
		return 1;
	self->destroy		=sinegen_destroy;
	self->handle_input	=sinegen_handle;
	self->command		=sinegen_command;

	frequency=va_arg(args,int);
	priv->step=2.0*M_PI*(double)frequency/SAMPLES_PER_SECOND;
	priv->currphase=0.0;

	return 0;
}
