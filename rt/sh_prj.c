/*
 *	S H _ P R J . C
 *
 *	A "slide projector" shader that projects multiple images
 *	onto an object.
 *
 *	The parameter "cfile" to the shader is the name of a control file that
 *	contains the real parameters.  This file contains a set of
 *	image and projection descriptions.  Comments in this file are
 *	denoted by a "#" character in column 1.  The "file" item
 *	must be the first for each new image projection described.
 *	The other items may be in any order.
 *
 *	# pt,x,y are in region coordinate system
 *
 *	# the view from the top, a 640x480 pix file
 *	file=top_view.pix
 *	dim=640,480,3
 *	pt=0,0,100
 *	x=640,0,0
 *	y=0,480,0
 *	#
 *	# The view from the right a 1024x768 bw file
 *	file=right_view.pix
 *	dim=1024,768,1
 *	pt=-20,-40,-20
 *	x=1024,0,0
 *	y=0,0,768
 *
 *	The parameter "method" is an integer indicating which method
 *	is used to choose the image which shades a hit-point.
 *
 *	Method	
 *	   1	choose image whose surface normal is closest to that of
 *			hit point normal.
 *	   
 *	Suggested Other methods:
 *	   
 *	   2	closest plane to pt
 *	   3	Average of visible plane pixels.
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"

#define prj_MAGIC 0x8194    /* make this a unique number for each shader */
#define CK_prj_SP(_p) RT_CKMAG(_p, prj_MAGIC, "prj_specific")

/* Description of a single "slide" or image to project */
struct slide {
	struct rt_list	l;
	struct bu_vls	sl_file;	/* name of file with image */
	unsigned char	*sl_img;	/* actual image data */
	int		sl_dim[3];	/* x,y,depth dimensions of image */
	point_t		sl_pt;
	vect_t		sl_x;		/* direction of image X in plane */
	double		sl_xdist;
	vect_t		sl_y;		/* direction of image X in plane */
	double		sl_ydist;
	plane_t		sl_plane;	/* eqn of plane of image */
};

struct bu_structparse prj_file_parse_tab[] = {
	{"%S",	1, "file", offsetof(struct slide, sl_file),	FUNC_NULL },
	{"%d",	3, "dim",  offsetofarray(struct slide, sl_dim),	FUNC_NULL },
	{"%f",	3, "x",    offsetofarray(struct slide, sl_x),	FUNC_NULL },
	{"%f",	3, "y",    offsetofarray(struct slide, sl_y),	FUNC_NULL },
	{"%f",	3, "pt",  offsetofarray(struct slide, sl_pt),	FUNC_NULL },


