/*
 *			X D R . C
 *
 *  Routines to implement an external data representation (XDR)
 *  compatible with the usual InterNet standards, e.g.:
 *  big-endian, twos-compliment fixed point, and IEEE floating point.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif



#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"


/*
 *  Routines to insert/extract short/long's into char arrays,
 *  independend of machine byte order and word-alignment.
 *  Uses encoding compatible with routines found in libpkg,
 *  and BSD system routines ntohl(), ntons(), ntohl(), ntohs().
 */

/*
 *			B U _ G S H O R T
 */
unsigned short
bu_gshort(msgp)
CONST unsigned char *msgp;
{
	register CONST unsigned char *p = msgp;
#ifdef vax
	/*
	 * vax compiler doesn't put shorts in registers
	 */
	register unsigned long u;
#else
	register unsigned short u;
#endif

	u = *p++ << 8;
	return ((unsigned short)(u | *p));
}

/*
 *			B U _ G L O N G
 */
unsigned long
bu_glong(msgp)
CONST unsigned char *msgp;
{
	register CONST unsigned char *p = msgp;
	register unsigned long u;

	u = *p++; u <<= 8;
	u |= *p++; u <<= 8;
	u |= *p++; u <<= 8;
	return (u | *p);
}

/*
 *			B U _ P S H O R T
 */
unsigned char *
bu_pshort(msgp, s)
register unsigned char *msgp;
register int s;
{

	msgp[1] = s;
	msgp[0] = s >> 8;
	return(msgp+2);
}

/*
 *			B U _ P L O N G
 */
unsigned char *
bu_plong(msgp, l)
register unsigned char *msgp;
register unsigned long l;
{

	msgp[3] = l;
	msgp[2] = (l >>= 8);
	msgp[1] = (l >>= 8);
	msgp[0] = l >> 8;
	return(msgp+4);
}
