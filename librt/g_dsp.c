/*
 *			G _ D S P . C
 *
 *  Purpose -
 *	Intersect a ray with a displacement map
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_dsp_internal --- parameters for solid
 *	define dsp_specific --- raytracing form, possibly w/precomuted terms
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_dsp_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_DSP, increment ID_MAXIMUM
 *	edit Cakefile to add g_dsp.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_dsp() routine
 *	go to /cad/mged and create the edit support
 *
 *  Authors -
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSdsp[] = "@(#)$Header$ (BRL)";
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


/* ray tracing form of solid, including precomputed terms */
struct dsp_specific {
	struct rt_dsp_internal dsp_i;
};

#define DSP_O(m) offsetof(struct rt_dsp_internal, m)
#define DSP_AO(a) offsetofarray(struct rt_dsp_internal, a)

static struct bu_structparse rt_dsp_parse[] = {
	{"%s",	DSP_NAME_LEN, "file", DSP_AO(dsp_file), FUNC_NULL },
	{"%d",	1, "xc", DSP_O(dsp_xcnt), FUNC_NULL },
	{"%d",	1, "yc", DSP_O(dsp_ycnt), FUNC_NULL },
	{"%d",	1, "xs", DSP_O(dsp_xs), FUNC_NULL },
	{"%d",	1, "ys", DSP_O(dsp_ys), FUNC_NULL },
	{"%d",	1, "ys", DSP_O(dsp_zs), FUNC_NULL },
	{"",	0, (char *)0, 0,			FUNC_NULL }
};


/*
 *  			R T _ D S P _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid DSP, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	DSP is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct dsp_specific is created, and it's address is stored in
 *  	stp->st_specific for use by dsp_shot().
 */
int
rt_dsp_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_dsp_internal		*dsp_ip;
	register struct dsp_specific	*dsp;
	CONST struct rt_tol		*tol = &rtip->rti_tol;

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);
}

/*
 *			R T _ D S P _ P R I N T
 */
void
rt_dsp_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;
}

/*
 *  			R T _ D S P _ S H O T
 *  
 *  Intersect a ray with a dsp.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_dsp_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;
	register struct seg *segp;
	CONST struct rt_tol	*tol = &ap->a_rt_i->rti_tol;

	return(0);			/* MISS */
}

#define RT_DSP_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ D S P _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_dsp_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ D S P _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_dsp_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ D S P _ C U R V E
 *
 *  Return the curvature of the dsp.
 */
void
rt_dsp_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ D S P _ U V
 *  
 *  For a hit on the surface of an dsp, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_dsp_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;
}

/*
 *		R T _ D S P _ F R E E
 */
void
rt_dsp_free( stp )
register struct soltab *stp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;

	rt_free( (char *)dsp, "dsp_specific" );
}

/*
 *			R T _ D S P _ C L A S S
 */
int
rt_dsp_class()
{
	return(0);
}

/*
 *			R T _ D S P _ P L O T
 */
int
rt_dsp_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

	return(-1);
}

/*
 *			R T _ D S P _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_dsp_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

	return(-1);
}

/*
 *			R T _ D S P _ I M P O R T
 *
 *  Import an DSP from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_dsp_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;
	union record			*rp;
	struct bu_vls			str;
	mat_t tmp;

#define IMPORT_FAIL(s) \
	bu_log("rt_ebm_import() '%s' %s\n", dsp_ip->dsp_file, s); \
	bu_free( (char *)dsp_ip , "rt_dsp_import: dsp_ip" ); \
	ip->idb_type = ID_NULL; \
	ip->idb_ptr = (genptr_t)NULL; \
	return -2


	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_STRSOL )  {
		rt_log("rt_dsp_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_DSP;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_dsp_internal), "rt_dsp_internal");
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	dsp_ip->magic = RT_DSP_INTERNAL_MAGIC;

	dsp_ip->dsp_file[0] = '\0';
	dsp_ip->dsp_xcnt = dsp_ip->dsp_ycnt = 0;
	dsp_ip->dsp_xs = dsp_ip->dsp_ys = dsp_ip->dsp_zs = 0.0;
	mat_idn(&dsp_ip->dsp_mtos);
	mat_idn(&dsp_ip->dsp_stom);
	bu_vls_init( &str );
	bu_vls_strcpy( &str, rp->ss.ss_args );
	if (bu_struct_parse( &str, rt_dsp_parse, (char *)dsp_ip ) < 0) {
		bu_vls_free( &str );
		IMPORT_FAIL("parse error");
	}
	bu_vls_free( &str );
	
	/* Validate parameters */
	if (dsp_ip->dsp_xcnt == 0 || dsp_ip->dsp_ycnt == 0) {
		IMPORT_FAIL("zero dimension on map");
	}
	
	/* Apply Modeling transoform */
	mat_copy(tmp, dsp_ip->dsp_mtos);
	mat_mul(dsp_ip->dsp_mtos, mat, dsp_ip->dsp_mtos);
	
	/* get file */
	if( !(dsp_ip->dsp_mp = rt_open_mapped_file( dsp_ip->dsp_file, "dsp" )) )  {
		IMPORT_FAIL("unable to open");
	}
	if (dsp_ip->dsp_mp->buflen != dsp_ip->dsp_xcnt*dsp_ip->dsp_ycnt*2) {
		IMPORT_FAIL("buffer wrong size");
	}
	


	return(0);			/* OK */
}

/*
 *			R T _ D S P _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_dsp_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_dsp_internal	*dsp_ip;
	union record		*rec;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_DSP )  return(-1);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record);
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "dsp external");
	rec = (union record *)ep->ext_buf;

	rec->s.s_id = ID_SOLID;
#if 0
	rec->s.s_type = DSP;	/* GED primitive type from db.h */
#endif
	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */


	/* convert from local editing units to mm and export
	 * to database record format
	 *
	 * Warning: type conversion: double to float
	 */
#if 0
	VSCALE( &rec->s.s_values[0], dsp_ip->dsp_V, local2mm );
	rec->s.s_values[3] = dsp_ip->dsp_radius * local2mm;
#endif
	return(0);
}

/*
 *			R T _ D S P _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_dsp_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_dsp_internal	*dsp_ip =
		(struct rt_dsp_internal *)ip->idb_ptr;
	struct bu_vls vls;



	RT_DSP_CK_MAGIC(dsp_ip);

	bu_vls_init( &vls );

	rt_vls_strcat( str, "Displacement Map\n");

	bu_vls_printf( &vls, "  file='%s' xc=%d yc=%d\n",
		dsp_ip->dsp_file, dsp_ip->dsp_xcnt, dsp_ip->dsp_ycnt);

	bu_vls_vlscat( str, &vls );
	bu_vls_free( &vls );

#if 0
	sprintf(buf, "\tV (%g, %g, %g)\n",
		dsp_ip->v[X] * mm2local,
		dsp_ip->v[Y] * mm2local,
		dsp_ip->v[Z] * mm2local );
	rt_vls_strcat( str, buf );
#endif

	return(0);
}

/*
 *			R T _ D S P _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_dsp_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_dsp_internal	*dsp_ip;

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);
	dsp_ip->magic = 0;			/* sanity */

	rt_free( (char *)dsp_ip, "dsp ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
