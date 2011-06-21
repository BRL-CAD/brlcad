/*                       S H _ F I R E . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file liboptical/sh_fire.c
 *
 * Fire shader
 *
 * Parameters:
 *
 * flicker=rate Specifies rate of translation through noise space for
 * animation.  swp->frametime * flicker gives a delta in Z of noise
 * space for animation.  Useful values probably in range 0 > flicker >
 * 10
 *
 * stretch=dist Specifies a scaling of the exponential stretch of the
 * flames.  flame stretch = e^(pos[Z] * -stretch)
 *
 *
 * Standard fbm parameters:
 *
 * lacunarity Scale between different levels of noise detail
 *
 * octaves Number of different levels of noise to add to get structure
 * of the flames.
 *
 * h_val power for frequency (usually 1)
 *
 * scale 3-tuple which scales noise WRT shader space: "how big is the
 * largest noise frequency on object"
 *
 * delta 3-tuple specifying origin delta in noise space: "what piece
 * of noise space maps to shader origin"
 *
 * Usage:
 * mged> shader flame.r {fire {st 1.25}}
 *
 * Note: The fire shader provides its own color.  It does not read any
 * color information from the region definition.
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


#define fire_MAGIC 0x46697265   /* ``Fire'' */
#define CK_fire_SP(_p) BU_CKMAG(_p, fire_MAGIC, "fire_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct fire_specific {
    long magic;	/* magic # for memory validity check, must come 1st */
    int fire_debug;
    double fire_flicker;		/* flicker rate */
    double fire_stretch;
    double noise_lacunarity;
    double noise_h_val;
    double noise_octaves;
    double noise_size;
    vect_t noise_vscale;
    vect_t noise_delta;
    /* the following values are computed */
    point_t fire_min;
    point_t fire_max;
    mat_t fire_m_to_sh;		/* model to shader space matrix */
    mat_t fire_sh_to_noise;	/* shader to noise space matrix */
    mat_t fire_colorspline_mat;
};


/* The default values for the variables in the shader specific structure */
static const
struct fire_specific fire_defaults = {
    fire_MAGIC,
    0,			/* fire_debug */
    1.0,		/* fire flicker rate */
    0.0,		/* fire_stretch */
    2.1753974,		/* noise_lacunarity */
    1.0,		/* noise_h_val */
    2.0,		/* noise_octaves */
    -1.0,		/* noise_size */
    VINITALL(10.0),	/* noise_vscale */
    VINIT_ZERO,		/* noise_delta */
    VINIT_ZERO,		/* fire_min */
    VINIT_ZERO,		/* fire_max */
    MAT_INIT_IDN,	/* fire_m_to_sh */
    MAT_INIT_IDN,	/* fire_sh_to_noise */
    MAT_INIT_ZERO	/* fire_colorspline_mat */
};


#define SHDR_NULL ((struct fire_specific *)0)
#define SHDR_O(m) bu_offsetof(struct fire_specific, m)
#define SHDR_AO(m) bu_offsetofarray(struct fire_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse fire_print_tab[] = {
    {"%d", 1, "debug",		SHDR_O(fire_debug),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "flicker",	SHDR_O(fire_flicker),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "stretch",	SHDR_O(fire_stretch),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "lacunarity",	SHDR_O(noise_lacunarity),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "H", 		SHDR_O(noise_h_val),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "octaves", 	SHDR_O(noise_octaves),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "scale",		SHDR_O(noise_size),		bu_mm_cvt, NULL, NULL },
    {"%f", 3, "vscale",		SHDR_AO(noise_vscale),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "delta",		SHDR_AO(noise_delta),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3,  "max",		SHDR_AO(fire_max),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3,  "min",		SHDR_AO(fire_min),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0,	0,				BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }

};
struct bu_structparse fire_parse_tab[] = {
    {"%p", bu_byteoffset(fire_print_tab[0]), "fire_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "f",	SHDR_O(fire_flicker),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "st",	SHDR_O(fire_stretch),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "l",	SHDR_O(noise_lacunarity),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "H", 	SHDR_O(noise_h_val),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "o", 	SHDR_O(noise_octaves),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "s",	SHDR_O(noise_size),		bu_mm_cvt, NULL, NULL },
    {"%f", 3, "v",	SHDR_AO(noise_vscale),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "vs",	SHDR_AO(noise_vscale),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "d",	SHDR_AO(noise_delta),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int fire_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int fire_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void fire_print(register struct region *rp, genptr_t dp);
HIDDEN void fire_free(genptr_t cp);

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs fire_mfuncs[] = {
    {MF_MAGIC,	"fire",		0,		MFI_HIT,	0,
     fire_setup,	fire_render,	fire_print,	fire_free },

    {0,		(char *)0,	0,		0,		0,
     0,		0,		0,		0 }
};


const double flame_colors[18][3] = {
    {0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0},
    {0.106, 0.0, 0.0},
    {0.212, 0.0, 0.0},
    {0.318, 0.0, 0.0},
    {0.427, 0.0, 0.0},
    {0.533, 0.0, 0.0},
    {0.651, 0.02, 0.0},
    {0.741, 0.118, 0.0},
    {0.827, 0.235, 0.0},
    {0.906, 0.353, 0.0},
    {0.933, 0.500, 0.0},
    {0.957, 0.635, 0.047},
    {0.973, 0.733, 0.227},
    {0.984, 0.820, 0.451},
    {0.990, 0.925, 0.824},
    {1.0, 0.945, 0.902},
    {1.0, 0.945, 0.902}
};


/* F I R E _ S E T U P
 *
 * This routine is called (at prep time)
 * once for each region which uses this shader.
 * Any shader-specific initialization should be done here.
 */
HIDDEN int
fire_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)


/* pointer to reg_udata in *rp */

/* New since 4.4 release */
{
    register struct fire_specific *fire_sp;

    /* check the arguments */
    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);


