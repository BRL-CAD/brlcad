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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
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

		VMOVE( work[0], &pgp->poly[p].verts[0*3] );
		VMINMAX( stp->st_min, stp->st_max, work[0] );
		VMOVE( work[1], &pgp->poly[p].verts[1*3] );
		VMINMAX( stp->st_min, stp->st_max, work[1] );

		for( i=2; i < pgp->poly[p].npts; i++ )  {
			VMOVE( work[2], &pgp->poly[p].verts[i*3] );
			VMINMAX( stp->st_min, stp->st_max, work[2] );

			/* output a face */
			(void)rt_pgface( stp,
				work[0], work[1], work[2], &rtip->rti_tol );

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
rt_pgface( stp, ap, bp, cp, tol )
struct soltab	*stp;
fastf_t		*ap, *bp, *cp;
CONST struct rt_tol	*tol;
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
	if( m1 < tol->dist || m2 < tol->dist ||
	    m3 < tol->dist || m4 < tol->dist )  {
		rt_free( (char *)trip, "getstruct tri_specific");
		if( rt_g.debug & DEBUG_ARB8 )
			rt_log("pg(%s): degenerate facet\n", stp->st_name);
		return(0);			/* BAD */
	}		

	/*  wn is a normal of not necessarily unit length.
	 *  N is an outward pointing unit normal.
	 *  We depend on the points being given in CCW order here.
	 */
	VMOVE( trip->tri_N, trip->tri_wn );
	VUNITIZE( trip->tri_N );

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
register CONST struct soltab *stp;
{
	register CONST struct tri_specific *trip =
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

		if( nerrors++ < 6 )  {
			rt_log("rt_pg_shot(%s): WARNING %d hits:\n", stp->st_name, nhits);
			for(i=0; i < nhits; i++ )
			{
				if( VDOT( rp->r_dir, hits[i].hit_normal ) < 0.0 )
					rt_log("\tentrance at %f\n", hits[i].hit_dist );
				else
					rt_log("\texit at %f\n", hits[i].hit_dist );
			}
		}

		if( nhits > 2 )
		{
			fastf_t dot1,dot2;
			int j;

			/* likely an extra hit,
			 * look for consecutive entrances or exits */

			dot2 = 1.0;
			i = 0;
			while( i<nhits )
			{
				dot1 = dot2;
				dot2 = VDOT( rp->r_dir, hits[i].hit_normal );
				if( dot1 > 0.0 && dot2 > 0.0 )
				{
					/* two consectutive exits,
					 * manufacture an entrance at same distance
					 * as second exit.
					 */
					for( j=nhits ; j>i ; j-- )
						hits[j] = hits[j-1];	/* struct copy */

					VREVERSE( hits[i].hit_normal, hits[i].hit_normal );
					dot2 = VDOT( rp->r_dir, hits[i].hit_normal );
					nhits++;
					rt_log( "\t\tadding fictitious entry at %f\n", hits[i].hit_dist );
				}
				else if( dot1 < 0.0 && dot2 < 0.0 )
				{
					/* two consectutive entrances,
					 * manufacture an exit between them.
					 */

					for( j=nhits ; j>i ; j-- )
						hits[j] = hits[j-1];	/* struct copy */

					hits[i] = hits[i-1];	/* struct copy */
					VREVERSE( hits[i].hit_normal, hits[i-1].hit_normal );
					dot2 = VDOT( rp->r_dir, hits[i].hit_normal );
					nhits++;
					rt_log( "\t\tadding fictitious exit at %f\n", hits[i].hit_dist );
				}
				i++;
			}

		}
		else
		{
			hits[nhits] = hits[nhits-1];	/* struct copy */
			VREVERSE( hits[nhits].hit_normal, hits[nhits-1].hit_normal );
			rt_log( "\t\tadding fictitious hit at %f\n", hits[nhits].hit_dist );
			nhits++;
		}
	}

	/* nhits is even, build segments */
	{
		register struct seg *segp;
		register int	i;
		for( i=0; i < nhits; i += 2 )  {
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
rt_pg_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
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

/*
 *			R T _ P G _ T E S S
 */
int
rt_pg_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	register int	i;
	struct shell	*s;
	struct vertex	**verts;	/* dynamic array of pointers */
	struct vertex	***vertp;/* dynamic array of ptrs to pointers */
	struct faceuse	*fu;
	register int	p;	/* current polygon number */
	struct rt_pg_internal	*pgp;

	RT_CK_DB_INTERNAL(ip);
	pgp = (struct rt_pg_internal *)ip->idb_ptr;
	RT_PG_CK_MAGIC(pgp);

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = RT_LIST_FIRST(shell, &(*r)->s_hd);

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
			verts[i] = nmg_find_pt_in_shell( s,
				&pp->verts[3*i], tol );
		}

		/* Construct the face.  Verts should be in CCW order */
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
		if( nmg_calc_face_g( fu ) )
		{
			nmg_pr_fu_briefly( fu, "" );
			rt_free( (char *)verts, "pg_tess verts[]" );
			rt_free( (char *)vertp, "pg_tess vertp[]" );
			return -1;			/* FAIL */
		}
	}

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

	/* Polysolids are often built with incorrect face normals.
	 * Don't depend on them here.
	 */
	nmg_fix_normals( s , tol );

	/* mark edges as real */
	(void)nmg_mark_edges_real( &s->l );
	
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
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
CONST mat_t			mat;
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
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
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

	if( !verbose )  return(0);

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
