/*
 *			G _ H F . C
 *
 *  Purpose -
 *	Intersect a ray with a height field,
 *	where the heights are imported from an external data file,
 *	and where some (or all) of the parameters of that data file
 *	may be read in from an external control file.
 *
 *  Authors -
 *	Michael John Muuss
 *	(Christopher T. Johnson, GSI)
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

struct rt_hf_internal {
	long	magic;
	char	cfile[128];		/* name of control file (optional) */
	char	dfile[128];		/* name of data file */
	char	fmt[8];			/* CV style file format descriptor */
	int	w;			/* # samples wide of data file.  ("i", "x") */
	int	n;			/* nlines of data file.  ("j", "y") */
	int	shorts;			/* !0 --> memory array is short, not double */
	fastf_t	file2mm;		/* scale factor to cvt file units to mm */
	vect_t	v;			/* origin of HT in model space */
	vect_t	x;			/* model vect corresponding to "w" dir (will be unitized) */
	vect_t	y;			/* model vect corresponding to "n" dir (will be unitized) */
	fastf_t	xlen;			/* model len of HT in "w" dir */
	fastf_t	ylen;			/* model len of HT in "n" dir */
};
#define RT_HF_INTERNAL_MAGIC	0x4846494d
#define RT_HF_CK_MAGIC(_p)	RT_CKMAG(_p,RT_HF_INTERNAL_MAGIC,"rt_hf_internal")
#define HF_O(m)			offsetof(struct rt_hf_internal, m)

/*
 *  Description of the external string description of the HF.
 */
CONST struct structparse rt_hf_parse[] = {
	{"%s",	128,	"cfile",	offsetofarray(struct rt_hf_internal, cfile), FUNC_NULL},
	{"%s",	128,	"dfile",	offsetofarray(struct rt_hf_internal, dfile), FUNC_NULL},
	{"%s",	8,	"fmt",		offsetofarray(struct rt_hf_internal, fmt), FUNC_NULL},
	{"%d",	1,	"w",		HF_O(w),		FUNC_NULL },
	{"%d",	1,	"n",		HF_O(n),		FUNC_NULL },
	{"%d",	1,	"shorts",	HF_O(shorts),		FUNC_NULL },
	{"%f",	1,	"file2mm",	HF_O(file2mm),		FUNC_NULL },
	{"%f",	3,	"v",		HF_O(v[0]),		FUNC_NULL },
	{"%f",	3,	"x",		HF_O(x[0]),		FUNC_NULL },
	{"%f",	3,	"y",		HF_O(y[0]),		FUNC_NULL },
};

struct hf_specific {
	vect_t	hf_V;
};

/*
 *  			R T _ H F _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid HF, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	HF is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct hf_specific is created, and it's address is stored in
 *  	stp->st_specific for use by hf_shot().
 */
int
rt_hf_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_hf_internal		*xip;
	register struct hf_specific	*hf;
	CONST struct rt_tol		*tol = &rtip->rti_tol;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_hf_internal *)ip->idb_ptr;
	RT_HF_CK_MAGIC(xip);
}

/*
 *			R T _ H F _ P R I N T
 */
void
rt_hf_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct hf_specific *hf =
		(struct hf_specific *)stp->st_specific;
}

/*
 *  			R T _ H F _ S H O T
 *  
 *  Intersect a ray with a hf.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_hf_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct hf_specific *hf =
		(struct hf_specific *)stp->st_specific;
	register struct seg *segp;
	CONST struct rt_tol	*tol = &ap->a_rt_i->rti_tol;

	return(0);			/* MISS */
}

#define RT_HF_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ H F _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_hf_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ H F _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_hf_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct hf_specific *hf =
		(struct hf_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ H F _ C U R V E
 *
 *  Return the curvature of the hf.
 */
void
rt_hf_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct hf_specific *hf =
		(struct hf_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ H F _ U V
 *  
 *  For a hit on the surface of an hf, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_hf_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct hf_specific *hf =
		(struct hf_specific *)stp->st_specific;
}

/*
 *		R T _ H F _ F R E E
 */
void
rt_hf_free( stp )
register struct soltab *stp;
{
	register struct hf_specific *hf =
		(struct hf_specific *)stp->st_specific;

	rt_free( (char *)hf, "hf_specific" );
}

/*
 *			R T _ H F _ C L A S S
 */
int
rt_hf_class()
{
	return(0);
}

/*
 *			R T _ H F _ P L O T
 */
int
rt_hf_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_hf_internal	*xip;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_hf_internal *)ip->idb_ptr;
	RT_HF_CK_MAGIC(xip);

	return(-1);
}

