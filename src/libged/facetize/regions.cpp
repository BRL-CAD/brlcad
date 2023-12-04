/*                     R E G I O N S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize.cpp
 *
 * The facetize command.
 *
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>

#include <string.h>

#include "bu/env.h"
#include "rt/search.h"
#include "raytrace.h"
#include "wdb.h"
#include "../ged_private.h"
#include "./ged_facetize.h"


struct _ged_facetize_report_info {
    double feature_size;
    double avg_thickness;
    double obj_bbox_vol;
    double pnts_bbox_vol;
    double bot_bbox_vol;
    int failure_mode;
};
#define FACETIZE_REPORT_INFO_INIT {0.0, 0.0, 0.0, 0.0, 0.0, 0}

void
_ged_facetize_failure_msg(struct bu_vls *msg, int type, const char *prefix, struct _ged_facetize_report_info *r)
{
    if (!msg) return;
    switch (type) {
	case FACETIZE_SUCCESS:
	    bu_vls_printf(msg, "%s: success\n", prefix);
	    break;
	case FACETIZE_FAILURE:
	    bu_vls_printf(msg, "%s: unknown failure\n", prefix);
	    break;
	case FACETIZE_FAILURE_PNTGEN:
	    bu_vls_printf(msg, "%s: point generation failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_PNTBBOX:
	    bu_vls_printf(msg, "%s: object bbox volume (%g) is widely different than the sampled point cloud bbox volume (%g).\n", prefix, r->obj_bbox_vol, r->pnts_bbox_vol);
	    break;
	case FACETIZE_FAILURE_BOTBBOX:
	    bu_vls_printf(msg, "%s: BoT bbox volume (%g) is widely different than the sampled point cloud bbox volume (%g).\n", prefix, r->bot_bbox_vol, r->pnts_bbox_vol);
	    break;
	case FACETIZE_FAILURE_BOTINVALID:
	    bu_vls_printf(msg, "%s: unable to create a valid BoT.\n", prefix);
	    break;
	case FACETIZE_FAILURE_DECIMATION:
	    bu_vls_printf(msg, "%s: decimation of mesh failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_NMG:
	    bu_vls_printf(msg, "%s: unable to successfully generate NMG object\n", prefix);
	    break;
	case FACETIZE_FAILURE_CONTINUATION_SURFACE:
	    bu_vls_printf(msg, "%s: continuation polygonization surface build failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_SPSR_SURFACE:
	    bu_vls_printf(msg, "%s: Screened Poisson surface reconstruction failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_SPSR_NONMATCHING:
	    bu_vls_printf(msg, "%s: Screened Poisson surface reconstruction did not produce a BoT matching the original shape.\n", prefix);
	    break;
	default:
	    return;
	    break;
    }
}

const char *
_ged_facetize_attr(int method)
{
    static const char *nmg_flag = "facetize:NMG";
    static const char *continuation_flag = "facetize:CM";
    static const char *spsr_flag = "facetize:SPSR";
    if (method == FACETIZE_METHOD_NMG) return nmg_flag;
    if (method == FACETIZE_METHOD_CONTINUATION) return continuation_flag;
    if (method == FACETIZE_METHOD_SPSR) return spsr_flag;
    return NULL;
}

int
_ged_facetize_attempted(struct ged *gedp, const char *oname, int method)
{
    int ret = 0;
    struct bu_attribute_value_set avs;
    struct directory *dp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);
    if (!dp) return 0;
    if (db5_get_attributes(gedp->dbip, &avs, dp)) return 0;
    if (bu_avs_get(&avs, _ged_facetize_attr(method))) ret = 1;
    bu_avs_free(&avs);
    return ret;
}

int
_ged_facetize_cpcomb(struct _ged_facetize_state *s, const char *o)
{
    struct ged *gedp = s->gedp;
    int ret = BRLCAD_OK;
    struct db_i *dbip = gedp->dbip;
    struct directory *dp;
    struct rt_db_internal ointern, intern;
    struct rt_comb_internal *ocomb, *comb;
    struct bu_attribute_value_set avs;
    int flags;

    /* Unpack original comb to get info to duplicate in new comb */
    dp = db_lookup(dbip, o, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB)) return BRLCAD_ERROR;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(dbip, &avs, dp)) return BRLCAD_ERROR;
    if (rt_db_get_internal(&ointern, dp, dbip, NULL, &rt_uniresource) < 0) return BRLCAD_ERROR;
    ocomb = (struct rt_comb_internal *)ointern.idb_ptr;
    RT_CK_COMB(ocomb);
    flags = dp->d_flags;

    /* Make a new empty comb with the same properties as the original, sans tree */
    RT_DB_INTERNAL_INIT(&intern);
    BU_ALLOC(comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_ptr = (void *)comb;
    intern.idb_meth = &OBJ[ID_COMBINATION];
    GED_DB_DIRADD(gedp, dp, bu_avs_get(s->c_map, o), -1, 0, flags, (void *)&intern.idb_type, 0);
    comb->region_flag = ocomb->region_flag;
    bu_vls_vlscat(&comb->shader, &ocomb->shader);
    comb->rgb_valid = ocomb->rgb_valid;
    comb->rgb[0] = ocomb->rgb[0];
    comb->rgb[1] = ocomb->rgb[1];
    comb->rgb[2] = ocomb->rgb[2];
    comb->region_id = ocomb->region_id;
    comb->aircode = ocomb->aircode;
    comb->GIFTmater = ocomb->GIFTmater;
    comb->los = ocomb->los;
    comb->inherit = ocomb->inherit;
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, 0);

    /* apply attributes to new comb */
    db5_update_attributes(dp, &avs, dbip);

    rt_db_free_internal(&ointern);

    return ret;
}

