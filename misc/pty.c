/* $Id$
******************************************************************************

   Fax program for ISDN.
   Handle all communications over a pseudo-tty (pty) to emulate a modem.

   Copyright (C) 1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
   Copyright (C) 1999 Andreas Beck [becka@ggi-project.org]
   
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

/* This module takes care of initialization and maintenance
 * of a pair of tty/pty for emulating a modem on a terminal
 * port.
 */


#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <ifax/misc/pty.h>
#include <ifax/debug.h>
#include <ifax/misc/globals.h>
#include <ifax/misc/malloc.h>


/* Open and initialize a pty device.  This is done both on initial setup
 * and later resets.
 */

static void pty_open(struct PtyHandle *ph)
{
	struct termios tios;

	if ( (ph->ptyfd=open(ph->device,O_RDWR|O_NONBLOCK)) < 0 ) {
		ifax_dprintf(DEBUG_LAST,
			     "%s: Unable to open pty device '%s': %s\n",
			     progname,ph->device,strerror(errno));
		exit(1);
	}

	ph->rx.rp = ph->rx.wp = ph->rx.size = 0;
	ph->tx.rp = ph->tx.wp = ph->tx.size = 0;

	if ( tcgetattr(ph->ptyfd,&tios) ) {
		ifax_dprintf(DEBUG_LAST,
			     "%s: Unable to tcgetattr pty device '%s': %s\n",
			     progname,ph->device,strerror(errno));
		exit(1);
	}

	tios.c_lflag &= ~ECHO;

	if ( tcsetattr(ph->ptyfd,TCSANOW,&tios) ) {
		ifax_dprintf(DEBUG_LAST,
			     "%s: Unable to tcsetattr pty device '%s: %s'\n",
			     progname,ph->device,strerror(errno));
		exit(1);
	}
}


static void pty_dump(char *prefix, ifax_uint8 *log1, int logsize1,
		     ifax_uint8 *log2, int logsize2)
{
	char buffer[256];
	char item[8], *i;
	ifax_uint8 c;
	int wp, prefixlen, itemlen;

	static char *ctrl[33] = {
		"NUL","\\001","\\002","ETX","\\004","\\005","\\006","\\007",
		"\\010","\\011","\\n","\\013","\\014","\\r","\\016","\\017",
		"DLE","\\021","\\022","\\023","\\024","\\025","\\026","\\027",
		"\\030","\\031","\\032","ESC","\\034","\\035","\\036","\\037",
		"SPC"
	};

	wp = 0;
	prefixlen = strlen(prefix);

	while ( logsize1 > 0 ) {

		if ( wp == 0 ) {
			strcpy(buffer,prefix);
			wp = prefixlen;
		}

		c = *log1++;
		logsize1--;

		if ( c < 33 ) {
			i = ctrl[c];
		} else if ( c < 127 ) {
			item[0] = c;
			item[1] = '\0';
			i = item;
		} else {
			sprintf(item,"%03o",(int)c);
			i = item;
		}

		itemlen = strlen(i);
		buffer[wp++] = ' ';
		strcpy(&buffer[wp],i);
		wp += itemlen;

		if ( wp > 75 ) {
			buffer[wp++] = '\n';
			buffer[wp] = '\0';
			ifax_dprintf(DEBUG_DEBUG,buffer);
			wp = 0;
		}

		if ( logsize1 == 0 ) {
			logsize1 = logsize2;
			log1 = log2;
			logsize2 = 0;
		}
	}

	if ( wp ) {
		buffer[wp++] = '\n';
		buffer[wp] = '\0';
		ifax_dprintf(DEBUG_DEBUG,buffer);
	}
}

		 
			

/* Allocate and initialize a pty instance for the given device.
 * Once initialized it can be read/written/serviced using all
 * functions in this file.
 */

struct PtyHandle *pty_initialize(char *device)
{
	struct PtyHandle *ph;

	ph = ifax_malloc(sizeof(*ph),"PTY instance");

	if ( strlen(device) >= PTY_DEVNAME_SIZE ) {
		ifax_dprintf(DEBUG_LAST,
			     "%s: Name of pty device '%s' too long\n",
			     progname,device);
		exit(1);
	}

	strcpy(ph->device,device);
	ph->debug = 7;
	pty_open(ph);

	return ph;
}


/* Reset the pty by closing and re-opening the pty-file and initializing
 * it again, but maintain the same PtyHandle to it.
 */

void pty_reset(struct PtyHandle *ph)
{
	ifax_dprintf(DEBUG_DEBUG, "%s: Resetting pty '%s'\n",
		     progname,ph->device);

	close(ph->ptyfd);
	pty_open(ph);
}


