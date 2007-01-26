/*                         G _ A R S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup g_  */
/** @{ */
/** @file g_ars.c
 *
 *	Intersect a ray with an ARS (Arbitrary faceted solid).
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#ifndef lint
static const char RCSars[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#include <math.h>
#include <ctype.h>

#include "machine.h"
#include "tcl.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"
#include "./plane.h"
#include "./bot.h"


#define TRI_NULL	((struct tri_specific *)0)

/* Describe algorithm here */

extern int rt_bot_minpieces;

/* from g_bot.c */
extern int rt_bot_prep( struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip );
extern void rt_bot_ifree( struct rt_db_internal *ip );

int rt_ars_tess( struct nmgregion **r, struct model *m, struct rt_db_internal *ip,
		 const struct rt_tess_tol *ttol, const struct bn_tol *tol );

void
rt_ars_free( register struct soltab *stp )
{
    bu_bomb("rt_ars_free s/b rt_bot_free\n");
}
int
rt_ars_class(const struct soltab	*stp,
	     const vect_t		min,
	     const vect_t		max,
	     const struct bn_tol	*tol)
{
    bu_bomb("rt_ars_class s/b rt_bot_class\n");
    return 0; /* not reached */
}


/**
 *			R T _ A R S _ R D _ C U R V E
 *
 *  rt_ars_rd_curve() reads a set of ARS B records and returns a pointer
 *  to a malloc()'ed memory area of fastf_t's to hold the curve.
 */
fastf_t *
rt_ars_rd_curve(union record *rp, int npts)
{
	LOCAL int lim;
	LOCAL fastf_t *base;
	register fastf_t *fp;		/* pointer to temp vector */
	register int i;
	LOCAL union record *rr;
	int	rec;

	/* Leave room for first point to be repeated */
	base = fp = (fastf_t *)bu_malloc(
	    (npts+1) * sizeof(fastf_t) * ELEMENTS_PER_VECT,
	    "ars curve" );

	rec = 0;
	for( ; npts > 0; npts -= 8 )  {
		rr = &rp[rec++];
		if( rr->b.b_id != ID_ARS_B )  {
			bu_log("rt_ars_rd_curve():  non-ARS_B record!\n");
			break;
		}
		lim = (npts>8) ? 8 : npts;
		for( i=0; i<lim; i++ )  {
			/* cvt from dbfloat_t */
			VMOVE( fp, (&(rr->b.b_values[i*3])) );
			fp += ELEMENTS_PER_VECT;
		}
	}
	return( base );
}




/**
 *			R T _ A R S _ I M P O R T
 *
 *  Read all the curves in as a two dimensional array.
 *  The caller is responsible for freeing the dynamic memory.
 *
 *  Note that in each curve array, the first point is replicated
 *  as the last point, to make processing the data easier.
 */
int
rt_ars_import(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_ars_internal *ari;
	union record	*rp;
	register int	i, j;
	LOCAL vect_t	base_vect;
	int		currec;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != ID_ARS_A )  {
		bu_log("rt_ars_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_ARS;
	ip->idb_meth = &rt_functab[ID_ARS];
	ip->idb_ptr = bu_malloc(sizeof(struct rt_ars_internal), "rt_ars_internal");
	ari = (struct rt_ars_internal *)ip->idb_ptr;
	ari->magic = RT_ARS_INTERNAL_MAGIC;
	ari->ncurves = rp[0].a.a_m;
	ari->pts_per_curve = rp[0].a.a_n;

	/*
	 * Read all the curves into internal form.
	 */
	ari->curves = (fastf_t **)bu_malloc(
		(ari->ncurves+1) * sizeof(fastf_t **), "ars curve ptrs" );
	currec = 1;
	for( i=0; i < ari->ncurves; i++ )  {
		ari->curves[i] =
			rt_ars_rd_curve( &rp[currec], ari->pts_per_curve );
		currec += (ari->pts_per_curve+7)/8;
	}

	/*
	 * Convert from vector to point notation IN PLACE
	 * by rotating vectors and adding base vector.
	 * Observe special treatment for base vector.
	 */
	if (mat == NULL) mat = bn_mat_identity;
	for( i = 0; i < ari->ncurves; i++ )  {
		register fastf_t *v;

		v = ari->curves[i];
		for( j = 0; j < ari->pts_per_curve; j++ )  {
			LOCAL vect_t	homog;

			if( i==0 && j == 0 )  {
				/* base vector */
				VMOVE( homog, v );
				MAT4X3PNT( base_vect, mat, homog );
				VMOVE( v, base_vect );
			}  else  {
				MAT4X3VEC( homog, mat, v );
				VADD2( v, base_vect, homog );
			}
			v += ELEMENTS_PER_VECT;
		}
		VMOVE( v, ari->curves[i] );		/* replicate first point */
	}
	return( 0 );
}

/**
 *			R T _ A R S _ E X P O R T
 *
 *  The name will be added by the caller.
 *  Generally, only libwdb will set conv2mm != 1.0
 */
int
rt_ars_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_ars_internal	*arip;
	union record		*rec;
	point_t		base_pt;
	int		per_curve_grans;
	int		cur;		/* current curve number */
	int		gno;		/* current granule number */

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ARS )  return(-1);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	per_curve_grans = (arip->pts_per_curve+7)/8;

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = (1 + per_curve_grans * arip->ncurves) *
		sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "ars external");
	rec = (union record *)ep->ext_buf;

	rec[0].a.a_id = ID_ARS_A;
	rec[0].a.a_type = ARS;			/* obsolete? */
	rec[0].a.a_m = arip->ncurves;
	rec[0].a.a_n = arip->pts_per_curve;
	rec[0].a.a_curlen = per_curve_grans;
	rec[0].a.a_totlen = per_curve_grans * arip->ncurves;

	VMOVE( base_pt, &arip->curves[0][0] );
	gno = 1;
	for( cur=0; cur<arip->ncurves; cur++ )  {
		register fastf_t	*fp;
		int			npts;
		int			left;

		fp = arip->curves[cur];
		left = arip->pts_per_curve;
		for( npts=0; npts < arip->pts_per_curve; npts+=8, left -= 8 )  {
			register int	el;
			register int	lim;
			register struct ars_ext	*bp = &rec[gno].b;

			bp->b_id = ID_ARS_B;
			bp->b_type = ARSCONT;	/* obsolete? */
			bp->b_n = cur+1;		/* obsolete? */
			bp->b_ngranule = (npts/8)+1; /* obsolete? */

			lim = (left > 8 ) ? 8 : left;
			for( el=0; el < lim; el++ )  {
				vect_t	diff;
				if( cur==0 && npts==0 && el==0 )
					VSCALE( diff , fp , local2mm )
				else
					VSUB2SCALE( diff, fp, base_pt, local2mm )
				/* NOTE: also type converts to dbfloat_t */
				VMOVE( &(bp->b_values[el*3]), diff );
				fp += ELEMENTS_PER_VECT;
			}
			gno++;
		}
	}
	return(0);
}


