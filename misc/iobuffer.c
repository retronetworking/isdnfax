/* $Id$
******************************************************************************

   Fax program for ISDN.
   Implement a ring buffer for use by pty, isdn etc. code to handle I/O.

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

/* This utility modules helps to implement asynchronous I/O using a ring
 * buffer for queueing, and various service functions for doing non-blocking
 * read and write on the buffer.  Functions exists to fill and drain the
 * buffer.
 *




 * Usage:
 *
 *     struct IOBuffer *iob = iobuffer_allocate(int size);
 *     ....
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ifax/misc/iobuffer.h>
#include <ifax/debug.h>


struct IOBuffer *iobuffer_allocate(int size, char *devinfo)
{
	struct IOBuffer *iob;

	iob = ifax_malloc(sizeof(*iob),"IOBuffer instance");
	iob->data = ifax_malloc(size,"IOBuffer data");
	iob->total = size;
	iobuffer_reset(iob);
	strcpy(iob->descr,devinfo);

	iob->debug = FILLUPDATE | WRITE;

	return iob;
}

void iobuffer_reset(struct IOBuffer *iob)
{
	iob->fp = iob->dp = iob->size = 0;
}


/* The debugging functions needs the following buffer to get things
 * right and good looking.  The debug_buffer holds the current line
 * of debugging output, while the previous_prefix holds the prefix of
 * the current debug buffer, so the buffer can be flushed when a
 * different prefix is specified.
 */
static char debug_buffer[128];
static char previous_prefix[64] = { '\0', };
static int debug_idx, debug_any = 0;

static void iodump_flush(void)
{
	if ( debug_any )
		ifax_dprintf(DEBUG_DEBUG,"%s\n",debug_buffer);
	strcpy(debug_buffer,previous_prefix);
	debug_idx = strlen(debug_buffer);
	debug_any = 0;
}

static void iodump(char *prefix, ifax_uint8 *log, int size)
{
	char item[8], *i;
	ifax_uint8 c;
	int itemlen;

	static char *ctrl[33] = {
		"NUL","\\001","\\002","ETX","\\004","\\005","\\006","\\007",
		"\\010","\\011","\\n","\\013","\\014","\\r","\\016","\\017",
		"DLE","\\021","\\022","\\023","\\024","\\025","\\026","\\027",
		"\\030","\\031","\\032","ESC","\\034","\\035","\\036","\\037",
		"SPC"
	};

	if ( strcmp(previous_prefix,prefix) ) {
		/* Flush previous debug-output with different prefix */
		strcpy(previous_prefix,prefix);
		iodump_flush();
	}

	while ( size > 0 ) {

		c = *log++, size--;

		if ( c < 33 ) {
			i = ctrl[c];
		} else if ( c < 127 ) {
			item[0] = c;
			item[1] = '\0';
			i = item;
		} else {
			sprintf(item,"\\%03o",(int)c);
			i = item;
		}

		itemlen = strlen(i);

		if ( debug_idx + itemlen > 79 ) {
			/* Needs flushing */
			iodump_flush();
		}

		debug_buffer[debug_idx] = ' ';
		strcpy(&debug_buffer[debug_idx+1],i);
		debug_idx += itemlen + 1;
		debug_any = 1;
	}
}

void iobuffer_fill_segment(struct IOBuffer *iob, ifax_uint8 **dst, int *len)
{
	if ( iob->size >= iob->total ) {
		/* No space available */
		*dst = 0;
		*len = 0;
		return;
	}

	*dst = &iob->data[iob->fp];

	if ( iob->fp < iob->dp ) {
		*len = iob->dp - iob->fp;
		return;
	}

	*len = iob->total - iob->fp;
}

void iobuffer_fill_update(struct IOBuffer *iob, int size, char *info)
{
	char prefix[128];

	if ( info != 0 ) {
		strcpy(prefix,iob->descr);
		strcat(prefix,info);
	}

	if ( iob->fp < iob->total ) {
		/* Didn't wrap around */
		if ( info != 0 )
			iodump(prefix,&iob->data[iob->fp],size);
		iob->fp += size;
		iob->size += size;
	} else {
		/* Did wrap, split logging in two */
		if ( info != 0 ) {
			iodump(prefix,&iob->data[iob->fp],iob->total-iob->fp);
			iob->fp = iob->fp + size - iob->total;
			iob->size += size;
			iodump(prefix,&iob->data[0],iob->fp);
		}
	}

	iodump_flush();
}

