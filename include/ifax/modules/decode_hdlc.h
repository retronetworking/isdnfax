/* $Id$
 *
 * HDLC decoder. Including CRC check.
 *
 * Copyright (C) 1998 Andreas Beck [becka@ggi-project.org]
 */

#define HDLC_FLAG   (0x100)
#define HDLC_CRC_OK  (0x200)
#define HDLC_CRC_ERR (0x201)

int decode_hdlc_construct(ifax_modp self, va_list args);