/**
 *			R T _ A R S _ I M P O R T 5
 *
 *  Read all the curves in as a two dimensional array.
 *  The caller is responsible for freeing the dynamic memory.
 *
 *  Note that in each curve array, the first point is replicated
 *  as the last point, to make processing the data easier.
 */
int
rt_ars_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
	struct rt_ars_internal *ari;
	register int		i, j;
	register unsigned char	*cp;
	vect_t			tmp_vec;
	register fastf_t	*fp;

	BU_CK_EXTERNAL( ep );
	RT_CK_DB_INTERNAL( ip );

	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_ARS;
	ip->idb_meth = &rt_functab[ID_ARS];
	ip->idb_ptr = bu_malloc(sizeof(struct rt_ars_internal), "rt_ars_internal");

	ari = (struct rt_ars_internal *)ip->idb_ptr;
	ari->magic = RT_ARS_INTERNAL_MAGIC;

	cp = (unsigned char *)ep->ext_buf;
	ari->ncurves = bu_glong( cp );
	cp += SIZEOF_NETWORK_LONG;
	ari->pts_per_curve = bu_glong( cp );
	cp += SIZEOF_NETWORK_LONG;

	/*
	 * Read all the curves into internal form.
	 */
	ari->curves = (fastf_t **)bu_calloc(
		(ari->ncurves+1), sizeof(fastf_t *), "ars curve ptrs" );
	if (mat == NULL) mat = bn_mat_identity;
	for( i=0; i < ari->ncurves; i++ )  {
		ari->curves[i] = (fastf_t *)bu_calloc( (ari->pts_per_curve + 1) * 3,
			sizeof( fastf_t ), "ARS points" );
		fp = ari->curves[i];
		for( j=0 ; j<ari->pts_per_curve ; j++ ) {
			ntohd( (unsigned char *)tmp_vec, cp, 3 );
			MAT4X3PNT( fp, mat, tmp_vec );
			cp += 3 * SIZEOF_NETWORK_DOUBLE;
			fp += ELEMENTS_PER_VECT;
		}
		VMOVE( fp, ari->curves[i] );	/* duplicate first point */
	}
	return( 0 );
}

/**
 *			R T _ A R S _ E X P O R T 5
 *
 *  The name will be added by the caller.
 *  Generally, only libwdb will set conv2mm != 1.0
 */
int
rt_ars_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_ars_internal	*arip;
	unsigned char	*cp;
	vect_t		tmp_vec;
	int		cur;		/* current curve number */

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_ARS )  return(-1);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = 2 * SIZEOF_NETWORK_LONG +
		3 * arip->ncurves * arip->pts_per_curve * SIZEOF_NETWORK_DOUBLE;
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "ars external");
	cp = (unsigned char *)ep->ext_buf;

	(void)bu_plong( cp, arip->ncurves );
	cp += SIZEOF_NETWORK_LONG;
	(void)bu_plong( cp, arip->pts_per_curve );
	cp += SIZEOF_NETWORK_LONG;

	for( cur=0; cur<arip->ncurves; cur++ )  {
		register fastf_t	*fp;
		int			npts;

		fp = arip->curves[cur];
		for( npts=0; npts < arip->pts_per_curve; npts++ )  {
			VSCALE( tmp_vec, fp, local2mm );
			ntohd( cp, (unsigned char *)tmp_vec, 3 );
			cp += 3 * SIZEOF_NETWORK_DOUBLE;
			fp += ELEMENTS_PER_VECT;
		}
	}
	return(0);
}

