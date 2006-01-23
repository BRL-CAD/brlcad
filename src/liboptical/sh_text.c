/*                       S H _ T E X T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
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
/** @file sh_text.c
 *
 *  Texture map lookup
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
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <ctype.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
/*#include "../rt/mathtab.h"*/

#define TXT_NAME_LEN 128
struct txt_specific {
	int	tx_transp[3];	/* RGB for transparency */
	/*	char	tx_file[TXT_NAME_LEN];	 Filename */
	struct bu_vls tx_name;  /* name of object or file (depending on tx_datasrc flag) */
	int	tx_w;		/* Width of texture in pixels */
	int	tx_n;		/* Number of scanlines */
	int	tx_trans_valid;	/* boolean: is tx_transp valid ? */
	fastf_t	tx_scale[2];	/* replication factors in U, V */
	int	tx_mirror;	/* flag: repetitions are mirrored */
#define TXT_SRC_FILE 'f'
#define TXT_SRC_OBJECT  'o'
#define TXT_SRC_AUTO 0
	char tx_datasrc; /* which type of datasource */
	struct rt_binunif_internal *tx_binunifp;  /* db internal object when TXT_SRC_OBJECT */
	struct bu_mapped_file	*tx_mp;    /* mapped file when TXT_SRC_FILE */
};
#define TX_NULL	((struct txt_specific *)0)
#define TX_O(m)	bu_offsetof(struct txt_specific, m)
#define TX_AO(m)	bu_offsetofarray(struct txt_specific, m)

struct region	env_region;  /* initialized in the app code view handler */

extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);

HIDDEN void txt_transp_hook(struct bu_structparse *ptab, char *name, char *cp, char *value);
HIDDEN void txt_source_hook(const struct bu_structparse *ip, const char *sp_name, genptr_t base, char *p);
HIDDEN int txt_load_datasource(struct txt_specific *texture, struct db_i *dbInstance, const long unsigned int size);
HIDDEN int bwtxt_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN int txt_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, const struct mfuncs *mfp, struct rt_i *rtip), txt_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN int ckr_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), ckr_render(struct application *ap, struct partition *pp, register struct shadework *swp, char *dp);
HIDDEN int bmp_setup(), bmp_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void bwtxtprint(), bwtxtfree();
HIDDEN void txt_print(register struct region *rp), txt_free(char *cp);
HIDDEN void ckr_print(register struct region *rp), ckr_free(char *cp);
HIDDEN void bmp_print(), bmp_free();
HIDDEN int tstm_render(struct application *ap, struct partition *pp, register struct shadework *swp, char *dp);
HIDDEN int star_render(register struct application *ap, register struct partition *pp, struct shadework *swp, char *dp);
HIDDEN int envmap_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, const struct mfuncs *mfp, struct rt_i *rtip, struct mfuncs **headp);
extern int mlib_zero(), mlib_one();
extern void mlib_void();


struct mfuncs txt_mfuncs[] = {
	{MF_MAGIC,	"texture",	0,		MFI_UV,		0,
	txt_setup,	txt_render,	txt_print,	txt_free },

	{MF_MAGIC,	"bwtexture",	0,		MFI_UV,		0,
	txt_setup,	bwtxt_render,	txt_print,	txt_free },

	{MF_MAGIC,	"checker",	0,		MFI_UV,		0,
	ckr_setup,	ckr_render,	ckr_print,	ckr_free },

	{MF_MAGIC,	"testmap",	0,		MFI_UV,		0,
	mlib_one,	tstm_render,	mlib_void,	mlib_void },

	{MF_MAGIC,	"fakestar",	0,		0,		0,
	mlib_one,	star_render,	mlib_void,	mlib_void },

	{MF_MAGIC,	"bump",		0,		MFI_UV|MFI_NORMAL, 0,
	txt_setup,	bmp_render,	txt_print,	txt_free },

	{MF_MAGIC,	"envmap",	0,		0,		0,
	envmap_setup,	mlib_zero,	mlib_void,	mlib_void },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};

