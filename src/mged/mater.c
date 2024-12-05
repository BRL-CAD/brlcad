/*                         M A T E R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file mged/mater.c
 *
 * Code to deal with establishing and maintaining the tables which map
 * region ID codes into worthwhile material information (colors and
 * outboard database "handles").
 *
 * It is expected that entries on this mater list will be sorted in
 * strictly ascending order, with no overlaps (i.e., monotonically
 * increasing).
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "rt/db4.h"
#include "raytrace.h"

#include "./mged.h"
#include "./mged_dm.h"


/*
 * Pass through the solid table and set pointer to appropriate
 * mater structure.
 */
void
mged_color_soltab(struct mged_state *s)
{
    dl_color_soltab(s->GEDP->ged_gdp->gd_headDisplay);
    update_views = 1;		/* re-write control list with new colors */
    dm_set_dirty(DMP, 1);
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
