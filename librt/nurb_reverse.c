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

rt_nurb_reverse_srf( srf )
struct snurb * srf;
{

	int i,j,k;
	int coords;
	int row, col;
	fastf_t * p_ptr;
	fastf_t * tmp;

        p_ptr = srf->ctl_points;
        coords = RT_NURB_EXTRACT_COORDS(srf->pt_type);

	row = srf->s_size[0];
	col = srf->s_size[1];

	tmp = (fastf_t *) rt_malloc(sizeof(fastf_t) * coords * 
		row * col, "nurb_reverse:temp");

	for(i = 0; i < row; i++)
	for(j = 0; j < col; j++)
	{
		int tmp_index, p_index;

		tmp_index = j * col * coords + i * coords;
		p_index   = i * row * coords + j * coords;

		for(k= 0; k < coords; k++)
			tmp[tmp_index +k] = p_ptr[p_index + k];

	}

	for( i = 0; i < row * col * coords; i++)
		p_ptr[i] = tmp[i];

	srf->s_size[0] = col;
	srf->s_size[1] = row;

	i = srf->u_knots.k_size;
	srf->u_knots.k_size = srf->v_knots.k_size;
	srf->v_knots.k_size = i;

	p_ptr = srf->u_knots.knots;
	srf->u_knots.knots = srf->v_knots.knots;
	srf->v_knots.knots = p_ptr;

	rt_free((char *) tmp, "temporary storage for transpose");
}
