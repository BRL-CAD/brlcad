/*
 * spltest.c
 *
 *  Simple spline test.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "db.h"
#include "vmath.h"
#include "rtlist.h"
#include "nurb.h"
#include "rtgeom.h"
#include "wdb.h"

struct rt_nurb_internal	si;

main(argc, argv)
char	**argv;
{
	point_t	a,b,c,d;

	si.magic = RT_NURB_INTERNAL_MAGIC;
	si.nsrf = 0;
	si.srfs = (struct snurb **)bu_malloc( sizeof(struct snurb *)*100, "snurb ptrs");

	mk_id( stdout, "Mike's Spline Test" );

	VSET( a,  0,  0,  0 );
	VSET( b, 10,  0,  0 );
	VSET( c, 10, 10,  0 );
	VSET( d,  0, 10,  0 );

	make_face( a, b, c, d, 2 );

	mk_export_fwrite( stdout, "spl", (genptr_t)&si, ID_BSPLINE );
}

make_face( a, b, c, d, order )
point_t	a,b,c,d;
int	order;
{
	register struct snurb	*srf;
	int	interior_pts = 0;
	int	cur_kv;
	int	i;
	int	ki;
	register fastf_t	*fp;
	point_t	mid;

	srf = rt_nurb_new_snurb( order, order,
		2*order+interior_pts, 2*order+interior_pts,	/* # knots */
		2+interior_pts, 2+interior_pts,
		RT_NURB_MAKE_PT_TYPE(3, RT_NURB_PT_XYZ, RT_NURB_PT_NONRAT ) );

	/* Build both knot vectors */
	cur_kv = 0;		/* current knot value */
	ki = 0;			/* current knot index */
	for( i=0; i<order; i++, ki++ )  {
		srf->u_knots.knots[ki] = srf->v_knots.knots[ki] = cur_kv;
	}
	cur_kv++;
	for( i=0; i<interior_pts; i++, ki++ )  {
		srf->u_knots.knots[ki] = srf->v_knots.knots[ki] = cur_kv++;
	}
	for( i=0; i<order; i++, ki++ )  {
		srf->u_knots.knots[ki] = srf->v_knots.knots[ki] = cur_kv;
	}

	rt_nurb_pr_kv( &srf->u_knots );

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

	VADD2SCALE( mid, a, b, 0.5 );
	SSET( 0, 0, a );
	SSET( 0, 1, b );
	SSET( 1, 0, d );
	SSET( 1, 1, c );

	si.srfs[si.nsrf++] = srf;
}
