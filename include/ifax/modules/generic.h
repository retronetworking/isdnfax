/* $Id$
 *
 * Defines for generic commands that may be implemented in several
 * modules.  No other modules should allocate commands in the
 * range set aside for the generic commands (0xFF420000 -> 0xFF42FFFF).
 *
 * Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
 */


#define CMD_GENERIC_INITIALIZE              0xFF420001
#define CMD_GENERIC_SCRAMBLEONES            0xFF420002
#define CMD_GENERIC_STARTPAYLOAD            0xFF420003
