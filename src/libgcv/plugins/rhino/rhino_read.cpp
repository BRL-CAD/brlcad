/*                  R H I N O _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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

#include "bu/path.h"
#include "bg/trimesh.h"
#include "gcv/api.h"
#include "gcv/util.h"
#include "wdb.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>


namespace rhino_read
{


template <typename T>
void
autoptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoPtr");
}


template <typename T>
void
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


template <typename T, std::size_t length>
HIDDEN std::size_t array_length(const T(&)[length])
{
    return length;
}


template <typename Target, typename Source>
HIDDEN Target lexical_cast(const Source &value)
{
    std::stringstream interpreter;
    Target result;

    if (!(interpreter << value) ||
	!(interpreter >> result) ||
	!(interpreter >> std::ws).eof())
	bu_bomb("bad lexical_cast");

    return result;
}


HIDDEN void
comb_to_region(db_i &db, const std::string &name)
{
    RT_CK_DBI(&db);

    if (directory * const dir = db_lookup(&db, name.c_str(), true)) {
	if (dir->d_flags & RT_DIR_COMB) {
	    if (db5_update_attribute(name.c_str(), "region", "R", &db))
		bu_bomb("db5_update_attribute() failed");

	    dir->d_flags |= RT_DIR_REGION;
	} else
	    bu_bomb("invalid directory type");
    } else
	bu_bomb("db_lookup() failed");
}

static void
comb_region_name_check(std::map<const directory *, std::string> &renamed, db_i &db, const std::string &name)
{
    RT_CK_DBI(&db);
    // If name doesn't have a .r suffix, add it
    struct bu_vls ext = BU_VLS_INIT_ZERO;
    bool add_ext = false;
    if (bu_path_component(&ext, name.c_str(), BU_PATH_EXT)) {
	if (!BU_STR_EQUAL(bu_vls_cstr(&ext), ".r")) {
	    // Have an ext, but not the right one - add .r
	    add_ext = true;
	}
    } else {
	// Don't have an ext - add it
	add_ext = true;
    }
    bu_vls_free(&ext);

    if (!add_ext) {
	return;
    }

    struct bu_vls nname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&nname, "%s.r", name.c_str());

    directory * const dir = db_lookup(&db, name.c_str(), true);
    if (dir == RT_DIR_NULL) {
	bu_vls_free(&nname);
	return;
    }
    std::pair<const directory *, std::string> rpair = std::make_pair(dir, name);
    if (db_rename(&db, dir, bu_vls_cstr(&nname))){
	bu_vls_free(&nname);
	return;
    }
    renamed.insert(rpair);
    bu_vls_free(&nname);
}

struct UuidCompare {
    bool operator()(const ON_UUID &left, const ON_UUID &right) const
    {
	return ON_UuidCompare(left, right) == -1;
    }
};


class InvalidRhinoModelError : public std::runtime_error
{
public:
    explicit InvalidRhinoModelError(const std::string &value) :
	std::runtime_error(value)
    {}
};


template <template<typename> class Array, typename T>
HIDDEN const T &at(const Array<T> &array, std::size_t index)
{
    if (const T * const result = array.At(static_cast<unsigned>(index)))
	return *result;
    else
	throw InvalidRhinoModelError("invalid index");
}


template <template<typename> class Array, typename T>
HIDDEN T &at(Array<T> &array, std::size_t index)
{
    return const_cast<T &>(at(const_cast<const Array<T> &>(array), index));
}


template <typename T, typename Array>
HIDDEN const T &at(const Array &array, std::size_t index)
{
    if (const T * const result = array.At(static_cast<unsigned>(index)))
	return *result;
    else
	throw InvalidRhinoModelError("invalid index");
}


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
    srand48(time(NULL));

#define REPLACE_UUIDS(array, member) \
    do { \
	for (std::size_t i = 0; i < (array).UnsignedCount(); ++i) { \
	    while (!seen.insert(at((array), i)member).second) { \
		at((array), i)member = generate_uuid(); \
		++num_repairs; \
	    } \
	} \
    } while (false)

    REPLACE_UUIDS(model.m_object_table, .m_attributes.m_uuid);
    REPLACE_UUIDS(model.m_layer_table, .m_layer_id);
    REPLACE_UUIDS(model.m_idef_table, .m_uuid);
    REPLACE_UUIDS(model.m_bitmap_table, ->m_bitmap_id);
    REPLACE_UUIDS(model.m_mapping_table, .m_mapping_id);
    REPLACE_UUIDS(model.m_linetype_table, .m_linetype_id);
    REPLACE_UUIDS(model.m_group_table, .m_group_id);
    REPLACE_UUIDS(model.m_font_table, .m_font_id);
    REPLACE_UUIDS(model.m_dimstyle_table, .m_dimstyle_id);
    REPLACE_UUIDS(model.m_light_table, .m_attributes.m_uuid);
    REPLACE_UUIDS(model.m_hatch_pattern_table, .m_hatchpattern_id);
    REPLACE_UUIDS(model.m_history_record_table, ->m_record_id);
    REPLACE_UUIDS(model.m_userdata_table, .m_uuid);

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
    name.Replace('/', '_');

    if (const std::size_t number = seen[name]++)
	name += ('_' + lexical_cast<std::string>(number + 1)).c_str();
}


HIDDEN void
load_model(const gcv_opts &gcv_options, const std::string &path,
	   ONX_Model &model, std::string &root_name)
{
    if (!model.Read(path.c_str()))
	throw InvalidRhinoModelError("ONX_Model::Read() failed.\n\nNote:  if this file was saved from Rhino3D, make sure it was saved using\nRhino's v5 format or lower - newer versions of the 3dm format are not\ncurrently supported by BRL-CAD.");

    std::size_t num_problems;
    std::size_t num_repairs = replace_invalid_uuids(model);

    {
	int temp;
	num_problems = static_cast<std::size_t>(model.Audit(true, &temp, NULL, NULL));
	num_repairs += static_cast<std::size_t>(temp);
    }

    if (num_problems)
	throw InvalidRhinoModelError("repair failed");
    else if (num_repairs && gcv_options.verbosity_level)
	std::cerr << "repaired " << num_repairs << " model issues\n";

    // clean and remove duplicate names
    std::map<ON_wString, std::size_t> seen;
    {
	ON_wString temp = root_name.c_str();
	clean_name(seen, gcv_options.default_name, temp);
	root_name = ON_String(temp).Array();
    }

#define REPLACE_NAMES(array, member) \
    do { \
	for (std::size_t i = 0; i < (array).UnsignedCount(); ++i) \
	    clean_name(seen, gcv_options.default_name, at((array), i).member); \
    } while (false)

    REPLACE_NAMES(model.m_layer_table, m_name);
    REPLACE_NAMES(model.m_idef_table, m_name);
    REPLACE_NAMES(model.m_object_table, m_attributes.m_name);
    REPLACE_NAMES(model.m_light_table, m_attributes.m_name);

#undef REPLACE_NAMES
}


HIDDEN void
write_geometry(rt_wdb &wdb, const std::string &name, const ON_Brep &brep)
{
    ON_Brep *b = const_cast<ON_Brep *>(&brep);
    if (mk_brep(&wdb, name.c_str(), (void *)b))
	bu_bomb("mk_brep() failed");
}


HIDDEN void
write_geometry(rt_wdb &wdb, const std::string &name, ON_Mesh mesh)
{
    mesh.ConvertQuadsToTriangles();
    mesh.CombineIdenticalVertices();
    mesh.Compact();

    const std::size_t num_vertices = mesh.m_V.UnsignedCount();
    const std::size_t num_faces = mesh.m_F.UnsignedCount();

    if (!num_vertices || !num_faces)
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
	    bu_bomb("unknown orientation");
    }

    std::vector<fastf_t> vertices(3 * num_vertices);

    for (std::size_t i = 0; i < num_vertices; ++i) {
	fastf_t * const dest_vertex = &vertices.at(3 * i);
	const ON_3fPoint &source_vertex = at<ON_3fPoint>(mesh.m_V, i);
	VMOVE(dest_vertex, source_vertex);
    }

    std::vector<int> faces(3 * num_faces);

    for (std::size_t i = 0; i < num_faces; ++i) {
	int * const dest_face = &faces.at(3 * i);
	const int * const source_face = at(mesh.m_F, i).vi;
	VMOVE(dest_face, source_face);
    }

    unsigned char mode;
    {
	rt_bot_internal bot;
	std::memset(&bot, 0, sizeof(bot));
	bot.magic = RT_BOT_INTERNAL_MAGIC;
	bot.orientation = orientation;
	bot.num_vertices = num_vertices;
	bot.num_faces = num_faces;
	bot.vertices = &vertices.at(0);
	bot.faces = &faces.at(0);
	mode = bg_trimesh_solid((int)bot.num_vertices, (int)bot.num_faces, bot.vertices, bot.faces, NULL) ? RT_BOT_PLATE : RT_BOT_SOLID;
    }

    std::vector<fastf_t> thicknesses;
    AutoPtr<bu_bitv, bu_bitv_free> bitv;

    if (mode == RT_BOT_PLATE) {
	const fastf_t plate_thickness = 1.0;

	thicknesses.assign(num_faces, plate_thickness);
	bitv.ptr = bu_bitv_new(num_faces);
    }

    if (mesh.m_FN.UnsignedCount() != num_faces) {
	if (mk_bot(&wdb, name.c_str(), mode, orientation, 0, num_vertices,
		   num_faces, &vertices.at(0), &faces.at(0),
		   thicknesses.empty() ? NULL :  &thicknesses.at(0), bitv.ptr))
	    bu_bomb("mk_bot() failed");

	return;
    }

    mesh.UnitizeFaceNormals();
    std::vector<fastf_t> normals(3 * mesh.m_FN.UnsignedCount());

    for (std::size_t i = 0; i < mesh.m_FN.UnsignedCount(); ++i) {
	fastf_t * const dest_normal = &normals.at(3 * i);
	const ON_3fVector &source_normal = at<ON_3fVector>(mesh.m_FN, i);
	VMOVE(dest_normal, source_normal);
    }

    std::vector<int> face_normals(3 * mesh.m_FN.UnsignedCount());

    for (std::size_t i = 0; i < mesh.m_FN.UnsignedCount(); ++i) {
	int * const dest_face_normal = &face_normals.at(3 * i);
	VSETALL(dest_face_normal, i);
    }

    if (mk_bot_w_normals(&wdb, name.c_str(), mode, orientation,
			 RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS, num_vertices,
			 num_faces, &vertices.at(0), &faces.at(0),
			 thicknesses.empty() ? NULL : &thicknesses.at(0), bitv.ptr,
			 num_faces, &normals.at(0), &face_normals.at(0)))
	bu_bomb("mk_bot_w_normals() failed");
}


HIDDEN bool
write_geometry(rt_wdb &wdb, const std::string &name,
	       const ON_Geometry &geometry)
{
    if (const ON_Brep * const brep = ON_Brep::Cast(&geometry)) {
	write_geometry(wdb, name, *brep);
    } else if (const ON_Mesh * const mesh = ON_Mesh::Cast(&geometry)) {
	write_geometry(wdb, name, *mesh);
    } else if (geometry.HasBrepForm()) {
	AutoPtr<ON_Brep, autoptr_wrap_delete> temp(geometry.BrepForm());
	write_geometry(wdb, name, *temp.ptr);
    } else
	return false;

    return true;
}


typedef std::pair<std::string, std::string> Shader;


HIDDEN Shader
get_shader(const ON_Material &material)
{
    std::ostringstream sstream;

    sstream << "{"
	    << " tr " << material.m_transparency
	    << " re " << material.m_reflectivity
	    << " sp " << 0
	    << " di " << 0.3
	    << " ri " << material.m_index_of_refraction
	    << " ex " << 0
	    << " sh " << material.m_shine
	    << " em " << material.m_emission
	    << " }";

    return std::make_pair("plastic", sstream.str());
}


HIDDEN void
get_object_material(const ON_3dmObjectAttributes &attributes,
		    const ONX_Model &model, Shader &out_shader, unsigned char *out_rgb,
		    bool &out_own_shader, bool &out_own_rgb)
{
    ON_Material temp;
    model.GetRenderMaterial(attributes, temp);
    out_shader = get_shader(temp);
    out_own_shader = attributes.MaterialSource() != ON::material_from_parent;

    out_rgb[0] = static_cast<unsigned char>(model.WireframeColor(attributes).Red());
    out_rgb[1] = static_cast<unsigned char>(model.WireframeColor(
	    attributes).Green());
    out_rgb[2] = static_cast<unsigned char>(model.WireframeColor(
	    attributes).Blue());
    out_own_rgb = attributes.ColorSource() != ON::color_from_parent;

    if (!out_rgb[0] && !out_rgb[1] && !out_rgb[2]) {
	ON_Material material;
	model.GetRenderMaterial(attributes, material);

	out_rgb[0] = static_cast<unsigned char>(material.m_diffuse.Red());
	out_rgb[1] = static_cast<unsigned char>(material.m_diffuse.Green());
	out_rgb[2] = static_cast<unsigned char>(material.m_diffuse.Blue());
	out_own_rgb = out_own_shader;
    }
}


HIDDEN void
write_comb(rt_wdb &wdb, const std::string &name,
	   const std::set<std::string> &members, const mat_t matrix = NULL,
	   const char *shader_name = NULL, const char *shader_options = NULL,
	   const unsigned char *rgb = NULL)
{
    wmember wmembers;
    BU_LIST_INIT(&wmembers.l);

    for (std::set<std::string>::const_iterator it = members.begin();
	 it != members.end(); ++it)
	mk_addmember(it->c_str(), &wmembers.l, const_cast<fastf_t *>(matrix),
		     WMOP_UNION);

    if (mk_comb(&wdb, name.c_str(), &wmembers.l, false, shader_name, shader_options,
		rgb, 0, 0, 0, 0, false, false, false))
	bu_bomb("mk_comb() failed");
}


HIDDEN void
import_object(rt_wdb &wdb, const std::string &name,
	      const ON_InstanceRef &instance_ref, const ONX_Model &model,
	      const char *shader_name, const char *shader_options, const unsigned char *rgb)
{
    const ON_InstanceDefinition &idef = at(model.m_idef_table,
					   static_cast<std::size_t>(model.IDefIndex(
						   instance_ref.m_instance_definition_uuid)));

    mat_t matrix;

    for (std::size_t i = 0; i < 4; ++i)
	for (std::size_t j = 0; j < 4; ++j)
	    matrix[4 * i + j] = instance_ref.m_xform[i][j];

    std::set<std::string> members;
    members.insert(ON_String(idef.m_name).Array());

    write_comb(wdb, name, members, matrix, shader_name, shader_options, rgb);
}


HIDDEN void
write_attributes(rt_wdb &wdb, const std::string &name, const ON_Object &object,
		 const ON_UUID &uuid)
{
    // ON_UuidToString() buffers must be >= 37 chars
    const std::size_t uuid_string_length = 37;
    char temp[uuid_string_length];

    if (db5_update_attribute(name.c_str(), "rhino::type",
			     object.ClassId()->ClassName(), wdb.dbip)
	|| db5_update_attribute(name.c_str(), "rhino::uuid", ON_UuidToString(uuid,
				temp), wdb.dbip))
	bu_bomb("db5_update_attribute() failed");
}


HIDDEN void
import_model_objects(const gcv_opts &gcv_options, rt_wdb &wdb,
		     const ONX_Model &model)
{
    std::size_t success_count = 0;

    for (std::size_t i = 0; i < model.m_object_table.UnsignedCount(); ++i) {
	const ONX_Model_Object &object = at(model.m_object_table, i);
	const std::string name = ON_String(object.m_attributes.m_name).Array();
	const std::string member_name = name + ".s";

	Shader shader;
	unsigned char rgb[3];
	bool own_shader, own_rgb;

	get_object_material(object.m_attributes, model, shader, rgb, own_shader,
			    own_rgb);

	if (const ON_InstanceRef * const iref = ON_InstanceRef::Cast(object.m_object))
	    import_object(wdb, name, *iref, model, own_shader ? shader.first.c_str() : NULL,
			  own_shader ? shader.second.c_str() : NULL, own_rgb ? rgb : NULL);
	else if (write_geometry(wdb, member_name,
				*ON_Geometry::Cast(object.m_object))) {
	    std::set<std::string> members;
	    members.insert(member_name);
	    write_comb(wdb, name, members, NULL, own_shader ? shader.first.c_str() : NULL,
		       own_shader ? shader.second.c_str() : NULL, own_rgb ? rgb : NULL);
	} else {
	    if (gcv_options.verbosity_level)
		std::cerr << "skipped " << object.m_object->ClassId()->ClassName() <<
			  " model object '" << name << "'\n";

	    continue;
	}

	write_attributes(wdb, name, *object.m_object, object.m_attributes.m_uuid);
	++success_count;
    }

    if (gcv_options.verbosity_level)
	if (success_count != model.m_object_table.UnsignedCount())
	    std::cerr << "imported " << success_count << " of " <<
		      model.m_object_table.UnsignedCount() << " objects\n";
}


HIDDEN void
import_idef(rt_wdb &wdb, const ON_InstanceDefinition &idef,
	    const ONX_Model &model)
{
    std::set<std::string> members;

    for (std::size_t i = 0; i < idef.m_object_uuid.UnsignedCount(); ++i) {
	const ONX_Model_Object &object = at(model.m_object_table,
					    model.ObjectIndex(at(idef.m_object_uuid, i)));

	members.insert(ON_String(object.m_attributes.m_name).Array());
    }

    const std::string name = ON_String(idef.m_name).Array();

    write_comb(wdb, name, members);
    write_attributes(wdb, name, idef, idef.m_uuid);
}


HIDDEN void
import_model_idefs(rt_wdb &wdb, const ONX_Model &model)
{
    for (std::size_t i = 0; i < model.m_idef_table.UnsignedCount(); ++i)
	import_idef(wdb, at(model.m_idef_table, i), model);
}


HIDDEN std::set<std::string>
get_all_idef_members(const ONX_Model &model)
{
    std::set<std::string> result;

    for (std::size_t i = 0; i < model.m_idef_table.UnsignedCount(); ++i) {
	const ON_InstanceDefinition &idef = at(model.m_idef_table, i);

	for (std::size_t j = 0; j < idef.m_object_uuid.UnsignedCount(); ++j) {
	    const ONX_Model_Object &object = at(model.m_object_table,
						model.ObjectIndex(at(idef.m_object_uuid, j)));
	    result.insert(ON_String(object.m_attributes.m_name).Array());
	}
    }

    return result;
}


HIDDEN std::set<std::string>
get_layer_members(const ON_Layer &layer, const ONX_Model &model)
{
    std::set<std::string> members;

    for (std::size_t i = 0; i < model.m_layer_table.UnsignedCount(); ++i) {
	const ON_Layer &current_layer = at(model.m_layer_table, i);

	if (current_layer.m_parent_layer_id == layer.ModelObjectId())
	    members.insert(ON_String(current_layer.m_name).Array());
    }

    for (std::size_t i = 0; i < model.m_object_table.UnsignedCount(); ++i) {
	const ONX_Model_Object &object = at(model.m_object_table, i);

	if (object.m_attributes.m_layer_index == layer.m_layer_index)
	    members.insert(ON_String(object.m_attributes.m_name).Array());
    }

    const std::set<std::string> model_idef_members = get_all_idef_members(model);
    std::set<std::string> result;
    std::set_difference(members.begin(), members.end(), model_idef_members.begin(),
			model_idef_members.end(), std::inserter(result, result.end()));

    return result;
}


HIDDEN void
import_layer(rt_wdb &wdb, const ON_Layer &layer, const ONX_Model &model)
{
    const std::string name = ON_String(layer.m_name).Array();

    unsigned char rgb[3];
    rgb[0] = static_cast<unsigned char>(layer.Color().Red());
    rgb[1] = static_cast<unsigned char>(layer.Color().Green());
    rgb[2] = static_cast<unsigned char>(layer.Color().Blue());

    const Shader shader = get_shader(layer.RenderMaterialIndex() != -1 ?
				     at(model.m_material_table, layer.RenderMaterialIndex()) : ON_Material());

    write_comb(wdb, name, get_layer_members(layer, model), NULL,
	       shader.first.c_str(), shader.second.c_str(), rgb);
    write_attributes(wdb, name, layer, layer.m_layer_id);
}


HIDDEN void
import_model_layers(rt_wdb &wdb, const ONX_Model &model,
		    const std::string &root_name)
{
    for (std::size_t i = 0; i < model.m_layer_table.UnsignedCount(); ++i)
	import_layer(wdb, at(model.m_layer_table, i), model);

    ON_Layer root_layer;
    root_layer.SetLayerName(root_name.c_str());
    import_layer(wdb, root_layer, model);
}


HIDDEN void
polish_output(const gcv_opts &gcv_options, db_i &db)
{
    std::map<const directory *, std::string> renamed;
    bu_ptbl found = BU_PTBL_INIT_ZERO;
    AutoPtr<bu_ptbl, db_search_free> autofree_found(&found);

    if (0 > db_search(&found, DB_SEARCH_RETURN_UNIQ_DP,
		      (std::string() +
		       "-attr rhino::type=ON_Layer -or ( ( -attr rhino::type=ON_InstanceDefinition -or -attr rhino::type=ON_InstanceRef ) -not -name IDef* -not -name "
		       + gcv_options.default_name + "* )").c_str(), 0, NULL, &db, NULL))
	bu_bomb("db_search() failed");

    const char * const ignored_attributes[] = {"rhino::type", "rhino::uuid"};
    rt_reduce_db(&db, array_length(ignored_attributes), ignored_attributes, &found);

    // apply region flag
    db_search_free(&found);
    BU_PTBL_INIT(&found);

    // Set region flags, add .r suffix to regions if not already present
    renamed.clear();
    const char *reg_search = "-type comb -attr rgb -not -above -attr rgb -or -attr shader -not -above -attr shader";
    if (0 > db_search(&found, DB_SEARCH_RETURN_UNIQ_DP, reg_search, 0, NULL, &db, NULL))
	bu_bomb("db_search() failed");
    bu_ptbl found_instances = BU_PTBL_INIT_ZERO;
    AutoPtr<bu_ptbl, db_search_free> autofree_found_instances(&found_instances);
    if (0 > db_search(&found_instances, DB_SEARCH_TREE, reg_search, 0, NULL, &db, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found)) {
	directory **entry;

	for (BU_PTBL_FOR(entry, (directory **), &found)) {

	    comb_to_region(db, (*entry)->d_namep);

	    comb_region_name_check(renamed, db, (*entry)->d_namep);

	    if (gcv_options.randomize_colors) {
		// random colors mode: TODO: move this into a filter after 7.26.0
		std::string rgb;

		// TODO - use bu_color for this...
		for (std::size_t i = 0; i < 3; ++i)
		    rgb.append(lexical_cast<std::string>(static_cast<unsigned>
							 (drand48() * 255.0 + 0.5)) + (i != 2 ? "/" : ""));

		if (db5_update_attribute((*entry)->d_namep, "rgb", rgb.c_str(), &db)
		    || db5_update_attribute((*entry)->d_namep, "color", rgb.c_str(), &db))
		    bu_bomb("db5_update_attribute() failed");
	    }
	}
    }

    // Update any combs that referred to old region names to reference the new ones instead
    if (BU_PTBL_LEN(&found_instances)) {
	db_full_path **entry;
	for (BU_PTBL_FOR(entry, (db_full_path **), &found_instances)) {
	    struct directory *ec = DB_FULL_PATH_CUR_DIR(*entry);
	    struct directory *ep = (*entry)->fp_names[(*entry)->fp_len - 2];
	    std::map<const directory *, std::string>::iterator rentry = renamed.find(ec);
	    if (rentry == renamed.end()) {
		continue;
	    }
	    bu_ptbl stack = BU_PTBL_INIT_ZERO;
	    AutoPtr<bu_ptbl, bu_ptbl_free> autofree_stack(&stack);
	    if (!db_comb_mvall(ep, &db, rentry->second.c_str(), ec->d_namep, &stack))
		bu_bomb("db_comb_mvall() failed");
	}
    }
    db_search_free(&found_instances);

    // rename shapes after their parent layers
    db_search_free(&found);
    BU_PTBL_INIT(&found);

    renamed.clear();
    if (0 > db_search(&found, DB_SEARCH_TREE, "-type shape", 0, NULL, &db, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found)) {
	const std::string unnamed_pattern = std::string() + gcv_options.default_name +
					    "*";
	db_full_path **entry;

	for (BU_PTBL_FOR(entry, (db_full_path **), &found)) {
	    if (!renamed.count(DB_FULL_PATH_CUR_DIR(*entry)))
		for (ssize_t i = (*entry)->fp_len - 2; i >= 0; --i) {
		    bu_attribute_value_set avs;
		    AutoPtr<bu_attribute_value_set, bu_avs_free> autofree_avs(&avs);

		    if (db5_get_attributes(&db, &avs, (*entry)->fp_names[i]))
			bu_bomb("db5_get_attributes() failed");

		    if (!bu_strcmp(bu_avs_get(&avs, "rhino::type"), "ON_Layer")
			|| (bu_path_match(unnamed_pattern.c_str(), (*entry)->fp_names[i]->d_namep, 0)
			    && bu_path_match("IDef*", (*entry)->fp_names[i]->d_namep, 0))) {
			const std::string prefix = (*entry)->fp_names[i]->d_namep;
			std::string suffix = ".s";
			std::size_t num = 1;

			while ((prefix + suffix) != DB_FULL_PATH_CUR_DIR(*entry)->d_namep
			       && db_lookup(&db, (prefix + suffix).c_str(), false))
			    suffix = "_" + lexical_cast<std::string>(++num) + ".s";

			renamed.insert(std::make_pair(DB_FULL_PATH_CUR_DIR(*entry),
						      DB_FULL_PATH_CUR_DIR(*entry)->d_namep));

			if (db_rename(&db, DB_FULL_PATH_CUR_DIR(*entry), (prefix + suffix).c_str()))
			    bu_bomb("db_rename() failed");

			break;
		    }
		}

	    if (renamed.count(DB_FULL_PATH_CUR_DIR(*entry)) && (*entry)->fp_len > 1) {
		bu_ptbl stack = BU_PTBL_INIT_ZERO;
		AutoPtr<bu_ptbl, bu_ptbl_free> autofree_stack(&stack);

		if (!db_comb_mvall((*entry)->fp_names[(*entry)->fp_len - 2], &db,
				   renamed.at(DB_FULL_PATH_CUR_DIR(*entry)).c_str(),
				   DB_FULL_PATH_CUR_DIR(*entry)->d_namep, &stack))
		    bu_bomb("db_comb_mvall() failed");
	    }
	}
    }

    // ensure that all solids are below regions
    db_search_free(&found);
    BU_PTBL_INIT(&found);

    if (0 > db_search(&found, DB_SEARCH_TREE,
		      "-type shape -not -below -type region", 0, NULL, &db, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found)) {
	db_full_path **entry;

	for (BU_PTBL_FOR(entry, (db_full_path **), &found)) {

	    // Sanity
	    if (!(*entry) || (*entry)->fp_len <= 0)
		continue;

	    std::string prefix = DB_FULL_PATH_CUR_DIR(*entry)->d_namep;
	    std::string suffix = ".r";

	    if (prefix.size() >= 2 && prefix.at(prefix.size() - 2) == '.'
		&& prefix.at(prefix.size() - 1) == 's')
		prefix.resize(prefix.size() - 2);

	    std::size_t num = 1;

	    while (db_lookup(&db, (prefix + suffix).c_str(), false))
		suffix = "_" + lexical_cast<std::string>(++num) + ".r";

	    const std::string region_name = prefix + suffix;

	    if ((*entry)->fp_len >= 2) {
		bu_ptbl stack = BU_PTBL_INIT_ZERO;
		AutoPtr<bu_ptbl, bu_ptbl_free> autofree_stack(&stack);

		if (!db_comb_mvall((*entry)->fp_names[(*entry)->fp_len - 2], &db,
				   DB_FULL_PATH_CUR_DIR(*entry)->d_namep, region_name.c_str(), &stack))
		    bu_bomb("db_comb_mvall() failed");
	    }

	    std::set<std::string> members;
	    members.insert(DB_FULL_PATH_CUR_DIR(*entry)->d_namep);
	    write_comb(*db.dbi_wdbp, region_name, members);

	    comb_to_region(db, region_name);

	    bool has_rgb = false, has_shader = false;

	    for (ssize_t i = (*entry)->fp_len - 2; i >= 0; --i) {
		bu_attribute_value_set avs;
		AutoPtr<bu_attribute_value_set, bu_avs_free> autofree_avs(&avs);

		if (db5_get_attributes(&db, &avs, (*entry)->fp_names[i]))
		    bu_bomb("db5_get_attributes() failed");

		if (!has_rgb)
		    if (const char * const rgb_attr = bu_avs_get(&avs, "rgb")) {
			has_rgb = true;

			if (db5_update_attribute(region_name.c_str(), "rgb", rgb_attr, &db)
			    || db5_update_attribute(region_name.c_str(), "color", rgb_attr, &db))
			    bu_bomb("db5_update_attribute() failed");
		    }

		if (!has_shader)
		    if (const char * const shader_attr = bu_avs_get(&avs, "shader")) {
			has_shader = true;

			if (db5_update_attribute(region_name.c_str(), "shader", shader_attr, &db)
			    || db5_update_attribute(region_name.c_str(), "oshader", shader_attr, &db))
			    bu_bomb("db5_update_attribute() failed");
		    }
	    }
	}
    }
}


HIDDEN int
rhino_read(gcv_context *context, const gcv_opts *gcv_options,
	   const void *UNUSED(options_data), const char *source_path)
{
    std::string root_name;
    {
	bu_vls temp = BU_VLS_INIT_ZERO;
	AutoPtr<bu_vls, bu_vls_free> autofree_temp(&temp);

	if (!bu_path_component(&temp, source_path, BU_PATH_BASENAME))
	    return 1;

	root_name = bu_vls_addr(&temp);
    }

    try {
	ONX_Model model;
	load_model(*gcv_options, source_path, model, root_name);
	import_model_layers(*context->dbip->dbi_wdbp, model, root_name);
	import_model_idefs(*context->dbip->dbi_wdbp, model);
	import_model_objects(*gcv_options, *context->dbip->dbi_wdbp, model);
    } catch (const InvalidRhinoModelError &exception) {
	std::cerr << "invalid input file ('" << exception.what() << "')\n";
	return 0;
    }

    polish_output(*gcv_options, *context->dbip);

    return 1;
}

HIDDEN int
rhino_can_read(const char *source_path)
{
    int fv;
    ON_String mSC;
    ON_3dmProperties mprop;
    if (!source_path) return 0;
    FILE *fp = ON::OpenFile(source_path,"rb");
    if (!fp) return 0;
    ON_BinaryFile file(ON::on_read3dm,fp);
    if (!file.Read3dmStartSection(&fv, mSC)) return 0;
    if (!file.Read3dmProperties(mprop)) return 0;
    return 1;
}


struct gcv_filter gcv_conv_rhino_read = {
    "Rhino Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_RHINO, rhino_can_read,
    NULL, NULL, rhino_read
};

static const gcv_filter * const filters[] = {&gcv_conv_rhino_read, NULL};


}


extern "C"
{
    extern const gcv_plugin gcv_plugin_info_s = {rhino_read::filters};
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
