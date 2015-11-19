/*                      O S L O _ M A P . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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
/** @file librt/oslo_map.c
 *
 * Map the oslo matrix with the old curve resulting in a new one.
 *
 */

#include "common.h"

#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "rt/nurb.h"

/* This routine takes a oslo refinement matrix as described in the
 * paper "Making the Oslo Algorithm More Efficient" and maps it to the
 * old control points resulting in new control points.  (this
 * procedure should probably never called by a user program but should
 * remain internal to the library. Bounds are given to facilitate
 * easier splitting of the surface.
 */

void
rt_nurb_map_oslo(struct oslo_mat *oslo, fastf_t *old_pts, fastf_t *new_pts, int o_stride, int n_stride, int lower, int upper, int pt_type)
/* Oslo matrix */
/* Old control points */
/* New control points */
/* inc to next point of old mesh*/
/* inc to next point of new mesh*/
/* Upper and lower bounds for curve generation */

{
    register fastf_t *c_ptr;		/* new curve pointer */
    register fastf_t *o_pts;
    register struct oslo_mat *o_ptr;	/* oslo matrix pointer */
    register int k;
    int j, 				/* j loop */
	i;				/* oslo loop */
    int coords;

    coords = RT_NURB_EXTRACT_COORDS(pt_type);

    c_ptr = new_pts;

    if (lower != 0)
	for (i = 0,  o_ptr = oslo; i < lower; i++,  o_ptr =
		 o_ptr->next)
	    ;
    else
	o_ptr = oslo;

    for (j = lower; j < upper; j++, o_ptr = o_ptr->next) {
	fastf_t o_scale;
	o_pts = &old_pts[(o_ptr->offset * o_stride)];

	o_scale = o_ptr->o_vec[0];

	for (k = 0; k < coords; k++)
	    c_ptr[k] = o_pts[k] * o_scale;

	for (i = 1; i <= o_ptr->osize; i++) {
	    o_scale = o_ptr->o_vec[i];
	    o_pts += o_stride;
	    for (k = 0; k < coords; k++)
		c_ptr[k] += o_scale * o_pts[k];
	}
	c_ptr += n_stride;
    }
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
