/*
 * Main include file for ISDNfax.
 * (c) 1998 Andreas Beck        <becka@ggi-project.org>
 */

/* 
 * ISDN sample rate. We include that in case someone want to use it for
 * a different purpose like faxing with a soundcard.
 */

#define SAMPLES_PER_SECOND 8000

/*
 * Include all other files here
 */

#include <ifax/types.h>
#include <ifax/debug.h>
#include <ifax/module.h>
#include <ifax/bitreverse.h>
#include <ifax/sincos.h>
#include <ifax/int2alaw.h>
#include <ifax/isdn.h>
#include <ifax/g711.h>
#include <ifax/atan.h>



#define abs(x) \
			x = x>0 ? x : -x


