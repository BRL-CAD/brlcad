/*                       S H _ T E M P . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2006 United States Government as represented by
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
/** @file sh_temp.c
 *
 *  Temperature map lookup.
 *  Based upon liboptical/sh_text.c
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"

extern struct region	env_region;		/* import from view.c */

HIDDEN int	temp_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, const struct mfuncs *mfp, struct rt_i *rtip), temp_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	temp_print(register struct region *rp), temp_free(char *cp);
HIDDEN void	temp_transp_hook();

extern int mlib_zero(), mlib_one();
extern void	mlib_void();

struct mfuncs temp_mfuncs[] = {
	{MF_MAGIC,	"temp",		0,		MFI_UV,		0,
	temp_setup,	temp_render,	temp_print,	temp_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};

#define TXT_NAME_LEN 128
struct temp_specific {
	char	t_file[TXT_NAME_LEN];	/* Filename */
	int	t_w;		/* Width of texture in pixels */
	int	t_n;		/* Number of scanlines */
	struct bu_mapped_file	*mp;
};
#define TX_NULL	((struct temp_specific *)0)
#define TX_O(m)	bu_offsetof(struct temp_specific, m)

struct bu_structparse temp_parse[] = {
	{"%s",	TXT_NAME_LEN, "file", bu_offsetofarray(struct temp_specific, t_file),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "w",		TX_O(t_w),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n",		TX_O(t_n),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "l",		TX_O(t_n),		BU_STRUCTPARSE_FUNC_NULL }, /*compat*/
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

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
temp_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct temp_specific *tp =
		(struct temp_specific *)dp;
	fastf_t xmin, xmax, ymin, ymax;
	int dx, dy;
	register fastf_t temp = 0;

	if( rdebug & RDEBUG_SHADE )
		bu_log( "in temp_render(): du=%g, dv=%g\n", swp->sw_uv.uv_du, swp->sw_uv.uv_dv );
	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 * Set temp in degK to be the sum of the X&Y screen coordinates.
	 */
	if( tp->t_file[0] == '\0' || !tp->mp )  {
		swp->sw_temperature = ap->a_x + ap->a_y;
		if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}

	/* u is left->right index, v is line number bottom->top */
	/* Don't filter more than 1/8 of the texture for 1 pixel! */
	if( swp->sw_uv.uv_du > 0.125 )  swp->sw_uv.uv_du = 0.125;
	if( swp->sw_uv.uv_dv > 0.125 )  swp->sw_uv.uv_dv = 0.125;

	if( swp->sw_uv.uv_du < 0 || swp->sw_uv.uv_dv < 0 )  {
		bu_log("temp_render uv=%g,%g, du dv=%g %g seg=%s\n",
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
			xmin * (tp->t_w-1), ymin * (tp->t_n-1),
			xmax * (tp->t_w-1), ymax * (tp->t_n-1) );

	dx = (int)(xmax * (tp->t_w-1)) - (int)(xmin * (tp->t_w-1));
	dy = (int)(ymax * (tp->t_n-1)) - (int)(ymin * (tp->t_n-1));

	if( rdebug & RDEBUG_SHADE )
		bu_log( "\tdx = %d, dy = %d\n", dx, dy );
	if( dx == 0 && dy == 0 )
	{
		/* No averaging necessary */

		register unsigned char *cp;
		double ttemp;

		cp = ((unsigned char *)(tp->mp->buf)) +
			(int)(ymin * (tp->t_n-1)) * tp->t_w * 8 +
			(int)(xmin * (tp->t_w-1)) * 8;
		ntohd( (unsigned char *)&ttemp, cp, 1 );
		temp += ttemp;
	}
	else
	{
		/* Calculate weighted average of cells in footprint */

		fastf_t tot_area=0.0;
		fastf_t cell_area;
		int start_line, stop_line, line;
		int start_col, stop_col, col;
		fastf_t xstart, xstop, ystart, ystop;

		xstart = xmin * (tp->t_w-1);
		xstop = xmax * (tp->t_w-1);
		ystart = ymin * (tp->t_n-1);
		ystop = ymax * (tp->t_n-1);

		start_line = ystart;
		stop_line = ystop;
		start_col = xstart;
		stop_col = xstop;

		if( rdebug & RDEBUG_SHADE )
		{
			bu_log( "\thit in texture space = (%g %g)\n", swp->sw_uv.uv_u * (tp->t_w-1), swp->sw_uv.uv_v * (tp->t_n-1) );
			bu_log( "\t averaging from  (%g %g) to (%g %g)\n", xstart, ystart, xstop, ystop );
			bu_log( "\tcontributions to average:\n" );
		}

		for( line = start_line ; line <= stop_line ; line++ )
		{
			register unsigned char *cp;
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
				line * tp->t_w * 8 + (int)(xstart) * 8;

			for( col = start_col ; col <= stop_col ; col++ )
			{
				fastf_t col_upper, col_lower;
				double ttemp;

				col_upper = col + 1.0;
				if( col_upper > xstop )
					col_upper = xstop;
				col_lower = col;
				if( col_lower < xstart )
					col_lower = xstart;
				cell_area = line_factor * (col_upper - col_lower);
				tot_area += cell_area;

				ntohd( (unsigned char *)&ttemp, cp, 1 );

				if( rdebug & RDEBUG_SHADE )
					bu_log( "\t %g weight=%g (from col=%d line=%d)\n",
						ttemp,
						cell_area, col, line );

				temp += ttemp * cell_area;
			}
		}
		temp /= tot_area;
	}

	if( rdebug & RDEBUG_SHADE )
		bu_log( " average temp: %g\n", temp );

	swp->sw_temperature = temp;

	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );
	return(1);
}

/*
 *			T X T _ S E T U P
 */
HIDDEN int
temp_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, const struct mfuncs *mfp, struct rt_i *rtip)




                                /* New since 4.4 release */
{
	register struct temp_specific *tp;
	int		pixelbytes = 8;

	BU_CK_VLS( matparm );
	BU_GETSTRUCT( tp, temp_specific );
	*dpp = (char *)tp;

	tp->t_file[0] = '\0';
	tp->t_w = tp->t_n = -1;
	if( bu_struct_parse( matparm, temp_parse, (char *)tp ) < 0 )  {
		bu_free( (char *)tp, "temp_specific" );
		return(-1);
	}
	if( tp->t_w < 0 )  tp->t_w = 512;
	if( tp->t_n < 0 )  tp->t_n = tp->t_w;

	if( tp->t_file[0] == '\0' )  return -1;	/* FAIL, no file */
	if( !(tp->mp = bu_open_mapped_file( tp->t_file, NULL )) )
		return -1;				/* FAIL */

	/* Ensure file is large enough */
	if( tp->mp->buflen < tp->t_w * tp->t_n * pixelbytes )  {
		bu_log("\ntemp_setup() ERROR %s %s needs %d bytes, '%s' only has %d\n",
			rp->reg_name,
			mfp->mf_name,
			tp->t_w * tp->t_n * pixelbytes,
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
temp_print(register struct region *rp)
{
	bu_struct_print(rp->reg_name, temp_parse, (char *)rp->reg_udata);
}

/*
 *			T X T _ F R E E
 */
HIDDEN void
temp_free(char *cp)
{
	struct temp_specific *tp =
		(struct temp_specific *)cp;

	if( tp->mp )  bu_close_mapped_file( tp->mp );
	bu_free( cp, "temp_specific" );
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
