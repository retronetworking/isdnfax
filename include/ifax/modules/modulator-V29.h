/* $Id$
 *
 * V.29 modulator module.
 *
 * Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
 * Copyright (C) 1998 Oliver Eichler [Oliver.Eichler@regensburg.netsurf.de]
 */

#define CMD_MODULATORV29_STARTSYNC 0x01
#define CMD_MODULATORV29_MAINTAIN  0x02

#define MODULATORV29_STATUS_SEGMENT1        0x00000001
#define MODULATORV29_STATUS_SEGMENT2        0x00000002
#define MODULATORV29_STATUS_SEGMENT3        0x00000004
#define MODULATORV29_STATUS_SEGMENT4        0x00000008
#define MODULATORV29_STATUS_SEGMENT1234     0x0000000f
#define MODULATORV29_STATUS_DATA            0x00000010

int modulator_V29_construct(ifax_modp self, va_list args);