/**
 *			R T _ A R S _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_ars_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register int			j;
	register struct rt_ars_internal	*arip =
		(struct rt_ars_internal *)ip->idb_ptr;
	char				buf[256];
	int				i;

	RT_ARS_CK_MAGIC(arip);
	bu_vls_strcat( str, "arbitrary rectangular solid (ARS)\n");

	sprintf(buf, "\t%d curves, %d points per curve\n",
		arip->ncurves, arip->pts_per_curve );
	bu_vls_strcat( str, buf );

	if( arip->ncurves > 0 ) {
		sprintf(buf, "\tV (%g, %g, %g)\n",
			INTCLAMP(arip->curves[0][X] * mm2local),
			INTCLAMP(arip->curves[0][Y] * mm2local),
			INTCLAMP(arip->curves[0][Z] * mm2local) );
		bu_vls_strcat( str, buf );
	}

	if( !verbose )  return(0);

	/* Print out all the points */
	for( i=0; i < arip->ncurves; i++ )  {
		register fastf_t *v = arip->curves[i];

		sprintf( buf, "\tCurve %d:\n", i );
		bu_vls_strcat( str, buf );
		for( j=0; j < arip->pts_per_curve; j++ )  {
			sprintf(buf, "\t\t(%g, %g, %g)\n",
				INTCLAMP(v[X] * mm2local),
				INTCLAMP(v[Y] * mm2local),
				INTCLAMP(v[Z] * mm2local) );
			bu_vls_strcat( str, buf );
			v += ELEMENTS_PER_VECT;
		}
	}

	return(0);
}

/**
 *			R T _ A R S _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_ars_ifree(struct rt_db_internal *ip)
{
	register struct rt_ars_internal	*arip;
	register int			i;

	RT_CK_DB_INTERNAL(ip);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	/*
	 *  Free storage for faces
	 */
	for( i = 0; i < arip->ncurves; i++ )  {
		bu_free( (char *)arip->curves[i], "ars curve" );
	}
	bu_free( (char *)arip->curves, "ars curve ptrs" );
	arip->magic = 0;		/* sanity */
	arip->ncurves = 0;
	bu_free( (char *)arip, "ars ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/**
 *			R T _ A R S _ P R E P
 *
 *  This routine is used to prepare a list of planar faces for
 *  being shot at by the ars routines.
 *
 * Process an ARS, which is represented as a vector
 * from the origin to the first point, and many vectors
 * from the first point to the remaining points.
 *
 *  This routine is unusual in that it has to read additional
 *  database records to obtain all the necessary information.
 */
int
rt_ars_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
#if 1
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    int ret;

    m = nmg_mm();
    r = BU_LIST_FIRST( nmgregion, &m->r_hd );

    if( rt_ars_tess( &r, m, ip, &rtip->rti_ttol, &rtip->rti_tol) ) {
	    bu_log( "Failed to tessellate ARS (%s)\n", stp->st_dp->d_namep );
	    nmg_km( m );
	    return( -1 );
    }
    rt_ars_ifree( ip );

    s = BU_LIST_FIRST( shell, &r->s_hd );
    bot = nmg_bot( s, &rtip->rti_tol );

    if( !bot ) {
	    bu_log( "Failed to convert ARS to BOT (%s)\n", stp->st_dp->d_namep );
	    nmg_km( m );
	    return( -1 );
    }

    nmg_km( m );

    intern.idb_magic = RT_DB_INTERNAL_MAGIC;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_BOT;
    intern.idb_meth = &rt_functab[ID_BOT];
    intern.idb_ptr = (genptr_t)bot;
    bu_avs_init( &intern.idb_avs, 0, "ARS to a BOT for prep" );

    ret = rt_bot_prep( stp, &intern, rtip );

    rt_bot_ifree( &intern );

    return( ret );
#else
    LOCAL fastf_t	dx, dy, dz;	/* For finding the bounding spheres */
    int	i, j, ntri;
    LOCAL fastf_t	f;
    struct rt_ars_internal	*arip;
    struct bot_specific	*bot;
    const struct bn_tol		*tol = &rtip->rti_tol;
    int ncv;

    arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    /* initialize the Bag-'o-triangles structure we need */
    BU_GETSTRUCT( bot, bot_specific );
    stp->st_specific = (genptr_t)bot;
    bot->bot_mode = RT_BOT_SOLID;
    bot->bot_orientation = RT_BOT_UNORIENTED;
    bot->bot_errmode = (unsigned char)NULL;
    bot->bot_thickness = (fastf_t *)NULL;
    bot->bot_facemode = (struct bu_bitv *)NULL;
    bot->bot_facelist = (struct tri_specific *)NULL;

    /*
     * Compute bounding sphere.
     * Find min and max of the point co-ordinates.
     */
    VSETALL( stp->st_max, -INFINITY );
    VSETALL( stp->st_min,  INFINITY );

    for( i = 0; i < arip->ncurves; i++ )  {
	register fastf_t *v;

	v = arip->curves[i];
	for( j = 0; j < arip->pts_per_curve; j++ )  {
	    VMINMAX( stp->st_min, stp->st_max, v );
	    v += ELEMENTS_PER_VECT;
	}
    }
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


    /*
     *  Compute planar faces
     *  Will examine curves[i][pts_per_curve], provided by rt_ars_rd_curve.
     */
    ncv = arip->ncurves-2;
    ntri = 0;
    for(i=0; i <= ncv; i++ )  {
	register fastf_t *v1, *v2;

	v1 = arip->curves[i];
	v2 = arip->curves[i+1];
	for( j = 0; j < arip->pts_per_curve;
	     j++, v1 += ELEMENTS_PER_VECT, v2 += ELEMENTS_PER_VECT )  {

	    /* XXX make sure the faces are actual triangles */


	    /* carefully make faces, w/inward pointing normals */
	    /* [0][0] [1][1], [0][1] */
	    if (i != 0 &&
		rt_botface(stp, bot, &v1[0], &v2[ELEMENTS_PER_VECT],
			   &v1[ELEMENTS_PER_VECT], ntri, tol) > 0)   ntri++;

	    /* [1][0] [1][1] [0][0] */
	    if (i < ncv &&
		rt_botface(stp, bot, &v2[0], &v2[ELEMENTS_PER_VECT],
			   &v1[0], ntri, tol) > 0)   ntri++;
	}
    }


    if( bot->bot_facelist == (struct tri_specific *)0 )  {
	bu_log("ars(%s):  no faces\n", stp->st_name);
	return(-1);             /* BAD */
    }

    bot->bot_ntri = ntri;


    /*
     *  Support for solid 'pieces'
     *  For now, each triangle is considered a separate piece.
     *  These array allocations can't be made until the number of
     *  triangles are known.
     *
     *  If the number of triangles is too small,
     *  don't bother making pieces, the overhead isn't worth it.
     *
     *  To disable BoT pieces, on the RT command line specify:
     *	-c "set rt_bot_minpieces=0"
     */
    if( rt_bot_minpieces <= 0 )  return 0;
    if( ntri < rt_bot_minpieces )  return 0;


    rt_bot_prep_pieces(bot, stp, ntri, tol);

    rt_ars_ifree( ip );


    return(0);		/* OK */
#endif
}


