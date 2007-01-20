/*                     N U R B _ C O P Y . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup nurb */
/*@{*/
/** @file nurb_copy.c
 *	Duplicate the nurb surface.
 *  Author -
 *	Paul Randal Stay
 *
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 */
/*@}*/

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

struct face_g_snurb *
rt_nurb_scopy(const struct face_g_snurb *srf, struct resource *res)
{
	register struct face_g_snurb * n;
	int i;

	NMG_CK_SNURB(srf);

	n = (struct face_g_snurb *) rt_nurb_new_snurb( srf->order[0], srf->order[1],
		srf->u.k_size, srf->v.k_size,
		srf->s_size[0],srf->s_size[1],
		srf->pt_type, res);

	for( i = 0; i < srf->u.k_size; i++)
		n->u.knots[i] =  srf->u.knots[i];

	for( i = 0; i < srf->v.k_size; i++)
		n->v.knots[i] =  srf->v.knots[i];

	for ( i = 0; i <  srf->s_size[0] * srf->s_size[1] *
		RT_NURB_EXTRACT_COORDS(srf->pt_type); i++)
	{

		n->ctl_points[i] = srf->ctl_points[i];
	}

	return (struct face_g_snurb *) n;
}

struct edge_g_cnurb *
rt_nurb_crv_copy(const struct edge_g_cnurb *crv)
{
	register struct edge_g_cnurb * n;
	int i;

	NMG_CK_CNURB( crv );

	n = (struct edge_g_cnurb *) rt_nurb_new_cnurb( crv->order,
		crv->k.k_size, crv->c_size, crv->pt_type);

	for( i = 0; i < crv->k.k_size; i++)
		n->k.knots[i] = crv->k.knots[i];

	for( i = 0; i < crv->c_size *
		RT_NURB_EXTRACT_COORDS(crv->pt_type); i++)
		n->ctl_points[i] = crv->ctl_points[i];

	return (struct edge_g_cnurb *) n;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
