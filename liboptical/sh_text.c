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

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "material.h"

struct txt_specific {
	char	tx_file[128];	/* Filename */
	int	tx_w;		/* Width of texture in pixels */
	int	tx_fw;		/* File width of texture in pixels */
	int	tx_l;		/* Length of pixels in lines */
	char	*tx_pixels;	/* Pixel holding area */
	char	tx_transp[4];	/* RGB for transparency */
};
#define TX_NULL	((struct txt_specific *)0)

struct matparse txt_parse[] = {
	"file",		(int)(TX_NULL->tx_file),	"%s",
	"w",		(int)&(TX_NULL->tx_w),		"%d",
	"l",		(int)&(TX_NULL->tx_l),		"%d",
	"fw",		(int)&(TX_NULL->tx_fw),		"%d",
#ifndef cray
	"transp",	(int)(TX_NULL->tx_transp),	"%C",
#endif
	(char *)0,	0,				(char *)0
};

/*
 *			T X T _ R E A D
 *
 *  Load the texture into memory.
 *  Returns 0 on failure, 1 on success.
 */
HIDDEN int
txt_read( tp )
register struct txt_specific *tp;
{
	char *linebuf;
	register FILE *fp;
	register int i;

	if( (fp = fopen(tp->tx_file, "r")) == NULL )  {
		rt_log("txt_render(%s):  can't open\n", tp->tx_file);
		tp->tx_file[0] = '\0';
		return(0);
	}
	linebuf = rt_malloc(tp->tx_fw*3,"texture file line");
	tp->tx_pixels = rt_malloc(
		tp->tx_w * tp->tx_l * 3,
		tp->tx_file );
	for( i=0; i<tp->tx_l; i++ )  {
		if( fread(linebuf,1,tp->tx_fw*3,fp) != tp->tx_fw*3 ) {
			rt_log("txt_read: read error on %s\n", tp->tx_file);
			tp->tx_file[0] = '\0';
			(void)fclose(fp);
			rt_free(linebuf,"file line, error");
			return(0);
		}
		bcopy( linebuf, tp->tx_pixels + i*tp->tx_w*3, tp->tx_w*3 );
	}
	(void)fclose(fp);
	rt_free(linebuf,"texture file line");
	return(1);	/* OK */
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
HIDDEN
txt_render( ap, pp )
struct application *ap;
struct partition *pp;
{
	struct txt_specific *tp =
		(struct txt_specific *)pp->pt_regionp->reg_udata;
	auto struct uvcoord uv;
	fastf_t xmin, xmax, ymin, ymax;
	int line;
	int dx, dy;
	int x,y;
	register long r,g,b;
	fastf_t f;

	rt_functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
		ap, pp->pt_inseg->seg_stp, pp->pt_inhit, &uv );

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */
	if( tp->tx_file[0] == '\0'  ||
	    ( tp->tx_pixels == (char *)0 && txt_read(tp) == 0 ) )  {
		VSET( ap->a_color, uv.uv_u*255, 0, uv.uv_v*255 );
		return(1);
	}
	/* u is left->right index, v is line number bottom->top */
	if( uv.uv_u < 0 || uv.uv_u > 1 || uv.uv_v < 0 || uv.uv_v > 1 )  {
		rt_log("txt_render:  bad u,v=%g,%g du,dv=%g,%g seg=%s\n",
			uv.uv_u, uv.uv_v,
			uv.uv_du, uv.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		VSET( ap->a_color, 0, 1, 0 );
		return(1);
	}
	/* Don't filter more than 1/8 of the texture for 1 pixel! */
	if( uv.uv_du > 0.125 )  uv.uv_du = 0.125;
	if( uv.uv_dv > 0.125 )  uv.uv_dv = 0.125;

	if( uv.uv_du < 0 || uv.uv_dv < 0 )  {
		rt_log("txt_render uv=%g,%g, du dv=%g %g seg=%s\n",
			uv.uv_u, uv.uv_v, uv.uv_du, uv.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		uv.uv_du = uv.uv_dv = 0;
	}
	xmin = uv.uv_u - uv.uv_du;
	xmax = uv.uv_u + uv.uv_du;
	ymin = uv.uv_v - uv.uv_dv;
	ymax = uv.uv_v + uv.uv_dv;
	if( xmin < 0 )  xmin = 0;
	if( ymin < 0 )  ymin = 0;
	if( xmax > 1 )  xmax = 1;
	if( ymax > 1 )  ymax = 1;
	x = xmin * (tp->tx_w-1);
	y = ymin * (tp->tx_l-1);
	dx = (xmax - xmin) * (tp->tx_w-1);
	dy = (ymax - ymin) * (tp->tx_l-1);
	if( dx < 1 )  dx = 1;
	if( dy < 1 )  dy = 1;
/** rt_log("x=%d y=%d, dx=%d, dy=%d\n", x, y, dx, dy); **/
	r = g = b = 0;
	for( line=0; line<dy; line++ )  {
		register unsigned char *cp;
		register unsigned char *ep;
		cp = (unsigned char *)(tp->tx_pixels +
		     (y+line) * tp->tx_w * 3  +  x * 3);
		ep = cp + 3*dx;
		while( cp < ep )  {
			r += *cp++;
			g += *cp++;
			b += *cp++;
		}
	}
	r /= (dx*dy);
	g /= (dx*dy);
	b /= (dx*dy);
	/*
	 * Transparency mapping is enabled, and we hit a transparent spot.
	 * Fire another ray to determine the actual color
	 */
	if( tp->tx_transp[3] == 0 ||
	    r != (tp->tx_transp[0]&0xFF) ||
	    g != (tp->tx_transp[1]&0xFF) ||
	    b != (tp->tx_transp[2]&0xFF) )  {
		f = 1.0 / 255.0;
		VSET( ap->a_color, r * f, g * f, b * f );
		return(1);
	}
	if( pp->pt_outhit->hit_dist >= INFINITY )  {
		rt_log("txt_render:  transparency on infinite object?\n");
		VSET( ap->a_color, 0, 1, 0 );
		return(1);
	}
	if( (ap->a_level%100) > 5 )  {
		VSET( ap->a_color, .1, .1, .1);
		return(1);
	}
	{
		auto struct application sub_ap;
		sub_ap = *ap;		/* struct copy */
		sub_ap.a_level = ap->a_level+1;
		VJOIN1( sub_ap.a_ray.r_pt, ap->a_ray.r_pt,
			pp->pt_outhit->hit_dist, ap->a_ray.r_dir );
		(void)rt_shootray( &sub_ap );
		VMOVE( ap->a_color, sub_ap.a_color );
	}
	return(1);
}

/*
 *			T X T _ S E T U P
 */
int
txt_setup( rp )
register struct region *rp;
{
	register struct txt_specific *tp;

	GETSTRUCT( tp, txt_specific );
	rp->reg_ufunc = txt_render;
	rp->reg_udata = (char *)tp;

	tp->tx_file[0] = '\0';
	tp->tx_w = tp->tx_fw = tp->tx_l = -1;
	mlib_parse( rp->reg_mater.ma_matparm, txt_parse, (char *)tp );
	if( tp->tx_w < 0 )  tp->tx_w = 512;
	if( tp->tx_l < 0 )  tp->tx_l = tp->tx_w;
	if( tp->tx_fw < 0 )  tp->tx_fw = tp->tx_w;
	tp->tx_pixels = (char *)0;
mlib_print("txt_setup", txt_parse, (char *)tp);
	return( txt_read(tp) );
}

struct ckr_specific  {
	int	ckr_r[2];
	int	ckr_g[2];
	int	ckr_b[2];
};
#define CKR_NULL ((struct ckr_specific *)0)

struct matparse ckr_parse[] = {
	"r",		(int)&(CKR_NULL->ckr_r[0]),	"%d",
	"g",		(int)&(CKR_NULL->ckr_g[0]),	"%d",
	"b",		(int)&(CKR_NULL->ckr_b[0]),	"%d",
	"R",		(int)&(CKR_NULL->ckr_r[1]),	"%d",
	"G",		(int)&(CKR_NULL->ckr_g[1]),	"%d",
	"B",		(int)&(CKR_NULL->ckr_b[1]),	"%d"
};

/*
 *			C K R _ R E N D E R
 */
HIDDEN
ckr_render( ap, pp )
register struct application *ap;
register struct partition *pp;
{
	register struct ckr_specific *ckp =
		(struct ckr_specific *)pp->pt_regionp->reg_udata;
	auto struct uvcoord uv;

	rt_functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
		ap, pp->pt_inseg->seg_stp, pp->pt_inhit, &uv );

	if( (uv.uv_u < 0.5 && uv.uv_v < 0.5) ||
	    (uv.uv_u >=0.5 && uv.uv_v >=0.5) )  {
		VSET( ap->a_color, ckp->ckr_r[0], ckp->ckr_g[0], ckp->ckr_b[0] );
	} else {
		VSET( ap->a_color, ckp->ckr_r[1], ckp->ckr_g[1], ckp->ckr_b[1] );
	}
}

/*
 *			C K R _ S E T U P
 */
int
ckr_setup( rp )
register struct region *rp;
{
	register struct ckr_specific *ckp;

	GETSTRUCT( ckp, ckr_specific );
	bzero( (char *)ckp, sizeof(struct ckr_specific) );
	rp->reg_ufunc = ckr_render;
	rp->reg_udata = (char *)ckp;
	mlib_parse( rp->reg_mater.ma_matparm, ckr_parse, (char *)ckp );
	return(1);
}

/*
 *			T E S T M A P _ R E N D E R
 *
 *  Render a map which varries red with U and blue with V values.
 *  Mostly useful for debugging ft_uv() routines.
 */
HIDDEN
testmap_render( ap, pp )
register struct application *ap;
register struct partition *pp;
{
	auto struct uvcoord uv;

	rt_functab[pp->pt_inseg->seg_stp->st_id].ft_uv(
		ap, pp->pt_inseg->seg_stp, pp->pt_inhit, &uv );
	VSET( ap->a_color, uv.uv_u, 0, uv.uv_v );
	return(1);
}

/*
 *			T M A P _ S E T U P
 */
tmap_setup( rp )
register struct region *rp;
{
	rp->reg_ufunc = testmap_render;
	rp->reg_udata = (char *)0;
	return(1);
}
