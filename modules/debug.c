/* $Id$
******************************************************************************

   Fax program for ISDN.
   Debugging module. Take input stream and print it out to stderr.
   Then pass it on.

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
#include <stdio.h>
#include <stdarg.h>
#include <ifax/ifax.h>

typedef struct {

	/* How many bytes are there per "len" unit ? */
	int bytes_per_sample;

} debug_private;

/* Free the private data
 */
void	debug_destroy(ifax_modp self)
{
	free(self->private);
	return;
}

int	debug_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	debug_handle(ifax_modp self, void *data, size_t length)
{
	unsigned char *dat=data;
	int handled=0;
	int x;
	
	debug_private *priv=(debug_private *)self->private;

	while(handled<length) {

		for (x=0;x<priv->bytes_per_sample;x++,dat++)
			fprintf(stderr,"%2x(%c) ",*dat,*dat);

		fprintf(stderr,"\n");
		fflush(stderr);
				
		handled++;
	}
	if (self->sendto)
		ifax_handle_input(self->sendto,data,length);
	return handled;
}

int	debug_construct(ifax_modp self,va_list args)
{
	debug_private *priv;
	if (NULL==(priv=self->private=malloc(sizeof(debug_private)))) 
		return 1;
	self->destroy		=debug_destroy;
	self->handle_input	=debug_handle;
	self->command		=debug_command;

	priv->bytes_per_sample=va_arg(args,int);

	return 0;
}
