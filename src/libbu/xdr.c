/*                           X D R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "bu.h"


unsigned char *
bu_pshort(register unsigned char *msgp, register uint16_t s)
{

    msgp[1] = (unsigned char)s;
    msgp[0] = (unsigned char)(s >> 8);
    return msgp+2;
}

uint16_t
bu_gshort(const unsigned char *msgp)
{
    register const unsigned char *p = msgp;
    register uint16_t u;

    u = *p++ << 8;
    return (uint16_t)(u | *p);
}


unsigned char *
bu_plong(register unsigned char *msgp, register uint32_t l)
{

    msgp[3] = (unsigned char)l;
    msgp[2] = (unsigned char)(l >>= 8);
    msgp[1] = (unsigned char)(l >>= 8);
    msgp[0] = (unsigned char)(l >> 8);
    return msgp+4;
}

uint32_t
bu_glong(const unsigned char *msgp)
{
    register const unsigned char *p = msgp;
    register uint32_t u;

    u = *p++; u <<= 8;
    u |= *p++; u <<= 8;
    u |= *p++; u <<= 8;
    return u | *p;
}


unsigned char *
bu_plonglong(register unsigned char *msgp, register uint64_t l)
{

    msgp[7] = (unsigned char)l;
    msgp[6] = (unsigned char)(l >>= 8);
    msgp[5] = (unsigned char)(l >>= 8);
    msgp[4] = (unsigned char)(l >>= 8);
    msgp[3] = (unsigned char)(l >>= 8);
    msgp[2] = (unsigned char)(l >>= 8);
    msgp[1] = (unsigned char)(l >>= 8);
    msgp[0] = (unsigned char)(l >> 8);
    return msgp+8;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
