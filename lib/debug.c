/* $Id$
******************************************************************************

   Fax program for ISDN.
   Debug functions.

   Copyright (C) 1998 Andreas Beck	[becka@ggi-project.org]
  
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

#include <stdarg.h>
#include <stdio.h>

#include <ifax/debug.h>

/* All debugging info of a severity of that or higher are printed to stderr.
 */
static enum debuglevel debuglevel=DEBUG_ALL;

/* Print debugging info.
 */
int ifax_dprintf(enum debuglevel severity,char *format,...)
{
	int rc=0;
	va_list list;
	
	if (severity>=debuglevel)
	{
		va_start(list,format);
		rc=vprintf(format,list);
		va_end(list);
	}
	return rc;
}

/* Set the debugging level.
 */
void ifax_debugsetlevel(enum debuglevel severity)
{
	debuglevel=severity;
}
