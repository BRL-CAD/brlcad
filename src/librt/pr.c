/*                            P R . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/pr.c
 *
 * Routines to print LIBRT data structures using bu_log().
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"

#include "raytrace.h"


void
rt_pr_soltab(register const struct soltab *stp)
{
    register int id = stp->st_id;

    if (id <= 0 || id > ID_MAX_SOLID) {
	bu_log("stp=%p, id=%d.\n", (void *)stp, id);
	bu_bomb("rt_pr_soltab:  bad id");
    }
    bu_log("------------ %s (bit %ld) %s ------------\n",
	   stp->st_dp->d_namep, stp->st_bit,
	   OBJ[id].ft_name);
    VPRINT("Bound Sph CENTER", stp->st_center);
    bu_log("Approx Sph Radius = %g\n", INTCLAMP(stp->st_aradius));
    bu_log("Bounding Sph Radius = %g\n", INTCLAMP(stp->st_bradius));
    VPRINT("Bound RPP min", stp->st_min);
    VPRINT("Bound RPP max", stp->st_max);
    bu_pr_ptbl("st_regions", &stp->st_regions, 1);
    if (OBJ[id].ft_print)
	OBJ[id].ft_print(stp);
}


void
rt_pr_region(register const struct region *rp)
{
    struct bu_vls v = BU_VLS_INIT_ZERO;

    RT_CK_REGION(rp);

    bu_log("REGION %s (bit %d)\n", rp->reg_name, rp->reg_bit);
    bu_log("instnum=%ld, id=%d, air=%d, gift_material=%d, los=%d\n",
	   rp->reg_instnum,
	   rp->reg_regionid, rp->reg_aircode,
	   rp->reg_gmater, rp->reg_los);
    if (rp->reg_is_fastgen != REGION_NON_FASTGEN) {
	bu_log("reg_is_fastgen = %s mode\n",
	       rp->reg_is_fastgen == REGION_FASTGEN_PLATE ?
	       "plate" : "volume");
    }
    if (rp->reg_mater.ma_color_valid)
	bu_log("Color %d %d %d\n",
	       (int)rp->reg_mater.ma_color[0]*255,
	       (int)rp->reg_mater.ma_color[1]*255,
	       (int)rp->reg_mater.ma_color[2]*255);
    if (rp->reg_mater.ma_temperature > 0)
	bu_log("Temperature %g degrees K\n", INTCLAMP(rp->reg_mater.ma_temperature));
    if (rp->reg_mater.ma_shader && rp->reg_mater.ma_shader[0] != '\0')
	bu_log("Shader '%s'\n", rp->reg_mater.ma_shader);

    rt_pr_tree_vls(&v, rp->reg_treetop);
    bu_log("%s %ld %s\n", rp->reg_name,
	   rp->reg_instnum, bu_vls_addr(&v));
    bu_vls_free(&v);
}


void
rt_pr_partitions(const struct rt_i *rtip, register const struct partition *phead, const char *title)
{
    register const struct partition *pp;
    struct bu_vls v = BU_VLS_INIT_ZERO;

    RT_CHECK_RTI(rtip);

    bu_log_indent_vls(&v);
    bu_vls_strcat(&v, "------");
    bu_vls_strcat(&v, title);
    bu_vls_strcat(&v, "\n");
    bu_log_indent_delta(2);

    for (pp = phead->pt_forw; pp != phead; pp = pp->pt_forw) {
	RT_CHECK_PT(pp);
	rt_pr_pt_vls(&v, rtip, pp);
    }
    bu_log_indent_delta(-2);
    bu_log_indent_vls(&v);
    bu_vls_strcat(&v, "------\n");

    bu_log("%s", bu_vls_addr(&v));
    bu_vls_free(&v);
}


void
rt_pr_pt_vls(struct bu_vls *v, const struct rt_i *rtip, register const struct partition *pp)
{
    register const struct soltab *stp;
    register struct seg **segpp;

    RT_CHECK_RTI(rtip);
    RT_CHECK_PT(pp);
    BU_CK_VLS(v);

    bu_log_indent_vls(v);
    bu_vls_printf(v, "%p: PT ", (void *)pp);

    stp = pp->pt_inseg->seg_stp;
    bu_vls_printf(v, "%s (%s#%ld) ",
		  stp->st_dp->d_namep,
		  OBJ[stp->st_id].ft_name+3,
		  stp->st_bit);

    stp = pp->pt_outseg->seg_stp;
    bu_vls_printf(v, "%s (%s#%ld) ",
		  stp->st_dp->d_namep,
		  OBJ[stp->st_id].ft_name+3,
		  stp->st_bit);

    bu_vls_printf(v, "(%g, %g)",
		  pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist);
    if (pp->pt_inflip) bu_vls_strcat(v, " Iflip");
    if (pp->pt_outflip) bu_vls_strcat(v, " Oflip");
    bu_vls_strcat(v, "\n");

    rt_pr_hit_vls(v, "  In", pp->pt_inhit);
    rt_pr_hit_vls(v, " Out", pp->pt_outhit);
    bu_log_indent_vls(v);
    bu_vls_strcat(v, "  Primitives: ");
    for (BU_PTBL_FOR(segpp, (struct seg **), &pp->pt_seglist)) {
	stp = (*segpp)->seg_stp;
	RT_CK_SOLTAB(stp);
	bu_vls_strcat(v, stp->st_dp->d_namep);
	bu_vls_strcat(v, ", ");
    }
    bu_vls_strcat(v, "\n");

    bu_log_indent_vls(v);
    bu_vls_strcat(v, "  Untrimmed Segments spanning this interval:\n");
    bu_log_indent_delta(4);
    for (BU_PTBL_FOR(segpp, (struct seg **), &pp->pt_seglist)) {
	RT_CK_SEG(*segpp);
	rt_pr_seg_vls(v, *segpp);
    }
    bu_log_indent_delta(-4);

    if (pp->pt_regionp) {
	RT_CK_REGION(pp->pt_regionp);
	bu_log_indent_vls(v);
	bu_vls_printf(v, "  Region: %s\n", pp->pt_regionp->reg_name);
    }
}


void
rt_pr_pt(const struct rt_i *rtip, register const struct partition *pp)
{
    struct bu_vls v = BU_VLS_INIT_ZERO;

    RT_CHECK_RTI(rtip);
    RT_CHECK_PT(pp);
    rt_pr_pt_vls(&v, rtip, pp);
    bu_log("%s", bu_vls_addr(&v));
    bu_vls_free(&v);
}


void
rt_pr_seg_vls(struct bu_vls *v, register const struct seg *segp)
{
    BU_CK_VLS(v);
    RT_CK_SEG(segp);

    bu_log_indent_vls(v);
    bu_vls_printf(v,
		  "%p: SEG %s (%g, %g) st_bit=%ld xray#=%d\n",
		  (void *)segp,
		  segp->seg_stp->st_dp->d_namep,
		  segp->seg_in.hit_dist,
		  segp->seg_out.hit_dist,
		  segp->seg_stp->st_bit,
		  segp->seg_in.hit_rayp->index);
}


void
rt_pr_seg(register const struct seg *segp)
{
    struct bu_vls v = BU_VLS_INIT_ZERO;

    RT_CK_SEG(segp);

    rt_pr_seg_vls(&v, segp);
    bu_log("%s", bu_vls_addr(&v));
    bu_vls_free(&v);
}


void
rt_pr_hit(const char *str, register const struct hit *hitp)
{
    struct bu_vls v = BU_VLS_INIT_ZERO;

    RT_CK_HIT(hitp);

    rt_pr_hit_vls(&v, str, hitp);
    bu_log("%s", bu_vls_addr(&v));
    bu_vls_free(&v);
}


void
rt_pr_hit_vls(struct bu_vls *v, const char *str, register const struct hit *hitp)
{
    BU_CK_VLS(v);
    RT_CK_HIT(hitp);

    bu_log_indent_vls(v);
    bu_vls_strcat(v, str);

    bu_vls_printf(v, "HIT dist=%g (surf %d)\n",
		  hitp->hit_dist, hitp->hit_surfno);
}


void
rt_pr_hitarray_vls(struct bu_vls *v, const char *str, register const struct hit *hitp, int count)
{
    int i;

    BU_CK_VLS(v);
    RT_CK_HIT(hitp);

    bu_log_indent_vls(v);
    bu_vls_strcat(v, str);

    for (i=0; i<count; i++, hitp++) {
	bu_vls_printf(v, "HIT%d dist=%g (surf %d)\n", i,
		      hitp->hit_dist, hitp->hit_surfno);
    }
}


/**
 * Warning: This function uses recursion rather than iteration and a
 * stack, to preserve simplicity.  On machines with limited stack
 * space, such as the Gould, this subroutine may overwhelm the stack
 * on complex expressions.
 */
