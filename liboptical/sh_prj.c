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

#define prj_MAGIC 0x1998    /* make this a unique number for each shader */
#define CK_prj_SP(_p) RT_CKMAG(_p, prj_MAGIC, "prj_specific")

#define NAME_LEN	128
#define IMAGE_MAGIC 0x1a
struct prj_image {
	struct bu_list	l;
	unsigned 	i_width;
	unsigned 	i_height;
	unsigned 	i_depth;
	struct bu_mapped_file	*i_mp;

	double		i_persp;
	point_t		i_eye;
	quat_t		i_quat;
	double		i_viewsize;

	plane_t		i_plane;	/* computed */

	mat_t		i_mat;	/* view rot/scale matrix */
};


/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct prj_specific {
	long		magic;	/* for memory validity check, must come 1st */
	char		prj_name[NAME_LEN];/* temp variable for filenames */
	mat_t		prj_m_to_sh;	/* model to shader space matrix */
	unsigned	prj_count;
	struct prj_image prj_images;	/* list of images to map on object */
};



#define SHDR_NULL	((struct prj_specific *)0)
#define SHDR_O(m)	offsetof(struct prj_specific, m)
#define SHDR_AO(m)	offsetofarray(struct prj_specific, m)

static void
image_dup(prj_sp)
struct prj_specific *prj_sp;
{
	struct prj_image *i;
	mat_t xlate, rotscale;
	quat_t newquat;

	/* dup off to new list element */
	GETSTRUCT( i, prj_image );

	memmove( i, &prj_sp->prj_images, sizeof(*i));

	BU_LIST_APPEND( &(prj_sp->prj_images.l), &(i->l) );
	BU_LIST_MAGIC_SET( &(i->l), IMAGE_MAGIC );

	/* According to rt/do.c this reconstructs the matrix from the
	 * three parameters "eye_pt" "orientation" and "viewsize"
	 */
	quat_quat2mat( rotscale, i->i_quat);
	rotscale[15] = 0.5 * i->i_viewsize;
	mat_idn( xlate );
	MAT_DELTAS_VEC_NEG(xlate, i->i_eye);
	mat_mul(i->i_mat, rotscale, xlate);
}

static void
image_hook(sdp, name, base, value)
struct bu_structparse			*sdp;	/* structure description */
register CONST char			*name;	/* struct member name */
char					*base;	/* begining of structure */
CONST char				*value;	/* string containing value */
{
	struct prj_specific *prj_sp = (struct prj_specific *)sdp;

	/* we just parsed a new filename.  If the prj_images struct has
	 * any values set, that means we have finished parsing the previous
	 * image's specification.  It is thus time to copy it off and zero the
	 * parameters for the next image
	 */

	if (prj_sp->prj_count)
		image_dup(prj_sp);

	/* initialize new image */
	prj_sp->prj_images.i_mp = bu_open_mapped_file( prj_sp->prj_name, NULL );
	prj_sp->prj_count++;

	prj_sp->prj_images.i_width = prj_sp->prj_images.i_height = 512;
	prj_sp->prj_images.i_depth = 3;
}
void
dimen_hook(sdp, name, base, value)
register CONST struct bu_structparse	*sdp;	/* structure description */
register CONST char			*name;	/* struct member name */
char					*base;	/* begining of structure */
CONST char				*value;	/* string containing value */
{
	struct prj_specific *prj_sp = (struct prj_specific *)sdp;

	prj_sp->prj_images.i_height = prj_sp->prj_images.i_width;
}

void
plane_cond(sdp, name, base, value)
register CONST struct bu_structparse	*sdp;	/* structure description */
register CONST char			*name;	/* struct member name */
char					*base;	/* begining of structure */
CONST char				*value;	/* string containing value */
{
}

