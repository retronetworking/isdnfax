/* $Id$
******************************************************************************

   Fax program for ISDN.
   Handle all communications over a pseudo-tty (pty) to emulate a modem.

   Copyright (C) 1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
   Copyright (C) 1998 Andreas Beck [becka@ggi-project.org]
   
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

#include <sys/types.h>
#include <sys/timeb.h>

#include <ifax/types.h>

#ifndef _MISC_PTY_H
#define _MISC_PTY_H

#define PTY_BUFFERSIZE     512
#define PTY_DEVNAME_SIZE    64

struct PtyHandle {
	int ptyfd;
	struct timeb lastinput;
	enum {
		NO_LOGGING=0,
		LOG_ADVANCE_READ=1,
		LOG_SERVICE_READ=2,
		LOG_WRITE=4
	} debug;

	struct PtyBuffer {
		int wp, rp, size;
		ifax_uint8 buffer[PTY_BUFFERSIZE];
	} tx, rx;

	char device[PTY_DEVNAME_SIZE+2];
};

extern struct PtyHandle *pty_initialize(char *device);
extern void pty_reset(struct PtyHandle *ph);
extern int pty_write_max(struct PtyHandle *ph);    /* How large write is OK */
extern int pty_write_queued(struct PtyHandle *ph); /* Size of current queue */
extern int pty_read_max(struct PtyHandle *ph);      /* How large read is OK */extern void pty_write(struct PtyHandle *ph, void *buf, size_t size);
extern void pty_prepare_select(struct PtyHandle *, int *,fd_set *, fd_set *);
extern void pty_service_write(struct PtyHandle *ph);
extern void pty_service_read(struct PtyHandle *ph);
extern void pty_readbuffer(struct PtyHandle *ph, ifax_uint8 **buf, size_t *sz);
extern void pty_advance(struct PtyHandle *ph, size_t size);
extern void pty_printf(struct PtyHandle *ph, char *format, ... );

#endif
