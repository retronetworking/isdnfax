/* $Id$
******************************************************************************

   Fax program for ISDN.
   Scrambler.

   Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
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

#include <ifax/modules/scrambler17.h>

#define MAXBUFFERING 256

typedef struct {

  unsigned int state;
  unsigned char buffer[MAXBUFFERING];

} scrambler_private;

/* Free the private data
 */
void scrambler_destroy(ifax_modp self)
{
  free(self->private);
  return;
}

int scrambler_command(ifax_modp self,int cmd,va_list cmds)
{
  return 0;
}

int scrambler_handle(ifax_modp self, void *data, size_t length)
{
  scrambler_private *priv=(scrambler_private *)self->private;
  size_t chunk, remaining=length;

  while ( remaining > 0 ) {
    chunk = remaining;
    if ( chunk > MAXBUFFERING )
      chunk = MAXBUFFERING;
    scramble17(data,priv->buffer,&priv->state,chunk);
    ifax_handle_input(self->sendto,priv->buffer,chunk);
    remaining -= chunk;
  }

  return length;
}

int scrambler_construct(ifax_modp self,va_list args)
{
  scrambler_private *priv;
  if (NULL==(priv=self->private=malloc(sizeof(scrambler_private)))) 
    return 1;
  self->destroy		=scrambler_destroy;
  self->handle_input	=scrambler_handle;
  self->command		=scrambler_command;

  priv->state=0;

  return 0;
}
