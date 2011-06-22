/*                  N U R B _ R E V E R S E . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2011 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file primitives/bspline/nurb_reverse.c
 *
 * Reverse the direction of a nurb surface by transposing the control
 * points
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

void
rt_nurb_reverse_srf(struct face_g_snurb *srf)
{

    int i, j, k;
    int coords;
    int row, col;
    fastf_t * p_ptr;
    fastf_t * tmp;
    fastf_t * ptr2;

    p_ptr = srf->ctl_points;
    coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

    row = srf->s_size[0];
    col = srf->s_size[1];

    tmp = (fastf_t *) bu_malloc(sizeof(fastf_t) * coords *
				row * col, "nurb_reverse:temp");

    ptr2 = tmp;

    for (i = 0; i < row; i++)
	for (j = 0; j < col; j++) {
	    for (k = 0; k < coords; k++)
		*ptr2++ = srf->ctl_points[ (j * col + i) * coords + k];
	}

    for (i = 0; i < row * col * coords; i++)
	p_ptr[i] = tmp[i];

    srf->s_size[0] = col;
    srf->s_size[1] = row;

    i = srf->u.k_size;
    srf->u.k_size = srf->v.k_size;
    srf->v.k_size = i;

    p_ptr = srf->u.knots;
    srf->u.knots = srf->v.knots;
    srf->v.knots = p_ptr;

    bu_free((char *) tmp, "temporary storage for transpose");
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
