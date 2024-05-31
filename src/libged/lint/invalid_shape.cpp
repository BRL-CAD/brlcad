/*                I N V A L I D _ S H A P E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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

    if (rt_db_get_internal(&intern, dp, ldata->gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return;
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
	    // TODO - check for twisted arbs.
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
    if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, bu_vls_cstr(&sopts), ldata->argc, ldata->dpa, ldata->gedp->dbip, NULL) < 0) {
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

