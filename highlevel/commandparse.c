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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <ifax/misc/malloc.h>
#include <ifax/highlevel/commandparse.h>
#include <ifax/misc/pty.h>


/* Initialize a modem-stat structure to initial default values. */

static void setup_default(modemstat_t *mstat)
{
	int t;

	for ( t=0; t < MSTAT_SIZE; t++ )
		mstat->parm[t] = 0;

	mstat->parm[MSTAT_ECHO] = 1;
	mstat->parm[MSTAT_WORDRESPONSE] = 1;
	mstat->parm[MSTAT_VOLUME] = 2;
	mstat->parm[MSTAT_SPEAKERMODE] = 1;
	mstat->parm[MSTAT_AUTOHANDSHAKE] = 1;
	mstat->parm[MSTAT_FLOWCONTROL] = 3;
	mstat->parm[MSTAT_NEGOTIATION] = 5;
	mstat->parm[MSTAT_DIALDETECT] = 4;

	mstat->parm[MSTAT_REG_2_ESC] = 43;	/* '+' = ASCII 43 */
	mstat->parm[MSTAT_REG_3_CR] = 13;	/* CR = ASCII 13 */
	mstat->parm[MSTAT_REG_4_LF] = 10;	/* LF = ASCII 10 */
	mstat->parm[MSTAT_REG_5_BS] = 8;	/* BS = ASCII 8 */
}

static void calculate_stuff(struct ModemHandle *mh)
{
	mh->crlf[0] = mh->mstat_current.parm[MSTAT_REG_3_CR];
	mh->crlf[1] = mh->mstat_current.parm[MSTAT_REG_4_LF];
	mh->crlf[2] = '\0';
}

/* Start parsing AT-commands.  This function is called to initialize
 * AT-commands parsing from the ground up, ie. commands not yet
 * completed on the previous command-line will be discarded.  This
 * is usually what you want when the line dicipline is changed.
 */

static int parse_at_commands(struct ModemHandle *, struct PtyHandle *);

void at_parse_reset(struct ModemHandle *mh)
{
	mh->function = parse_at_commands;
	mh->at_read_line_mode = 1;		/* Start by reading a line */
	mh->readpos = 0;			/* Fill in from start of buf */
}

void at_bad_error(struct ModemHandle *mh, struct PtyHandle *ph)
{
	pty_printf(ph,"ERROR%s",mh->crlf);
	at_parse_reset(mh);
}

struct ModemHandle *modem_initialize(void)
{
	struct ModemHandle *mh;

	if ( (mh = ifax_malloc(sizeof(*mh),"ModemHandle instance")) == 0 )
		return 0;

	at_parse_reset(mh);
	setup_default(&mh->mstat_current);
	calculate_stuff(mh);

	return mh;
}


/* The 'isquery' function is used to handle queries into various variables
 * and the current configuration.  It works like this:  Make the buffer
 * point to the first byte past the decoded command and let parmnum hold
 * the parameter number in the mstat array that holds the relevant parameter.
 *
 * Error situations are handeled by calling the error routine directly, and
 * returning a "yes it was a query" status to make the calling function
 * finish quickly (and return to the main loop).
 */

int isquery(struct ModemHandle *mh, struct PtyHandle *ph,
	    char **buf, int parmnum)
{
	if ( **buf == '?' ) {
		(*buf)++;
		if ( parmnum == 0 ) {
			at_bad_error(mh,ph);
			return 1;
		}
		pty_printf(ph,"%d%s",mh->mstat_current.parm[parmnum],mh->crlf);
		return 1;
	}

	return 0;
}

#define PARMOFFSET(x)       mh->mstat_current.parm[cmd->parm_offset+x]
#define PARM PARMOFFSET(0)


int set_to_1(struct ModemHandle *mh, struct PtyHandle *ph,
	     char *prebuf, char **postbuf, mdmcmd *cmd)
{
	PARM = 1;
	return 0;
}

int set_to_0(struct ModemHandle *mh, struct PtyHandle *ph,
	     char *prebuf, char **postbuf, mdmcmd *cmd)
{
	PARM = 0;
	return 0;
}

