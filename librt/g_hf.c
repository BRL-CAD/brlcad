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
#include <fcntl.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"

#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

/*
 *  Description of the external string description of the HF.
 *
 *  There are two versions of this parse table.
 *  The string solid in the .g file can set any parameter.
 *  The indirect control file (cfile) can only set parameters
 *  relating to dfile parameters, and not to the geometric
 *  position, orientation, and scale of the HF's bounding RPP.
 *
 *  In general, the cfile should be thought of as describing
 *  the data arrangement of the dfile, and the string solid should
 *  be thought of as describing the "geometry" of the height
 *  field's bounding RPP.
 *
 *  The string solid is parsed first.  If a cfile is present, it is
 *  parsed second, and any parameters specified in the cfile override
 *  the values taken from the string solid.
 */
#define HF_O(m)			offsetof(struct rt_hf_internal, m)

/* All fields valid in string solid */
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
	{"%f",	1,	"xlen",		HF_O(xlen),		FUNC_NULL },
	{"%f",	1,	"ylen",		HF_O(ylen),		FUNC_NULL },
	{"%f",	1,	"zscale",	HF_O(zscale),		FUNC_NULL },
	{"",	0,	(char *)0,	0,			FUNC_NULL }
};
/* Subset of fields found in cfile */
CONST struct structparse rt_hf_cparse[] = {
	{"%s",	128,	"dfile",	offsetofarray(struct rt_hf_internal, dfile), FUNC_NULL},
	{"%s",	8,	"fmt",		offsetofarray(struct rt_hf_internal, fmt), FUNC_NULL},
	{"%d",	1,	"w",		HF_O(w),		FUNC_NULL },
	{"%d",	1,	"n",		HF_O(n),		FUNC_NULL },
	{"%d",	1,	"shorts",	HF_O(shorts),		FUNC_NULL },
	{"%f",	1,	"file2mm",	HF_O(file2mm),		FUNC_NULL },
	{"",	0,	(char *)0,	0,			FUNC_NULL }
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

	return -1;	/* FAIL */
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

	RT_CK_RTI(ap->a_rt_i);
	RT_CK_TOL(tol);

	return(0);			/* MISS */
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
	register unsigned short		*sp;
	vect_t		xbasis;
	vect_t		ybasis;
	vect_t		zbasis;
	point_t		start;
	point_t		cur;
	int		x;
	int		y;
	int		cmd;
	int		step;
	int		half_step;
	int		goal;

	RT_CK_DB_INTERNAL(ip);
	xip = (struct rt_hf_internal *)ip->idb_ptr;
	RT_HF_CK_MAGIC(xip);

	if( !xip->shorts )  rt_bomb("rt_hf_plot() does shorts only, for now\n");

	VSCALE( xbasis, xip->x, xip->xlen / (xip->w - 1) );
	VSCALE( ybasis, xip->y, xip->ylen / (xip->n - 1) );
	VCROSS( zbasis, xip->x, xip->y );
	VSCALE( zbasis, zbasis, xip->zscale * xip->file2mm );

	/* XXX This should be set from the tessellation tolerance */
	goal = 20000;

	/* Draw the 4 corners of the base plate */
	RT_ADD_VLIST( vhead, xip->v, RT_VLIST_LINE_MOVE );

	VJOIN1( start, xip->v, xip->xlen, xip->x );
	RT_ADD_VLIST( vhead, start, RT_VLIST_LINE_DRAW );

	VJOIN2( start, xip->v, xip->xlen, xip->x, xip->ylen, xip->y );
	RT_ADD_VLIST( vhead, start, RT_VLIST_LINE_DRAW );

	VJOIN1( start, xip->v, xip->ylen, xip->y );
	RT_ADD_VLIST( vhead, start, RT_VLIST_LINE_DRAW );

	RT_ADD_VLIST( vhead, xip->v, RT_VLIST_LINE_DRAW );
	goal -= 5;

#define HF_GET(_p,_x,_y)	((_p)[(_y)*xip->w+(_x)])
	/*
	 *  Draw the four "ridge lines" at full resolution, for edge matching.
	 */
	/* X direction, Y=0, with edges down to base */
	RT_ADD_VLIST( vhead, xip->v, RT_VLIST_LINE_MOVE );
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, 0, 0 );
	for( x = 0; x < xip->w; x++ )  {
		VJOIN2( cur, xip->v, x, xbasis, *sp, zbasis );
		RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_DRAW );
		sp++;
	}
	VJOIN1( cur, xip->v, xip->xlen, xip->x );
	RT_ADD_VLIST( vhead, cur, RT_VLIST_LINE_DRAW );

	/* X direction, Y=n-1, with edges down to base */
	VJOIN1( start, xip->v, xip->ylen, xip->y );
	RT_ADD_VLIST( vhead, start, RT_VLIST_LINE_MOVE );
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, 0, xip->n - 1 );
	VJOIN1( start, xip->v, xip->ylen, xip->y );
	for( x = 0; x < xip->w; x++ )  {
		VJOIN2( cur, start, x, xbasis, *sp, zbasis );
		RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_DRAW );
		sp++;
	}
	VJOIN2( cur, xip->v, xip->xlen, xip->x, xip->ylen, xip->y );
	RT_ADD_VLIST( vhead, cur, RT_VLIST_LINE_DRAW );

	/* Y direction, X=0 */
	cmd = RT_VLIST_LINE_MOVE;
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, 0, 0 );
	for( y = 0; y < xip->n; y++ )  {
		VJOIN2( cur, xip->v, y, ybasis, *sp, zbasis );
		RT_ADD_VLIST(vhead, cur, cmd );
		cmd = RT_VLIST_LINE_DRAW;
		sp += xip->w;
	}

	/* Y direction, X=w-1 */
	cmd = RT_VLIST_LINE_MOVE;
	sp = &HF_GET((unsigned short *)xip->mp->apbuf, xip->w - 1, 0 );
	VJOIN1( start, xip->v, xip->xlen, xip->x );
	for( y = 0; y < xip->n; y++ )  {
		VJOIN2( cur, start, y, ybasis, *sp, zbasis );
		RT_ADD_VLIST(vhead, cur, cmd );
		cmd = RT_VLIST_LINE_DRAW;
		sp += xip->w;
	}
	goal -= 4 + 2 * (xip->w + xip->n);

	/* Apply relative tolerance, if specified */
	if( ttol->rel )  {
		int	rstep;
		rstep = xip->w;
		V_MAX( rstep, xip->n );
		step = (int)(ttol->rel * rstep);
	} else {
		/* No relative tol specified, limit drawing to 'goal' # of vectors */
		if( goal <= 0 )  return 0;		/* no vectors for interior */

		/* Compute data stride based upon producing no more than 'goal' vectors */
		step = ceil(sqrt( 2*(xip->w-1)*(xip->n-1) / (double)goal ));
	}
	if( step < 1 )  step = 1;
	if( (half_step = step/2) < 1 )  half_step = 1;

	/* Draw the contour lines in W (x) direction.  Don't redo ridges. */
	for( y = half_step; y < xip->n-half_step; y += step )  {
		VJOIN1( start, xip->v, y, ybasis );
		x = 0;
		sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y );
		VJOIN2( cur, start, x, xbasis, *sp, zbasis );
		RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_MOVE );
		x += half_step;
		sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y );
		for( ; x < xip->w; x += step )  {
			VJOIN2( cur, start, x, xbasis, *sp, zbasis );
			RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_DRAW );
			sp += step;
		}
		if( x != step+xip->w-1+step )  {
			x = xip->w - 1;
			sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y );
			VJOIN2( cur, start, x, xbasis, *sp, zbasis );
			RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_DRAW );
		}
	}

	/* Draw the contour lines in the N (y) direction */
	for( x = half_step; x < xip->w-half_step; x += step )  {
		VJOIN1( start, xip->v, x, xbasis );
		y = 0;
		sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y );
		VJOIN2( cur, start, y, ybasis, *sp, zbasis );
		RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_MOVE );
		y += half_step;
		for( ; y < xip->n; y += step )  {
			sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y );
			VJOIN2( cur, start, y, ybasis, *sp, zbasis );
			RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_DRAW );
		}
		if( y != step+xip->n-1+step )  {
			y = xip->n - 1;
			sp = &HF_GET((unsigned short *)xip->mp->apbuf, x, y );
			VJOIN2( cur, start, y, ybasis, *sp, zbasis );
			RT_ADD_VLIST(vhead, cur, RT_VLIST_LINE_DRAW );
		}
	}
	return 0;
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
	struct rt_mapped_file		*mp;
	vect_t				tmp;
	int				in_cookie;	/* format cookie */
	int				in_len;
	int				out_cookie;
	int				count;
	int				got;

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
	xip->zscale = 1;
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
		if( rt_structparse( &str, rt_hf_cparse, (char *)xip ) < 0 )  {
			rt_log("rt_hf_import() parse error in cfile input '%s'\n",
				rt_vls_addr(&str) );
			rt_vls_free( &str );
			goto err1;
		}
	}

	/* Check for reasonable values */
	if( !xip->dfile[0] )  {
		/* XXX Should create 2x2 data file instead, for positioning use (FPO) */
		rt_log("rt_hf_import() no dfile specified\n");
		goto err1;
	}
	if( xip->w < 2 || xip->n < 2 )  {
		rt_log("rt_hf_import() w=%d, n=%d too small\n");
		goto err1;
	}
	if( xip->xlen <= 0 || xip->ylen <= 0 )  {
		rt_log("rt_hf_import() xlen=%g, ylen=%g too small\n", xip->xlen, xip->ylen);
		goto err1;
	}

	/* Apply modeling transformations */
	MAT4X3PNT( tmp, mat, xip->v );
	VMOVE( xip->v, tmp );
	MAT4X3VEC( tmp, mat, xip->x );
	VMOVE( xip->x, tmp );
	MAT4X3VEC( tmp, mat, xip->y );
	VMOVE( xip->y, tmp );
	xip->xlen /= mat[15];
	xip->ylen /= mat[15];
	xip->zscale /= mat[15];

	VUNITIZE(xip->x);
	VUNITIZE(xip->y);

	/* Prepare for cracking input file format */
	if( (in_cookie = cv_cookie( xip->fmt )) == 0 )  {
		rt_log("rt_hf_import() fmt='%s' unknown\n", xip->fmt);
		goto err1;
	}
	in_len = cv_itemlen( in_cookie );

	/*
	 *  Load data file, and transform to internal format
	 */
	if( !(mp = rt_open_mapped_file( xip->dfile, "hf" )) )  {
		rt_log("rt_hf_import() unable to open '%s'\n", xip->dfile);
		goto err1;
	}
	xip->mp = mp;
	count = mp->buflen / in_len;

	/* If this data has already been mapped, all done */
	if( mp->apbuf )  return 0;		/* OK */

	/* Transform external data to internal format -- short or float */
	if( xip->shorts )  {
		mp->apbuflen = sizeof(unsigned short) * count;
		out_cookie = cv_cookie("hus");
	} else {
		mp->apbuflen = sizeof(float) * count;
		out_cookie = cv_cookie("hf");
	}

	if( in_cookie == out_cookie )  {
		/* Don't replicate the data, just re-use the pointer */
		mp->apbuf = mp->buf;
		return 0;		/* OK */
	}

	mp->apbuf = (genptr_t)rt_malloc( mp->apbuflen, "rt_hf_import apbuf[]" );
	got = cv_w_cookie( mp->apbuf, out_cookie, mp->apbuflen,
		mp->buf, in_cookie, count );
	if( got != count )  {
		rt_log("rt_hf_import(%s) cv_w_cookie count=%d, got=%d\n",
			xip->dfile, count, got );
	}

	return(0);			/* OK */
}

/*
 *			R T _ H F _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 *
 *  The meaning of the export here is slightly different than that of
 *  most other solids.  The cfile and dfile are not modified, only
 *  changes to the string solid parameters are placed back into the .g file.
 *  Note that any parameters taken from a cfile are included in the new
 *  string solid.  This isn't a problem, because if the cfile is changed
 *  (perhaps to substitute a different resolution height field of the same
 *  location in space), it's new parameters will override those stored
 *  in the string solid (including the dfile name).
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

	/* Apply any scale transformation */
	xip->xlen /= local2mm;
	xip->ylen /= local2mm;
	xip->zscale /= local2mm;

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

	RT_VLS_CHECK(str);
	RT_HF_CK_MAGIC(xip);
	rt_vls_printf( str, "Height Field (HF)  mm2local=%g\n", mm2local);
	rt_vls_structprint( str, rt_hf_parse, ip->idb_ptr );
	rt_vls_strcat( str, "\n" );

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

	RT_CK_MAPPED_FILE(xip->mp);
	rt_close_mapped_file(xip->mp);

	rt_free( (char *)xip, "hf ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
