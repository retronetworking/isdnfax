/* scrambler17.h
 *
 * Scramble/descramble a bitstream using a polynomial of
 *
 *     1 + x-18 + x-23
 *
 * As defined by the V.17 modulation standard.
 *
 * (C) 1998 Morten Rolland
 */

extern void scramble17(unsigned char *, unsigned char *, unsigned int *, int);
