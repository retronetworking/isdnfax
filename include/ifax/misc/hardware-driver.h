/* $Id$
******************************************************************************

   Fax program for ISDN.
   
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

#ifndef _HARDWARE_DRIVER_H
#define _HARDWARE_DRIVER_H

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <ifax/types.h>

typedef enum {
	UNKNOWN=0,		/* Unknown, uninitialized status */
	INITIALIZING=1,		/* Initialization in progress */
	FAILED=2,		/* Initialization or other failed */
	IDLE=3,			/* All is well, ready to do work */
	RINGING=4,		/* Someone is calling us */
	CALLING=5,		/* We are calling someone else */
	ONLINE=6,		/* We have a voice connection established */
	BUSY=7,			/* Remote end was busy */
	NOANSWER=8		/* Remote end didn't answer the phone */
} hardware_state_t;

typedef enum {
	ISDN		/* Use /dev/ttyI* devices for ISDN line access */
} hardware_type_t;

struct HardwareHandle {
	
	void *private;			/* Driver specific private data */
	hardware_type_t type;		/* Type of hardware driver */
	hardware_state_t state;		/* Status of hardware right now */
	int read_size, write_size;	/* Next read/write sizes expected */

	/* If an error is encountered in the hardware driver or some of
	 * the modules it uses, the 'error' variable will be non-zero to
	 * indicate this, and an error message in 'errormsg'.
	 */
	int error;
	char errormsg[128];

	/* Set up all three fd_set used in a select for the hardware
	 * driver.  The 'maxfd' is increased to the max fd-value used by
	 * the hardware driver (if it is smaller initially).
	 */
	void (*prepare_select)(struct HardwareHandle *hh,
			       int *maxfd, fd_set *r, fd_set *w, fd_set *e);

	/* Call this function right after the select in order to service
	 * the hardware driver.  All fd_set used in the select must be
	 * supplied in case the hardware driver needs to inspect them.
	 */
	void (*service_select)(struct HardwareHandle *hh,
			       fd_set *r, fd_set *w, fd_set *e);

	/* This service function must be called after all the read and
	 * write operations on the hardware has been performed.  This is
	 * used to clean up, flush buffers etc. (or nothing at all).
	 */
	void (*service_readwrite)(struct HardwareHandle *hh);

	/* Call this function to *initialize* dialling a given number.
	 * The dialing operation may have to be carried out over a longer
	 * time interval, which means the work must be done in the
	 * 'service_select' routine (DTMF generation, wait for answer etc.)
	 */
	void (*dial)(struct HardwareHandle *hh, char *number);

	/* Force a hangup.  The hangup may not happen immediately, but
	 * carried out on subsequent calls to 'service_select'.
	 */
	void (*hangup)(struct HardwareHandle *hh);

	/* Read a number of samples from the hardware driver.
	 */
	void (*read)(struct HardwareHandle *hh, ifax_uint8 *smpls, int cnt);

	/* Write a number of (16-bit signed) samples to the hardware driver.
	 */
	void (*write)(struct HardwareHandle *hh, ifax_sint16 *smpls, int cnt);

	/* Supply the hardware driver with configuration parameters.
	 */
	void (*configure)(struct HardwareHandle *hh, char *param, char *value);

	/* After the driver has been configured, it must be initialized before
	 * it can be used properly (used in select loops).
	 */
	void (*initialize)(struct HardwareHandle *hh);
};

struct HardwareHandle *hardware_allocate(char *hardware);

#endif
