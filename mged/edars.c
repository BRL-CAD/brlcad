/*
 *			E D A R S . C
 *
 * Functions -
 *	find_nearest_ars_pt - find which vertex of an ARS is nearest
 *			the ray from "pt" in the viewing direction (for vertex selection in MGED)
 *
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
	
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "wdb.h"

#include "./ged.h"
#include "./solid.h"
#include "./sedit.h"
#include "./mged_dm.h"
#include "./menu.h"

extern struct rt_tol		mged_tol;	/* from ged.c */
void
find_nearest_ars_pt( crv, col, ars, pick_pt, dir )
int *crv;
int *col;
struct rt_ars_internal *ars;
point_t pick_pt;
vect_t dir;
{
	int i,j;
	int pt_no;
	int closest_i=0,closest_j=0;
	fastf_t min_dist_sq=MAX_FASTF;

	RT_ARS_CK_MAGIC( ars );

	pt_no = 0;
	for( i=0 ; i<ars->ncurves ; i++ )
	{
		for( j=0 ; j<ars->pts_per_curve ; j++ )
		{
			fastf_t dist_sq;

			dist_sq = rt_distsq_line3_pt3( pick_pt, dir, &ars->curves[i][j*3] );
			if( dist_sq < min_dist_sq )
			{
				min_dist_sq = dist_sq;
				closest_i = i;
				closest_j = j;
			}
			pt_no += 3;
		}
	}
	*crv = closest_i;
	*col = closest_j;
}
