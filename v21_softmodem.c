/* $Id$
******************************************************************************

   Fax program for ISDN.
   V21 Softmodem implementation.

   Copyright (C) 1998 Andreas Beck   [becka@ggi-project.org]
   Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <ifax/ifax.h>

/* Prototypes for all the modules. We should probably move them to a big
 * bunch of .h files.
 */
int     send_to_audio_construct(ifax_modp self,va_list args);
int     pulsegen_construct(ifax_modp self,va_list args);
int     sinegen_construct(ifax_modp self,va_list args);
int     scrambler_construct(ifax_modp self, va_list args);
int     modulator_V29_construct(ifax_modp self, va_list args);
int     fskdemod_construct(ifax_modp self, va_list args);
int     fskmod_construct(ifax_modp self, va_list args);
int     decode_serial_construct(ifax_modp self, va_list args);
int     encode_serial_construct(ifax_modp self, va_list args);
int     debug_construct(ifax_modp self, va_list args);
#include <ifax/modules/replicate.h>

/* Module-IDs for all the modules. Needed for instantiating them.
 */
ifax_module_id	IFAX_TOAUDIO;
ifax_module_id	IFAX_REPLICATE;
ifax_module_id  IFAX_FSKDEMOD;
ifax_module_id  IFAX_FSKMOD;
ifax_module_id  IFAX_DECODE_SERIAL;
ifax_module_id  IFAX_ENCODE_SERIAL;
ifax_module_id  IFAX_DEBUG;

/* Register all needed modules.
 */
static void setup_all_modules(void)
{
	IFAX_TOAUDIO	  = ifax_register_module_class("Audio output",send_to_audio_construct);
	IFAX_REPLICATE	  = ifax_register_module_class("Replicator",replicate_construct);
	IFAX_FSKDEMOD	  = ifax_register_module_class("FSK demodulator",fskdemod_construct);
	IFAX_FSKMOD	  = ifax_register_module_class("FSK modulator",fskmod_construct);
	IFAX_DECODE_SERIAL= ifax_register_module_class("Serializer",decode_serial_construct);
	IFAX_ENCODE_SERIAL= ifax_register_module_class("Serial encoder",encode_serial_construct);
	IFAX_DEBUG        = ifax_register_module_class("Debugger",debug_construct);
}

/* Handles for the ISDN device, and the in- and output.
 */
static int isdnhandle=-1;
static int inputhandle=0;
static int outputhandle=1;

enum MODEM_MODE { ORIGINATE, ANSWER };

/* The actual "modem" code.
 */
