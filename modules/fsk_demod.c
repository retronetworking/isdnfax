/* $Id$
******************************************************************************

   Fax program for ISDN.
   FSK demodulator. Takes signed 16 bit data and demodulates it to a 
   stream of bytes containing 0/1,conf pairs.
   conf is a "confidence value" ranging from 0-100% giving you an idea 
   about how sure the demodulator is about the result. Extended periods 
   of low confidence usually indicate carrier loss. Short periods are 
   normal when the transmitted symbol changes. This behaviour can be used
   to re-lock PLLs.
   
   We do a s16->alaw conversion here to allow for table lookup. 
   This is suboptimal as we then do alaw->s16->alaw.

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
#include <ifax/alaw.h>

/* Turn on to generate a big bunch of debugging code.
 */
#undef BIGDEBUG

/* Data structures for the fourier analysis.
 */
typedef struct four_help {

	int depth,		/* width of the fourier window */
	    depthsquare,	/* square of it. Used for normalizing */
	    currpos;		/* sliding pointer into the history */
	int currreal,currimag;	/* The current real and imaginary parts of
				   the fourier transform. */
	char *histreal,*histimag;/* History over the window to be able to 
				    remove the coefficients when they
				    slide out of the window */
	int stpos,ctpos,stdepth;/* Offsets for the current sine-position, 
				   the cosine position (shifted by pi/4) 
				   and the depth of the table. */
	char *fasttable;	/* Lookup table for the multiplication 
				   in the DFT. Directly gives 
				   sin(omega*t)*i(t) */

} four_help;

/* private data of a fsk_demod module.
 */
typedef struct {

	four_help	freq1,freq2;	/* fourier data for both channels  */
	int		f1,f2;		/* frequencies for 1 and 0         */
	int		baud;		/* baudrate - gives fourier window */
	int 		sps;		/* samples per second              */

} fskdemod_private;

/* Calculate the greatest common divisor. This is used to see when a waveform
 * repeats. This optimizes the length of the waveform table.
 * Euclidian algorithm.
 */
int gcd(int d1,int d2)
{
	while(d1!=d2)
	{
		if (d1>d2) d1-=d2;
		else       d2-=d1;
	}
	ifax_dprintf(DEBUG_INFO,"GCD is %d\n",d1);
	return d1;
}

/* The following do the DFT "on the fly" with a sliding window.
 */

/* Set up all the tables.
 */
