/* $Id$
******************************************************************************

   Fax program for ISDN.
   Pulse generator. Takes a dummy input stream and generates a pulsed 
   output of 0/1.

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
#include <ifax/ifax.h>

typedef struct {

	int on,off;
	int counter,state;

} pulsegen_private;

/* Free the private data
 */
void	pulsegen_destroy(ifax_modp self)
{
	free(self->private);
	return;
}

int	pulsegen_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	pulsegen_handle(ifax_modp self, void *data, size_t length)
{
	unsigned char dat;
	int handled=0;
	
	pulsegen_private *priv=(pulsegen_private *)self->private;

	while(length--) {

		dat= priv->state ? 255 : 0;
		
		if (--priv->counter==0) { 
			priv->state = !priv->state;
			priv->counter = priv->state ? priv->on : priv->off ;
		}
		
		ifax_handle_input(self->sendto,&dat,1);
		handled++;
	}
	return handled;
}

int	pulsegen_construct(ifax_modp self,va_list args)
{
	pulsegen_private *priv;
	if (NULL==(priv=self->private=malloc(sizeof(pulsegen_private)))) 
		return 1;
	self->destroy		=pulsegen_destroy;
	self->handle_input	=pulsegen_handle;
	self->command		=pulsegen_command;

	priv->on =va_arg(args,int);
	priv->off=va_arg(args,int);
	priv->counter=priv->on;	/* Start in the "on" state */
	priv->state=1;

	return 0;
}
