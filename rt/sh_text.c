/*
 *  			T E X T . C
 *  
 *  Texture map lookup
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
static char RCStext[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

struct region	env_region;			/* share with view.c */

HIDDEN int	bwtxt_render();
HIDDEN int	txt_setup(), txt_render();
HIDDEN int	ckr_setup(), ckr_render();
HIDDEN int	bmp_setup(), bmp_render();
HIDDEN void	bwtxtprint(), bwtxtfree();
HIDDEN void	txt_print(), txt_free();
HIDDEN void	ckr_print(), ckr_free();
HIDDEN void	bmp_print(), bmp_free();
HIDDEN void	txt_transp_hook();
HIDDEN int tstm_render();
HIDDEN int star_render();
HIDDEN int envmap_setup();
extern int mlib_zero(), mlib_one();
extern void	mlib_void();

struct mfuncs txt_mfuncs[] = {
	"texture",	0,		0,		MFI_UV,		0,
	txt_setup,	txt_render,	txt_print,	txt_free,

	"bwtexture",	0,		0,		MFI_UV,		0,
	txt_setup,	bwtxt_render,	txt_print,	txt_free,

	"checker",	0,		0,		MFI_UV,		0,
	ckr_setup,	ckr_render,	ckr_print,	ckr_free,

	"testmap",	0,		0,		MFI_UV,		0,
	mlib_one,	tstm_render,	mlib_void,	mlib_void,

	"fakestar",	0,		0,		0,		0,
	mlib_one,	star_render,	mlib_void,	mlib_void,

	"bump",		0,		0,		MFI_UV|MFI_NORMAL, 0,
	txt_setup,	bmp_render,	txt_print,	txt_free,

	"envmap",	0,		0,		0,		0,
	envmap_setup,	mlib_zero,	mlib_void,	mlib_void,

	(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0
};

#define TXT_NAME_LEN 128
struct txt_specific {
	int	tx_transp[3];	/* RGB for transparency */
	char	tx_file[TXT_NAME_LEN];	/* Filename */
	int	tx_w;		/* Width of texture in pixels */
	int	tx_n;		/* Number of scanlines */
	int	tx_trans_valid;	/* boolean: is tx_transp valid ? */
	struct rt_mapped_file	*mp;
};
#define TX_NULL	((struct txt_specific *)0)
#define TX_O(m)	offsetof(struct txt_specific, m)

struct bu_structparse txt_parse[] = {
	{"%d",	1, "transp",	offsetofarray(struct txt_specific, tx_transp),	txt_transp_hook },
	{"%s",	TXT_NAME_LEN, "file", offsetofarray(struct txt_specific, tx_file),		FUNC_NULL },
	{"%d",	1, "w",		TX_O(tx_w),		FUNC_NULL },
	{"%d",	1, "n",		TX_O(tx_n),		FUNC_NULL },
	{"%d",	1, "l",		TX_O(tx_n),		FUNC_NULL }, /*compat*/
	{"%d",	1, "trans_valid",	TX_O(tx_trans_valid),	FUNC_NULL },
	{"",	0, (char *)0,	0,			FUNC_NULL }
};

/*
 *			T X T _ T R A N S P _ H O O K
 *
 *  Hooked function, called by bu_structparse
 */
HIDDEN void
txt_transp_hook( ptab, name, cp, value )
struct bu_structparse *ptab;
char	*name;
char	*cp;
char	*value;
{
	register struct txt_specific *tp =
		(struct txt_specific *)cp;

	if (!strcmp(name, txt_parse[0].sp_name) && ptab == txt_parse) {
		tp->tx_trans_valid = 1;
	} else {
		rt_log("file:%s, line:%d txt_transp_hook name:(%s) instead of (%s)\n",
			__FILE__, __LINE__, name, txt_parse[0].sp_name);
	}
}

/*
 *  			T X T _ R E N D E R
 *  
 *  Given a u,v coordinate within the texture ( 0 <= u,v <= 1.0 ),
 *  return a pointer to the relevant pixel.
 *
 *  Note that .pix files are stored left-to-right, bottom-to-top,
 *  which works out very naturally for the indexing scheme.
 */
HIDDEN int
txt_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct txt_specific *tp =
		(struct txt_specific *)dp;
	fastf_t xmin, xmax, ymin, ymax;
	int line;
	int dx, dy;
	int x,y;
	register fastf_t r,g,b;

