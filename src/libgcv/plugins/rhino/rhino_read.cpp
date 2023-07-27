/*                  R H I N O _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016-2023 United States Government as represented by
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
 * Import Rhino 3DM files into BRL-CAD
 *
 */


#include "common.h"

#include "bu/path.h"
#include "bu/units.h"
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
#include <unordered_map>


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
std::size_t array_length(const T(&)[length])
{
    return length;
}


template <typename Target, typename Source>
Target lexical_cast(const Source &value)
{
    std::stringstream interpreter;
    Target result;

    if (!(interpreter << value) ||
	!(interpreter >> result) ||
	!(interpreter >> std::ws).eof())
	bu_bomb("bad lexical_cast");

    return result;
}


void
comb_to_region(db_i &db, const std::string &name)
{
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

void
comb_region_name_check(std::map<const directory *, std::string> &renamed, db_i &db, const std::string &name)
{
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
const T &at(const Array<T> &array, std::size_t index)
{
    if (const T * const result = array.At(static_cast<unsigned>(index)))
	return *result;
    else
	throw InvalidRhinoModelError("invalid index");
}


template <template<typename> class Array, typename T>
T &at(Array<T> &array, std::size_t index)
{
    return const_cast<T &>(at(const_cast<const Array<T> &>(array), index));
}


template <typename T, typename Array>
const T &at(const Array &array, std::size_t index)
{
    if (const T * const result = array.At(static_cast<unsigned>(index)))
	return *result;
    else
	throw InvalidRhinoModelError("invalid index");
}


// ON_CreateUuid() is not implemented for all platforms.
// When it fails, we create a UUIDv4.
ON_UUID
generate_uuid()
{
    ON_UUID result;

    if (ON_CreateUuid(result))
	return result;

    // TODO - our fallback here should probably be based on bu_uuid_create (and
    // use this logic there if any of its details constitute improvements...)
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

/* cleans ON_wString of non-standard characters and removes duplicate spaces and underscores
 * RETURNS: std::string of modified name
 */
std::string
clean_name(const ON_wString chk_name, const std::string default_name)
{
    /* make sure we have a name in std::string format */
    std::string ret = default_name;
    if (chk_name.IsNotEmpty()) {
	ret = std::string(ON_String(chk_name).Array());

	/* remove spaces, slashes and non-standard characters */
	bu_vls scrub = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&scrub, "%s", ret.c_str());
	bu_vls_simplify(&scrub, nullptr, " _\0", " _\0");
	ret = ret.size() > 0 ? std::string(bu_vls_cstr(&scrub)) : default_name;   /* somemtimes we scrub out the entire name */
    }

    return ret;
}

/* check for non-unique names in map
 * RETURNS: string of non-conflicting name
 */
static std::string
chk_name(const ON_wString& _name, std::unordered_map <std::string, int>& used_names, const char* default_name)
{
    std::string name = clean_name(_name, default_name);

    /* ensure name is unique */
    if (used_names.find(name) == used_names.end()) {
        used_names.insert({ name, 0 });
    } else {
        /* find non-conflicting name */
        auto handle = used_names.find(name);
        while (used_names.find(name + "_" + std::to_string(++handle->second)) != used_names.end())
            ;
        name.append("_" + std::to_string(handle->second));
    }

    return name;
}

/* loads model and then maps component id to a unique name */
std::unordered_map <std::string, std::string>
load_model(const char* default_name, const std::string& path, ONX_Model& model)
{
    if (!model.Read(path.c_str()))
	throw InvalidRhinoModelError("ONX_Model::Read() failed.\n\nNote:  if this file was saved from Rhino3D, make sure it was saved using\nRhino's v5 format or lower - newer versions of the 3dm format are not\ncurrently supported by BRL-CAD.");

    std::unordered_map <std::string, std::string> uuid_to_name;
    std::unordered_map <std::string, int> used_names;			/* used names, times used */
    ON_ModelComponent::Type types[3] = {ON_ModelComponent::Type::Layer,
					ON_ModelComponent::Type::InstanceDefinition,
					ON_ModelComponent::Type::ModelGeometry
				       };
    for (int i = 0; i < 3; i++) {
	ON_ModelComponent::Type type = types[i];

	ONX_ModelComponentIterator it(model, type);    
	for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference()) {
	    std::string ret = chk_name(cr.ModelComponent()->Name(), used_names, default_name);
	    ON_String id;
	    ON_UuidToString(cr.ModelComponent()->Id(), id);
	    uuid_to_name.insert({std::string(id.Array()), ret});
	}
    }

    return uuid_to_name;
}

void
write_geometry(rt_wdb &wdb, const std::string &name, const ON_Brep &brep)
{
    ON_Brep *b = const_cast<ON_Brep *>(&brep);
    if (mk_brep(&wdb, name.c_str(), (void *)b))
	bu_bomb("mk_brep() failed");
    /* tag solid with converter attribute */
    if (db5_update_attribute(name.c_str(), "converter", "3dm - BRL", wdb.dbip))
	bu_bomb("db5_update_attribute() failed");
}


void
write_geometry(rt_wdb &wdb, const std::string &name, const ON_Mesh &in_mesh)
{
    ON_Mesh mesh = in_mesh;
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
    /* leverage opennurbs API to check mode */
    mode = (mesh.IsSolid() && mesh.IsManifold() && mesh.IsClosed() && mesh.IsOriented()) ? RT_BOT_SOLID : RT_BOT_PLATE;

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
	/* tag solid with converter attribute */
	if (db5_update_attribute(name.c_str(), "converter", "3dm - BRL", wdb.dbip))
	    bu_bomb("db5_update_attribute() failed");
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
	VSETALL(dest_face_normal, (int)i);
    }

    if (mk_bot_w_normals(&wdb, name.c_str(), mode, orientation,
			 RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS, num_vertices,
			 num_faces, &vertices.at(0), &faces.at(0),
			 thicknesses.empty() ? NULL : &thicknesses.at(0), bitv.ptr,
			 num_faces, &normals.at(0), &face_normals.at(0)))
	bu_bomb("mk_bot_w_normals() failed");
}