    if (rdebug&RDEBUG_SHADE)
	bu_log("fire_setup(%s)\n", rp->reg_name);

    /* Get memory for the shader parameters and shader-specific data */
    BU_GETSTRUCT(fire_sp, fire_specific);
    *dpp = fire_sp;

    /* initialize the default values for the shader */
    memcpy(fire_sp, &fire_defaults, sizeof(struct fire_specific));

    /* parse the user's arguments for this use of the shader. */
    if (bu_struct_parse(matparm, fire_parse_tab, (char *)fire_sp) < 0)
	return -1;

    if (!EQUAL(fire_sp->noise_size, -1.0)) {
	VSETALL(fire_sp->noise_vscale, fire_sp->noise_size);
    }

    /*
     * The shader needs to operate in a coordinate system which stays
     * fixed on the region when the region is moved (as in animation).
     * We need to get a matrix to perform the appropriate transform(s).
     */

    rt_shader_mat(fire_sp->fire_m_to_sh, rtip, rp, fire_sp->fire_min,
		  fire_sp->fire_max, &rt_uniresource);

    /* Build matrix to map shader space to noise space.
     * XXX If only we could get the frametime at this point
     * we could factor the flicker of flames into this matrix
     * rather than having to recompute it on a pixel-by-pixel basis.
     */
    MAT_IDN(fire_sp->fire_sh_to_noise);
    MAT_DELTAS_VEC(fire_sp->fire_sh_to_noise, fire_sp->noise_delta);
    MAT_SCALE_VEC(fire_sp->fire_sh_to_noise, fire_sp->noise_vscale);

    /* get matrix for performing spline of fire colors */
    rt_dspline_matrix(fire_sp->fire_colorspline_mat, "Catmull", 0.5, 0.0);


    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug) {
	bu_struct_print(" FIRE Parameters:", fire_print_tab, (char *)fire_sp);
	bn_mat_print("m_to_sh", fire_sp->fire_m_to_sh);
	bn_mat_print("sh_to_noise", fire_sp->fire_sh_to_noise);
	bn_mat_print("colorspline", fire_sp->fire_colorspline_mat);
    }

    return 1;
}


/*
 * F I R E _ P R I N T
 */
HIDDEN void
fire_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, fire_print_tab, (char *)dp);
}


/*
 * F I R E _ F R E E
 */
HIDDEN void
fire_free(genptr_t cp)
{
    bu_free(cp, "fire_specific");
}


/*
 * F I R E _ R E N D E R
 *
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.
 */
int
fire_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp)


/* defined in material.h */
/* ptr to the shader-specific struct */
{
#define DEBUG_SPACE_PRINT(str, i_pt, o_pt)			\
    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug) {		\
	bu_log("fire_render() %s space \n", str);		\
	bu_log("fire_render() i_pt(%g %g %g)\n", V3ARGS(i_pt)); \
	bu_log("fire_render() o_pt(%g %g %g)\n", V3ARGS(o_pt)); \
    }