	if( rdebug & RDEBUG_SHADE )
		bu_log( "in txt_render(): du=%g, dv=%g\n", swp->sw_uv.uv_du, swp->sw_uv.uv_dv );
	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */
	if( tp->tx_file[0] == '\0' || !tp->mp )  {
		VSET( swp->sw_color, swp->sw_uv.uv_u, 0, swp->sw_uv.uv_v );
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}

	/* u is left->right index, v is line number bottom->top */
	/* Don't filter more than 1/8 of the texture for 1 pixel! */
	if( swp->sw_uv.uv_du > 0.125 )  swp->sw_uv.uv_du = 0.125;
	if( swp->sw_uv.uv_dv > 0.125 )  swp->sw_uv.uv_dv = 0.125;

	if( swp->sw_uv.uv_du < 0 || swp->sw_uv.uv_dv < 0 )  {
		rt_log("txt_render uv=%g,%g, du dv=%g %g seg=%s\n",
			swp->sw_uv.uv_u, swp->sw_uv.uv_v, swp->sw_uv.uv_du, swp->sw_uv.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		swp->sw_uv.uv_du = swp->sw_uv.uv_dv = 0;
	}

	xmin = swp->sw_uv.uv_u - swp->sw_uv.uv_du;
	xmax = swp->sw_uv.uv_u + swp->sw_uv.uv_du;
	ymin = swp->sw_uv.uv_v - swp->sw_uv.uv_dv;
	ymax = swp->sw_uv.uv_v + swp->sw_uv.uv_dv;
	if( xmin < 0 )  xmin = 0;
	if( ymin < 0 )  ymin = 0;
	if( xmax > 1 )  xmax = 1;
	if( ymax > 1 )  ymax = 1;

	if( rdebug & RDEBUG_SHADE )
		bu_log( "footprint in texture space is (%g %g) <-> (%g %g)\n",
			xmin * (tp->tx_w-1), ymin * (tp->tx_n-1),
			xmax * (tp->tx_w-1), ymax * (tp->tx_n-1) );
			
#if 1
	dx = (int)(xmax * (tp->tx_w-1)) - (int)(xmin * (tp->tx_w-1));
	dy = (int)(ymax * (tp->tx_n-1)) - (int)(ymin * (tp->tx_n-1));

	if( rdebug & RDEBUG_SHADE )
		bu_log( "\tdx = %d, dy = %d\n", dx, dy );
	if( dx == 0 && dy == 0 )
	{
		/* No averaging necessary */

		register unsigned char *cp;

		cp = ((unsigned char *)(tp->mp->buf)) +
			(int)(ymin * (tp->tx_n-1)) * tp->tx_w * 3 +
			(int)(xmin * (tp->tx_w-1)) * 3;
		r = *cp++;
		g = *cp++;
		b = *cp;
	}
	else
	{
		/* Calculate weighted average of cells in footprint */

		fastf_t tot_area=0.0;
		fastf_t cell_area;
		int start_line, stop_line, line;
		int start_col, stop_col, col;
		fastf_t xstart, xstop, ystart, ystop;
		fastf_t u, v;

		xstart = xmin * (tp->tx_w-1);
		xstop = xmax * (tp->tx_w-1);
		ystart = ymin * (tp->tx_n-1);
		ystop = ymax * (tp->tx_n-1);

		start_line = ystart;
		stop_line = ystop;
		start_col = xstart;
		stop_col = xstop;

		r = g = b = 0.0;

		if( rdebug & RDEBUG_SHADE )
		{
			bu_log( "\thit in texture space = (%g %g)\n", swp->sw_uv.uv_u * (tp->tx_w-1), swp->sw_uv.uv_v * (tp->tx_n-1) );
			bu_log( "\t averaging from  (%g %g) to (%g %g)\n", xstart, ystart, xstop, ystop );
			bu_log( "\tcontributions to average:\n" );
		}

		for( line = start_line ; line <= stop_line ; line++ )
		{
			register unsigned char *cp;
			register unsigned char *ep;
			fastf_t line_factor;
			fastf_t line_upper, line_lower;

			line_upper = line + 1.0;
			if( line_upper > ystop )
				line_upper = ystop;
			line_lower = line;
			if( line_lower < ystart )
				line_lower = ystart;
			line_factor = line_upper - line_lower;
			cp = ((unsigned char *)(tp->mp->buf)) +
				line * tp->tx_w * 3 + (int)(xstart) * 3;

			for( col = start_col ; col <= stop_col ; col++ )
			{
				fastf_t col_upper, col_lower;

				col_upper = col + 1.0;
				if( col_upper > xstop )
					col_upper = xstop;
				col_lower = col;
				if( col_lower < xstart )
					col_lower = xstart;
				cell_area = line_factor * (col_upper - col_lower);
				tot_area += cell_area;

				if( rdebug & RDEBUG_SHADE )
					bu_log( "\t %d %d %d weight=%g (from col=%d line=%d)\n", *cp, *(cp+1), *(cp+2), cell_area, col, line );

				r += (*cp++) * cell_area;
				g += (*cp++) * cell_area;
				b += (*cp++) * cell_area;
			}
		}
		r /= tot_area;
		g /= tot_area;
		b /= tot_area;
	}
	
	if( rdebug & RDEBUG_SHADE )
		bu_log( " average: %g %g %g\n", r, g, b );
#else
	x = xmin * (tp->tx_w-1);
	y = ymin * (tp->tx_n-1);
	dx = (xmax - xmin) * (tp->tx_w-1);
	dy = (ymax - ymin) * (tp->tx_n-1);
	if( dx < 1 )  dx = 1;
	if( dy < 1 )  dy = 1;

	if( rdebug & RDEBUG_SHADE )
		bu_log(" in txt_render(): x=%d y=%d, dx=%d, dy=%d\n", x, y, dx, dy);

	r = g = b = 0;
	for( line=0; line<dy; line++ )  {
		register unsigned char *cp;
		register unsigned char *ep;
		cp = ((unsigned char *)(tp->mp->buf)) +
		     (y+line) * tp->tx_w * 3  +  x * 3;
		ep = cp + 3*dx;
		while( cp < ep )  {
			if( rdebug & RDEBUG_SHADE )
				bu_log( "\tAdding %d %d %d\n", *cp, *(cp+1), *(cp+2) );
			r += *cp++;
			g += *cp++;
			b += *cp++;
		}
	}
	if( rdebug & RDEBUG_SHADE )
		bu_log( "Totals: %d %d %d,", r, g, b );
	r /= (dx*dy);
	g /= (dx*dy);
	b /= (dx*dy);
	if( rdebug & RDEBUG_SHADE )
		bu_log( " average: %d %d %d\n", r, g, b );
#endif

	if (!tp->tx_trans_valid) {
opaque:
		VSET( swp->sw_color,
			r * rt_inv255,
			g * rt_inv255,
			b * rt_inv255 );
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}
	/* This circumlocution needed to keep expression simple for Cray,
	 * and others
	 */
	if( r != ((long)tp->tx_transp[0]) )  goto opaque;
	if( g != ((long)tp->tx_transp[1]) )  goto opaque;
	if( b != ((long)tp->tx_transp[2]) )  goto opaque;

	/*
	 *  Transparency mapping is enabled, and we hit a transparent spot.
	 *  Let higher level handle it in reflect/refract code.
	 */
	swp->sw_transmit = 1.0;
	swp->sw_reflect = 0.0;
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );
	return(1);
}