int check_andK_allowed(struct ModemHandle *mh, struct PtyHandle *ph,
		       char *prebuf, char **postbuf, mdmcmd *cmd)
{
	switch( PARM ) { 
		case 0:
		case 3:
		case 4:
		case 5:
			return 0;
		default: /* not allowed */
			return 1;
	}
}


/* Do special handles commands that can be viewed as register assignments,
 * with possible query of its value, but with a possibly more complicated
 * action than just assigning a parameter.
 *
 * If a special command has to be run, this function returns 1 to indicate
 * this fact (do-special).  If no action should be taken (a simple query
 * or an error occured), then 0 is returned.  The calling function should
 * return immediately when a zero is returned.
 */

int do_special(struct ModemHandle *mh, struct PtyHandle *ph,
		char *prebuf, char **postbuf, mdmcmd *cmd, char *caps)
{
	int value, checkv;
	char *checkp;

	if ( isquery(mh,ph,postbuf,cmd->parm_offset) )
		return 0;

	if ( **postbuf == '=' ) {		/* Assignment */

		if ( *++*postbuf == '?' ) {	/* query options */
			pty_printf(ph,"%s%s",caps,mh->crlf);
			(*postbuf)++;
			return 0;
		}

		value = strtol(*postbuf,postbuf,10);

		checkp = caps;
		while ( *checkp ) {
			checkv = strtol(checkp,&checkp,10);
			if ( checkv == value ) {
				PARM = value;
				return 1;
			}
			if ( *checkp )
				checkp++;
		}
	}
	
	/* Bad news; no legal value or bad syntax - give error */

	at_bad_error(mh,ph);
	return 0;		/* FIXME: Is this correct return value? */
}

int do_fclass(struct ModemHandle *mh, struct PtyHandle *ph,
	      char *prebuf, char **postbuf, mdmcmd *cmd)
{	
	do_special(mh,ph,prebuf,postbuf,cmd,"0,1");
	return 0;
}

static int parse_ftm_data(struct ModemHandle *mh,struct PtyHandle *ph)
{
	ifax_uint8 c, *walk;
	size_t size, used;

	for (;;) {
		pty_readbuffer(ph,&walk,&size);

		if ( size == 0 )
			return 0;

		used = 0;
		while ( size > 0 ) {

			c = *walk++;
			used++;
			size--;
			
			if ( mh->have_dle ) {
				if ( c == ETX ) {
					pty_advance(ph,used);
					/* FIXME: Call HDLC-queuing here */
					at_parse_reset(mh);
					return 1;
				}
				mh->have_dle = 0;
			} else if ( c == DLE ) {
				mh->have_dle = 1;
				continue;
			}

			if ( mh->auxpos < MODEMHANDLE_TMPBUFFER_SIZE )
				mh->aux_buffer[mh->auxpos++] = c;
		}

		pty_advance(ph,used);
	}
}


/* MR: AT+FTM is transmit binary sequence, fax class 1 */

int do_ftm(struct ModemHandle *mh, struct PtyHandle *ph,
	   char *prebuf, char **postbuf,mdmcmd *cmd)
{
	int rc;

	if ( !do_special(mh,ph,prebuf,postbuf,cmd,"3,72,96") )
		return 0;

	mh->function = parse_ftm_data;
	mh->auxpos = 0;
	mh->have_dle = 0;
	pty_printf(ph,"CONNECT%s",mh->crlf);
	rc = 0;

	return 0;
}



int do_frm(struct ModemHandle *mh, struct PtyHandle *ph,
	   char *prebuf,char **postbuf,mdmcmd *cmd)
{
	if ( !do_special(mh,ph,prebuf,postbuf,cmd,"3,72,96") )
		return 0;

	/* FIXME - we should start receiving here ! */
	return 0;
}

