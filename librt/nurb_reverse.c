/*		N U R B _ R E V E R S E . C
 *
 *  Function-
 *  	Reverse the direction of a nurb surface
 *	by transposing the control points
 *
 *  Author -
 *	Paul R. Stay
 *
 *  Source -
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1991 by the United States Army.
 *     All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

void
rt_nurb_reverse_srf( srf )
struct face_g_snurb * srf;
{

	int i,j,k;
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

	for(i = 0; i < row; i++)
	for(j = 0; j < col; j++)
	{
                for( k = 0; k < coords; k++)
 	               *ptr2++ = srf->ctl_points[ (j * col + i) * coords + k];
 	}

	for( i = 0; i < row * col * coords; i++)
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