/* filters acceptable geometry types and writes accordingly
 * RETURNS: 1 when geometry is written
 *          0 when geometry is not
 */
int
write_geometry(rt_wdb &wdb, const std::string &name, const ON_Geometry *geometry)
{
    if (const ON_Brep * const brep = ON_Brep::Cast(geometry)) {
	write_geometry(wdb, name, *brep);
    } else if (const ON_Mesh * const mesh = ON_Mesh::Cast(geometry)) {
	write_geometry(wdb, name, *mesh);
    } else if (geometry->HasBrepForm()) {
	AutoPtr<ON_Brep, autoptr_wrap_delete> temp(geometry->BrepForm());
	write_geometry(wdb, name, *temp.ptr);
    } else if (const ON_SubD* subd = ON_SubD::Cast(geometry)) {
	/* NOTE: pending proper support for subdivision surfaces, 
	 * this is a very poor approximation just to have *something*
	 */
	if (ON_SubD* new_subd = subd->Duplicate()) {
	    /* The number of subdivisions */
	    const int count = 3;

	    /* use opennurbs Catmull-Clark subdivision algorithm */
	    new_subd->GlobalSubdivide(count);

	    /* Get control net mesh */
	    ON_Mesh* nmesh = new_subd->GetControlNetMesh(nullptr, ON_SubDGetControlNetMeshPriority::Geometry);
	    if (nullptr != nmesh) {
		write_geometry(wdb, name, *nmesh);
		delete nmesh;
	    }
	    delete new_subd;
	}
    } else {
        return 0;
    }

    return 1;
}


typedef std::pair<std::string, std::string> Shader;


Shader
get_shader(const ON_Material *im)
{
    std::ostringstream sstream;
    ON_Material dm;
    const ON_Material *material = (im) ? im : &dm;

    sstream << "{"
	    << " tr " << material->m_transparency
	    << " re " << material->m_reflectivity
	    << " sp " << 0
	    << " di " << 0.3
	    << " ri " << material->m_index_of_refraction
	    << " ex " << 0
	    << " sh " << material->m_shine
	    << " em " << material->m_emission
	    << " }";

    return std::make_pair("plastic", sstream.str());
}


