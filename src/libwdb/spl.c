/*                           S P L . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2006 United States Government as represented by
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
/** @file spl.c
 *
 *  Library for writing spline objects into
 *  MGED databases from arbitrary procedures.
 *
 *  It is expected that this library will grow as experience is gained.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"

#include "nurb.h"
#define B_SPLINE_DEFINED 1		/* currently wdb.h needs this */

#include "wdb.h"

/*
 *			M K _ B S O L I D
 *
 *  Write the header record for a B-spline solid, which is
 *  composed of several B-spline surfaces.
 *  The call to this routine must be immediately followed by
 *  the appropriate number of calls to mk_bsurf().
 */
int
mk_bsolid( FILE *fp, char *name, int nsurf, double res )
{
	union record rec;

	/* if caller has an rt_nurb_internal struct, should use mk_export_fwrite or mk_fwrite_internal */
	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.d.d_id = ID_BSOLID;
	NAMEMOVE( name, rec.B.B_name );
	rec.B.B_nsurf = nsurf;
	rec.B.B_resolution = res;

	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ B S U R F
 *
 *  Write the external MGED description of a single B-spline surface
 *  given it's internal form from b_spline.h
 */

int
mk_bsurf( FILE *filep, struct face_g_snurb *srf )
{
	union record rec;
	dbfloat_t	*kp;		/* knot vector area */
	dbfloat_t	*mp;		/* mesh area */
	register dbfloat_t	*dbp;
	register fastf_t	*fp;
	register int	i;
	int		n;

	/* if caller has an rt_nurb_internal struct, should use mk_export_fwrite or mk_fwrite_internal */
	BU_ASSERT_LONG( mk_version, <=, 4 );

	if( srf->u.k_size != srf->s_size[RT_NURB_SPLIT_COL] + srf->order[0] ||
	    srf->v.k_size != srf->s_size[RT_NURB_SPLIT_ROW] + srf->order[1]) {
	    	fprintf(stderr,"mk_bsurf:  mis-matched knot/mesh/order\n");
	    	return(-1);
	}

	bzero( (char *)&rec, sizeof(rec) );
	rec.d.d_id = ID_BSURF;

	n = srf->u.k_size + srf->v.k_size;
	n = ((n * sizeof(dbfloat_t)) + sizeof(rec)-1) / sizeof(rec);
	kp = (dbfloat_t *)malloc(n*sizeof(rec));
	bzero( (char *)kp, n*sizeof(rec) );
	rec.d.d_nknots = n;
	rec.d.d_order[RT_NURB_SPLIT_ROW] = srf->order[RT_NURB_SPLIT_ROW];	/* [0] */
	rec.d.d_order[RT_NURB_SPLIT_COL] = srf->order[RT_NURB_SPLIT_COL];	/* [1] */
	rec.d.d_kv_size[RT_NURB_SPLIT_ROW] = srf->u.k_size;
	rec.d.d_kv_size[RT_NURB_SPLIT_COL] = srf->v.k_size;

	n = srf->s_size[RT_NURB_SPLIT_ROW] * srf->s_size[RT_NURB_SPLIT_COL] *
	    RT_NURB_EXTRACT_COORDS(srf->pt_type);
	n = ((n * sizeof(dbfloat_t)) + sizeof(rec)-1) / sizeof(rec);
	mp = (dbfloat_t *)malloc(n*sizeof(rec));
	bzero( (char *)mp, n*sizeof(rec) );
	rec.d.d_nctls = n;
	rec.d.d_geom_type = RT_NURB_EXTRACT_COORDS(srf->pt_type);
	rec.d.d_ctl_size[RT_NURB_SPLIT_ROW] = srf->s_size[RT_NURB_SPLIT_ROW];
	rec.d.d_ctl_size[RT_NURB_SPLIT_COL] = srf->s_size[RT_NURB_SPLIT_COL];

	/* Reformat the knot vectors */
	dbp = kp;
	for( i=0; i<srf->u.k_size; i++ )
		*dbp++ = srf->u.knots[i];
	for( i=0; i<srf->v.k_size; i++ )
		*dbp++ = srf->v.knots[i];

	/* Reformat the mesh */
	dbp = mp;
	fp = srf->ctl_points;
	i = srf->s_size[RT_NURB_SPLIT_ROW] * srf->s_size[RT_NURB_SPLIT_COL] *
	    RT_NURB_EXTRACT_COORDS(srf->pt_type);	/* # floats/point */
	for( ; i>0; i-- )
		*dbp++ = *fp++ * mk_conv2mm;

	if( fwrite( (char *)&rec, sizeof(rec), 1, filep ) != 1 ||
	    fwrite( (char *)kp, sizeof(rec), rec.d.d_nknots, filep ) != rec.d.d_nknots ||
	    fwrite( (char *)mp, sizeof(rec), rec.d.d_nctls, filep ) != rec.d.d_nctls )  {
		free( (char *)kp );
		free( (char *)mp );
	    	return(-1);
	}

	free( (char *)kp );
	free( (char *)mp );
	return(0);
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
