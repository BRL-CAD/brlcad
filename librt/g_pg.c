/*
 *			G _ P G . C
 *
 *  Function -
 *	Intersect a ray with a Polygonal Object
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSpg[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "nmg.h"
#include "./debug.h"
#include "./plane.h"

#define TRI_NULL	((struct tri_specific *)0)

HIDDEN int rt_pgface();

/* Describe algorithm here */


/*
 *			R T _ P G _ P R E P
 *  
 *  This routine is used to prepare a list of planar faces for
 *  being shot at by the triangle routines.
 *
 * Process a PG, which is represented as a vector
 * from the origin to the first point, and many vectors
 * from the first point to the remaining points.
 *  
 */
int
rt_pg_prep( stp, rp, rtip )
struct soltab	*stp;
union record	*rp;
struct rt_i	*rtip;
{
	LOCAL vect_t	work[3];
	LOCAL vect_t	norm;
	register int	i, j;

	/* rp[0] has ID_P_HEAD record */
	for( j = 1; j < stp->st_dp->d_len; j++ )  {
		if( rp[j].u_id != ID_P_DATA )  {
			rt_log("rt_pg_prep(%s):  bad record 0%o\n",
				stp->st_name, rp[j].u_id );
			return(-1);		/* BAD */
		}
		if( rp[j].q.q_count < 3 || rp[j].q.q_count > 5 )  {
			rt_log("rt_pg_prep(%s):  q_count = %d\n",
				stp->st_name, rp[j].q.q_count);
			return(-1);		/* BAD */
		}
		/* Import dbfloat_t normal */
		MAT4X3PNT( norm, stp->st_pathmat, rp[j].q.q_norms[0] );
		VUNITIZE( norm );
		/* Should worry about importing dbfloat_t here */
		MAT4X3PNT( work[0], stp->st_pathmat, rp[j].q.q_verts[0] );
		VMINMAX( stp->st_min, stp->st_max, work[0] );
		/* Should worry about importing dbfloat_t here */
		MAT4X3PNT( work[1], stp->st_pathmat, rp[j].q.q_verts[1] );
		VMINMAX( stp->st_min, stp->st_max, work[1] );
		for( i=2; i < rp[j].q.q_count; i++ )  {
			/* Should worry about importing dbfloat_t here */
			MAT4X3PNT( work[2], stp->st_pathmat, rp[j].q.q_verts[i] );
			VMINMAX( stp->st_min, stp->st_max, work[2] );
			/* output a face */
			(void)rt_pgface( stp, work[0], work[1], work[2], norm );
			VMOVE( work[1], work[2] );
		}
	}
	if( stp->st_specific == (genptr_t)0 )  {
		rt_log("pg(%s):  no faces\n", stp->st_name);
		return(-1);		/* BAD */
	}

	{
		LOCAL fastf_t dx, dy, dz;
		LOCAL fastf_t	f;

		VSET( stp->st_center,
			(stp->st_max[X] + stp->st_min[X])/2,
			(stp->st_max[Y] + stp->st_min[Y])/2,
			(stp->st_max[Z] + stp->st_min[Z])/2 );

		dx = (stp->st_max[X] - stp->st_min[X])/2;
		f = dx;
		dy = (stp->st_max[Y] - stp->st_min[Y])/2;
		if( dy > f )  f = dy;
		dz = (stp->st_max[Z] - stp->st_min[Z])/2;
		if( dz > f )  f = dz;
		stp->st_aradius = f;
		stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);
	}

	return(0);		/* OK */
}

/*
 *			R T _ P G F A C E
 *
 *  This function is called with pointers to 3 points,
 *  and is used to prepare PG faces.
 *  ap, bp, cp point to vect_t points.
 *
 * Return -
 *	0	if the 3 points didn't form a plane (eg, colinear, etc).
 *	#pts	(3) if a valid plane resulted.
 */