static db_op_t
_int_to_opt(int op)
{
    if (op == 2) return DB_OP_UNION;
    if (op == 3) return DB_OP_INTERSECT;
    if (op == 4) return DB_OP_SUBTRACT;
    return DB_OP_NULL;
}

int
_ged_facetize_add_children(struct _ged_facetize_state *s, struct directory *cdp)
{
    struct ged *gedp = s->gedp;
    int i = 0;
    int ret = BRLCAD_OK;
    struct db_i *dbip = gedp->dbip;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb = NULL;
    struct directory **children = NULL;
    int child_cnt = 0;
    int *bool_ops = NULL;
    matp_t *mats = NULL;
    int non_ident_mat = 0;
    int non_union_bool = 0;
    const char *nparent;

    RT_DB_INTERNAL_INIT(&intern);
    nparent = bu_avs_get(s->c_map, cdp->d_namep);
    if (rt_db_get_internal(&intern, cdp, dbip, NULL, &rt_uniresource) < 0) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_add_children_memfree;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    child_cnt = db_comb_children(dbip, comb, &children, &bool_ops, &mats);
    if (child_cnt <= 0) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_add_children_memfree;
    }

    /* See if anything fancy is going in with the comb children... */
    for (i = 0; i < child_cnt; i++) {
	if (mats[i] && !bn_mat_is_identity(mats[i])) non_ident_mat++;
    }
    for (i = 0; i < child_cnt; i++) {
	if (_int_to_opt(bool_ops[i]) != DB_OP_UNION) non_union_bool++;
    }

    if (non_ident_mat || non_union_bool) {
	/* More complicated comb, have to rebuild item by item */
	for (i = 0; i < child_cnt; i++) {
	    matp_t m = NULL;
	    const char *nc = bu_avs_get(s->c_map, children[i]->d_namep);
	    if (!nc) {
		nc = bu_avs_get(s->s_map, children[i]->d_namep);
	    }
	    if (!nc) {
		bu_log("Error - object %s has no name mapping??\n", children[i]->d_namep);
		ret = BRLCAD_ERROR;
		goto ged_facetize_add_children_memfree;
	    }
	    m = (mats[i]) ? mats[i] : NULL;
	    if (_ged_combadd2(gedp, (char *)nparent, 1, (const char **)&nc, 0, _int_to_opt(bool_ops[i]), 0, 0, m, 0) != BRLCAD_OK) {
		ret = BRLCAD_ERROR;
		goto ged_facetize_add_children_memfree;
	    }
	}
    } else {
	/* Simple comb, rebuild in one shot */
	const char **av = (const char **)bu_calloc(child_cnt, sizeof(const char *), "av array");
	for (i = 0; i < child_cnt; i++) {
	    av[i] = bu_avs_get(s->c_map, children[i]->d_namep);
	    if (!av[i]) {
		av[i] = bu_avs_get(s->s_map, children[i]->d_namep);
	    }
	    if (!av[i]) {
		bu_log("Error - object %s has no name mapping??\n", children[i]->d_namep);
		ret = BRLCAD_ERROR;
		goto ged_facetize_add_children_memfree;
	    }
	}
	ret = _ged_combadd2(gedp, (char *)nparent, child_cnt, av, 0, DB_OP_UNION, 0, 0, NULL, 0);
    }

ged_facetize_add_children_memfree:

    if (mats) {
	for (i = 0; i < child_cnt; i++) {
	    if (mats[i]) {
		bu_free(mats[i], "free matrix");
	    }
	}
	bu_free(mats, "free mats array");
    }

    if (bool_ops) {
	bu_free(bool_ops, "free ops");
    }

    bu_free(children, "free children struct directory ptr array");

    return ret;
}