void
rt_pr_tree(register const union tree *tp, int lvl)

/* recursion level */
{
    register int i;

    RT_CK_TREE(tp);

    bu_log("%p ", (void *)tp);
    for (i=lvl; i>0; i--)
	bu_log("  ");

    if (tp == TREE_NULL) {
	bu_log("Null???\n");
	return;
    }

    switch (tp->tr_op) {

	case OP_NOP:
	    bu_log("NOP\n");
	    return;

	case OP_SOLID:
	    bu_log("SOLID %s (bit %ld)\n",
		   tp->tr_a.tu_stp->st_dp->d_namep,
		   tp->tr_a.tu_stp->st_bit);
	    return;

	case OP_REGION:
	    bu_log("REGION ctsp=%p\n", (void *)tp->tr_c.tc_ctsp);
	    db_pr_combined_tree_state(tp->tr_c.tc_ctsp);
	    return;

	case OP_DB_LEAF:
	    bu_log("DB_LEAF %s%s\n",
		   tp->tr_l.tl_name,
		   tp->tr_l.tl_mat ? " (matrix)" : "");
	    return;

	default:
	    bu_log("Unknown op=x%x\n", tp->tr_op);
	    return;

	case OP_UNION:
	    bu_log("UNION\n");
	    break;
	case OP_INTERSECT:
	    bu_log("INTERSECT\n");
	    break;
	case OP_SUBTRACT:
	    bu_log("MINUS\n");
	    break;
	case OP_XOR:
	    bu_log("XOR\n");
	    break;
	case OP_NOT:
	    bu_log("NOT\n");
	    break;
    }

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* BINARY type */
	    rt_pr_tree(tp->tr_b.tb_left, lvl+1);
	    rt_pr_tree(tp->tr_b.tb_right, lvl+1);
	    break;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* UNARY tree */
	    rt_pr_tree(tp->tr_b.tb_left, lvl+1);
	    break;
    }
}


