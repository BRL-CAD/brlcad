/*                           A R B . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2008 United States Government as represented by
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
/** @file arb.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "bio.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./mged.h"

void
dbpr_arb(struct solidrec *sp, register struct directory *dp)
{
    int i;
    char *s;

    if ( (i=sp->s_cgtype) < 0 )
	i = -i;
    switch ( i )  {
	case ARB4:
	    s="ARB4";
	    break;
	case ARB5:
	    s="ARB5";
	    break;
	case RAW:
	case ARB6:
	    s="ARB6";
	    break;
	case ARB7:
	    s="ARB7";
	    break;
	case ARB8:
	    s="ARB8";
	    break;
	default:
	    s="??";
	    break;
    }

    bu_log("%s:  ARB8 (%s)\n", dp->d_namep, s );

    /* more in edsol.c/pr_solid, called from do_list */
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
