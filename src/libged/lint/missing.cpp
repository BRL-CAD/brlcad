/*                   M I S S I N G . C P P
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
/** @file missing.cpp
 *
 * Look for references to geometry not present where it is expected
 */

#include "common.h"

#include <set>
#include <map>
#include <string>
#include "json.hpp"

extern "C" {
#include "bu/opt.h"
#include "bg/trimesh.h"
}
#include "./ged_lint.h"

static void
comb_find_missing(lint_data *mdata, const char *parent, struct db_i *dbip, union tree *tp)
{
    if (!tp) return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    comb_find_missing(mdata, parent, dbip, tp->tr_b.tb_right);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    comb_find_missing(mdata, parent, dbip, tp->tr_b.tb_left);
	    break;
	case OP_DB_LEAF:
	    if (db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET) == RT_DIR_NULL) {
		std::string inst = std::string(parent) + std::string("/") + std::string(tp->tr_l.tl_name);
		nlohmann::json missing_json;
		missing_json["problem_type"] = "missing_comb_ref";
		missing_json["path"] = inst;
		mdata->j.push_back(missing_json);
	    }
	    break;
	default:
	    bu_log("ged_lint: comb_find_invalid: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("ged_lint: comb_find_invalid\n");
    }
}

static void
shape_find_missing(lint_data *mdata, struct db_i *dbip, struct directory *dp)
{
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    struct db5_raw_internal raw;
    unsigned char *cp;
    char datasrc;
    char *sketch_name;
    struct bu_vls dsp_name = BU_VLS_INIT_ZERO;

    if (!mdata || !dbip || !dp) return;

    switch (dp->d_minor_type) {
	case DB5_MINORTYPE_BRLCAD_DSP:
	    /* For DSP we can't do a full import up front, since the potential invalidity we're looking to detect will cause
	     * the import to fail.  Partially crack it, enough to where we can get what we need */
	    if (db_get_external(&ext, dp, dbip) < 0) return;
	    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
		bu_free_external(&ext);
		return;
	    }
	    cp = (unsigned char *)raw.body.ext_buf;
	    cp += 2*SIZEOF_NETWORK_LONG + SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_MAT + SIZEOF_NETWORK_SHORT;
	    datasrc = *cp;
	    cp++; cp++;
	    bu_vls_strncpy(&dsp_name, (char *)cp, raw.body.ext_nbytes - (cp - (unsigned char *)raw.body.ext_buf));
	    if (datasrc == RT_DSP_SRC_OBJ) {
		if (db_lookup(dbip, bu_vls_addr(&dsp_name), LOOKUP_QUIET) == RT_DIR_NULL) {
		    std::string inst = std::string(dp->d_namep) + std::string("/") + std::string(bu_vls_addr(&dsp_name));
		    nlohmann::json missing_json;
		    missing_json["problem_type"] = "missing_dsp_obj";
		    missing_json["path"] = inst;
		    mdata->j.push_back(missing_json);
		}
	    }
	    if (datasrc == RT_DSP_SRC_FILE) {
		if (!bu_file_exists(bu_vls_addr(&dsp_name), NULL)) {
		    std::string inst = std::string(dp->d_namep) + std::string("/") + std::string(bu_vls_addr(&dsp_name));
		    nlohmann::json missing_json;
		    missing_json["problem_type"] = "missing_dsp_file";
		    missing_json["path"] = inst;
		    mdata->j.push_back(missing_json);
		}
	    }
	    bu_vls_free(&dsp_name);
	    bu_free_external(&ext);
	    break;
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	    /* For EXTRUDE we can't do a full import up front, since the potential invalidity we're looking to detect will cause
	     * the import to fail.  Partially crack it, enough to where we can get what we need */
	    if (db_get_external(&ext, dp, dbip) < 0) return;
	    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
		bu_free_external(&ext);
		return;
	    }
	    cp = (unsigned char *)raw.body.ext_buf;
	    sketch_name = (char *)cp + ELEMENTS_PER_VECT*4*SIZEOF_NETWORK_DOUBLE + SIZEOF_NETWORK_LONG;
	    if (db_lookup(dbip, sketch_name, LOOKUP_QUIET) == RT_DIR_NULL) {
		std::string inst = std::string(dp->d_namep) + std::string("/") + std::string(sketch_name);
		nlohmann::json missing_json;
		missing_json["problem_type"] = "missing_extrude_sketch";
		missing_json["path"] = inst;
		mdata->j.push_back(missing_json);
	    }
	    bu_free_external(&ext);
	    break;
	default:
	    break;
    }
}

int
_ged_missing_check(lint_data *mdata, int argc, struct directory **dpa)
{
    if (!mdata)
	return BRLCAD_ERROR;
    if (argc && !dpa)
	return BRLCAD_ERROR;

    int ret = BRLCAD_OK;
    struct ged *gedp = mdata->gedp;

    if (argc) {
	const char *osearch = "-type comb";
	struct bu_ptbl pc = BU_PTBL_INIT_ZERO;
	if (db_search(&pc, DB_SEARCH_RETURN_UNIQ_DP, osearch, argc, dpa, gedp->dbip, NULL) < 0) {
	    ret = BRLCAD_ERROR;
	} else {
	    for (unsigned int i = 0; i < BU_PTBL_LEN(&pc); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(&pc, i);
		if (dp->d_flags & RT_DIR_COMB) {
		    struct rt_db_internal in;
		    struct rt_comb_internal *comb;
		    if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0) continue;
		    comb = (struct rt_comb_internal *)in.idb_ptr;
		    comb_find_missing(mdata, dp->d_namep, gedp->dbip, comb->tree);
		} else {
		    shape_find_missing(mdata, gedp->dbip, dp);
		}
	    }
	}
	bu_ptbl_free(&pc);
    } else {
	for (int i = 0; i < RT_DBNHASH; i++) {
	    for (struct directory *dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (dp->d_flags & RT_DIR_COMB) {
		    struct rt_db_internal in;
		    struct rt_comb_internal *comb;
		    if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0) continue;
		    comb = (struct rt_comb_internal *)in.idb_ptr;
		    comb_find_missing(mdata, dp->d_namep, gedp->dbip, comb->tree);
		} else {
		    shape_find_missing(mdata, gedp->dbip, dp);
		}
	    }
	}
    }

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

