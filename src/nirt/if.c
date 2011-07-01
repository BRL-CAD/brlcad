/*                            I F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file nirt/if.c
 *
 * This program is an Interactive Ray-Tracer
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "bu.h"

#include "./nirt.h"
#include "./usrfmt.h"


extern outval ValTab[];
extern int nirt_debug;
extern int overlap_claims;
extern double base2local;
extern double local2base;
overlap ovlp_list;

overlap *find_ovlp(struct partition *pp);
void del_ovlp(overlap *op);
void init_ovlp(void);

int
if_hit(struct application *ap, struct partition *part_head, struct seg *finished_segs)
{
    struct partition *part;
    fastf_t ar = azimuth() * DEG2RAD;
    fastf_t er = elevation() * DEG2RAD;
    int i;
    int part_nm = 0;
    overlap *ovp;	/* the overlap record for this partition */
    point_t inormal;
    point_t onormal;
    struct bu_vls claimant_list;	/* Names of the claiming regions */
    int need_to_free = 0;	/* Clean up the bu_vls? */
    fastf_t get_obliq(fastf_t *ray, fastf_t *normal);
    struct bu_vls attr_vls;
    char regionPN[512] = {0};
    struct bu_attribute_value_pair *avpp;

    /* quellage */
    finished_segs = finished_segs;

    report(FMT_RAY);
    report(FMT_HEAD);
    if (overlap_claims == OVLP_REBUILD_FASTGEN)
	rt_rebuild_overlaps(part_head, ap, 1);
    else if (overlap_claims == OVLP_REBUILD_ALL)
	rt_rebuild_overlaps(part_head, ap, 0);

    for (part = part_head->pt_forw; part != part_head; part = part->pt_forw) {
	++part_nm;

	RT_HIT_NORMAL(inormal, part->pt_inhit, part->pt_inseg->seg_stp,
		      &ap->a_ray, part->pt_inflip);
	RT_HIT_NORMAL(onormal, part->pt_outhit, part->pt_outseg->seg_stp,
		      &ap->a_ray, part->pt_outflip);

	/* Update the output values */
	/*
	 * WARNING - target, grid, direct, az, and el should be
	 * updated by the functions in command.c as well
	 */
	if (part_nm > 1) {
	    for (i = 0; i < 3; ++i) {
		g_entry(i) = r_exit(i);
	    }
	    g_entry(D) = r_exit(D);
	}
	for (i = 0; i < 3; ++i) {
	    r_entry(i) = part-> pt_inhit->hit_point[i];
	    r_exit(i) = part-> pt_outhit->hit_point[i];
	    n_entry(i) = inormal[i];
	    n_exit(i) = onormal[i];
	}
	if (nirt_debug & DEBUG_HITS) {
	    bu_log("Partition %d entry: (%g, %g, %g) exit: (%g, %g, %g)\n",
		   part_nm, r_entry(X), r_entry(Y), r_entry(Z),
		   r_exit(X),  r_exit(Y),  r_exit(Z));
	}

	r_entry(D) = r_entry(X) * cos(er) * cos(ar)
	    + r_entry(Y) * cos(er) * sin(ar)
	    + r_entry(Z) * sin(er);
	r_exit(D) = r_exit(X) * cos(er) * cos(ar)
	    + r_exit(Y) * cos(er) * sin(ar)
	    + r_exit(Z) * sin(er);
	n_entry(D) = n_entry(X) * cos(er) * cos(ar)
	    + n_entry(Y) * cos(er) * sin(ar)
	    + n_entry(Z) * sin(er);
	n_entry(H) = n_entry(X) * (-sin(ar))
	    + n_entry(Y) * cos(ar);
	n_entry(V) = n_entry(X) * (-sin(er)) * cos(ar)
	    + n_entry(Y) * (-sin(er)) * sin(ar)
	    + n_entry(Z) * cos(er);
	n_exit(D) = n_exit(X) * cos(er) * cos(ar)
	    + n_exit(Y) * cos(er) * sin(ar)
	    + n_exit(Z) * sin(er);
	n_exit(H) = n_exit(X) * (-sin(ar))
	    + n_exit(Y) * cos(ar);
	n_exit(V) = n_exit(X) * (-sin(er)) * cos(ar)
	    + n_exit(Y) * (-sin(er)) * sin(ar)
	    + n_exit(Z) * cos(er);
	ValTab[VTI_LOS].value.fval = r_entry(D) - r_exit(D);
	ValTab[VTI_SLOS].value.fval = 0.01 * ValTab[VTI_LOS].value.fval *
	    part->pt_regionp->reg_los;
	if (part_nm > 1) {
	    ValTab[VTI_GAP_LOS].value.fval = g_entry(D) - r_entry(D);
	    if (ValTab[VTI_GAP_LOS].value.fval > 0) report(FMT_GAP);
	}
	bu_strlcpy(regionPN, part->pt_regionp->reg_name, sizeof(regionPN));

	ValTab[VTI_PATH_NAME].value.sval = part->pt_regionp->reg_name;
	ValTab[VTI_REG_NAME].value.sval = bu_basename(regionPN);
	ValTab[VTI_REG_ID].value.ival = part->pt_regionp->reg_regionid;
	ValTab[VTI_SURF_NUM_IN].value.ival = part->pt_inhit->hit_surfno;
	ValTab[VTI_SURF_NUM_OUT].value.ival = part->pt_outhit->hit_surfno;
	ValTab[VTI_OBLIQ_IN].value.fval =
	    get_obliq(ap->a_ray.r_dir, inormal);
	ValTab[VTI_OBLIQ_OUT].value.fval =
	    get_obliq(ap->a_ray.r_dir, onormal);

	if (part->pt_overlap_reg == 0) {
	    ValTab[VTI_CLAIMANT_COUNT].value.ival = 1;
	    ValTab[VTI_CLAIMANT_LIST].value.sval =
		ValTab[VTI_CLAIMANT_LISTN].value.sval =
		ValTab[VTI_REG_NAME].value.sval;
	} else {
	    struct region **rpp;
	    char *cp;

	    bu_vls_init(&claimant_list);
	    ValTab[VTI_CLAIMANT_COUNT].value.ival = 0;
	    for (rpp = part->pt_overlap_reg; *rpp != REGION_NULL; ++rpp) {
		char tmpcp[512] = {0};
		char *base = NULL;

		if (ValTab[VTI_CLAIMANT_COUNT].value.ival++)
		    bu_vls_strcat(&claimant_list, " ");
		bu_strlcpy(tmpcp, (*rpp)->reg_name, sizeof(tmpcp));
		
		base = bu_basename(tmpcp);
		bu_vls_strcat(&claimant_list, base);
		bu_free(base, "bu_basename");
	    }
	    ValTab[VTI_CLAIMANT_LIST].value.sval =
		bu_vls_addr(&claimant_list);

	    for (cp = bu_vls_strdup(&claimant_list);
		 *cp != '\0'; ++cp)
		if (*cp == ' ')
		    *cp = '\n';

	    ValTab[VTI_CLAIMANT_LISTN].value.sval = cp;

	    need_to_free = 1;
	}

	/* format up the attribute strings into a single string */
	bu_vls_init(&attr_vls);
	for (BU_AVS_FOR(avpp, &(part->pt_regionp->attr_values))) {
		if (avpp->value)
			if (strlen(avpp->value) != 0)
				bu_vls_printf(&attr_vls, "%s=%s", avpp->name, avpp->value);
	}

	ValTab[VTI_ATTRIBUTES].value.sval = bu_vls_addr(&attr_vls);

	/* Do the printing for this partition */
	report(FMT_PART);

	if (need_to_free) {
	    bu_vls_free(&claimant_list);
	    bu_free((genptr_t)ValTab[VTI_CLAIMANT_LISTN].value.sval, "returned by bu_vls_strdup");
	    need_to_free = 0;
	}

	while ((ovp = find_ovlp(part)) != OVERLAP_NULL) {
#ifdef NIRT_OVLP_PATH
	    ValTab[VTI_OV_REG1_NAME].value.sval = ovp->reg1->reg_name;
	    ValTab[VTI_OV_REG2_NAME].value.sval = ovp->reg2->reg_name;
#else
	    char *copy_ovlp_reg1 = bu_strdup(ovp->reg1->reg_name);
	    char *copy_ovlp_reg2 = bu_strdup(ovp->reg2->reg_name);

	    ValTab[VTI_OV_REG1_NAME].value.sval = bu_basename(copy_ovlp_reg1);
	    ValTab[VTI_OV_REG2_NAME].value.sval = bu_basename(copy_ovlp_reg2);
#endif
	    ValTab[VTI_OV_REG1_ID].value.ival = ovp->reg1->reg_regionid;
	    ValTab[VTI_OV_REG2_ID].value.ival = ovp->reg2->reg_regionid;
	    ValTab[VTI_OV_SOL_IN].value.sval =
		(char *)(part->pt_inseg->seg_stp->st_dp->d_namep);
	    ValTab[VTI_OV_SOL_OUT].value.sval =
		(char *)(part->pt_outseg->seg_stp->st_dp->d_namep);

	    for (i = 0; i < 3; ++i) {
		ov_entry(i) = ovp->in_point[i];
		ov_exit(i) = ovp->out_point[i];
	    }

	    ov_entry(D) = target(D) - ovp->in_dist;
	    ov_exit(D) = target(D) - ovp->out_dist;
	    ValTab[VTI_OV_LOS].value.fval = ov_entry(D) - ov_exit(D);
	    report(FMT_OVLP);

#ifndef NIRT_OVLP_PATH
	    bu_free((genptr_t)copy_ovlp_reg1, "copy_ovlp_reg1");
	    bu_free((genptr_t)copy_ovlp_reg2, "copy_ovlp_reg2");
#endif

	    del_ovlp(ovp);
	}
    }
    report(FMT_FOOT);

    if (ovlp_list.forw != &ovlp_list) {
	fprintf(stderr, "Previously unreported overlaps.  Shouldn't happen\n");
	ovp = ovlp_list.forw;
	while (ovp != &ovlp_list) {
	    bu_log(" OVERLAP:\n\t%s %s (%g %g %g) %g\n", ovp->reg1->reg_name, ovp->reg2->reg_name, V3ARGS(ovp->in_point), ovp->out_dist - ovp->in_dist);
	    ovp = ovp->forw;
	}
    }

    bu_vls_free(&attr_vls);

    return HIT;
}


