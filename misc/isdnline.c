/* $Id$
******************************************************************************

   Fax program for ISDN.
   Communicate with the /dev/ttyI* devices to transfer audio over ISDN

   Copyright (C) 1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
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

/* Utility-functions to administrate the ISDN-audio subsystem and
 * general transfering of audio over ISDN.  The /dev/ttyI* devices
 * are used to access the underlying device-drivers.  Audio over
 * the ISDN-ttys must be supported by the low-level ISDN-driver used.
 */

#include <stdio.h>
#include <time.h>
#include <termios.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ifax/ifax.h>
#include <ifax/alaw.h>
#include <ifax/misc/isdnline.h>
#include <ifax/misc/malloc.h>


/* New-line sequence used by the tty-device for AT-command exchanges */
#define LINECHAR1 '\r'
#define LINECHAR2 '\n'
#define CMDLNECHR '\r'


void display_incomming(struct IsdnHandle *);

/* Function to help debugging */

static void format_ascii(char *dst, char *src, int dstsize, int srcsize)
{
  int t;
  char c, tmp[16];

  dst[0] = 0;
  for ( t=0; t < srcsize; t++ ) {
    c = src[t];
    switch ( c ) {
      case '\r':
	strcpy(tmp,"\\r");
	break;
      case '\n':
	strcpy(tmp,"\\n");
	break;
      default:
	if ( c < 0x20 || c > 126 )
	  sprintf(tmp,"\\0%03o",((unsigned int)c)&255);
	else
	  sprintf(tmp,"%c",c);
	break;
    }
    strcat(dst,tmp);
    if ( strlen(dst) > (dstsize-9) ) {  /* 9 chars:  \ 0 3 7 7 . . . \0 */
      if ( t < (dstsize-1) )
	strcat(dst,"...");
      return;
    }
  }
}


/* Helper function to sleep for a short while.  This is needed when
 * the ttyI-driver is opened with nonblocking-IO as the driver needs
 * some time to come up with a response to the AT-commands.
 * Time to sleep in range 1-99 (hundreds of a second).
 */

void IsdnShortSleep(int hundreds)
{
  struct timeval sleep;

  sleep.tv_sec = 0;
  sleep.tv_usec = hundreds * 10000;

  select(0,(fd_set *)0, (fd_set *)0, (fd_set *)0, &sleep);
}


/* Use this function to wait for input on the ISDN-device that is
 * known to arrive - ie. echo and responces from AT-commands.
 * The function will timeout after the specified time has elapsed.
 */

static void IsdnWaitForInput(struct IsdnHandle *ih, int sec, int usec)
{
  fd_set rfds;
  struct timeval timeout;

  FD_ZERO(&rfds);
  FD_SET(ih->fd, &rfds);
  timeout.tv_sec = sec;
  timeout.tv_usec = usec;

  select(ih->fd+1, &rfds, (fd_set *)0, (fd_set *)0, &timeout);
}


/* Helper function to read from a non-blocking device, with EAGAIN
 * filtered out to make life easier.  The return value still should be
 * checked for other errors.
 */

static int nonblock_read(int fd, void *buf, int size)
{
  int retval;

  retval = read(fd,buf,size);

  if ( retval >= 0 ) {
    return retval;
  }

  if ( errno == EAGAIN )
    return 0;

  return -1;
}


/* Copy the input-buffer of the ISDN-device (ring-buffer) into
 * a linear buffer for easy parsing.  This function should be
 * avoided for large volume data, unless each call actually
 * results in equal advances for the buffer.
 */

int IsdnCopyInput(struct IsdnHandle *ih, char *dst, int size)
{
  int retsize = 0;
  int src, chunk;

  if ( ih->recbufsize < size )
    size = ih->recbufsize;

  src = ih->recbufrp;

  if ( ih->recbufrp + size > ISDNRECBUF_SIZE ) {
    chunk = ISDNRECBUF_SIZE - ih->recbufrp;
    memcpy(dst,&ih->recbuf[src],chunk);
    dst += chunk;
    retsize += chunk;
    size -= chunk;
    src = 0;
  }

  memcpy(dst,&ih->recbuf[src],size);
  retsize += size;

  return retsize;
}


/* Read as much as possible from the ISDN-device and queue up in the
 * IsdnHandle structure for later use.  The queue is in raw-format:
 * interpretation of DLE etc. is done later, by the read-functions.
 *
 * This function may flag an error in the IsdnHandle structure.
 */

