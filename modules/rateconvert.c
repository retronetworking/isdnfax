/* $Id$
******************************************************************************

   Fax program for ISDN.
   Rateconvert a stream of signed 16-bit words by means of interpolation
   and oversampeling and decimation.

   Copyright (C) 1998 Oliver Eichler [Oliver.Eichler@regensburg.netsurf.de]
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

/* The module interface is:
 *
 * Input and Output:
 *   - 16-bit signed samples
 *   - length specifies number of samples
 *
 * Commands supported:
 *   None
 *
 * Parameters are:
 *      int       upsample factor
 *      int       downsample factor
 *      int       size of filter
 *      short *   filter coeficients
 *      int       scale output (0x10000 is unit scale)
 *
 * Filtersize must be an integer multiple of upsample-factor.
 *
 * Expected filter coeficients:
 *      1) First filter coeficient in array gets multiplied with
 *         the *newest* sample in the stream to be filtered.
 *         The filter h = { 0, 0, 32767 } is thus a two sample delay.
 *      2) Filter coeficients are ordered linearly, h[0]...h[n].
 *
 * Range:
 *      For accumulation, 32-bit signed integers are used.  Make
 *      sure your worst possible input data will not overflow
 *      the accumulator.  The "scale" parameter can be used
 *      to trim the output to the desired level.
 *
 * If you want a low-amplitude signal out of your filter, design
 * the filter for full-blown output and use the scale to
 * reduce it, to make full use of the filter coefs resolution.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <ifax/ifax.h>
#include <ifax/types.h>
#include <ifax/misc/malloc.h>
#include <ifax/modules/rateconvert.h>


#define MAXBUFFER 256

typedef struct {

  int filtersize;
  ifax_sint16 *coefs;
  ifax_sint16 *data;
  ifax_sint16 **subfilter;
  int subfiltsize;
  int storenext;
  int upfactor, downfactor;
  unsigned int next_seq;
  unsigned int seq_size;
  ifax_sint32 scale;
  ifax_uint32 rate_factor;      /*  0x10000 * downfactor / upfactor */
  int dry_period;

  struct {
    ifax_sint16 *startcoef;
    int count;
  } *seq;

  ifax_sint16 buffer[MAXBUFFER];

} rateconvert_private;



static int rateconvert_handle(ifax_modp self, void *data, size_t length)
{
  rateconvert_private *priv = self->private;
  ifax_sint16 *idp = data, *dp, *coef, temp;
  int bp, t, p, k, count;
  ifax_sint32 sum;

  bp = 0;
  for ( t=0; t < length; t++ ) {

    priv->data[priv->storenext++] = *idp++;
    if ( priv->storenext >= priv->subfiltsize )
      priv->storenext = 0;

    coef  = priv->seq[priv->next_seq].startcoef;
    count = priv->seq[priv->next_seq].count;
    priv->next_seq++;

    if ( priv->next_seq >= priv->seq_size )
      priv->next_seq = 0;

    for ( p=0; p < count; p++ ) {

      sum = 0;

      dp = &priv->data[priv->storenext];
      k = priv->subfiltsize - priv->storenext;
      while ( k-- )
	sum += (*dp++) * (*coef++);

      dp = &priv->data[0];
      k = priv->storenext;
      while ( k-- )
	sum += (*dp++) * (*coef++);

      temp = sum >> 15;
      sum = temp * priv->scale;
      temp = sum >> 16;
      
      priv->buffer[bp++] = temp;

      if ( bp >= MAXBUFFER ) {
	ifax_handle_input(self->sendto,priv->buffer,MAXBUFFER);
	bp = 0;
      }
    }
  }

  if ( bp > 0 )
    ifax_handle_input(self->sendto,priv->buffer,bp);

  return length;
}

static void rateconvert_destroy(ifax_modp self)
{
  rateconvert_private *priv=(rateconvert_private *)self->private;

  if ( priv->coefs != 0 )
    free(priv->coefs);

  if ( priv->data != 0 )
    free(priv->data);

  if ( priv->seq != 0 )
    free(priv->seq);

  if ( priv->subfilter != 0 )
    free(priv->subfilter);

  free(self->private);
}