/**
 *  			R T _ A R S _ P R I N T
 */
void
rt_ars_print(register const struct soltab *stp)
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;

	if( trip == TRI_NULL )  {
		bu_log("ars(%s):  no faces\n", stp->st_name);
		return;
	}
	do {
		VPRINT( "A", trip->tri_A );
		VPRINT( "B-A", trip->tri_BA );
		VPRINT( "C-A", trip->tri_CA );
		VPRINT( "BA x CA", trip->tri_wn );
		VPRINT( "Normal", trip->tri_N );
		bu_log("\n");
	} while( (trip = trip->tri_forw) );
}

/**
 *			R T _ A R S _ S H O T
 *
 * Function -
 *	Shoot a ray at an ARS.
 *
 * Returns -
 *	0	MISS
 *  	!0	HIT
 */
int
rt_ars_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct tri_specific *trip =
		(struct tri_specific *)stp->st_specific;
#define RT_ARS_MAXHITS 128		/* # surfaces hit, must be even */
	LOCAL struct hit hits[RT_ARS_MAXHITS];
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
		LOCAL fastf_t	ds;
		LOCAL vect_t	wxb;		/* vertex - ray_start */
		LOCAL vect_t	xp;		/* wxb cross ray_dir */

		/*
		 *  Ray Direction dot N.  (wn is inward pointing normal)
		 */
		dn = VDOT( trip->tri_wn, rp->r_dir );
		if( RT_G_DEBUG & DEBUG_ARB8 )
			bu_log("N.Dir=%g ", dn );

		/*
		 *  If ray lies directly along the face, drop this face.
		 */
		abs_dn = dn >= 0.0 ? dn : (-dn);
		if( abs_dn < 1.0e-10 )
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
		ds = VDOT( wxb, trip->tri_wn );
		k = ds / dn;		/* shot distance */

		/* For hits other than the first one, might check
		 *  to see it this is approx. equal to previous one */

		/*  If dn < 0, we should be entering the solid.
		 *  However, we just assume in/out sorting later will work.
		 */
		hp->hit_magic = RT_HIT_MAGIC;
		hp->hit_dist = k;
		hp->hit_private = (char *)trip;
		hp->hit_vpriv[X] = dn;
		hp->hit_rayp = rp;

		if(RT_G_DEBUG&DEBUG_ARB8) bu_log("ars: dist k=%g, ds=%g, dn=%g\n", k, ds, dn );

		/* Bug fix: next line was "nhits++".  This caused rt_hitsort
			to exceed bounds of "hits" array by one member and
			clobber some stack variables i.e. "stp" -- GSM */
		if( ++nhits >= RT_ARS_MAXHITS )  {
			bu_log("ars(%s): too many hits\n", stp->st_name);
			break;
		}
		hp++;
	}
	if( nhits == 0 )
		return(0);		/* MISS */

	/* Sort hits, Near to Far */
	rt_hitsort( hits, nhits );

	/* Remove duplicate hits.
	   We remove one of a pair of hits when they are
		1) close together, and
		2) both "entry" or both "exit" occurrences.
	   Two immediate "entry" or two immediate "exit" hits suggest
	   that we hit both of two joined faces, while we want to hit only
	   one.  An "entry" followed by an "exit" (or vice versa) suggests
	   that we grazed an edge, and thus we should leave both
	   in the hit list. */

	{
		register int i, j;

		if( nhits )
			RT_HIT_NORM( &hits[0], stp, 0 )

		for( i=0 ; i<nhits-1 ; i++ )
		{
			fastf_t dist;

			RT_HIT_NORM( &hits[i+1], stp, 0 )
			dist = hits[i].hit_dist - hits[i+1].hit_dist;
			if( NEAR_ZERO( dist, ap->a_rt_i->rti_tol.dist ) &&
				VDOT( hits[i].hit_normal, rp->r_dir ) *
			        VDOT( hits[i+1].hit_normal, rp->r_dir) > 0)
			{
				for( j=i ; j<nhits-1 ; j++ )
					hits[j] = hits[j+1];
				nhits--;
				i--;
			}
		}
	}

	if( nhits&1 )  {
		register int i;
		/*
		 * If this condition exists, it is almost certainly due to
		 * the dn==0 check above.  Just log error.
		 */
		bu_log("ERROR: ars(%s): %d hits odd, skipping solid\n",
			stp->st_name, nhits);
		for(i=0; i < nhits; i++ )
			bu_log("k=%g dn=%g\n",
				hits[i].hit_dist, hp->hit_vpriv[X]);
		return(0);		/* MISS */
	}

	/* nhits is even, build segments */
	{
		register struct seg *segp;
		register int	i,j;

		/* Check in/out properties */
		for( i=nhits; i > 0; i -= 2 )  {
			if( hits[i-2].hit_vpriv[X] >= 0 )
				continue;		/* seg_in */
			if( hits[i-1].hit_vpriv[X] <= 0 )
				continue;		/* seg_out */

#ifndef CONSERVATIVE
			/* if this segment is small enough, just swap the in/out hits */
			if( (hits[i-1].hit_dist - hits[i-2].hit_dist) < 200.0*RT_LEN_TOL )
			{
				struct hit temp;
				fastf_t temp_dist;

				temp_dist = hits[i-1].hit_dist;
				hits[i-1].hit_dist = hits[i-2].hit_dist;
				hits[i-2].hit_dist = temp_dist;

				temp = hits[i-1];	/* struct copy */
				hits[i-1] = hits[i-2];	/* struct copy */
				hits[i-2] = temp;	/* struct copy */
				continue;
			}
#endif
		   	bu_log("ars(%s): in/out error\n", stp->st_name );
			for( j=nhits-1; j >= 0; j-- )  {
		   		bu_log("%d %s dist=%g dn=%g\n",
					j,
					((hits[j].hit_vpriv[X] > 0) ?
		   				" In" : "Out" ),
			   		hits[j].hit_dist,
					hits[j].hit_vpriv[X] );
				if( j>0 )
					bu_log( "\tseg length = %g\n", hits[j].hit_dist - hits[j-1].hit_dist );
		   	}
#ifdef CONSERVATIVE
		   	return(0);
#else
			/* For now, just chatter, and return *something* */
			break;
#endif
		}

		for( i=nhits; i > 0; i -= 2 )  {
			RT_GET_SEG(segp, ap->a_resource);
			segp->seg_stp = stp;
			segp->seg_in = hits[i-2];	/* struct copy */
			segp->seg_out = hits[i-1];	/* struct copy */
			BU_LIST_INSERT( &(seghead->l), &(segp->l) );
		}
	}
	return(nhits);			/* HIT */
}

