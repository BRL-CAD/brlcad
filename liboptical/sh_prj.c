/*
 *  S H _ P R J . C
 *
 *  Projection shader
 *
 *  Usage:
 *	shader prj file="foo.pix" w=256 n=512 N=x/y/z/w \
 *		   file="foo.pix" w=256 n=512 N=x/y/z/w \
 *		   file="foo.pix" w=256 n=512 N=x/y/z/w
 *
 *	For each image the "file" directive must come first.  This signals
 *	the shader to produce a new image structure.  All other options
 *	following "file" apply to that image until the next "file" directive.
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "../rt/mathtab.h"
#include "../rt/rdebug.h"

#define prj_MAGIC 0x1998    /* make this a unique number for each shader */
#define CK_prj_SP(_p) RT_CKMAG(_p, prj_MAGIC, "prj_specific")

#define NAME_LEN	128
#define IMAGE_MAGIC 0x1a
struct prj_image {
	struct bu_list	l;
	struct bu_mapped_file	*i_mp;		/* image data */
	unsigned 	i_width;		/* image description */
	unsigned 	i_height;
	unsigned 	i_depth;

	double		i_persp;		/* projection description */
	double		i_viewsize;
	point_t		i_eyept;
	quat_t		i_orient;

#if 0
	plane_t		i_plane;		/* computed */
#endif
	mat_t		i_mat;			/* view rot/scale matrix */
	char		i_fname[NAME_LEN];	/* file with the pixels */
	char		i_cfile[NAME_LEN];	/* where we got all this */
};

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct prj_specific {
	long		magic;	/* for memory validity check, must come 1st */
	char		prj_name[NAME_LEN];/* temp variable for filenames */
	mat_t		prj_m_to_sh;	/* model to shader space matrix */
	unsigned	prj_count;	/* Number of images in list prj_img */
	struct bu_list	prj_img;	/* list of images to map on object */
};



/* description of how to parse/print the arguments to the shader.  Not much
 * here as it is all done in the image-specific parsing 
 */
#define SHDR_NULL	((struct prj_specific *)0)
#define SHDR_O(m)	offsetof(struct prj_specific, m)
#define SHDR_AO(m)	offsetofarray(struct prj_specific, m)
static void image_hook();
struct bu_structparse prj_parse_tab[] = {
	{"%s",	NAME_LEN, "pfile", SHDR_AO(prj_name), image_hook },
	{"%s",	NAME_LEN, "p",	   SHDR_AO(prj_name), image_hook },
	{"",	0, (char *)0,	0,		      FUNC_NULL  }
};


#define IMG_NULL	((struct prj_image *)0)
#define IMG_O(m)	offsetof(struct prj_image, m)
#define IMG_AO(m)	offsetofarray(struct prj_image, m)
struct bu_structparse image_print_tab[] = {
	{"%s",	NAME_LEN,"file",IMG_AO(i_fname),	FUNC_NULL },
	{"%d",  1, "depth",	IMG_O(i_depth),		FUNC_NULL },
	{"%d",  1, "width",	IMG_O(i_width),		FUNC_NULL },
	{"%d",  1, "numscan",	IMG_O(i_height),	FUNC_NULL },
	{"%d",	1, "perpective",IMG_O(i_persp),		FUNC_NULL },
	{"%d",	1, "viewsize",	IMG_O(i_viewsize),	FUNC_NULL },
	{"%d",	3, "orientation",IMG_AO(i_orient),	FUNC_NULL },
	{"%d",	3, "eye_pt",	IMG_AO(i_eyept),	FUNC_NULL },
	{"",	0, (char *)0,	0,			FUNC_NULL }
};

struct bu_structparse image_parse_tab[] = {
	{"i",bu_byteoffset(image_print_tab[0]), "image_print_tab", 0, FUNC_NULL },
	{"%s",	NAME_LEN,"f",	IMG_AO(i_fname),	FUNC_NULL },
	{"%d",  1,	 "d",	IMG_O(i_depth),		FUNC_NULL },
	{"%d",  1,	 "w",	IMG_O(i_width),		FUNC_NULL },
	{"%d",  1,	 "n",	IMG_O(i_height),	FUNC_NULL },
	{"%d",	1,	 "p",	IMG_O(i_persp),		FUNC_NULL },
	{"%d",	1,	 "v",	IMG_O(i_viewsize),	FUNC_NULL },
	{"%d",	3,	 "o",	IMG_AO(i_orient),	FUNC_NULL },
	{"%d",	3, 	 "e",	IMG_AO(i_eyept),	FUNC_NULL },
	{"",	0, (char *)0,	0,			FUNC_NULL }
};

/*
 * Process a parameter file describing an image and how it is projected onto
 * the object.
 *
 */
static void
image_hook(sdp, name, base, value)
struct bu_structparse			*sdp;	/* structure description */
register CONST char			*name;	/* struct member name */
char					*base;	/* begining of structure */
CONST char				*value;	/* string containing value */
{
	struct prj_specific *prj_sp = (struct prj_specific *)sdp;
	struct prj_image *img;
	FILE *fd;
	int i;
	struct stat s;
	struct bu_vls img_params;

	i = stat(prj_sp->prj_name, &s);
	if ((fd=fopen(prj_sp->prj_name, "r")) == (FILE *)NULL) {
		return;
	}

	/* read in the parameters */
	bu_vls_init(&img_params);
	bu_vls_extend(&img_params, i);
	fread(bu_vls_addr(&img_params), 1, i, fd);
	(void)fclose(fd);

	/* set default values */
	BU_GETSTRUCT(img, prj_image);
	strncpy(img->i_cfile, prj_sp->prj_name, sizeof(img->i_cfile));
	img->i_width = img->i_height = 512;
	img->i_depth = 3;
	img->i_persp = 0.0;
	VSET(img->i_eyept, 0.0, 0.0, 1.0);
	QSET(img->i_orient, 0.0, 0.0, 0.0, 1.0 );
	img->i_viewsize = 1.0;

	/* parse specific values */
	i = bu_struct_parse( &img_params, image_parse_tab, (char *)img);
	bu_vls_free(&img_params);

	if (i < 0) {
		bu_log("Error parsing image parameter file %s\n",
			prj_sp->prj_name);
		bu_free(img, "prj_image");
		return;
	}

	/* map the pixel data */
	img->i_mp = bu_open_mapped_file(img->i_fname, NULL);
	if ( ! img->i_mp) {
		/* File couldn't be mapped */
		bu_log("Error mapping image file %s\n", img->i_fname);
		bu_free(img, "prj_image");
		return;
	}

	/* add to list */
	BU_LIST_APPEND(&prj_sp->prj_img, &(img->l));
	prj_sp->prj_count++;
}



