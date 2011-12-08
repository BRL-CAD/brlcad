/*                      S H _ G A U S S . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @file liboptical/sh_gauss.c
 *
 * To add a new shader to the "rt" program:
 *
 *	1) Copy this file to sh_shadername.c
 *	2) edit sh_shadername.c:
 *		change "G A U S S" to "S H A D E R N A M E"
 *		change "gauss" to "shadername"
 *		Set a new number for the gauss_MAGIC define
 *		define shader specific structure and defaults
 *		edit/build parse table for bu_structparse from gauss_parse
 *		edit/build shader_mfuncs tables from gauss_mfuncs for
 *			each shader name being built.
 *		edit the gauss_setup function to do shader-specific setup
 *		edit the gauss_render function to do the actual rendering
 *	3) Edit view.c to add extern for gauss_mfuncs and call to mlib_add
 *		to function view_init()
 *	4) Edit Makefile.am to add shader file to the compilation
 *	5) replace this list with a description of the shader, its parameters
 *		and use.
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "optical.h"


/* The internal representation of the solids must be stored so that we
 * can access their parameters at shading time.  This is done with
 * a list of "struct reg_db_internals".  Each struct holds the
 * representation of one of the solids which make up the region.
 */
#define DBINT_MAGIC 0xDECCA
struct reg_db_internals {
    struct bu_list l;
    struct rt_db_internal ip;	/* internal rep from rtgeom.h */
    struct soltab *st_p;
    vect_t one_sigma;
    mat_t ell2model;	/* maps ellipse coord to model coord */
    mat_t model2ell;	/* maps model coord to ellipse coord */
};
#define DBINT_MAGIC 0xDECCA
#define CK_DBINT(_p) BU_CKMAG(_p, DBINT_MAGIC, "struct reg_db_internals")

struct tree_bark {
    struct db_i *dbip;
    struct bu_list *l;	/* lists solids in region (built in setup) */
    const char *name;
    struct gauss_specific *gs;
};


#define gauss_MAGIC 0x6a05    /* make this a unique number for each shader */
#define CK_gauss_SP(_p) BU_CKMAG(_p, gauss_MAGIC, "gauss_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct gauss_specific {
    uint32_t magic;	/* magic # for memory validity check, must come 1st */
    double gauss_sigma;	/* # std dev represented by ell bounds */
    point_t gauss_min;
    point_t gauss_max;
    mat_t gauss_m_to_sh;	/* model to shader space matrix */
    struct bu_list dbil;
};


/* The default values for the variables in the shader specific structure */
static const
struct gauss_specific gauss_defaults = {
    gauss_MAGIC,
    4.0,
    VINIT_ZERO, /* min */
    VINIT_ZERO, /* max */
    MAT_INIT_ZERO, /* gauss_m_to_sh */
    {0, NULL, NULL}
};


