/* isdn.c
 *
 * Implement all functions needed to talk to the ISDN-card and send and
 * receive audio.
 */

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ifax/ifax.h>

#define CMDRESULTSIZE 128
#define CMDMAXSIZE    128

char isdnerr[128];

static char cmdresult[CMDRESULTSIZE+2];

/* Open the Isdn-device and prepare for audio-transmission */

int IsdnOpenDevice(char *device)
{
	int isdnfd;
	struct termios isdnsetting;

	if ( (isdnfd=open(device,O_RDWR)) < 0 ) {
		sprintf(isdnerr,"Unable to open '%s'",device);
		return -1;
	}

#if 0
	/* What is this?  Is it needed? */
	if ( fcntl(isdnfd, F_SETFL, O_RDWR) < 0 ) {
		sprintf(isdnerr,"...");
		close(isdnfd);
		return -1;
	}
#endif

	if ( tcgetattr(isdnfd,&isdnsetting) < 0 ) {
		sprintf(isdnerr,"Unable to read terminal settings for '%s'",device);
		close(isdnfd);
		return -1;
	}

	isdnsetting.c_iflag = 0;
	isdnsetting.c_oflag = 0;
	isdnsetting.c_lflag = 0;

	isdnsetting.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CLOCAL);
	isdnsetting.c_cflag |=  (CS8 | CREAD | HUPCL | CLOCAL | CRTSCTS);

	if ( tcsetattr(isdnfd, TCSANOW, &isdnsetting) < 0 ) {
		sprintf(isdnerr,"Unable to set terminal settings for '%s'",device);
		close(isdnfd);
		return -1;
	}

	return isdnfd;
}

void IsdnPrintChar(char *what, char c)
{
  char tmp[32];

  if ( c < 0x20 || c > 0x7e ) {
    sprintf(tmp,"%02x",(int)c);
  } else {
    tmp[0] = c;
    tmp[1] = 0;
  }

  if ( c == '\r' ) strcpy(tmp,"'\\r'");
  if ( c == '\n' ) strcpy(tmp,"'\\n'");

  fprintf(stderr,"<%s: %s>\n",what,tmp);
}


void IsdnReadLine(int isdnfd, char *buf,int size)
{
  int t, r, skip, skipcrlf;

  skipcrlf = 1;

  fprintf(stderr,"---------------------------------------------\n");

  for ( t=0; t < size; t++ ) {
    r = read(isdnfd,&buf[t],1);
    if ( r != 1 ) {
      fprintf(stderr,"Read audio-data errno: %d\n",errno);
      sprintf(isdnerr,"Can't read device one byte at a time");
      buf[t] = 0;
      return;
    }

    /* IsdnPrintChar("Read",buf[t]); */

    if ( (buf[t] == '\n' || buf[t] == '\r') && skipcrlf ) {
      skip = 1;
      skipcrlf = 0;
    }

    if ( buf[t] != '\n' && buf[t] != '\r' ) {
      skipcrlf = 0;
      skip = 0;
    }

    if ( skip ) {
      t--;
    } else if ( buf[t] == '\r' ) {
	buf[t] = 0;
	return;
    }
  }
  buf[t+1] = 0;
}

int IsdnCommand(int isdnfd, char *cmd, int checho, int result)
{
  char localcmd[CMDMAXSIZE+4];
  int cmdlen;

  strcpy(localcmd,cmd);
  cmdlen = strlen(localcmd);

  if ( cmdlen > 0 && localcmd[cmdlen-1] != '\r' ) {
    strcat(localcmd,"\r");
    cmdlen = strlen(localcmd);
  }

  if ( cmdlen == 0 ) {
    return 0;
  }

  if ( write(isdnfd,localcmd,cmdlen) != cmdlen ) {
    sprintf(isdnerr,"Unable to write command to device");
    return 0;
  }

  if ( checho ) {
    IsdnReadLine(isdnfd, cmdresult,CMDRESULTSIZE);

    localcmd[cmdlen-1] = 0;
    if ( strcmp(cmdresult,localcmd) ) {
      sprintf(isdnerr,"Command '%s' echoed back as '%s'",localcmd,cmdresult);
      return 0;
    }
  }

  if ( result ) {
    IsdnReadLine(isdnfd, cmdresult,CMDRESULTSIZE);
    fprintf(stderr,"Command '%s' resulted in '%s'\n",localcmd,cmdresult);
  }

  return 1;
}

/* ISDN4Linux has the uncommon behaviour of expecting _bitreversed_ aLaw.
 * We compensate for that.
 */
void IsdnSendAudio(int isdnfd, unsigned char *data, int size)
{
	int t;
	unsigned char dathelp;

	for ( t=0; t < size; t++ ) 
	{
		dathelp=bitrev(data[t]);  
		if ( dathelp == 0x10 ) 
		{
			write(isdnfd,&dathelp,1);
		}
		write(isdnfd,&dathelp,1);
	}
}

/* ISDN4Linux has the uncommon behaviour of expecting _bitreversed_ aLaw.
 * We compensate for that.
 */
void IsdnReadAudio(int isdnfd, unsigned char *data, int size)
{
	int t;

	for ( t=0; t < size; t++ ) 
	{
		data[t] = '@';
		if ( read(isdnfd,&data[t],1) != 1 ) 
		{
			fprintf(stderr,"Oops: %d\n",errno);
		}
		if ( data[t] == 0x10 ) 
		{
			read(isdnfd,&data[t],1);
			if ( data[t] != 0x10 ) 
			{
				fprintf(stderr,"<%02x>\n",data[t]);
			}
		}
		data[t]=bitrev(data[t]);
	}
}