static void test_v21(enum MODEM_MODE direction,int outfd)
{
	/* module handles for all used modules.
	 */
	ifax_modp	fskd,fskenc,toisdn,totty,deserial,enserial;

	/* helper for data in-/output
	 */
	unsigned char data;

	/* helper to avoid overflowing the send buffer.
	 */
	int wait;
	
	/* The lines below are commented out. They were once used 
	 * for debugging.
	 */
#if 0
	ifax_modp	toaudio,debug,replicate;

	/* debugger */
	debug=ifax_create_module(IFAX_DEBUG,1);

	/* Replicate the incoming signal. */
	replicate=ifax_create_module(IFAX_REPLICATE);
//	ifax_command(replicate,CMD_REPLICATE_ADD,toisdn);
#endif

	/* Output wave to ISDN 
	 */
	toisdn=ifax_create_module(IFAX_TOAUDIO, IsdnSendAudio, isdnhandle);

	/* The FSK modulator. Its output is send to the ISDN line.
	 */
	switch(direction)
	{
		case ORIGINATE:
			fskenc=ifax_create_module(IFAX_FSKMOD,980,1180);
			break;
		case ANSWER:
			fskenc=ifax_create_module(IFAX_FSKMOD,1650,1850);
			break;
	}
	fskenc->sendto=toisdn;

	/* The serializer. Its output is sent to the FSK modulator.
	 * These three modules make up the sender.
	 */
	enserial=ifax_create_module(IFAX_ENCODE_SERIAL,300,"8N1");
	enserial->sendto=fskenc;

	/* Now for the receiver. When all is decoded, the text is sent
	 * to the outputhandle.
	 */
	totty=ifax_create_module(IFAX_TOAUDIO, write, outputhandle);

	/* The deserializer synchronizes on the startbits and decodes
	 * from the 0/1 stream from the demodulator to the bytes.
	 * Its output is sent to the totty module. Seee above.
	 */
	deserial=ifax_create_module(IFAX_DECODE_SERIAL,300,"8N1");
	deserial->sendto=totty;

	/* The FSK demodulator. Takes the aLaw input stream and sends the
	 * decoded version to the deserializer. See above. 
	 */
	switch(direction)
	{
		case ORIGINATE:
			fskd=ifax_create_module(IFAX_FSKDEMOD,1650,1850,300);
			break;
		case ANSWER:
			fskd=ifax_create_module(IFAX_FSKDEMOD,980,1180,300);
			break;
	}
	fskd->sendto=deserial;

	/* The first 2 seconds are silence to allow for finding the
	 * noise level.
	 * No output is made until the "wait" variable is down to zero.
	 */
	wait=4*SAMPLES_PER_SECOND;	/* 2s steady carrier ! */

	while(wait--)
	{
		/* Read data from isdn4linux.
		 */
		IsdnReadAudio(isdnhandle,&data,1);
		/* Log data if requested.
		 */
		if (outfd!=-1) write(outfd,&data,1);

		/* Send back a steady 0
		 */
		data=int2alaw(0);
		IsdnSendAudio(isdnhandle,&data,1);
	}

	/* The first two seconds are reserved to send a steady carrier.
	 * No output is made until the "wait" variable is down to zero.
	 */
	wait=2*SAMPLES_PER_SECOND;	/* 2s steady carrier ! */

	/* Run until explicitly terminated.
	 */
	while(1) {
		/* helpers for select()ing on input.
		 */
		struct timeval tv={0,0};
		fd_set rfds;
		
		/* Count down wait.
		 */
		if (wait) wait--;

		/* Prepare for select()ing on input. If wait() is still 
		 * active, don't ask for input.
		 */
		FD_ZERO(&rfds);
		FD_SET(inputhandle,&rfds);
		if (!wait && select(inputhandle+1,&rfds,NULL,NULL,&tv)>0)
		{
			/* Send the received data.
			 */
			ifax_handle_input(enserial,&data,
					  read(inputhandle,&data,1));

			/* Wait for 11 bits (plus a little extra space).
			 * Avoid overflowing the send buffer.
			 * FIXME. We should rather check the return code.
			 */
			wait=11*SAMPLES_PER_SECOND/300+100;
		} else
			/* Send a dummy packet, to keep the sender going.
			 */
			ifax_handle_input(enserial,&data,0);

		/* Read data from isdn4linux.
		 */
		IsdnReadAudio(isdnhandle,&data,1);
		/* Log data if requested.
		 */
		if (outfd!=-1) write(outfd,&data,1);

		/* Send it to the demodulator.
		 */
		ifax_handle_input(fskd,&data,1);
	}
}

/* A function that more or less implements the "ATZ" function of a modem.
 */
void setup_isdn(char *sourcemsn)
{
	char cmdbuffer[128];

	/* Reset the modem
	 */
	sprintf(cmdbuffer,"ATZ");
	IsdnCommand(isdnhandle,cmdbuffer,1,1);

	/* Set up the MSN.
	 */
	sprintf(cmdbuffer,"AT&E%s",sourcemsn);
	IsdnCommand(isdnhandle,cmdbuffer,1,1);

	/* Go to VOICE mode.
	 */
	IsdnCommand(isdnhandle,"AT+FCLASS=8",1,1);

	/* Set the data format to aLaw.
	 */
	IsdnCommand(isdnhandle,"AT+VSM=5",1,1);

	/* Set the service identifier to "audio".
	 */
	IsdnCommand(isdnhandle,"ATS18=1",1,1);	/* service == audio */

	/* Set layer 2 to "audio".
	 */
	IsdnCommand(isdnhandle,"ATS14=4",1,1);	/* layer 2 == audio */

	/* Set the data format to aLaw, and the device to "phone-line".
	 */
//	IsdnCommand(isdnhandle,"AT+VSM=5+VLS=2",1,1);
}

