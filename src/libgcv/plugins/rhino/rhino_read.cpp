/*                  R H I N O _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016 United States Government as represented by
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
/** @file rhino_read.cpp
 *
 * Brief description
 *
 */


#include "common.h"

#include "gcv/api.h"
#include "gcv/util.h"
#include "wdb.h"

#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>


#ifndef OBJ_BREP
#error OBJ_BREP is not defined
#endif


namespace
{


template <typename T> void
autoptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoPtr");
}


template <typename T> void
autoptr_wrap_delete(T *ptr)
{
    delete ptr;
}


template <typename T, void free_fn(T *) = autoptr_wrap_bu_free>
struct AutoPtr {
    explicit AutoPtr(T *vptr = NULL) :
	ptr(vptr)
    {}


    ~AutoPtr()
    {
	if (ptr)
	    free_fn(ptr);
    }


    T *ptr;


private:
    AutoPtr(const AutoPtr &source);
    AutoPtr &operator=(const AutoPtr &source);
};


template<typename T, std::size_t length>
HIDDEN std::size_t array_length(const T(&)[length])
{
    return length;
}


template<typename Target, typename Source>
HIDDEN Target lexical_cast(const Source &value)
{
    std::stringstream interpreter;
    Target result;

    if (!(interpreter << value) ||
	!(interpreter >> result) ||
	!(interpreter >> std::ws).eof())
	throw std::invalid_argument("bad lexical_cast");

    return result;
}


struct UuidCompare {
    bool operator()(const ON_UUID &left, const ON_UUID &right) const
    {
	return ON_UuidCompare(left, right) == -1;
    }
};


// ON_CreateUuid() is not implemented for all platforms.
// When it fails, we create a UUIDv4.
HIDDEN ON_UUID
generate_uuid()
{
    ON_UUID result;

    if (ON_CreateUuid(result))
	return result;

    result.Data1 = static_cast<ON__UINT32>(drand48() *
					   std::numeric_limits<ON__UINT32>::max() + 0.5);
    result.Data2 = static_cast<ON__UINT16>(drand48() *
					   std::numeric_limits<ON__UINT16>::max() + 0.5);
    result.Data3 = static_cast<ON__UINT16>(drand48() *
					   std::numeric_limits<ON__UINT16>::max() + 0.5);

    for (std::size_t i = 0; i < array_length(result.Data4); ++i)
	result.Data4[i] = static_cast<unsigned char>(drand48() * 255.0 + 0.5);

    // set the UUIDv4 reserved bits
    result.Data3 = (result.Data3 & 0x0fff) | 0x4000;
    result.Data4[0] = (result.Data4[0] & 0x3f) | 0x80;

    return result;
}


// ONX_Model::Audit() fails to repair UUID issues on platforms
// where ON_CreateUuid() is not implemented.
HIDDEN std::size_t
replace_invalid_uuids(ONX_Model &model)
{
    std::size_t num_repairs = 0;
    std::set<ON_UUID, UuidCompare> seen;
    seen.insert(ON_nil_uuid); // UUIDs can't be nil

#define REPLACE_UUIDS(array, member) \
do { \
    for (unsigned i = 0; i < (array).UnsignedCount(); ++i) { \
	while (!seen.insert((array).At(i)->member).second) { \
	    (array).At(i)->member = generate_uuid(); \
	    ++num_repairs; \
	} \
    } \
} while (false)

#define REPLACE_UUIDS_POINTER(array, member) \
do { \
    for (unsigned i = 0; i < (array).UnsignedCount(); ++i) { \
	while (!seen.insert((*(array).At(i))->member).second) { \
	    (*(array).At(i))->member = generate_uuid(); \
	    ++num_repairs; \
	} \
    } \
} while (false)

    REPLACE_UUIDS_POINTER(model.m_bitmap_table, m_bitmap_id);
    REPLACE_UUIDS(model.m_mapping_table, m_mapping_id);
    REPLACE_UUIDS(model.m_linetype_table, m_linetype_id);
    REPLACE_UUIDS(model.m_layer_table, m_layer_id);
    REPLACE_UUIDS(model.m_group_table, m_group_id);
    REPLACE_UUIDS(model.m_font_table, m_font_id);
    REPLACE_UUIDS(model.m_dimstyle_table, m_dimstyle_id);
    REPLACE_UUIDS(model.m_light_table, m_attributes.m_uuid);
    REPLACE_UUIDS(model.m_hatch_pattern_table, m_hatchpattern_id);
    REPLACE_UUIDS(model.m_idef_table, m_uuid);
    REPLACE_UUIDS(model.m_object_table, m_attributes.m_uuid);
    REPLACE_UUIDS_POINTER(model.m_history_record_table, m_record_id);
    REPLACE_UUIDS(model.m_userdata_table, m_uuid);

#undef REPLACE_UUIDS_POINTER
#undef REPLACE_UUIDS

    if (num_repairs)
	model.DestroyCache();

    return num_repairs;
}


