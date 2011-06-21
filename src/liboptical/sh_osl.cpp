
/*                        S H _ O S L . C
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
/** @file liboptical/sh_osl.c
 *
 * This shader is an interface for OSL shadying system. More information
 * when more features are implemented.
 *
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

#ifdef __cplusplus

/* Oslrenderer system */
OSLRenderer *oslr = NULL;
#endif


/*
 * The shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct osl_specific {
    long magic;              	 /* magic # for memory validity check */
    /*OSLRenderer* oslr;*/           /* reference to osl ray tracer */
};

/* The default values for the variables in the shader specific structure */
static const
struct osl_specific osl_defaults = {
    OSL_MAGIC,
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

#ifdef __cplusplus
extern "C" {
#endif

    HIDDEN int osl_setup(register struct region *rp, struct bu_vls *matparm, 
			 genptr_t *dpp, const struct mfuncs *mfp, 
			 struct rt_i *rtip);

    HIDDEN int osl_render(struct application *ap, const struct partition *pp, 
			  struct shadework *swp, genptr_t dp);
    HIDDEN void osl_print(register struct region *rp, genptr_t dp);
    HIDDEN void osl_free(genptr_t cp);

#ifdef __cplusplus
}
#endif

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

/* 
 * The remaining code should be hidden from C callers
 * 
 */

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

#ifdef __cplusplus
extern "C" {
#endif

HIDDEN int osl_setup(register struct region *rp, struct bu_vls *matparm, 
		     genptr_t *dpp, const struct mfuncs *mfp, 
		     struct rt_i *rtip)
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
    *dpp = (char *)osl_sp;

    /* -----------------------------------
     * Initialize the osl specific values
     * -----------------------------------
     */
    memcpy(osl_sp, &osl_defaults, sizeof(struct osl_specific));

    /* parse the user's arguments for this use of the shader. */
    if (bu_struct_parse(matparm, osl_parse_tab, (char *)osl_sp) < 0)
	return -1;

    /* -----------------------------------
     * Initialize osl render system
     * -----------------------------------
     */
    /* If OSL system was not initialized yet, do it */
    /* FIXME: take care of multi-thread issues */
    if (oslr == NULL){
	oslr = new OSLRenderer();
    }
    /* Add this shader to OSL system */
    oslr->AddShader("yellow");
  
    if (rdebug&RDEBUG_SHADE) {
	bu_struct_print(" Parameters:", osl_print_tab, (char *)osl_sp);
    }

    return 1;
}

/*
 * O S L _ P R I N T
 */
HIDDEN void osl_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, osl_print_tab, (char *)dp);
}


/*
 * O S L _ F R E E
 */
HIDDEN void osl_free(genptr_t cp)
{
    register struct osl_specific *osl_sp =
	(struct osl_specific *)cp;
    bu_free(cp, "osl_specific");

    /* FIXME: take care of multi-thread issues */
    if(oslr != NULL){
	delete oslr;
	oslr = NULL;
    }
}


/*
 * O S L _ R E N D E R
 *
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.
 */
HIDDEN int osl_render(struct application *ap, const struct partition *pp, 
		      struct shadework *swp, genptr_t dp)
/* defined in ../h/shadework.h */
/* ptr to the shader-specific struct */
{
    register struct osl_specific *osl_sp =
	(struct osl_specific *)dp;
    point_t pt;
  
    VSETALL(pt, 0);

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_OSL_SP(osl_sp);

    if (rdebug&RDEBUG_SHADE)
	bu_struct_print("osl_render Parameters:", osl_print_tab,
			(char *)osl_sp);

    point_t scolor;
    VSETALL(scolor, 0.0f);

    int nsamples = 4;
    for(int s=0; s<nsamples; s++){

	/* -----------------------------------
	 * Fill in all necessary information for the OSL renderer
	 * -----------------------------------
	 */
	RenderInfo info;

	/* Set hit point */
	VMOVE(info.P, swp->sw_hit.hit_point);
    
	/* Set normal at the poit */
	VMOVE(info.N, swp->sw_hit.hit_normal);
    
	/* Set incidence ray direction */
	VMOVE(info.I, ap->a_inv_dir);
    
	/* U-V mapping stuff */
	info.u = swp->sw_uv.uv_u;
	info.v = swp->sw_uv.uv_v;
	VSETALL(info.dPdu, 0.0f);
	VSETALL(info.dPdv, 0.0f);
    
	/* x and y pixel coordinates */
	info.screen_x = ap->a_x;
	info.screen_y = ap->a_y;

	info.depth = ap->a_level;
	info.isbackfacing = 0;
	info.surfacearea = 1.0f;
    
	info.shadername = "yellow";

	/* We only perform reflection if application decides to */
	info.doreflection = 0;
    
	Color3 weight = oslr->QueryColor(&info);

	if(info.doreflection == 1){
	
	    /* We shoot another ray */
	    ap->a_level++;

	    point_t inv_dir;
	    VREVERSE(inv_dir, info.out_ray.dir);

	    VMOVE(ap->a_ray.r_pt, info.out_ray.origin);
	    VMOVE(ap->a_ray.r_dir, info.out_ray.dir);

	    (void)rt_shootray(ap);

	    ap->a_level--;
	    /* The resulting color is always on ap_color, but
	       we need to update it through sw_color */
	    Color3 rec(ap->a_color[0], ap->a_color[1], ap->a_color[2]);
	    Color3 res = rec*weight;
	    VADD2(scolor, scolor, res);
	}
	else {
	    /* Final color */
	    VADD2(scolor, scolor, weight);
	}
    }
    VSCALE(swp->sw_color, scolor, 1.0/nsamples);

    return 1;
}

#ifdef __cplusplus
}
#endif


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
