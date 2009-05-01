/*                        G E T P U T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
/** @addtogroup libfb */
/** @{ */
/** @file getput.c
 *
 * Routines to insert/extract short/long's. Must account for byte
 * order and non-alignment problems.
 *
 * DEPRECATED: use the libbu serialization routines.
 */
/** @} */


unsigned short
fbgetshort(char *msgp)
{
    register unsigned char *p = (unsigned char *) msgp;
    register unsigned long u;

    u = *p++ << 8;
    return ((unsigned short)(u | *p));
}

unsigned long
fbgetlong(char *msgp)
{
    register unsigned char *p = (unsigned char *) msgp;
    register unsigned long u;

    u = *p++; u <<= 8;
    u |= *p++; u <<= 8;
    u |= *p++; u <<= 8;
    return (u | *p);
}

char *
fbputshort(register short unsigned int s, register char *msgp)
{

    msgp[1] = s;
    msgp[0] = s >> 8;
    return(msgp+2);
}

char *
fbputlong(register long unsigned int l, register char *msgp)
{

    msgp[3] = l;
    msgp[2] = (l >>= 8);
    msgp[1] = (l >>= 8);
    msgp[0] = l >> 8;
    return(msgp+4);
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
