/*                         G _ X X X . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** \addtogroup g */

/*@{*/
/** @file g_xxx.c
 *	Intersect a ray with a. 
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_xxx_internal --- parameters for solid
 *	define xxx_specific --- raytracing form, possibly w/precomuted terms
 *	define rt_xxx_parse --- struct bu_structparse for "db get", "db adjust", ...
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_xxx_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_XXX, increment ID_MAXIMUM
 *	edit db_scan.c to add the new solid to db_scan()
 *	edit Cakefile to add g_xxx.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_xxx() routine
 *	go to /cad/conv and edit g2asc.c and asc2g.c to support the new solid
 *	go to /cad/librt and edit tcl.c to add the new solid to 
 *		rt_solid_type_lookup[]
 *		also add the interface table and to rt_id_solid() in table.c
 *	go to /cad/mged and create the edit support
 *
 *  Authors -
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
/*@}*/

#ifndef lint
static const char RCSxxx[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

#if 0
/* parameters for solid, internal representation
 * This goes in rtgeom.h
 */
/* parameters for solid, internal representation */
struct rt_xxx_internal {
	long	magic;
	vect_t	v;
};
#define RT_XXX_INTERNAL_MAGIC	0xxx
#define RT_XXX_CK_MAGIC(_p)	BU_CKMAG(_p,RT_XXX_INTERNAL_MAGIC,"rt_xxx_internal")
#endif

/* ray tracing form of solid, including precomputed terms */
struct xxx_specific {
	vect_t	xxx_V;
};

/**
 *  			R T _ X X X _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid XXX, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	XXX is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct xxx_specific is created, and it's address is stored in
 *  	stp->st_specific for use by xxx_shot().
 */
int
rt_xxx_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_xxx_internal		*xxx_ip;
	register struct xxx_specific	*xxx;
	const struct bn_tol		*tol = &rtip->rti_tol;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);
}

/**
 *			R T _ X X X _ P R I N T
 */
void
rt_xxx_print( stp )
register const struct soltab *stp;
{
	register const struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
}

/**
 *  			R T _ X X X _ S H O T
 *  
 *  Intersect a ray with a xxx.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_xxx_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
	register struct seg *segp;
	const struct bn_tol	*tol = &ap->a_rt_i->rti_tol;

	return(0);			/* MISS */
}

#define RT_XXX_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/**
 *			R T _ X X X _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_xxx_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/**
 *  			R T _ X X X _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_xxx_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/**
 *			R T _ X X X _ C U R V E
 *
 *  Return the curvature of the xxx.
 */
void
rt_xxx_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/**
 *  			R T _ X X X _ U V
 *  
 *  For a hit on the surface of an xxx, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_xxx_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;
}

/**
 *		R T _ X X X _ F R E E
 */
void
rt_xxx_free( stp )
register struct soltab *stp;
{
	register struct xxx_specific *xxx =
		(struct xxx_specific *)stp->st_specific;

	bu_free( (char *)xxx, "xxx_specific" );
}

/**
 *			R T _ X X X _ C L A S S
 */
int
rt_xxx_class( stp, min, max, tol )
const struct soltab    *stp;
const vect_t		min, max;
const struct bn_tol    *tol;
{
	return RT_CLASSIFY_UNIMPLEMENTED;
}

/**
 *			R T _ X X X _ P L O T
 */
int
rt_xxx_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
const struct rt_tess_tol *ttol;
const struct bn_tol	*tol;
{
	LOCAL struct rt_xxx_internal	*xxx_ip;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);

	return(-1);
}

/**
 *			R T _ X X X _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_xxx_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
const struct rt_tess_tol *ttol;
const struct bn_tol	*tol;
{
	LOCAL struct rt_xxx_internal	*xxx_ip;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);

	return(-1);
}

/**
 *			R T _ X X X _ I M P O R T
 *
 *  Import an XXX from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_xxx_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
const struct bu_external	*ep;
register const mat_t		mat;
const struct db_i		*dbip;
{
	LOCAL struct rt_xxx_internal	*xxx_ip;
	union record			*rp;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != ID_SOLID )  {
		bu_log("rt_xxx_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_XXX;
	ip->idb_meth = &rt_functab[ID_XXX];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_xxx_internal), "rt_xxx_internal");
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	xxx_ip->magic = RT_XXX_INTERNAL_MAGIC;

	MAT4X3PNT( xxx_ip->xxx_V, mat, &rp->s.s_values[0] );

	return(0);			/* OK */
}