HIDDEN void
clean_name(std::map<ON_wString, std::size_t> &seen,
	   const std::string &default_name, ON_wString &name)
{
    name = ON_String(name);

    if (name.IsEmpty())
	name = default_name.c_str();

    name.ReplaceWhiteSpace('_');

    if (const std::size_t number = seen[name]++)
	name += ('_' + lexical_cast<std::string>(number + 1)).c_str();
}


HIDDEN void
load_model(ONX_Model &model, const std::string &path,
	   const std::string &default_name)
{
    if (!model.Read(path.c_str()))
	throw std::runtime_error("ONX_Model::Read() failed");

    int num_problems;
    int num_repairs = replace_invalid_uuids(model);

    {
	int temp;
	num_problems = model.Audit(true, &temp, NULL, NULL);
	num_repairs += temp;
    }

    if (num_problems)
	throw std::runtime_error("repair failed");
    else if (num_repairs)
	std::cerr << "repaired " << num_repairs << " model issues\n";

    // clean and remove duplicate names
    std::map<ON_wString, std::size_t> seen;

#define REPLACE_NAMES(array, member) \
do { \
    for (unsigned i = 0; i < (array).UnsignedCount(); ++i) \
	clean_name(seen, default_name, (array).At(i)->member); \
} while (false)

    REPLACE_NAMES(model.m_layer_table, m_name);
    REPLACE_NAMES(model.m_idef_table, m_name);
    REPLACE_NAMES(model.m_object_table, m_attributes.m_name);
    REPLACE_NAMES(model.m_light_table, m_attributes.m_name);

#undef REPLACE_NAMES
}


HIDDEN std::set<ON_UUID, UuidCompare>
get_idef_members(const ONX_Model &model)
{
    std::set<ON_UUID, UuidCompare> result;

    for (unsigned i = 0; i < model.m_idef_table.UnsignedCount(); ++i) {
	const ON_InstanceDefinition &idef = *model.m_idef_table.At(i);

	for (unsigned j = 0; j < idef.m_object_uuid.UnsignedCount(); ++j)
	    result.insert(*idef.m_object_uuid.At(j));
    }

    return result;
}


HIDDEN void
matrix_from_xform(mat_t dest, const ON_Xform &source)
{
    for (std::size_t i = 0; i < 4; ++i)
	for (std::size_t j = 0; j < 4; ++j)
	    dest[4 * i + j] = source[i][j];
}


HIDDEN void
import_object(rt_wdb &wdb, const std::string &name,
	      const ON_InstanceRef &instance_ref, const ONX_Model &model)
{
    const ON_InstanceDefinition &idef = *model.m_idef_table.At(model.IDefIndex(
					    instance_ref.m_instance_definition_uuid));

    mat_t matrix;
    matrix_from_xform(matrix, instance_ref.m_xform);

    wmember members;
    BU_LIST_INIT(&members.l);
    mk_addmember(ON_String(idef.m_name).Array(), &members.l, matrix, WMOP_UNION);
    if (mk_comb(&wdb, name.c_str(), &members.l, false, NULL, NULL, NULL, 0, 0, 0, 0,
		false, false, false))
	throw std::runtime_error("mk_comb() failed");
}


