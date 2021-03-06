/* $Id$
******************************************************************************

   Fax program for ISDN.
   Set up environment for a real-time daemon process like the 'amodemd'

   Copyright (C) 1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
   Copyright (C) 1999 Andreas Beck   [becka@ggi-project.org]
  
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

/* Sufficient space (in bytes) to hold the machine-stack with
 * auto-variables, arguments and return addresses.  Modify if
 * more space is needed, or the current value is too large.
 * However; it should never be smaller than the actual maximum
 * stack-usage. One megabyte is probably generous.
 */
#define TOTAL_STACK_USAGE                  1048576
#define MINIMUM_FREE_STACK_SAFETY_MARGIN     20480

extern int  initialize_realtime(void);	
/* RC is 0 for o.k. 
 * |1 for unable to lock memory, 
 * |2 for unable to get realtime priority
 * warnings are printed in both cases.
 */

extern void start_daemon(void);
extern void check_stack_usage(void);
