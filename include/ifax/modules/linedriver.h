/* $Id$
 *
 * Linedriver module.
 *
 * Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
 */

#define CMD_LINEDRIVER_WORK       0x01
#define CMD_LINEDRIVER_AUDIO      0x02
#define CMD_LINEDRIVER_FILE		 0x03
#define CMD_LINEDRIVER_CLOSE		 0x04

int linedriver_construct(ifax_modp self, va_list args);