void
get_object_material(const ON_3dmObjectAttributes *attributes,
		    const ONX_Model &model, Shader &out_shader, unsigned char *out_rgb,
		    bool &out_own_shader, bool &out_own_rgb)
{
    if (attributes) {
	const ON_Material *temp = ON_Material::Cast(model.RenderMaterialFromAttributes(*attributes).ModelComponent());
	out_shader = get_shader(temp);
	out_own_shader = attributes->MaterialSource() != ON::material_from_parent;

	ON_Color wc = model.WireframeColorFromAttributes(*attributes);
	out_rgb[0] = static_cast<unsigned char>(wc.Red());
	out_rgb[1] = static_cast<unsigned char>(wc.Green());
	out_rgb[2] = static_cast<unsigned char>(wc.Blue());
	out_own_rgb = attributes->ColorSource() != ON::color_from_parent;

	if (!out_rgb[0] && !out_rgb[1] && !out_rgb[2]) {
	    const ON_Material *material= ON_Material::Cast(model.RenderMaterialFromAttributes(*attributes).ModelComponent());
	    if (material) {
		out_rgb[0] = static_cast<unsigned char>(material->m_diffuse.Red());
		out_rgb[1] = static_cast<unsigned char>(material->m_diffuse.Green());
		out_rgb[2] = static_cast<unsigned char>(material->m_diffuse.Blue());
		out_own_rgb = out_own_shader;
	    } else {
		out_rgb[0] = static_cast<unsigned char>(0);
		out_rgb[1] = static_cast<unsigned char>(0);
		out_rgb[2] = static_cast<unsigned char>(0);
		out_own_rgb = false;
	    }
	}
    } else {
	out_shader = get_shader(NULL);
	out_rgb[0] = static_cast<unsigned char>(0);
	out_rgb[1] = static_cast<unsigned char>(0);
	out_rgb[2] = static_cast<unsigned char>(0);
	out_own_rgb = false;
	out_own_shader = false;
    }
}


void
write_comb(rt_wdb &wdb, const std::string &name,
	   const std::vector<std::string> &members, const mat_t matrix = NULL,
	   const char *shader_name = NULL, const char *shader_options = NULL,
	   const unsigned char *rgb = NULL)
{
    wmember wmembers;
    BU_LIST_INIT(&wmembers.l);

    for (std::vector<std::string>::const_iterator it = members.begin();
	 it != members.end(); ++it)
	mk_addmember(it->c_str(), &wmembers.l, const_cast<fastf_t *>(matrix),
		     WMOP_UNION);

    if (mk_comb(&wdb, name.c_str(), &wmembers.l, false, shader_name, shader_options,
		rgb, 0, 0, 0, 0, false, false, false))
	bu_bomb("mk_comb() failed");
}

void
import_object(rt_wdb &wdb, const std::string &name,
	      const ON_InstanceRef * const iref,
	      const ONX_Model &model,
	      const char *shader_name, const char *shader_options, const unsigned char *rgb,
	      std::unordered_map<std::string, std::string>& uuid_to_names)
{
    ON_ModelComponentReference idef_reference = model.ComponentFromId(ON_ModelComponent::Type::InstanceDefinition, iref->m_instance_definition_uuid);
    const ON_InstanceDefinition* idef = ON_InstanceDefinition::FromModelComponentRef(idef_reference,nullptr);
    if (!idef)
	return;

    mat_t matrix;

    for (std::size_t i = 0; i < 4; ++i)
	for (std::size_t j = 0; j < 4; ++j)
	    matrix[4 * i + j] = iref->m_xform[i][j];

    ON_String id;
    ON_UuidToString(idef->Id(), id);
    std::string mem_name = uuid_to_names[std::string(id.Array())];
    std::vector<std::string> members;
    members.push_back(mem_name);

    write_comb(wdb, name, members, matrix, shader_name, shader_options, rgb);
}