struct bu_structparse txt_parse[] = {
	{"%d",	1, "transp",	bu_offsetofarray(struct txt_specific, tx_transp),	txt_transp_hook },
	{"%S",	1, "file", TX_O(tx_name),		txt_source_hook },
	{"%S",	1, "obj", TX_O(tx_name),		txt_source_hook },
	{"%S",	1, "object", TX_O(tx_name),		txt_source_hook },
	{"%S",	1, "texture", TX_O(tx_name),	 BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "w",		TX_O(tx_w),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n",		TX_O(tx_n),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "l",		TX_O(tx_n),		BU_STRUCTPARSE_FUNC_NULL }, /*compat*/
	{"%d",	1, "trans_valid",TX_O(tx_trans_valid),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "t",		TX_O(tx_trans_valid),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  2, "uv",	TX_AO(tx_scale), 	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "m",		TX_O(tx_mirror),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};


/* txt_datasource_hook() is used to automatically try to load a default texture
 * datasource.  The type gets set to auto and the datasource is detected.  First
 * the database is searched for a matching object, then a file on disk is
 * looked up.  If neither is found, object name is left null so txt_setup() will
 * fail.
 */
HIDDEN void txt_source_hook(const struct bu_structparse *ip, const char *sp_name, genptr_t base, char *p) {
	struct txt_specific *textureSpecific = (struct txt_specific *)base;
	if (strncmp(sp_name, "file", 4)==0) {
		textureSpecific->tx_datasrc=TXT_SRC_FILE;
	}
	else if (strncmp(sp_name, "obj", 3)==0) {
		textureSpecific->tx_datasrc=TXT_SRC_OBJECT;
	}
	else {
		textureSpecific->tx_datasrc=TXT_SRC_AUTO;
	}
}


/*
 *			T X T _ T R A N S P _ H O O K
 *
 *  Hooked function, called by bu_structparse
 */
HIDDEN void
txt_transp_hook(struct bu_structparse *ptab, char *name, char *cp, char *value)
{
	register struct txt_specific *tp =
		(struct txt_specific *)cp;

	if (!strcmp(name, txt_parse[0].sp_name) && ptab == txt_parse) {
		tp->tx_trans_valid = 1;
	} else {
		bu_log("file:%s, line:%d txt_transp_hook name:(%s) instead of (%s)\n",
			__FILE__, __LINE__, name, txt_parse[0].sp_name);
	}
}


/*
 *	t x t _ l o a d _ d a t a s o u r c e
 *
 * This is a helper routine used in txt_setup() to load a texture either from
 * a file or from a db object.  The resources are released in txt_free()
 * (there is no specific unload_datasource function).
 */
HIDDEN int txt_load_datasource(struct txt_specific *texture, struct db_i *dbInstance, const unsigned long int size) {
	struct directory *dirEntry;

	RT_CK_DBI(dbInstance);

	if (texture == (struct txt_specific *)NULL) {
		bu_bomb("ERROR: txt_load_datasource() received NULL arg (struct txt_specific *)\n");
	}

	bu_log("Loading texture %s [%S]...", texture->tx_datasrc==TXT_SRC_AUTO?"from auto-determined datasource":texture->tx_datasrc==TXT_SRC_OBJECT?"from a database object":texture->tx_datasrc==TXT_SRC_FILE?"from a file":"from an unknown source (ERROR)", &texture->tx_name);

	/* if the source is auto or object, we try to load the object */
	if ((texture->tx_datasrc==TXT_SRC_AUTO) || (texture->tx_datasrc==TXT_SRC_OBJECT)) {

		/* see if the object exists */
		if ( (dirEntry=db_lookup(dbInstance, bu_vls_addr(&texture->tx_name), LOOKUP_QUIET)) == DIR_NULL) {

			/* unable to find the texture object */
			if (texture->tx_datasrc!=TXT_SRC_AUTO) {
				return -1;
			}
		}
		else {
			struct rt_db_internal *dbip=(struct rt_db_internal *)bu_malloc(sizeof(struct rt_db_internal), "txt_load_datasource");

			RT_INIT_DB_INTERNAL(dbip);
			RT_CK_DB_INTERNAL(dbip);
			RT_CK_DIR(dirEntry);

			/* the object was in the directory, so go get it */
			if (rt_db_get_internal(dbip, dirEntry, dbInstance, NULL, NULL) <= 0) {
				/* unable to load/create the texture database record object */
				return -1;
			}

			RT_CK_DB_INTERNAL(dbip);
			RT_CK_BINUNIF(dbip->idb_ptr);

			/* keep the binary object pointer */
			texture->tx_binunifp=(struct rt_binunif_internal *)dbip->idb_ptr; /* make it so */

			/* release the database instance pointer struct we created */
			RT_INIT_DB_INTERNAL(dbip);
			bu_free(dbip, "txt_load_datasource");

			/* check size of object */
			if (texture->tx_binunifp->count < size) {
				bu_log("\nWARNING: %S needs %d bytes, binary object only has %d\n", texture->tx_name, size, texture->tx_binunifp->count);
			} else if (texture->tx_binunifp->count > size) {
				bu_log("\nWARNING: Binary object is larger than specified texture size\n\tBinary Object: %d pixels\n\tSpecified Texture Size: %d pixels\n...continuing to load using image subsection...", texture->tx_binunifp->count);
			}
		}
	}

	/* if we are auto and we couldn't find a database object match, or if source
	 * is explicitly a file then we load the file.
	 */
	if ( ( (texture->tx_datasrc==TXT_SRC_AUTO) && (texture->tx_binunifp==NULL) ) || (texture->tx_datasrc==TXT_SRC_FILE) ) {

		texture->tx_mp = bu_open_mapped_file_with_path(dbInstance->dbi_filepath,	bu_vls_addr(&texture->tx_name), NULL);

		if ( texture->tx_mp==NULL )
			return -1;				/* FAIL */

		if (texture->tx_mp->buflen < size) {
			bu_log("\nWARNING: %S needs %d bytes, file only has %d\n", &texture->tx_name, size, texture->tx_mp->buflen);
		} else if (texture->tx_mp->buflen > size) {
			bu_log("\nWARNING: Texture file size is larger than specified texture size\n\tInput File: %d pixels\n\tSpecified Texture Size: %d pixels\n...continuing to load using image subsection...", texture->tx_mp->buflen, size);
		}

	}

	bu_log("done.\n");

	return 0;
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
txt_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct txt_specific *tp =
		(struct txt_specific *)dp;
	fastf_t xmin, xmax, ymin, ymax;
	int dx, dy;
	register fastf_t r,g,b;
	struct uvcoord uvc;
	long tmp;

	RT_CK_AP(ap);
	RT_CHECK_PT(pp);

	uvc = swp->sw_uv;

	if (rdebug & RDEBUG_SHADE )
		bu_log( "in txt_render(): du=%g, dv=%g\n",
			uvc.uv_du, uvc.uv_dv );

	/* take care of scaling U,V coordinates to get the desired amount
	 * of replication of the texture
	 */
	uvc.uv_u *= tp->tx_scale[X];
	tmp = uvc.uv_u;
	uvc.uv_u -= tmp;
	if (tp->tx_mirror && (tmp & 1) )
		uvc.uv_u = 1.0 - uvc.uv_u;

	uvc.uv_v *= tp->tx_scale[Y];
	tmp = uvc.uv_v;
	uvc.uv_v -= tmp;
	if (tp->tx_mirror && (tmp & 1) )
		uvc.uv_v = 1.0 - uvc.uv_v;

	uvc.uv_du /= tp->tx_scale[X];
	uvc.uv_dv /= tp->tx_scale[Y];

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */

	if ((bu_vls_strlen(&tp->tx_name)<=0) || (!tp->tx_mp && !tp->tx_binunifp)) {
		bu_log("WARNING: texture [%S] could not be read\n", &tp->tx_name);
		VSET( swp->sw_color, uvc.uv_u, 0, uvc.uv_v );
		if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}

	/* u is left->right index, v is line number bottom->top */
	/* Don't filter more than 1/8 of the texture for 1 pixel! */
	if (uvc.uv_du > 0.125 )  uvc.uv_du = 0.125;
	if (uvc.uv_dv > 0.125 )  uvc.uv_dv = 0.125;

	if (uvc.uv_du < 0 || uvc.uv_dv < 0 )  {
		bu_log("txt_render uv=%g,%g, du dv=%g %g seg=%s\n",
			uvc.uv_u, uvc.uv_v, uvc.uv_du, uvc.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		uvc.uv_du = uvc.uv_dv = 0;
	}

	xmin = uvc.uv_u - uvc.uv_du;
	xmax = uvc.uv_u + uvc.uv_du;
	ymin = uvc.uv_v - uvc.uv_dv;
	ymax = uvc.uv_v + uvc.uv_dv;
	if (xmin < 0 )  xmin = 0;
	if (ymin < 0 )  ymin = 0;
	if (xmax > 1 )  xmax = 1;
	if (ymax > 1 )  ymax = 1;

	if (rdebug & RDEBUG_SHADE )
		bu_log( "footprint in texture space is (%g %g) <-> (%g %g)\n",
			xmin * (tp->tx_w-1), ymin * (tp->tx_n-1),
			xmax * (tp->tx_w-1), ymax * (tp->tx_n-1) );

#if 1
	dx = (int)(xmax * (tp->tx_w-1)) - (int)(xmin * (tp->tx_w-1));
	dy = (int)(ymax * (tp->tx_n-1)) - (int)(ymin * (tp->tx_n-1));

	if (rdebug & RDEBUG_SHADE )
		bu_log( "\tdx = %d, dy = %d\n", dx, dy );

	if (dx == 0 && dy == 0 )
	{
		/* No averaging necessary */

		register unsigned char *cp=NULL;

		if (tp->tx_mp) {
			cp = ((unsigned char *)(tp->tx_mp->buf)) +
				(int)(ymin * (tp->tx_n-1)) * tp->tx_w * 3 +
				(int)(xmin * (tp->tx_w-1)) * 3;
		}
		else if (tp->tx_binunifp) {
			cp = ((unsigned char *)(tp->tx_binunifp->u.uint8)) +
				(int)(ymin * (tp->tx_n-1)) * tp->tx_w * 3 +
				(int)(xmin * (tp->tx_w-1)) * 3;
		}
		else {
			bu_bomb("sh_text.c -- No texture data found\n");
		}
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

		xstart = xmin * (tp->tx_w-1);
		xstop = xmax * (tp->tx_w-1);
		ystart = ymin * (tp->tx_n-1);
		ystop = ymax * (tp->tx_n-1);

		start_line = ystart;
		stop_line = ystop;
		start_col = xstart;
		stop_col = xstop;

		r = g = b = 0.0;

		if (rdebug & RDEBUG_SHADE ) {
			bu_log( "\thit in texture space = (%g %g)\n", uvc.uv_u * (tp->tx_w-1), uvc.uv_v * (tp->tx_n-1) );
			bu_log( "\t averaging from  (%g %g) to (%g %g)\n", xstart, ystart, xstop, ystop );
			bu_log( "\tcontributions to average:\n" );
		}

		for( line = start_line ; line <= stop_line ; line++ ) {
			register unsigned char *cp=NULL;
			fastf_t line_factor;
			fastf_t line_upper, line_lower;

			line_upper = line + 1.0;
			if (line_upper > ystop )
				line_upper = ystop;
			line_lower = line;
			if (line_lower < ystart )
				line_lower = ystart;
			line_factor = line_upper - line_lower;

			if (tp->tx_mp) {
				cp = ((unsigned char *)(tp->tx_mp->buf)) +
					line * tp->tx_w * 3 + (int)(xstart) * 3;
			}
			else if (tp->tx_binunifp) {
				cp = ((unsigned char *)(tp->tx_binunifp->u.uint8)) +
					line * tp->tx_w * 3 + (int)(xstart) * 3;
			}
			else {
				/* not reachable */
				bu_bomb("sh_text.c -- Unable to read datasource\n");
			}

			for( col = start_col ; col <= stop_col ; col++ ) {
				fastf_t col_upper, col_lower;

				col_upper = col + 1.0;
				if (col_upper > xstop )	col_upper = xstop;
				col_lower = col;
				if (col_lower < xstart ) col_lower = xstart;

				cell_area = line_factor * (col_upper - col_lower);
				tot_area += cell_area;

				if (rdebug & RDEBUG_SHADE )
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

	if (rdebug & RDEBUG_SHADE )
		bu_log( " average: %g %g %g\n", r, g, b );
#else

	x = xmin * (tp->tx_w-1);
	y = ymin * (tp->tx_n-1);
	dx = (xmax - xmin) * (tp->tx_w-1);
	dy = (ymax - ymin) * (tp->tx_n-1);
	if (dx < 1 )  dx = 1;
	if (dy < 1 )  dy = 1;

	if (rdebug & RDEBUG_SHADE )
		bu_log(" in txt_render(): x=%d y=%d, dx=%d, dy=%d\n", x, y, dx, dy);

	r = g = b = 0;
	for( line=0; line<dy; line++ )  {
		register unsigned char *cp;
		register unsigned char *ep;

		if (tp->tx_mp) {
			cp = ((unsigned char *)(tp->tx_mp->buf)) +
				(y+line) * tp->tx_w * 3  +  x * 3;
		}
		else if (tp->tx_binunifp) {
			cp = ((unsigned char *)(tp->tx_binunifp->u.unit8)) +
				(y+line) * tp->tx_w * 3  +  x * 3;
		}
		else {
			/* not reachable */
			bu_bomb("sh_text.c -- Unable to read datasource\n");
		}

		ep = cp + 3*dx;
		while( cp < ep )  {
			if (rdebug & RDEBUG_SHADE )
				bu_log( "\tAdding %d %d %d\n", *cp, *(cp+1), *(cp+2) );
			r += *cp++;
			g += *cp++;
			b += *cp++;
		}
	}
	if (rdebug & RDEBUG_SHADE )
		bu_log( "Totals: %d %d %d,", r, g, b );
	r /= (dx*dy);
	g /= (dx*dy);
	b /= (dx*dy);
	if (rdebug & RDEBUG_SHADE )
		bu_log( " average: %d %d %d\n", r, g, b );
#endif

	if (!tp->tx_trans_valid) {
opaque:
		VSET( swp->sw_color,
			r * bn_inv255,
			g * bn_inv255,
			b * bn_inv255 );

		if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}
	/* This circumlocution needed to keep expression simple for Cray,
	 * and others
	 */
	if (r != ((long)tp->tx_transp[0]) )  goto opaque;
	if (g != ((long)tp->tx_transp[1]) )  goto opaque;
	if (b != ((long)tp->tx_transp[2]) )  goto opaque;

	/*
	 *  Transparency mapping is enabled, and we hit a transparent spot.
	 *  Let higher level handle it in reflect/refract code.
	 */
	swp->sw_transmit = 1.0;
	swp->sw_reflect = 0.0;

	bu_log("leaving txt_render()\n");

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
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
bwtxt_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct txt_specific *tp =
		(struct txt_specific *)dp;
	fastf_t xmin, xmax, ymin, ymax;
	int line;
	int dx, dy;
	int x,y;
	register long bw;
	struct uvcoord uvc;
	long tmp;

	uvc = swp->sw_uv;

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug colors
	 */
	if ((bu_vls_strlen(&tp->tx_name)<=0) || (!tp->tx_mp && !tp->tx_binunifp) )  {
		VSET( swp->sw_color, uvc.uv_u, 0, uvc.uv_v );
		if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}

	/* take care of scaling U,V coordinates to get the desired amount
	 * of replication of the texture
	 */
	uvc.uv_u *= tp->tx_scale[X];
	tmp = uvc.uv_u;
	uvc.uv_u -= tmp;
	if (tp->tx_mirror && (tmp & 1) )
		uvc.uv_u = 1.0 - uvc.uv_u;

	uvc.uv_v *= tp->tx_scale[Y];
	tmp = uvc.uv_v;
	uvc.uv_v -= tmp;
	if (tp->tx_mirror && (tmp & 1) )
		uvc.uv_v = 1.0 - uvc.uv_v;

	uvc.uv_du /= tp->tx_scale[X];
	uvc.uv_dv /= tp->tx_scale[Y];


	/* u is left->right index, v is line number bottom->top */
	/* Don't filter more than 1/8 of the texture for 1 pixel! */
	if (uvc.uv_du > 0.125 )  uvc.uv_du = 0.125;
	if (uvc.uv_dv > 0.125 )  uvc.uv_dv = 0.125;

	if (uvc.uv_du < 0 || uvc.uv_dv < 0 )  {
		bu_log("bwtxt_render uv=%g,%g, du dv=%g %g seg=%s\n",
			uvc.uv_u, uvc.uv_v, uvc.uv_du, uvc.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		uvc.uv_du = uvc.uv_dv = 0;
	}
	xmin = uvc.uv_u - uvc.uv_du;
	xmax = uvc.uv_u + uvc.uv_du;
	ymin = uvc.uv_v - uvc.uv_dv;
	ymax = uvc.uv_v + uvc.uv_dv;
	if (xmin < 0 )  xmin = 0;
	if (ymin < 0 )  ymin = 0;
	if (xmax > 1 )  xmax = 1;
	if (ymax > 1 )  ymax = 1;
	x = xmin * (tp->tx_w-1);
	y = ymin * (tp->tx_n-1);
	dx = (xmax - xmin) * (tp->tx_w-1);
	dy = (ymax - ymin) * (tp->tx_n-1);
	if (dx < 1 )  dx = 1;
	if (dy < 1 )  dy = 1;
	bw = 0;
	for( line=0; line<dy; line++ )  {
		register unsigned char *cp=NULL;
		register unsigned char *ep;

		if (tp->tx_mp) {
			cp = ((unsigned char *)(tp->tx_mp->buf)) +
				(y+line) * tp->tx_w  +  x;
		}
		else if (tp->tx_binunifp) {
			cp = ((unsigned char *)(tp->tx_binunifp->u.uint8)) +
				(y+line) * tp->tx_w  +  x;
		}
		else {
			/* not reachable */
			bu_bomb("sh_text.c -- Unable to read datasource\n");
		}

		ep = cp + dx;
		while( cp < ep )  {
			bw += *cp++;
		}
	}

	if (!tp->tx_trans_valid) {
opaque:
		VSETALL( swp->sw_color,
			bw * bn_inv255 / (dx*dy) );
		if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}
	/* This circumlocution needed to keep expression simple for Cray,
	 * and others
	 */
	if (bw / (dx*dy) != ((long)tp->tx_transp[0]) )  goto opaque;

	/*
	 *  Transparency mapping is enabled, and we hit a transparent spot.
	 *  Let higher level handle it in reflect/refract code.
	 */
	swp->sw_transmit = 1.0;
	swp->sw_reflect = 0.0;
	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );
	return(1);
}

/*
 *			T X T _ S E T U P
 */
HIDDEN int
txt_setup( register struct region *rp, struct bu_vls *matparm, char **dpp, const struct mfuncs *mfp, struct rt_i *rtip ) {
	register struct txt_specific *tp;
	int		pixelbytes = 3;

	BU_CK_VLS( matparm );
	BU_GETSTRUCT( tp, txt_specific );
	*dpp = (char *)tp;

	bu_vls_init(&tp->tx_name);
	/* !?!	tp->tx_name[0] = '\0';*/

	/* defaults */
	tp->tx_w = tp->tx_n = -1;
	tp->tx_trans_valid = 0;
	tp->tx_scale[X] = 1.0;
	tp->tx_scale[Y] = 1.0;
	tp->tx_mirror = 0;
	tp->tx_datasrc = 0; /* source is auto-located by default */
	tp->tx_binunifp = NULL;
	tp->tx_mp = NULL;

	/* load given values */
	if (bu_struct_parse( matparm, txt_parse, (char *)tp ) < 0 )  {
		bu_free( (char *)tp, "txt_specific" );
		return(-1);
	}

	/* validate values */
	if (tp->tx_w < 0 ) tp->tx_w = 512;
	if (tp->tx_n < 0 ) tp->tx_n = tp->tx_w;
	if (tp->tx_trans_valid ) rp->reg_transmit = 1;
	BU_CK_VLS(&tp->tx_name);
	if (bu_vls_strlen(&tp->tx_name)<=0) return -1;
	/*	!?! if (tp->tx_name[0] == '\0' )  return -1;	*/ /* FAIL, no file */

	if (strcmp( mfp->mf_name, "bwtexture" ) == 0 ) pixelbytes = 1;

	/* load the texture from its datasource */
	if (txt_load_datasource(tp, rtip->rti_dbip, tp->tx_w * tp->tx_n * pixelbytes)<0) {
		bu_log("\nERROR: txt_setup() %s %s could not be loaded [source was %s]\n", rp->reg_name, bu_vls_addr(&tp->tx_name), tp->tx_datasrc==TXT_SRC_OBJECT?"object":tp->tx_datasrc==TXT_SRC_FILE?"file":"auto");
		return -1;
	}


	if (rdebug & RDEBUG_SHADE ) {
		bu_log("txt_setup: texture loaded!  type=%s name=%s\n", tp->tx_datasrc==TXT_SRC_AUTO?"auto":tp->tx_datasrc==TXT_SRC_OBJECT?"object":tp->tx_datasrc==TXT_SRC_FILE?"file":"unknown", bu_vls_addr(&tp->tx_name));
		bu_struct_print("texture", txt_parse, (char *)tp);
	}

	return 1;				/* OK */
}

/*
 *			T X T _ P R I N T
 */
HIDDEN void
txt_print(register struct region *rp)
{
	bu_struct_print(rp->reg_name, txt_parse, (char *)rp->reg_udata);
}

/*
 *			T X T _ F R E E
 */
HIDDEN void
txt_free(char *cp)
{
	struct txt_specific *tp =	(struct txt_specific *)cp;

	bu_vls_free(&tp->tx_name);
	if (tp->tx_binunifp) rt_binunif_free( tp->tx_binunifp );
	if (tp->tx_mp) bu_close_mapped_file( tp->tx_mp );
	tp->tx_binunifp = GENPTR_NULL; /* sanity */
	tp->tx_mp = GENPTR_NULL; /* sanity */
	bu_free( cp, "txt_specific" );
}

struct ckr_specific  {
	int	ckr_a[3];	/* first RGB */
	int	ckr_b[3];	/* second RGB */
	double	ckr_scale;
};
#define CKR_NULL	((struct ckr_specific *)0)
#define CKR_O(m)	bu_offsetof(struct ckr_specific, m)

struct bu_structparse ckr_parse[] = {
	{"%d",	3, "a",	bu_offsetofarray(struct ckr_specific, ckr_a), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	3, "b",	bu_offsetofarray(struct ckr_specific, ckr_b), BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "s", bu_offsetof(struct ckr_specific, ckr_scale), BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

/*
 *			C K R _ R E N D E R
 */
HIDDEN int
ckr_render(struct application *ap, struct partition *pp, register struct shadework *swp, char *dp)
{
	register struct ckr_specific *ckp =
		(struct ckr_specific *)dp;
	register int *cp;
	int u, v;

	u = swp->sw_uv.uv_u * ckp->ckr_scale;
	v = swp->sw_uv.uv_v * ckp->ckr_scale;

	if ( ((u&1) && (v&1)) || (!(u&1) && !(v&1)) ) {
		cp = ckp->ckr_a;
	} else {
		cp = ckp->ckr_b;
	}

#if 0
	if ((swp->sw_uv.uv_u < 0.5 && swp->sw_uv.uv_v < 0.5) ||
	    (swp->sw_uv.uv_u >=0.5 && swp->sw_uv.uv_v >=0.5) )  {
		cp = ckp->ckr_a;
	} else {
		cp = ckp->ckr_b;
	}
#endif

	VSET( swp->sw_color,
		(unsigned char)cp[0] * bn_inv255,
		(unsigned char)cp[1] * bn_inv255,
		(unsigned char)cp[2] * bn_inv255 );

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}

/*
 *			C K R _ S E T U P
 */
HIDDEN int
ckr_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)




                                /* New since 4.4 release */
{
	register struct ckr_specific *ckp;

	/* Default will be white and black checkers */
	BU_GETSTRUCT( ckp, ckr_specific );
	*dpp = (char *)ckp;
	ckp->ckr_a[0] = ckp->ckr_a[1] = ckp->ckr_a[2] = 255;
	ckp->ckr_b[0] = ckp->ckr_b[1] = ckp->ckr_b[2] = 0;
	ckp->ckr_scale = 2.0;
	if (bu_struct_parse( matparm, ckr_parse, (char *)ckp ) < 0 )  {
		bu_free( (char *)ckp, "ckr_specific" );
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
ckr_print(register struct region *rp)
{
	bu_struct_print(rp->reg_name, ckr_parse, rp->reg_udata);
}

/*
 *			C K R _ F R E E
 */
HIDDEN void
ckr_free(char *cp)
{
	bu_free( cp, "ckr_specific" );
}

/*
 *			T S T M _ R E N D E R
 *
 *  Render a map which varries red with U and blue with V values.
 *  Mostly useful for debugging ft_uv() routines.
 */
HIDDEN int
tstm_render(struct application *ap, struct partition *pp, register struct shadework *swp, char *dp)
{
	VSET( swp->sw_color, swp->sw_uv.uv_u, 0, swp->sw_uv.uv_v );

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
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
HIDDEN int
star_render(register struct application *ap, register struct partition *pp, struct shadework *swp, char *dp)
{
	/* Probably want to diddle parameters based on what part of sky */
	if (bn_rand0to1(ap->a_resource->re_randptr) >= 0.98 )  {
		register int i;
		FAST fastf_t f;
		i = (sizeof(star_colors)-1) / sizeof(star_colors[0]);

		/* "f" used for intermediate result to avoid an SGI compiler error */
		f = bn_rand0to1(ap->a_resource->re_randptr);
		i = ((double)i) * f;

		f = bn_rand0to1(ap->a_resource->re_randptr);
		VSCALE( swp->sw_color, star_colors[i], f );
	} else {
		VSETALL( swp->sw_color, 0 );
	}

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
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
HIDDEN int
bmp_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)
{
	register struct txt_specific *tp =
		(struct txt_specific *)dp;
	unsigned char *cp=NULL;
	fastf_t	pertU, pertV;
	vect_t	y;		/* world coordinate axis vectors */
	vect_t	u, v;		/* surface coord system vectors */
	int	i, j;		/* bump map pixel indicies */

	/*
	 * If no texture file present, or if
	 * texture isn't and can't be read, give debug color.
	 */
	if ( (bu_vls_strlen(&tp->tx_name)<=0) || (!tp->tx_mp && !tp->tx_binunifp) )  {
		VSET( swp->sw_color, swp->sw_uv.uv_u, 0, swp->sw_uv.uv_v );
		if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
			(void)rr_render( ap, pp, swp );
		return(1);
	}
	/* u is left->right index, v is line number bottom->top */
	if (swp->sw_uv.uv_u < 0 || swp->sw_uv.uv_u > 1 || swp->sw_uv.uv_v < 0 || swp->sw_uv.uv_v > 1 )  {
		bu_log("bmp_render:  bad u,v=%g,%g du,dv=%g,%g seg=%s\n",
			swp->sw_uv.uv_u, swp->sw_uv.uv_v,
			swp->sw_uv.uv_du, swp->sw_uv.uv_dv,
			pp->pt_inseg->seg_stp->st_name );
		VSET( swp->sw_color, 0, 1, 0 );
		if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
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

	if (tp->tx_mp) {
		cp = ((unsigned char *)(tp->tx_mp->buf)) +
			(j) * tp->tx_w * 3  +  i * 3;
	}
	else if (tp->tx_binunifp) {
		cp = ((unsigned char *)(tp->tx_binunifp->u.uint8)) +
			(j) * tp->tx_w * 3  +  i * 3;
	}
	else {
		/* not reachable */
		bu_bomb("sh_text.c -- Unreachable code reached while reading datasource\n");
	}
	pertU = ((fastf_t)(*cp) - 128.0) / 128.0;
	pertV = ((fastf_t)(*(cp+2)) - 128.0) / 128.0;

	if (rdebug&RDEBUG_LIGHT ) {
		VPRINT("normal", swp->sw_hit.hit_normal);
		VPRINT("u", u );
		VPRINT("v", v );
		bu_log("cu = %d, cv = %d\n", *cp, *(cp+2));
		bu_log("pertU = %g, pertV = %g\n", pertU, pertV);
	}
	VJOIN2( swp->sw_hit.hit_normal, swp->sw_hit.hit_normal, pertU, u, pertV, v );
	VUNITIZE( swp->sw_hit.hit_normal );
	if (rdebug&RDEBUG_LIGHT ) {
		VPRINT("after", swp->sw_hit.hit_normal);
	}

	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}

/*
 *			E N V M A P _ S E T U P
 */
HIDDEN int
envmap_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, const struct mfuncs *mfp, struct rt_i *rtip, struct mfuncs **headp)
{

	BU_CK_VLS( matparm );
	RT_CK_RTI(rtip);
	if (env_region.reg_mfuncs )  {
		bu_log("envmap_setup:  second environment map ignored\n");
		return(0);		/* drop region */
	}
	env_region = *rp;		/* struct copy */
	/* Get copies of, or eliminate, references to dynamic structures */
	env_region.reg_name = bu_strdup( rp->reg_name );
	env_region.reg_treetop = TREE_NULL;
	env_region.l.forw = BU_LIST_NULL;
	env_region.l.back = BU_LIST_NULL;
	env_region.reg_mfuncs = (char *)0;

	env_region.reg_mater.ma_shader = bu_vls_strdup( matparm );

	if (mlib_setup( headp, &env_region, rtip ) < 0 )
		bu_log("envmap_setup() material '%s' failed\n", env_region.reg_mater );

	return(0);		/* This region should be dropped */
}



/*
 *			M L I B _ Z E R O
 *
 *  Regardless of arguments, always return zero.
 *  Useful mostly as a stub print, and/or free routine.
 */
/* VARARGS */
int
mlib_zero()
{
	return(0);
}

/*
 *			M L I B _ O N E
 *
 *  Regardless of arguments, always return one.
 *  Useful mostly as a stub setup routine.
 */
/* VARARGS */
int
mlib_one()
{
	return(1);
}

/*
 *			M L I B _ V O I D
 */
/* VARARGS */
void
mlib_void()
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