/*
 *  			B W T X T _ R E N D E R
 *  
 *  Given a u,v coordinate within the texture ( 0 <= u,v <= 1.0 ),
 *  return the filtered intensity.
 *
 *  Note that .bw files are stored left-to-right, bottom-to-top,
 *  which works out very naturally for the indexing scheme.
 */
HIDDEN int
bwtxt_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct txt_specific *tp =
		(struct txt_specific *)dp;
	fastf_t xmin, xmax, ymin, ymax;
	int line;
	int dx, dy;
	int x,y;
	register long bw;

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */
	if( tp->tx_file[0] == '\0' || !tp->mp )  {
		VSET( swp->sw_color, swp->sw_uv.uv_u, 0, swp->sw_uv.uv_v );
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}

	/* u is left->right index, v is line number bottom->top */
	/* Don't filter more than 1/8 of the texture for 1 pixel! */
	if( swp->sw_uv.uv_du > 0.125 )  swp->sw_uv.uv_du = 0.125;
	if( swp->sw_uv.uv_dv > 0.125 )  swp->sw_uv.uv_dv = 0.125;

	if( swp->sw_uv.uv_du < 0 || swp->sw_uv.uv_dv < 0 )  {
		rt_log("bwtxt_render uv=%g,%g, du dv=%g %g seg=%s\n",
			swp->sw_uv.uv_u, swp->sw_uv.uv_v, swp->sw_uv.uv_du, swp->sw_uv.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		swp->sw_uv.uv_du = swp->sw_uv.uv_dv = 0;
	}
	xmin = swp->sw_uv.uv_u - swp->sw_uv.uv_du;
	xmax = swp->sw_uv.uv_u + swp->sw_uv.uv_du;
	ymin = swp->sw_uv.uv_v - swp->sw_uv.uv_dv;
	ymax = swp->sw_uv.uv_v + swp->sw_uv.uv_dv;
	if( xmin < 0 )  xmin = 0;
	if( ymin < 0 )  ymin = 0;
	if( xmax > 1 )  xmax = 1;
	if( ymax > 1 )  ymax = 1;
	x = xmin * (tp->tx_w-1);
	y = ymin * (tp->tx_n-1);
	dx = (xmax - xmin) * (tp->tx_w-1);
	dy = (ymax - ymin) * (tp->tx_n-1);
	if( dx < 1 )  dx = 1;
	if( dy < 1 )  dy = 1;
	bw = 0;
	for( line=0; line<dy; line++ )  {
		register unsigned char *cp;
		register unsigned char *ep;
		cp = ((unsigned char *)(tp->mp->buf)) +
		     (y+line) * tp->tx_w  +  x;
		ep = cp + dx;
		while( cp < ep )  {
			bw += *cp++;
		}
	}

	if (!tp->tx_trans_valid) {
opaque:
		VSETALL( swp->sw_color,
			bw * rt_inv255 / (dx*dy) );
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}
	/* This circumlocution needed to keep expression simple for Cray,
	 * and others
	 */
	if( bw / (dx*dy) != ((long)tp->tx_transp[0]) )  goto opaque;

	/*
	 *  Transparency mapping is enabled, and we hit a transparent spot.
	 *  Let higher level handle it in reflect/refract code.
	 */
	swp->sw_transmit = 1.0;
	swp->sw_reflect = 0.0;
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );
	return(1);
}