static void IsdnReadDevice(struct IsdnHandle *ih)
{
  int max, actual;

  if ( ih->error || ih->recbufsize >= ISDNRECBUF_SIZE )
    return;

  if ( ih->recbufwp < ih->recbufrp ) {
    /* One segment to be filled */
    max = ih->recbufrp - ih->recbufwp;
    actual = nonblock_read(ih->fd,&ih->recbuf[ih->recbufwp],max);
    if ( actual < 0 )
      goto readfail;
    ih->recbufwp += actual;
    ih->recbufsize += actual;
  } else {
    /* Possibly two segments to be filled */
    max = ISDNRECBUF_SIZE - ih->recbufwp;
    actual = nonblock_read(ih->fd,&ih->recbuf[ih->recbufwp],max);
    if ( actual < 0 )
      goto readfail;
    ih->recbufwp += actual;
    ih->recbufsize += actual;
    if ( actual == max ) {
      /* We have to wrap and fill second segment too */
      max = ih->recbufrp;
      actual = nonblock_read(ih->fd,&ih->recbuf[0],max);
      if ( actual < 0 )
	goto readfail;
      ih->recbufwp = actual;
      ih->recbufsize += actual;
    }
  }

  return;

 readfail:

  sprintf(ih->errormsg,"Unable to read from %s: %s",
	  ih->devname,strerror(errno));
  ih->error = 1;
  return;
}


/* Send a binary string of bytes directly to the ISDN-audio subsystem.
 * This function is used for all write operations on the device.
 */

static void IsdnSendRaw(struct IsdnHandle *ih, char *cmd, int size)
{
  int retval;

  if ( ih->error )
    return;

  retval = write(ih->fd,cmd,size);

  if ( retval == size )
    return;

  if ( retval < 0 ) {
    perror("Can't write to ISDN-driver");
    exit(1);
  }

  exit(54);
}


/* Flush all received data for the ISDN-device.  This operation can be
 * used to get out of a messy state (like during initialization).
 */

static void IsdnFlushInput(struct IsdnHandle *ih)
{
  ih->recbufrp = ih->recbufwp = ih->recbufsize = 0;

  if ( ih->error )
    return;

  IsdnWaitForInput(ih,0,50000);     /* 0.05 seconds timeout */
  IsdnReadDevice(ih);

  while ( ih->recbufsize > 0 ) {
    ih->recbufrp = ih->recbufwp = ih->recbufsize = 0;
    IsdnWaitForInput(ih,0,20000);
    IsdnReadDevice(ih);
  }

  ih->recbufrp = ih->recbufwp = ih->recbufsize = 0;
}


/* Skip a given number of bytes in the receive buffer, probably
 * because it has already been captured by IsdnCopyInput.
 */

static void IsdnAdvanceInput(struct IsdnHandle *ih, int size)
{
  if ( ih->recbufsize < size ) {
    ih->recbufrp = ih->recbufwp = ih->recbufsize = 0;
    return;
  }

  ih->recbufsize -= size;
  ih->recbufrp += size;
  if ( ih->recbufrp >= ISDNRECBUF_SIZE )
    ih->recbufrp -= ISDNRECBUF_SIZE;
}


/* Read a line into a buffer.  A line is everything up to
 * the terminating CR or CR+LF characters.  NOTE: If the line is too long,
 * it will not be possible to read it, and -1 is returned.  Also
 * note that sleeping up to one second is done to wait for data.
 */

int IsdnGetLine(struct IsdnHandle *ih, char *buf, int size, int devread)
{
  char tmp[ISDNMAXLNE_SIZE+4];
  int t, s, u;

  for ( u=0; u < 5; u++ ) {

    s = IsdnCopyInput(ih,tmp,size+2);

    if ( s > 0 ) {
      tmp[s] = 0;
      /* format_ascii(str,tmp,1000,s);
	 printf("IsdnCopyLine: '%s'\n",str); */
    }

    for ( t=0; t < s; t++ ) {

      if ( tmp[t] == LINECHAR1 && tmp[t+1] == LINECHAR2 ) {
	/* Found CR+LF, return everything up to this point */
	memcpy(buf,tmp,t);
	buf[t] = 0;
	IsdnAdvanceInput(ih,t+2);
	return t;
      }

      if ( tmp[t] == CMDLNECHR ) {
	/* Found single AT-command termination character, use it */
	memcpy(buf,tmp,t);
	buf[t] = 0;
	IsdnAdvanceInput(ih,t+1);
	return t;
      }
    }

    if ( !devread )
      return -1;

    IsdnWaitForInput(ih,0,100000);       /* 0.1 second timeout */
    IsdnReadDevice(ih);
    if ( ih->error )
      return -1;
  }

  return -1;   /* Very long line or no data.  Flush may be in order */
}