/**
 *			R T _ H I T S O R T
 *
 *  Sort an array of hits into ascending order.
 */
void
rt_hitsort(register struct hit *h, register int nh)
{
	register int i, j;
	LOCAL struct hit temp;

	for( i=0; i < nh-1; i++ )  {
		for( j=i+1; j < nh; j++ )  {
			if( h[i].hit_dist <= h[j].hit_dist )
				continue;
			temp = h[j];		/* struct copy */
			h[j] = h[i];		/* struct copy */
			h[i] = temp;		/* struct copy */
		}
	}
}

/**
 *  			R T _ A R S _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_ars_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	register struct tri_specific *trip =
		(struct tri_specific *)hitp->hit_private;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	VMOVE( hitp->hit_normal, trip->tri_N );
}

/**
 *			R T _ A R S _ C U R V E
 *
 *  Return the "curvature" of the ARB face.
 *  Pick a principle direction orthogonal to normal, and
 *  indicate no curvature.
 */
void
rt_ars_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
/*	register struct tri_specific *trip =
 *		(struct tri_specific *)hitp->hit_private;
 */
	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/**
 *  			R T _ A R S _ U V
 *
 *  For a hit on a face of an ARB, return the (u,v) coordinates
 *  of the hit point.  0 <= u,v <= 1.
 *  u extends along the Xbasis direction defined by B-A,
 *  v extends along the "Ybasis" direction defined by (B-A)xN.
 */
void
rt_ars_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	register struct tri_specific *trip =
		(struct tri_specific *)hitp->hit_private;
	LOCAL vect_t P_A;
	LOCAL fastf_t r;
	LOCAL fastf_t xxlen, yylen;

	xxlen = MAGNITUDE(trip->tri_BA);
	yylen = MAGNITUDE(trip->tri_CA);

	VSUB2( P_A, hitp->hit_point, trip->tri_A );
	/* Flipping v is an artifact of how the faces are built */
	uvp->uv_u = VDOT( P_A, trip->tri_BA ) * xxlen;
	uvp->uv_v = 1.0 - ( VDOT( P_A, trip->tri_CA ) * yylen );
	if( uvp->uv_u < 0 || uvp->uv_v < 0 )  {
		if( RT_G_DEBUG )
			bu_log("rt_ars_uv: bad uv=%g,%g\n", uvp->uv_u, uvp->uv_v);
		/* Fix it up */
		if( uvp->uv_u < 0 )  uvp->uv_u = (-uvp->uv_u);
		if( uvp->uv_v < 0 )  uvp->uv_v = (-uvp->uv_v);
	}
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	uvp->uv_du = r * xxlen;
	uvp->uv_dv = r * yylen;
}


/**
 *			R T _ A R S _ P L O T
 */
