/*
 *			S T X T . C
 *
 *  Routines to implement solid (ie, 3-D) texture maps.
 *
 *  Author -
 *	Tom DiGiacinto
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

struct	stxt_specific  {
	unsigned char stx_transp[8];	/* RGB for transparency */
	char	stx_file[128];	/* Filename */
	int	stx_w;		/* Width of texture in pixels */
	int	stx_fw;		/* File width of texture in pixels */
	int	stx_n;		/* Number of scanlines */
	int	stx_d;		/* Depth of texture (Num pix files)*/
	int	stx_norm;
	vect_t	stx_min;
	vect_t	stx_max;
	char	*stx_pixels;	/* Pixel holding area */
};
#define SOL_NULL ((struct stxt_specific *)0)

struct	matparse stxt_parse[] = {
#ifndef cray
	"transp",	(mp_off_ty)(SOL_NULL->stx_transp),"%C",
	"file",		(mp_off_ty)(SOL_NULL->stx_file),	"%s",
#else
	"transp",	(mp_off_ty)0,			"%C",
	"file",		(mp_off_ty)1,			"%s",
#endif
	"w",		(mp_off_ty)&(SOL_NULL->stx_w),	"%d",
	"n",		(mp_off_ty)&(SOL_NULL->stx_n),	"%d",
	"d",		(mp_off_ty)&(SOL_NULL->stx_d),	"%d",
	"fw",		(mp_off_ty)&(SOL_NULL->stx_fw),	"%d",
	(char *)0,	(mp_off_ty)0,		(char *)0
};

HIDDEN int  stxt_setup(), brick_render(), mbound_render(), rbound_render();

struct	mfuncs stxt_mfuncs[] = {
	"brick",	0,		0,		MFI_HIT,
	stxt_setup,	brick_render,	0,		0,

	"mbound",	0,		0,		MFI_HIT,
	stxt_setup,	mbound_render,	0,		0,

	"rbound",	0,		0,		MFI_HIT,
	stxt_setup,	rbound_render,	0,		0,

	(char *)0,	0,		0,		0,
	0,		0,		0,		0
};

/*
 *			S T X T _ R E A D
 *
 *  Load the texture into memory.
 *  Returns 0 on failure, 1 on success.
 */
HIDDEN int
stxt_read( stp )
register struct stxt_specific *stp;
{
	char *linebuf;
	register FILE *fp;
	register int i;
	char name[256];
	int frame, ln;
	int rd, rdd;

	/*** MEMORY HOG ***/
	stp->stx_pixels = rt_malloc(
		stp->stx_w * stp->stx_n * stp->stx_d * 3,
		stp->stx_file );

	ln = 0;
	rdd = 0;
	rd = 0;

	/**  LOOP: through list of basename.n files **/
	for( frame=0; frame <= stp->stx_d-1; frame++ )  {

		sprintf(name, "%s.%d", stp->stx_file, frame);

		if( (fp = fopen(name, "r")) == NULL )  {
			rt_log("stxt_read(%s):  can't open\n", name);
			stp->stx_file[0] = '\0';
			return(0);
		}
		linebuf = rt_malloc(stp->stx_fw*3,"texture file line");

		for( i = 0; i < stp->stx_n; i++ )  {
			if( (rd = fread(linebuf,1,stp->stx_fw*3,fp)) != stp->stx_fw*3 ) {
				rt_log("stxt_read: read error on %s\n", name);
				stp->stx_file[0] = '\0';
				(void)fclose(fp);
				rt_free(linebuf,"file line, error");
				return(0);
			}
			bcopy( linebuf, stp->stx_pixels + ln*stp->stx_w*3, stp->stx_w*3 );
			ln++;
			rdd += rd;
		}
		(void)fclose(fp);
		rt_free(linebuf,"texture file line");
	}
	return(1);	/* OK */
}


/*
 *			S T X T _ S E T U P
 */
