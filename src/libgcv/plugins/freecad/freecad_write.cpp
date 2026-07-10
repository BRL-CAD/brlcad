/*                F R E E C A D _ W R I T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file freecad_write.cpp
 *
 * Export BRL-CAD BRep primitives to FreeCAD .FCStd using open2open's
 * openNURBS -> OCCT writer.
 */

#include "common.h"

#include "bu/path.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "rt/comb.h"
#include "rt/geom.h"

#include "opennurbs.h"
#include "open2open/fcstd_convert.h"

#include <cctype>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace freecad_write
{


static std::string
clean_name(const char *raw, const char *fallback, int index)
{
    std::string name = (raw && raw[0]) ? raw : std::string(fallback) + "_" + std::to_string(index);

    for (std::string::iterator it = name.begin(); it != name.end(); ++it) {
	unsigned char c = static_cast<unsigned char>(*it);
	if (!std::isalnum(c) && *it != '_' && *it != '-' && *it != '.')
	    *it = '_';
    }

    return name;
}


static bool
append_brep(struct db_i *dbip, const struct directory *dp, ONX_Model &model, int index)
{
    if (!dbip || !dp)
	return false;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);

    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return false;

    if (intern.idb_type != ID_BREP) {
	rt_db_free_internal(&intern);
	return false;
    }

    struct rt_brep_internal *bi = (struct rt_brep_internal *)intern.idb_ptr;
    if (!bi || !bi->brep) {
	rt_db_free_internal(&intern);
	return false;
    }

    ON_3dmObjectAttributes attrs;
    attrs.Default();
    attrs.m_name = clean_name(dp->d_namep, "brlcad_brep", index).c_str();
    model.AddModelGeometryComponent(ON_Brep::New(*bi->brep), &attrs, true);

    rt_db_free_internal(&intern);
    return true;
}


static void
free_child_matrices(matp_t *mats)
{
    if (!mats)
	return;

    for (int i = 0; mats[i]; ++i)
	bu_free(mats[i], "free matrix");
    bu_free(mats, "free mats array");
}


static int
append_breps_from_dp(struct db_i *dbip, const struct directory *dp, ONX_Model &model, int &index, std::set<const struct directory *> &seen_breps, std::set<const struct directory *> &seen_combs)
{
    if (!dbip || !dp)
	return 0;

    if (dp->d_flags & RT_DIR_SOLID) {
	if (seen_breps.count(dp))
	    return 0;
	seen_breps.insert(dp);
	if (append_brep(dbip, dp, model, index)) {
	    ++index;
	    return 1;
	}
	return 0;
    }

    if (!(dp->d_flags & RT_DIR_COMB) || seen_combs.count(dp))
	return 0;
    seen_combs.insert(dp);

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);

    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	return 0;

    if (intern.idb_type != ID_COMBINATION) {
	rt_db_free_internal(&intern);
	return 0;
    }

    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    struct directory **children = NULL;
    matp_t *mats = NULL;
    int child_count = db_comb_children(dbip, comb, &children, NULL, &mats);
    int brep_count = 0;

    for (int i = 0; i < child_count && children && children[i] != RT_DIR_NULL; ++i)
	brep_count += append_breps_from_dp(dbip, children[i], model, index, seen_breps, seen_combs);

    if (children)
	bu_free(children, "free children struct directory ptr array");
    free_child_matrices(mats);
    rt_db_free_internal(&intern);

    return brep_count;
}


static int
append_selected_breps(struct db_i *dbip, const gcv_opts *gcv_options, ONX_Model &model)
{
    int count = 0;
    int index = 0;
    std::set<const struct directory *> seen_breps;
    std::set<const struct directory *> seen_combs;

    if (gcv_options && gcv_options->num_objects) {
	for (size_t i = 0; i < gcv_options->num_objects; ++i) {
	    const char *name = gcv_options->object_names[i];
	    const struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
	    if (!dp)
		continue;
	    count += append_breps_from_dp(dbip, dp, model, index, seen_breps, seen_combs);
	}
	return count;
    }

    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip) {
	if (dp->d_flags & RT_DIR_SOLID) {
	    if (!seen_breps.count(dp) && append_brep(dbip, dp, model, index)) {
		seen_breps.insert(dp);
		++count;
		++index;
	    }
	}
    } FOR_ALL_DIRECTORY_END;

    return count;
}


static int
freecad_write(gcv_context *context, const gcv_opts *gcv_options, const void *UNUSED(options_data), const char *dest_path)
{
    if (!context || !context->dbip || !dest_path)
	return 0;

    ONX_Model model;
    model.AddDefaultLayer(L"Default", ON_Color::UnsetColor);
    model.m_properties.m_RevisionHistory.NewRevision();
    model.m_properties.m_Application.m_application_name = "BRL-CAD gcv FreeCAD writer";

    int brep_count = append_selected_breps(context->dbip, gcv_options, model);
    if (brep_count <= 0) {
	bu_log("freecad_write: no BRep primitives available to export\n");
	return 0;
    }

    int written = open2open::ONX_ModelToFCStdFile(dest_path, model);
    if (written <= 0) {
	bu_log("freecad_write: failed to write %s\n", dest_path);
	return 0;
    }

    if (written < brep_count)
	bu_log("freecad_write: wrote %d of %d BRep primitives to %s\n", written, brep_count, dest_path);

    return 1;
}


extern "C" const struct gcv_filter gcv_conv_freecad_write = {
    "FreeCAD Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_VND_FREECAD, NULL,
    NULL, NULL, freecad_write
};


}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
