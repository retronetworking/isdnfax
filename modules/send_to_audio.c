/* $Id$
******************************************************************************

   Fax program for ISDN.
   Send the received data to a write()-compatible function.

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

typedef int (*auw)(int handle,void *ptr,size_t length);

typedef struct {

	auw audio_write;
	int handle;

} send_to_audio_private;

/* Free the private data
 */
void	send_to_audio_destroy(ifax_modp self)
{
	free(self->private);
	return;
}

int	send_to_audio_handle(ifax_modp self, void *data, size_t length)
{
	send_to_audio_private *priv=(send_to_audio_private *)self->private;
	return priv->audio_write(priv->handle,data,length);
}

int	send_to_audio_construct(ifax_modp self,va_list args)
{
	send_to_audio_private *priv;
	if (NULL==(priv=self->private=malloc(sizeof(send_to_audio_private)))) 
		return 1;
	self->destroy		=send_to_audio_destroy;
	self->handle_input	=send_to_audio_handle;
	self->command		=NULL;

	priv->audio_write=va_arg(args,auw);
	priv->handle     =va_arg(args,int);
	return 0;
}