static void rateconvert_demand(ifax_modp self, size_t demand)
{
  rateconvert_private *priv = self->private;
  unsigned int needed;

  needed = demand * priv->rate_factor;
  needed >>= 16;

  if ( needed < priv->dry_period )
    needed = priv->dry_period;

  ifax_handle_demand(self->recvfrom,needed);
}

static int rateconvert_command(ifax_modp self, int cmd, va_list cmds)
{
  return 0;
}


int rateconvert_construct(ifax_modp self, va_list args )
{
  rateconvert_private *priv;
  int t, k, n, decimate;
  ifax_sint16 *filtercoef, *dp, *sp;
  int dry;

  priv = ifax_malloc(sizeof(rateconvert_private),"Rateconverter instance");
  self->private = priv;

  self->destroy = rateconvert_destroy;
  self->handle_input = rateconvert_handle;
  self->handle_demand = rateconvert_demand;
  self->command = rateconvert_command;

  priv->upfactor = va_arg(args,int);
  priv->downfactor = va_arg(args,int);
  priv->filtersize = va_arg(args,int);
  filtercoef = va_arg(args,short *);
  priv->scale = va_arg(args,signed int);

  /* Find size of subfilter, and fail if not proper */
  priv->subfiltsize = priv->filtersize / priv->upfactor;
  if ( (priv->subfiltsize * priv->upfactor) != priv->filtersize )
    return 1;

  priv->data = ifax_malloc(sizeof(*priv->data)*priv->subfiltsize,
			   "Rateconverter sample history");

  priv->coefs = ifax_malloc(sizeof(*priv->coefs)*priv->filtersize,
			    "Rateconverter coeficient storage");

  priv->subfilter = ifax_malloc(sizeof(*priv->subfilter)*priv->upfactor,
				"Rateconverter subfilter pointer array");

  priv->storenext = 0;
  for ( t=0; t < priv->subfiltsize; t++ )
    priv->data[t] = 0;

  /* Copy and reorganize the filter coeficients into local storage.
   * When I=5 and D=3, the subfilters h0,h1,h2,h3,h4 will have the
   * following order in the coefs array:
   *    h0,h3,h1,h4,h2
   * This makes it possible to do several output samples for a
   * single input sample without updating the coeficient pointer.
   * It will just exit h0 and enter (the correct) h3.
   */

  for ( t=0; t < priv->upfactor; t++ )
    priv->subfilter[t] = 0;

  dp = priv->coefs;
  for ( t=0; t < priv->upfactor; t++ ) {
    k = t;
    if ( priv->subfilter[k] == 0 ) {
      while ( k < priv->upfactor ) {
	priv->subfilter[k] = dp;
	sp = &filtercoef[(priv->subfiltsize-1)*priv->upfactor+k];
	for ( n=0; n < priv->subfiltsize; n++ ) {
	  *dp++ = *sp;
	  sp -= priv->upfactor;
	}
	k += priv->downfactor;
      }
    }
  }

  /* Pre-calculate a whole interpolate/decimate rotation, so that
   * for each input sample, we make a table-lookup to see how many
   * output samples we have to generate, and which coef to start
   * with.
   */

  priv->dry_period = 0;
  priv->next_seq = 0;
  priv->seq_size = priv->upfactor * priv->downfactor;
  priv->seq = ifax_malloc(sizeof(*priv->seq)*priv->seq_size,
			  "Rateconverter sequencing lookup table");

  decimate = 0;
  dry = 0;
  for ( t=0; t < priv->seq_size; t++ ) {
      priv->seq[t].count = 0;
      if ( decimate < priv->upfactor ) {
	priv->seq[t].startcoef = priv->subfilter[decimate];
      } else {
	priv->seq[t].startcoef = 0;
      }
      while ( decimate < priv->upfactor ) {
	priv->seq[t].count++;
	decimate += priv->downfactor;
      }
      decimate -= priv->upfactor;

      dry++;
      if ( priv->seq[t].count != 0 ) {
	if ( dry > priv->dry_period )
	  priv->dry_period = dry;
	dry = 0;
      }
  }

  priv->rate_factor = (0x10000 * priv->downfactor) / priv->upfactor;

  return 0;
}