int
rt_ars_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	register int	i;
	register int	j;
	struct rt_ars_internal	*arip;

	RT_CK_DB_INTERNAL(ip);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	/*
	 *  Draw the "waterlines", by tracing each curve.
	 *  n+1th point is first point replicated by code above.
	 */
	for( i = 0; i < arip->ncurves; i++ )  {
		register fastf_t *v1;

		v1 = arip->curves[i];
		RT_ADD_VLIST( vhead, v1, BN_VLIST_LINE_MOVE );
		v1 += ELEMENTS_PER_VECT;
		for( j = 1; j <= arip->pts_per_curve; j++, v1 += ELEMENTS_PER_VECT )
			RT_ADD_VLIST( vhead, v1, BN_VLIST_LINE_DRAW );
	}

	/*
	 *  Connect the Ith points on each curve, to make a mesh.
	 */
	for( i = 0; i < arip->pts_per_curve; i++ )  {
		RT_ADD_VLIST( vhead, &arip->curves[0][i*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
		for( j = 1; j < arip->ncurves; j++ )
			RT_ADD_VLIST( vhead, &arip->curves[j][i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	}

	return(0);
}

#define IJ(ii,jj)	(((i+(ii))*(arip->pts_per_curve+1))+(j+(jj)))
#define ARS_PT(ii,jj)	(&arip->curves[i+(ii)][(j+(jj))*ELEMENTS_PER_VECT])
#define FIND_IJ(a,b)	\
	if( !(verts[IJ(a,b)]) )  { \
		verts[IJ(a,b)] = \
		nmg_find_pt_in_shell( s, ARS_PT(a,b), tol ); \
	}
#define ASSOC_GEOM(corn, a,b)	\
	if( !((*corners[corn])->vg_p) )  { \
		nmg_vertex_gv( *(corners[corn]), ARS_PT(a,b) ); \
	}
/**
 *			R T _ A R S _ T E S S
 */
int
rt_ars_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	register int	i;
	register int	j;
	register int	k;
	struct rt_ars_internal	*arip;
	struct shell	*s;
	struct vertex	**verts;
	struct faceuse	*fu;
	struct bu_ptbl	kill_fus;
	int		bad_ars=0;

	RT_CK_DB_INTERNAL(ip);
	arip = (struct rt_ars_internal *)ip->idb_ptr;
	RT_ARS_CK_MAGIC(arip);

	/* Check for a legal ARS */
	for( i = 0; i < arip->ncurves-1; i++ )
	{
		for( j = 2; j < arip->pts_per_curve; j++ )
		{
			fastf_t dist;
			vect_t pca;
			int code;

			if( VAPPROXEQUAL( ARS_PT(0,-2), ARS_PT(0,-1), tol->dist ) )
				continue;

			code = bn_dist_pt3_lseg3( &dist, pca, ARS_PT(0,-2), ARS_PT(0,-1), ARS_PT(0,0), tol );

			if( code < 2 )
			{
				bu_log( "ARS curve backtracks on itself!!!\n" );
				bu_log( "\tCurve #%d, points #%d through %d are:\n", i, j-2, j );
				bu_log( "\t\t%d (%f %f %f)\n", j-2, V3ARGS( ARS_PT(0,-2) ) );
				bu_log( "\t\t%d (%f %f %f)\n", j-1, V3ARGS( ARS_PT(0,-1) ) );
				bu_log( "\t\t%d (%f %f %f)\n", j, V3ARGS( ARS_PT(0,0) ) );
				bad_ars = 1;
				j++;
			}
		}
	}

	if( bad_ars )
	{
		bu_log( "TESSELATION FAILURE: This ARS solid has not been tesselated.\n\tAny result you may obtain is incorrect.\n" );
		return( -1 );
	}

	bu_ptbl_init( &kill_fus, 64, " &kill_fus");

	/* Build the topology of the ARS.  Start by allocating storage */

	*r = nmg_mrsv( m );	/* Make region, empty shell, vertex */
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);

	verts = (struct vertex **)bu_calloc( arip->ncurves * (arip->pts_per_curve+1),
		sizeof(struct vertex *),
		"rt_tor_tess *verts[]" );

	/*
	 *  Draw the "waterlines", by tracing each curve.
	 *  n+1th point is first point replicated by import code.
	 */
	k = arip->pts_per_curve-2;	/* next to last point on curve */
	for( i = 0; i < arip->ncurves-1; i++ )  {
		int double_ended;

		if( k != 1 && VAPPROXEQUAL( &arip->curves[i][1*ELEMENTS_PER_VECT], &arip->curves[i][k*ELEMENTS_PER_VECT], tol->dist ) )
			double_ended = 1;
		else
			double_ended = 0;

		for( j = 0; j < arip->pts_per_curve; j++ )  {
			struct vertex **corners[3];


			if( double_ended &&
			     i != 0 &&
			     ( j == 0 || j == k || j == arip->pts_per_curve-1 ) )
					continue;

			/*
			 *  First triangular face
			 */
			if( bn_3pts_distinct( ARS_PT(0,0), ARS_PT(1,1),
				ARS_PT(0,1), tol )
			   && !bn_3pts_collinear( ARS_PT(0,0), ARS_PT(1,1),
				ARS_PT(0,1), tol )
			)  {
				/* Locate these points, if previously mentioned */
				FIND_IJ(0, 0);
				FIND_IJ(1, 1);
				FIND_IJ(0, 1);

				/* Construct first face topology, CCW order */
				corners[0] = &verts[IJ(0,0)];
				corners[1] = &verts[IJ(0,1)];
				corners[2] = &verts[IJ(1,1)];

				if( (fu = nmg_cmface( s, corners, 3 )) == (struct faceuse *)0 )  {
					bu_log("rt_ars_tess() nmg_cmface failed, skipping face a[%d][%d]\n",
						i,j);
				}

				/* Associate vertex geometry, if new */
				ASSOC_GEOM( 0, 0, 0 );
				ASSOC_GEOM( 1, 0, 1 );
				ASSOC_GEOM( 2, 1, 1 );
				if( nmg_calc_face_g( fu ) )
				{
					bu_log( "Degenerate face created, will kill it later\n" );
					bu_ptbl_ins( &kill_fus, (long *)fu );
				}
			}

			/*
			 *  Second triangular face
			 */
			if( bn_3pts_distinct( ARS_PT(1,0), ARS_PT(1,1),
				ARS_PT(0,0), tol )
			   && !bn_3pts_collinear( ARS_PT(1,0), ARS_PT(1,1),
				ARS_PT(0,0), tol )
			)  {
				/* Locate these points, if previously mentioned */
				FIND_IJ(1, 0);
				FIND_IJ(1, 1);
				FIND_IJ(0, 0);

				/* Construct second face topology, CCW */
				corners[0] = &verts[IJ(1,0)];
				corners[1] = &verts[IJ(0,0)];
				corners[2] = &verts[IJ(1,1)];

				if( (fu = nmg_cmface( s, corners, 3 )) == (struct faceuse *)0 )  {
					bu_log("rt_ars_tess() nmg_cmface failed, skipping face b[%d][%d]\n",
						i,j);
				}

				/* Associate vertex geometry, if new */
				ASSOC_GEOM( 0, 1, 0 );
				ASSOC_GEOM( 1, 0, 0 );
				ASSOC_GEOM( 2, 1, 1 );
				if( nmg_calc_face_g( fu ) )
				{
					bu_log( "Degenerate face created, will kill it later\n" );
					bu_ptbl_ins( &kill_fus, (long *)fu );
				}
			}
		}
	}

	bu_free( (char *)verts, "rt_ars_tess *verts[]" );

	/* kill any degenerate faces that may have been created */
	for( i=0 ; i<BU_PTBL_END( &kill_fus ) ; i++ )
	{
		fu = (struct faceuse *)BU_PTBL_GET( &kill_fus, i );
		NMG_CK_FACEUSE( fu );
		(void)nmg_kfu( fu );
	}

	/* ARS solids are often built with incorrect face normals.
	 * Don't depend on them to be correct.
	 */
	nmg_fix_normals( s , tol );

	/* set edge's is_real flag */
	nmg_mark_edges_real( &s->l.magic );

	/* Compute "geometry" for region and shell */
	nmg_region_a( *r, tol );

	return(0);
}

int
rt_ars_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
	register struct rt_ars_internal *ars=(struct rt_ars_internal *)intern->idb_ptr;
	Tcl_DString	ds;
	struct bu_vls	vls;
	int		i,j;

	RT_ARS_CK_MAGIC( ars );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	if( attr == (char *)NULL ) {
		bu_vls_strcpy( &vls, "ars" );
		bu_vls_printf( &vls, " NC %d PPC %d", ars->ncurves, ars->pts_per_curve );
		for( i=0 ; i<ars->ncurves ; i++ ) {
			bu_vls_printf( &vls, " C%d {", i );
			for( j=0 ; j<ars->pts_per_curve ; j++ ) {
				bu_vls_printf( &vls, " { %.25g %.25g %.25g }",
					       V3ARGS( &ars->curves[i][j*3] ) );
			}
			bu_vls_printf( &vls, " }" );
		}
	}
	else if( !strcmp( attr, "NC" ) ) {
		bu_vls_printf( &vls, "%d", ars->ncurves );
	}
	else if( !strcmp( attr, "PPC" ) ) {
		bu_vls_printf( &vls, "%d", ars->pts_per_curve );
	}
	else if( attr[0] == 'C' ) {
		char *ptr;

		if( attr[1] == '\0' ) {
			/* all the curves */
			for( i=0 ; i<ars->ncurves ; i++ ) {
				bu_vls_printf( &vls, " C%d {", i );
				for( j=0 ; j<ars->pts_per_curve ; j++ ) {
					bu_vls_printf( &vls, " { %.25g %.25g %.25g }",
						       V3ARGS( &ars->curves[i][j*3] ) );
				}
				bu_vls_printf( &vls, " }" );
			}
		}
		else if( !isdigit( attr[1] ) ) {
			Tcl_SetResult( interp,
			      "ERROR: illegal argument, must be NC, PPC, C, C#, or C#P#\n",
			      TCL_STATIC );
			return( TCL_ERROR );
		}

		if( (ptr=strchr( attr, 'P' )) ) {
			/* a specific point on a specific curve */
			if( !isdigit( *(ptr+1) ) ) {
			   Tcl_SetResult( interp,
			       "ERROR: illegal argument, must be NC, PPC, C, C#, or C#P#\n",
				TCL_STATIC );
			   return( TCL_ERROR );
			}
			j = atoi( (ptr+1) );
			*ptr = '\0';
			i = atoi( &attr[1] );
			bu_vls_printf( &vls, "%.25g %.25g %.25g",
				 V3ARGS( &ars->curves[i][j*3] ) );
		}
		else {
			/* the entire curve */
			i = atoi( &attr[1] );
			for( j=0 ; j<ars->pts_per_curve ; j++ ) {
				bu_vls_printf( &vls, " { %.25g %.25g %.25g }",
					       V3ARGS( &ars->curves[i][j*3] ) );
			}
		}
	}
	Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );
	bu_vls_free( &vls );
	return( TCL_OK );
}

