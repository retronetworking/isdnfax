/* $Id$
 *
 * Scrambler module.
 *
 * Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
 */

#define CMD_SCRAMBLER_INIT              0x01
#define CMD_SCRAMBLER_SCRAM_V29         0x02
#define CMD_SCRAMBLER_DESCR_V29         0x03

int scrambler_construct(ifax_modp self, va_list args);
