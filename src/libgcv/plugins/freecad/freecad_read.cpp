/*                 F R E E C A D _ R E A D . C P P
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
/** @file freecad_read.cpp
 *
 * Import FreeCAD .FCStd files into BRL-CAD using open2open's FCStd reader and
 * OCCT -> openNURBS BRep conversion core.
 */

#include "common.h"

#include "bu/path.h"
#include "gcv/api.h"
#include "wdb.h"

#include "opennurbs.h"
#include "open2open/fcstd_convert.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace freecad_read
{


static std::string
clean_name(const std::string &raw, const char *fallback, int index)
{
    std::string name = raw.empty() ? std::string(fallback) + "_" + std::to_string(index) : raw;

    for (std::string::iterator it = name.begin(); it != name.end(); ++it) {
	unsigned char c = static_cast<unsigned char>(*it);
	if (!std::isalnum(c) && *it != '_' && *it != '-' && *it != '.')
	    *it = '_';
    }

    if (name.empty())
	name = std::string(fallback) + "_" + std::to_string(index);

    return name;
}


static std::string
unique_name(const std::string &base, std::map<std::string, int> &used)
{
    std::map<std::string, int>::iterator found = used.find(base);
    if (found == used.end()) {
	used[base] = 1;
	return base;
    }

    int next = ++found->second;
    return base + "_" + std::to_string(next);
}


static std::string
root_name_from_path(const char *source_path, const gcv_opts *gcv_options)
{
    if (gcv_options && gcv_options->default_name)
	return clean_name(gcv_options->default_name, "freecad_model", 0);

    bu_vls basename = BU_VLS_INIT_ZERO;
    if (source_path && bu_path_component(&basename, source_path, BU_PATH_BASENAME_EXTLESS)) {
	std::string result = clean_name(bu_vls_cstr(&basename), "freecad_model", 0);
	bu_vls_free(&basename);
	return result;
    }

    bu_vls_free(&basename);
    return "freecad_model";
}


static void
add_color_attribute(struct rt_wdb *wdbp, const std::string &name, const ON_3dmObjectAttributes *attrs)
{
    if (!wdbp || !attrs)
	return;

    ON_Color c = attrs->m_color;
    if (c == ON_Color::UnsetColor)
	return;

    bu_vls rgb = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&rgb, "%d/%d/%d", c.Red(), c.Green(), c.Blue());
    (void)db5_update_attribute(name.c_str(), "color", bu_vls_cstr(&rgb), wdbp->dbip);
    bu_vls_free(&rgb);
}


static int
freecad_read(gcv_context *context, const gcv_opts *gcv_options, const void *UNUSED(options_data), const char *source_path)
{
    if (!context || !context->dbip || !source_path)
	return 0;

    ONX_Model model;
    int converted = open2open::FCStdFileToONX_Model(source_path, model);
    if (converted <= 0) {
	bu_log("freecad_read: no FreeCAD shapes were converted from %s\n", source_path);
	return 0;
    }

    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	return 0;

    std::map<std::string, int> used;
    std::vector<std::string> solids;

    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::ModelGeometry);
    int index = 0;
    for (const ON_ModelComponent *component = it.FirstComponent(); component; component = it.NextComponent(), ++index) {
	const ON_ModelGeometryComponent *geometry_component = ON_ModelGeometryComponent::Cast(component);
	if (!geometry_component)
	    continue;

	const ON_Geometry *geometry = geometry_component->Geometry(NULL);
	if (!geometry)
	    continue;

	std::unique_ptr<ON_Brep> brep_holder;
	const ON_Brep *brep = ON_Brep::Cast(geometry);
	if (!brep && geometry->HasBrepForm()) {
	    brep_holder.reset(geometry->BrepForm());
	    brep = brep_holder.get();
	}

	if (!brep)
	    continue;

	if (brep->m_F.Count() <= 0)
	    continue;

	std::string raw_name;
	const ON_3dmObjectAttributes *attrs = geometry_component->Attributes(NULL);
	if (attrs && attrs->m_name.IsNotEmpty()) {
	    ON_String name_utf8(attrs->m_name);
	    raw_name = name_utf8.Array();
	}

	std::string solid_name = unique_name(clean_name(raw_name, "freecad_shape", index), used);
	ON_Brep *writable_brep = const_cast<ON_Brep *>(brep);
	if (mk_brep(wdbp, solid_name.c_str(), (void *)writable_brep)) {
	    bu_log("freecad_read: failed to write BRep %s\n", solid_name.c_str());
	    continue;
	}

	(void)db5_update_attribute(solid_name.c_str(), "importer", "gcv-freecad", wdbp->dbip);
	add_color_attribute(wdbp, solid_name, attrs);
	solids.push_back(solid_name);
    }

    if (solids.empty()) {
	bu_log("freecad_read: no BRep solids were written for %s\n", source_path);
	return 0;
    }

    struct wmember root;
    BU_LIST_INIT(&root.l);
    for (std::vector<std::string>::const_iterator s = solids.begin(); s != solids.end(); ++s)
	mk_addmember(s->c_str(), &root.l, NULL, WMOP_UNION);

    std::string root_name = unique_name(root_name_from_path(source_path, gcv_options), used);
    if (mk_comb(wdbp, root_name.c_str(), &root.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0))
	bu_log("freecad_read: failed to write top-level combination %s\n", root_name.c_str());

    return 1;
}


static int
freecad_can_read(const char *source_path)
{
    if (!source_path)
	return 0;

    const char *ext = strrchr(source_path, '.');
    if (ext && BU_STR_EQUIV(ext + 1, "fcstd"))
	return 1;

    open2open::FcstdDoc doc;
    return open2open::ReadFcstdDoc(source_path, doc) ? 1 : 0;
}


static const struct gcv_filter gcv_conv_freecad_read = {
    "FreeCAD Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_FREECAD, freecad_can_read,
    NULL, NULL, freecad_read
};


extern "C" const struct gcv_filter gcv_conv_freecad_write;
static const struct gcv_filter * const filters[] = {&gcv_conv_freecad_read, &gcv_conv_freecad_write, NULL};


}


extern "C"
{
    extern const gcv_plugin gcv_plugin_info_s = {freecad_read::filters};
    COMPILER_DLLEXPORT const struct gcv_plugin *
    gcv_plugin_info()
    {
	return &gcv_plugin_info_s;
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
