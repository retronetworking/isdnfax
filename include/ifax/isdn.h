/* $Id$
 * Isdn abstraction layer
 * (c) 1998 Morten Rolland
 */
   
extern int IsdnCommand(int, char *,int,int);
extern void IsdnReadLine(int, char *, int);
extern int IsdnOpenDevice(char *);
extern void IsdnSendAudio(int, unsigned char *, int);
extern void IsdnReadAudio(int, unsigned char *, int);
extern char cmdresult[];
extern char isdnerr[];
