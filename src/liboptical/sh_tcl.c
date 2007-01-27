/*                        S H _ T C L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file sh_tcl.c
 *
 *  To add a new shader to the "rt" program's LIBOPTICAL library:
 *
 *	6) Edit shaders.tcl and comb.tcl in the ../tclscripts/mged directory to
 *		add a new gui for this shader.
 */
#include "common.h"

#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
#include <tcl.h>

extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);
#define tcl_MAGIC 0x54434C00    /* "TCL" */
#define CK_tcl_SP(_p) BU_CKMAG(_p, tcl_MAGIC, "tcl_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct tcl_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	mat_t			tcl_m_to_r; /* model to shader space matrix */
	Tcl_Interp	       *tcl_interp[MAX_PSW];
	Tcl_Obj		       *tcl_objPtr;
	struct bu_vls		tcl_file;   /* name of script to run */
	struct bu_mapped_file  *tcl_mp;	    /* actual script */
};

/* The default values for the variables in the shader specific structure */
const static
struct tcl_specific tcl_defaults = {
	tcl_MAGIC,
	{	0.0, 0.0, 0.0, 0.0,	/* tcl_m_to_r */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 }
	};

#define SHDR_NULL	((struct tcl_specific *)0)
#define SHDR_O(m)	bu_offsetof(struct tcl_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct tcl_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse tcl_print_tab[] = {
	{"%S",  1, "file", SHDR_O(tcl_file),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL }

};
struct bu_structparse tcl_parse_tab[] = {
	{"i",	bu_byteoffset(tcl_print_tab[0]), "tcl_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL },
	{"%S",  1, "f", SHDR_O(tcl_file),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	tcl_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), tcl_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	tcl_print(register struct region *rp, char *dp), tcl_free(char *cp);

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs tcl_mfuncs[] = {
	{MF_MAGIC,	"tcl",		0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	tcl_setup,	tcl_render,	tcl_print,	tcl_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};


/*	T C L _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
tcl_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)


				/* pointer to reg_udata in *rp */

				/* New since 4.4 release */
{
	register struct tcl_specific	*tcl_sp;
	int cpu;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);


	if (rdebug&RDEBUG_SHADE)
		bu_log("tcl_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	BU_GETSTRUCT( tcl_sp, tcl_specific );
	*dpp = (char *)tcl_sp;

	/* initialize the default values for the shader */
	memcpy(tcl_sp, &tcl_defaults, sizeof(struct tcl_specific) );

	/* parse the user's arguments for this use of the shader. */
	if (bu_struct_parse( matparm, tcl_parse_tab, (char *)tcl_sp ) < 0 )
		return(-1);

#if 0
	tcl_sp->tcl_mp = bu_open_mapped_file(bu_vls_addr(tcl_sp->tcl_file),
					     "tclShader");
	if (!tcl_sp->tcl_mp) {
		bu_log("Error opening Tcl shader file \"%s\"\n",
		       bu_vls_addr(tcl_sp->tcl_file));
		bu_bomb("");
	}
#endif

	for (cpu=0 ; cpu < MAX_PSW ; cpu++) {
		tcl_sp->tcl_interp[cpu] = Tcl_CreateInterp();
		Tcl_Init(tcl_sp->tcl_interp[cpu]);
	}

	/* the shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 *
	 * Shading is be done in "region coordinates":
	 */
	db_region_mat(tcl_sp->tcl_m_to_r, rtip->rti_dbip, rp->reg_name, &rt_uniresource);


	if (rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", tcl_print_tab,
				 (char *)tcl_sp );
		bn_mat_print( "m_to_sh", tcl_sp->tcl_m_to_r );
	}

	return(1);
}

/*
 *	T C L _ P R I N T
 */
HIDDEN void
tcl_print(register struct region *rp, char *dp)
{
	bu_struct_print( rp->reg_name, tcl_print_tab, (char *)dp );
}

/*
 *	T C L _ F R E E
 */
HIDDEN void
tcl_free(char *cp)
{
	bu_free( cp, "tcl_specific" );
}

/*
 *	T C L _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
tcl_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)


				/* defined in material.h */
				/* ptr to the shader-specific struct */
{
	register struct tcl_specific *tcl_sp =
		(struct tcl_specific *)dp;
	point_t pt;
	int tcl_status;
	register int cpu = ap->a_resource->re_cpu;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_tcl_SP(tcl_sp);

	if (rdebug&RDEBUG_SHADE)
		bu_struct_print( "tcl_render Parameters:",
				 tcl_print_tab, (char *)tcl_sp );

	/* we are performing the shading in "region" space, so we must
	 * transform the hit point from "model" space to "region" space.
	 */
	MAT4X3PNT(pt, tcl_sp->tcl_m_to_r, swp->sw_hit.hit_point);

	if (rdebug&RDEBUG_SHADE) {
		bu_log("tcl_render()  model:(%g %g %g) shader:(%g %g %g)\n",
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
	}

	/* set some Tcl variables to the shadework structure values */

#define rt_Tcl_LV(_s, _v) Tcl_LinkVar(tcl_sp->tcl_interp[cpu], _s, (char *)_v, \
				TCL_LINK_DOUBLE)

	rt_Tcl_LV("sw_transmit",	&swp->sw_transmit);
	rt_Tcl_LV("sw_reflect",		&swp->sw_reflect);
	rt_Tcl_LV("sw_refrac_index",	&swp->sw_refrac_index);
	rt_Tcl_LV("sw_temperature",	&swp->sw_temperature);
	rt_Tcl_LV("reflect",		&swp->sw_reflect);
	rt_Tcl_LV("sw_red",		&swp->sw_color[0]);
	rt_Tcl_LV("sw_grn",		&swp->sw_color[1]);
	rt_Tcl_LV("sw_blu",		&swp->sw_color[2]);
	rt_Tcl_LV("sw_base_red",	&swp->sw_basecolor[0]);
	rt_Tcl_LV("sw_base_grn",	&swp->sw_basecolor[1]);
	rt_Tcl_LV("sw_base_blu",	&swp->sw_basecolor[2]);
	rt_Tcl_LV("sw_dist",		&swp->sw_hit.hit_dist);
	rt_Tcl_LV("sw_hitpt_x",		&swp->sw_hit.hit_point[X]);
	rt_Tcl_LV("sw_hitpt_y",		&swp->sw_hit.hit_point[Y]);
	rt_Tcl_LV("sw_hitpt_z",		&swp->sw_hit.hit_point[Z]);
	rt_Tcl_LV("sw_normal_x",	&swp->sw_hit.hit_normal[X]);
	rt_Tcl_LV("sw_normal_y",	&swp->sw_hit.hit_normal[Y]);
	rt_Tcl_LV("sw_normal_z",	&swp->sw_hit.hit_normal[Z]);
	rt_Tcl_LV("sw_uv_u",		&swp->sw_uv.uv_u);
	rt_Tcl_LV("sw_uv_v",		&swp->sw_uv.uv_v);
	rt_Tcl_LV("sw_x",		&pt[X]);
	rt_Tcl_LV("sw_y",		&pt[Y]);
	rt_Tcl_LV("sw_z",		&pt[Z]);
	Tcl_LinkVar(tcl_sp->tcl_interp[cpu], "ap_x", (char *)&ap->a_x,
		    TCL_LINK_INT);
	Tcl_LinkVar(tcl_sp->tcl_interp[cpu], "ap_y", (char *)&ap->a_y,
		    TCL_LINK_INT);

	/* XXX run the script (should be Tcl_EvalObj) */
	tcl_status = Tcl_EvalFile(tcl_sp->tcl_interp[cpu],
				  bu_vls_addr(&tcl_sp->tcl_file));

	if (tcl_status != TCL_OK) {
		bu_log("%s\n", Tcl_GetStringResult(tcl_sp->tcl_interp[cpu]));
	}

	/* break the links to these stack variables */
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_transmit");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_reflect");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_refrac_index");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_temperature");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "reflect");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_red");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_grn");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_blu");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_base_red");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_base_grn");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_base_blu");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_dist");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_hitpt_x");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_hitpt_y");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_hitpt_z");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_normal_x");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_normal_y");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_normal_z");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_uv_u");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_uv_v");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_x");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_y");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "sw_z");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "ap_x");
	Tcl_UnlinkVar(tcl_sp->tcl_interp[cpu], "ap_y");


	/* shader must perform transmission/reflection calculations
	 *
	 * 0 < swp->sw_transmit <= 1 causes transmission computations
	 * 0 < swp->sw_reflect <= 1 causes reflection computations
	 */
	if (swp->sw_reflect > 0 || swp->sw_transmit > 0 )
		(void)rr_render( ap, pp, swp );

	return(1);
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