int
_ged_methodcomb_add(struct _ged_facetize_state *s, const char *objname, int method)
{
    struct ged *gedp = s->gedp;
    int ret = BRLCAD_OK;
    struct bu_vls method_cname = BU_VLS_INIT_ZERO;
    if (!objname || method == FACETIZE_METHOD_NULL) return BRLCAD_ERROR;

    if (method == FACETIZE_METHOD_NMG && !bu_vls_strlen(s->nmg_comb)) {
	bu_vls_sprintf(s->nmg_comb, "%s_NMG-0", bu_vls_addr(s->froot));
	bu_vls_incr(s->nmg_comb, NULL, NULL, &_db_uniq_test, (void *)gedp);
    }
    if (method == FACETIZE_METHOD_CONTINUATION && !bu_vls_strlen(s->continuation_comb)) {
	bu_vls_sprintf(s->continuation_comb, "%s_CONTINUATION-0", bu_vls_addr(s->froot));
	bu_vls_incr(s->continuation_comb, NULL, NULL, &_db_uniq_test, (void *)gedp);
    }
    if (method == FACETIZE_METHOD_SPSR && !bu_vls_strlen(s->spsr_comb)) {
	bu_vls_sprintf(s->spsr_comb, "%s_SPSR-0", bu_vls_addr(s->froot));
	bu_vls_incr(s->spsr_comb, NULL, NULL, &_db_uniq_test, (void *)gedp);
    }

    switch (method) {
	case FACETIZE_METHOD_NMG:
	    bu_vls_sprintf(&method_cname, "%s", bu_vls_addr(s->nmg_comb));
	    break;
	case FACETIZE_METHOD_CONTINUATION:
	    bu_vls_sprintf(&method_cname, "%s", bu_vls_addr(s->continuation_comb));
	    break;
	case FACETIZE_METHOD_SPSR:
	    bu_vls_sprintf(&method_cname, "%s", bu_vls_addr(s->spsr_comb));
	    break;
	default:
	    bu_vls_free(&method_cname);
	    return BRLCAD_ERROR;
	    break;
    }

    ret =_ged_combadd2(gedp, bu_vls_addr(&method_cname), 1, (const char **)&objname, 0, DB_OP_UNION, 0, 0, NULL, 0);
    bu_vls_free(&method_cname);
    return ret;
}