HIDDEN int
rt_pgface( stp, ap, bp, cp, np )
struct soltab	*stp;
fastf_t		*ap, *bp, *cp;
fastf_t		*np;
{
	register struct tri_specific *trip;
	vect_t work;
	LOCAL fastf_t m1, m2, m3, m4;

	GETSTRUCT( trip, tri_specific );
	VMOVE( trip->tri_A, ap );
	VSUB2( trip->tri_BA, bp, ap );
	VSUB2( trip->tri_CA, cp, ap );
	VCROSS( trip->tri_wn, trip->tri_BA, trip->tri_CA );

	/* Check to see if this plane is a line or pnt */
	m1 = MAGNITUDE( trip->tri_BA );
	m2 = MAGNITUDE( trip->tri_CA );
	VSUB2( work, bp, cp );
	m3 = MAGNITUDE( work );
	m4 = MAGNITUDE( trip->tri_wn );
	if( NEAR_ZERO(m1, 0.0001) || NEAR_ZERO(m2, 0.0001) ||
	    NEAR_ZERO(m3, 0.0001) || NEAR_ZERO(m4, 0.0001) )  {
		free( (char *)trip);
		if( rt_g.debug & DEBUG_ARB8 )
			rt_log("pg(%s): degenerate facet\n", stp->st_name);
		return(0);			/* BAD */
	}		

	/*  wn is a GIFT-style normal, not necessarily of unit length.
	 *  N is an outward pointing unit normal.
	 *  Eventually, N should be computed as a blend of the given normals.
	 */
	m3 = MAGNITUDE( np );
	if( !NEAR_ZERO( m3, 0.0001 ) )  {
		VMOVE( trip->tri_N, np );
		m3 = 1 / m3;
		VSCALE( trip->tri_N, trip->tri_N, m3 );
	} else {
		VMOVE( trip->tri_N, trip->tri_wn );
		VUNITIZE( trip->tri_N );
	}

	/* Add this face onto the linked list for this solid */
	trip->tri_forw = (struct tri_specific *)stp->st_specific;
	stp->st_specific = (genptr_t)trip;
	return(3);				/* OK */
}

/*
 *  			R T _ P G _ P R I N T
 */
void
rt_pg_print( stp )
register struct soltab *stp;
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;

	if( trip == TRI_NULL )  {
		rt_log("pg(%s):  no faces\n", stp->st_name);
		return;
	}
	do {
		VPRINT( "A", trip->tri_A );
		VPRINT( "B-A", trip->tri_BA );
		VPRINT( "C-A", trip->tri_CA );
		VPRINT( "BA x CA", trip->tri_wn );
		VPRINT( "Normal", trip->tri_N );
		rt_log("\n");
	} while( trip = trip->tri_forw );
}

/*
 *			R T _ P G _ S H O T
 *  
 * Function -
 *	Shoot a ray at a polygonal object.
 *  
 * Returns -
 *	0	MISS
 *  	segp	HIT
 */