/**
 * Produce a compact representation of this tree.  The destination vls
 * must be initialized by the caller.
 *
 * Operations are responsible for generating white space.
 */
void
rt_pr_tree_vls(struct bu_vls *vls, register const union tree *tp)
{
    char *str;

    if (tp == TREE_NULL) {
	bu_vls_strcat(vls, "??NULL_tree??");
	return;
    }

    switch (tp->tr_op) {

	case OP_NOP:
	    bu_vls_strcat(vls, "NOP");
	    return;

	case OP_SOLID:
	    bu_vls_strcat(vls, tp->tr_a.tu_stp->st_dp->d_namep);
	    return;

	case OP_REGION:
	    str = db_path_to_string(&(tp->tr_c.tc_ctsp->cts_p));
	    bu_vls_strcat(vls, str);
	    bu_free(str, "path string");
	    return;

	case OP_DB_LEAF:
	    bu_vls_strcat(vls, tp->tr_l.tl_name);
	    return;

	default:
	    bu_log("rt_pr_tree_vls() Unknown op=x%x\n", tp->tr_op);
	    return;

	case OP_UNION:
	    /* BINARY type */
	    bu_vls_strcat(vls, " (");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_left);
	    bu_vls_printf(vls, ") %c (", DB_OP_UNION);
	    rt_pr_tree_vls(vls, tp->tr_b.tb_right);
	    bu_vls_strcat(vls, ") ");
	    break;
	case OP_INTERSECT:
	    /* BINARY type */
	    bu_vls_strcat(vls, " (");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_left);
	    bu_vls_printf(vls, ") %c (", DB_OP_INTERSECT);
	    rt_pr_tree_vls(vls, tp->tr_b.tb_right);
	    bu_vls_strcat(vls, ") ");
	    break;
	case OP_SUBTRACT:
	    /* BINARY type */
	    bu_vls_strcat(vls, " (");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_left);
	    bu_vls_printf(vls, ") %c (", DB_OP_SUBTRACT);
	    rt_pr_tree_vls(vls, tp->tr_b.tb_right);
	    bu_vls_strcat(vls, ") ");
	    break;
	case OP_XOR:
	    /* BINARY type */
	    bu_vls_strcat(vls, " (");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_left);
	    bu_vls_strcat(vls, ") ^ (");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_right);
	    bu_vls_strcat(vls, ") ");
	    break;
	case OP_NOT:
	    /* UNARY tree */
	    bu_vls_strcat(vls, " !(");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_left);
	    bu_vls_strcat(vls, ") ");
	    break;
	case OP_GUARD:
	    /* UNARY tree */
	    bu_vls_strcat(vls, " guard(");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_left);
	    bu_vls_strcat(vls, ") ");
	    break;
	case OP_XNOP:
	    /* UNARY tree */
	    bu_vls_strcat(vls, " xnop(");
	    rt_pr_tree_vls(vls, tp->tr_b.tb_left);
	    bu_vls_strcat(vls, ") ");
	    break;
    }
}