void
_ged_methodattr_set(struct _ged_facetize_state *s, const char *rcname, int method, struct _ged_facetize_report_info *info)
{
    struct ged *gedp = s->gedp;
    struct bu_vls anum = BU_VLS_INIT_ZERO;
    const char *attrav[5];
    attrav[0] = "attr";
    attrav[1] = "set";
    attrav[2] = rcname;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (method == FACETIZE_METHOD_NMG) {
	struct bg_tess_tol *tol = &(wdbp->wdb_ttol);
	attrav[3] = _ged_facetize_attr(method);
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:nmg_abs";
	bu_vls_sprintf(&anum, "%g", tol->abs);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:nmg_rel";
	bu_vls_sprintf(&anum, "%g", tol->rel);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:nmg_norm";
	bu_vls_sprintf(&anum, "%g", tol->norm);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    if (info && method == FACETIZE_METHOD_CONTINUATION) {
	attrav[3] = _ged_facetize_attr(method);
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:continuation_feature_size";
	bu_vls_sprintf(&anum, "%g", info->feature_size);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:continuation_average_thickness";
	bu_vls_sprintf(&anum, "%g", info->avg_thickness);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    if (method == FACETIZE_METHOD_SPSR) {
	attrav[3] = _ged_facetize_attr(method);
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:spsr_depth";
	bu_vls_sprintf(&anum, "%d", s->s_opts.depth);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:spsr_weight";
	bu_vls_sprintf(&anum, "%g", s->s_opts.point_weight);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:spsr_samples_per_node";
	bu_vls_sprintf(&anum, "%g", s->s_opts.samples_per_node);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    if (info && info->failure_mode == FACETIZE_FAILURE_PNTGEN) {
	attrav[3] = "facetize:EMPTY";
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    bu_vls_free(&anum);
}

int
_ged_facetize_region_obj(struct _ged_facetize_state *s, const char *oname, const char *sname, int ocnt, int max_cnt, int cmethod, struct _ged_facetize_report_info *cinfo)
{
    struct ged *gedp = s->gedp;
    int ret = FACETIZE_FAILURE;
    struct directory *dp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) {
	return BRLCAD_ERROR;
    }

#if 0
    if (cmethod == FACETIZE_MANIFOLD) {

	/* We're staring a new object, so we want to write out the header in the
	 * log file the first time we get an NMG logging event.  (Re)set the flag
	 * so the logger knows to do so. */
	s->log_s->nmg_log_print_header = 1;
	bu_vls_sprintf(s->log_s->nmg_log_header, "MANIFOLD: tessellating %s (%d of %d) with tolerances a=%g, r=%g, n=%g\n", oname, ocnt, max_cnt, s->tol->abs, s->tol->rel, s->tol->norm);

	/* Let the user know what's going on, unless output is suppressed */
	if (!s->quiet) {
	    bu_log("%s", bu_vls_addr(s->log_s->nmg_log_header));
	}

	ret = _ged_manifold_obj(gedp, 1, (const char **)&oname, sname, opts);

	if (ret != FACETIZE_FAILURE) {
	    if (_ged_methodcomb_add(s, sname, FACETIZE_MANIFOLD) != BRLCAD_OK && s->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }
#endif

    if (cmethod == FACETIZE_METHOD_NMG) {

	/* We're staring a new object, so we want to write out the header in the
	 * log file the first time we get an NMG logging event.  (Re)set the flag
	 * so the logger knows to do so. */
	//s->log_s->nmg_log_print_header = 1;
	//bu_vls_sprintf(s->log_s->nmg_log_header, "NMG: tessellating %s (%d of %d) with tolerances a=%g, r=%g, n=%g\n", oname, ocnt, max_cnt, s->tol->abs, s->tol->rel, s->tol->norm);

	/* Let the user know what's going on, unless output is suppressed */
	if (!s->quiet) {
	    //bu_log("%s", bu_vls_addr(s->log_s->nmg_log_header));
	}

	//ret = _ged_facetize_booleval(s, 1, (const char **)&oname, sname);

	if (ret != FACETIZE_FAILURE) {
	    if (_ged_methodcomb_add(s, sname, FACETIZE_METHOD_NMG) != BRLCAD_OK && s->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }

    if (cmethod == FACETIZE_METHOD_CONTINUATION) {

	if (!s->quiet) {
	    bu_log("CM: tessellating %s (%d of %d)\n", oname, ocnt, max_cnt);
	}

	//ret = _ged_continuation_obj(s, oname, sname);
	if (ret == FACETIZE_FAILURE) {
	    if (!s->quiet) {
		struct bu_vls lmsg = BU_VLS_INIT_ZERO;
		_ged_facetize_failure_msg(&lmsg, cinfo->failure_mode, "CM", cinfo);
		bu_log("%s", bu_vls_addr(&lmsg));
		bu_vls_free(&lmsg);
	    }
	} else {
	    if (_ged_methodcomb_add(s, sname, FACETIZE_METHOD_CONTINUATION) != BRLCAD_OK && s->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }

    if (cmethod == FACETIZE_METHOD_SPSR) {

	if (!s->quiet) {
	    bu_log("SPSR: tessellating %s (%d of %d)\n", oname, ocnt, max_cnt);
	}

	if (s->verbosity) {
	    bu_log("SPSR: tessellating %s with depth %d, interpolation weight %g, and samples-per-node %g\n", oname, s->s_opts.depth, s->s_opts.point_weight, s->s_opts.samples_per_node);
	}

	//ret =_ged_spsr_obj(s, oname, sname);
	if (ret == FACETIZE_FAILURE) {
	    if (!s->quiet) {
		struct bu_vls lmsg = BU_VLS_INIT_ZERO;
		_ged_facetize_failure_msg(&lmsg, cinfo->failure_mode, "SPSR", cinfo);
		bu_log("%s", bu_vls_addr(&lmsg));
		bu_vls_free(&lmsg);
	    }
	} else {
	    if (_ged_methodcomb_add(s, sname, FACETIZE_METHOD_SPSR) != BRLCAD_OK && s->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }

    return FACETIZE_FAILURE;
}

int
_ged_facetize_regions_resume(struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int to_convert = 0;
    int methods = s->method_flags;
    unsigned int i = 0;
    int newobj_cnt = 0;
    int ret = BRLCAD_OK;
    struct bu_ptbl *ar = NULL;
    struct bu_ptbl *ar2 = NULL;
    const char *resume_regions = "-attr facetize:original_region";
    struct directory **dpa = NULL;
    struct bu_attribute_value_set rnames;
    struct bu_attribute_value_set bnames;
    struct db_i *dbip = gedp->dbip;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (!argc) return BRLCAD_ERROR;

    bu_avs_init_empty(&bnames);
    bu_avs_init_empty(&rnames);

    /* Use the first object name for the root */
    bu_vls_sprintf(s->froot, "%s", argv[0]);

    /* Used the libged tolerances */
    s->tol = &(wdbp->wdb_ttol);

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (newobj_cnt) {
	bu_vls_sprintf(gedp->ged_result_str, "one or more new object names supplied to resume.");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, resume_regions, argc, dpa, dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for active regions - aborting.\n");
	}
	ret = BRLCAD_ERROR;
	goto ged_facetize_regions_resume_memfree;
    }
    if (!BU_PTBL_LEN(ar)) {
	/* No active regions (possible), nothing to do */
	ret = BRLCAD_OK;
	goto ged_facetize_regions_resume_memfree;
    }

    /* Only work on regions with conversion information */
    BU_ALLOC(ar2, struct bu_ptbl);
    bu_ptbl_init(ar2, 8, "second table");
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	struct bu_attribute_value_set avs;
	const char *rname;
	const char *bname;
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->dbip, &avs, n)) continue;
	rname = bu_avs_get(&avs, "facetize:original_region");
	bname = bu_avs_get(&avs, "facetize:target_name");
	if (!rname || !bname) {
	    bu_avs_free(&avs);
	    continue;
	}
	bu_avs_add(&bnames, n->d_namep, bname);
	bu_avs_add(&rnames, n->d_namep, rname);
	bu_ptbl_ins(ar2, (long *)n);
	bu_avs_free(&avs);
    }

    to_convert = BU_PTBL_LEN(ar2);
    if (!to_convert) {
	/* No regions with conversion information, nothing to do */
	ret = BRLCAD_OK;
	goto ged_facetize_regions_resume_memfree;
    }

    while (methods && BU_PTBL_LEN(ar2) > 0) {
	struct bu_ptbl *tmp;
	int cmethod = 0;
	ssize_t avail_mem = 0;
	bu_ptbl_reset(ar);

	if (!cmethod && (methods & FACETIZE_METHOD_NMG)) {
	    cmethod = FACETIZE_METHOD_NMG;
	    methods = methods & ~(FACETIZE_METHOD_NMG);
	}

	if (!cmethod && (methods & FACETIZE_METHOD_CONTINUATION)) {
	    cmethod = FACETIZE_METHOD_CONTINUATION;
	    methods = methods & ~(FACETIZE_METHOD_CONTINUATION);
	}

	if (!cmethod && (methods & FACETIZE_METHOD_SPSR)) {
	    cmethod = FACETIZE_METHOD_SPSR;
	    methods = methods & ~(FACETIZE_METHOD_SPSR);
	}

	for (i = 0; i < BU_PTBL_LEN(ar2); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar2, i);
	    const char *cname = n->d_namep;
	    const char *sname = bu_avs_get(&bnames, cname);
	    const char *oname = bu_avs_get(&rnames, cname);
	    struct directory *dp = db_lookup(dbip, sname, LOOKUP_QUIET);
	    struct _ged_facetize_report_info cinfo;

	    if (dp == RT_DIR_NULL) {
		if (s->retry || !_ged_facetize_attempted(gedp, cname, cmethod)) {
		    /* Before we try this (unless we're point sampling), check that all the objects in the specified tree(s) are valid solids */
		    struct directory *odp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);

		    /* Regardless of the outcome, record what settings were tried. */
		    _ged_methodattr_set(s, cname, cmethod, &cinfo);

		    if (odp == RT_DIR_NULL || (!_ged_facetize_verify_solid(s, 1, &odp) && cmethod != FACETIZE_METHOD_SPSR)) {
			if (!s->quiet) {
			    bu_log("%s: non-solid objects in specified tree(s) - cannot apply facetization method %s\n", oname, _ged_facetize_attr(cmethod));
			}
			bu_ptbl_ins(ar, (long *)n);
			continue;
		    }

		    if (_ged_facetize_region_obj(s, oname, sname, i+1, (int)BU_PTBL_LEN(ar2), cmethod, &cinfo) == FACETIZE_FAILURE) {
			bu_ptbl_ins(ar, (long *)n);

			avail_mem = bu_mem(BU_MEM_AVAIL, NULL);
			if (avail_mem > 0 && avail_mem < FACETIZE_MEMORY_THRESHOLD) {
			    bu_log("Too little available memory to continue, aborting\n");
			    ret = BRLCAD_ERROR;
			    goto ged_facetize_regions_resume_memfree;
			}
		    }
		} else {
		    bu_ptbl_ins(ar, (long *)n);
		}
	    }
	}

	tmp = ar;
	ar = ar2;
	ar2 = tmp;
    }

    /* Stash the failures */
    if (BU_PTBL_LEN(ar2) > 0) {
	/* Stash any failed regions into a top level comb for easy subsequent examination */
	const char **avv = (const char **)bu_calloc(BU_PTBL_LEN(ar2)+1, sizeof(char *), "argv array");
	struct bu_vls failed_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&failed_name, "%s_FAILED-0", argv[0]);
	bu_vls_incr(&failed_name, NULL, NULL, &_db_uniq_test, (void *)gedp);
	for (i = 0; i < BU_PTBL_LEN(ar2); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar2, i);
	    const char *oname = bu_avs_get(&rnames, n->d_namep);
	    avv[i] = oname;
	}
	ret = _ged_combadd2(gedp, bu_vls_addr(&failed_name), (int)BU_PTBL_LEN(ar2), avv, 0, DB_OP_UNION, 0, 0, NULL, 0);
	bu_vls_free(&failed_name);
	bu_free(avv, "argv array");
    }

ged_facetize_regions_resume_memfree:

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    if (bu_vls_strlen(s->log_s->nmg_log) && s->method_flags & FACETIZE_METHOD_NMG && s->verbosity > 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(s->log_s->nmg_log));
    }

    /* Final report */
    bu_vls_printf(gedp->ged_result_str, "Objects successfully converted: %d of %d\n", (int)(to_convert - BU_PTBL_LEN(ar2)), to_convert);
    if (BU_PTBL_LEN(ar2)) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: %d objects failed:\n", (int)BU_PTBL_LEN(ar2));
	for (i = 0; i < BU_PTBL_LEN(ar2); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar2, i);
	    const char *oname = bu_avs_get(&rnames, n->d_namep);
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", oname);
	}
    }

    bu_avs_free(&bnames);
    bu_avs_free(&rnames);
    if (ar2) {
	bu_ptbl_free(ar2);
	bu_free(ar2, "ar table");
    }
    if (ar) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
    }
    bu_free(dpa, "free dpa");

    return ret;
}