	{"%f",	4, "plane", offsetofarray(struct slide, sl_plane),FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};


/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct prj_specific {
	long		magic;		/* magic # for memory validity check, must come 1st */
	int		prj_method;	/* image selection method */
	mat_t		prj_m_to_r;	/* model to region space matrix */
	struct bu_vls	prj_cfile;	/* name of control file */
	struct bu_list	prj_slides;		/* linked list of images */
};



/* The default values for the variables in the shader specific structure */
CONST static
struct prj_specific prj_defaults = {
	prj_MAGIC,
	1,
	{	1.0, 0.0, 0.0, 0.0,	/* prj_m_to_r */
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0 }
	};

#define SHDR_NULL	((struct prj_specific *)0)
#define SHDR_O(m)	offsetof(struct prj_specific, m)
#define SHDR_AO(m)	offsetofarray(struct prj_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse prj_print_tab[] = {
	{"%S",  1, "cfile",		SHDR_O(prj_cfile),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse prj_parse_tab[] = {
	{"i",	bu_byteoffset(prj_print_tab[0]), "prj_print_tab", 0, FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};


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
	{"prj",	0,	0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	prj_setup,	prj_render,	prj_print,	prj_free },

	{(char *)0,	0,		0,		0,		0,
	0,		0,		0,		0 }
};

/* Once user has specified all parameters, compute some useful 
 * traits of the "slide"
 */
static void
finish_slide_setup(slide)
register struct slide *slide;
{
	plane_t pl;

	slide->sl_xdist = MAGNITUDE(slide->sl_x);
	VUNITIZE(slide->sl_x);

	slide->sl_ydist = MAGNITUDE(slide->sl_y);
	VUNITIZE(slide->sl_y);

	VCROSS(slide->sl_plane, slide->sl_x, slide->sl_y);
	slide->sl_plane[W] = 0;
	slide->sl_plane[W] = DIST_PT_PLANE(slide->sl_pt, slide->sl_plane);
}

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
	char	*cfile;
	char	*dfile;
	FILE	*cfd;
	struct bu_vls	line;
	struct slide	*slide;
	int	size, n;
	point_t	xpt, ypt;


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
	memcpy(prj_sp, &prj_defaults, sizeof(struct prj_specific) );

	bu_vls_init(&prj_sp->prj_cfile);
	BU_LIST_INIT(&prj_sp->prj_slides);

	/* Unlike most shaders, prj only gets the name of a file from the
	 * database.  The real parameters for the shader are stored in the
	 * file.  So we have to open the file and parse the contents.
	 * Unlike most shaders, we can't very well provide defaults.
	 */
	if( bu_struct_parse( matparm, prj_parse_tab, (char *)prj_sp ) < 0 ||
	  ! bu_vls_strlen(&prj_sp->prj_cfile) ) {
		bu_free(prj_sp, "prj_specific");
		return -1;
	}

	cfile = bu_vls_addr(&prj_sp->prj_cfile);
	if ((cfd = fopen(cfile, "r")) == (FILE *)NULL) {
		rt_log("%s %s\n", strerror(errno), cfile);
		bu_free(prj_sp, "prj_specific");
		return -1;
	}


	GETSTRUCT(slide, slide);
	BU_LIST_APPEND(&prj_sp->prj_slides, &(slide->l) );

	/* get each line from the control file, and fill in the slide
	 * descriptions
	 */
	for (bu_vls_init(&line) ; bu_vls_gets(&line, cfd) ; bu_vls_trunc2(&line, 0) ) {
		register char *p = bu_vls_addr(&line);

		if (*p && *p != '#') continue;

		if (! strncmp(p, "file=", 5)) {
			/* starting a new "slide"
			 *
			 * Before we forget the old slide, compute some
			 * important values: Unitize vectors, compute plane
			 */
			finish_slide_setup(slide);

			/* get the new slide and put it in place */
			GETSTRUCT(slide, slide);
			BU_LIST_APPEND(&prj_sp->prj_slides, &(slide->l) );
		}

		bu_struct_parse(&line, prj_file_parse_tab,
			(char *)BU_LIST_FIRST(slide, &prj_sp->prj_slides) );
	}

	finish_slide_setup(slide);

	fclose(cfd);

	/* load the images */
	for (BU_LIST_FOR(slide, slide, &prj_sp->prj_slides)) {

		/* open the image file */
		dfile = bu_vls_addr(&slide->sl_file);
		if ((cfd=fopen( dfile, "r")) == (FILE*)NULL) {
			struct slide *badslide = slide;

			rt_log("%s %s\n", strerror(errno), dfile);

			/* open failed, remove this slide */
			slide = BU_LIST_PREV(slide, &badslide->l);
			BU_LIST_DEQUEUE( &badslide->l );
			bu_free(badslide, "slide");
			continue;
		}

		/* malloc storage for the image data */
		size = slide->sl_dim[0] * slide->sl_dim[1]; 
		slide->sl_img = bu_malloc(size*slide->sl_dim[2], "image data");

		/* read in the data */
		n = fread(slide->sl_img, slide->sl_dim[2], size, cfd);
		if (n != size) {
			struct slide *badslide = slide;

			rt_log("read error %s: %d != %d\n", slide->sl_file,
				n, size);

			/* read failed, remove this slide */
			slide = BU_LIST_PREV(slide, &badslide->l);
			BU_LIST_DEQUEUE( &badslide->l );
			bu_free(badslide, "slide");
		}
		(void)fclose(cfd);
	}

	/*
	 * If the shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 *
	 * Shading is done in "region coordinates"
	 */
	db_region_mat(prj_sp->prj_m_to_r, rtip->rti_dbip, rp->reg_name);


	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", prj_print_tab, (char *)prj_sp );
		mat_print( "m_to_r", prj_sp->prj_m_to_r );
	}

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
	struct slide *slide;
	struct prj_specific *prj_sp = (struct prj_specific *)dp;

	bu_struct_print( rp->reg_name, prj_print_tab, (char *)dp );

	for (BU_LIST_FOR(slide, slide, &prj_sp->prj_slides )) {
		bu_struct_print("", prj_file_parse_tab, (char *)slide);
	}
}

/*
 *	P R J _ F R E E
 */
HIDDEN void
prj_free( cp )
char *cp;
{
	struct slide *slide;
	struct prj_specific *prj_sp = (struct prj_specific *)cp;

	while (BU_LIST_WHILE(slide, slide, &prj_sp->prj_slides)) {
		BU_LIST_DEQUEUE( &(slide->l) );
		bu_free((char *)slide, "slide");
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
	struct slide *prj_slide = (struct slide *)NULL;
	point_t pt;
	vect_t	N;


	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_prj_SP(prj_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "prj_render Parameters:", prj_print_tab, (char *)prj_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in prj_setup().
	 */
	MAT4X3PNT(pt, prj_sp->prj_m_to_r, swp->sw_hit.hit_point);
	MAT4X3VEC(N, prj_sp->prj_m_to_r, swp->sw_hit.hit_normal);

	if( rdebug&RDEBUG_SHADE) {
		rt_log("prj_render()  model:(%g %g %g) shader:(%g %g %g)\n", 
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}


	switch (prj_sp->prj_method) {
	default:
	case	1:
		{
			struct slide *slide;
			double max_cos_angle = -2.0;
			double cos_angle;
			fastf_t dist;
			int error;

			for (BU_LIST_FOR(slide, slide, &prj_sp->prj_slides)) {
				cos_angle = VDOT(N, slide->sl_plane);
				if (cos_angle > max_cos_angle) {
					max_cos_angle = cos_angle;
					prj_slide = slide;
				}
			}
			if (! prj_slide ) {
				/* What?  No slides? */
				VSETALL(swp->sw_color, 1.0);
				goto reflect;
			}

			error = bn_isect_line3_plane(&dist, pt, N,
				slide->sl_plane, &ap->a_rt_i->rti_tol);

			if (error) {
				VSETALL(swp->sw_color, 1.0);
				goto reflect;
			}

			/* compute point in image plane */
			VJOIN1(pt, pt, dist, N);

			/* */

		}
	}

	


reflect:
	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if( swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
}