int
rt_ars_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
	struct rt_ars_internal		*ars;
	int				i,j,k;
	int				len;
	fastf_t				*array;

	RT_CK_DB_INTERNAL( intern );

	ars = (struct rt_ars_internal *)intern->idb_ptr;
	RT_ARS_CK_MAGIC( ars );

	while( argc >= 2 ) {
		if( !strcmp( argv[0], "NC" ) ) {
			/* change number of curves */
			i = atoi( argv[1] );
			if( i < ars->ncurves ) {
				for( j=i ; j<ars->ncurves ; j++ )
					bu_free( (char *)ars->curves[j], "ars->curves[j]" );
				ars->curves = (fastf_t **)bu_realloc( ars->curves,
						    i*sizeof( fastf_t *), "ars->curves" );
				ars->ncurves = i;
			}
			else if( i > ars->ncurves ) {
				ars->curves = (fastf_t **)bu_realloc( ars->curves,
						    i*sizeof( fastf_t *), "ars->curves" );
				if( ars->pts_per_curve ) {
				        /* new curves are duplicates of the last */
					for( j=ars->ncurves ; j<i ; j++ ) {
					    ars->curves[j] = (fastf_t *)bu_malloc(
						 ars->pts_per_curve * 3 * sizeof( fastf_t ),
								    "ars->curves[j]" );
					    for( k=0 ; k<ars->pts_per_curve ; k++ ) {
						 if ( j ) {
							 VMOVE( &ars->curves[j][k*3],
								&ars->curves[j-1][k*3] );
						 }
						 else {
							 VSETALL(&ars->curves[j][k*3], 0.0);
						 }
					    }
					}
				}
				else {
					for( j=ars->ncurves ; j<i ; j++ ) {
						ars->curves[j] = NULL;
					}
				}
				ars->ncurves = i;
			}
		}
		else if( !strcmp( argv[0], "PPC" ) ) {
			/* change the number of points per curve */
			i = atoi( argv[1] );
			if( i < 3 ) {
				Tcl_SetResult( interp,
				      "ERROR: must have at least 3 points per curve\n",
				      TCL_STATIC );
				return( TCL_ERROR );
			}
			if( i < ars->pts_per_curve ) {
				for( j=0 ; j<ars->ncurves ; j++ ) {
					ars->curves[j] = bu_realloc( ars->curves[j],
						    i * 3 * sizeof( fastf_t ),
						    "ars->curves[j]" );
				}
				ars->pts_per_curve = i;
			}
			else if( i > ars->pts_per_curve ) {
				for( j=0 ; j<ars->ncurves ; j++ ) {
					ars->curves[j] = bu_realloc( ars->curves[j],
						    i * 3 * sizeof( fastf_t ),
						    "ars->curves[j]" );
					/* new points are duplicates of last */
					for( k=ars->pts_per_curve ; k<i ; k++ ) {
						if( k ) {
							VMOVE( &ars->curves[j][k*3],
							       &ars->curves[j][(k-1)*3] );
						}
						else {
							VSETALL( &ars->curves[j][k*3], 0 );
						}
					}
				}
				ars->pts_per_curve = i;
			}
		}
		else if( argv[0][0] == 'C' ) {
			if( isdigit( argv[0][1] ) ) {
				char *ptr;

				/* a specific curve */
				if( (ptr=strchr( argv[0], 'P' )) ) {
					/* a specific point on this curve */
					i = atoi( &argv[0][1] );
					j = atoi( ptr+1 );
					len = 3;
					array = &ars->curves[i][j*3];
					if( tcl_list_to_fastf_array( interp, argv[1],
						   &array,
						   &len )!= len ) {
						Tcl_SetResult( interp,
						    "WARNING: incorrect number of parameters provided for a point\n",
						       TCL_STATIC );
					}
				}
				else {
					/* one complete curve */
					i = atoi( &argv[0][1] );
					len = ars->pts_per_curve * 3;
					ptr = argv[1];
					while( *ptr ) {
						if( *ptr == '{' || *ptr == '}' )
							*ptr = ' ';
						ptr++;
					}
					if( !ars->curves[i] ) {
						ars->curves[i] = (fastf_t *)bu_calloc(
								  ars->pts_per_curve * 3,
								  sizeof( fastf_t ),
								  "ars->curves[i]" );
					}
					if( tcl_list_to_fastf_array( interp, argv[1],
						   &ars->curves[i],
						   &len ) != len ) {
						Tcl_SetResult( interp,
						    "WARNING: incorrect number of parameters provided for a curve\n",
						       TCL_STATIC );
					}
				}
			}
			else {
				Tcl_SetResult( interp,
				  "ERROR: Illegal argument, must be NC, PPC, C#, or C#P#\n",
				  TCL_STATIC );
				return( TCL_ERROR );
			}
		}
		else {
			Tcl_SetResult( interp,
				 "ERROR: Illegal argument, must be NC, PPC, C#, or C#P#\n",
				 TCL_STATIC );
			return( TCL_ERROR );
		}
		argc -= 2;
		argv += 2;
	}

	return( TCL_OK );
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