void
import_model_objects(const gcv_opts& gcv_options, rt_wdb& wdb, ONX_Model& model,
		     std::unordered_map<std::string, std::string>& uuid_to_names)
{
    size_t total_count = 0;
    size_t success_count = 0;
    std::unordered_map<std::string, ON_UUID> to_remove;
    ON_ModelComponent::Type type = ON_ModelComponent::Type::ModelGeometry;
    ONX_ModelComponentIterator it(model, type);

    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
    {
	total_count++;

	const ON_ModelGeometryComponent *mg = ON_ModelGeometryComponent::Cast(cr.ModelComponent());
	if (!mg)
	    continue;
        
	Shader shader;
	unsigned char rgb[3];
	bool own_shader, own_rgb;

	const ON_3dmObjectAttributes *attributes = mg->Attributes(nullptr);
	get_object_material(attributes, model, shader, rgb, own_shader, own_rgb);

	ON_String id;
	ON_UuidToString(mg->Id(), id);
	std::string name = uuid_to_names[std::string(id.Array())];

	if (const ON_Geometry *g = mg->Geometry(nullptr)) {
	    std::string member_name = name + ".s";
	    if (const ON_InstanceRef* const iref = ON_InstanceRef::Cast(g)) {
		++success_count;
		import_object(wdb, name, iref, model, own_shader ? shader.first.c_str() : NULL, own_shader ? shader.second.c_str() : NULL, own_rgb ? rgb : NULL, uuid_to_names);
	    } else if (write_geometry(wdb, member_name, g)) {
                ++success_count;
		std::vector<std::string> members_vec;
		members_vec.push_back(member_name);
		write_comb(wdb, name, members_vec, NULL, own_shader ? shader.first.c_str() : NULL, own_shader ? shader.second.c_str() : NULL, own_rgb ? rgb : NULL);
	    } else
                to_remove.insert(std::make_pair(member_name, mg->Id()));
	} else {
	    if (gcv_options.verbosity_level)
		std::cerr << "skipped " << name << ", no geometry associated with ModelGeometryComponent (?)\n";
	}
    }

    /* cleanup the geometry not written */
    for (auto& itr : to_remove) {
        if (gcv_options.verbosity_level)
            bu_log("WARNING: cannot handle type [%s] for object [%s] -- skipping\n", 
                    ON_ObjectTypeToString(model.ModelGeometryComponentFromId(itr.second).Geometry(nullptr)->ObjectType()),
                    itr.first.c_str());
        model.RemoveModelComponent(type, itr.second);
    }

    if (gcv_options.verbosity_level)
	std::cerr << "imported " << success_count << " of " << total_count << " objects\n";
}

void
import_idef(rt_wdb &wdb, const ON_InstanceDefinition *idef, const ONX_Model &model, 
	    std::unordered_map<std::string, std::string>& uuid_to_names)
{
    // TODO - I think this needs to eventually be translated into a scale
    // matrix component over the members, if we have anything other than the
    // global unit...
    //double munit = idef->UnitSystem().MillimetersPerUnit(ON_DBL_QNAN);
    //std::cout << name << " units: " << bu_units_string(munit) << "\n";

    std::vector<std::string> members_vec;
    const ON_SimpleArray<ON_UUID>& g_ids = idef->InstanceGeometryIdList();
    for (int i = 0; i < g_ids.Count(); i++) {
	const ON_ModelComponentReference& m_cr = model.ComponentFromId(ON_ModelComponent::Type::ModelGeometry, g_ids[i]);
	const ON_ModelGeometryComponent* mg = ON_ModelGeometryComponent::Cast(m_cr.ModelComponent());
	if (!mg)
	    continue;

	ON_String id;
	ON_UuidToString(mg->Id(), id);
	std::string name = uuid_to_names[std::string(id.Array())];
	members_vec.push_back(name);
    }

    ON_String id;
    ON_UuidToString(idef->Id(), id);
    std::string name = uuid_to_names[std::string(id.Array())];
    write_comb(wdb, name, members_vec);
    int ret1 = db5_update_attribute(name.c_str(), "rhino::type", idef->ClassId()->ClassName(), wdb.dbip);
    int ret2 = db5_update_attribute(name.c_str(), "rhino::uuid", id.Array(), wdb.dbip);
    if (ret1 || ret2)
	bu_bomb("db5_update_attribute() failed");
}

