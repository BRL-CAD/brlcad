/*
 *       M A P _ O S L O . C
 *
 * Function -
 *     Map the olso matrix with the old curve resulting in a new one.
 * 
 * Author -
 *     Paul R. Stay
 *
 * Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1990-2004 by the United States Army.
 *     All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"

/* This routine takes a oslo refinement matrix as described in the
 * paper "Making the Oslo Algorithm More Efficient" and maps it to the
 * old control points resulting in new control points.
 * (this procedure should probably never called by a user program but
 * should remain internal to the library. Bounds are given to facilitate
 * easier spliting of the surface.
 */

void
rt_nurb_map_oslo( oslo, old_pts, new_pts, o_stride, n_stride, lower, upper, pt_type)
struct oslo_mat *oslo;			/* Oslo matrix  */
fastf_t *old_pts;			/* Old control points */
fastf_t *new_pts;			/* New control points */
int	o_stride;			/* inc to next point of old mesh*/
int	n_stride;			/* inc to next point of new mesh*/
int	lower,  upper;	/* Upper and lower bounds for curve generation */
int	pt_type;
{
	register fastf_t *c_ptr;		/* new curve pointer */
	register fastf_t *o_pts;
	register struct oslo_mat *o_ptr;	/* oslo matrix pointer */
	register int	k;
	int	j, 				/* j loop */
		i;				/* oslo loop */
	int	coords;

	coords = RT_NURB_EXTRACT_COORDS( pt_type);

	c_ptr = new_pts;

	if ( lower != 0)
		for ( i = 0,  o_ptr = oslo; i < lower; i++,  o_ptr = 
		    o_ptr->next)
			;
	else
		o_ptr = oslo;

	for ( j = lower; j < upper; j++, o_ptr = o_ptr->next) {
		fastf_t o_scale;
		o_pts = &old_pts[(o_ptr->offset * o_stride)];

		o_scale = o_ptr->o_vec[0];

		for ( k = 0; k < coords; k++)
			c_ptr[k] = o_pts[k] * o_scale;

		for ( i = 1; i <= o_ptr->osize; i++) {
			o_scale = o_ptr->o_vec[i];
			o_pts += o_stride;
			for ( k = 0; k < coords; k++)
				c_ptr[k] += o_scale * o_pts[k];
		}
		c_ptr += n_stride;
	}
}
