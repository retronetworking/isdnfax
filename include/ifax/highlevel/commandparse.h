/* $Id$
******************************************************************************

   Fax program for ISDN.
   Parse AT-commands received on a pty-interface

   Copyright (C) 1999 Andreas Beck [becka@ggi-project.org]
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

#include <ifax/types.h>
#include <ifax/misc/pty.h>

#define MODEMHANDLE_TMPBUFFER_SIZE	2048

#define DLE	0x10
#define ETX	0x03
#define S_DLE	"\x10"
#define S_ETX	"\x03"

#define MSTAT_UNUSEDMARKER		0
#define MSTAT_BELLMODULATION		1
#define MSTAT_ECHO			2
#define MSTAT_IDENTIFY			3
#define MSTAT_WORDRESPONSE		4
#define MSTAT_NODTERESPONSES		5
#define MSTAT_VOLUME			6
#define MSTAT_SPEAKERMODE		7
#define MSTAT_AUTOHANDSHAKE		8
#define MSTAT_WANTRETRAIN		9
#define MSTAT_ENHRESPONSE		10
#define MSTAT_LONGSPACEDISCONNECT	11
#define MSTAT_PROFILE			12
#define MSTAT_FLOWCONTROL		13
#define MSTAT_DTRBEHAVIOUR		14
#define MSTAT_DCDBEHAVIOUR		15
#define MSTAT_MAKEBREAK			16
#define MSTAT_NEGOTIATION		17
#define MSTAT_FORCECTS			18
#define MSTAT_FORCEDSR			19
#define MSTAT_FAE			20
#define MSTAT_FAXCLASS			21
#define MSTAT_FRH			22
#define MSTAT_FRM			23
#define MSTAT_FRS			24
#define MSTAT_FTH			25
#define MSTAT_FTM			26
#define MSTAT_FTS			27
#define MSTAT_DIALDETECT		28
#define MSTAT_DIALPULSE			29
#define MSTAT_OFFHOOK			30
#define MSTAT_REGS			31
#define MSTAT_REG_2_ESC			(MSTAT_REGS+2)
#define MSTAT_REG_3_CR			(MSTAT_REGS+3)
#define MSTAT_REG_4_LF			(MSTAT_REGS+4)
#define MSTAT_REG_5_BS			(MSTAT_REGS+5)
#define MSTAT_NUMREGS			80
#define MSTAT_SIZE			(MSTAT_REGS+MSTAT_NUMREGS)


typedef struct {
	int parm[MSTAT_SIZE];
} modemstat_t;


struct ModemHandle {
	 /* AT-command store for buildup on piecemeal input */
	ifax_uint8 at_command_buffer[MODEMHANDLE_TMPBUFFER_SIZE+8];
	char *parseptr;
	int readpos;
	int at_read_line_mode;

	/* Auxilary data, like a HDLC-frame in fax-class 1 is stored here */
	ifax_uint8 aux_buffer[MODEMHANDLE_TMPBUFFER_SIZE+8];
	int auxpos;		/* Current end of frame pointer */
	int have_dle;		/* Nonzer of last byte was DLE */

	/* Function to call to handle input to the modem */
	int (*function)(struct ModemHandle *, struct PtyHandle *);

	modemstat_t mstat_current, mstat_profile[2];
	char crlf[3];
};

struct modemcommand;

typedef int (*cmdhandler)(struct ModemHandle *mh,
			  struct PtyHandle *ph,
			  char *prebuf,
			  char **postbuf,
			  struct modemcommand *cmd);

typedef struct modemcommand {
	char *cmd;
	int parmmin,parmmax;
	int parm_offset;
	cmdhandler handler;
} mdmcmd;

extern void modeminput(struct ModemHandle *, struct PtyHandle *);
extern struct ModemHandle *modem_initialize(void);