int
if_miss(void)
{
    report(FMT_RAY);
    report(FMT_MISS);
    return MISS;
}


/**
 * I F _ O V E R L A P
 *
 * Default handler for overlaps in rt_boolfinal().
 * Returns -
 *  0 to eliminate partition with overlap entirely
 * !0 to retain partition in output list
 *
 */
int
if_overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *InputHdp)
{
    overlap *new_ovlp;

    /* N. B. bu_malloc() only returns on successful allocation */
    new_ovlp = (overlap *) bu_malloc(sizeof(overlap), "new_ovlp");

    new_ovlp->ap = ap;
    new_ovlp->pp = pp;
    new_ovlp->reg1 = reg1;
    new_ovlp->reg2 = reg2;
    new_ovlp->in_dist = pp->pt_inhit->hit_dist;
    new_ovlp->out_dist = pp->pt_outhit->hit_dist;
    VJOIN1(new_ovlp->in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
	   ap->a_ray.r_dir);
    VJOIN1(new_ovlp->out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
	   ap->a_ray.r_dir);

    /* Insert the new overlap into the list of overlaps */
    new_ovlp->forw = ovlp_list.forw;
    new_ovlp->backw = &ovlp_list;
    new_ovlp->forw->backw = new_ovlp;
    ovlp_list.forw = new_ovlp;

    /* Match current BRL-CAD default behavior */
    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}