void
import_model_idefs(rt_wdb &wdb, const ONX_Model &model, std::unordered_map<std::string, std::string>& uuid_to_names)
{
    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::InstanceDefinition);
    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
    {
	const ON_InstanceDefinition *idef = ON_InstanceDefinition::Cast(cr.ModelComponent());
	if (!idef)
	    continue;

	if (idef->IsLinkedType()) {
	    std::cout << "Warning - instance " << idef->Name().Array() << " is defined using external file, unsupported\n";
	    continue;
	}

	import_idef(wdb, idef, model, uuid_to_names);
    }
}

// In OpenNURBS, an instance definition is a unique grouping of objects,
// essentially analogous to the grouping structure provided by BRL-CAD's comb.
// https://developer.rhino3d.com/guides/opennurbs/traverse-instance-definitions/
std::set<std::string>
get_all_idef_members(const ONX_Model &model, std::unordered_map<std::string, std::string>& uuid_to_names)
{
    std::set<std::string> result;
    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::InstanceDefinition);
    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
    {
	const ON_InstanceDefinition *idef = ON_InstanceDefinition::Cast(cr.ModelComponent());
	if (!idef)
	    continue;

	const ON_SimpleArray<ON_UUID>& g_ids = idef->InstanceGeometryIdList();
	for (int i = 0; i < g_ids.Count(); i++) {
	    const ON_ModelComponentReference& m_cr = model.ComponentFromId(ON_ModelComponent::Type::ModelGeometry, g_ids[i]);
	    const ON_ModelGeometryComponent* mg = ON_ModelGeometryComponent::Cast(m_cr.ModelComponent());
	    if (!mg)
		continue;
	    ON_String id;
	    ON_UuidToString(mg->Id(), id);
	    std::string name = uuid_to_names[std::string(id.Array())];
	    result.insert(name);
	}
    }

    return result;
#if 0
	    const ON_Geometry* g = mg->Geometry(nullptr);
	    if (!g)
		continue;
	    const ON_InstanceRef* iref = ON_InstanceRef::Cast(g);
	    if (iref) {
		const ON_ModelComponentReference& chr = model.ComponentFromId(ON_ModelComponent::Type::InstanceDefinition, iref->m_instance_definition_uuid);
		const ON_InstanceDefinition* cdef = ON_InstanceDefinition::Cast(chr.ModelComponent());
		ON_String cname = ON_String(cdef->Name());
		const char *cstr = cname.Array();
		ON_String cnsu;
		const char *cuuidstr = ON_UuidToString(cdef->Id(), cnsu);
		if (cstr) {
		    std::cout << "	Child idef instance " << cuuidstr << ": " << cstr << "\n";
		} else {
		    std::cout << "	Child idef instance " << cuuidstr << "\n";
		}
	    } else {
		ON_String cname = ON_String(mg->Name());
		const char *cstr = cname.Array();
		if (cstr) {
		    std::cout << "	Child idef object " << cstr << ": " << ON_ObjectTypeToString(g->ObjectType()) << "\n";
		} else {
		    std::cout << "	Child idef object, type: " << ON_ObjectTypeToString(g->ObjectType()) << "\n";
		}
	    }
#endif
}

