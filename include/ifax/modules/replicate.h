/* $Id$
 *
 * Replicator module.
 *
 * (C) 1998 Andreas Beck 	<becka@ggi-project.org>
 */

#define CMD_REPLICATE_ADD	0x01000000
#define CMD_REPLICATE_DEL	0x01000001

int	replicate_construct(ifax_modp self,va_list args);
