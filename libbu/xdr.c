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
static const char libbu_xdr_RCSid[] = "@(#)$Header$ (ARL)";
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
const unsigned char *msgp;
{
	register const unsigned char *p = msgp;
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
const unsigned char *msgp;
{
	register const unsigned char *p = msgp;
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

#if 0
/* XXX How do we get "struct timeval" declared for all of bu.h? */
/*
 *			B U _ G T I M E V A L
 *
 *  Get a timeval structure from an external representation
 *  which "on the wire" is a 64-bit seconds, followed by a 32-bit usec.
 */
typedef unsigned char ext_timeval_t[8+4];	/* storage for on-wire format */

void
bu_gtimeval( tvp, msgp )
struct timeval *tvp;
const unsigned char *msgp;
{
	tvp->tv_sec = (((time_t)BU_GLONG( msgp+0 )) << 32) |
		BU_GLONG( msgp+4 );
	tvp->tv_usec = BU_GLONG( msgp+8 );
}

unsigned char *
bu_ptimeval( msgp, tvp )
const struct timeval *tvp;
unsigned char *msgp;
{
	long upper = (long)(tvp->tv_sec >> 32);
	long lower = (long)(tvp->tv_sec & 0xFFFFFFFFL);

	(void)bu_plong( msgp+0, upper );
	(void)bu_plong( msgp+4, lower );
	return bu_plong( msgp+8, tvp->tv_usec );
}
#endif