/*	P R J _ P R I N T _ S P
 *
 *  Print prj_specific by printing each prj_image struct
 */
static void
prj_print_sp(prj_sp)
CONST struct prj_specific *prj_sp;
{
	struct prj_image *img;
	
	for(BU_LIST_FOR(img, prj_image, &(prj_sp->prj_img))) {
		bu_struct_print(img->i_cfile, image_print_tab, (char *)img);
		mat_print("mat", img->i_mat);
	}
}


HIDDEN int	prj_setup(), prj_render();
HIDDEN void	prj_print(), prj_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs prj_mfuncs[] = {
	{MF_MAGIC,	"prj",		0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	prj_setup,	prj_render,	prj_print,	prj_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	P R J _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
prj_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct prj_specific	*prj_sp;
	mat_t	tmp;
	vect_t	bb_min, bb_max, v_tmp;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);


	if( rdebug&RDEBUG_SHADE)
		rt_log("prj_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( prj_sp, prj_specific );
	*dpp = (char *)prj_sp;

	/* initialize the default values for the shader */
	prj_sp->magic = prj_MAGIC;
	*prj_sp->prj_name = '\0';
	bn_mat_idn(prj_sp->prj_m_to_sh);
	prj_sp->prj_count = 0;
	BU_LIST_INIT(&prj_sp->prj_img);

	/* The shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 */
	db_region_mat(prj_sp->prj_m_to_sh, rtip->rti_dbip, rp->reg_name);

	/* get the name of the parameter file(s) and parse them (via hook) */
	if( bu_struct_parse( matparm, prj_parse_tab, (char *)prj_sp ) < 0 )
		return(-1);

	if (! prj_sp->prj_count) {
		bu_bomb("No images specified for prj shader\n");
	}

	if( rdebug&RDEBUG_SHADE)
		prj_print_sp(prj_sp);

	return(1);
}

/*
 *	P R J _ P R I N T
 */
HIDDEN void
prj_print( rp, dp )
register struct region *rp;
char	*dp;
{
	prj_print_sp((struct prj_specific *)dp);
}

/*
 *	P R J _ F R E E
 */
HIDDEN void
prj_free( cp )
char *cp;
{
	struct prj_specific *prj_sp = (struct prj_specific *)cp;
	struct prj_image *img;

	while (BU_LIST_WHILE( img, prj_image, &(prj_sp->prj_img) )) {
		BU_LIST_DEQUEUE( &(img->l) );
		bu_close_mapped_file( img->i_mp );
		bu_free( (char *)img, "prj_image" );
	}

	rt_free( cp, "prj_specific" );
}

/*
 *	P R J _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
prj_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct prj_specific *prj_sp =
		(struct prj_specific *)dp;
	point_t pt;
	struct prj_image *i;
	struct prj_image *best;	/* image that best suits this hit point */
	double best_angle = 8.0;
	struct application my_ap;
	static plane_t pl = { 0.0, 0.0, 1.0, -1.0 };
	vect_t N;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_prj_SP(prj_sp);

	if( rdebug&RDEBUG_SHADE)
		prj_print_sp(prj_sp);


	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in prj_setup().
	 */
	MAT4X3PNT(pt, prj_sp->prj_m_to_sh, swp->sw_hit.hit_point);
	MAT4X3VEC(N, prj_sp->prj_m_to_sh, swp->sw_hit.hit_normal);

	if( rdebug&RDEBUG_SHADE) {
		rt_log("prj_render()  model:(%g %g %g) shader:(%g %g %g)\n", 
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}

	/* Find the image with the closest projection angle to the normal */
	best = (struct prj_image *)NULL;
	for (BU_LIST_FOR(i, prj_image, &prj_sp->prj_img)) {
		double dist;
		vect_t dir;
		point_t pl_pt;

/* XXX removed to make compilation work, fix and reinstate */
/*		if (VDOT(i->i_plane, N) >= 0.0) continue; */

		/* convert hit point into view coordinates of image */
		MAT4X3PNT(pl_pt, i->i_mat, pt);


		/* compute plane point */
		if (i->i_persp) {
			VSUB2(dir, pt, i->i_eyept);
		} else {
			VREVERSE(dir, pl);
		}

#if 0
/* XXX removed to make compilation work, fix and reinstate */
		switch (bn_isect_line3_plane(&dist, pt, dir, i->i_plane, &ap->a_rt_i->rti_tol)) {
		case 0: /* line lies on plane */
			break;
		case 1: /* hit entering */
			break;
		default:
			continue;
			break;
		}
#else
		dist = 42;
#endif
		/* get point on image plane */
		VJOIN1(pl_pt, pt, dist, dir);

		/* XXX fire ray to check self-occlusion */


		/* transform model space to image space */



	}

	VMOVE(swp->sw_color, pt);

	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}