/*
 *			R T _ H F _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_hf_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_hf_internal	*xip;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_hf_internal *)ip->idb_ptr;
	RT_HF_CK_MAGIC(xip);

	return(-1);
}

/*
 *			R T _ H F _ I M P O R T
 *
 *  Import an HF from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_hf_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	LOCAL struct rt_hf_internal	*xip;
	union record			*rp;
	struct rt_vls			str;

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_STRSOL )  {
		rt_log("rt_hf_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_HF;
	ip->idb_ptr = rt_calloc( 1, sizeof(struct rt_hf_internal), "rt_hf_internal");
	xip = (struct rt_hf_internal *)ip->idb_ptr;
	xip->magic = RT_HF_INTERNAL_MAGIC;

	/* Provide defaults.  Only non-defaulted fields are dfile, w, n */
	xip->shorts = 1;		/* for now */
	xip->file2mm = 1.0;
	VSETALL( xip->v, 0 );
	VSET( xip->x, 1, 0, 0 );
	VSET( xip->y, 0, 1, 0 );
	xip->xlen = 1000;
	xip->ylen = 1000;
	strcpy( xip->fmt, "nd" );

	/* Process parameters found in .g file */
	rt_vls_init( &str );
	rt_vls_strcpy( &str, rp->ss.ss_args );
	if( rt_structparse( &str, rt_hf_parse, (char *)xip ) < 0 )  {
		rt_vls_free( &str );
err1:
		rt_free( (char *)xip , "rt_hf_import: xip" );
		ip->idb_type = ID_NULL;
		ip->idb_ptr = (genptr_t)NULL;
		return -2;
	}
	rt_vls_free( &str );

	/* If "cfile" was specified, process parameters from there */
	if( xip->cfile[0] )  {
		FILE	*fp;

		RES_ACQUIRE( &rt_g.res_syscall );
		fp = fopen( xip->cfile, "r" );
		RES_RELEASE( &rt_g.res_syscall );
		if( !fp )  {
			perror(xip->cfile);
			rt_log("rt_hf_import() unable to open cfile=%s\n", xip->cfile);
			goto err1;
		}
		rt_vls_init( &str );
		while( rt_vls_gets( &str, fp ) >= 0 )
			rt_vls_strcat( &str, " " );
		RES_ACQUIRE( &rt_g.res_syscall );
		fclose(fp);
		RES_RELEASE( &rt_g.res_syscall );
		if( rt_structparse( &str, rt_hf_parse, (char *)xip ) < 0 )  {
			rt_log("rt_hf_import() parse error in cfile input '%s'\n",
				rt_vls_addr(&str) );
			rt_vls_free( &str );
			goto err1;
		}
	}

	/* Check for reasonable values */
	if( !xip->dfile[0] )  {
		rt_log("rt_hf_import() no dfile specified\n");
		goto err1;
	}
	if( xip->w < 2 || xip->n < 2 )  {
		rt_log("rt_hf_import() w=%d, n=%d too small\n");
		goto err1;
	}

	/* Apply modeling transformations */

	/* Load data file, and transform to internal format */

	return(0);			/* OK */
}

/*
 *			R T _ H F _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_hf_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_hf_internal	*xip;
	union record		*rec;
	struct rt_vls		str;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_HF )  return(-1);
	xip = (struct rt_hf_internal *)ip->idb_ptr;
	RT_HF_CK_MAGIC(xip);

	xip->file2mm /= local2mm;		/* xform */

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record) * DB_SS_NGRAN;
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "hf external");
	rec = (union record *)ep->ext_buf;

	RT_VLS_INIT( &str );
	rt_vls_structprint( &str, rt_hf_parse, (char *)&xip );

	/* Any changes made by solid editing affect .g file only,
	 * and not the cfile, if specified.
	 */

	rec->s.s_id = DBID_STRSOL;
	strncpy( rec->ss.ss_keyword, "hf", NAMESIZE-1 );
	strncpy( rec->ss.ss_args, rt_vls_addr(&str), DB_SS_LEN-1 );
	rt_vls_free( &str );

	return(0);
}

/*
 *			R T _ H F _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_hf_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_hf_internal	*xip =
		(struct rt_hf_internal *)ip->idb_ptr;
	char	buf[256];

	RT_HF_CK_MAGIC(xip);
	rt_vls_strcat( str, "Height Field (HF)\n");

	sprintf(buf, "\tV (%g, %g, %g)\n",
		xip->v[X] * mm2local,
		xip->v[Y] * mm2local,
		xip->v[Z] * mm2local );
	rt_vls_strcat( str, buf );

	return(0);
}

/*
 *			R T _ H F _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_hf_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_hf_internal	*xip;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_hf_internal *)ip->idb_ptr;
	RT_HF_CK_MAGIC(xip);
	xip->magic = 0;			/* sanity */

	rt_free( (char *)xip, "hf ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