HIDDEN void
import_object(rt_wdb &wdb, const std::string &name, const ON_Brep &brep)
{
    if (mk_brep(&wdb, name.c_str(), const_cast<ON_Brep *>(&brep)))
	throw std::runtime_error("mk_brep() failed");
}


HIDDEN void
import_object(rt_wdb &wdb, const std::string &name, const ON_Mesh &orig)
{
    ON_Mesh mesh = orig;

    mesh.ConvertQuadsToTriangles();
    mesh.CombineIdenticalVertices();
    mesh.Compact();

    if (!mesh.m_V.UnsignedCount() || !mesh.m_F.UnsignedCount())
	return;

    unsigned char orientation = RT_BOT_UNORIENTED;

    switch (mesh.SolidOrientation()) {
	case 0:
	    orientation = RT_BOT_UNORIENTED;
	    break;

	case 1:
	    orientation = RT_BOT_CW;
	    break;

	case -1:
	    orientation = RT_BOT_CCW;
	    break;

	default:
	    throw std::logic_error("unknown orientation");
    }

    std::vector<fastf_t> vertices(3 * mesh.m_V.UnsignedCount());

    for (unsigned i = 0; i < mesh.m_V.UnsignedCount(); ++i) {
	fastf_t * const dest_vertex = &vertices.at(3 * i);
	const ON_3fPoint &source_vertex = *mesh.m_V.At(i);
	VMOVE(dest_vertex, source_vertex);
    }

    std::vector<int> faces(3 * mesh.m_F.UnsignedCount());

    for (unsigned i = 0; i < mesh.m_F.UnsignedCount(); ++i) {
	int * const dest_face = &faces.at(3 * i);
	const int * const source_face = mesh.m_F.At(i)->vi;
	VMOVE(dest_face, source_face);
    }

    unsigned char mode;
    {
	rt_bot_internal bot;
	bot.magic = RT_BOT_INTERNAL_MAGIC;
	bot.orientation = orientation;
	bot.num_vertices = vertices.size();
	bot.num_faces = faces.size();
	bot.vertices = &vertices.at(0);
	bot.faces = &faces.at(0);
	mode = gcv_bot_is_solid(&bot) ? RT_BOT_SOLID : RT_BOT_PLATE;
    }

    std::vector<fastf_t> thicknesses;
    AutoPtr<bu_bitv, bu_bitv_free> bitv;

    if (mode == RT_BOT_PLATE) {
	const fastf_t plate_thickness = 1.0;

	thicknesses.assign(faces.size(), plate_thickness);
	bitv.ptr = bu_bitv_new(faces.size());
    }

    if (mesh.m_FN.UnsignedCount() != mesh.m_F.UnsignedCount()) {
	if (mk_bot(&wdb, name.c_str(), mode, orientation, 0, vertices.size(),
		   faces.size(), &vertices.at(0), &faces.at(0),
		   thicknesses.empty() ? NULL :  &thicknesses.at(0), bitv.ptr))
	    throw std::runtime_error("mk_bot() failed");

	return;
    }

    mesh.UnitizeFaceNormals();
    std::vector<fastf_t> normals(3 * mesh.m_FN.UnsignedCount());

    for (unsigned i = 0; i < mesh.m_FN.UnsignedCount(); ++i) {
	fastf_t * const dest_normal = &normals.at(3 * i);
	const ON_3fVector &source_normal = *mesh.m_FN.At(i);
	VMOVE(dest_normal, source_normal);
    }

    std::vector<int> face_normals(mesh.m_FN.UnsignedCount());

    for (unsigned i = 0; i < mesh.m_FN.UnsignedCount(); ++i) {
	int * const dest_face_normal = &face_normals.at(3 * i);
	VSETALL(dest_face_normal, i);
    }

    if (mk_bot_w_normals(&wdb, name.c_str(), mode, orientation,
			 RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS, vertices.size(), faces.size(),
			 &vertices.at(0), &faces.at(0), thicknesses.empty() ? NULL : &thicknesses.at(0),
			 bitv.ptr, face_normals.size(), &normals.at(0), &face_normals.at(0)))
	throw std::runtime_error("mk_bot_w_normals() failed");
}


