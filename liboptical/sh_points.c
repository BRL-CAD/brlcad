/*
 *			P O I N T S . C
 *
 *  Reads a file of u,v point locations and associated RGB color values.
 *  For each u,v texture mapping cell, this routine fills in the color
 *  of the "brightest" point contained in that cell (if any).
 *
 *  This routine was born in order to environment map the Yale Bright
 *  Star catalog data without under or over sampling the point sources.
 *  It was soon realized that making it "star" specific limited its
 *  usefulness.
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "spm.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

#define PT_NAME_LEN 128
struct points_specific {
	char	pt_file[PT_NAME_LEN];	/* Filename */
	int	pt_size;	/* number of bins around equator */
	spm_map_t *pt_map;	/* stuff */
};
#define POINTS_NULL	((struct points_specific *)0)
#define POINTS_O(m)	offsetof(struct points_specific, m)

struct bu_structparse points_parse[] = {
	{"%s",	PT_NAME_LEN, "file", offsetofarray(struct points_specific, pt_file),	FUNC_NULL },
	{"%d",	1, "size",		POINTS_O(pt_size),	FUNC_NULL },
	{"%d",	1, "w",			POINTS_O(pt_size),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	points_setup(), points_render();
HIDDEN void	points_print(), points_mfree();

CONST struct mfuncs points_mfuncs[] = {
	{MF_MAGIC,	"points",	0,		MFI_UV,		0,
	points_setup,	points_render,	points_print,	points_mfree },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};

HIDDEN
struct points {
	fastf_t	u;			/* u location */
	fastf_t	v;			/* v location */
	vect_t	color;			/* color of point */
	struct points	*next;		/* next point in list */
};

/*
 *			P O I N T S _ S E T U P
 *
 *  Returns -
 *	<0	failed
 *	>0	success
 */
HIDDEN int
points_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
struct mfuncs           *mfp;
struct rt_i             *rtip;  /* New since 4.4 release */
{
	register struct points_specific *ptp;
	char	buf[513];
	FILE	*fp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( ptp, points_specific );
	*dpp = (char *)ptp;

	/* get or default shader parameters */
	ptp->pt_file[0] = '\0';
	ptp->pt_size = -1;
	if( bu_struct_parse( matparm, points_parse, (char *)ptp ) < 0 )  {
		rt_free( (char *)ptp, "points_specific" );
		return(-1);
	}
	if( ptp->pt_size < 0 )
		ptp->pt_size = 512;
	if( ptp->pt_file[0] == '\0' )
		strcpy( ptp->pt_file, "points.ascii" );

	/* create a spherical data structure to bin point lists into */
	if( (ptp->pt_map = spm_init( ptp->pt_size, sizeof(struct points) )) == SPM_NULL )
		goto fail;

	/* read in the data */
	if( (fp = fopen(ptp->pt_file, "r")) == NULL ) {
		rt_log("points_setup: can't open \"%s\"\n", ptp->pt_file);
		goto fail;
	}
	while( fgets(buf,512,fp) != NULL ) {
		double	u, v, mag;
		struct points	*headp, *pp;

		if( buf[0] == '#' )
			continue;		/* comment */

		pp = (struct points *)rt_calloc(1, sizeof(struct points), "point");
		sscanf( buf, "%lf%lf%lf", &u, &v, &mag );
		pp->u = u;
		pp->v = v;
		pp->color[0] = mag;
		pp->color[1] = mag;
		pp->color[2] = mag;

		/* find a home for it */
		headp = (struct points *)spm_get( ptp->pt_map, u, v );
		pp->next = headp->next;
		headp->next = pp;
	}
	(void)fclose(fp);

	return(1);
fail:
	rt_free( (char *)ptp, "points_specific" );
	return(-1);
}

/*
 *  			P O I N T S _ R E N D E R
 *  
 *  Given a u,v coordinate within the texture ( 0 <= u,v <= 1.0 ),
 *  and a "size" of the pixel being rendered (du, dv), fill in the
 *  color of the "brightest" point (if any) within that region.
 */
HIDDEN int
points_render( ap, partp, swp, dp )
struct application *ap;
struct partition *partp;
struct shadework	*swp;
char	*dp;
{
	register struct points_specific *ptp =
		(struct points_specific *)dp;
	register spm_map_t	*mapp;
	fastf_t	umin, umax, vmin, vmax;
	int	xmin, xmax, ymin, ymax;
	register int	x, y;
	register struct points	*pp;
	fastf_t	mag;

swp->sw_uv.uv_du = ap->a_diverge;
swp->sw_uv.uv_dv = ap->a_diverge;
	/*rt_log( "du,dv = %g %g\n", swp->sw_uv.uv_du, swp->sw_uv.uv_dv);*/

	/* compute and clip bounds in u,v space */
	umin = swp->sw_uv.uv_u - swp->sw_uv.uv_du;
	umax = swp->sw_uv.uv_u + swp->sw_uv.uv_du;
	vmin = swp->sw_uv.uv_v - swp->sw_uv.uv_dv;
	vmax = swp->sw_uv.uv_v + swp->sw_uv.uv_dv;
	if( umin < 0 )  umin = 0;
	if( vmin < 0 )  vmin = 0;
	if( umax > 1 )  umax = 1;
	if( vmax > 1 )  vmax = 1;

	mapp = ptp->pt_map;

	mag = 0;
	ymin = vmin * mapp->ny;
	ymax = vmax * mapp->ny;
	/* for each latitude band */
	for( y = ymin; y < ymax; y++ ) {
		xmin = umin * mapp->nx[y];
		xmax = umax * mapp->nx[y];
		/* for each bin spanned in that band */
		for( x = xmin; x < xmax; x++ ) {
			pp = (struct points *)&(mapp->xbin[y][x*mapp->elsize]);
			while( pp != NULL ) {
				if(  pp->u < umax && pp->u >= umin
				  && pp->v < vmax && pp->v >= vmin
				  && pp->color[0] > mag ) {
					mag = pp->color[0];
				}
				pp = pp->next;
			}
		}
	}

	/*rt_log( "points_render ([%g %g][%g %g]) = %g\n",
		umin, umax, vmin, vmax, mag );*/

	if( mag == 0 ) {
		VSET( swp->sw_color, 0, 0, 0 );
	} else {
		VSET( swp->sw_color, mag/255.0, mag/255.0, mag/255.0 );
	}

	return(1);
}

/*
 *			P O I N T S _ P R I N T
 */
HIDDEN void
points_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print("points_setup", points_parse, (char *)dp);
	/* Should be more here */
}

HIDDEN void
points_mfree( cp )
char *cp;
{
	/* XXX - free linked lists in every bin! */
	spm_free( (spm_map_t *)cp );
}
