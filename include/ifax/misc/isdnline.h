/* $Id$
******************************************************************************

   Fax program for ISDN.
   Communicate with the /dev/ttyI* devices to transfer audio over ISDN

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

/* Utility-functions to administrate ISDN-audio transfers.
 */

#include <time.h>

#define ISDNTXBUF_SIZE     512
#define ISDNRECBUF_SIZE    512
#define ISDNMAXLNE_SIZE    256
#define ISDNERRMSG_SIZE    128
#define ISDNDEVNME_SIZE     64
#define ISDNMSN_SIZE        64

#define ISDN_SLEEPING       -1
#define ISDN_RINGING        -2
#define ISDN_FAILED         -3
#define ISDN_ANSWER         -4
#define ISDN_BUSY           -5
#define ISDN_NOANSWER       -6

#define ISDNTTY_ETX       0x03
#define ISDNTTY_DLE       0x10
#define ISDNTTY_DC4       0x14

#define TTYIMODE_CMDOFFLINE  0
#define TTYIMODE_CMDONLINE   1
#define TTYIMODE_AUDIO       2

struct IsdnHandle {
  int fd;                             /* Filedescriptor of ISDN device */
  int error;                          /* Nonzero if error (errormsg valid) */
  int mode;                           /* Which mode the tty is in */
  int ringing;                        /* Nonzero to indicate ringing */
  int recbufrp, recbufwp, recbufsize; /* Receive buffer characteristics */
  char cmdresult[ISDNMAXLNE_SIZE+2];  /* Result of command */
  char errormsg[ISDNERRMSG_SIZE+2];   /* Detailed error description */
  char devname[ISDNDEVNME_SIZE];      /* Name of ISDN-device */
  char remotemsn[ISDNMSN_SIZE];       /* Remote phone-number */
  time_t start_time, end_time;        /* Timing characteristics for call */

  ifax_uint8 recbuf[ISDNRECBUF_SIZE+1];  /* Receive buffer array */
  ifax_uint8 txbuf[ISDNTXBUF_SIZE+ISDNTXBUF_SIZE];
};

extern struct IsdnHandle *IsdnInit(char *, char *);
extern int IsdnService(struct IsdnHandle *);
extern int IsdnAnswer(struct IsdnHandle *);
extern int IsdnDial(struct IsdnHandle *, char *, int);
extern int IsdnReadSamples(struct IsdnHandle *, ifax_sint16 *, int);
extern void IsdnWriteSamples(struct IsdnHandle *, ifax_sint16 *, int);
extern void IsdnShortSleep(int);