int do_frh(struct ModemHandle *mh, struct PtyHandle *ph,
	   char *prebuf,char **postbuf,mdmcmd *cmd)
{
	/* FIXME */

	unsigned char retval[]={
		25,			/* CSI */
		0xFF,0x03,0x40,0x74,
		0x73,0x65,0x74,0x20,
		0x65,0x4b,0x4b,0x20,
		0x20,0x20,0x20,0x20,
		0x20,0x20,0x20,0x20,
		0x20,0x20,0x20,0x0F,
		0x46,
		
		12,
		0xFF,0x13,0x80,0x00,
		0xEE,0xFA,0x80,0x9E,
		0xC0,0x03,0x3D,0x85,
		
		5,			/* Training success */
		0xFF,0x13,0x84,0xEA,
		0x7D,
		
		5,			/* MCF - Message confirmation */
		0xFF,0x13,0x8C,0xA2,
		0xF1,
		
		0,0,0,0,0,0,0,0,0
	};

	static int retvalpos=0;
	int retvalnum;

	if ( !do_special(mh,ph,prebuf,postbuf,cmd,"3,72,96") )
		return 0;

	pty_printf(ph,"CONNECT%s",mh->crlf);
	printf("Sending Pseudo-HDLC from %d\n",retvalpos);
	retvalnum=retval[retvalpos++];
	while(retvalnum--) {
		if (retval[retvalpos]==DLE)	/* DLE doubling */ 
			pty_write(ph,&retval[retvalpos],1);
		printf("%02x  ",(unsigned char)retval[retvalpos]);
		pty_write(ph,&retval[retvalpos++],1);
	}

	pty_write(ph,S_DLE S_ETX,2);	/* DLE ETX*/
	printf("DLE ETX\n");

	return 0;
}


/* FIXME: Needs to have a separate filler function */

int do_fth(struct ModemHandle *mh, struct PtyHandle *ph,
	   char *prebuf, char **postbuf, mdmcmd *cmd)
{
#if 0
	/* FIXME: Needs to do this properly */
	int have_dle;
	char buf;
	char bigbuffer[1024];
	int bufferpos;

	int rc;

	rc = do_fspecial(mh,ph,prebuf,postbuf,cmd,"3,72,96");
	if ( rc < 0 ) {
		have_dle=0;
		printf("Receiving HDLC:");
		do {
			printf("\nCONNECT\n");
			pty_write(ph,"CONNECT\n",8);
			bufferpos=0;
			while(1) {
				read(io,&buf,1);
				if (have_dle) {
					if (buf==DLE) {
						bigbuffer[bufferpos++]=0x10;
						printf("DLE ");
					}
					if (buf==ETX) {
						printf("ETX ");
						break;
					}
					have_dle=0;
				} else if (buf==DLE) {
					have_dle=1;
				} else {
					bigbuffer[bufferpos++]=buf;
					printf("%02x  ",buf);
				}
			}
			if (bigbuffer[1]==0x03) printf("\n more frames follow !");
			else break;
		} while(1);
		rc=0;
	}

	return rc;
#endif

	return 0;
}

int do_dial(struct ModemHandle *mh, struct PtyHandle *ph,
	    char *prebuf,char **postbuf,mdmcmd *cmd)
{
#define MAXDIALDIGITS 64
	char dialnum[MAXDIALDIGITS];
	int t;
	char c;

	t = 0;
	while ( t < (MAXDIALDIGITS-1) ) {
		c = **postbuf;
		if ( c >= '0' && c <= '9' ) {
			dialnum[t++] = c;
			*postbuf++;
			continue;
		}
		if ( c == 'w' || c == 'W' || c == ',' ) {
			*postbuf++;
			continue;
		}

		break;
	}

	dialnum[t] = '\0';

	if ( !last_command(mh,ph) )	/* Should be last command on line */
		return 1;

	if ( dialnum[0] == '\0' )	/* A simple ATD(T) should give OK */
		return 1;

	/* We have a valid ATD command; we have to test if all is well
	 * (not allready online....) and initialize the correct mode
	 * and signalling chain.
	 */

	/* FIXME: Check for online allready here */

	/* The following function initializes timers, signalling chain
	 * etc. depending on the mode of the modem.  Currently, we only
	 * opt for Fax class one support, and maybe 300 bit/s data.
	 */

	setup_call(mh);

	
	
		
			
#if 0
	/* This is a hack by becka it seems, disable it for now */
	char *dummy="+FRH=3";
	char *dummy2=dummy+4;
        mdmcmd mycmd={
        	"+FRH",-1,-1, 0, do_frh
        };
        
	mycmd.handle = &mh->mstat_current.frh;
	

	(*postbuf)+=strlen(*postbuf);	/* FIXME ! */

	return do_frh(mh,ph,dummy,&dummy2,&mycmd);

	printf("Sending CONNECT\n");
	pty_write(ph,"CONNECT\n",8);
#endif
	return 0;
}

