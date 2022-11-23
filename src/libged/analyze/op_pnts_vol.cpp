/*                 O P _ P N T S _ V O L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file op_pnts_vol.cpp
 *
 * Test all points in a pnts object for inside/outside status against
 * a CSG volume.
 *
 */

#include "common.h"

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

extern "C" {
#include "rt/geom.h"
#include "analyze.h"
#include "../ged_private.h"
#include "../pnts_util.h"
#include "./ged_analyze.h"
}

struct ray_result {
    point_t *p;
    double dist_test_pt;
    int flag;
};

static void
_tgc_hack_fix(struct partition *part, struct soltab *stp) {
    /* hack fix for bad tgc surfaces - avoids a logging crash, which is probably something else altogether... */
    if (bu_strncmp("rec", stp->st_meth->ft_label, 3) == 0 || bu_strncmp("tgc", stp->st_meth->ft_label, 3) == 0) {

	/* correct invalid surface number */
	if (part->pt_inhit->hit_surfno < 1 || part->pt_inhit->hit_surfno > 3) {
	    part->pt_inhit->hit_surfno = 2;
	}
	if (part->pt_outhit->hit_surfno < 1 || part->pt_outhit->hit_surfno > 3) {
	    part->pt_outhit->hit_surfno = 2;
	}
    }
}


static int
in_out_hit(struct application *ap, struct partition *partH, struct seg *UNUSED(segs))
{
    struct partition *part = partH->pt_forw;
    struct soltab *stp = part->pt_inseg->seg_stp;

    struct ray_result *r= (struct ray_result *)(ap->a_uptr);

    RT_CK_APPLICATION(ap);

    _tgc_hack_fix(part, stp);

    // Any partition containing the test point will have a hit distance less than
    // or at the test point.  If we find such a partition, set the flag.
    bool t1 = ((part->pt_inhit->hit_dist < r->dist_test_pt) || NEAR_EQUAL(part->pt_inhit->hit_dist, r->dist_test_pt, VUNITIZE_TOL));
    bool t2 = ((part->pt_outhit->hit_dist > r->dist_test_pt) || NEAR_EQUAL(part->pt_outhit->hit_dist, r->dist_test_pt, VUNITIZE_TOL));
    if (t1 && t2) {
	r->flag = -1;
    }

    // Test point not on the partition
    return 0;
}

static int
in_out_miss(struct application *UNUSED(ap))
{
    return 0;
}

static double _backout(struct application *ap, point_t *p)
{
    double bov;
    point_t ray_point;
    vect_t diag, dvec, ray_dir, center_bsphere;
    fastf_t bsphere_diameter, dist_to_target, delta;

    VMOVE(ray_point, *p);
    VSET(ray_dir, 0, 0, 1);

    VSUB2(diag, ap->a_rt_i->mdl_max, ap->a_rt_i->mdl_min);
    bsphere_diameter = MAGNITUDE(diag);

    /*
     * calculate the distance from a plane normal to the ray direction through the center of
     * the bounding sphere and a plane normal to the ray direction through the aim point.
     */
    VADD2SCALE(center_bsphere, ap->a_rt_i->mdl_max, ap->a_rt_i->mdl_min, 0.5);

    dist_to_target = DIST_PNT_PNT(center_bsphere, ray_point);

    VSUB2(dvec, ray_point, center_bsphere);
    VUNITIZE(dvec);
    delta = dist_to_target*VDOT(ray_dir, dvec);

    /*
     * this should put us about a bounding sphere radius in front of the bounding sphere
     */
    bov = bsphere_diameter + delta;

    return bov;
}