/*
 *			T X T _ S E T U P
 */
HIDDEN int
txt_setup( rp, matparm, dpp, mfp )
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;
CONST struct mfuncs	*mfp;
{
	register struct txt_specific *tp;
	int		pixelbytes = 3;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( tp, txt_specific );
	*dpp = (char *)tp;

	tp->tx_file[0] = '\0';
	tp->tx_w = tp->tx_n = -1;
	tp->tx_trans_valid = 0;
	if( bu_struct_parse( matparm, txt_parse, (char *)tp ) < 0 )  {
		rt_free( (char *)tp, "txt_specific" );
		return(-1);
	}
	if( tp->tx_w < 0 )  tp->tx_w = 512;
	if( tp->tx_n < 0 )  tp->tx_n = tp->tx_w;

	if( tp->tx_trans_valid )
		rp->reg_transmit = 1;

	if( tp->tx_file[0] == '\0' )  return -1;	/* FAIL, no file */
	if( !(tp->mp = rt_open_mapped_file( tp->tx_file, NULL )) )
		return -1;				/* FAIL */

	/* Ensure file is large enough */
	if( strcmp( mfp->mf_name, "bwtexture" ) == 0 )
		pixelbytes = 1;
	if( tp->mp->buflen < tp->tx_w * tp->tx_n * pixelbytes )  {
		rt_log("\ntxt_setup() ERROR %s %s needs %d bytes, '%s' only has %d\n",
			rp->reg_name,
			mfp->mf_name,
			tp->tx_w * tp->tx_n * pixelbytes,
			tp->mp->name,
			tp->mp->buflen );
		return -1;				/* FAIL */
	}

	return 1;				/* OK */
}

