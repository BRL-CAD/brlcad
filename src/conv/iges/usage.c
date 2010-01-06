/*                         U S A G E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2010 United States Government as represented by
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
/** @file usage.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "bu.h"


char *message="Usage:  iges-g [-N solid_name] [-X nmg_debug_flag] [-x rt_debug_flag] [-n|d|t] -o file.g file.iges\n\
	-n - Convert all rational B-spline surfaces to a single spline solid\n\
	-d - Convert IGES drawings to NMG objects (and ignore solid objects)\n\
	-3 - Convert IGES drawings to NMG objects, but don't project to 2D (and ignore solid objects)\n\
	-t - Convert all trimmed surfaces to a single NMG trimmed NURBS solid\n\
	-o - Specify BRL-CAD output file\n\
	-p - Write BREP objects as NMG's rather than BOT's\n\
	-X - Set debug flag for NMG routines\n\
	-x - Set debug flag for librt\n\
	-N - Specify name of solid to be created\n\
The n, d (or 3), and t options are mutually exclusive.\n\
With none of the n, d (or 3), or t options specified, the default action\n\
is to convert only IGES solid model entities (CSG and planar face BREP)\n\
The N option provides a name for the single solid created with the n or t\n\
options, it is ignored for all other options\n";


void
usage()
{
    bu_exit( 1, message );
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
