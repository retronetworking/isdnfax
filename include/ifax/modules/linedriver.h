/* $Id$
 *
 * Linedriver module.
 *
 * Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
 */

#define CMD_LINEDRIVER_WORK       0x01
#define CMD_LINEDRIVER_AUDIO      0x02
#define CMD_LINEDRIVER_HARDWARE   0x03
#define CMD_LINEDRIVER_LOOPBACK   0x04
#define CMD_LINEDRIVER_RECORD     0x05

int linedriver_construct(ifax_modp self, va_list args);