/*
 *			T X T _ P R I N T
 */
HIDDEN void
txt_print( rp )
register struct region *rp;
{
	bu_struct_print(rp->reg_name, txt_parse, (char *)rp->reg_udata);
}

/*
 *			T X T _ F R E E
 */
HIDDEN void
txt_free( cp )
char *cp;
{
	struct txt_specific *tp =
		(struct txt_specific *)cp;

	if( tp->mp )  rt_close_mapped_file( tp->mp );
	rt_free( cp, "txt_specific" );
}

struct ckr_specific  {
	int	ckr_a[3];	/* first RGB */
	int	ckr_b[3];	/* second RGB */
};
#define CKR_NULL	((struct ckr_specific *)0)
#define CKR_O(m)	offsetof(struct ckr_specific, m)

struct bu_structparse ckr_parse[] = {
	{"%d",	3, "a",	offsetofarray(struct ckr_specific, ckr_a), FUNC_NULL },
	{"%d",	3, "b",	offsetofarray(struct ckr_specific, ckr_b), FUNC_NULL },
	{"",	0, (char *)0,	0,			FUNC_NULL }
};

/*
 *			C K R _ R E N D E R
 */
HIDDEN int
ckr_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
register struct shadework	*swp;
char	*dp;
{
	register struct ckr_specific *ckp =
		(struct ckr_specific *)dp;
	register int *cp;

	if( (swp->sw_uv.uv_u < 0.5 && swp->sw_uv.uv_v < 0.5) ||
	    (swp->sw_uv.uv_u >=0.5 && swp->sw_uv.uv_v >=0.5) )  {
		cp = ckp->ckr_a;
	} else {
		cp = ckp->ckr_b;
	}
	VSET( swp->sw_color,
		(unsigned char)cp[0] * rt_inv255,
		(unsigned char)cp[1] * rt_inv255,
		(unsigned char)cp[2] * rt_inv255 );

	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}

/*
 *			C K R _ S E T U P
 */
HIDDEN int
ckr_setup( rp, matparm, dpp )
register struct region	 *rp;
struct rt_vls		*matparm;
char			**dpp;
{
	register struct ckr_specific *ckp;

	/* Default will be white and black checkers */
	GETSTRUCT( ckp, ckr_specific );
	*dpp = (char *)ckp;
	ckp->ckr_a[0] = ckp->ckr_a[1] = ckp->ckr_a[2] = 255;
	if( bu_struct_parse( matparm, ckr_parse, (char *)ckp ) < 0 )  {
		rt_free( (char *)ckp, "ckr_specific" );
		return(-1);
	}
	ckp->ckr_a[0] &= 0x0ff;
	ckp->ckr_a[1] &= 0x0ff;
	ckp->ckr_a[2] &= 0x0ff;
	ckp->ckr_b[0] &= 0x0ff;
	ckp->ckr_b[1] &= 0x0ff;
	ckp->ckr_b[2] &= 0x0ff;
	return(1);
}