int
_ged_facetize_regions(struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int to_convert = 0;
    int methods = s->method_flags;
    char *newname = NULL;
    int newobj_cnt = 0;
    int ret = BRLCAD_OK;
    unsigned int i = 0;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;
    struct bu_ptbl *pc = NULL;
    struct bu_ptbl *ar = NULL;
    struct bu_ptbl *ar2 = NULL;

    /* We need to copy combs above regions that are not themselves regions.
     * Also, facetize will need all "active" regions that will define shapes.
     * Construct searches to get these sets. */
    const char *preserve_combs = "-type c ! -type r ! -below -type r";
    const char *active_regions = "( -type r ! -below -type r ) -or ( ! -below -type r ! -type comb )";

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    s->tol = &(wdbp->wdb_ttol);

    if (!argc) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (_ged_validate_objs_list(s, argc, argv, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    if (!s->in_place) {
	newname = (char *)argv[argc-1];
	argc--;
    }

    if (!newname) {
	bu_vls_sprintf(s->froot, "%s", argv[0]);
    } else {
	/* Use the new name for the root */
	bu_vls_sprintf(s->froot, "%s", newname);
    }


    /* Set up mapping names for the original toplevel object(s).  If we have
     * top level solids, deal with them now. */
    for (i = 0; i < (unsigned int)argc; i++) {
	struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    /* solid object in list at top level - handle directly */
	    _ged_facetize_mkname(s, argv[i], SOLID_OBJ_NAME);

	    /* Let the user know what's going on, unless output is suppressed */
	    bu_vls_sprintf(s->log_s->nmg_log_header, "NMG: tessellating solid %s with tolerances a=%g, r=%g, n=%g\n", argv[0], s->tol->abs, s->tol->rel, s->tol->norm);
	    if (!s->quiet) {
		bu_log("%s", bu_vls_addr(s->log_s->nmg_log_header));
	    }
	    s->log_s->nmg_log_print_header = 1;

	    //if (_ged_facetize_booleval(s, 1, (const char **)&argv[i], bu_avs_get(s->s_map, argv[i])) != BRLCAD_OK) {
	//	return BRLCAD_ERROR;
	 //   }
	}
    }


    /* Find assemblies and regions */
    BU_ALLOC(pc, struct bu_ptbl);
    if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, preserve_combs, newobj_cnt, dpa, dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for parent combs - aborting.\n");
	}
	ret = BRLCAD_ERROR;
	goto ged_facetize_regions_memfree;
    }
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, newobj_cnt, dpa, dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for active regions - aborting.\n");
	}
	ret = BRLCAD_ERROR;
	goto ged_facetize_regions_memfree;
    }
    if (!BU_PTBL_LEN(ar)) {
	/* No active regions (unlikely but technically possible), nothing to do */
	ret = BRLCAD_OK;
	goto ged_facetize_regions_memfree;
    }


    /* Set up all the names we will need */
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	/* Regions will have a name mapping both to a new region comb AND a facetized
	 * solid object - set up both names, and create the region combs */
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);

	_ged_facetize_mkname(s, n->d_namep, SOLID_OBJ_NAME);

	/* Only generate a comb name if the "region" is actually a comb...
	 * this may not be true for solids with no regions above them. */
	if ((n->d_flags & RT_DIR_COMB)) {
	    _ged_facetize_mkname(s, n->d_namep, COMB_OBJ_NAME);
	}
    }
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(pc, i);
	_ged_facetize_mkname(s, n->d_namep, COMB_OBJ_NAME);
    }



    /* First, add the new toplevel comb to hold all the new geometry */
    if (!s->in_place) {
	const char **ntop = (const char **)bu_calloc(argc, sizeof(const char *), "new top level names");
	for (i = 0; i < (unsigned int)argc; i++) {
	    ntop[i] = bu_avs_get(s->c_map, argv[i]);
	    if (!ntop[i]) {
		ntop[i] = bu_avs_get(s->s_map, argv[i]);
	    }
	}
	if (!s->quiet) {
	    bu_log("Creating new top level assembly object %s...\n", newname);
	}
	ret = _ged_combadd2(gedp, newname, argc, ntop, 0, DB_OP_UNION, 0, 0, NULL, 0);
	bu_free(ntop, "new top level names");
    }


    /* For the assemblies, make new versions with the suffixed names */
    if (!s->quiet) {
	bu_log("Initializing copies of assembly combinations...\n");
    }
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	struct directory *cdp = RT_DIR_NULL;
	const char *nparent;
	struct directory *n = (struct directory *)BU_PTBL_GET(pc, i);

	if (_ged_facetize_cpcomb(s, n->d_namep) != BRLCAD_OK) {
	    if (s->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(s->c_map, n->d_namep), n->d_namep);
	    }
	    continue;
	}

	/* Add the members from the map with the settings from the original
	 * comb */
	cdp = (struct directory *)BU_PTBL_GET(pc, i);
	nparent = bu_avs_get(s->c_map, cdp->d_namep);
	if (!s->quiet) {
	    bu_log("Duplicating assembly (%d of %zu) %s -> %s\n", i+1, BU_PTBL_LEN(pc), cdp->d_namep, nparent);
	}
	if (_ged_facetize_add_children(s, cdp) != BRLCAD_OK) {
	    if (!s->quiet) {
		bu_log("Error: duplication of assembly %s failed!\n", cdp->d_namep);
	    }
	    continue;
	}
    }

    /* For regions, make the new region comb and add a reference to the to-be-created solid */
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	_ged_facetize_mkname(s, n->d_namep, SOLID_OBJ_NAME);
	_ged_facetize_mkname(s, n->d_namep, COMB_OBJ_NAME);
	if (!s->quiet) {
	    bu_log("Copying region (%d of %d) %s -> %s\n", (int)(i+1), (int)BU_PTBL_LEN(ar), n->d_namep, bu_avs_get(s->c_map, n->d_namep));
	}
	if (_ged_facetize_cpcomb(s, n->d_namep) != BRLCAD_OK) {
	    if (s->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(s->c_map, n->d_namep), n->d_namep);
	    }
	} else {
	    const char *rcname = bu_avs_get(s->c_map, n->d_namep);
	    const char *ssname = bu_avs_get(s->s_map, n->d_namep);
	    if (_ged_combadd2(gedp, (char *)rcname, 1, (const char **)&ssname, 0, DB_OP_UNION, 0, 0, NULL, 0) != BRLCAD_OK) {
		if (s->verbosity) {
		    bu_log("Error adding %s to comb %s", ssname, rcname);
		}
	    } else {
		/* By default, store the original region name and target bot name in attributes to make resuming easier */
		const char *attrav[5];
		attrav[0] = "attr";
		attrav[1] = "set";
		attrav[2] = rcname;
		attrav[3] = "facetize:original_region";
		attrav[4] = n->d_namep;
		if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
		    bu_log("Error adding attribute facetize_original_region to comb %s", rcname);
		}
		attrav[3] = "facetize:target_name";
		attrav[4] = ssname;
		if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && s->verbosity) {
		    bu_log("Error adding attribute facetize_target_name to comb %s", rcname);
		    bu_log("Error adding %s to comb %s", ssname, rcname);
		}
	    }
	}
    }


    /* Now, actually trigger the facetize logic on each region.
     *
     * TODO - someday this should be done in parallel, but that's one deep
     * rabbit hole - for now, just try them in order and make sure we can
     * handle (non-crashing) failures to convert sanely. */
    BU_ALLOC(ar2, struct bu_ptbl);
    bu_ptbl_init(ar2, 8, "second table");
    to_convert = BU_PTBL_LEN(ar);

    while (methods && BU_PTBL_LEN(ar) > 0) {
	struct bu_ptbl *tmp;
	int cmethod = 0;
	ssize_t avail_mem = 0;
	bu_ptbl_reset(ar2);

	if (!cmethod && (methods & FACETIZE_METHOD_NMG)) {
	    cmethod = FACETIZE_METHOD_NMG;
	    methods = methods & ~(FACETIZE_METHOD_NMG);
	}

	if (!cmethod && (methods & FACETIZE_METHOD_CONTINUATION)) {
	    cmethod = FACETIZE_METHOD_CONTINUATION;
	    methods = methods & ~(FACETIZE_METHOD_CONTINUATION);
	}

	if (!cmethod && (methods & FACETIZE_METHOD_SPSR)) {
	    cmethod = FACETIZE_METHOD_SPSR;
	    methods = methods & ~(FACETIZE_METHOD_SPSR);
	}

	if (!cmethod) {
	    bu_log("Error: methods isn't empty (%d), but no method selected??\n", methods);
	    break;
	}

	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    const char *oname = n->d_namep;
	    const char *cname = bu_avs_get(s->c_map, oname);
	    const char *sname = bu_avs_get(s->s_map, oname);
	    struct directory *dp = db_lookup(dbip, sname, LOOKUP_QUIET);

	    if (dp == RT_DIR_NULL) {
		/* Before we try this (unless we're point sampling), check that all the objects in the specified tree(s) are valid solids */
		struct directory *odp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);
		struct _ged_facetize_report_info cinfo = FACETIZE_REPORT_INFO_INIT;

		/* Regardless of the outcome, record what settings were tried. */
		_ged_methodattr_set(s, cname, cmethod, &cinfo);

		if (odp == RT_DIR_NULL || (!_ged_facetize_verify_solid(s, 1, &odp) && cmethod != FACETIZE_METHOD_SPSR)) {
		    if (!s->quiet) {
			bu_log("%s: non-solid objects in specified tree(s) - cannot apply facetization method %s\n", oname, _ged_facetize_attr(cmethod));
		    }
		    bu_ptbl_ins(ar2, (long *)n);
		    continue;
		}

		if (_ged_facetize_region_obj(s, oname, sname, i+1, (int)BU_PTBL_LEN(ar), cmethod, &cinfo) == FACETIZE_FAILURE) {
		    bu_ptbl_ins(ar2, (long *)n);

		    avail_mem = bu_mem(BU_MEM_AVAIL, NULL);
		    if (avail_mem > 0 && avail_mem < FACETIZE_MEMORY_THRESHOLD) {
			bu_log("Too little available memory to continue, aborting\n");
			ret = BRLCAD_ERROR;
			goto ged_facetize_regions_memfree;
		    }

		}
	    }
	}

	tmp = ar;
	ar = ar2;
	ar2 = tmp;

    }

    /* For easier user inspection of what happened, make some high level debugging combs */
    if (BU_PTBL_LEN(ar) > 0) {
	/* Stash any failed regions into a top level comb for easy subsequent examination */
	const char **avv = (const char **)bu_calloc(BU_PTBL_LEN(ar)+1, sizeof(char *), "argv array");
	struct bu_vls failed_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&failed_name, "%s_FAILED-0", newname);
	bu_vls_incr(&failed_name, NULL, NULL, &_db_uniq_test, (void *)gedp);
	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    avv[i] = n->d_namep;
	}
	ret = _ged_combadd2(gedp, bu_vls_addr(&failed_name), (int)BU_PTBL_LEN(ar), avv, 0, DB_OP_UNION, 0, 0, NULL, 0);
	bu_vls_free(&failed_name);
	bu_free(avv, "argv array");
    }

    if (s->in_place) {
	/* The "new" tree is actually the preservation of the old tree in this
	 * scenario, so swap all the region names */
	if (s->verbosity) {
	    bu_log("Generation complete, swapping new geometry into original tree...\n");
	}
	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    if (_ged_facetize_obj_swap(gedp, n->d_namep, bu_avs_get(s->c_map, n->d_namep)) != BRLCAD_OK) {
		ret = BRLCAD_ERROR;
		goto ged_facetize_regions_memfree;
	    }
	}
    }

ged_facetize_regions_memfree:

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    //if (bu_vls_strlen(s->log_s->nmg_log) && s->method_flags & FACETIZE_METHOD_NMG && s->verbosity > 1) {
	//bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(s->log_s->nmg_log));
    //}

    /* Final report */
    bu_vls_printf(gedp->ged_result_str, "Objects successfully converted: %d of %d\n", (int)(to_convert - BU_PTBL_LEN(ar)), to_convert);
    if (BU_PTBL_LEN(ar)) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: %d objects failed:\n", (int)BU_PTBL_LEN(ar));
	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", n->d_namep);
	}
    }

    if (ar2) {
	bu_ptbl_free(ar2);
	bu_free(ar2, "ar table");
    }
    if (ar) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
    }
    if (pc) {
	bu_ptbl_free(pc);
	bu_free(pc, "pc table");
    }
    bu_free(dpa, "dpa array");
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