/**
 * JRA's tree pretty-printer.  Formats the tree compactly into a
 * dynamically allocated string.  Uses recursion and lots of
 * malloc/free activity.
 */
char *
rt_pr_tree_str(const union tree *tree)
{
    char *left, *right;
    char *return_str;
    char op = OP_GUARD;
    size_t return_length;

    if (tree == NULL)
	return bu_strdup("NULL_ptr");
    RT_CK_TREE(tree);
    if (tree->tr_op == OP_UNION || tree->tr_op == OP_SUBTRACT || tree->tr_op == OP_INTERSECT) {
	char *blankl, *blankr;

	left = rt_pr_tree_str(tree->tr_b.tb_left);
	right = rt_pr_tree_str(tree->tr_b.tb_right);
	switch (tree->tr_op) {
	    case OP_UNION:
		op = DB_OP_UNION;
		break;
	    case OP_SUBTRACT:
		op = DB_OP_SUBTRACT;
		break;
	    case OP_INTERSECT:
		op = DB_OP_INTERSECT;
		break;
	}
	return_length = strlen(left) + strlen(right) + 8;
	return_str = (char *)bu_malloc(return_length, "rt_pr_tree_str: return string");

	blankl = strchr(left, ' ');
	blankr = strchr(right, ' ');
	if (blankl && blankr)
	    snprintf(return_str, return_length, "(%s) %c (%s)", left, op, right);
	else if (blankl && !blankr)
	    snprintf(return_str, return_length, "(%s) %c %s", left, op, right);
	else if (!blankl && blankr)
	    snprintf(return_str, return_length, "%s %c (%s)", left, op, right);
	else
	    snprintf(return_str, return_length, "%s %c %s", left, op, right);

	if (tree->tr_b.tb_left->tr_op != OP_DB_LEAF)
	    bu_free((void *)left, "rt_pr_tree_str: left string");
	if (tree->tr_b.tb_right->tr_op != OP_DB_LEAF)
	    bu_free((void *)right, "rt_pr_tree_str: right string");
	return return_str;
    } else if (tree->tr_op == OP_DB_LEAF)
	return bu_strdup(tree->tr_l.tl_name);
    else if (tree->tr_op == OP_REGION)
	return db_path_to_string(&tree->tr_c.tc_ctsp->cts_p);
    else if (tree->tr_op == OP_SOLID) {
	RT_CK_SOLTAB(tree->tr_a.tu_stp);
	return bu_strdup(tree->tr_a.tu_stp->st_dp->d_namep);
    }


    return bu_strdup("Unknown:tr_op");
}