struct seg *
rt_pg_shot( stp, rp, ap )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;
#define MAXHITS 32		/* # surfaces hit, must be even */
	LOCAL struct hit hits[MAXHITS];
	register struct hit *hp;
	LOCAL int	nhits;

	nhits = 0;
	hp = &hits[0];

	/* consider each face */
	for( ; trip; trip = trip->tri_forw )  {
		FAST fastf_t	dn;		/* Direction dot Normal */
		LOCAL fastf_t	abs_dn;
		FAST fastf_t	k;
		LOCAL fastf_t	alpha, beta;
		LOCAL vect_t	wxb;		/* vertex - ray_start */
		LOCAL vect_t	xp;		/* wxb cross ray_dir */

		/*
		 *  Ray Direction dot N.  (N is outward-pointing normal)
		 *  wn points inwards, and is not unit length.
		 */
		dn = VDOT( trip->tri_wn, rp->r_dir );

		/*
		 *  If ray lies directly along the face, (ie, dot product
		 *  is zero), drop this face.
		 */
		abs_dn = dn >= 0.0 ? dn : (-dn);
		if( abs_dn < SQRT_SMALL_FASTF )
			continue;
		VSUB2( wxb, trip->tri_A, rp->r_pt );
		VCROSS( xp, wxb, rp->r_dir );

		/* Check for exceeding along the one side */
		alpha = VDOT( trip->tri_CA, xp );
		if( dn < 0.0 )  alpha = -alpha;
		if( alpha < 0.0 || alpha > abs_dn )
			continue;

		/* Check for exceeding along the other side */
		beta = VDOT( trip->tri_BA, xp );
		if( dn > 0.0 )  beta = -beta;
		if( beta < 0.0 || beta > abs_dn )
			continue;
		if( alpha+beta > abs_dn )
			continue;
		k = VDOT( wxb, trip->tri_wn ) / dn;

		/* For hits other than the first one, might check
		 *  to see it this is approx. equal to previous one */

		/*  If dn < 0, we should be entering the solid.
		 *  However, we just assume in/out sorting later will work.
		 *  Really should mark and check this!
		 */
		VJOIN1( hp->hit_point, rp->r_pt, k, rp->r_dir );

		/* HIT is within planar face */
		hp->hit_dist = k;
		VMOVE( hp->hit_normal, trip->tri_N );
		if( ++nhits >= MAXHITS )  {
			rt_log("rt_pg_shot(%s): too many hits\n", stp->st_name);
			break;
		}
		hp++;
	}
	if( nhits == 0 )
		return(SEG_NULL);		/* MISS */

	/* Sort hits, Near to Far */
	{
		register int i, j;
		LOCAL struct hit temp;

		for( i=0; i < nhits-1; i++ )  {
			for( j=i+1; j < nhits; j++ )  {
				if( hits[i].hit_dist <= hits[j].hit_dist )
					continue;
				temp = hits[j];		/* struct copy */
				hits[j] = hits[i];	/* struct copy */
				hits[i] = temp;		/* struct copy */
			}
		}
	}

	if( nhits&1 )  {
		register int i;
		static int nerrors = 0;		/* message counter */
		/*
		 * If this condition exists, it is almost certainly due to
		 * the dn==0 check above.  Thus, we will make the last
		 * surface rather thin.
		 * This at least makes the
		 * presence of this solid known.  There may be something
		 * better we can do.
		 */
		hits[nhits] = hits[nhits-1];	/* struct copy */
		VREVERSE( hits[nhits].hit_normal, hits[nhits-1].hit_normal );
		hits[nhits++].hit_dist += 0.1;	/* mm thick */
		if( nerrors++ < 6 )  {
			rt_log("rt_pg_shot(%s): %d hits: ", stp->st_name, nhits-1);
			for(i=0; i < nhits; i++ )
				rt_log("%f, ", hits[i].hit_dist );
			rt_log("\n");
		}
	}

	/* nhits is even, build segments */
	{
		register struct seg *segp;			/* XXX */
		segp = SEG_NULL;
		while( nhits > 0 )  {
			register struct seg *newseg;		/* XXX */
			GET_SEG(newseg, ap->a_resource);
			newseg->seg_next = segp;
			segp = newseg;
			segp->seg_stp = stp;
			segp->seg_in = hits[nhits-2];	/* struct copy */
			segp->seg_out = hits[nhits-1];	/* struct copy */
			nhits -= 2;
		}
		return(segp);			/* HIT */
	}
	/* NOTREACHED */
}

/*
 *			R T _ P G _ F R E E
 */
void
rt_pg_free( stp )
struct soltab *stp;
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;

	while( trip != TRI_NULL )  {
		register struct tri_specific *nexttri = trip->tri_forw;

		rt_free( (char *)trip, "pg tri_specific");
		trip = nexttri;
	}
}

/*
 *			R T _ P G _ N O R M
 */
