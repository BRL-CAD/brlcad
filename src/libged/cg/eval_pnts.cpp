/*                   E V A L _ P N T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file eval_pnts.cpp
 *
 * Brief description
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
#include "./ged_cg.h"
}

extern "C" int
pnts_eval_op(
	struct ged *gedp,
	db_op_t op,
	struct rt_db_internal *tpnts_intern,
	const char *vol_obj,
	const char *output_pnts_obj
	)
{
    int ret = GED_OK;
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
    struct rt_pnts_internal *opnts = NULL;
    struct rt_db_internal internal;
    long pntcnt = 0;
    struct rt_pnts_internal *pnts = (struct rt_pnts_internal *)tpnts_intern->idb_ptr;
    RT_PNTS_CK_MAGIC(pnts);

    /* For the vol object, we need to raytrace it */
    struct application *ap;
    struct resource *resp;
    struct rt_i *rtip;
    size_t ncpus;
    BU_GET(ap, struct application);
    RT_APPLICATION_INIT(ap);
    BU_GET(resp, struct resource);
    rtip = rt_new_rti(gedp->ged_wdbp->dbip);
    rt_init_resource(resp, 0, rtip);
    ap->a_rt_i = rtip;
    ap->a_resource = resp;
    ap->a_onehit = 1;
    ap->a_hit = NULL;
    ap->a_miss = NULL;
    ap->a_overlap = NULL;
    ap->a_logoverlap = rt_silent_logoverlap;
    if ((rt_gettree(rtip, vol_obj) < 0)) {
	ret = GED_ERROR;
	goto pnts_internal_memfree;
    }
    ncpus = bu_avail_cpus();
    rt_prep_parallel(rtip, ncpus);

    /* For the output, make a new pnts object */
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

    switch(pnts->type) {
	case RT_PNT_TYPE_PNT:
	    pstdl = (struct pnt *)pnts->point;
	    for (BU_LIST_FOR(pstd, pnt, &(pstdl->l))) {
		iostat = analyze_pnt_in_vol(&(pstd->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)pstd, RT_PNT_TYPE_PNT);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL:
	    pcl = (struct pnt_color *)pnts->point;
	    for (BU_LIST_FOR(pc, pnt_color, &(pcl->l))) {
		iostat = analyze_pnt_in_vol(&(pc->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)pc, RT_PNT_TYPE_COL);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_SCA:
	    psl = (struct pnt_scale *)pnts->point;
	    for (BU_LIST_FOR(ps, pnt_scale, &(psl->l))) {
		iostat = analyze_pnt_in_vol(&(ps->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)ps, RT_PNT_TYPE_SCA);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_NRM:
	    pnl = (struct pnt_normal *)pnts->point;
	    for (BU_LIST_FOR(pn, pnt_normal, &(pnl->l))) {
		iostat = analyze_pnt_in_vol(&(pn->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)pn, RT_PNT_TYPE_NRM);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    pcsl = (struct pnt_color_scale *)pnts->point;
	    for (BU_LIST_FOR(pcs, pnt_color_scale, &(pcsl->l))) {
		iostat = analyze_pnt_in_vol(&(pcs->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)pcs, RT_PNT_TYPE_COL_SCA);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    pcnl = (struct pnt_color_normal *)pnts->point;
	    for (BU_LIST_FOR(pcn, pnt_color_normal, &(pcnl->l))) {
		iostat = analyze_pnt_in_vol(&(pcn->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)pcn, RT_PNT_TYPE_COL_NRM);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    psnl = (struct pnt_scale_normal *)pnts->point;
	    for (BU_LIST_FOR(psn, pnt_scale_normal, &(psnl->l))) {
		iostat = analyze_pnt_in_vol(&(psn->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)psn, RT_PNT_TYPE_SCA_NRM);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    pcsnl = (struct pnt_color_scale_normal *)pnts->point;
	    for (BU_LIST_FOR(pcsn, pnt_color_scale_normal, &(pcsnl->l))) {
		iostat = analyze_pnt_in_vol(&(pcsn->v), ap, 1);
		if ((op == DB_OP_INTERSECT && iostat == -1) || (op == DB_OP_SUBTRACT && iostat != -1)) {
		    npnt = _ged_pnts_dup((void *)pcsn, RT_PNT_TYPE_COL_SCA_NRM);
		    _ged_pnts_add(opnts, npnt);
		    pntcnt++;
		}
	    }
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Unknown point type in object, aborting\n");
	    ret = GED_ERROR;
	    goto pnts_internal_memfree;
    };

    opnts->count = pntcnt;
    struct directory *dp;
    GED_DB_DIRADD(gedp, dp, output_pnts_obj, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    bu_vls_printf(gedp->ged_result_str, "Generated pnts object %s with %ld matching points", output_pnts_obj, pntcnt);

pnts_internal_memfree:
    rt_clean_resource(rtip, resp);
    rt_free_rti(rtip);
    BU_PUT(resp, struct resource);
    BU_PUT(ap, struct appliation);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