/**
 * Print the actual values of the terms in a boolean expression.
 *
 * The values for pr_name determine the printing action:
 * 0 bit value
 * 1 name
 * 2 bit number
 */
void
rt_pr_tree_val(register const union tree *tp, const struct partition *partp, int pr_name, int lvl)
/* Tree to print */
/* Partition to evaluate */
/* 1=print name, 0=print value */
/* Recursion level */
{

    if (lvl == 0) {
	switch (pr_name) {
	    default:
		bu_log("tree val: ");
		break;
	    case 1:
		bu_log("tree primitives: ");
		break;
	    case 2:
		bu_log("tree primitive bits: ");
		break;
	}
    }

    if (tp == TREE_NULL) {
	bu_log("Null???\n");
	return;
    }

    switch (tp->tr_op) {
	default:
	    bu_log("Unknown_op=x%x", tp->tr_op);
	    break;

	case OP_SOLID:
	    switch (pr_name) {
		case 0:
		    {
			register struct soltab *seek_stp = tp->tr_a.tu_stp;
			register struct seg **segpp;
			for (BU_PTBL_FOR(segpp, (struct seg **), &partp->pt_seglist)) {
			    if ((*segpp)->seg_stp == seek_stp) {
				bu_log("1");
				goto out;
			    }
			}
			bu_log("0");
		    }
		    break;
		case 1:
		    bu_log("%s", tp->tr_a.tu_stp->st_dp->d_namep);
		    break;
		case 2:
		    bu_log("%ld", tp->tr_a.tu_stp->st_bit);
		    break;
	    }
	    break;


	case OP_UNION:
	    bu_log("(");
	    rt_pr_tree_val(tp->tr_b.tb_left,  partp, pr_name, lvl+1);
	    bu_log(" %c ", DB_OP_UNION);
	    rt_pr_tree_val(tp->tr_b.tb_right, partp, pr_name, lvl+1);
	    bu_log(")");
	    break;
	case OP_INTERSECT:
	    bu_log("(");
	    rt_pr_tree_val(tp->tr_b.tb_left,  partp, pr_name, lvl+1);
	    bu_log(" %c ", DB_OP_INTERSECT);
	    rt_pr_tree_val(tp->tr_b.tb_right, partp, pr_name, lvl+1);
	    bu_log(")");
	    break;
	case OP_SUBTRACT:
	    bu_log("(");
	    rt_pr_tree_val(tp->tr_b.tb_left,  partp, pr_name, lvl+1);
	    bu_log(" %c ", DB_OP_SUBTRACT);
	    rt_pr_tree_val(tp->tr_b.tb_right, partp, pr_name, lvl+1);
	    bu_log(")");
	    break;
	case OP_XOR:
	    bu_log("(");
	    rt_pr_tree_val(tp->tr_b.tb_left,  partp, pr_name, lvl+1);
	    bu_log(" XOR ");
	    rt_pr_tree_val(tp->tr_b.tb_right, partp, pr_name, lvl+1);
	    bu_log(")");
	    break;

	case OP_NOT:
	    bu_log(" !");
	    rt_pr_tree_val(tp->tr_b.tb_left, partp, pr_name, lvl+1);
	    break;
	case OP_GUARD:
	    bu_log(" GUARD ");
	    rt_pr_tree_val(tp->tr_b.tb_left, partp, pr_name, lvl+1);
	    break;
    }

out:
    if (lvl == 0) bu_log("\n");
}


