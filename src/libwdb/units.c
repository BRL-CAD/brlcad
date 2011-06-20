/*                         U N I T S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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

#include <stdio.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

double mk_conv2mm = 1.0;		/* Conversion factor to mm */


/**
 * M K _ C O N V E R S I O N
 *
 * Given a string conversion value, find the appropriate factor, and
 * establish it.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
int
mk_conversion(char *str)
{
    double d;

    if ((d = bu_units_conversion(str)) <= 0.0) return -1;
    return mk_set_conversion(d);
}


/**
 * M K _ S E T _ C O N V E R S I O N
 *
 * Establish a new conversion factor for LIBWDB routines.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
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