void
rt_pg_norm( hitp, stp, rp )
register struct hit *hitp;
struct soltab *stp;
register struct xray *rp;
{
	/* Normals computed in rt_pg_shot, nothing to do here */
}

/*
 *			R T _ P G _ U V
 */
void
rt_pg_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	/* Do nothing.  Really, should do what ARB does. */
	uvp->uv_u = uvp->uv_v = 0;
	uvp->uv_du = uvp->uv_dv = 0;
}

/*
 *			R T _ P G _ C L A S S
 */
int
rt_pg_class()
{
	return(0);
}

/*
 *			R T _ P G _ P L O T
 */
int
rt_pg_plot( rp, xform, vhead, dp )
union record	*rp;
mat_t		xform;
struct vlhead	*vhead;
struct directory *dp;
{
	register int	i;
	register int	n;	/* number of P_DATA records involved */
	int		npts;
	vect_t		work[5];

	for( n=1; n < dp->d_len; n++ )  {
		if( (npts = rp[n].q.q_count) < 3 )  {
			rt_log("rt_pg_plot() face with only %d verts?\n", npts);
			continue;
		}
		if( npts > 5 )
			npts = 5;
		for( i=0; i < npts; i++ )  {
			MAT4X3PNT( work[i], xform, rp[n].q.q_verts[i] );
		}
		ADD_VL( vhead, work[npts-1], 0 );
		for( i=0; i < npts; i++ )  {
			ADD_VL( vhead, work[i], 1 );
		}
	}
	return(0);		/* OK */
}

/*
 *			R T _ P G _ C U R V E
 */
void
rt_pg_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit *hitp;
struct soltab *stp;
{
	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/*
 *			R T _ N M G _ F I N D _ P T _ I N _ F A C E
 *
 *  Given a point in 3-space and a face pointer, try to find a vertex
 *  in the face which is within sqrt(tol_sq) distance of the given point.
 *
 *  Returns -
 *	pointer to vertex with matching geometry
 *	NULL
 */
struct vertex *
rt_nmg_find_pt_in_face( fu, pt, tol_sq )
struct faceuse	*fu;
point_t		pt;
double		tol_sq;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	vect_t		delta;

	NMG_CK_FACEUSE(fu);
	lu = fu->lu_p;
	do {
		NMG_CK_LOOPUSE(lu);
		if (*lu->down.magic_p == NMG_VERTEXUSE_MAGIC) {
			VSUB2( delta, lu->down.vu_p->v_p->vg_p->coord, pt );
			if( MAGSQ(delta) < tol_sq )
				return(lu->down.vu_p->v_p);
		}
		else if (*lu->down.magic_p == NMG_EDGEUSE_MAGIC) {
			eu = lu->down.eu_p;
			do {
				VSUB2( delta, eu->vu_p->v_p->vg_p->coord, pt );
				if( MAGSQ(delta) < tol_sq )
					return(eu->vu_p->v_p);
				eu = eu->next;
			} while (eu != lu->down.eu_p);
		} else
			rt_bomb("rt_nmg_find_pt_in_face: Bogus child of loop\n");

		lu = lu->next;
	} while (lu != fu->lu_p);
	return ((struct vertex *)NULL);
}

/*
 *			R T _ N M G _ F I N D _ P T _ I N _ S H E L L
 *
 *  Given a point in 3-space and a shell pointer, try to find a vertex
 *  anywhere in the shell which is within sqrt(tol_sq) distance of
 *  the given point.
 *
 *  Returns -
 *	pointer to vertex with matching geometry
 *	NULL
 */