void
rt_pr_fallback_angle(struct bu_vls *str, const char *prefix, const double *angles)
{
    BU_CK_VLS(str);

    bu_vls_printf(str, "%s direction cosines=(%1.f, %1.f, %1.f)\n",
		  prefix, INTCLAMP(angles[0]), INTCLAMP(angles[1]), INTCLAMP(angles[2]));

    bu_vls_printf(str, "%s rotation angle=%1.f, fallback angle=%1.f\n",
		  prefix, INTCLAMP(angles[3]), INTCLAMP(angles[4]));
}


/**
 * In degrees.
 */
void
rt_find_fallback_angle(double *angles, const fastf_t *vec)
{
    register double f;
    double asinZ;

    /* convert direction cosines into axis angles */
    if (vec[X] <= -1.0) {
	angles[X] = 180.0;
    } else if (vec[X] >= 1.0) {
	angles[X] = 0.0;
    } else {
	angles[X] = acos(vec[X]) * RAD2DEG;
    }

    if (vec[Y] <= -1.0) {
	angles[Y] = 180.0;
    } else if (vec[Y] >= 1.0) {
	angles[Y] = 0.0;
    } else {
	angles[Y] = acos(vec[Y]) * RAD2DEG;
    }

    if (vec[Z] <= -1.0) {
	angles[Z] = 180.0;
    } else if (vec[Z] >= 1.0) {
	angles[Z] = 0.0;
    } else {
	angles[Z] = acos(vec[Z]) * RAD2DEG;
    }

    /* fallback angle */
    if (vec[Z] <= -1.0) {
	/* 270 degrees:  3/2 pi */
	asinZ = M_PI_2 * 3;
    } else if (vec[Z] >= 1.0) {
	/* +90 degrees: 1/2 pi */
	asinZ = M_PI_2;
    } else {
	asinZ = asin(vec[Z]);
    }
    angles[4] = asinZ * RAD2DEG;

    /* rotation angle */
    /* For the tolerance below, on an SGI 4D/70, cos(asin(1.0)) != 0.0
     * with an epsilon of +/- 1.0e-17, so the tolerance below was
     * substituted for the original +/- 1.0e-20.
     */
    if ((f = cos(asinZ)) > 1.0e-16 || f < -1.0e-16) {
	f = vec[X]/f;
	if (f <= -1.0) {
	    angles[3] = 180;
	} else if (f >= 1.0) {
	    angles[3] = 0;
	} else {
	    angles[3] = RAD2DEG * acos(f);
	}
    } else {
	angles[3] = 0.0;
    }
    if (vec[Y] < 0) {
	angles[3] = 360.0 - angles[3];
    }
}


/**
 * Print a tolerance structure.
 */
void
rt_pr_tol(const struct bn_tol *tol)
{
    BN_CK_TOL(tol);

    bu_log("%p TOL %e (sq=%e) perp=%e, para=%e\n",
	   (void *)tol, tol->dist, tol->dist_sq,
	   tol->perp, tol->para);
}


void
rt_pr_uvcoord(const struct uvcoord *uvp)
{
    bu_log("u, v=(%g, %g), du, dv=(%g, %g)\n",
	   INTCLAMP(uvp->uv_u), INTCLAMP(uvp->uv_v),
	   INTCLAMP(uvp->uv_du), INTCLAMP(uvp->uv_dv));
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