/*
 *			C K R _ P R I N T
 */
HIDDEN void
ckr_print( rp )
register struct region *rp;
{
	bu_struct_print(rp->reg_name, ckr_parse, rp->reg_udata);
}

/*
 *			C K R _ F R E E
 */
HIDDEN void
ckr_free( cp )
char *cp;
{
	rt_free( cp, "ckr_specific" );
}

/*
 *			T S T M _ R E N D E R
 *
 *  Render a map which varries red with U and blue with V values.
 *  Mostly useful for debugging ft_uv() routines.
 */
HIDDEN
tstm_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
register struct shadework	*swp;
char	*dp;
{
	VSET( swp->sw_color, swp->sw_uv.uv_u, 0, swp->sw_uv.uv_v );

	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}

static vect_t star_colors[] = {
	{ 0.825769, 0.415579, 0.125303 },	/* 3000 */
	{ 0.671567, 0.460987, 0.258868 },
	{ 0.587580, 0.480149, 0.376395 },
	{ 0.535104, 0.488881, 0.475879 },
	{ 0.497639, 0.493881, 0.556825 },
	{ 0.474349, 0.494836, 0.624460 },
	{ 0.456978, 0.495116, 0.678378 },
	{ 0.446728, 0.493157, 0.727269 },	/* 10000 */
	{ 0.446728, 0.493157, 0.727269 },	/* fake 11000 */
#if 0
	{ 0.446728, 0.493157, 0.727269 },	/* fake 12000 */
	{ 0.446728, 0.493157, 0.727269 },	/* fake 13000 */
	{ 0.446728, 0.493157, 0.727269 },	/* fake 14000 */
	{ 0.446728, 0.493157, 0.727269 },	/* fake 15000 */
	{ 0.393433 0.488079 0.940423 }		/* 20000 */
#endif
};

/*
 *			S T A R _ R E N D E R
 */
HIDDEN
star_render( ap, pp, swp, dp )
register struct application *ap;
register struct partition *pp;
struct shadework	*swp;
char	*dp;
{
	/* Probably want to diddle parameters based on what part of sky */
	if( rand0to1(ap->a_resource->re_randptr) >= 0.98 )  {
		register int i;
		FAST fastf_t f;
		i = (sizeof(star_colors)-1) / sizeof(star_colors[0]);

		/* "f" used for intermediate result to avoid an SGI compiler error */
		f = rand0to1(ap->a_resource->re_randptr);
		i = ((double)i) * f;

		f = rand0to1(ap->a_resource->re_randptr);
		VSCALE( swp->sw_color, star_colors[i], f );
	} else {
		VSETALL( swp->sw_color, 0 );
	}

	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}

/*
 *  			B M P _ R E N D E R
 *  
 *  Given a u,v coordinate within the texture ( 0 <= u,v <= 1.0 ),
 *  compute a new surface normal.
 *  For now we come up with a local coordinate system, and
 *  make bump perturbations from the red and blue channels of
 *  an RGB image.
 *
 *  Note that .pix files are stored left-to-right, bottom-to-top,
 *  which works out very naturally for the indexing scheme.
 */