HIDDEN int
stxt_setup( rp, matparm, dpp )
register struct region *rp;
char	*matparm;
char	**dpp;
{
	register struct stxt_specific *stp;

	GETSTRUCT( stp, stxt_specific );
	*dpp = (char *)stp;

	/**  Set up defaults  **/
	stp->stx_file[0] = '\0';
	stp->stx_w = stp->stx_fw = stp->stx_n = stp->stx_d = -1;
	stp->stx_norm = 0;
	VSETALL(stp->stx_min,  INFINITY);
	VSETALL(stp->stx_max, -INFINITY);
	rt_rpp_tree(rp->reg_treetop,stp->stx_min,stp->stx_max);

	/**	Get input values  **/
	mlib_parse( matparm, stxt_parse, (mp_off_ty)stp );
	/*** DEFAULT SIZE OF STXT FILES ***/
	if( stp->stx_w < 0 )  stp->stx_w = 512;
	if( stp->stx_n < 0 )  stp->stx_n = stp->stx_w;

	/**  Defaults to an orthogonal projection??  **/
	if( stp->stx_d < 0 )  stp->stx_d = 1;

	if( stp->stx_fw < 0 )  stp->stx_fw = stp->stx_w;
	stp->stx_pixels = (char *)0;
	if( stp->stx_transp[3] != 0 )
		rp->reg_transmit = 1;

	/**	Read in texture file/s  **/
	return( stxt_read(stp) );
}



HIDDEN
brick_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct stxt_specific *stp =
		(struct stxt_specific *)dp;
	register struct soltab *sp;

	vect_t  lx, ly, lz;	/* local coordinate axis */
	fastf_t f;
	int iptr;

	fastf_t sx, sy, sz;
	fastf_t tx, ty, tz;
	int line;
	int dx, dy;
	int x,y;
	register long r,g,b;

	int u1, u2, u3;
	register unsigned char *cp;

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */
	if( stp->stx_file[0] == '\0'  ||
	    ( stp->stx_pixels == (char *)0 && stxt_read(stp) == 0 ) )  {
		VSET( swp->sw_color, 1, 0, 1 );
		return(1);
	}

	/** Local Coordinate Axis **/
	VSET( lx, 1, 0, 0 );
	VSET( ly, 0, 1, 0 );
	VSET( lz, 0, 0, 1 );

	f = VDOT( swp->sw_hit.hit_point, lx ) / (float)stp->stx_w;
	if( f < 0 ) f = -f;
	f = modf( f, &iptr );
	sx=f;
/********************************
*	if( f < 0.5 )
*		sx = 2 * f;
*	else
*		sx = 2 * ( 1 - f );
*********************************/

	f = VDOT( swp->sw_hit.hit_point, ly ) / (float)stp->stx_n;
	if( f < 0 ) f = -f;
	f = modf( f, &iptr );
	sy=f;
/*********************************
*	if( f < 0.5 )
*		sy = 2 * f;
*	else
*		sy = 2 * ( 1 - f );
**********************************/

	f = VDOT( swp->sw_hit.hit_point, lz ) / (float)stp->stx_d;
	if( f < 0 ) f = -f;
	f = modf( f, &iptr );
	sz=f;
/*********************************
*	if( f < 0.5 )
*		sz = 2 * f;
*	else
*		sz = 2 * ( 1 - f );
**********************************/

/*rt_log("sx = %f\tsy = %f\tsz = %f\n",sx,sy,sz);*/

	/* Index into TEXTURE SPACE */
	tx = sx * stp->stx_w;
	ty = sy * stp->stx_n;
	tz = sz * stp->stx_d;

	u1 = (int)tz * stp->stx_n * stp->stx_w *  3.0;
	u2 = (int)ty * stp->stx_w * 3.0;
	u3 = (int)tx * 3.0;

	cp = (unsigned char *)(stp->stx_pixels + u1 + u2 + u3 );

	r = *cp++;
	g = *cp++;
	b = *cp++;

	VSET( swp->sw_color,
		(r+0.5) * rt_inv255,
		(g+0.5) * rt_inv255,
		(b+0.5) * rt_inv255 );

	return(1);
}