/* When a line is received on the ttyI-interface, it may be for two
 * reasons: It may be an echo or response of an AT-command, or it may
 * be an asynchronous signal (RING) from the interface.  The following
 * function tests any line to see if it is a ring-indication.
 * If not, all is well.  If it is, the information is parsed and
 * stored for later action.
 */

static int IsdnIsAsync(struct IsdnHandle *ih, char *buf)
{
  if ( !strcmp(buf,"") ) {
    /* Difficult to say... may be the start of a RING or NO CARRIER */
    goto async;
  }

  if ( !strcmp(buf,"RING") ) {
    /* No doubt... */
    ih->ringing = 1;
    goto async;
  }

  if ( !strncmp(buf,"CALLER NUMBER: ",15) ) {
    /* Record remote-MSN */
    strcpy(ih->remotemsn,&buf[15]);
    goto async;
  }

  if ( !strcmp(buf,"NO CARRIER") ) {
    /* Oops, connection lost */
    ih->mode = TTYIMODE_CMDOFFLINE;
    ih->ringing = 0;
    goto async;
  }

  return 0;

 async:

  /* printf("Async: %s\n",buf); */
  return 1;
}


/* After the ttyI-driver has been initialized, the following function is
 * used to send AT-commands.  The returned echo is checked, return value
 * is read, and other strange responses like 'RING' is taken care
 * of (inbetween commands).
 *
 * This function may flag an error in the IsdnHandle structure.
 */

static void IsdnSendCommand(struct IsdnHandle *ih, char *cmd, int ack, int res)
{
  char command[ISDNMAXLNE_SIZE+4], tmp[4];
  char echo[ISDNMAXLNE_SIZE+4];

  if ( ih->error )
    return;

  strcpy(command,cmd);
  tmp[0] = CMDLNECHR;
  tmp[1] = 0;
  strcat(command,tmp);

  IsdnSendRaw(ih,command,strlen(command));
  if ( ih->error ) {
    printf("Barf...\n");
    return;
  }

  for (;;) {
    IsdnGetLine(ih,echo,ISDNMAXLNE_SIZE,1);
    if ( ih->error ) {
      printf("Barf2...\n");
      return;
    }
    if ( !IsdnIsAsync(ih,echo) )
      break;
  }

  if ( strcmp(cmd,echo) ) {
    sprintf(ih->errormsg,"Command '%s' echoed '",cmd);
    format_ascii(command,echo,ISDNERRMSG_SIZE-strlen(echo)-5,strlen(echo));
    strcat(ih->errormsg,command);
    strcat(ih->errormsg,"'");
    printf("%s\n",ih->errormsg);
    ih->error = 1;
    return;
  }

  ih->cmdresult[0] = 0;
  if ( res ) {
    /* Some commands returns a result - only one line results are supported */
    for (;;) {
      IsdnGetLine(ih,ih->cmdresult,ISDNMAXLNE_SIZE,1);
      if ( !IsdnIsAsync(ih,ih->cmdresult) )
	break;
    }
    printf("Command '%s' gave '%s'\n",cmd,ih->cmdresult);
  }

  if ( ack ) {
    /* This command is expected to give the 'OK' acknowledgement */
    for (;;) {
      IsdnGetLine(ih,echo,ISDNMAXLNE_SIZE,1);
      if ( !IsdnIsAsync(ih,echo) )
	break;
    }
    if ( strcmp(echo,"OK") ) {
      printf("Command '%s' didn't 'OK' (%s)\n",cmd,echo);
      exit(1);
    }

    if ( ! res ) {
      printf("Command '%s' OK\n",cmd);
    }
  }
}


/* Try to initialize the ISDN-device for audio transfer.  The
 * initialization sequence attempts to bootstrap the ttyI* devices
 * from a very low level, to assure a valid initialization even
 * with very strange default settings.
 */

