/* $Id$
******************************************************************************

   Fax program for ISDN.
   Read the configuration file.

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

/* Open and read the configuration file, parse it, and set
 * the relevnt variables accordingly.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <ifax/misc/readconfig.h>
#include <ifax/misc/globals.h>


#ifndef DEFAULT_CONFIG_FILE
#define DEFAULT_CONFIG_FILE "/etc/amodemd.conf"
#endif

#ifndef DEFAULT_PID_FILE
#define DEFAULT_PID_FILE "/var/run/amodemd.pid"
#endif

#ifndef DEFAULT_ISDN_DEVICE
#define DEFAULT_ISDN_DEVICE "/dev/ttyI5"
#endif

#ifndef DEFAULT_WATCHDOG_TIMEOUT
#define DEFAULT_WATCHDOG_TIMEOUT 0
#endif

#ifndef DEFAULT_REALTIME_PRIORITY
#define DEFAULT_REALTIME_PRIORITY 0
#endif

#ifndef DEFAULT_ISDN_MSN
#define DEFAULT_ISDN_MSN "5551234"
#endif

#ifndef DEFAULT_COUNTRY
#define DEFAULT_COUNTRY "47"
#endif

#ifndef DEFAULT_INT_PREFIX
#define DEFAULT_INT_PREFIX "00"
#endif

char *config_file = DEFAULT_CONFIG_FILE;
char *pid_file = DEFAULT_PID_FILE;
char *isdn_device = DEFAULT_ISDN_DEVICE;
char *isdn_msn = DEFAULT_ISDN_MSN;
char *subscriber_id = "";
char *home_country = DEFAULT_COUNTRY;
char *int_prefix = DEFAULT_INT_PREFIX;
int watchdog_timeout = DEFAULT_WATCHDOG_TIMEOUT;
int realtime_priority = DEFAULT_REALTIME_PRIORITY;
int do_lock_memory = 1;


#define MAXCFGLINE 128

static int cmd_match(char *cmd, char **pp)
{
  char *c, *p;

  c = cmd;
  p = *pp;

  while ( *c && *p ) {
    if ( tolower(*c) != tolower(*p) )
      return 0;
    c++, p++;
  }

  if ( *c )
    return 0;

  *pp = p;

  return 1;
}

static void skip_assignment(char **pp)
{
  char *p = *pp;

  while ( isspace(*p) )
    p++;

  if ( *p++ != '=' ) {
    fprintf(stderr,"%s: Missing assigment operator in config file\n",progname);
    exit(1);
  }

  while ( isspace(*p) )
    p++;

  *pp = p;
}

static int get_int(char **pp)
{
  int t;
  char buffer[32];
  char *p = *pp;

  if ( !isdigit(*p) ) {
    fprintf(stderr,"%s: Bad integer in config file\n",progname);
    exit(1);
  }

  for ( t=0; t < 30; t++ ) {
    if ( !isdigit(*p) ) {
      buffer[t] = 0;
      *pp = p;
      return atoi(buffer);
    }
    buffer[t] = *p++;
  }

  fprintf(stderr,"%s: Integer too long in config file\n",progname);
  exit(1);
}

static char *get_string(char **pp)
{
  int length;
  char *string;
  char *p = *pp, *start, *stop;

  while ( isspace(*p) )
    p++;

  if ( *p == '"' ) {
    start = p + 1;
    stop = index(start,'"');
    if ( stop == 0 ) {
      fprintf(stderr,"%s: Missing terminating \" in string in config file\n",
	      progname);
      exit(1);
    }
    *pp = stop + 1;
  } else {
    start = p;
    while ( !isspace(*p) )
      p++;
    *pp = stop = p;
  }

  length = stop - start;
  string = malloc(length+1);
  strncpy(string,start,length);
  string[length] = 0;

  return string;
}

static void end_line(char **pp)
{
  char *p = *pp;

  while ( isspace(*p) )
    p++;
  if ( *p == '#' || *p == 0 )
    return;

  fprintf(stderr,"%s: Garbage at end of line in config file\n",progname);
  exit(1);
}

void read_configuration_file(void)
{
  FILE *cfg;
  char *p, *tmp;
  char buffer[MAXCFGLINE+4];

  if ( (cfg=fopen(config_file,"r")) == 0 ) {
    fprintf(stderr,"%s: Unable to read config file '%s'\n",
	    progname,config_file);
    exit(1);
  }

  for (;;) {

    if ( fgets(buffer,MAXCFGLINE,cfg) == 0 )
      break;

    p = buffer;

    while ( isspace(*p) )
      p++;

    if ( *p == '#' )
      continue;

    if ( cmd_match("pidfile",&p) ) {
      skip_assignment(&p);
      pid_file = get_string(&p);
      end_line(&p);
      continue;
    }

    if ( cmd_match("isdn-device",&p) ) {
      skip_assignment(&p);
      isdn_device = get_string(&p);
      end_line(&p);
      continue;
    }

    if ( cmd_match("isdn-msn",&p) ) {
      skip_assignment(&p);
      isdn_msn = get_string(&p);
      end_line(&p);
      continue;
    }

    if ( cmd_match("country",&p) ) {
      skip_assignment(&p);
      home_country = get_string(&p);
      end_line(&p);
      continue;
    }

    if ( cmd_match("international-prefix",&p) ) {
      skip_assignment(&p);
      int_prefix = get_string(&p);
      end_line(&p);
      continue;
    }

    if ( cmd_match("watchdog-timer",&p) ) {
      skip_assignment(&p);
      watchdog_timeout = get_int(&p);
      end_line(&p);
      if ( watchdog_timeout < 0 ) {
	fprintf(stderr,"%s: Watchdog timer must be positive or zero\n",
		progname);
	exit(1);
      }
      continue;
    }

    if ( cmd_match("realtime-priority",&p) ) {
      skip_assignment(&p);
      realtime_priority = get_int(&p);
      end_line(&p);
      if ( realtime_priority < 0 || realtime_priority > 99 ) {
	fprintf(stderr,"%s: Real-Time priority must be in range 0-99\n",
		progname);
	exit(1);
      }
      continue;
    }

    if ( cmd_match("lock-memory",&p) ) {
      skip_assignment(&p);
      do_lock_memory = get_int(&p);
      end_line(&p);
      if ( do_lock_memory < 0 || do_lock_memory > 1 ) {
	fprintf(stderr,"%s: 'lock-memory' must be 0 or 1 in config file\n",
		progname);
	exit(1);
      }
      continue;
    }

    if ( cmd_match("subscriber-identification",&p) ) {
      skip_assignment(&p);
      subscriber_id = get_string(&p);
      end_line(&p);
      if ( strlen(subscriber_id) > 20 ) {
	fprintf(stderr,"%s: subscriber-identification more than 20 chars\n",
		progname);
	exit(1);
      }
      tmp = subscriber_id;
      while ( *tmp ) {
	if ( *tmp != '+' && *tmp != ' ' && (*tmp < '0' || *tmp > '9') ) {
	  fprintf(stderr,"%s: subscriber-identification illegal char '%c'\n",
		  progname,*tmp);
	  exit(1);
	}
	tmp++;
      }
      continue;
    }
  }

  fclose(cfg);
}
