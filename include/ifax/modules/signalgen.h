/* $Id$
 *
 * Simple signal generator.
 *
 * Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
 */

#define CMD_SIGNALGEN_SINUS       0x01
#define CMD_SIGNALGEN_RNDBITS     0x02

int signalgen_construct(ifax_modp self, va_list args);
