/* $Id$
******************************************************************************

   Fax program for ISDN.
   Make a memmory allocation and initialize the memory, or exit when failing.

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

/* The memory-locking stuff likes all memory allocated to actually be
 * initialized so all the memory pages of a program is mapped before
 * memory locking is attempted.
 */


#include <stdlib.h>
#include <string.h>
#include <ifax/debug.h>
#include <ifax/misc/globals.h>
#include <ifax/misc/malloc.h>

/* When asking for memory, specify number of bytes and a short string
 * identifying the usage.  This ID-string may ease debugging if the
 * memory allocations should ever fail.
 */

void *ifax_malloc(size_t size, char *usage)
{
  void *mem;

  mem = malloc(size);

  if ( mem == 0 ) {
    /* No memory can be allocated, bail out directly */
    ifax_dprintf(DEBUG_LAST,"%s: Can't allocate memory for: %s",
		 progname,usage);
    exit(1);
  }

  memset(mem,0,size);

  return mem;
}