static void IsdnInitialize(struct IsdnHandle *ih, char *msn)
{
  char cmd[64];

  /* Try to initialize S3+S4 to CR+LF ... very paranoid, and will
   * only work if CR or NL is recognized as a command termination
   * character (which it probably will).
   */

  sprintf(cmd,"%cATS3=%d%c",LINECHAR1,LINECHAR1,LINECHAR1);
  IsdnSendRaw(ih,cmd,strlen(cmd));

  sprintf(cmd,"%cATS3=%d%c",LINECHAR2,LINECHAR1,LINECHAR2);
  IsdnSendRaw(ih,cmd,strlen(cmd));

  sprintf(cmd,"%cATS4=%d%c",LINECHAR1,LINECHAR2,LINECHAR1);
  IsdnSendRaw(ih,cmd,strlen(cmd));

  sprintf(cmd,"%cATS4=%d%c",LINECHAR2,LINECHAR2,LINECHAR2);
  IsdnSendRaw(ih,cmd,strlen(cmd));


  /* The new-line sequence should be CR+LF now ... It seems the
   * ISDN4Linux kernel is hardwired to CR+LF, in which case it is
   * not important to initialize S3 and S4, but now it is done...
   */

  sprintf(cmd,"ATE1%c",CMDLNECHR);         /* Enable echo of commands */
  IsdnSendRaw(ih,cmd,strlen(cmd));

  sprintf(cmd,"ATV1%c",CMDLNECHR);         /* English responses */
  IsdnSendRaw(ih,cmd,strlen(cmd));

  IsdnFlushInput(ih);  /* Echo or not, it is probably just rubish anyway */


  /* The AT-command interface should be properly initialized now,
   * continue using the 'IsdnSendCommand' for the rest of the AT-commands.
   */

  /* Let the device identify itself and check if we can use it */
  IsdnSendCommand(ih,"ATI",1,1);
  if ( strcmp(ih->cmdresult,"Linux ISDN") ) {
    sprintf(ih->errormsg,
	    "Device '%s' not supported (Only 'Linux ISDN')",
	    ih->cmdresult);
    ih->error = 1;
    return;
  }

  sprintf(cmd,"AT&E%s",msn);               /* Set phone-number to use */
  IsdnSendCommand(ih,cmd,1,0);

  IsdnSendCommand(ih,"AT+FCLASS=8",1,0);   /* Enable audio */

  IsdnSendCommand(ih,"AT+VSM=5",1,0);      /* A-law encoding */

  IsdnSendCommand(ih,"AT+VLS=2",1,0);      /* Use phone-line */

  IsdnSendCommand(ih,"ATS18=1",1,0);       /* Set audio-service only */

  IsdnSendCommand(ih,"ATS14=4",1,0);       /* Transparent layer 2 (audio) */
}


/* Open the Isdn-device and prepare for audio-transmission.  If the variable
 * 'error' in the IsdnHandle struct is nonzero, the operation failed, and a
 * description of the problem will be in the 'errormsg' array.
 * The ISDN-device is opened in NONBLOCK mode to make things run smooth
 * with multiple input sources (needs to use select to do this).
 */

struct IsdnHandle *IsdnInit(char *device, char *msn)
{
  struct termios isdnsetting;
  struct IsdnHandle *ih;

  ih = ifax_malloc(sizeof(*ih),"Isdn Handle");

  ih->fd = -1;
  ih->error = 0;
  ih->mode = TTYIMODE_CMDOFFLINE;
  strncpy(ih->devname,device,ISDNDEVNME_SIZE);
  ih->devname[ISDNDEVNME_SIZE-1] = 0;

  ih->recbufrp = 0;
  ih->recbufwp = 0;
  ih->recbufsize = 0;
  ih->start_time = time((time_t)0);
  ih->end_time = 0;

  if ( (ih->fd=open(device,O_RDWR|O_NONBLOCK)) < 0 ) {
    ih->error = 1;
    ih->fd = -1;
    sprintf(ih->errormsg,"Can't open '%s'",device);
    return ih;
  }

  if ( tcgetattr(ih->fd,&isdnsetting) < 0 ) {
    ih->error = 1;
    sprintf(ih->errormsg,"Unable to read terminal settings for '%s'",device);
    close(ih->fd);
    ih->fd = -1;
    return ih;
  }

  isdnsetting.c_iflag = 0;
  isdnsetting.c_oflag = 0;
  isdnsetting.c_lflag = 0;

  isdnsetting.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CLOCAL);
  isdnsetting.c_cflag |=  (CS8 | CREAD | HUPCL | CLOCAL | CRTSCTS);

  if ( tcsetattr(ih->fd, TCSANOW, &isdnsetting) < 0 ) {
    ih->error = 1;
    sprintf(ih->errormsg,"Unable to set terminal settings for '%s'",device);
    close(ih->fd);
    ih->fd = -1;
    return ih;
  }

  IsdnInitialize(ih,msn);

  return ih;
}