/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse prj_print_tab[] = {
	{"%s", NAME_LEN, "file",SHDR_AO(prj_name),		image_hook },
	{"%d",  1, "depth",	SHDR_O(prj_images.i_depth),	FUNC_NULL },
	{"%d",  1, "width",	SHDR_O(prj_images.i_width),	FUNC_NULL },
	{"%d",  1, "numscan",	SHDR_O(prj_images.i_height),	FUNC_NULL },
	{"%d",  1, "squaresize",SHDR_O(prj_images.i_width),	dimen_hook },

	{"",	0, (char *)0,	0,				FUNC_NULL }
};

struct bu_structparse prj_parse_tab[] = {
	{"i",bu_byteoffset(prj_print_tab[0]), "prj_print_tab", 0, FUNC_NULL },
	{"%s",	NAME_LEN, "f",	SHDR_AO(prj_name),		image_hook },
	{"%d",  1, "d",		SHDR_O(prj_images.i_depth),	FUNC_NULL },
	{"%d",  1, "w",		SHDR_O(prj_images.i_width),	FUNC_NULL },
	{"%d",  1, "n",		SHDR_O(prj_images.i_height),	FUNC_NULL },
	{"%d",  1, "s",		SHDR_O(prj_images.i_width),	dimen_hook },

	{"%f",  1, "p",		SHDR_O(prj_images.i_persp),	plane_cond },
	{"%f",  1, "v",		SHDR_O(prj_images.i_viewsize),	plane_cond },
	{"%f",  4, "q",		SHDR_AO(prj_images.i_quat),	plane_cond },
	{"%f",  3, "e",		SHDR_AO(prj_images.i_eye),	plane_cond },
	{"",	0, (char *)0,	0,				FUNC_NULL }
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
CONST struct mfuncs prj_mfuncs[] = {
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
	bn_mat_idn(prj_sp->prj_m_to_sh);
	BU_LIST_INIT(&prj_sp->prj_images.l);
	*prj_sp->prj_name = '\0';
	prj_sp->prj_count = 0;
	memset(&prj_sp->prj_images, 0, sizeof(prj_sp->prj_images));

	/* The shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 */
	db_region_mat(prj_sp->prj_m_to_sh, rtip->rti_dbip, rp->reg_name);

	/* parse the user's arguments for this use of the shader. */
	if( bu_struct_parse( matparm, prj_parse_tab, (char *)prj_sp ) < 0 )
		return(-1);

	if (prj_sp->prj_count) {
		image_dup(prj_sp);
	} else {
		bu_bomb("No images specified for prj shader\n");
	}


	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", prj_print_tab, (char *)prj_sp );
		mat_print( "m_to_sh", prj_sp->prj_m_to_sh );
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
	bu_struct_print( rp->reg_name, prj_print_tab, (char *)dp );
}

/*
 *	P R J _ F R E E
 */
HIDDEN void
prj_free( cp )
char *cp;
{
	struct prj_specific *prj_sp = (struct prj_specific *)cp;
	struct prj_image *i;

	while (BU_LIST_WHILE( i, prj_image, &(prj_sp->prj_images.l) )) {
		BU_LIST_DEQUEUE( &(i->l) );
		bu_free( (char *)i, "prj_image" );
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
		bu_struct_print( "prj_render Parameters:", prj_print_tab, (char *)prj_sp );

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
	for (BU_LIST_FOR(i, prj_image, &prj_sp->prj_images.l)) {
		double dist;
		vect_t dir;
		point_t pl_pt;

		if (VDOT(i->i_plane, N) >= 0.0) continue;

		/* convert hit point into view coordinates of image */
		MAT4X3PNT(pl_pt, i->i_mat, pt);


		/* compute plane point */
		if (i->i_persp) {
			VSUB2(dir, pt, i->i_eye);
		} else {
			VREVERSE(dir, pl);
		}

		switch (bn_isect_line3_plane(&dist, pt, dir, i->i_plane, &ap->a_rt_i->rti_tol)) {
		case 0: /* line lies on plane */
			break;
		case 1: /* hit entering */
			break;
		default:
			continue;
			break;
		}

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