int do_sregs(struct ModemHandle *mh, struct PtyHandle *ph,
	     char *prebuf,char **postbuf,mdmcmd *cmd)
{
	int regnum, parmnum, value;

	/* Find register number, and test if inside valid range */
	regnum = strtol(*postbuf,postbuf,10);
	parmnum = regnum + MSTAT_REGS;
	if ( regnum < 0 || regnum >= MSTAT_NUMREGS )
		return 1;

	/* Make it possible to query value */
	if ( isquery(mh,ph,postbuf,parmnum) )
		return 0;

	if ( **postbuf == '=' ) {
		(*postbuf)++;
		if ( **postbuf == '?' ) {
			(*postbuf)++;
			/* FIXME ! Need to send response here ! */
			return 0;
		}

		/* Change the value of the register */
		value = strtol(*postbuf,postbuf,10);
		mh->mstat_current.parm[parmnum] = value;
		calculate_stuff(mh);
		return 0;
	}

	return 1;	/* Not a valid S-register command. */
}

int do_hangup(struct ModemHandle *mh, struct PtyHandle *ph,
	      char *prebuf,char **postbuf,mdmcmd *cmd)
{
	/* FIXME */
	return 0;
}

int do_answer(struct ModemHandle *mh, struct PtyHandle *ph,
	      char *prebuf,char **postbuf,mdmcmd *cmd)
{
	/* FIXME */
	return 0;
}

int back_to_data(struct ModemHandle *mh, struct PtyHandle *ph,
		 char *prebuf, char **postbuf, mdmcmd *cmd)
{
	/* FIXME */
	return 0;
}

int do_reset(struct ModemHandle *mh, struct PtyHandle *ph,
	     char *prebuf, char **postbuf, mdmcmd *cmd)
{
	mh->mstat_current = mh->mstat_profile[PARM];
	return 0;
}

int store_profile(struct ModemHandle *mh, struct PtyHandle *ph,
		  char *prebuf, char **postbuf, mdmcmd *cmd)
{
	mh->mstat_profile[PARM] = mh->mstat_current;
	return 0;
}

int do_facreset(struct ModemHandle *mh, struct PtyHandle *ph,
		char *prebuf, char **postbuf, mdmcmd *cmd)
{
	setup_default(&mh->mstat_current);
	return 0;
}

int show_regs(struct ModemHandle *mh, struct PtyHandle *ph,
	      char *prebuf, char **postbuf, mdmcmd *cmd)
{
	/* FIXME */
	return 0;
}

int do_fae(struct ModemHandle *mh, struct PtyHandle *ph,
	   char *prebuf, char **postbuf, mdmcmd *cmd)
{
	/* FIXME */
	return 0;
}

int do_frs(struct ModemHandle *mh, struct PtyHandle *ph,
	   char *prebuf, char **postbuf, mdmcmd *cmd)
{
	/* FIXME */
	return 0;
}

int do_fts(struct ModemHandle *mh, struct PtyHandle *ph,
	   char *prebuf, char **postbuf, mdmcmd *cmd)
{
	/* FIXME */
	return 0;
}

int do_identify(struct ModemHandle *mh, struct PtyHandle *ph,
		char *prebuf, char **postbuf, mdmcmd *cmd)
{
	/* Simple Rockwell 28800 Modem */

	const char *idstrings[]={	/* Simple Rockwell 28800 Modem */
		"28800" ,		/* 0 */
		"255",			/* 1 */
		"",			/* 2 */
		"CRS-02 GER 950419",	/* 3 */
		"ROCKWELL  VERSION=V1.100A  MODEL=V34_DS",	/* 4 */
		"022",			/* 5 */
		"RCV288DPi Rev 0",	/* 6 */
		"000"			/* 7 */
	};

	pty_printf(ph,"%s%s",idstrings[PARM],mh->crlf);
	return 0;
}


