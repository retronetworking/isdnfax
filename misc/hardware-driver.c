/* $Id$
******************************************************************************

   Fax program for ISDN.
   Implement an abstraction layer for telephone line access.
   
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

/* This file defines an API for accessing a phone line using a generic
 * interface.  This is called a HAL in Windows speak - a hardware
 * abstraction layer.  The reasons for having such an API is to make it
 * easy to support new and/or strange devices.
 *
 * A device is initialized by first allocating the HardwareHandle for
 * the hardware driver by calling the allocate function with the proper
 * device type specified (ISDN, soundcard, WinModems etc.).
 * Allocating the HardwareHandle is only the first step; it needs to
 * be configured and initialized with device dependant configuration
 * parameters before it can be used.
 *
 * Configuration parameters should be passed to the various drivers verbatim
 * from the configuration files in order to minimize impact to the whole system
 * when a new driver or options are added (ie. only the driver has to be
 * changed/added).
 *
 * After the configuration process has finished, the initialize function
 * is called to initialize the device and make it avilable.  If the device
 * can't be made available, a descriptive error should be returned.
 *
 * Example configuration sequence for an ISDN device:
 *
 *	hh = hardware_allocate("isdn");
 *	rc = hh->configure(hh,"msn","5551234");
 *	rc = hh->configure(hh,"device","/dev/ttyI5");
 *	rc = hh->initialize(hh);
 *
 * The 'hardware_allocate' will return zero if the device couldn't be
 * initialized (Ie. there is no such hardware supported).
 *
 * The above example configuration could be specified in the configuration
 * file as:
 *
 *	hardware = isdn
 *	isdn-msn = 5551234
 *	isdn-device = /dev/ttyI5
 */

#include <ifax/misc/malloc.h>
#include <ifax/misc/isdnline.h>
#include <ifax/misc/hardware-driver.h>


struct HardwareHandle *hardware_allocate(char *hwclass)
{
	struct HardwareHandle *hh;

	hh = ifax_malloc(sizeof(*hh),"Hardware driver instance");
	hh->state = UNKNOWN;
	hh->read_size = hh->write_size = 0;
	hh->error = 0;
	hh->errormsg[0] = '\0';

	if ( !strcmp(hwclass,"isdn") ) {
		/* ISDN is our primary goal and first target */
		hh->type = ISDN;
		isdn_allocate(hh);
		return hh;
	}

	/* Check for more supported hardware here */

	free(hh);		/* Failed, no such hardware supported */
	return 0;
}
