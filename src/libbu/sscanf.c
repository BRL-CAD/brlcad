/*                        S S C A N F . C
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
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
/** @file sscanf.c
 *
 * Custom sscanf and vsscanf.
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include "bu.h"

/* if no vsscanf, these are implemented as macros */
#ifdef HAVE_VSSCANF
int
bu_vsscanf(const char *src, const char *fmt, va_list ap)
{
    return vsscanf(src, fmt, ap);
}

int
bu_sscanf(const char *src, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = bu_vsscanf(src, fmt, ap);
    va_end(ap);

    return ret;
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
