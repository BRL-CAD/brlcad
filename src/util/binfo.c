/*                         B I N F O . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file binfo.c
 *
 *  provides information about the version of libraries in use
 *
 *  Author -
 *  	Charles M Kennedy
 *	Christopher Sean Morrison
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tcl.h"

#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bu.h"
#include "db.h"


int
main(int argc, char *argv[])
{
    if (argc > 0) {
	bu_log("Usage: binfo\n\treturns information about the BRL-CAD runtime environment characteristics\n");
    }

    bu_log("bu_version=[%s]\n", bu_version());
    bu_log("bn_version=[%s]\n", bn_version());
    bu_log("rt_version=[%s]\n", rt_version());
    bu_log("fb_version=[%s]\n", fb_version());

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