#define SHDR_NULL ((struct gauss_specific *)0)
#define SHDR_O(m) bu_offsetof(struct gauss_specific, m)
#define SHDR_AO(m) bu_offsetofarray(struct gauss_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse gauss_print_tab[] = {
    {"%f", 1, "sigma",		SHDR_O(gauss_sigma),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }

};
struct bu_structparse gauss_parse_tab[] = {
    {"%p", bu_byteoffset(gauss_print_tab[0]), "gauss_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 1, "s",			SHDR_O(gauss_sigma),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int gauss_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int gauss_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp);
HIDDEN void gauss_print(register struct region *rp, genptr_t dp);
HIDDEN void gauss_free(genptr_t cp);

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs gauss_mfuncs[] = {
    {MF_MAGIC,	"gauss",	0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
     gauss_setup,	gauss_render,	gauss_print,	gauss_free },

    {0,		(char *)0,	0,		0,		0,
     0,		0,		0,		0 }
};


static void
tree_solids(union tree *tp, struct tree_bark *tb, int op, struct resource *resp)
{
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return;

	case OP_SOLID: {
	    struct reg_db_internals *dbint;
	    matp_t mp;
	    long sol_id;
	    struct rt_ell_internal *ell_p;
	    vect_t v;

	    BU_GET(dbint, struct reg_db_internals);
	    BU_LIST_INIT_MAGIC(&(dbint->l), DBINT_MAGIC);

	    if (tp->tr_a.tu_stp->st_matp)
		mp = tp->tr_a.tu_stp->st_matp;
	    else
		mp = (matp_t)bn_mat_identity;

	    /* Get the internal form of this solid & add it to the list */
	    rt_db_get_internal(&dbint->ip, tp->tr_a.tu_stp->st_dp,
			       tb->dbip, mp, resp);

	    RT_CK_DB_INTERNAL(&dbint->ip);
	    dbint->st_p = tp->tr_a.tu_stp;

	    sol_id = dbint->ip.idb_type;

	    if (sol_id < 0) {
		bu_log("Primitive ID %ld out of bounds\n", sol_id);
		bu_bomb("");
	    }


	    if (sol_id != ID_ELL) {

		if (op == OP_UNION)
		    bu_log("Non-ellipse \"union\" primitive of \"%s\" being ignored\n",
			   tb->name);

		if (rdebug&RDEBUG_SHADE)
		    bu_log(" got a primitive type %d \"%s\".  This primitive ain't no ellipse bucko!\n",
			   sol_id, rt_functab[sol_id].ft_name);

		break;
	    }


	    ell_p = (struct rt_ell_internal *)dbint->ip.idb_ptr;

	    if (rdebug&RDEBUG_SHADE)
		bu_log(" got a primitive type %d \"%s\"\n",
		       sol_id,
		       rt_functab[sol_id].ft_name);

	    RT_ELL_CK_MAGIC(ell_p);

	    if (rdebug&RDEBUG_SHADE) {
		VPRINT("point", ell_p->v);
		VPRINT("a", ell_p->a);
		VPRINT("b", ell_p->b);
		VPRINT("c", ell_p->c);
	    }

	    /* create the matrix that maps the coordinate system defined
	     * by the ellipse into model space, and get inverse for use
	     * in the _render() proc
	     */
	    MAT_IDN(mp);
	    VMOVE(v, ell_p->a);	VUNITIZE(v);
	    mp[0] = v[0];	mp[4] = v[1];	mp[8] = v[2];

	    VMOVE(v, ell_p->b);	VUNITIZE(v);
	    mp[1] = v[0];	mp[5] = v[1];	mp[9] = v[2];

	    VMOVE(v, ell_p->c);	VUNITIZE(v);
	    mp[2] = v[0];	mp[6] = v[1];	mp[10] = v[2];

	    MAT_DELTAS_VEC(mp, ell_p->v);

	    MAT_COPY(dbint->ell2model, mp);
	    bn_mat_inv(dbint->model2ell, mp);


	    /* find scaling of gaussian puff in ellipsoid space */
	    VSET(dbint->one_sigma,
		 MAGNITUDE(ell_p->a) / tb->gs->gauss_sigma,
		 MAGNITUDE(ell_p->b) / tb->gs->gauss_sigma,
		 MAGNITUDE(ell_p->c) / tb->gs->gauss_sigma);


	    if (rdebug&RDEBUG_SHADE) {
		VPRINT("sigma", dbint->one_sigma);
	    }
	    BU_LIST_APPEND(tb->l, &(dbint->l));

	    break;
	}
	case OP_UNION:
	    tree_solids(tp->tr_b.tb_left, tb, tp->tr_op, resp);
	    tree_solids(tp->tr_b.tb_right, tb, tp->tr_op, resp);
	    break;

	case OP_NOT: bu_log("Warning: 'Not' region operator in %s\n", tb->name);
	    tree_solids(tp->tr_b.tb_left, tb, tp->tr_op, resp);
	    break;
	case OP_GUARD:bu_log("Warning: 'Guard' region operator in %s\n", tb->name);
	    tree_solids(tp->tr_b.tb_left, tb, tp->tr_op, resp);
	    break;
	case OP_XNOP:bu_log("Warning: 'XNOP' region operator in %s\n", tb->name);
	    tree_solids(tp->tr_b.tb_left, tb, tp->tr_op, resp);
	    break;

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* XXX this can get us in trouble if 1 solid is subtracted
	     * from less than all the "union" solids of the region.
	     */
	    tree_solids(tp->tr_b.tb_left, tb, tp->tr_op, resp);
	    tree_solids(tp->tr_b.tb_right, tb, tp->tr_op, resp);
	    return;

	default:
	    bu_bomb("rt_tree_region_assign: bad op\n");
    }
}


/* G A U S S _ S E T U P
 *
 * This routine is called (at prep time)
 * once for each region which uses this shader.
 * Any shader-specific initialization should be done here.
 */
HIDDEN int
gauss_setup(register struct region *rp, struct bu_vls *matparm, genptr_t *dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)


/* pointer to reg_udata in *rp */

/* New since 4.4 release */
{
    register struct gauss_specific *gauss_sp;
    struct tree_bark tb;

    /* check the arguments */
    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);


