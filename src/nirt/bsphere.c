/*                       B S P H E R E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file nirt/bsphere.c
 *
 * compute a bounding sphere
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"

#include "./nirt.h"
#include "./usrfmt.h"


fastf_t	bsphere_diameter = (fastf_t)-1.0;

void
set_diameter(struct rt_i *rtip)
{
    vect_t	diag;

    VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
    bsphere_diameter = MAGNITUDE(diag);
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
