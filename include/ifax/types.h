/* $Id$
******************************************************************************

   Fax program for ISDN.
   Define data-types needed to reliably do DSP-code on a standard system

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


/* FIXME - We should autoconf that !
 */

#ifndef _IFAX_TYPES_H
#define _IFAX_TYPES_H

typedef unsigned char ifax_uint8;
typedef signed   char ifax_sint8;
typedef          char ifax_int8;

typedef unsigned short ifax_uint16;
typedef signed   short ifax_sint16;
typedef          short ifax_int16;

typedef unsigned int ifax_uint32;
typedef signed   int ifax_sint32;
typedef          int ifax_int32;

#endif