HIDDEN bool
import_object(rt_wdb &wdb, const std::string &name,
	      const ON_Object &object)
{
    if (const ON_Brep * const temp = ON_Brep::Cast(&object)) {
	import_object(wdb, name, *temp);
	return true;
    }

    if (const ON_Geometry * const temp = ON_Geometry::Cast(&object)) {
	if (temp->HasBrepForm()) {
	    AutoPtr<ON_Brep> brep(temp->BrepForm());
	    import_object(wdb, name, *brep.ptr);
	    return true;
	} else
	    return false;
    }

    if (const ON_Mesh * const temp = ON_Mesh::Cast(&object)) {
	import_object(wdb, name, *temp);
	return true;
    }

    return false;
}


HIDDEN void
import_model_objects(rt_wdb &wdb, const ONX_Model &model)
{
    for (unsigned i = 0; i < model.m_object_table.UnsignedCount(); ++i) {
	const ONX_Model_Object &model_object = *model.m_object_table.At(i);
	const std::string name = ON_String(model_object.m_attributes.m_name).Array();

	if (const ON_InstanceRef * const temp = ON_InstanceRef::Cast(
		model_object.m_object))
	    import_object(wdb, name, *temp, model);
	else if (!import_object(wdb, name, *model_object.m_object))
	    std::cerr
		    << "skipped "
		    << model_object.m_object->ClassId()->ClassName()
		    << " model object '"
		    << ON_String(model_object.m_attributes.m_name).Array()
		    << "'\n";
    }
}


HIDDEN void
import_idef(rt_wdb &wdb, const ON_InstanceDefinition &idef,
	    const ONX_Model &model)
{
    wmember members;
    BU_LIST_INIT(&members.l);

    for (unsigned i = 0; i < idef.m_object_uuid.UnsignedCount(); ++i) {
	const ONX_Model_Object &model_object = *model.m_object_table.At(
		model.ObjectIndex(*idef.m_object_uuid.At(i)));

	mk_addmember(ON_String(model_object.m_attributes.m_name).Array(), &members.l,
		     NULL, WMOP_UNION);
    }

    if (mk_comb(&wdb, ON_String(idef.m_name).Array(), &members.l, false, NULL, NULL,
		NULL, 0, 0, 0, 0, false, false, false))
	throw std::runtime_error("mk_comb() failed");
}


HIDDEN void
import_model_idefs(rt_wdb &wdb, const ONX_Model &model)
{
    for (unsigned i = 0; i < model.m_idef_table.UnsignedCount(); ++i) {
	const ON_InstanceDefinition &idef = *model.m_idef_table.At(i);

	import_idef(wdb, idef, model);
    }
}


HIDDEN int
rhino_read(gcv_context *context, const gcv_opts *gcv_options,
	   const void *UNUSED(options_data), const char *source_path)
{
    ONX_Model model;
    load_model(model, source_path, gcv_options->default_name);

    const std::set<ON_UUID, UuidCompare> idef_members = get_idef_members(model);

    import_model_objects(*context->dbip->dbi_wdbp, model);
    import_model_idefs(*context->dbip->dbi_wdbp, model);

    return 1;
}


struct gcv_filter gcv_conv_rhino_read = {
    "Rhino Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_RHINO,
    NULL, NULL, rhino_read
};

static const gcv_filter * const filters[] = {&gcv_conv_rhino_read, NULL};


}


extern "C"
{
    extern const gcv_plugin gcv_plugin_info = {filters};
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