// Return 0 if not inside, 1 if (approximately) on and -1 if inside the volume.
static int
_pnt_in_vol(point_t *p, struct application *ap)
{
    int (*a_hit)(struct application *, struct partition *, struct seg *);
    int (*a_miss)(struct application *);
    void *uptr_stash;
    struct ray_result result;
    result.flag = 0;
    result.p = p;

    vect_t pz;
    VSET(pz, 0, 0, 1);

    /* reuse existing application, just cache pre-existing hit routines and
     * substitute our own */
    a_hit = ap->a_hit;
    a_miss = ap->a_miss;
    uptr_stash = ap->a_uptr;

    ap->a_hit = in_out_hit;
    ap->a_miss = in_out_miss;

    VMOVE(ap->a_ray.r_pt, *p);

    double bov = _backout(ap, p);
    for (int i = 0; i < 3; i++) {
	ap->a_ray.r_pt[i] = ap->a_ray.r_pt[i] + (bov * -1*(pz[i]));
    }
    result.dist_test_pt = DIST_PNT_PNT(ap->a_ray.r_pt, *p);

    ap->a_uptr = &result;
    VMOVE(ap->a_ray.r_dir, pz);
    (void)rt_shootray(ap);

    /* restore application */
    ap->a_hit = a_hit;
    ap->a_miss = a_miss;
    ap->a_uptr = uptr_stash;

    return result.flag;
}