/* This function returns how many bytes can be written to the pty
 * at this instance.  Trying to write more bytes than this function
 * reports possible is undefined.
 */

int pty_write_max(struct PtyHandle *ph)
{
	return PTY_BUFFERSIZE - ph->tx.size;
}


/* Return number of bytes queued on the write-queue.  We may want
 * to watch this value to keep modem-to-modem flow control updated
 * on progress.  The total buffer capacity is found with:
 *
 *       pty_write_queued(ph) + pty_write_max(ph)
 */

int pty_write_queued(struct PtyHandle *ph)
{
	return ph->tx.size;
}


/* Return how many bytes can be read from the pty.  NOTE: The number of
 * bytes returned here may have to be accessed using two pty_reabbuffer
 * operations.  In other words, this function may return 1000 bytes
 * available for reading, but the first call to pty_readbuffer may only
 * yield 1 byte... The next however will yield the remaining 999 bytes.
 */

int pty_read_max(struct PtyHandle *ph)
{
	return ph->rx.size;
}


/* Write a buffer to the pty.  It is important not to write more than
 * the software buffer can hold.  If more is written, the result is
 * undefined.
 */

void pty_write(struct PtyHandle *ph, void *buffer, size_t size)
{
	ifax_uint8 *dst, *src = buffer;
	size_t chunk;
	char prefix[256];

	if ( pty_write_max(ph) < size ) {
		ifax_dprintf(DEBUG_SEVERE,
			     "%s: Overruns on output pty device '%s'\n",
			     progname,ph->device);
		return;
	}

	if ( ph->debug & LOG_WRITE ) {
		sprintf(prefix,"PTY-wrt %s:",ph->device);
		pty_dump(prefix,buffer,size,0,0);
	}

	if ( ph->tx.wp >= ph->tx.rp ) {

		/* Unused space in both ends of buffer */
		dst = &ph->tx.buffer[ph->tx.wp];
		chunk = PTY_BUFFERSIZE - ph->tx.wp;

		ph->tx.size += size;
		if ( chunk > size ) {
			ph->tx.wp += size;
			memcpy(dst,src,size);
		} else {
			memcpy(dst,src,chunk);
			if ( (ph->tx.wp = size - chunk) > 0 ) {
				src += chunk;
				memcpy(&ph->tx.buffer[0],src,ph->tx.wp);
			}
		}
		return;
	}

	/* Unused space in the middle */
	memcpy(&ph->tx.buffer[ph->tx.wp],src,size);
	ph->tx.wp += size;
	ph->tx.size += size;
}

/* When a select is to be called to wait for things to happen, the
 * following function should be used to set up the read and write
 * fd_set structures so the wakeup conditions will be suitable
 * for the pty-service function.
 */

void pty_prepare_select(struct PtyHandle *ph, int *maxfd,
			fd_set *rfd, fd_set *wfd)
{
	/* read only if rx buffer can hold data */
	if ( ph->rx.size < PTY_BUFFERSIZE ) {
		FD_SET(ph->ptyfd,rfd);
	} else {
		FD_CLR(ph->ptyfd,rfd);
	}

	/* Write only if there is anything queued */
	if ( ph->tx.size > 0 ) {
		FD_SET(ph->ptyfd,wfd);
	} else {
		FD_CLR(ph->ptyfd,wfd);
	}

	if ( ph->ptyfd + 1 > *maxfd )
		*maxfd = ph->ptyfd + 1;
}


/* Call this function regularly, preferably after a select has
 * indicated there is traffic or possible traffic on the pty.
 * This function will try to flush the output queue.
 */

void pty_service_write(struct PtyHandle *ph)
{
	int chunk, rc;

	/* Try to flush tx-buffer if there is something queued */
	if ( ph->tx.size > 0 ) {
		chunk = PTY_BUFFERSIZE - ph->tx.rp;
		if ( chunk > ph->tx.size )
			chunk = ph->tx.size;
		rc = write(ph->ptyfd,&ph->tx.buffer[ph->tx.rp],chunk);
		if ( rc < 0 ) {
			if ( errno == EAGAIN )
				return;
			goto failed_write;
		}
		ph->tx.rp += rc;
		ph->tx.size -= rc;
		if ( rc < chunk )
			return;
		if ( ph->tx.rp >= PTY_BUFFERSIZE )
			ph->tx.rp = 0;
		if ( ph->tx.size > 0 ) {
			rc = write(ph->ptyfd,&ph->tx.buffer[0],ph->tx.size);
			if ( rc < 0 ) {
				if ( errno == EAGAIN )
					return;
				goto failed_write;
			}
			ph->tx.rp = rc;
			ph->tx.size -= rc;
		}
	}

	return;

 failed_write:

	ifax_dprintf(DEBUG_LAST,
		     "%s: Can't write on pty '%s': %s\n",
		     progname,ph->device,strerror(errno));
	exit(1);
}

