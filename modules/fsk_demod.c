/* $Id$
******************************************************************************

   Fax program for ISDN.
   FSK demodulator. Takes alaw data and demodulates it to a stream of
   0/1,conf pairs.

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

typedef struct four_help {

	int depth,depthsquare,currpos;
	int currreal,currimag;
	char *histreal,*histimag;
	int stpos,ctpos,stdepth;
	char *fasttable;

} four_help;


typedef struct {

	four_help	freq1,freq2;
	int		f1,f2;
	int		baud;

} fskdemod_private;

int gcd(int d1,int d2)
{
	/* FIXME ! Is this correct ? Pretty long ago that I did that last time ... */
	while(d1!=d2)
	{
		if (d1>d2) d1-=d2;
		else       d2-=d1;
	}
	dprintf(DEBUG_INFO,"GCD is %d\n",d1);
	return d1;
}

/* The following do the DFT "on the fly" with a sliding window.
 */

static void init_four_help(four_help *hlp,int depth,int freq)
{
	int x,y;
	char *ftptr;
	double sinval;
	
	hlp->depth=depth;
	hlp->depthsquare=depth*depth;
	hlp->currpos=0;
	hlp->currreal=hlp->currimag=0;
	hlp->histreal=malloc(sizeof(hlp->histreal[0])*depth);
	hlp->histimag=malloc(sizeof(hlp->histimag[0])*depth);
	while(depth--) 
		hlp->histreal[depth]=
		hlp->histimag[depth]=0;

	x=gcd(freq,SAMPLES_PER_SECOND);x=SAMPLES_PER_SECOND/x;
	if (x%4) {
		fprintf(stderr,"fsk_demod: ERROR: sinetable size not divideable by 4.\n"
			"Cosine shift is slighty incorrect.\n");
	}
	hlp->stdepth=256*x;
	hlp->stpos=0;
	hlp->ctpos=256*(x/4); /* The bracket _IS_ important. Do not optimize. */

	hlp->fasttable=malloc(sizeof(hlp->fasttable[0])*hlp->stdepth);
	ftptr=hlp->fasttable;
	for(x=0;x<hlp->stdepth/256;x++)
	{
		sinval=sin(x*2.0*M_PI*(double)freq/(double)SAMPLES_PER_SECOND);
		for(y=0;y<256;y++)
			*ftptr++=((int)(sinval*alaw2int(y))) >> 
				 ((sizeof(int)-sizeof(char))*8);
	}
}

/* Free the private data
 */
static void destroy_four_help(four_help *hlp)
{
	free(hlp->histreal); hlp->histreal=NULL;
	free(hlp->histimag); hlp->histimag=NULL;
	free(hlp->fasttable);hlp->fasttable=NULL;
}

inline void add_samp(four_help *hlp,char sample)
{
	hlp->currreal-=hlp->histreal[hlp->currpos];
	hlp->currimag-=hlp->histimag[hlp->currpos];

	hlp->currreal+=
		(hlp->histreal[hlp->currpos]=hlp->fasttable[hlp->stpos+(unsigned char)sample]);
	hlp->currimag+=
		(hlp->histimag[hlp->currpos]=hlp->fasttable[hlp->ctpos+(unsigned char)sample]);

	hlp->currpos++ ;if (hlp->currpos>=hlp->depth  ) hlp->currpos=0;
	hlp->stpos+=256;if (hlp->stpos  >=hlp->stdepth) hlp->stpos=0;
	hlp->ctpos+=256;if (hlp->ctpos  >=hlp->stdepth) hlp->ctpos=0;
}

void	fskdemod_destroy(ifax_modp self)
{
	fskdemod_private *priv=(fskdemod_private *)self->private;

	destroy_four_help(&priv->freq1);
	destroy_four_help(&priv->freq2);
	free(self->private);

	return;
}

int	fskdemod_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

int	fskdemod_handle(ifax_modp self, void *data, size_t length)
{
	unsigned char dat[2];
	char *input=data;
	int a1,a2,conf,pwr;
	int handled=0;
	
	fskdemod_private *priv=(fskdemod_private *)self->private;

	while(length--) {

		add_samp(&priv->freq1,*input);
		add_samp(&priv->freq2,*input);
		input++;

		a1=priv->freq1.currreal*priv->freq1.currreal+
		   priv->freq1.currimag*priv->freq1.currimag;
		a2=priv->freq2.currreal*priv->freq2.currreal+
		   priv->freq2.currimag*priv->freq2.currimag;
#ifdef BIGDEBUG
		printf("Power: %d: %8d, %d: %8d",
			priv->f1,a1,priv->f2,a2);
#endif
		if (!a2) a2=1;if (!a1) a1=1;
		if (a1>a2) { conf=100-100*a2/a1; pwr=a1/priv->freq1.depthsquare; }
		else       { conf=100-100*a1/a2; pwr=a2/priv->freq2.depthsquare; }
#ifdef BIGDEBUG
			printf(" dec: %d conf=%d pwr=%d\n", 
				a1 > a2 ? 1 : 0,
				conf,
				pwr );
#endif
                                		
		dat[0]= a1 > a2 ? 1 : 0;
		dat[1]= pwr<10 ? 0 : conf;	/* Don't trust weak signals. */

		ifax_handle_input(self->sendto,dat,1);
		handled++;
	}
	return handled;
}

int	fskdemod_construct(ifax_modp self,va_list args)
{
	fskdemod_private *priv;
	int sampbaud;
	
	if (NULL==(priv=self->private=malloc(sizeof(fskdemod_private)))) 
		return 1;
	self->destroy		=fskdemod_destroy;
	self->handle_input	=fskdemod_handle;
	self->command		=fskdemod_command;

	priv->f1  =va_arg(args,int);
	priv->f2  =va_arg(args,int);
	priv->baud=va_arg(args,int);

	sampbaud=(SAMPLES_PER_SECOND+priv->baud)/priv->baud;

	init_four_help(&priv->freq1,sampbaud,priv->f1);
	init_four_help(&priv->freq2,sampbaud,priv->f2);

	return 0;
}
