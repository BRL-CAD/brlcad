/*                           X D R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup hton */
/** @{ */
/** @file xdr.c
 *
 * Routines to implement an external data representation (XDR)
 * compatible with the usual InterNet standards, e.g.:
 * big-endian, twos-compliment fixed point, and IEEE floating point.
 *
 * Routines to insert/extract short/long's into char arrays,
 * independend of machine byte order and word-alignment.
 * Uses encoding compatible with routines found in libpkg,
 * and BSD system routines htonl(), htons(), ntohl(), ntohs().
 *
 */

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "bu.h"


/**
 * B U _ G S H O R T
 */
unsigned short
bu_gshort(const unsigned char *msgp)
{
    register const unsigned char *p = msgp;
    register unsigned short u;

    u = *p++ << 8;
    return ((unsigned short)(u | *p));
}


/**
 * B U _ G L O N G
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
 * B U _ P S H O R T
 */
unsigned char *
bu_pshort(register unsigned char *msgp, register int s)
{

    msgp[1] = s;
    msgp[0] = s >> 8;
    return (msgp+2);
}


/**
 * B U _ P L O N G
 */
unsigned char *
bu_plong(register unsigned char *msgp, register long unsigned int l)
{

    msgp[3] = l;
    msgp[2] = (l >>= 8);
    msgp[1] = (l >>= 8);
    msgp[0] = l >> 8;
    return (msgp+4);
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
