/*
 *			G _ P G . C
 *
 *  Function -
 *	Intersect a ray with a Polygonal Object
 *	that has no explicit topology.
 *	It is assumed that the solid has no holes.
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
#include "rtgeom.h"
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
rt_pg_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_pg_internal	*pgp;
	register int	i;
	int		p;

	pgp = (struct rt_pg_internal *)ip->idb_ptr;
	RT_PG_CK_MAGIC(pgp);

	for( p = 0; p < pgp->npoly; p++ )  {
		LOCAL vect_t	work[3];
		LOCAL vect_t	norm;

		/* Just use first normal as a face normal.  Ignore rest */
		VMOVE( norm, pgp->poly[p].norms );
		VUNITIZE( norm );

		VMOVE( work[0], &pgp->poly[p].verts[0*3] );
		VMINMAX( stp->st_min, stp->st_max, work[0] );
		VMOVE( work[1], &pgp->poly[p].verts[1*3] );
		VMINMAX( stp->st_min, stp->st_max, work[1] );

		for( i=2; i < pgp->poly[p].npts; i++ )  {
			VMOVE( work[2], &pgp->poly[p].verts[i*3] );
			VMINMAX( stp->st_min, stp->st_max, work[2] );

			/* output a face */
			(void)rt_pgface( stp,
				work[0], work[1], work[2], norm );

			/* Chop off a triangle, and continue */
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

		VADD2SCALE( stp->st_center, stp->st_max, stp->st_min, 0.5 );

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
 *	>0	HIT
 */
int
rt_pg_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
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
		return(0);		/* MISS */

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
		register struct seg *segp;
		register int	i;
		for( i=0; i < nhits; i -= 2 )  {
			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			segp->seg_in = hits[i];		/* struct copy */
			segp->seg_out = hits[i+1];	/* struct copy */
			RT_LIST_INSERT( &(seghead->l), &(segp->l) );
		}
	}
	return(nhits);			/* HIT */
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
rt_pg_plot( vhead, ip, abs_tol, rel_tol, norm_tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	register int	i;
	register int	p;	/* current polygon number */
	struct rt_pg_internal	*pgp;

	RT_CK_DB_INTERNAL(ip);
	pgp = (struct rt_pg_internal *)ip->idb_ptr;
	RT_PG_CK_MAGIC(pgp);

	for( p = 0; p < pgp->npoly; p++ )  {
		register struct rt_pg_face_internal	*pp;

		pp = &pgp->poly[p];
		RT_ADD_VLIST( vhead, &pp->verts[3*(pp->npts-1)],
			RT_VLIST_LINE_MOVE );
		for( i=0; i < pp->npts; i++ )  {
			RT_ADD_VLIST( vhead, &pp->verts[3*i],
				RT_VLIST_LINE_DRAW );
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
#if 0
/*	Superseeded by nmg_find_vu_in_face
 */
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
#endif
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
	struct vertexuse *vu;
	struct vertex	*v;
	struct vertex_g	*vg;
	vect_t		delta;

	NMG_CK_SHELL(s);

	fu = NMG_LIST_FIRST(faceuse, &s->fu_hd);
	while (NMG_LIST_MORE(fu, faceuse, &s->fu_hd) ) {
		/* Shell has faces */
		NMG_CK_FACEUSE(fu);
			if( (vu = nmg_find_vu_in_face( pt, fu, tol_sq )) )
				return(vu->v_p);

			if (NMG_LIST_PNEXT(faceuse, fu) == fu->fumate_p)
				fu = NMG_LIST_PNEXT_PNEXT(faceuse, fu);
			else
				fu = NMG_LIST_PNEXT(faceuse, fu);
	}

	for (NMG_LIST(lu, loopuse, &s->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (NMG_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			vu = NMG_LIST_FIRST(vertexuse, &lu->down_hd);
			NMG_CK_VERTEX(vu->v_p);
			if (vg = vu->v_p->vg_p) {
				NMG_CK_VERTEX_G(vg);
				VSUB2( delta, vg->coord, pt );
				if( MAGSQ(delta) < tol_sq )
					return(vu->v_p);
			}
		} else if (NMG_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC) {
			rt_log("in %s at %d ", __FILE__, __LINE__);
			rt_bomb("loopuse has bad child\n");
		} else {
			/* loopuse made of edgeuses */
			for (NMG_LIST(eu, edgeuse, &lu->down_hd)) {
				
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
			}
		}
	}



	lu = NMG_LIST_FIRST(loopuse, &s->lu_hd);
	while (NMG_LIST_MORE(lu, loopuse, &s->lu_hd) ) {

			NMG_CK_LOOPUSE(lu);
			/* XXX what to do here? */
			rt_log("nmg_find_vu_in_face(): lu?\n");

			if (NMG_LIST_PNEXT(loopuse, lu) == lu->lumate_p)
				lu = NMG_LIST_PNEXT_PNEXT(loopuse, lu);
			else
				lu = NMG_LIST_PNEXT(loopuse, lu);
	}

	for (NMG_LIST(eu, edgeuse, &s->eu_hd)) {
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
 */
int
rt_pg_tess( r, m, ip, abs_tol, rel_tol, norm_tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
double			abs_tol;
double			rel_tol;
double			norm_tol;
{
	register int	i;
	struct shell	*s;
	struct vertex	**verts;	/* dynamic array of pointers */
	struct vertex	***vertp;/* dynamic array of ptrs to pointers */
	struct faceuse	*fu;
	fastf_t		tol;
	fastf_t		tol_sq;
	register int	p;	/* current polygon number */
	struct rt_pg_internal	*pgp;

	RT_CK_DB_INTERNAL(ip);
	pgp = (struct rt_pg_internal *)ip->idb_ptr;
	RT_PG_CK_MAGIC(pgp);

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = NMG_LIST_FIRST(shell, &(*r)->s_hd);


	/* rel_tol is hard to deal with, given we don't know the RPP yet */
	tol = abs_tol;
	if( tol <= 0.0 )
		tol = 0.1;	/* mm */
	tol_sq = tol * tol;

	verts = (struct vertex **)rt_malloc(
		pgp->max_npts * sizeof(struct vertex *), "pg_tess verts[]");
	vertp = (struct vertex ***)rt_malloc(
		pgp->max_npts * sizeof(struct vertex **), "pg_tess vertp[]");
	for( i=0; i < pgp->max_npts; i++ )
		vertp[i] = &verts[i];

	for( p = 0; p < pgp->npoly; p++ )  {
		register struct rt_pg_face_internal	*pp;

		pp = &pgp->poly[p];

		/* Locate these points, if previously mentioned */
		for( i=0; i < pp->npts; i++ )  {
			verts[i] = rt_nmg_find_pt_in_shell( s,
				&pp->verts[3*i], tol_sq );
		}

		/* Construct the face */
		if( (fu = nmg_cmface( s, vertp, pp->npts )) == (struct faceuse *)0 )  {
			rt_log("rt_pg_tess() nmg_cmface failed, skipping face %d\n",
				p);
		}

		/* Associate vertex geometry, where none existed before */
		for( i=0; i < pp->npts; i++ )  {
			if( verts[i]->vg_p )  continue;
			nmg_vertex_gv( verts[i], &pp->verts[3*i] );
		}

		/* Associate face geometry */
		rt_mk_nmg_planeeqn( fu );
	}

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r );

	rt_free( (char *)verts, "pg_tess verts[]" );
	rt_free( (char *)vertp, "pg_tess vertp[]" );

	return(0);		/* OK */
}

/*
 *			R T _ P G _ I M P O R T
 *
 *  Read all the polygons in as a complex dynamic structure.
 *  The caller is responsible for freeing the dynamic memory.
 *  (vid rt_pg_ifree).
 */
int
rt_pg_import( ip, ep, mat )
struct rt_db_internal	*ip;
struct rt_external	*ep;
mat_t			mat;
{
	struct rt_pg_internal	*pgp;
	union record		*rp;
	register int		i;
	int			rno;		/* current record number */
	int			p;		/* current polygon index */

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_P_HEAD )  {
		rt_log("rt_pg_import: defective header record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_POLY;
	ip->idb_ptr = rt_malloc(sizeof(struct rt_pg_internal), "rt_pg_internal");
	pgp = (struct rt_pg_internal *)ip->idb_ptr;
	pgp->magic = RT_PG_INTERNAL_MAGIC;

	pgp->npoly = (ep->ext_nbytes - sizeof(union record)) /
		sizeof(union record);
	pgp->poly = (struct rt_pg_face_internal *)rt_malloc(
		pgp->npoly * sizeof(struct rt_pg_face_internal), "rt_pg_face_internal");
	pgp->max_npts = 0;

	for( p=0; p < pgp->npoly; p++ )  {
		register struct rt_pg_face_internal	*pp;

		pp = &pgp->poly[p];
		rno = p+1;
		if( rp[rno].q.q_id != ID_P_DATA )  {
			rt_log("rt_pg_import: defective data record\n");
			return -1;
		}
		pp->npts = rp[rno].q.q_count;
		pp->verts = (fastf_t *)rt_malloc(
			pp->npts * 3 * sizeof(fastf_t), "pg verts[]" );
		pp->norms = (fastf_t *)rt_malloc(
			pp->npts * 3 * sizeof(fastf_t), "pg norms[]" );
#		include "noalias.h"
		for( i=0; i < pp->npts; i++ )  {
			/* Note:  side effect of importing dbfloat_t */
			MAT4X3PNT( &pp->verts[i*3], mat,
				rp[rno].q.q_verts[i] );
			MAT4X3VEC( &pp->norms[i*3], mat,
				rp[rno].q.q_norms[i] );
		}
		if( pp->npts > pgp->max_npts )  pgp->max_npts = pp->npts;
	}
	return( 0 );
}

/*
 *			R T _ P G _ E X P O R T
 *
 *  The name will be added by the caller.
 *  Generally, only libwdb will set conv2mm != 1.0
 */
int
rt_pg_export( ep, ip, local2mm )
struct rt_external	*ep;
struct rt_db_internal	*ip;
double			local2mm;
{
	struct rt_pg_internal	*pgp;
	union record		*rec;
	register int		i;
	int			rno;		/* current record number */
	int			p;		/* current polygon index */

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_POLY )  return(-1);
	pgp = (struct rt_pg_internal *)ip->idb_ptr;
	RT_PG_CK_MAGIC(pgp);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = (1 + pgp->npoly) * sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "pg external");
	rec = (union record *)ep->ext_buf;

	rec[0].p.p_id = ID_P_HEAD;

	for( p=0; p < pgp->npoly; p++ )  {
		register struct rt_pg_face_internal	*pp;

		rno = p+1;
		pp = &pgp->poly[p];
		if( pp->npts < 3 || pp->npts > 5 )  {
			rt_log("rt_pg_export:  unable to support npts=%d\n",
				pp->npts);
			return(-1);
		}

		rec[rno].q.q_id = ID_P_DATA;
		rec[rno].q.q_count = pp->npts;
#		include "noalias.h"
		for( i=0; i < pp->npts; i++ )  {
			/* NOTE: type conversion to dbfloat_t */
			VSCALE( rec[rno].q.q_verts[i],
				&pp->verts[i*3], local2mm );
			VMOVE( rec[rno].q.q_norms[i], &pp->norms[i*3] );
		}
	}
	return(0);
}

/*
 *			R T _ P G _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_pg_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register int			j;
	register struct rt_pg_internal	*pgp =
		(struct rt_pg_internal *)ip->idb_ptr;
	char				buf[256];
	int				i;

	RT_PG_CK_MAGIC(pgp);
	rt_vls_strcat( str, "polygon solid with no topology (POLY)\n");

	sprintf(buf, "\t%d polygons (faces)\n",
		pgp->npoly );
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tMost complex face has %d vertices\n",
		pgp->max_npts );
	rt_vls_strcat( str, buf );

	sprintf(buf, "\tFirst vertex (%g, %g, %g)\n",
		pgp->poly[0].verts[X] * mm2local,
		pgp->poly[0].verts[Y] * mm2local,
		pgp->poly[0].verts[Z] * mm2local );
	rt_vls_strcat( str, buf );

	if( !verbose )  return;

	/* Print out all the vertices of all the faces */
	for( i=0; i < pgp->npoly; i++ )  {
		register fastf_t *v = pgp->poly[i].verts;
		register fastf_t *n = pgp->poly[i].norms;

		sprintf( buf, "\tPolygon %d: (%d pts)\n",
			i, pgp->poly[i].npts );
		rt_vls_strcat( str, buf );
		for( j=0; j < pgp->poly[i].npts; j++ )  {
			sprintf(buf, "\t\tV (%g, %g, %g)\n\t\t N (%g, %g, %g)\n",
				v[X] * mm2local,
				v[Y] * mm2local,
				v[Z] * mm2local,
				n[X] * mm2local,
				n[Y] * mm2local,
				n[Z] * mm2local );
			rt_vls_strcat( str, buf );
			v += ELEMENTS_PER_VECT;
			n += ELEMENTS_PER_VECT;
		}
	}

	return(0);
}

/*
 *			R T _ P G _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_pg_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_pg_internal	*pgp;
	register int			i;

	RT_CK_DB_INTERNAL(ip);
	pgp = (struct rt_pg_internal *)ip->idb_ptr;
	RT_PG_CK_MAGIC(pgp);

	/*
	 *  Free storage for each polygon
	 */
	for( i=0; i < pgp->npoly; i++ )  {
		rt_free( (char *)pgp->poly[i].verts, "pg verts[]");
		rt_free( (char *)pgp->poly[i].norms, "pg norms[]");
	}
	rt_free( (char *)pgp->poly, "pg poly[]" );
	pgp->magic = 0;			/* sanity */
	pgp->npoly = 0;
	rt_free( (char *)pgp, "pg ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