/**
 *			R T _ X X X _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_xxx_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
const struct rt_db_internal	*ip;
double				local2mm;
const struct db_i		*dbip;
{
	struct rt_xxx_internal	*xxx_ip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_XXX )  return(-1);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "xxx external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
	rec->s.s_type = XXX;	/* GED primitive type from db.h */

	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */


	/* convert from local editing units to mm and export
	 * to database record format
	 *
	 * Warning: type conversion: double to float
	 */
	VSCALE( &rec->s.s_values[0], xxx_ip->xxx_V, local2mm );
	rec->s.s_values[3] = xxx_ip->xxx_radius * local2mm;

	return(0);
}



/**
 *			R T _ X X X _ I M P O R T 5
 *
 *  Import an XXX from the database format to the internal format.
 *  Note that the data read will be in network order.  This means
 *  Big-Endian integers and IEEE doubles for floating point.
 *
 *  Apply modeling transformations as well.
 *
 */
int
rt_xxx_import5( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
const struct bu_external	*ep;
register const mat_t		mat;
const struct db_i		*dbip;
{
	LOCAL struct rt_xxx_internal	*xxx_ip;
	fastf_t				vv[ELEMENTS_PER_VECT*1];

	RT_CK_DB_INTERNAL(ip)
	BU_CK_EXTERNAL( ep );

	BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*4 );

	/* set up the internal structure */
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_XXX;
	ip->idb_meth = &rt_functab[ID_XXX];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_xxx_internal), "rt_xxx_internal");
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	xxx_ip->magic = RT_XXX_INTERNAL_MAGIC;

	/* Convert the data in ep->ext_buf into internal format.
	 * Note the conversion from network data 
	 * (Big Endian ints, IEEE double floating point) to host local data
	 * representations.
	 */
	ntohd( (unsigned char *)&vv, (char *)ep->ext_buf, ELEMENTS_PER_VECT*1 );

	/* Apply the modeling transformation */
	MAT4X3PNT( xxx_ip->v, mat, vv );

	return(0);			/* OK */
}

/**
 *			R T _ X X X _ E X P O R T 5
 *
 *  Export an XXX from internal form to external format.
 *  Note that this means converting all integers to Big-Endian format
 *  and floating point data to IEEE double.
 *
 *  Apply the transformation to mm units as well.
 */
int
rt_xxx_export5( ep, ip, local2mm, dbip )
struct bu_external		*ep;
const struct rt_db_internal	*ip;
double				local2mm;
const struct db_i		*dbip;
{
	struct rt_xxx_internal	*xxx_ip;
	fastf_t			vec[ELEMENTS_PER_VECT];

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_XXX )  return(-1);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_VECT;
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "xxx external");


	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */
	VSCALE( vec, xxx_ip->v, local2mm );

	/* Convert from internal (host) to database (network) format */
	htond( ep->ext_buf, (unsigned char *)vec, ELEMENTS_PER_VECT*1 );

	return 0;
}

/**
 *			R T _ X X X _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_xxx_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
const struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_xxx_internal	*xxx_ip =
		(struct rt_xxx_internal *)ip->idb_ptr;
	char	buf[256];

	RT_XXX_CK_MAGIC(xxx_ip);
	bu_vls_strcat( str, "truncated general xxx (XXX)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		xxx_ip->v[X] * mm2local,
		xxx_ip->v[Y] * mm2local,
		xxx_ip->v[Z] * mm2local );
	bu_vls_strcat( str, buf );

	return(0);
}

/**
 *			R T _ X X X _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_xxx_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_xxx_internal	*xxx_ip;

	RT_CK_DB_INTERNAL(ip);
	xxx_ip = (struct rt_xxx_internal *)ip->idb_ptr;
	RT_XXX_CK_MAGIC(xxx_ip);
	xxx_ip->magic = 0;			/* sanity */

	bu_free( (char *)xxx_ip, "xxx ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/**
 *			R T _ X X X _ X F O R M
 *
 *  Create transformed version of internal form.  Free *ip if requested.
 *  Implement this if it's faster than doing an export/import cycle.
 */
int
rt_xxx_xform( op, mat, ip, free )
struct rt_db_internal	*op;
const mat_t		mat;
struct rt_db_internal	*ip;
int			free;
{
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
