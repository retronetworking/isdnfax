/* $Id$
******************************************************************************

   Fax program for ISDN.
   Line driver interface for ISDN/soundcard and timing/sequencing control.

   Copyright (C) 1998-1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]

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

/* NOTE: This code deals with timing-critical device-driver
 *       buffering with synchronized read/write on same
 *       device and between devices (ISDN/Audio).
 *       The relationships between buffer-sizes, read/write
 *       strategy and possible timing glitches should be
 *       investigated more closely. UPDATE: Should work well
 *       as long as (low-level) transmit-buffer is
 *       pre-charged to avoid draining.
 *
 * BUGS: Error situations are not handeled well (no warnings etc.)
 */

/* Maintain buffers and send/receive samples to the underlying
 * physical transmission medium, like ISDN or a soundcard, named
 * pipes for faxing self (debugging) etc.
 *
 * The module interface is:
 *
 *    Input and output:
 *      - 16-bit signed samples
 *      - length specifies number of samples
 *
 *    Commands supported:
 *       CMD_LINEDRIVER_WORK
 *       CMD_LINEDRIVER_AUDIO
 *       CMD_LINEDRIVER_ISDN,<ih>
 *       CMD_LINEDRIVER_LOOPBACK
 *       CMD_LINEDRIVER_RECORD,<filename>
 *
 *    Parameters:
 *       None
 *
 * NOTE: This module will produce a constant stream of (real-time)
 * samples when online.  When online, the signal chain feeding this
 * linedriver module must be prepared to deliver samples at any time
 * through the 'handle_demand' functionality.  Ie.: If no data is
 * available for shipment, the carrier should still be maintained,
 * or silence should be explicitely sent to maintain proper timing
 * and avoid glitches on the output.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>

#include <ifax/ifax.h>
#include <ifax/types.h>
#include <ifax/misc/malloc.h>
#include <ifax/misc/isdnline.h>
#include <ifax/modules/linedriver.h>

/* These defines should probably be run-time configurable.
 * BUFFERSIZE is the size of the main output buffer which is
 * filled by the handle function (incomming signal-chain).
 * We need a buffer here since the signaling-chain may not generate
 * the exact number of samples requested.
 *
 * MAXIOSIZE is the maximum number of samples we can transmit/receive
 * before we call the signal chains to fill/drain the buffers.
 */

#define BUFFERSIZE      512
#define MAXIOSIZE       256


/* An instance of this linedriver module holds the following
 * state-information and buffer storage.
 */

typedef struct {

  int dsp_fd;                     /* File-descriptor for audio-monitoring */
  int rec_fd;                     /* File-descriptor for recording */
  struct HardwareHandle *hh;	  /* Handle for the phone interface */
  int loopback;                   /* Nonzero if software loopback enabled */
  ifax_sint32 dsp_rx_volume;      /* Audio-monitoring Rx-level */
  ifax_sint32 dsp_tx_volume;      /* Audio-monitoring Tx-level */

  ifax_sint16 tx_buffer[MAXIOSIZE];         /* Transmit buffer */
  ifax_sint16 rx_buffer[MAXIOSIZE];         /* Receive buffer */
  ifax_sint16 stereo_buffer[2*MAXIOSIZE];   /* Monitoring/recording buffer */

  struct queue {                  /* Output queue */
    int wp, rp, size;
    ifax_sint16 buffer[BUFFERSIZE];
  } output;

} linedriver_private;


/* Method of operation:
 *
 * The 'handle' function is called, possibly as a result of a 'demand'
 * request, with data to fill the output queue (TX).  However, no data
 * is actually transmitted until the signal-chain has finished and the
 * CMD_LINEDRIVER_WORK command can act depending on the (new) size of
 * the output-queue.
 *
 * The input-queue is filled and the signal-chain is called.  When the
 * signal chain returns, the number of samples involved is returned,
 * so the timers may be updated.
 */

static int linedriver_handle(ifax_modp self, void *data, size_t length)
{
  linedriver_private *priv = self->private;
  ifax_sint16 *src = data;
  size_t remaining = length;
   
  while ( remaining > 0 ) {
    priv->output.buffer[priv->output.wp++] = *src++;
    priv->output.size++;
    if (priv->output.wp >= BUFFERSIZE)
      priv->output.wp = 0;
    remaining--;
  }
   
  return length;
}