mdmcmd mdmcmdlist[]={
	{"A",  -1,     -1, 0					,do_answer},
	{"B",   0,      1, MSTAT_BELLMODULATION			,NULL},
	{"C",   1,      1, 0 /* dummy carrierhandling */	,NULL},
	{"DT",  -1,    -1, 0					,do_dial},
	{"D",   -1,    -1, 0					,do_dial},
	{"E", 	0, 	1, MSTAT_ECHO				,NULL},
	{"F",   1,      1, 0 /* dummy nodataecho */		,NULL},
	{"H", 	0, 	1, MSTAT_OFFHOOK			,NULL},
	{"I",   0,      7, MSTAT_IDENTIFY			,do_identify},
	{"L", 	0, 	1, MSTAT_VOLUME				,NULL},
	{"M", 	0, 	1, MSTAT_SPEAKERMODE			,NULL},
	{"N",   0,      1, MSTAT_AUTOHANDSHAKE			,NULL},
	{"O",   0,      1, MSTAT_WANTRETRAIN			,back_to_data},
	{"P",  -1,     -1, MSTAT_DIALPULSE			,set_to_1},
	{"Q", 	0, 	1, MSTAT_NODTERESPONSES			,NULL},
	{"S",  -1,     -1, MSTAT_REGS				,do_sregs},
	{"T",  -1,     -1, MSTAT_DIALPULSE			,set_to_0},
	{"V", 	0, 	1, MSTAT_WORDRESPONSE			,NULL},
	{"W", 	0, 	2, MSTAT_ENHRESPONSE			,NULL},
	{"X", 	0, 	4, MSTAT_DIALDETECT			,NULL},
	{"Y", 	0, 	1, MSTAT_LONGSPACEDISCONNECT		,NULL},
	{"Z", 	0, 	1, MSTAT_PROFILE			,do_reset},
	{"&C", 	0, 	1, MSTAT_DCDBEHAVIOUR			,NULL},
	{"&D", 	0, 	3, MSTAT_DTRBEHAVIOUR			,NULL},
	{"&F", -1,     -1, 0					,do_facreset},
	{"&G",  0,      2, 0	/* dummy */			,NULL},
	{"&J",  0,      1, 0	/* dummy */			,NULL},
	{"&K", 	0, 	5, MSTAT_FLOWCONTROL		,check_andK_allowed},
	{"&M", 	0, 	3, MSTAT_NEGOTIATION			,NULL},
	{"&P", 	0, 	2, MSTAT_MAKEBREAK			,NULL},
	{"&Q", 	0, 	9, MSTAT_NEGOTIATION			,NULL},
	{"&R", 	0, 	1, MSTAT_FORCECTS			,NULL},
	{"&S", 	0, 	1, MSTAT_FORCEDSR			,NULL},
	{"&T", 	0, 	8, 0	/* Loopback test */		,NULL},
	{"&V", -1,     -1, 0	/* Show resg */			,show_regs},
	{"&W",  0,      1, MSTAT_PROFILE		,store_profile},
	{"&X", 	0, 	2, 0	/* EIA clock handling */	,NULL},
	{"&Y", 	0, 	1, 0	/* active profile */		,NULL},
#if 0
 FIXME	{"&Z", 	0, 	1, 0	/* store phone number */	,NULL},
#endif
	{"+FAE",-1,    -1, MSTAT_FAE				,do_fae},
	{"+FCLASS",-1, -1, MSTAT_FAXCLASS			,do_fclass},
	{"+FRH",-1,    -1, MSTAT_FRH				,do_frh},
	{"+FRM",-1,    -1, MSTAT_FRM				,do_frm},
	{"+FRS",-1,    -1, MSTAT_FRS				,do_frs},
	{"+FTH",-1,    -1, MSTAT_FTH				,do_fth},
	{"+FTM",-1,    -1, MSTAT_FTM				,do_ftm},
	{"+FTS",-1,    -1, MSTAT_FTS				,do_fts},
	{NULL, -1,     -1, 0	/* End of cmd list marker */	,NULL}
};


/* Read a line from the pty - ie. an AT-command line, terminated with
 * CR, LF, CR+LF or whatever.  Return 1 if a complete line has been read
 * into the ModemHandle buffer (mh->at_parse_buffer).
 */