HIDDEN
bmp_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;
char	*dp;
{
	register struct txt_specific *tp =
		(struct txt_specific *)dp;
	unsigned char *cp;
	fastf_t	pertU, pertV;
	vect_t	y;		/* world coordinate axis vectors */
	vect_t	u, v;		/* surface coord system vectors */
	int	i, j;		/* bump map pixel indicies */

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug color.
	 */
	if( tp->tx_file[0] == '\0' || !tp->mp )  {
		VSET( swp->sw_color, swp->sw_uv.uv_u, 0, swp->sw_uv.uv_v );
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}
	/* u is left->right index, v is line number bottom->top */
	if( swp->sw_uv.uv_u < 0 || swp->sw_uv.uv_u > 1 || swp->sw_uv.uv_v < 0 || swp->sw_uv.uv_v > 1 )  {
		rt_log("bmp_render:  bad u,v=%g,%g du,dv=%g,%g seg=%s\n",
			swp->sw_uv.uv_u, swp->sw_uv.uv_v,
			swp->sw_uv.uv_du, swp->sw_uv.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		VSET( swp->sw_color, 0, 1, 0 );
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}

	/* Find a local coordinate system */
	VSET( y, 0, 1, 0 );
	VCROSS( u, y, swp->sw_hit.hit_normal );
	VUNITIZE( u );
	VCROSS( v, swp->sw_hit.hit_normal, u );

	/* Find our RGB value */
	i = swp->sw_uv.uv_u * (tp->tx_w-1);
	j = swp->sw_uv.uv_v * (tp->tx_n-1);
	cp = ((unsigned char *)(tp->mp->buf)) +
	     (j) * tp->tx_w * 3  +  i * 3;
	pertU = ((fastf_t)(*cp) - 128.0) / 128.0;
	pertV = ((fastf_t)(*(cp+2)) - 128.0) / 128.0;

	if( rdebug&RDEBUG_LIGHT ) {
		VPRINT("normal", swp->sw_hit.hit_normal);
		VPRINT("u", u );
		VPRINT("v", v );
		rt_log("cu = %d, cv = %d\n", *cp, *(cp+2));
		rt_log("pertU = %g, pertV = %g\n", pertU, pertV);
	}
	VJOIN2( swp->sw_hit.hit_normal, swp->sw_hit.hit_normal, pertU, u, pertV, v );
	VUNITIZE( swp->sw_hit.hit_normal );
	if( rdebug&RDEBUG_LIGHT ) {
		VPRINT("after", swp->sw_hit.hit_normal);
	}

	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}

/*
 *			E N V M A P _ S E T U P
 */
HIDDEN int
envmap_setup( rp, matparm, dpp, mfp, rtip )
register struct region *rp;
struct rt_vls *matparm;
char	**dpp;
CONST struct mfuncs	*mfp;
struct rt_i	*rtip;
{
	register char	*cp;
	struct rt_vls	material;

	RT_VLS_CHECK( matparm );
	RT_CK_RTI(rtip);
	if( env_region.reg_mfuncs )  {
		rt_log("envmap_setup:  second environment map ignored\n");
		return(0);		/* drop region */
	}
	env_region = *rp;		/* struct copy */
	/* Get copies of, or eliminate, references to dynamic structures */
	env_region.reg_name = rt_strdup( rp->reg_name );
	env_region.reg_treetop = TREE_NULL;
	env_region.reg_forw = REGION_NULL;
	env_region.reg_mfuncs = (char *)0;

	rt_vls_init( &material );
	rt_vls_vlscat( &material, matparm );
	/* Expect "material SPACE args", find space */
	cp = RT_VLS_ADDR( &material );
	while( *cp != '\0' )  {
		if( !isascii(*cp) || isspace(*cp) )  break;
		cp++;
	}
	if( *cp == '\0' )  {
		/*  Null was encountered while searching for space,
		 *  leave "cp" pointing at null, e.g., no parameter string.
		 */
	} else {
		/* Replace space with null, advance "cp" to parameter */
		*cp++ = '\0';
	}

	env_region.reg_mater.ma_shader = bu_vls_strdup( &material );

	if( mlib_setup( &env_region, rtip ) < 0 )
		rt_log("envmap_setup() material '%s' failed\n", env_region.reg_mater );
	rt_vls_free( &material );
	return(0);		/* This region should be dropped */
}