HIDDEN
rbound_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct stxt_specific *stp =
		(struct stxt_specific *)dp;
	register struct soltab *sp;
	fastf_t xmin, xmax, ymin, ymax;
	fastf_t sx, sy, sz;
	fastf_t tx, ty, tz;
	int line;
	int dx, dy;
	int x,y;
	register long r,g,b;

	int u1, u2, u3;
	register unsigned char *cp;

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */
	if( stp->stx_file[0] == '\0'  ||
	    ( stp->stx_pixels == (char *)0 && stxt_read(stp) == 0 ) )  {
		VSET( swp->sw_color, 1, 0, 1 );
		return(1);
	}

	/* NORMALIZE x,y,z to [0..1) */
	sx = (swp->sw_hit.hit_point[0] - stp->stx_min[0]) / (stp->stx_max[0] - stp->stx_min[0] + 1.0);
	sy = (swp->sw_hit.hit_point[1] - stp->stx_min[1]) / (stp->stx_max[1] - stp->stx_min[1] + 1.0);
	sz = (swp->sw_hit.hit_point[2] - stp->stx_min[2]) / (stp->stx_max[2] - stp->stx_min[2] + 1.0);

	/* Index into TEXTURE SPACE */
	tx = sx * stp->stx_w;
	ty = sy * stp->stx_n;
	tz = sz * stp->stx_d;

	u1 = (int)tz * stp->stx_n * stp->stx_w *  3.0;
	u2 = (int)ty * stp->stx_w * 3.0;
	u3 = (int)tx * 3.0;

	cp = (unsigned char *)(stp->stx_pixels + u1 + u2 + u3 );

	r = *cp++;
	g = *cp++;
	b = *cp++;

	VSET( swp->sw_color,
		(r+0.5) * rt_inv255,
		(g+0.5) * rt_inv255,
		(b+0.5) * rt_inv255 );

	return(1);
}


HIDDEN
mbound_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct stxt_specific *stp =
		(struct stxt_specific *)dp;
	register struct soltab *sp;
	fastf_t xmin, xmax, ymin, ymax;
	fastf_t sx, sy, sz;
	fastf_t tx, ty, tz;
	int line;
	int dx, dy;
	int x,y;
	register long r,g,b;

	int u1, u2, u3;
	register unsigned char *cp;

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */
	if( stp->stx_file[0] == '\0'  ||
	    ( stp->stx_pixels == (char *)0 && stxt_read(stp) == 0 ) )  {
		VSET( swp->sw_color, 1, 0, 1 );
		return(1);
	}

   /**  Using Model-RPP as Texture Bounds **/
	if( stp->stx_norm <= 0 )  {
		VMOVE(stp->stx_min,ap->a_rt_i->mdl_min);
		VMOVE(stp->stx_max,ap->a_rt_i->mdl_max);
		stp->stx_norm++;
	}

	/* NORMALIZE x,y,z to [0..1) */
	sx = (swp->sw_hit.hit_point[0] - stp->stx_min[0]) / (stp->stx_max[0] - stp->stx_min[0] + 1.0);
	sy = (swp->sw_hit.hit_point[1] - stp->stx_min[1]) / (stp->stx_max[1] - stp->stx_min[1] + 1.0);
	sz = (swp->sw_hit.hit_point[2] - stp->stx_min[2]) / (stp->stx_max[2] - stp->stx_min[2] + 1.0);

	/* Index into TEXTURE SPACE */
	tx = sx * stp->stx_w;
	ty = sy * stp->stx_n;
	tz = sz * stp->stx_d;

	u1 = (int)tz * stp->stx_n * stp->stx_w *  3.0;
	u2 = (int)ty * stp->stx_w * 3.0;
	u3 = (int)tx * 3.0;

	cp = (unsigned char *)(stp->stx_pixels + u1 + u2 + u3 );

	r = *cp++;
	g = *cp++;
	b = *cp++;

	VSET( swp->sw_color,
		(r+0.5) * rt_inv255,
		(g+0.5) * rt_inv255,
		(b+0.5) * rt_inv255 );

	return(1);
}
