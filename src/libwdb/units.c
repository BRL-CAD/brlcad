/*                         U N I T S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2016 United States Government as represented by
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
/** @file libwdb/units.c
 *
 * Module of libwdb to handle units conversion.
 *
 */

#include "common.h"

#include <ctype.h>
#include "bio.h"

#include "vmath.h"
#include "bu/units.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

double mk_conv2mm = 1.0;		/* Conversion factor to mm */


int
mk_conversion(char *str)
{
    double d;

    if ((d = bu_units_conversion(str)) <= 0.0) return -1;
    return mk_set_conversion(d);
}


int
mk_set_conversion(double val)
{
    if (val <= 0.0) return -1;
    mk_conv2mm = val;
    return 0;
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