/* The 'IsdnService' function is called whenever there is any data
 * available on the 'ih->fd' device-handle - Ie. someone is calling.
 */

int IsdnService(struct IsdnHandle *ih)
{
  char buffer[ISDNMAXLNE_SIZE+4], tmp[80];
  int x;

  IsdnReadDevice(ih);
  for (;;) {
    x = IsdnGetLine(ih,buffer,ISDNMAXLNE_SIZE,0);
    if ( x < 0 )
      break;
    if ( !IsdnIsAsync(ih,buffer) ) {
      /* Strange, unwanted info is comming our way ... ? */
      format_ascii(tmp,buffer,72,x);
      printf("Unrecognized data: '%s'\n",tmp);
      display_incomming(ih);         /* _______ */
    }
  }

  if ( ih->ringing )
    return ISDN_RINGING;

  return ISDN_SLEEPING;
}


/* When 'IsdnService' says there is an ISDN_RINGING condition, this
 * function can be called to establish the connection.  The function
 * returns nonzero when a (two-way) connection is established.
 */

int IsdnAnswer(struct IsdnHandle *ih)
{
  ih->ringing = 0;

  IsdnSendCommand(ih,"ATA",0,1);

  if ( !strcmp(ih->cmdresult,"NO ANSWER") ) {
    /* False alarm, no incomming call, or hung up already */
    ih->mode = TTYIMODE_CMDOFFLINE;
    ih->remotemsn[0] = 0;
    return 0;
  }

  if ( strcmp(ih->cmdresult,"VCON") ) {
    /* Didn't get the expected VCON */
    ih->error = 1;
    strcpy(ih->errormsg,"Issuing ATA didn't result in 'VCON' as it should");
    return 0;
  }

  ih->mode = TTYIMODE_AUDIO;
  IsdnSendCommand(ih,"AT+VTX+VRX",0,0);

  return 1;
}


/* Starting an outgoing call is done with the following 'IsdnDial'
 * function, with the phone-number and timeout (in seconds) as the
 * argument.  The phone-number must be all digits, no commas, 'w' or space.
 */

int IsdnDial(struct IsdnHandle *ih, char *number, int timeout)
{
  char cmd[128], buffer[128], str[512];
  int t, r;

  sprintf(cmd,"ATD%s",number);
  IsdnSendCommand(ih,cmd,0,0);

  for ( t=0; t < timeout; t++ ) {

    printf("Waiting for response to DIAL...\n");
    IsdnWaitForInput(ih,1,0);
    IsdnReadDevice(ih);
    printf("got some or timeout\n");

    for (;;) {
      r = IsdnGetLine(ih,buffer,120,0);
      if ( r < 0 )
	break;
      printf("GotLine: '%s'\n",buffer);
      if ( !IsdnIsAsync(ih,buffer) )
	goto checkanswer;
    }
  }

  /* We have a timeout - hang up the connection; use the direct
   * approach to avoid having a 'VCON' mess up at a critical time.
   */

  printf("Timeout, hanging up\n");

  sprintf(cmd,"%cATH%c",CMDLNECHR,CMDLNECHR);
  IsdnSendRaw(ih,cmd,strlen(cmd));
  IsdnFlushInput(ih);

  return ISDN_NOANSWER;

 checkanswer:

  if ( !strcmp(buffer,"VCON") ) {
    ih->mode = TTYIMODE_AUDIO;
    IsdnSendCommand(ih,"AT+VTX+VRX",0,0);
    return ISDN_ANSWER;
  }

  if ( !strcmp(buffer,"BUSY") )
    return ISDN_BUSY;

  ih->error = 1;
  format_ascii(str,buffer,512,strlen(buffer));
  sprintf(ih->errormsg,"Got unknown response when dialing: '%s'",str);

  return ISDN_FAILED;
}

/* Send a series of samples over the ISDN-line by using this function.
 * The full-scale 16-bit signed integers are converted to a-Law before
 * they are transmitted.
 * NOTE: This function may not write all the samples that was requested
 * due to a buffer overflow, hangup or other.  This is not checked for,
 * so overflows should be avoided by not sending more than is reveived.
 */

void IsdnWriteSamples(struct IsdnHandle *ih, ifax_sint16 *src, int size)
{
  int t;
  ifax_uint8 alaw, *dst;
  ifax_sint16 s16;
  ifax_uint16 idx;

  if ( ih->mode != TTYIMODE_AUDIO )
    return;

  dst = &ih->txbuf[0];

  for ( t=0; t < size; t++ ) {
    s16 = *src++;
    idx = (((ifax_uint16)s16)>>4) & 0xFFF;
    alaw = sint2wala[idx];
    *dst++ = alaw;
    if ( alaw == ISDNTTY_DLE )
      *dst++ = alaw;
  }

  write(ih->fd,&ih->txbuf[0],dst-&ih->txbuf[0]);
}