    if (rdebug&RDEBUG_SHADE)
	bu_log("gauss_setup(%s)\n", rp->reg_name);

    if (! rtip->useair)
	bu_bomb("gauss shader used and useair not set\n");


    /* Get memory for the shader parameters and shader-specific data */
    BU_GET(gauss_sp, struct gauss_specific);
    *dpp = gauss_sp;

    /* initialize the default values for the shader */
    memcpy(gauss_sp, &gauss_defaults, sizeof(struct gauss_specific));

    /* parse the user's arguments for this use of the shader. */
    if (bu_struct_parse(matparm, gauss_parse_tab, (char *)gauss_sp) < 0)
	return -1;

    /* We have to pick up the parameters for the gaussian puff now.
     * They won't be available later.  So what we do is sneak a peak
     * inside the region, make sure the first item in it is an ellipsoid
     * solid, and if it is go steal a peek at its balls ... er ... make
     * that definition/parameters.
     */

    BU_LIST_INIT(&gauss_sp->dbil);
    tb.l = &gauss_sp->dbil;

    tb.dbip = rtip->rti_dbip;
    tb.name = rp->reg_name;
    tb.gs = gauss_sp;

    tree_solids (rp->reg_treetop, &tb, OP_UNION, &rt_uniresource);


    /* XXX If this puppy isn't axis-aligned, we should come up with a
     * matrix to rotate it into alignment.  We're going to have to do
     * computation in the space defined by this ellipsoid.
     */

    if (rdebug&RDEBUG_SHADE) {
	bu_struct_print(" Parameters:", gauss_print_tab, (char *)gauss_sp);
	bn_mat_print("m_to_sh", gauss_sp->gauss_m_to_sh);
    }

    return 1;
}


/*
   * G A U S S _ P R I N T
   */
HIDDEN void
gauss_print(register struct region *rp, genptr_t dp)
{
    bu_struct_print(rp->reg_name, gauss_print_tab, (char *)dp);
}


/*
 * G A U S S _ F R E E
 */
HIDDEN void
gauss_free(genptr_t cp)
{
    register struct gauss_specific *gauss_sp =
	(struct gauss_specific *)cp;
    struct reg_db_internals *p;

    while (BU_LIST_WHILE(p, reg_db_internals, &gauss_sp->dbil)) {
	BU_LIST_DEQUEUE(&(p->l));
	bu_free(p->ip.idb_ptr, "internal ptr");
	bu_free((genptr_t)p, "gauss reg_db_internals");
    }

    bu_free(cp, "gauss_specific");
}



/*
 *
 * Evaluate the 3-D gaussian "puff" function:
 *
 * 1.0 / ((2*PI)^(3/2) * sigmaX*sigmaY*sigmaZ)) *
 * exp(-0.5 * ((x-ux)^2/sigmaX + (y-uy)^2/sigmaY + (z-uz)^2/sigmaZ))
 *
 * for a given point "pt" where the center of the puff is at {ux, uy, uz} and
 * the size of 1 standard deviation is {sigmaX, sigmaY, sigmaZ}
 */
static double
gauss_eval(fastf_t *pt, fastf_t *ell_center, fastf_t *sigma)
{
    double term2;
    point_t p;
    double val;

    VSUB2(p, pt, ell_center);
    p[X] *= p[X];
    p[Y] *= p[Y];
    p[Z] *= p[Z];

    term2 = (p[X]/sigma[X]) + (p[Y]/sigma[Y]) + (p[Z]/sigma[Z]);
    term2 *= term2;

    val = exp(-0.5 * term2);

    if (rdebug&RDEBUG_SHADE)
	bu_log("pt(%g %g %g) term2:%g val:%g\n",
	       V3ARGS(pt), term2, val);

    return val;
}


/*
 * Given a seg which participates in the partition we are shading evaluate
 * the transmission on the path
 */
