/* $Id$
******************************************************************************

   Fax program for ISDN.
   FSK modulator. This is a pretty simple one. It takes a stream of 0/1
   and generates an FSK modulated signal.
   BUGBUG: The output encoding isn't well defined. It is configured in the
   source at the moment ...

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

/* Turn on to generate a big bunch of debugging code.
 */
#undef BIGDEBUG

typedef struct {

	int		f1,f2;
	double		p1,p2;
	double 		currphase;

} fskmod_private;

/* Free the private data
 */
void	fskmod_destroy(ifax_modp self)
{
	fskmod_private *priv=(fskmod_private *)self->private;

	free(self->private);

	return;
}

int	fskmod_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	fskmod_handle(ifax_modp self, void *data, size_t length)
{
	unsigned char dat;
	char *input=data;
	int handled=0;
	
	fskmod_private *priv=(fskmod_private *)self->private;

	while(length--) {

		priv->currphase+= *input++ ? priv->p1 : priv->p2 ;
		dat=int2alaw((int)(0.62*2147483647.0*sin(priv->currphase)));
//		dat=linear2ulaw((short)(25000.0*sin(priv->currphase)));
#if 0
		ifax_dprintf(DEBUG_DEBUG,"Phases: %8.5f %11d %5d %11d %11d\n",sin(priv->currphase),
			(int)(2147483647.0*sin(priv->currphase)),
			dat, alaw2int(dat), 
			(int)(2147483647.0*sin(priv->currphase))-alaw2int(dat));
#endif
		ifax_handle_input(self->sendto,&dat,1);
		handled++;
	}
	return handled;
}

int	fskmod_construct(ifax_modp self,va_list args)
{
	fskmod_private *priv;
	
	if (NULL==(priv=self->private=malloc(sizeof(fskmod_private)))) 
		return 1;
	self->destroy		=fskmod_destroy;
	self->handle_input	=fskmod_handle;
	self->command		=fskmod_command;

	priv->f1  =va_arg(args,int);
	priv->f2  =va_arg(args,int);
	priv->p1  =(double)2.0*M_PI * (double)priv->f1 / SAMPLES_PER_SECOND ;
	priv->p2  =(double)2.0*M_PI * (double)priv->f2 / SAMPLES_PER_SECOND ;
	priv->currphase=0.0;

	ifax_dprintf(DEBUG_DEBUG,"Phases : %f %f\n",priv->p1,priv->p2);

	return 0;
}
