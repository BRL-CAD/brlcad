/*                       S P L T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file spltest.c
 * spltest.c
 *
 *  Simple spline test.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "nurb.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"


struct rt_nurb_internal	si;

void make_face( point_t a, point_t b, point_t c, point_t d, int order);


main(int argc, char **argv)
{
	point_t	a,b,c,d;
	struct rt_wdb *fp;

	si.magic = RT_NURB_INTERNAL_MAGIC;
	si.nsrf = 0;
	si.srfs = (struct face_g_snurb **)bu_malloc( sizeof(struct face_g_snurb *)*100, "snurb ptrs");

	if ((fp = wdb_fopen(argv[1])) == NULL) {
	  bu_log("unable to open new database [%s]\n", argv[1]);
	  perror("unable to open database file");
	  return 1;
	}

	mk_id( fp, "Mike's Spline Test" );

	VSET( a,  0,  0,  0 );
	VSET( b, 10,  0,  0 );
	VSET( c, 10, 10,  0 );
	VSET( d,  0, 10,  0 );

	make_face( a, b, c, d, 2 );

	mk_export_fwrite( fp, "spl", (genptr_t)&si, ID_BSPLINE );
}

void
make_face(fastf_t *a, fastf_t *b, fastf_t *c, fastf_t *d, int order)
{
	register struct face_g_snurb	*srf;
	int	interior_pts = 0;
	int	cur_kv;
	int	i;
	int	ki;
	register fastf_t	*fp;

	srf = rt_nurb_new_snurb( order, order,
		2*order+interior_pts, 2*order+interior_pts,	/* # knots */
		2+interior_pts, 2+interior_pts,
		RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT ),
		&rt_uniresource );

	/* Build both knot vectors */
	cur_kv = 0;		/* current knot value */
	ki = 0;			/* current knot index */
	for( i=0; i<order; i++, ki++ )  {
		srf->u.knots[ki] = srf->v.knots[ki] = cur_kv;
	}
	cur_kv++;
	for( i=0; i<interior_pts; i++, ki++ )  {
		srf->u.knots[ki] = srf->v.knots[ki] = cur_kv++;
	}
	for( i=0; i<order; i++, ki++ )  {
		srf->u.knots[ki] = srf->v.knots[ki] = cur_kv;
	}

	rt_nurb_pr_kv( &srf->u );

	/*
	 *  The control mesh is stored in row-major order.
	 */
	/* Head from point A to B */
#if 0
	row = 0;
	for( col=0; col < srf->s_curve[1]; col++ )  {
		fp = &srf->ctl_points[col*srf->s_curve[1]+row];
		VSET( fp
	}
#endif

#define SSET(_col, _row, _val)	{ \
	fp = &srf->ctl_points[((_col*srf->s_size[1])+_row)*3]; \
	VMOVE( fp, _val ); }

	/* VADD2SCALE( mid, a, b, 0.5 ); */
	SSET( 0, 0, a );
	SSET( 0, 1, b );
	SSET( 1, 0, d );
	SSET( 1, 1, c );

	si.srfs[si.nsrf++] = srf;
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