/* Call this function regularly, preferably after a select has
 * indicated there is traffic or possible traffic on the pty.
 * This function will try to read any new input into the input queue.
 */

void pty_service_read(struct PtyHandle *ph)
{
	int chunk;
	char prefix[256];
	ifax_uint8 *log1 = 0, *log2 = 0;
	int logsize1, logsize2;

	logsize1 = logsize2 = 0;

	/* Try to read if there is space in rx-buffer */
	if ( (chunk = PTY_BUFFERSIZE - ph->rx.size) == 0 )
		return;

	if ( (PTY_BUFFERSIZE - ph->rx.wp) < chunk )
		chunk = PTY_BUFFERSIZE - ph->rx.wp;

	log1 = &ph->rx.buffer[ph->rx.wp];
	logsize1 = read(ph->ptyfd,log1,chunk);
	if ( logsize1 < 0 ) {
		if ( errno == EAGAIN )
			return;
		goto failed_read;
	}

	ph->rx.wp += logsize1;
	ph->rx.size += logsize1;
	if ( logsize1 < chunk )
		goto all_ok;

	if ( ph->rx.wp >= PTY_BUFFERSIZE )
		ph->rx.wp = 0;
	if ( (chunk = PTY_BUFFERSIZE - ph->rx.size) == 0 )
		goto all_ok;

	ph->rx.wp = 0;
	log2 = &ph->rx.buffer[ph->rx.wp];
	logsize2 = read(ph->ptyfd,log2,chunk);
	if ( logsize2 < 0 ) {
		if ( errno == EAGAIN ) {
			logsize2 = 0;
			goto all_ok;
		}
		goto failed_read;
	}

	ph->rx.wp += logsize2;
	ph->rx.size += logsize2;

 all_ok:

	if ( ph->debug & LOG_SERVICE_READ ) {
		sprintf(prefix,"PTY-srd %s:",ph->device);
		pty_dump(prefix,log1,logsize1,log2,logsize2);
	}

	return;

 failed_read:
	
	ifax_dprintf(DEBUG_DEBUG, "%s: Reading pty '%s' failed: %s\n",
		     progname,ph->device,strerror(errno));

	pty_reset(ph);
	return;
}


/* The following function sets up a pointer into a buffer and
 * its size, which can be used for direct parsing without copying.
 * The size of the buffer returned may be shorter than the total
 * size of the receive buffer.  Advance the pty by the full size
 * and call 'pty_next_buffer' to get access to the next chunk of
 * data.
 */

void pty_readbuffer(struct PtyHandle *ph, ifax_uint8 **buffer, size_t *size)
{
	size_t chunk;

	*buffer = &ph->rx.buffer[ph->rx.rp];
  
	chunk = PTY_BUFFERSIZE - ph->rx.rp;
	if ( ph->rx.size < chunk )
		chunk = ph->rx.size;

	*size = chunk;
}

/* The 'pty_advance' function advances the read buffer without any
 * memory copy operations, assuming they have been processed allready
 * (by 'pty_readbuffer') or can be discarded.
 */

void pty_advance(struct PtyHandle *ph, size_t size)
{
	int logsize1, logsize2;
	char prefix[256];

	if ( ph->debug & LOG_ADVANCE_READ ) {

		sprintf(prefix,"PTY-adv %s:",ph->device);

		logsize1 = size;
		logsize2 = 0;
		if ( ph->rx.rp + logsize1 > PTY_BUFFERSIZE ) {
			logsize1 = PTY_BUFFERSIZE - ph->rx.rp;
			logsize2 = size - logsize1;
		}

		pty_dump(prefix,&ph->rx.buffer[ph->rx.rp],logsize1,
			 &ph->rx.buffer[0],logsize2);
	}
	
	ph->rx.size -= size;
	ph->rx.rp += size;
	if ( ph->rx.rp >= PTY_BUFFERSIZE )
		ph->rx.rp -= PTY_BUFFERSIZE;
}


/* Simple helper function to simply output formatted strings to the pty */

void pty_printf(struct PtyHandle *ph, char *format, ... )
{
#define PTY_PRINTF_MAXSIZE	512

	va_list args;
	char tmp[PTY_PRINTF_MAXSIZE+4];

	va_start(args,format);
	vsnprintf(tmp, PTY_PRINTF_MAXSIZE, format, args);
	va_end(args);

	pty_write(ph,tmp,strlen(tmp));
}
