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

#ifndef _MISC_IOBUFFER_H
#define _MISC_IOBUFFER_H

#include <ifax/types.h>
#include <ifax/misc/malloc.h>

struct IOBuffer {
	ifax_uint32 fp, dp, size, total;
	ifax_uint8 *data;
	int debug_buffer_idx;

	enum {
		FILLUPDATE = 1,
		WRITE = 2
	} debug;

	char descr[32];
};

struct IOBuffer *iobuffer_allocate(int size, char *devinfo);
void iobuffer_fill_segment(struct IOBuffer *iob, ifax_uint8 **dst, int *len);
void iobuffer_fill_update(struct IOBuffer *iob, int size, char *info);
void iobuffer_fill(struct IOBuffer *iob, ifax_uint8 *src, int size);
void iobuffer_drain_segments(struct IOBuffer *iob,
			     ifax_uint8 **src1, int *len1,
			     ifax_uint8 **src2, int *len2);
void iobuffer_drain_update(struct IOBuffer *iob, int size);
int iobuffer_drain(struct IOBuffer *iob, ifax_uint8 *dst, int size);
int iobuffer_read(struct IOBuffer *iob, int fd);
int iobuffer_write(struct IOBuffer *iob, int fd);
void iobuffer_reset(struct IOBuffer *iob);

#endif