static int work(ifax_modp self)
{
  int wanted, chunk, total, more, t;
  ifax_sint32 sp;
  ifax_uint32 up;
  linedriver_private *priv = self->private;

  /* When driving the ISDN-line, this is how it works:
   * Read as much as we can from the line, and then
   * transmit *exactly* as much.  This will keep the
   * flow of samples smooth.  It may be a very good idea
   * to pre-charge the output-buffers somewhat to be more
   * resistant to buffer-drains during heavy computations.
   */

  total = more = 0;

  do {

    chunk = MAXIOSIZE;      /* Default buffer-size of IO-operations */

    if ( priv->ih != 0 ) {
      /* ISDN is online, read first, then transmit */
      chunk = IsdnReadSamples(priv->ih,&priv->rx_buffer[0],MAXIOSIZE);
      if ( chunk < 0 )
	return -1;
      more = chunk == MAXIOSIZE;
    } else
      for ( t=0; t < chunk; t++)
	priv->rx_buffer[t] = 0;

    /* Fill output queue by demanding data (if needed) */
    while ( priv->output.size < chunk ) {
      wanted = chunk - priv->output.size + 5;
      ifax_handle_demand (self->recvfrom, wanted);
    }

    /* Prepare TX-buffer */
    priv->output.size -= chunk;
    for (t = 0; t < chunk; t++) {
      priv->tx_buffer[t] = priv->output.buffer[priv->output.rp++];
      if ( priv->output.rp >= BUFFERSIZE )
	priv->output.rp = 0;
    }

    if ( priv->ih != 0 ) {
      /* ISDN is online, send TX-buffer */
      IsdnWriteSamples(priv->ih,&priv->tx_buffer[0],chunk);
    }

    if ( priv->loopback ) {
      /* Software loopback enabled, overwrite receive-buffer */
      for ( t=0; t < chunk; t++ )
	priv->rx_buffer[t] = priv->tx_buffer[t];
    }

    if ( priv->dsp_fd >= 0 ) {
      /* Soundcard audio monitoring enabled */
      for ( t=0; t < chunk; t++ ) {
	sp = priv->tx_buffer[t] * priv->dsp_tx_volume;
	up = sp;
	up >>= 16;
	priv->stereo_buffer[t+t+0] = up;
         
	sp = priv->rx_buffer[t] * priv->dsp_rx_volume;
	up = sp;
	up >>= 16;
	priv->stereo_buffer[t+t+1] = up;
      }

      write(priv->dsp_fd, priv->stereo_buffer, 4*chunk);
    }

    if ( priv->rec_fd >= 0 ) {
      /* Recording all audio to file */
      for ( t=0; t < chunk; t++ ) {
	priv->stereo_buffer[t+t+0] = priv->tx_buffer[t];
	priv->stereo_buffer[t+t+1] = priv->rx_buffer[t];
      }

      write(priv->rec_fd, priv->stereo_buffer, 4*chunk);
    }

    if ( self->sendto != 0 ) {
      /* Feed the receiver signaling-chain */
      ifax_handle_input(self->sendto, &priv->rx_buffer[0], chunk);
    }

    total += chunk;

  } while ( more );

  return total;
}

static void linedriver_destroy(ifax_modp self)
{
  free(self->private);
}

static int init_audio(void)
{
  int rate = 8000;
  int stereo = 1;
  int format = AFMT_S16_LE;
  int smplsize = 16;
  int dspfd, bufsize, err = 0;

  dspfd = open ("/dev/dsp", O_WRONLY);

  err |= ioctl (dspfd, SNDCTL_DSP_GETBLKSIZE, &bufsize);
  err |= ioctl (dspfd, SNDCTL_DSP_SPEED, &rate);
  err |= ioctl (dspfd, SNDCTL_DSP_STEREO, &stereo);
  err |= ioctl (dspfd, SNDCTL_DSP_SAMPLESIZE, &smplsize);
  err |= ioctl (dspfd, SNDCTL_DSP_SETFMT, &format);

  if ( dspfd < 0 || err || bufsize < MAXIOSIZE ) {
    if ( dspfd >= 0 )
      close(dspfd);
    dspfd = -1;
  }

  return dspfd;
}

static int start_recording(char *file)
{
  return creat(file,0660);
}

static void stop_recording(int fd)
{
  close(fd);
}

static int linedriver_command(ifax_modp self, int cmd, va_list cmds)
{
  linedriver_private *priv = self->private;
  char *filename;

  switch ( cmd ) {

    case CMD_LINEDRIVER_WORK:
      return work(self);
      break;

    case CMD_LINEDRIVER_AUDIO:
      priv->dsp_fd = init_audio();
      break;

    case CMD_LINEDRIVER_HARDWARE:
      priv->hh = va_arg(cmds,struct HardwareHandle *);
      break;

    case CMD_LINEDRIVER_LOOPBACK:
      priv->loopback = 1;
      break;

    case CMD_LINEDRIVER_RECORD:
      filename = va_arg(cmds,char *);
      if ( filename != 0 ) {
	priv->rec_fd = start_recording(filename);
      } else {
	stop_recording(priv->rec_fd);
	priv->rec_fd = -1;
      }
      break;

    default:
      return 1;
  }

  return 0;
}

int linedriver_construct (ifax_modp self, va_list args)
{
  linedriver_private *priv;

  priv = ifax_malloc(sizeof(linedriver_private),"Linedriver instance");
  self->private = priv;

  self->destroy = linedriver_destroy;
  self->handle_input = linedriver_handle;
  self->command = linedriver_command;

  priv->output.wp = 0;
  priv->output.rp = 0;
  priv->output.size = 0;

  priv->ih = 0;
  priv->loopback = 0;
  priv->dsp_fd = -1;
  priv->rec_fd = -1;
  priv->dsp_rx_volume = 0x8000;
  priv->dsp_tx_volume = 0x6000;

  return 0;
}