// Layer members (translated to comb children in BRL-CAD terms) may be either
// other layers (additional combs) or objects.
//
// Unlike instance definitions, layer hierarchies are only used to group
// objects that are not otherwise the children of a comb.  An object is only
// assigned as a layer member if it is a member of the layer AND it is not used
// as a reference object for an instance elsewhere in the model.
//
// This avoids generating large combs for layers, while at the same time
// grouping objects that would otherwise be ungrouped top level objects in the
// .g file.
//
// TODO - should we also be doing something for the Group type?
std::vector<std::string>
get_layer_members(const ON_Layer *layer, const ONX_Model &model, 
		  std::unordered_map<std::string, std::string>& uuid_to_names)
{
    std::vector<std::string> members_vec;
    {
	ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::Layer);
	for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
	{
	    const ON_Layer *cl = ON_Layer::Cast(cr.ModelComponent());
	    if (!cl)
		continue;
	    if (cl->ParentLayerId() == layer->ModelObjectId()) {
                ON_String id;
		ON_UuidToString(cl->Id(), id);
		std::string name = uuid_to_names[std::string(id.Array())];
		members_vec.push_back(name);
	    }
	}
    }

    {
	ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::ModelGeometry);
	for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
	{
            if (!layer->IndexIsSet())
                continue;
	    const ON_ModelGeometryComponent *mg = ON_ModelGeometryComponent::Cast(cr.ModelComponent());
	    if (!mg)
		continue;
	    const ON_3dmObjectAttributes* attributes = mg->Attributes(nullptr);
	    if (!attributes)
		continue;

	    if (attributes->m_layer_index == layer->Index()) {
		ON_String id;
		ON_UuidToString(mg->Id(), id);
		std::string name = uuid_to_names[std::string(id.Array())];
		if (!mg->IsInstanceDefinitionGeometry())
		    members_vec.push_back(name);
	    }
	}
    }

    return members_vec;
}


// Each openNURBS layer is imported into the .g file as a comb
void
import_layer(rt_wdb &wdb, const ON_Layer *l, const ONX_Model &model, 
	     std::unordered_map<std::string, std::string>& uuid_to_names)
{
    ON_String id;
    ON_UuidToString(l->Id(), id);
    std::string name = uuid_to_names[std::string(id.Array())];
    ON_Color wc = (l->Color() == ON_UNSET_COLOR) ? l->PlotColor() : l->Color();
    ON_ModelComponentReference mref = model.RenderMaterialFromIndex(l->RenderMaterialIndex());
    const ON_Material *mp = ON_Material::Cast(mref.ModelComponent());

    // TODO - there's also wc.Alpha() - should translate that to shader settings if not 0
    unsigned char rgb[3] = {0};
    rgb[0] = static_cast<unsigned char>(wc.Red());
    rgb[1] = static_cast<unsigned char>(wc.Green());
    rgb[2] = static_cast<unsigned char>(wc.Blue());

    const Shader shader = get_shader(mp);

    auto layer_children = get_layer_members(l, model, uuid_to_names);

    write_comb(wdb, name, layer_children, NULL, shader.first.c_str(), shader.second.c_str(), rgb);
    int ret1 = db5_update_attribute(name.c_str(), "rhino::type", l->ClassId()->ClassName(), wdb.dbip);
    int ret2 = db5_update_attribute(name.c_str(), "rhino::uuid", id.Array(), wdb.dbip);
    if (ret1 || ret2)
	bu_bomb("db5_update_attribute() failed");

#if 0
    ON_String ltype = ON_ModelComponent::ComponentTypeToString(l->ComponentType());
    const char *ltype_str = ltype.Array();
    const ON__UINT64 content_version_number = l->ContentVersionNumber();
    std::cout << "Layer: " << name << "\n";
    std::cout << " Type: " << ltype_str << "\n";
    std::cout << "   Id: " << lid_str << "\n";
    std::cout << "   V#: " << content_version_number << "\n";

    if (wc != ON_UNSET_COLOR) {
	int lrgba[4] = {0};
	lrgba[0] = wc.Red();
	lrgba[1] = wc.Green();
	lrgba[2] = wc.Blue();
	lrgba[3] = wc.Alpha(); // (0 = opaque, 255 = transparent)
	std::cout << "  RGBA: " << lrgba[0] << "," << lrgba[1] << "," << lrgba[2] << "," << lrgba[3] << "\n";
    }

    std::cout << "Shader: " << shader.first.c_str() << "," << shader.second.c_str() << "\n";

    ON_wString wonstr;
    ON_TextLog log(wonstr);
    l->Dump(log);
    ON_String onstr = ON_String(wonstr);
    const char *ldesc = onstr.Array();
    bu_log("%s\n", ldesc);
#endif

}