extern "C" long
op_pnts_vol(
    const char *output_pnts_obj,
    struct ged *gedp,
    db_op_t op,
    const char *pnts_obj,
    const char *vol_obj
	   )
{
    struct pnt *pstd, *pstdl = NULL;
    struct pnt_color *pc, *pcl = NULL;
    struct pnt_scale *ps, *psl = NULL;
    struct pnt_normal *pn, *pnl = NULL;
    struct pnt_color_scale *pcs, *pcsl = NULL;
    struct pnt_color_normal *pcn, *pcnl = NULL;
    struct pnt_scale_normal *psn, *psnl = NULL;
    struct pnt_color_scale_normal *pcsn, *pcsnl = NULL;
    void *npnt = NULL;
    int iostat = 0;
    struct rt_db_internal internal;
    long pntcnt = 0;
    struct rt_pnts_internal *opnts = NULL;

    // If we're supposed to make an output object, don't do anything
    // until we're sure the way is clear.
    if (output_pnts_obj) {
	struct directory *odp = db_lookup(gedp->dbip, output_pnts_obj, LOOKUP_QUIET);
	if (odp != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists, aborting", output_pnts_obj);
	    return -1;
	}
    }

    // Unpack the points object
    struct directory *dp = db_lookup(gedp->dbip, pnts_obj, LOOKUP_QUIET);
    struct rt_db_internal tpnts_intern;
    GED_DB_GET_INTERNAL(gedp, &tpnts_intern, dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
    if (tpnts_intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PNTS) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a pnts object, aborting", pnts_obj);
	rt_db_free_internal(&tpnts_intern);
	return -1;
    }
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)tpnts_intern.idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    // Since we need to verify type info about vol_obj, look it up
    if (vol_obj) {
	dp = db_lookup(gedp->dbip, vol_obj, LOOKUP_QUIET);
    }
    // If we're missing the volume object, union or subtraction result
    // in the point set being unchanged.
    if (!dp && (op == DB_OP_UNION || op == DB_OP_SUBTRACT)) {
	if (output_pnts_obj) {
	    const char *av[3];
	    av[0] = "copy";
	    av[1] = pnts_obj;
	    av[2] = output_pnts_obj;
	    (void)ged_exec(gedp, 3, (const char **)av);
	}
	rt_db_free_internal(&tpnts_intern);
	return pnts->count;
    }
    // Intersecting with nothing yields nothing
    if (!dp && op == DB_OP_INTERSECT) {
	rt_db_free_internal(&tpnts_intern);
	return 0;
    }

    /* Verify the object supplied as the volume object has
     * non non-volumetric object types in its hierarchy.  If
     * there is such an object, abandon the test.  */
    const char *tfilter = "! -type shape";
    if (db_search(NULL, DB_SEARCH_QUIET, tfilter, 1, &dp, gedp->dbip, NULL) > 0) {
	bu_vls_printf(gedp->ged_result_str, "Non-solid object found in %s, aborting\n", vol_obj);
	rt_db_free_internal(&tpnts_intern);
	return -1;
    }

    /* For the volume object, we need to raytrace it */
    struct application *ap;
    struct resource *resp;
    struct rt_i *rtip;
    size_t ncpus;
    BU_GET(ap, struct application);
    RT_APPLICATION_INIT(ap);
    BU_GET(resp, struct resource);
    rtip = rt_new_rti(gedp->dbip);
    rt_init_resource(resp, 0, rtip);
    ap->a_rt_i = rtip;
    ap->a_resource = resp;
    ap->a_onehit = 1;
    ap->a_hit = NULL;
    ap->a_miss = NULL;
    ap->a_overlap = NULL;
    ap->a_logoverlap = rt_silent_logoverlap;
    if ((rt_gettree(rtip, vol_obj) < 0)) {
	pntcnt = -1;
	goto pnts_internal_memfree;
    }
    ncpus = bu_avail_cpus();
    rt_prep_parallel(rtip, ncpus);

    /* For the output, make a new pnts object */
    if (output_pnts_obj) {
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_type = ID_PNTS;
	internal.idb_meth = &OBJ[ID_PNTS];
	BU_ALLOC(internal.idb_ptr, struct rt_pnts_internal);
	opnts = (struct rt_pnts_internal *) internal.idb_ptr;
	opnts->magic = RT_PNTS_INTERNAL_MAGIC;
	opnts->scale = pnts->scale;
	opnts->type = pnts->type;
	opnts->point = _ged_pnts_new_pnt(pnts->type);
	_ged_pnts_init_head_pnt(opnts);
    }

    switch(pnts->type) {
	case RT_PNT_TYPE_PNT:
	    pstdl = (struct pnt *)pnts->point;
	    for (BU_LIST_FOR(pstd, pnt, &(pstdl->l))) {
		iostat = _pnt_in_vol(&(pstd->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)pstd, RT_PNT_TYPE_PNT);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL:
	    pcl = (struct pnt_color *)pnts->point;
	    for (BU_LIST_FOR(pc, pnt_color, &(pcl->l))) {
		iostat = _pnt_in_vol(&(pc->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)pc, RT_PNT_TYPE_COL);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_SCA:
	    psl = (struct pnt_scale *)pnts->point;
	    for (BU_LIST_FOR(ps, pnt_scale, &(psl->l))) {
		iostat = _pnt_in_vol(&(ps->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)ps, RT_PNT_TYPE_SCA);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_NRM:
	    pnl = (struct pnt_normal *)pnts->point;
	    for (BU_LIST_FOR(pn, pnt_normal, &(pnl->l))) {
		iostat = _pnt_in_vol(&(pn->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)pn, RT_PNT_TYPE_NRM);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    pcsl = (struct pnt_color_scale *)pnts->point;
	    for (BU_LIST_FOR(pcs, pnt_color_scale, &(pcsl->l))) {
		iostat = _pnt_in_vol(&(pcs->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)pcs, RT_PNT_TYPE_COL_SCA);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    pcnl = (struct pnt_color_normal *)pnts->point;
	    for (BU_LIST_FOR(pcn, pnt_color_normal, &(pcnl->l))) {
		iostat = _pnt_in_vol(&(pcn->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)pcn, RT_PNT_TYPE_COL_NRM);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    psnl = (struct pnt_scale_normal *)pnts->point;
	    for (BU_LIST_FOR(psn, pnt_scale_normal, &(psnl->l))) {
		iostat = _pnt_in_vol(&(psn->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)psn, RT_PNT_TYPE_SCA_NRM);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    pcsnl = (struct pnt_color_scale_normal *)pnts->point;
	    for (BU_LIST_FOR(pcsn, pnt_color_scale_normal, &(pcsnl->l))) {
		iostat = _pnt_in_vol(&(pcsn->v), ap);
		if ((op == DB_OP_INTERSECT && iostat) || (op == DB_OP_SUBTRACT && !iostat)) {
		    if (output_pnts_obj) {
			npnt = _ged_pnts_dup((void *)pcsn, RT_PNT_TYPE_COL_SCA_NRM);
			_ged_pnts_add(opnts, npnt);
		    }
		    pntcnt++;
		}
	    }
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Unknown point type in object, aborting\n");
	    pntcnt = -1;
	    goto pnts_internal_memfree;
    };

    if (output_pnts_obj) {
	opnts->count = pntcnt;
	GED_DB_DIRADD(gedp, dp, output_pnts_obj, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
	GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, BRLCAD_ERROR);

	bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s\n", output_pnts_obj);
    }

pnts_internal_memfree:
    rt_clean_resource(rtip, resp);
    rt_free_rti(rtip);
    rt_db_free_internal(&tpnts_intern);
    BU_PUT(resp, struct resource);
    BU_PUT(ap, struct appliation);

    return pntcnt;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