static double
eval_seg(struct application *ap, struct reg_db_internals *dbint, struct seg *seg_p)
{
    double span;
    point_t pt;
    struct rt_ell_internal *ell_p = (struct rt_ell_internal *)dbint->ip.idb_ptr;
    double optical_density = 0.0;
    double step_dist;
    double dist;
    int steps;


    /* XXX Should map the ray into the coordinate system of the ellipsoid
     * here, so that all computations are done in an axis-aligned system
     * with the axes being the gaussian dimensions
     */


    span = seg_p->seg_out.hit_dist - seg_p->seg_in.hit_dist;
    steps = (int)(span / 100.0 + 0.5);
    if (steps < 2) steps = 2;

    step_dist = span / (double)steps;


    if (rdebug&RDEBUG_SHADE) {
	bu_log("Evaluating Segment:\n");
	bu_log("dist_in:%g dist_out:%g span:%g step_dist:%g steps:%d\n",
	       seg_p->seg_in.hit_dist,
	       seg_p->seg_out.hit_dist,
	       span, step_dist, steps);

    }
#if 1
    for (dist=seg_p->seg_in.hit_dist; dist < seg_p->seg_out.hit_dist; dist += step_dist) {
	VJOIN1(pt, ap->a_ray.r_pt, dist, ap->a_ray.r_dir);
	optical_density += gauss_eval(pt, ell_p->v, dbint->one_sigma);
    }


    return optical_density;
#else
    return gauss_eval(ell_p->v, ell_p->v, dbint->one_sigma);
#endif
}


/*
 * G A U S S _ R E N D E R
 *
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.
 */
int
gauss_render(struct application *ap, const struct partition *pp, struct shadework *swp, genptr_t dp)


/* defined in material.h */
/* ptr to the shader-specific struct */
{
    register struct gauss_specific *gauss_sp =
	(struct gauss_specific *)dp;
    struct seg *seg_p;
    struct reg_db_internals *dbint_p;
    double optical_density = 0.0;

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_gauss_SP(gauss_sp);

    if (rdebug&RDEBUG_SHADE) {
	bu_struct_print("gauss_render Parameters:", gauss_print_tab, (char *)gauss_sp);

	bu_log("r_pt(%g %g %g) r_dir(%g %g %g)\n",
	       V3ARGS(ap->a_ray.r_pt),
	       V3ARGS(ap->a_ray.r_dir));
    }

    BU_CK_LIST_HEAD(&swp->sw_segs->l);
    BU_CK_LIST_HEAD(&gauss_sp->dbil);


    /* look at each segment that participated in the ray partition(s) */
    for (BU_LIST_FOR(seg_p, seg, &swp->sw_segs->l)) {

	if (rdebug&RDEBUG_SHADE) {
	    bu_log("seg %g -> %g\n",
		   seg_p->seg_in.hit_dist,
		   seg_p->seg_out.hit_dist);
	}
	RT_CK_SEG(seg_p);
	RT_CK_SOLTAB(seg_p->seg_stp);

	/* check to see if the seg/solid is in this partition */
	if (bu_ptbl_locate(&pp->pt_seglist, (long *)seg_p) != -1) {

	    /* XXX You might use a bu_ptbl list of the solid pointers... */
	    /* check to see if the solid is from this region */
	    for (BU_LIST_FOR(dbint_p, reg_db_internals,
			     &gauss_sp->dbil)) {

		CK_DBINT(dbint_p);

		if (dbint_p->st_p == seg_p->seg_stp) {
		    /* The solid from the region is
		     * the solid from the segment
		     * from the partition
		     */
		    optical_density +=
			eval_seg(ap, dbint_p, seg_p);
		    break;
		}
	    }
	} else {
	    if (rdebug&RDEBUG_SHADE)
		bu_log("gauss_render() bittest failed\n");
	}
    }


    if (rdebug&RDEBUG_SHADE)
	bu_log("Optical Density %g\n", optical_density);

    /* get the path length right */
/* if (pp->pt_inhit->hit_dist < 0.0)
   partition_dist = pp->pt_outhit->hit_dist;
   else
   partition_dist = (pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist);

   tau = optical_density * partition_dist;
   swp->sw_transmit = exp(-tau); */

    swp->sw_transmit = 1.0 - optical_density;

/* VMOVE(swp->sw_color, pt);*/

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