void
import_model_layers(rt_wdb &wdb, const ONX_Model &model,
		    std::unordered_map<std::string, std::string>& uuid_to_names)
{

    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::Layer);
    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
    {
	const ON_Layer *p = ON_Layer::Cast(cr.ModelComponent());
	if (!p)
	    continue;

	import_layer(wdb, p, model, uuid_to_names);
    }

    ON_Layer root_layer;
    import_layer(wdb, &root_layer, model, uuid_to_names);
}


void
polish_output(const gcv_opts& gcv_options, db_i& db, rt_wdb& wdb)
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
	    if ((*entry)->fp_len < 2)	/* orphaned idefs at top level */
		continue;
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

			// If we have a .r suffix, we don't want to incorporate it into the solid name
			struct bu_vls basename = BU_VLS_INIT_ZERO;
			bu_path_component(&basename, (*entry)->fp_names[i]->d_namep, BU_PATH_EXTLESS);
			const std::string prefix = bu_vls_cstr(&basename);
			bu_vls_free(&basename);

			std::string suffix = ".s";
			std::size_t num = 0;

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

    /* instanceDefinitions still at the top level are not being used currently in the model - group them up */
    db_search_free(&found);
    BU_PTBL_INIT(&found);

    if (0 > db_search(&found, DB_SEARCH_RETURN_UNIQ_DP, 
		      "-attr rhino::type=ON_InstanceDefinition -not -below -type comb", 0, NULL, &db, NULL))
	bu_bomb("db_search() failed");

    if (BU_PTBL_LEN(&found)) {
	std::vector<std::string> members_vec;
	directory **entry;
	for (BU_PTBL_FOR(entry, (directory **), &found)) {
	    members_vec.push_back((*entry)->d_namep);
	}
	write_comb(wdb, "unused_model_idefs", members_vec);
    }

#if 0
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

	    std::size_t num = 0;

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

	    std::vector<std::string> members_vec;
	    members_vec.push_back(DB_FULL_PATH_CUR_DIR(*entry)->d_namep);
	    write_comb(wdb, region_name, members_vec);

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
#endif
}


int
rhino_read(gcv_context *context, const gcv_opts *gcv_options,
	   const void *UNUSED(options_data), const char *source_path)
{
    std::string root_name;
    {
	// Extract the root filename from the supplied path.  This will
	// be used to create the top level comb name in the .g file
	bu_vls temp = BU_VLS_INIT_ZERO;
	AutoPtr<bu_vls, bu_vls_free> autofree_temp(&temp);

	if (!bu_path_component(&temp, source_path, BU_PATH_BASENAME))
	    return 1;

	root_name = bu_vls_addr(&temp);
    }

    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);

    try {
	/* Use the openNURBS extenstion to read the whole 3dm file into memory. */
	ONX_Model model;
	std::unordered_map <std::string, std::string> uuid_to_names;
	uuid_to_names = load_model(gcv_options->default_name, source_path, model);
	/* the root layer created in import_model_layers always has 0 id - map it to root_name */
	uuid_to_names.insert({"00000000-0000-0000-0000-000000000000", root_name});

	import_model_objects(*gcv_options, *wdbp, model, uuid_to_names);
	import_model_idefs(*wdbp, model, uuid_to_names);
	import_model_layers(*wdbp, model, uuid_to_names);
    } catch (const InvalidRhinoModelError &exception) {
	std::cerr << "invalid input file ('" << exception.what() << "')\n";
	return 0;
    }

    polish_output(*gcv_options, *context->dbip, *wdbp);

    return 1;
}

int
rhino_can_read(const char *source_path)
{
    int fv;
    ON_String mSC;
    ON_3dmProperties mprop;
    if (!source_path) return 0;
    FILE *fp = ON::OpenFile(source_path,"rb");
    if (!fp) return 0;
    ON_BinaryFile file(ON::archive_mode::read3dm,fp);
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