int read_line_ok(struct ModemHandle *mh, struct PtyHandle *ph)
{
	size_t size, used;
	ifax_uint8 *start, *walk, c;

	for (;;) {
		
		pty_readbuffer(ph,&start,&size);
		if ( size == 0 )
			return 0;
		
		used = 0;
		walk = start;

		while ( size > 0 ) {

			c = *walk++;
			used++;
			size--;

			if ( c == '\r' || c == '\n' ) {

				if ( mh->mstat_current.parm[MSTAT_ECHO] ) {
					pty_write(ph,start,used);
					pty_printf(ph,"%s",mh->crlf);
				}

				mh->at_command_buffer[mh->readpos] = '\0';
				mh->parseptr =
					(char *)&mh->at_command_buffer[0];
				pty_advance(ph,used);

				return 1;
			}
		
			if ( mh->readpos < MODEMHANDLE_TMPBUFFER_SIZE )
				mh->at_command_buffer[mh->readpos++] = c;
		}

		pty_write(ph,start,used);
		pty_advance(ph,used);
	}

	return 0;
}


/* Parse and execute the next AT command in the command-buffer in
 * the ModemHandle structure.  The 'parseptr' pointer is used to determine
 * how far along the command line we have gotten.
 *
 * Return values:
 *          0   Ok
 *    nonzero   An error has occured
 */

static int exec_at_command(struct ModemHandle *mh, struct PtyHandle *ph)
{
	int cmdlen;
	char *start;
	mdmcmd *cmd;

	for ( cmd = mdmcmdlist; cmd->cmd; cmd++ ) {

		cmdlen = strlen(cmd->cmd);
		if ( strncasecmp(mh->parseptr,cmd->cmd,cmdlen) )
			continue;

		start = mh->parseptr;
		mh->parseptr += cmdlen;

		if ( cmd->parmmax > 0 ) {
			/* Parse numeric parameter */
			PARM = strtol(mh->parseptr,&mh->parseptr,10);
			if ( PARM < cmd->parmmin || PARM > cmd->parmmax )
				return 1;
		}

		if ( cmd->handler )
			return (*cmd->handler)(mh,ph,start,&mh->parseptr,cmd);

		return 0;
	}

	return 1;    /* FIXME: What return-values are defined/used? */
}


/* Line-disipline function for parsing AT-commands */

static int parse_at_commands(struct ModemHandle *mh, struct PtyHandle *ph)
{
	ifax_uint8 c;

	/* We may have to read a line of input before parsing it */
	if ( mh->at_read_line_mode ) {
		if ( !read_line_ok(mh,ph) )
			return 0;

		/* We have a fresh command line, check if it starts with AT */
		mh->at_read_line_mode = 0;

		while ( (c = *mh->parseptr) && isspace(c) )
			mh->parseptr++;

		if ( *mh->parseptr == '\0' ) {
			/* Empty command line, try read another one */
			at_parse_reset(mh);
			return 1;
		}

		/* Expect first command to start with 'AT' */
		if ( strncasecmp(mh->parseptr,"AT",2) ) {
			at_bad_error(mh,ph);
			return 1;
		}

		/* We have 'AT' command(s), parse it */
		mh->parseptr += 2;
	}

	if ( 0 ) {
		/* For 2nd and subsequent commands, we need a space first */
		if ( (c = *mh->parseptr) && !isspace(c) ) {
			at_bad_error(mh,ph);
			return 1;
		}
	}
	
	while ( (c = *mh->parseptr) && isspace(c) )
		mh->parseptr++;

	while ( *mh->parseptr != '\0' ) {
		printf("Parsing at: '%s'\n",mh->parseptr);
		if ( exec_at_command(mh,ph) ) {
			at_bad_error(mh,ph);
			return 1;
		}
		return 1;	/* Come back soon */
	}

	/* Parsed right up to the end of the command line, and with
	 * no 'at_bad_error' resetting parsing - so all should be well.
	 */

	pty_printf(ph,"OK%s",mh->crlf);
	at_parse_reset(mh);

	return 1;
}


/* Call this function to feed the modem with data from the application.
 * The data will be acted upon depending on how the modem is currently
 * operating (Parse AT-command when in command-mode etc.)
 * The line-disipline function called is responsible for advancing the
 * pty for all the bytes that it "swallows".
 *
 * The return values of the line-disipline functions are:
 *
 *       0  No use calling again unless more data is available or
 *          some other event has happened (call me again soon).
 *
 *       1  There is (probably) more that needs to be done right
 *          away, so please call me again immediately.
 */

void modeminput(struct ModemHandle *mh, struct PtyHandle *ph)
{
	while ( (*mh->function)(mh,ph) )
	   ;
}