struct vertex *
rt_nmg_find_pt_in_shell( s, pt, tol_sq )
struct shell	*s;
point_t		pt;
double		tol_sq;
{
	struct faceuse	*fu;
	struct loopuse	*lu;
	struct edgeuse	*eu;
	struct vertex	*v;
	struct vertex_g	*vg;
	vect_t		delta;

	NMG_CK_SHELL(s);

	if( fu = s->fu_p ) {
		/* Shell has faces */
		do {
			NMG_CK_FACEUSE(fu);
			if( (v = rt_nmg_find_pt_in_face( fu, pt, tol_sq )) )
				return(v);
			if (fu->next != s->fu_p &&
			    fu->next->f_p == fu->f_p)
				fu = fu->next->next;
			else
				fu = fu->next;
		} while (fu != s->fu_p);
	}
	if( lu = s->lu_p ) {
		do {
			NMG_CK_LOOPUSE(lu);
			/* XXX what to do here? */
			rt_log("rt_nmg_find_pt_in_face(): lu?\n");
			lu = lu->next;
		} while (lu != s->fu_p->lu_p);

	}
	if( eu = s->eu_p ) {
		do {
			NMG_CK_EDGEUSE(eu);
			NMG_CK_VERTEXUSE(eu->vu_p);
			v = eu->vu_p->v_p;
			NMG_CK_VERTEX(v);
			if( (vg = v->vg_p) )  {
				NMG_CK_VERTEX_G(vg);
				VSUB2( delta, vg->coord, pt );
				if( MAGSQ(delta) < tol_sq )
					return(v);
			}
			eu = eu->next;
		} while (eu != s->eu_p);
	}
	if (s->vu_p) {
		NMG_CK_VERTEXUSE(s->vu_p);
		v = s->vu_p->v_p;
		NMG_CK_VERTEX(v);
		if( (vg = v->vg_p) )  {
			NMG_CK_VERTEX_G( vg );
			VSUB2( delta, vg->coord, pt );
			if( MAGSQ(delta) < tol_sq )
				return(v);
		}
	}
	return( (struct vertex *)0 );
}

/*
 *			R T _ P G _ T E S S
 *
 *  This is ugly, need to deal with import/export issues!
 */
int
rt_pg_tess( r, m, rp, mat, dp, abs_tol, rel_tol )
struct nmgregion	**r;
struct model		*m;
register union record	*rp;
register mat_t		mat;
struct directory	*dp;
double			abs_tol;
double			rel_tol;
{
	register int	i;
	register int	n;	/* number of P_DATA records involved */
	vect_t		work[5];
	struct shell	*s;
	struct vertex	*verts[5];
	struct vertex	**vertp[5];
	struct faceuse	*fu;
	fastf_t		tol;
	fastf_t		tol_sq;
	int		npts;

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = m->r_p->s_p;

	/* rel_tol is hard to deal with, given we don't know the RPP yet */
	tol = abs_tol;
	if( tol <= 0.0 )
		tol = 0.1;	/* mm */
	tol_sq = tol * tol;

	for( i=0; i<5; i++ )
		vertp[i] = &verts[i];
	for( n=1; n < dp->d_len; n++ )  {
		if( (npts = rp[n].q.q_count) < 3 )  {
			rt_log("rt_pg_tess() face with only %d verts?\n", npts);
			continue;
		}
		if( npts > 5 )
			npts = 5;
		for( i=0; i < npts; i++ )  {
			MAT4X3PNT( work[i], mat, rp[n].q.q_verts[i] );
		}

		/* Locate these points, if previously mentioned */
		for( i=0; i < npts; i++ )  {
			verts[i] = rt_nmg_find_pt_in_shell( s, work[i], tol_sq );
		}

		/* Construct the face */
		if( (fu = nmg_cmface( s, vertp, rp[n].q.q_count )) == (struct faceuse *)0 )  {
			rt_log("rt_pg_tess() nmg_cmface failed, skipping face %d\n",
				n);
		}

		/* Associate vertex geometry, where none existed before */
		for( i=0; i < npts; i++ )  {
			if( verts[i]->vg_p )  continue;
			nmg_vertex_gv( verts[i], work[i] );
		}

		/* Associate face geometry */
		rt_mk_nmg_planeeqn( fu );
	}

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r );

	return(0);		/* OK */
}
