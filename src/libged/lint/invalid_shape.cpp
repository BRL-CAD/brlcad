/*                I N V A L I D _ S H A P E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
/** @file invalid_shape.cpp
  */

#include "common.h"

#include <set>
#include <map>
#include <string>

#include "./ged_lint.h"

static bool
arb_check_enabled(const std::map<std::string, std::set<std::string>> &imt, const char *check)
{
    if (!imt.size())
	return true;

    std::map<std::string, std::set<std::string>>::const_iterator a_it = imt.find(std::string("arb"));
    if (a_it == imt.end())
	return false;

    return a_it->second.find(std::string(check)) != a_it->second.end();
}


/* Someday, when we have parametric constraint evaluation for parameters for primitives, we can hook
 * that into this logic as well... for now, run various special-case routines that are available to
 * spot various categories of problematic primitives. */
void
_ged_invalid_prim_check(lint_data *ldata, struct directory *dp)
{
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct bu_vls vlog = BU_VLS_INIT_ZERO;
    int not_valid = 0;
    if (!ldata || !dp) return;

    std::map<std::string, std::set<std::string>> &imt = ldata->im_techniques;

    if (dp->d_flags & RT_DIR_HIDDEN) return;
    if (dp->d_addr == RT_DIR_PHONY_ADDR) return;

    if (rt_db_get_internal(&intern, dp, ldata->gedp->dbip, (fastf_t *)NULL) < 0) return;
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD) {
	rt_db_free_internal(&intern);
	return;
    }

    switch (dp->d_minor_type) {
	case DB5_MINORTYPE_BRLCAD_BOT:
	    bot = (struct rt_bot_internal *)intern.idb_ptr;
	    RT_BOT_CK_MAGIC(bot);
	    bot_checks(ldata, dp, bot);
	    rt_db_free_internal(&intern);
	    break;
	case DB5_MINORTYPE_BRLCAD_BREP:
	    if (imt.size() && imt.find(std::string("brep")) == imt.end())
		return;
	    not_valid = !rt_brep_valid(&vlog, &intern, 0);
	    if (not_valid) {
		nlohmann::json berr;
		berr["problem_type"] = "opennurbs_invalid";
		berr["object_type"] = "brep";
		berr["object_name"] = dp->d_namep;
		berr["verbose_log"] = std::string(bu_vls_cstr(&vlog));
		ldata->j.push_back(berr);
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_ARB8:
	    if (imt.size() && imt.find(std::string("arb")) == imt.end())
		return;
	    {
		struct rt_arb_internal *arb = (struct rt_arb_internal *)intern.idb_ptr;
		RT_ARB_CK_MAGIC(arb);
		struct bn_tol tol = BN_TOL_INIT_TOL;
		int issues = 0;
		struct bu_vls arb_log = BU_VLS_INIT_ZERO;

		if (ldata->ftol > 0.0) {
		    tol.dist = ldata->ftol;
		    tol.dist_sq = tol.dist * tol.dist;
		}

		rt_arb_validate(&arb_log, arb, &tol, &issues);
		if ((issues & RT_ARB_VALIDATE_NONSTANDARD) && arb_check_enabled(imt, "non_standard_ordering")) {
		    nlohmann::json aerr;
		    aerr["problem_type"] = "non_standard_ordering";
		    aerr["object_type"] = "arb";
		    aerr["object_name"] = dp->d_namep;
		    aerr["verbose_log"] = std::string(bu_vls_cstr(&arb_log));
		    ldata->j.push_back(aerr);
		}
		if ((issues & RT_ARB_VALIDATE_NONCOPLANAR) && arb_check_enabled(imt, "non_coplanar_face")) {
		    nlohmann::json aerr;
		    aerr["problem_type"] = "non_coplanar_face";
		    aerr["object_type"] = "arb";
		    aerr["object_name"] = dp->d_namep;
		    aerr["verbose_log"] = std::string(bu_vls_cstr(&arb_log));
		    ldata->j.push_back(aerr);
		}
		if ((issues & RT_ARB_VALIDATE_CONCAVE) && arb_check_enabled(imt, "concave")) {
		    nlohmann::json aerr;
		    aerr["problem_type"] = "concave";
		    aerr["object_type"] = "arb";
		    aerr["object_name"] = dp->d_namep;
		    aerr["verbose_log"] = std::string(bu_vls_cstr(&arb_log));
		    ldata->j.push_back(aerr);
		}
		if ((issues & RT_ARB_VALIDATE_TWISTED) && arb_check_enabled(imt, "twisted")) {
		    nlohmann::json aerr;
		    aerr["problem_type"] = "twisted";
		    aerr["object_type"] = "arb";
		    aerr["object_name"] = dp->d_namep;
		    aerr["verbose_log"] = std::string(bu_vls_cstr(&arb_log));
		    ldata->j.push_back(aerr);
		}
		bu_vls_free(&arb_log);
	    }
	    break;
	case DB5_MINORTYPE_BRLCAD_DSP:
	    if (imt.size() && imt.find(std::string("dsp")) == imt.end())
		return;
	    // TODO - check for empty data object and zero length dimension vectors.
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    if (imt.size() && imt.find(std::string("extrude")) == imt.end())
		return;
	    // TODO - check for zero length dimension vectors.
	    break;
	default:
	    if (OBJ[dp->d_minor_type].ft_validate) {
		struct bu_vls generic_log = BU_VLS_INIT_ZERO;
		struct bn_tol tol = BN_TOL_INIT_TOL;
		if (ldata->ftol > 0.0) {
		    tol.dist = ldata->ftol;
		    tol.dist_sq = tol.dist * tol.dist;
		}
		int issues = OBJ[dp->d_minor_type].ft_validate(&generic_log, &intern, &tol);
		if (issues > 0) {
		    try {
			nlohmann::json errs = nlohmann::json::parse(bu_vls_cstr(&generic_log));
			if (errs.is_array()) {
			    for (auto& err : errs) {
				if (!err.contains("object_type"))
				    err["object_type"] = OBJ[dp->d_minor_type].ft_name;
				if (!err.contains("object_name"))
				    err["object_name"] = dp->d_namep;
				ldata->j.push_back(err);
			    }
			}
		    } catch (...) {
			nlohmann::json berr;
			berr["problem_type"] = "invalid_shape";
			berr["object_type"] = OBJ[dp->d_minor_type].ft_name;
			berr["object_name"] = dp->d_namep;
			berr["verbose_log"] = std::string(bu_vls_cstr(&generic_log));
			ldata->j.push_back(berr);
		    }
		}
		bu_vls_free(&generic_log);
	    }
	    break;
    }

    bu_vls_free(&vlog);
}

int
_ged_invalid_shape_check(lint_data *ldata)
{
    int ret = BRLCAD_OK;
    struct directory *dp;
    unsigned int i;
    struct bu_ptbl *pc = NULL;
    struct bu_vls sopts = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&sopts, "! -type comb %s", ldata->filter.c_str());
    BU_ALLOC(pc, struct bu_ptbl);
    if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, bu_vls_cstr(&sopts), ldata->argc, ldata->dpa, ldata->gedp->dbip, NULL, NULL, NULL) < 0) {
	ret = BRLCAD_ERROR;
	bu_free(pc, "pc table");
    } else {
	for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	    dp = (struct directory *)BU_PTBL_GET(pc, i);
	    _ged_invalid_prim_check(ldata, dp);
	}
	bu_ptbl_free(pc);
	bu_free(pc, "pc table");
    }
    bu_vls_free(&sopts);
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
