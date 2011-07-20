
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

#include "./liboslrend.h"

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


#define OSL_MAGIC 0x1837    /* make this a unique number for each shader */
#define CK_OSL_SP(_p) BU_CKMAG(_p, OSL_MAGIC, "osl_specific")

/* Oslrenderer system */
OSLRenderer *oslr = NULL;

/* Every time a thread reaches osl_render for the first time,
   we save the address of their own buffers, which is an ugly way to
   identify them */
std::vector<struct resource *> visited_addrs;
/* Holds information about the context necessary to correctly execute a
   shader */
std::vector<void *> thread_infos;

/* Save default a_hit */
int (*default_a_hit)(struct application *, struct partition *, struct seg *);	
/* Save default a_miss */
int (*default_a_miss)(struct application *);	

/*
 * The shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct osl_specific {
    long magic;           /* magic # for memory validity check */
    ShadingAttribStateRef shader_ref;  /* Reference to this shader in OSLRender system */
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
    {"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

extern "C" {

    HIDDEN int osl_setup(register struct region *rp, struct bu_vls *matparm, 
			 genptr_t *dpp, const struct mfuncs *mfp, 
			 struct rt_i *rtip);

    HIDDEN int osl_render(struct application *ap, const struct partition *pp, 
			  struct shadework *swp, genptr_t dp);
    HIDDEN void osl_print(register struct region *rp, genptr_t dp);
    HIDDEN void osl_free(genptr_t cp);

}

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

int 
osl_parse_edge(char *edge, ShaderEdge &sh_edge)
{
    /* Split string arount # */
    const char *item;
    
    ShaderParam sh_param1, sh_param2;

    /* Name of the first shader */
    if((item = strtok(edge, "#")) == NULL){
	fprintf(stderr, "[Error] Expecting the first shader name, found NULL.\n");
	return -1;
    }
    sh_param1.layername = item;

    /* Parameter of the first shader */
    if((item = strtok(NULL, "#")) == NULL){
	fprintf(stderr, "[Error] Expecting the parameter of the first shader, found NULL.\n");
	return -1;
    }
    sh_param1.paramname = item;

    /* Name of the first shader */
    if((item = strtok(NULL, "#")) == NULL){
	fprintf(stderr, "[Error] Expecting the second shader name, found NULL.\n");
	return -1;
    }
    sh_param2.layername = item;

    /* Name of the first shader */
    if((item = strtok(NULL, "#")) == NULL){
	fprintf(stderr, "[Error] Expecting the parameter of the second shader, found NULL.\n");
	return -1;
    }
    sh_param2.paramname = item;

    sh_edge = std::make_pair(sh_param1, sh_param2);
}

int 
osl_parse_shader(char *shadername, ShaderInfo &sh_info)
{

    /* Split string arount # */
    const char *item;
    item = strtok(shadername, "#");
    /* We are going to look for shader in ../shaders/ */
    sh_info.shadername = "../shaders/" + std::string(item);  

    /* Check for parameters */
    while((item = strtok(NULL, "#")) != NULL){

	/* Setting layer name, in case we're doing a shader group */
	if(strcmp(item, "layername") == 0){

	    /* Get the name of the layer being set */
	    item = strtok(NULL, "#");
	    if(item == NULL){
		fprintf(stderr, "[Error] Missing layer name\n");
		return -1;
	    }
	    sh_info.layername = std::string(item);
	}
	else {
	    /* Name of the parameter */
	    std::string param_name = item;

	    /* Get the type of parameter being set */
	    item = strtok(NULL, "#");
	    if(item == NULL){
		fprintf(stderr, "[Error] Missing parameter type\n");
		return -1;
	    }
	    else if(strcmp(item, "float") == 0){
		item = strtok(NULL, "#");
		if(item == NULL){
		    fprintf(stderr, "[Error] Missing float value\n");
		    return -1;
		}
		float value = atof(item);
		sh_info.fparam.push_back(std::make_pair(param_name, value));
	    }
	    else if(strcmp(item, "color") == 0){
		Color3 color_value;
		for(int i=0; i<3; i++){
		    item = strtok(NULL, "#");
		    if(item == NULL){
			fprintf(stderr, "[Error] Missing %d-th component of color value\n", i);
			return -1;
		    }
		    color_value[i] = atof(item);
		}
		sh_info.cparam.push_back(std::make_pair(param_name, color_value));
	    }
	    else if(strcmp(item, "string") == 0){
		item = strtok(NULL, "#");
		if(item == NULL){
		    fprintf(stderr, "[Error] Missing string\n");
		    return -1;
		}
		std::string string_value = item;
		sh_info.sparam.push_back(std::make_pair(param_name, string_value));
	    }
	    else {
		/* FIXME: add support to TypeInt, TypePoint, TypeVector, TypeNormal, TypeString parameters */
		fprintf(stderr, "[Error] Unknown parameter type\n");
		return -1;			
	    }
	}
    }
}

/**
 * This function parses the input shaders
 * Example:
 * shadername=color#Cin#point#0.0#0.0#1.0
 * shadername=glass
 * shadername=checker#K#float#4.0
 * join=color#Cout#shader#Cin1
 * join=glass#Cout#shader#Cin1
 **/
int
osl_parse(const struct bu_vls *in_vls, ShaderGroupInfo &group_info)
{
    struct bu_vls vls;
    register char *cp;
    char *name;
    char *value;
    int retval;

    BU_CK_VLS(in_vls);

    /* Duplicate the input string.  This algorithm is destructive. */
    bu_vls_init(&vls);
    bu_vls_vlscat(&vls, in_vls);
    cp = bu_vls_addr(&vls);

    while (*cp) {
	/* NAME = VALUE white-space-separator */

	/* skip any leading whitespace */
	while (*cp != '\0' && isspace(*cp))
	    cp++;

	/* Find equal sign */
	name = cp;
	while (*cp != '\0' && *cp != '=')
	    cp++;

	if (*cp == '\0') {
	    if (name == cp) break;

	    /* end of string in middle of arg */
	    bu_log("bu_structparse: input keyword '%s' is not followed by '=' in '%s'\nInput must be in keyword=value format.\n",
		   name, bu_vls_addr(in_vls));
	    bu_vls_free(&vls);
	    return -2;
	}

	*cp++ = '\0';

	/* Find end of value. */
	if (*cp == '"') {
	    /* strings are double-quote (") delimited skip leading " &
	     * find terminating " while skipping escaped quotes (\")
	     */
	    for (value = ++cp; *cp != '\0'; ++cp)
		if (*cp == '"' &&
		    (cp == value || *(cp-1) != '\\'))
		    break;

	    if (*cp != '"') {
		bu_log("bu_structparse: keyword '%s'=\" without closing \"\n",
		       name);
		bu_vls_free(&vls);
		return -3;
	    }
	} else {
	    /* non-strings are white-space delimited */
	    value = cp;
	    while (*cp != '\0' && !isspace(*cp))
		cp++;
	}

	if (*cp != '\0')
	    *cp++ = '\0';

	if(strcmp(name, "shadername") == 0){
	    ShaderInfo sh_info;
	    osl_parse_shader(value, sh_info);
	    group_info.shader_layers.push_back(sh_info);
	}
	else if (strcmp(name, "join") == 0){
	    ShaderEdge sh_edge;
	    osl_parse_edge(value, sh_edge);
	    group_info.shader_edges.push_back(sh_edge);
	}
    }
    bu_vls_free(&vls);
    return 0;
}

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

extern "C" {

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

    osl_sp->magic = OSL_MAGIC;

    /* Parse the user's arguments to fill osl specifics */
    ShaderGroupInfo group_info;
    if (osl_parse(matparm, group_info) < 0){
	return -1;
    }

    /* -----------------------------------
     * Initialize osl render system
     * -----------------------------------
     */
    /* If OSL system was not initialized yet, do it */
    if (oslr == NULL){
	oslr = new OSLRenderer();
    }
    /* Add this shader to OSL system */
    osl_sp->shader_ref = oslr->AddShader(group_info); 

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

#if 0
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    if(oslr != NULL){
	delete oslr;
	oslr = NULL;
    }
    bu_semaphore_release(BU_SEM_SYSCALL);
#endif
}

/*
 * Callback function to be called whenever a refraction ray hits an object
 */
int
osl_refraction_hit(struct application *ap, struct partition *PartHeadp, struct seg *finished_segs)
{

    /* iterating over partitions, this will keep track of the current
     * partition we're working on.
     */
    struct partition *pp;

    /* will serve as a pointer for the entry and exit hitpoints */
    struct hit *hitp;

    /* will serve as a pointer to the solid primitive we hit */
    struct soltab *stp;

    /* will contain surface curvature information at the entry */
    struct curvature cur;

    /* will contain our hit point coordinate */
    point_t pt;

    /* will contain normal vector where ray enters geometry */
     vect_t inormal;

    /* will contain normal vector where ray exits geometry */
    vect_t onormal;

    struct shadework sw;

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {

	register const struct mfuncs *mfp;
	register const struct region *rp;

	memset((char *)&sw, 0, sizeof(sw));
	sw.sw_transmit = sw.sw_reflect = 0.0;
	sw.sw_refrac_index = 1.0;
	sw.sw_extinction = 0;
	sw.sw_xmitonly = 0;		/* want full data */
	sw.sw_inputs = 0;		/* no fields filled yet */
	//sw.sw_frame = curframe;
	//sw.sw_pixeltime = sw.sw_frametime = curframe * frame_delta_t;
	sw.sw_segs = finished_segs;
	VSETALL(sw.sw_color, 1);
	VSETALL(sw.sw_basecolor, 1);
    
	rp = pp->pt_regionp;
	mfp = (struct mfuncs *)pp->pt_regionp->reg_mfuncs;

	/* Determine the hit point */
	sw.sw_hit = *(pp->pt_outhit);		/* struct copy */
    	VJOIN1(sw.sw_hit.hit_point, ap->a_ray.r_pt, sw.sw_hit.hit_dist, ap->a_ray.r_dir);

#if 0
	point_t pt;
    	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VPRINT("output point: ", sw.sw_hit.hit_point);
	VPRINT(" input point: ", pt);
#endif

	/* Determine the normal point */
	stp = pp->pt_outseg->seg_stp;
	RT_HIT_NORMAL(sw.sw_hit.hit_normal, &(sw.sw_hit), stp, &(ap->a_ray), pp->pt_outflip);

	/* Invoke the actual shader (may be a tree of them) */
	if (mfp && mfp->mf_render)
	    (void)mfp->mf_render(ap, pp, &sw, rp->reg_udata);

	VMOVE(ap->a_color, sw.sw_color);
    }
    return 1;
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
    
    void * thread_info;

    int nsamples; /* Number of samples */

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_OSL_SP(osl_sp);
    
    if (rdebug&RDEBUG_SHADE)
	bu_struct_print("osl_render Parameters:", osl_print_tab,
			(char *)osl_sp);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    
    /* Check if it is the first time this thread is calling this function */
    bool visited = false;
    for(size_t i = 0; i < visited_addrs.size(); i++){
	if(ap->a_resource == visited_addrs[i]){
	    visited = true;
	    thread_info = thread_infos[i];
	    break;
	}
    } 
    if(!visited){
	visited_addrs.push_back(ap->a_resource);
	/* Get thread specific information from OSLRender system */
	thread_info = oslr->CreateThreadInfo();
	thread_infos.push_back(thread_info);
    }

    if(ap->a_level == 0){
	default_a_hit = ap->a_hit; /* save the default hit callback (colorview @ rt) */
	default_a_miss = ap->a_miss;
	/* Get the number of samples from the environment */
	char *str_nsamples = getenv("LIBRT_OSL_SAMPLES");
	if(str_nsamples == NULL) nsamples = 10;
	else nsamples = atoi(str_nsamples);
    }
    else nsamples = 1;

    bu_semaphore_release(BU_SEM_SYSCALL);

    Color3 acc_color(0.0f);

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
    VMOVE(info.I, ap->a_ray.r_dir);
    
    /* U-V mapping stuff */
    info.u = swp->sw_uv.uv_u;
    info.v = swp->sw_uv.uv_v;
    VSETALL(info.dPdu, 0.0f);
    VSETALL(info.dPdv, 0.0f);
    
    /* x and y pixel coordinates */
    info.screen_x = ap->a_x;
    info.screen_y = ap->a_y;
    
    info.depth = ap->a_level;
    info.surfacearea = 1.0f;
    
    info.shader_ref = osl_sp->shader_ref;

    /* We assume that the only information that will be written is thread_info,
       so that oslr->QueryColor is thread safe */
    info.thread_info = thread_info;
    
    for(int i = 0; i < nsamples; i++){

    	/* We only perform reflection if application decides to */
	info.doreflection = 0;

	Color3 weight = oslr->QueryColor(&info);

	/* Fire another ray */
	if(info.doreflection == 1){
	
	    struct application new_ap;
	    RT_APPLICATION_INIT(&new_ap);
	
	    new_ap = *ap;                     /* struct copy */
	    new_ap.a_onehit = 1;
	    new_ap.a_hit = default_a_hit;
	    new_ap.a_level = info.depth + 1;
	    new_ap.a_flag = 0;

	    VMOVE(new_ap.a_ray.r_dir, info.out_ray.dir);
	    VMOVE(new_ap.a_ray.r_pt, info.out_ray.origin);
	
	    /* This next ray represents refraction */
	    if (VDOT(info.N, info.out_ray.dir) < 0.0f){
	    
		/* Displace the hit point a little bit in the direction
		   of the next ray */
		Vec3 tmp;
		VSCALE(tmp, info.out_ray.dir, 1e-4);
		VADD2(new_ap.a_ray.r_pt, new_ap.a_ray.r_pt, tmp);

		new_ap.a_onehit = 1;
		new_ap.a_refrac_index = 1.5;
		new_ap.a_flag = 2; /* mark as refraction */
		new_ap.a_hit = osl_refraction_hit;
	    }	

	    (void)rt_shootray(&new_ap);

	    Color3 rec;
	    VMOVE(rec, new_ap.a_color);
 
	    Color3 res = rec*weight;
	    VADD2(acc_color, acc_color, res);
	}
	else {
	    /* Final color */
	    VADD2(acc_color, acc_color, weight);
	}
    }

    VSCALE(swp->sw_color, acc_color, 1.0/nsamples);

    return 1;
}
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
