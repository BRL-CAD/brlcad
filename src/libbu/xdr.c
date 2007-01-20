/*                           X D R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup hton */
/*@{*/
/** @file xdr.c
 *  Routines to implement an external data representation (XDR)
 *  compatible with the usual InterNet standards, e.g.:
 *  big-endian, twos-compliment fixed point, and IEEE floating point.
 *
 *  Routines to insert/extract short/long's into char arrays,
 *  independend of machine byte order and word-alignment.
 *  Uses encoding compatible with routines found in libpkg,
 *  and BSD system routines ntohl(), ntons(), ntohl(), ntohs().
 *
 *
 *  @author
 *	Michael John Muuss
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */


#ifndef lint
static const char libbu_xdr_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"



/**
 *			B U _ G S H O R T
 */
unsigned short
bu_gshort(const unsigned char *msgp)
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

/**
 *			B U _ G L O N G
 */
unsigned long
bu_glong(const unsigned char *msgp)
{
    register const unsigned char *p = msgp;
    register unsigned long u;

    u = *p++; u <<= 8;
    u |= *p++; u <<= 8;
    u |= *p++; u <<= 8;
    return (u | *p);
}

/**
 *			B U _ P S H O R T
 */
unsigned char *
bu_pshort(register unsigned char *msgp, register int s)
{

    msgp[1] = s;
    msgp[0] = s >> 8;
    return(msgp+2);
}

/**
 *			B U _ P L O N G
 */
unsigned char *
bu_plong(register unsigned char *msgp, register long unsigned int l)
{

    msgp[3] = l;
    msgp[2] = (l >>= 8);
    msgp[1] = (l >>= 8);
    msgp[0] = l >> 8;
    return(msgp+4);
}

#if 0
/* XXX How do we get "struct timeval" declared for all of bu.h? */
/**
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

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