/* A function that more or less implements the "ATDxxx" function of a modem.
 */
void dial_isdn(char *number)
{
	char cmdbuffer[128];

	/* Dial the given number.
	 */
	sprintf(cmdbuffer,"ATD%s",number);
	IsdnCommand(isdnhandle,cmdbuffer,1,1);

	/* Set up the packet size.
	 */
//	IsdnCommand(isdnhandle,"ATS16=48",1,1);	/* Sendpacketsize/16 WHY ??? */

	/* Start full duplex audio transmission.
	 */
	IsdnCommand(isdnhandle,"AT+VTX+VRX",1,0);
}

/* A function that more or less implements the "ATA" function of a modem.
 */
void answer_isdn(void)
{
//	/* Set up the packet size.
//	 */
//	IsdnCommand(isdnhandle,"ATS16=48",1,1);	/* Sendpacketsize/16 WHY ??? */

	/* Answer the pending call - hope there is one ...
	 */
	IsdnCommand(isdnhandle,"ATA",1,1);

//	/* Set the data format to aLaw, and the device to "phone-line".
//	 */
//	IsdnCommand(isdnhandle,"AT+VSM=5+VLS=2",1,1);

	/* Start full duplex audio transmission.
	 */
	IsdnCommand(isdnhandle,"AT+VTX+VRX",1,0);
}

/* Print out how to use this program.
 */
void usage(char *prgnam)
{
	printf("Usage: %s [-i isdndevice]\n"
	       "       -m MSN \n"
	       "       [-d number|'answer']\n"
	       "       [-l logfile]\n",prgnam);	
}

/* Main program. Get the arguments and set everything up.
 * It _should_ then open a pty and emulate a modem there.
 * This is not yet implemented. It just calls out and connects.
 */
void main(int argc,char **argv)
{
	/* Remember the character we get from getopts(). 
	 */
	int optchr;

	/* Defaults for the options. We cannot guess an MSN, so this is
	 * an error, if it remains unset.
	 */
	char *isdndevname="/dev/ttyI1";
	char *numbertodial=NULL;
	char *sourcemsn=NULL;
	int outdata=-1;

	/* Register all the modules.
	 */
	setup_all_modules();

	/* Get all options from the commandline.
	 */
	while(EOF!=(optchr=getopt(argc,argv,"i:m:d:l:")))
	{
		switch(optchr)
		{
			case 'i': 
				/* Device name for IsdnOpenDevice.
				 */
				isdndevname=optarg;
				break;
			case 'm':
				/* The MSN to use as source-MSN. Or for
				 * incoming calls, the one to listen for.
				 */
				sourcemsn=optarg; 
				break;
			case 'd':
				/* The number to dial.
				 */
				numbertodial=optarg; 
				break;
			case 'l':
				/* The logfile name.
				 */
				outdata=open(optarg,O_WRONLY|O_CREAT|O_TRUNC,0644);
				break;
		}
	}

	/* We cannot guess an MSN.
	 */
	if (!sourcemsn) {
		usage(argv[0]);
		exit(1);
	}

	/* Open the ISDN device 
	 */
	if (-1==(isdnhandle=IsdnOpenDevice(isdndevname)))
	{
		fprintf(stderr,"%s: cannot open isdn-device %s.\n",argv[0],isdndevname);
		exit(1);
	}

	/* Set it up.
	 */
	setup_isdn(sourcemsn);

	if (strcmp(numbertodial,"answer")==0)
	{
		/* Wait for the remote station.
		 */
		while(1)
		{
			char buffer[256];
			IsdnReadLine(isdnhandle,buffer,sizeof(buffer));
			printf("Readline:%s\n",buffer);
			if (strncmp(buffer,"RING",4)==0) break;
		}
		answer_isdn();
		/* Go to transfer mode.
		 */
		test_v21(ANSWER,outdata);
	}
	else
	{
		/* Call the remote station.
		 */
		dial_isdn(numbertodial);
		/* Go to transfer mode.
		 */
		test_v21(ORIGINATE,outdata);
	}


	/* Clean up and exit.
	 */
	if (outdata!=-1) {
		close(outdata);
	}
	close(isdnhandle);
}