static void init_four_help(fskdemod_private *priv,four_help *hlp,int depth,int freq)
{
	int x,y;
	char *ftptr;
	double sinval;
	
	hlp->depth=depth;	/* window size */
	hlp->depthsquare=depth*depth;	/* normalizing helper variable */
	hlp->currpos=0;		/* window initial position. */
	hlp->currreal=hlp->currimag=0;	/* there is no energy in a zero window */

	/* Allocate the history windows and erase them. */
	hlp->histreal=malloc(sizeof(hlp->histreal[0])*depth);
	hlp->histimag=malloc(sizeof(hlp->histimag[0])*depth);
	while(depth--) 
		hlp->histreal[depth]=
		hlp->histimag[depth]=0;

	/* Calculate size of the sine lookup tables. At worst this is
	   sps, if they have no common divisor (we have no fractional
	   rates, so after 1 second we must have a match again). 
	   As this is pretty large like 8000sps*256values=2MB, we try to
	   optimize by finding the GCD which gives the fraction of seconds
	   after which the frequency pattern repeats at the samplerate.
	 */
	x=gcd(freq,priv->sps);x=priv->sps/x;

	/* We derive the cosine part by shifting by PI/4. If that doesn't
	 * compute, we have a slight error. Warn about that. All V.21
	 * signals at 8000 or 7200 sps will not trigger that case.
	 */
	if (x%4) {
		fprintf(stderr,"fsk_demod: ERROR: sinetable size not divideable by 4.\n"
			"Cosine shift is slighty incorrect.\n");
	}
	/* The table is 256 entries per sample */
	hlp->stdepth=256*x;
	hlp->stpos=0;
	hlp->ctpos=256*(x/4); /* The bracket _IS_ important. Do not optimize. */

	/* Allocate and fill the table. The table contains sin(omega*t)*i(t)
	 * and is accessed by t*256+i(t), where i(t) is assumed to be alaw-
	 * encoded for better dynamics. Any encoding would do. Just make sure
	 * you choose the same encoding in add_samp.
	 */
	hlp->fasttable=malloc(sizeof(hlp->fasttable[0])*hlp->stdepth);
	ftptr=hlp->fasttable;
	for(x=0;x<hlp->stdepth/256;x++)
	{
		sinval=sin(x*2.0*M_PI*(double)freq/(double)priv->sps);
		for(y=0;y<256;y++)
			*ftptr++=((int)(sinval*532610.0*wala2sint[y])) >> 
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

/* Slide the DFT window by one sample.
 */
inline void add_samp(four_help *hlp,ifax_sint16 sample)
{
	unsigned char mysample;
	
	/* convert sample to alaw for table lookup.
	 */
	mysample=sint2wala[((unsigned)sample>>4)&0xFFF];

	/* remove old sample that slides out of the window.
	 * We could optimize a little further here by directly storing
	 * the power instead of the individual values, but I prefer to
	 * keep the information. We could easily modify this system to
	 * demodulate a PSK stream this way.
	 */
	hlp->currreal-=hlp->histreal[hlp->currpos];
	hlp->currimag-=hlp->histimag[hlp->currpos];

	/* Add and store new samples using the lookup-tables. */
	hlp->currreal+=
		(hlp->histreal[hlp->currpos]=hlp->fasttable[hlp->stpos+(unsigned char)mysample]);
	hlp->currimag+=
		(hlp->histimag[hlp->currpos]=hlp->fasttable[hlp->ctpos+(unsigned char)mysample]);

	/* Slide on the table pointers. */
	hlp->currpos++ ;if (hlp->currpos>=hlp->depth  ) hlp->currpos=0;
	hlp->stpos+=256;if (hlp->stpos  >=hlp->stdepth) hlp->stpos=0;
	hlp->ctpos+=256;if (hlp->ctpos  >=hlp->stdepth) hlp->ctpos=0;
}

/* Destroy the private data.
 */
void	fskdemod_destroy(ifax_modp self)
{
	fskdemod_private *priv=(fskdemod_private *)self->private;

	destroy_four_help(&priv->freq1);
	destroy_four_help(&priv->freq2);
	free(self->private);

	return;
}

/* Module has no commands.
 */
int	fskdemod_command(ifax_modp self,int cmd,va_list cmds)
{
	return 0;	/* Not yet used. */
}

/* Handler - go through the data, slide it into the DFT, give back a 0/1
 * decision and a confidence value.
 */
int	fskdemod_handle(ifax_modp self, void *data, size_t length)
{
	unsigned char dat[2];
	ifax_sint16 *input=data;
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

/* Constructor. Arguments:
 * int samplerate;	- rate at which samples come in
 * int f1,f2;		- frequencies that encode 1 and 0
 * int baud;		- the baudrate of the transmission. Influences DFT window.
 */
int	fskdemod_construct(ifax_modp self,va_list args)
{
	fskdemod_private *priv;
	int sampbaud;
	
	if (NULL==(priv=self->private=malloc(sizeof(fskdemod_private)))) 
		return 1;
	self->destroy		=fskdemod_destroy;
	self->handle_input	=fskdemod_handle;
	self->command		=fskdemod_command;

	priv->sps =va_arg(args,int);
	priv->f1  =va_arg(args,int);
	priv->f2  =va_arg(args,int);
	priv->baud=va_arg(args,int);

	sampbaud=(priv->sps+priv->baud)/priv->baud;

	init_four_help(priv,&priv->freq1,sampbaud,priv->f1);
	init_four_help(priv,&priv->freq2,sampbaud,priv->f2);

	return 0;
}