fastf_t
get_obliq(fastf_t *ray, fastf_t *normal)
{
    fastf_t cos_obl;
    fastf_t obliquity;

    cos_obl = fabs(VDOT(ray, normal) * MAGNITUDE(normal) / MAGNITUDE(ray));
    if (cos_obl < 1.001) {
	if (cos_obl > 1)
	    cos_obl = 1;
	obliquity = acos(cos_obl);
    } else {
	fflush(stdout);
	fprintf (stderr, "Error:  cos(obliquity) > 1 (%g)\n", cos_obl);
	obliquity = 0;
	bu_exit(1, NULL);
    }

    /* convert obliquity to degrees */
    obliquity = fabs(obliquity * RAD2DEG);
    if (obliquity > 90 && obliquity <= 180)
	obliquity = 180 - obliquity;
    else if (obliquity > 180 && obliquity <= 270)
	obliquity = obliquity - 180;
    else if (obliquity > 270 && obliquity <= 360)
	obliquity = 360 - obliquity;

    return obliquity;
}


overlap *
find_ovlp(struct partition *pp)
{
    overlap *op;

    for (op = ovlp_list.forw; op != &ovlp_list; op = op->forw) {
	if (((pp->pt_inhit->hit_dist <= op->in_dist)
	     && (op->in_dist <= pp->pt_outhit->hit_dist)) ||
	    ((pp->pt_inhit->hit_dist <= op->out_dist)
	     && (op->in_dist <= pp->pt_outhit->hit_dist)))
	    break;
    }
    return (op == &ovlp_list) ? OVERLAP_NULL : op;
}


void
del_ovlp(overlap *op)
{
    op->forw->backw = op->backw;
    op->backw->forw = op->forw;
    bu_free((char *)op, "free op in del_ovlp");
}


void
init_ovlp(void)
{
    ovlp_list.forw = ovlp_list.backw = &ovlp_list;
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