#define SHADER_TO_NOISE(n_pt, sh_pt, fire_sp, zdelta) {			\
	point_t tmp_pt;							\
	tmp_pt[X] = sh_pt[X];						\
	tmp_pt[Y] = sh_pt[Y];						\
	if (! NEAR_ZERO(fire_sp->fire_stretch, SQRT_SMALL_FASTF))	\
	    tmp_pt[Z] = exp((sh_pt[Z]+0.125) * -fire_sp->fire_stretch); \
	else								\
	    tmp_pt[Z] = sh_pt[Z];					\
	MAT4X3PNT(n_pt, fire_sp->fire_sh_to_noise, tmp_pt);		\
	n_pt[Z] += zdelta;						\
    }


    register struct fire_specific *fire_sp =
	(struct fire_specific *)dp;
    point_t m_i_pt, m_o_pt;	/* model space in/out points */
    point_t sh_i_pt, sh_o_pt;	/* shader space in/out points */
    point_t noise_i_pt, noise_o_pt;	/* shader space in/out points */
    point_t noise_pt;
    point_t color;
    vect_t noise_r_dir;
    double noise_r_thick;
    int i;
    double samples_per_unit_noise;
    double noise_dist_per_sample;
    point_t shader_pt;
    vect_t shader_r_dir;
    double shader_r_thick;
    double shader_dist_per_sample;
    double noise_zdelta;

    int samples;
    double dist;
    double noise_val;
    double lumens;

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_fire_SP(fire_sp);

    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug) {
/* bu_struct_print("fire_render Parameters:", fire_print_tab, (char *)fire_sp); */
	bu_log("fire_render()\n");
    }
    /* If we are performing the shading in "region" space, we must
     * transform the hit point from "model" space to "region" space.
     * See the call to db_region_mat in fire_setup().
     */

    /*
     * Compute the ray/solid in and out points,
     */
    VMOVE(m_i_pt, swp->sw_hit.hit_point);
    VJOIN1(m_o_pt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
    DEBUG_SPACE_PRINT("model", m_i_pt, m_o_pt);

    /* map points into shader space */
    MAT4X3PNT(sh_i_pt, fire_sp->fire_m_to_sh, m_i_pt);
    MAT4X3PNT(sh_o_pt, fire_sp->fire_m_to_sh, m_o_pt);
    DEBUG_SPACE_PRINT("shader", sh_i_pt, sh_o_pt);

    noise_zdelta = fire_sp->fire_flicker * swp->sw_frametime;

    SHADER_TO_NOISE(noise_i_pt, sh_i_pt, fire_sp, noise_zdelta);
    SHADER_TO_NOISE(noise_o_pt, sh_o_pt, fire_sp, noise_zdelta);

    VSUB2(noise_r_dir, noise_o_pt, noise_i_pt);

    noise_r_thick = MAGNITUDE(noise_r_dir);

    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug) {
	bu_log("fire_render() noise_r_dir (%g %g %g)\n",
	       V3ARGS(noise_r_dir));
	bu_log("fire_render() noise_r_thick %g\n", noise_r_thick);
    }


    /* compute number of samples per unit length in noise space.
     *
     * The noise field used by the bn_noise_turb and bn_noise_fbm routines
     * has a maximum frequency of about 1 cycle per integer step in
     * noise space.  Each octave increases this frequency by the
     * "lacunarity" factor.  To sample this space adequately, nyquist
     * tells us we need at least 4 samples/cycle at the highest octave
     * rate.
     */

    samples_per_unit_noise =
	pow(fire_sp->noise_lacunarity, fire_sp->noise_octaves-1) * 4.0;

    noise_dist_per_sample = 1.0 / samples_per_unit_noise;

    samples = samples_per_unit_noise * noise_r_thick;

    if (samples < 1) samples = 1;

    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug) {
	bu_log("samples:%d\n", samples);
	bu_log("samples_per_unit_noise %g\n", samples_per_unit_noise);
	bu_log("noise_dist_per_sample %g\n", noise_dist_per_sample);
    }

    /* To do the exponential stretch and decay properly we need to
     * do the computations in shader space, and convert the points
     * to noise space.  Performance pig.
     */

    VSUB2(shader_r_dir, sh_o_pt, sh_i_pt);
    shader_r_thick = MAGNITUDE(shader_r_dir);
    VUNITIZE(shader_r_dir);

    shader_dist_per_sample = shader_r_thick / samples;

    lumens = 0.0;
    for (i = 0; i < samples; i++) {
	dist = (double)i * shader_dist_per_sample;
	VJOIN1(shader_pt, sh_i_pt, dist, shader_r_dir);

	SHADER_TO_NOISE(noise_pt, shader_pt, fire_sp, noise_zdelta);

	noise_val = bn_noise_turb(noise_pt, fire_sp->noise_h_val,
				  fire_sp->noise_lacunarity, fire_sp->noise_octaves);

	if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug)
	    bu_log("bn_noise_turb(%g %g %g) = %g\n",
		   V3ARGS(noise_pt),
		   noise_val);

	/* XXX
	 * When doing the exponential stretch, we scale the noise
	 * value by the height in shader space
	 */

	if (NEAR_ZERO(fire_sp->fire_stretch, SQRT_SMALL_FASTF))
	    lumens += noise_val * 0.025;
	else {
	    register double t;
	    t = lumens;
	    lumens += noise_val * 0.025 * (1.0 -shader_pt[Z]);
	    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug)
		bu_log("lumens:%g = %g + %g * %g\n",
		       lumens, t, noise_val,
		       0.025 * (1.0 - shader_pt[Z]));

	}
	if (lumens >= 1.0) {
	    lumens = 1.0;
	    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug)
		bu_log("early exit from lumens loop\n");
	    break;
	}

    }

    if (rdebug&RDEBUG_SHADE || fire_sp->fire_debug)
	bu_log("lumens = %g\n", lumens);

    if (lumens < 0.0) lumens = 0.0;
    else if (lumens > 1.0) lumens = 1.0;


    rt_dspline_n(color, fire_sp->fire_colorspline_mat, (double *)flame_colors,
		 18, 3, lumens);

    VMOVE(swp->sw_color, color);
/* VSETALL(swp->sw_basecolor, 1.0);*/

    swp->sw_transmit = 1.0 - (lumens * 4.);
    if (swp->sw_reflect > 0 || swp->sw_transmit > 0)
	(void)rr_render(ap, pp, swp);

    return 1;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
