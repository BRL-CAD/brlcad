/*	N U R B _ C O P Y . C
 *
 *  Function -
 *	duplicate the nurb surface.
 *  Author -
 *	Paul Randal Stay
 * 
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1991 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"

struct face_g_snurb *
rt_nurb_scopy( srf, res )
CONST struct face_g_snurb * srf;
struct resource *res;
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
rt_nurb_crv_copy( crv )
CONST struct edge_g_cnurb * crv;
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
