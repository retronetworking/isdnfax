/* $Id$
******************************************************************************

   Fax program for ISDN.
   Line driver interface for ISDN/soundcard and timing/sequencing control.

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

/* NOTE: This code deals with timing-critical device-driver
 *       buffering with synchronized read/write on same
 *       device and between devices (ISDN/Audio).
 *       The relationships between buffer-sizes, read/write
 *       strategy and possible timing glitches should be
 *       investigated more closely.
 *
 * BUGS: Support for ISDN line-driving is not implemented yet.
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
#include <ifax/modules/linedriver.h>

/* These defines should probably be run-time configurable.
 *
 *   BUFFERSIZE is the size of the two main input/output buffers
 *   holding all the samples we may get from the line or from the
 *   modulators.  It must be large enough to hold all samples that
 *   may arrive in one signal-chain computation.
 *
 *   ATOMICSAMPLES is the number of samples we transmit/receive
 *   before we call the signal chains to fill/drain the buffers.
 *   The smaller this value, the shorter delay (but less efficient).
 */

#define BUFFERSIZE      256
#define ATOMICSAMPLES    16

   typedef struct dummy{                          /* header for *.WAV-Files         */ 
                                             /* format, 1st left, 2nd right    */ 
      char              main_chunk[ 4 ];     /* 'RIFF'                         */ 
      unsigned long     length;              /* length of file                 */ 
      char              chunk_type[ 4 ];     /* 'WAVE'                         */ 
      char              sub_chunk[ 4 ];      /* 'fmt'                          */ 
      unsigned long     length_chunk;        /* length sub_chunk,  
                                             always 16 bytes                */ 
      unsigned short    format;              /* always 1 = PCM-Code            */ 
      unsigned short    channels;            /* 1 = Mono, 2 = Stereo           */ 
      unsigned long     ft;                  /* Sample Freq                    */ 
      unsigned long     bytes_per_second;    /* Data per sec                   */ 
      unsigned short    bytes_per_sample;    /* bytes per sample, 
                                             1=8 bit, 2=16 bit (mono) 
                                             2=8 bit, 4=16 bit (stereo)     */ 
      unsigned short    bits_per_sample;     /* bits per sample, 8, 12, 16     */ 
      char              data_chunk[ 4 ];     /* 'data'                         */ 
      unsigned long     data_length;         /* length of data                 */ 
   
   } wav_header; 

   typedef struct {
   
      int isdn_fd, dsp_fd;
      FILE * file_fd;
      ifax_sint32 dsp_rx_volume, dsp_tx_volume;
      
      ifax_sint16 tx_buffer[ATOMICSAMPLES];
      ifax_sint16 rx_buffer[ATOMICSAMPLES];
      ifax_sint16 dsp_buffer[2*ATOMICSAMPLES];
      
      struct queue {
         int wp, rp, size;
         ifax_sint16 buffer[BUFFERSIZE];
      } input, output;
      
      wav_header wh;
   
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
   
      //  printf("Got:  %d\n",length);
   
      while ( remaining > 0 ) {
         priv->output.buffer[priv->output.wp++] = *src++;
         priv->output.size++;
         if ( priv->output.wp >= BUFFERSIZE )
            priv->output.wp = 0;
         remaining--;
      }
   
      return length;
   }


   static int work(ifax_modp self)
   {
      int wanted, t;
      ifax_sint32 sp;
      ifax_uint32 up;
      linedriver_private *priv = self->private;
      short p;
   
   /* Fill output queue by demanding data */
      while ( priv->output.size < ATOMICSAMPLES ) {
         wanted = ATOMICSAMPLES - priv->output.size + 5;
         /*printf("Want: %d\n",wanted);*/
         ifax_handle_demand(self->recvfrom,wanted);
      }
   
   /* Prepare TX-buffer */
      priv->output.size -= ATOMICSAMPLES;
      for ( t=0; t < ATOMICSAMPLES; t++ ) {
         priv->tx_buffer[t] = priv->output.buffer[priv->output.rp++];
         if ( priv->output.rp >= BUFFERSIZE )
            priv->output.rp = 0;
      }
   
      if ( priv->isdn_fd >= 0 ) {
      /* ISDN is online, send TX-buffer, receive into RX-buffer */
      } 
      else {
         for ( t=0; t < ATOMICSAMPLES; t++ ) {
            priv->rx_buffer[t] = 0;
         }
      }
   
      if ( priv->dsp_fd >= 0 ) {
      /* Soundcard audio monitoring enabled */
         for ( t=0; t < ATOMICSAMPLES; t++ ) {
         
            sp = priv->tx_buffer[t] * priv->dsp_tx_volume;
            up = sp;
            up >>= 16;
            priv->dsp_buffer[2*t+0] = up;
         
            sp = priv->rx_buffer[t] * priv->dsp_rx_volume;
            up = sp;
            up >>= 16;
            priv->dsp_buffer[2*t+1] = up;
         }
      
         write(priv->dsp_fd,priv->dsp_buffer,4*ATOMICSAMPLES);
      }
   
      if ( priv->file_fd) {
      /* wave file monitoring enabled */
         for ( t=0; t < ATOMICSAMPLES; t++ ) {
            p = (short)priv->tx_buffer[t];
            fwrite(&p, sizeof(p),1,priv->file_fd);
         }
      }
   
      if(self->sendto){
         ifax_handle_input(self->sendto,&priv->tx_buffer[0],ATOMICSAMPLES);
      }
   
      return ATOMICSAMPLES;
   }


   static void linedriver_destroy(ifax_modp self)
   {
      int pos;
      linedriver_private *priv = self->private;
   
      if(priv->file_fd){
         pos = ftell(priv->file_fd);
         priv->wh.length 					= pos + 44;
         priv->wh.channels 				= 1;
         priv->wh.ft 						= 8000;
         priv->wh.bytes_per_second 		= 16000;
         priv->wh.bytes_per_sample 		= 2;
         priv->wh.bits_per_sample 		= 16;
         priv->wh.data_length 			= pos;
         rewind(priv->file_fd);
         fwrite(&priv->wh, sizeof( wav_header ), 1, priv->file_fd);   
         fclose(priv->file_fd);
      }
      free(self->private);
   }

   static int init_audio()
   {
      int rate = 8000;
      int stereo = 1;
      int format = AFMT_S16_LE;
      int smplsize = 16;
      int dspfd, bufsize, err = 0;
   
      dspfd = open("/dev/dsp",O_WRONLY);
      err |= ioctl(dspfd, SNDCTL_DSP_GETBLKSIZE, &bufsize);
      if ( bufsize < ATOMICSAMPLES )
         exit(54);
      err |= ioctl(dspfd, SNDCTL_DSP_SPEED,&rate);
      err |= ioctl(dspfd, SNDCTL_DSP_STEREO, &stereo);
      err |= ioctl(dspfd, SNDCTL_DSP_SAMPLESIZE,&smplsize);
      err |= ioctl(dspfd, SNDCTL_DSP_SETFMT, &format);
   
      if ( err )
         exit(3);
   
      /*printf("dsp_fd=%d   err=%d\n",dspfd,err);*/
   
      return dspfd;
   }

   FILE * init_file(ifax_modp self)
   {
      linedriver_private *priv = self->private;
   
   
      priv->file_fd = fopen("test.wav","wb");
   
      priv->wh.length 					= 0;
      priv->wh.channels 				= 1;
      priv->wh.ft 						= 8000;
      priv->wh.bytes_per_second 		= 16000;
      priv->wh.bytes_per_sample 		= 2;
      priv->wh.bits_per_sample 		= 16;
      priv->wh.data_length 			= 0;
   
      priv->wh.main_chunk[ 0 ]  = 'R'; 
      priv->wh.main_chunk[ 1 ]  = 'I'; 
      priv->wh.main_chunk[ 2 ]  = 'F'; 
      priv->wh.main_chunk[ 3 ]  = 'F'; 
      priv->wh.chunk_type[ 0 ]  = 'W'; 
      priv->wh.chunk_type[ 1 ]  = 'A'; 
      priv->wh.chunk_type[ 2 ]  = 'V'; 
      priv->wh.chunk_type[ 3 ]  = 'E'; 
      priv->wh.sub_chunk[ 0 ]  = 'f'; 
      priv->wh.sub_chunk[ 1 ]  = 'm'; 
      priv->wh.sub_chunk[ 2 ]  = 't'; 
      priv->wh.sub_chunk[ 3 ]  = 0x20; 
      priv->wh.length_chunk  	 = 0x10; 
      priv->wh.format  = 1; 
      priv->wh.data_chunk[ 0 ]  = 'd'; 
      priv->wh.data_chunk[ 1 ]  = 'a'; 
      priv->wh.data_chunk[ 2 ]  = 't'; 
      priv->wh.data_chunk[ 3 ]  = 'a'; 
      priv->wh.length  = priv->wh.data_length + sizeof( wav_header ); 
   
      fwrite(&priv->wh, sizeof( wav_header ), 1, priv->file_fd);   
   
      return priv->file_fd;
   }


   static int linedriver_command(ifax_modp self, int cmd, va_list cmds)
   {
      linedriver_private *priv = self->private;
   
      switch ( cmd ) {
      
         case CMD_LINEDRIVER_WORK:
            return work(self);
            break;
      
         case CMD_LINEDRIVER_AUDIO:
            priv->dsp_fd = init_audio();
            break;
      
         case CMD_LINEDRIVER_FILE:
            priv->file_fd = init_file(self);
            break;
      
         case CMD_LINEDRIVER_CLOSE:
            linedriver_destroy(self);
            break;
      
         default:
            return 1;
      }
   
      return 0;
   }

   int linedriver_construct(ifax_modp self,va_list args)
   {
      linedriver_private *priv;
   
      if ( (priv = self->private = malloc(sizeof(linedriver_private))) == 0 )
         return 1;
   
      self->destroy = linedriver_destroy;
      self->handle_input = linedriver_handle;
      self->command = linedriver_command;
   
      priv->input.wp = 0;
      priv->input.rp = 0;
      priv->input.size = 0;
   
      priv->output.wp = 0;
      priv->output.rp = 0;
      priv->output.size = 0;
   
      priv->isdn_fd = -1;
      priv->dsp_fd = -1;
      priv->dsp_rx_volume = 0x8000;
      priv->dsp_tx_volume = 0x6000;
   
      priv->file_fd = 0;
   
      return 0;
   }