/* Read audio-samples from the ISDN-system once we are online.  Works
 * much like an ordinary read, with partial reads indicating there is
 * no more data available at this point.  A complete read should be
 * followed by another read, since there may be more data waiting
 * to get off the buffers.
 */

int IsdnReadSamples(struct IsdnHandle *ih, ifax_sint16 *dst, int size)
{
  int remaining, more, chunk;
  ifax_uint8 alaw, *src, *end;

  if ( ih->mode != TTYIMODE_AUDIO )
    return -1;

  remaining = size;

  do {

    /* Read device until it empties or request completes */
    IsdnReadDevice(ih);
    more = ih->recbufsize == ISDNRECBUF_SIZE;

    while ( remaining > 0 ) {

      /* We need at least two samples to handle DLE+? simply/efficiently */
      if ( ih->recbufsize < 2 )
	break;

      /* Set up 'src' and 'chunk' for processing so that chunk+1 bytes are
       * available for investigation.  The extra byte is for DLE-examination.
       */

      src = &ih->recbuf[ih->recbufrp];

      if ( ih->recbufwp <= ih->recbufrp ) {
	/* Read from the read-pointer to the end of the ring-buffer */
	chunk = ISDNRECBUF_SIZE - ih->recbufrp;
	if ( ih->recbufwp > 0 ) {
	  /* ih->recbuf[0] is valid, use it as the 'chunk+1' byte */
	  ih->recbuf[ISDNRECBUF_SIZE] = ih->recbuf[0];
	} else {
	  /* ih->recbuf[0] is invalid, reduce chunk to make 'chunk+1' valid */
	  chunk--;
	}
      } else {
	/* Read section from read-pointer to write-pointer in ring-buffer */
	chunk = ih->recbufwp - ih->recbufrp - 1;
      }

      if ( chunk > remaining )
	chunk = remaining;

      end = &ih->recbuf[ih->recbufrp+chunk];

      while ( src < end ) {
	alaw = *src++;
	if ( alaw != ISDNTTY_DLE ) {
	  /* Pure sample value to be converted */
	  *dst++ = wala2sint[alaw];
	} else {
	  /* DLE-escape sequence, see what it is */
	  alaw = *src++;
	  if ( alaw == ISDNTTY_DLE ) {
	    /* Escaped a-Law code (bitreversed) of 0x10 */
	    *dst++ = wala2sint[alaw];
	    remaining++;
	   } else {
	     /* The DLE-escape is out of band signaling (no sample) */
	     remaining += 2;
	     if ( alaw == ISDNTTY_ETX ) {
	       /* DLE+ETX indicates a remote hangup */
	       ih->mode = TTYIMODE_CMDOFFLINE;

	       /* Calculate how much work has been done, and return */
	       chunk = src - &ih->recbuf[ih->recbufrp];
	       remaining -= chunk;
	       ih->recbufsize -= chunk;
	       ih->recbufrp += chunk;
	       if ( ih->recbufrp >= ISDNRECBUF_SIZE )
		 ih->recbufrp -= ISDNRECBUF_SIZE;
	       return size - remaining;
	     }
	   }
	}
      }

      chunk = src - &ih->recbuf[ih->recbufrp];
      remaining -= chunk;
      ih->recbufsize -= chunk;
      ih->recbufrp += chunk;
      if ( ih->recbufrp >= ISDNRECBUF_SIZE )
	ih->recbufrp -= ISDNRECBUF_SIZE;
    }
  } while ( more );

  return size - remaining;
}

void display_incomming(struct IsdnHandle *ih)
{
  char buf[4], c, str[32];

  for (;;) {
    while ( ih->recbufsize > 0 ) {
      c = ih->recbuf[ih->recbufrp];
      ih->recbufrp++;
      ih->recbufsize--;
      if ( ih->recbufrp >= ISDNRECBUF_SIZE )
	ih->recbufrp = 0;

      buf[0] = c;
      buf[1] = 0;
      format_ascii(str,buf,20,1);
      printf("%s",str);
      fflush(stdout);
    }

    IsdnWaitForInput(ih,1,0);
    IsdnReadDevice(ih);
  }
}