void iobuffer_fill(struct IOBuffer *iob, ifax_uint8 *src, int size)
{
	int chunk;
	ifax_uint8 *dst;

	if ( size <= 0 )
		return;

	while ( size > 0 ) {
		iobuffer_fill_segment(iob,&dst,&chunk);
		if ( chunk == 0 ) {
			/* Oops, we couldn't get all to fit in buffer! */
			break;
		}
		if ( size < chunk )
			chunk = size;
		memcpy(dst,src,chunk);
		src += chunk;
		size -= chunk;
		iobuffer_fill_update(iob,chunk," FILL:");
	}
}

void iobuffer_drain_segments(struct IOBuffer *iob,
			     ifax_uint8 **src1, int *len1,
			     ifax_uint8 **src2, int *len2)
{
	if ( iob->size == 0 ) {
		/* No data in buffer at all */
		*src1 = 0, *len1 = 0;
		*src2 = 0, *len2 = 0;
		return;
	}
	
	if ( iob->dp < iob->fp ) {
		/* One continous segment */
		*src1 = &iob->data[iob->dp];
		*len1 = iob->fp - iob->dp;
		*len2 = 0;
		return;
	}

	/* Two segments */
	*src1 = &iob->data[iob->dp];
	*len1 = iob->total - iob->dp;
	*src2 = &iob->data[0];
	*len2 = iob->fp;
}

void iobuffer_drain_update(struct IOBuffer *iob, int size)
{
	iob->size -= size;
	iob->dp += size;

	if ( iob->dp >= iob->total )
		iob->dp -= iob->total;
}

int iobuffer_drain(struct IOBuffer *iob, ifax_uint8 *dst, int size)
{
	int len1, len2, total;
	ifax_uint8 *src1, *src2;

	iobuffer_drain_segments(iob,&src1,&len1,&src2,&len2);

	if ( len1 == 0 )
		return 0;

	if ( size < len1 ) {
		memcpy(dst,src1,size);
		return size;
	}

	memcpy(dst,src1,len1);
	dst += len1;
	total = len1;
	size -= len1;

	if ( size > 0 ) {
		if ( size < len2 ) {
			memcpy(dst,src2,size);
			total += size;
		} else {
			memcpy(dst,src2,len2);
			total += len2;
		}
	}

	return total;
}

int iobuffer_read(struct IOBuffer *iob, int fd)
{
	int rc, total = 0;
	ifax_uint8 *dst;
	int size;

	for (;;) {
		iobuffer_fill_segment(iob,&dst,&size);
		if ( size == 0 )
			break;
		rc = read(fd,dst,size);
		if ( rc < 0 ) {
			if ( errno == EAGAIN )
				break;
			if ( errno == EINTR )
				continue;
			printf("Errno1=%d (%s)\n",errno,strerror(errno));
			exit(69);
		}

		total += rc;
	}
	iobuffer_fill_update(iob,total," READ:");
	return total;
}

int iobuffer_write(struct IOBuffer *iob, int fd)
{
	char prefix[64];
	int rc, total = 0;
	ifax_uint8 *src1, *src2;
	int len1, len2;

	for (;;) {
		iobuffer_drain_segments(iob,&src1,&len1,&src2,&len2);
		if ( len1 == 0 )
			break;
		rc = write(fd,src1,len1);
		if ( rc < 0 ) {
			if ( errno == EAGAIN )
				break;
			if ( errno == EINTR )
				continue;
			printf("Errno2=%d (%s)\n",errno,strerror(errno));
			exit(69);
		}

		if ( iob->debug & WRITE ) {
			strcpy(prefix,iob->descr);
			strcat(prefix," WRITE:");
			iodump(prefix,src1,rc);
		}

		total += rc;
		iobuffer_drain_update(iob,rc);
	}

	if ( iob->debug & WRITE )
		iodump_flush();

	return total;
}
