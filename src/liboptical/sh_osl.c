
/*                        S H _ X X X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file liboptical/sh_xxx.c
 *
 * To add a new shader to the "rt" program's LIBOPTICAL library:
 *
 * 1) Copy this file to sh_shadername.c
 *
 * 2) edit sh_shadername.c:
 *	change "X X X" to "S H A D E R N A M E"
 *	change "xxx" to "shadername"
 *	Set a new number for the xxx_MAGIC define
 *	define shader specific structure and defaults
 *	edit/build parse table for bu_structparse from xxx_parse
 *	edit/build shader_mfuncs tables from xxx_mfuncs for
 *		each shader name being built.
 *	edit the xxx_setup function to do shader-specific setup
 *	edit the xxx_render function to do the actual rendering
 *
 * If you are building a dynamically loaded shader, compile this into a
 * shared library called "shadername.so".  If you have a number of shaders
 * for you are adding, you can create a single library called "shaders.so"
 * which contains all of your DSO shaders.
 *
 * RT will look in the following locations for DSO shaders:
 *		./
 *		$prefix/lib/
 *		$LD_LIBRARY_PATH
 *
 * If you are adding the shader to "rt" as a permanent shader, then the
 * following steps are necessary:
 *
 * 3) Edit init.c to add extern for osl_mfuncs and a call to
 * mlib_add_shader().
 *
 * 4) Edit Makefile.am to add shader file to the compilation
 *
 * 5) replace this list with a description of the shader, its
 * parameters and use.
 *
 * 6) Edit shaders.tcl and comb.tcl in the ../tclscripts/mged
 * directory to add a new gui for this shader.
 */

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "./osl-renderer.h"

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


#define OSL_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_OSL_SP(_p) BU_CKMAG(_p, OSL_MAGIC, "osl_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct osl_specific {
    long magic;              	 /* magic # for memory validity check */
    OSLRenderer* oslr;           /* reference to osl ray tracer */
};


/* The default values for the variables in the shader specific structure */
static const
struct osl_specific osl_defaults = {
    OSL_MAGIC,
    NULL
};


#define SHDR_NULL ((struct osl_specific *)0)
#define SHDR_O(m) bu_offsetof(struct osl_specific, m)
#define SHDR_AO(m) bu_offsetofarray(struct osl_specific, m)

/* description of how to parse/print the arguments to the shader */
struct bu_structparse osl_print_tab[] = {
    {"",	0, (char *)0,		0,		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};
struct bu_structparse osl_parse_tab[] = {
    {"%p",	bu_byteoffset(osl_print_tab[0]), "osl_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int osl_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int osl_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void osl_print(register struct region *rp, genptr_t dp);
HIDDEN void osl_free(genptr_t cp);

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs osl_mfuncs[] = {
    {MF_MAGIC,	"osl",	0,	MFI_NORMAL|MFI_HIT|MFI_UV,	0,     osl_setup,	osl_render,	osl_print,	osl_free },
    {0,		(char *)0,	0,		0,		0,     0,		0,		0,		0 }
};

/* O S L _ S E T U P
 *
 * This routine is called (at prep time)
 * once for each region which uses this shader.
 * Any shader-specific initialization should be done here.
 *
 * Returns:
 * 1 success
 * 0 success, but delete region
 * -1 failure
 */
HIDDEN int
osl_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)


/* pointer to reg_udata in *rp */

/* New since 4.4 release */
{
    register struct osl_specific *osl_sp;

    /* check the arguments */
    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);

    if (rdebug&RDEBUG_SHADE)
	bu_log("osl_setup(%s)\n", rp->reg_name);

    /* Get memory for the shader parameters and shader-specific data */
    BU_GETSTRUCT(osl_sp, osl_specific);
    *dpp = osl_sp;

    /* -----------------------------------
     * Initialize the osl specific values
     * -----------------------------------
     */
    memcpy(osl_sp, &osl_defaults, sizeof(struct osl_specific));

    /* parse the user's arguments for this use of the shader. */
    if (bu_struct_parse(matparm, osl_parse_tab, (char *)osl_sp) < 0)
	return -1;
  
    if (rdebug&RDEBUG_SHADE) {
	bu_struct_print(" Parameters:", osl_print_tab, (char *)osl_sp);
    }

    return 1;
}

/*
 * O S L _ P R I N T
 */
HIDDEN void
osl_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, osl_print_tab, (char *)dp);
}


/*
 * O S L _ F R E E
 */
HIDDEN void
osl_free(genptr_t cp)
{
    register struct osl_specific *osl_sp =
	(struct osl_specific *)cp;
    oslrenderer_free(&(osl_sp->oslr));
    bu_free(cp, "osl_specific");
}


/*
 * O S L _ R E N D E R
 *
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.
 */
int
osl_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp)


/* defined in ../h/shadework.h */
/* ptr to the shader-specific struct */
{
    register struct osl_specific *osl_sp =
	(struct osl_specific *)dp;
    point_t pt;
    RenderInfo info;
    static int first = 1;
    static OSLRenderer *oslr = NULL;

    if(first == 1){
	oslr = oslrenderer_init("yellow");
    }
    first = 0;

    VSETALL(pt, 0);

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_OSL_SP(osl_sp);

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print("osl_render Parameters:", osl_print_tab, (char *)osl_sp);
    /* OSL perform shading operations here */

    /* Fill in all necessary information for the OSL renderer */
    VMOVE(info.P, swp->sw_hit.hit_point);
    printf("[DEB] %.2lf %.2lf %.2lf\n", info.P[0], info.P[1], info.P[2]);
    VMOVE(info.N, swp->sw_hit.hit_normal);
    printf("[DEB] N %.2lf %.2lf %.2lf\n", info.N[0], info.N[1], info.N[2]);
    /*VMOVE(info.I, ap->a_inv_dir);*/
    VMOVE(info.I, ap->a_inv_dir);
    /*printf("[DEB] I %.8lf %.8lf %.8lf\n", info.I[0], info.I[1], info.I[2]);*/
    info.u = swp->sw_uv.uv_u;
    info.v = swp->sw_uv.uv_v;
    info.screen_x = ap->a_x;
    info.screen_y = ap->a_y;

    /* FIXME */
    VSETALL(info.dPdu, 0.0f);
    VSETALL(info.dPdv, 0.0f);
    info.depth = ap->a_level;
    info.isbackfacing = 0;
    info.surfacearea = 1.0f;

    /* a priori we won't do reflection. If OSLRender decides to do
     so, it will set it to 1 */
    info.doreflection = 0; 

    oslrenderer_query_color(oslr, &info);
    VMOVE(swp->sw_color, info.pc);    

    /* if(info.doreflection == 1){ */
    /* 	ap->a_onehit = 0; */
    /* 	ap->a_level++; */
    /* 	swp->sw_reflect = 1; */
    /* } */


    /* printf("parameter a_onehit: %d\n", ap->a_onehit); */

    /* shader must perform transmission/reflection calculations
     *
     * 0 < swp->sw_transmit <= 1 causes transmission computations
     * 0 < swp->sw_reflect <= 1 causes reflection computations
     */
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
