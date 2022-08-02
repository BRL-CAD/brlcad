/*                  R H I N O _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016-2022 United States Government as represented by
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

void
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
clean_name(const ON_wString chk_name, const std::string default_name = "Default")
{
    /* jump through some hoops to make sure we have a name in std::string format */
    std::string ret = default_name;
    if (chk_name.IsNotEmpty())
        ret = std::string(ON_String(chk_name).Array());

    /* remove spaces, slashes and non-standard characters */
    bu_vls scrub = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&scrub, "%s", ret.c_str());
    bu_vls_simplify(&scrub, nullptr, " _\0", " _\0");
    ret = ret.size() > 0 ? std::string(bu_vls_cstr(&scrub)) : default_name;   /* somemtimes we scrub out the entire name */
    
    return ret;
}

/* loop over ModelGeometry to check for non-unique names. Updates are reflected in the model */
HIDDEN void
chk_geometry_names(ONX_Model& model, const char* default_name)
{
    std::unordered_map <std::string, int> used_names;			/* used names, times used */
    std::vector<std::pair<ON_UUID, ON_ModelComponent*>>to_remove;	/* remove ID, replacing copy */
    ON_ModelComponent::Type curr_type = ON_ModelComponent::Type::ModelGeometry;
    ONX_ModelComponentIterator it(model, curr_type);

    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference()) {
        std::string name = clean_name(cr.ModelComponent()->Name(), default_name);

        /* ensure name is unique */
        if (used_names.find(name) == used_names.end()) {
            used_names.insert({ name, 0 });
            /* if the changed the name at all we need to update */
            if (name != default_name &&
                cr.ModelComponent()->NameIsNotEmpty() &&
                name == std::string(ON_String(cr.ModelComponent()->Name()).Array()))
                continue;
        } else {
            /* find non-conflicting name */
            auto handle = used_names.find(name);
            while (used_names.find(name + "_" + std::to_string(++handle->second)) != used_names.end())
                ;
            name.append("_" + std::to_string(handle->second));
        }

        /* make a copy of the component with the non-conflicting name */
        ON_ModelComponent* cpy = cr.ModelComponent()->Duplicate();
        cpy->SetName(ON_wString(name.c_str()));

        /* this is a linked list iteration, so we cannot add/delete inside the loop.
         * keep track of offender/copy for clean up later
         */
        to_remove.push_back(std::make_pair(cr.ModelComponent()->Id(), cpy));
    }
    
    /* do the removes and replacing additions */
    for (size_t j = 0; j < to_remove.size(); j++) {
        model.RemoveModelComponent(curr_type, to_remove[j].first);
        model.AddManagedModelComponent(to_remove[j].second);
    }
}

/* loads model and then checks and updates geometry names to be unique */
HIDDEN void
load_model(const char* default_name, const std::string& path, ONX_Model& model)
{
    if (!model.Read(path.c_str()))
	throw InvalidRhinoModelError("ONX_Model::Read() failed.\n\nNote:  if this file was saved from Rhino3D, make sure it was saved using\nRhino's v5 format or lower - newer versions of the 3dm format are not\ncurrently supported by BRL-CAD.");

    chk_geometry_names(model, default_name);
}

void
write_geometry(rt_wdb &wdb, const std::string &name, const ON_Brep &brep)
{
    ON_Brep *b = const_cast<ON_Brep *>(&brep);
    if (mk_brep(&wdb, name.c_str(), (void *)b))
	bu_bomb("mk_brep() failed");
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
 *          0 when geometry is valid, but no new geometry is written - ie InstanceRef's
 *         -1 when geoemtry is not written
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
    } else if (ON_InstanceRef::Cast(geometry)) {
        /* InstanceRef are valid geometry but their referenced geometry is already written.
         * ie do nothing here
         */
        return 0;
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
        return -1;
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

void
write_layer_comb(rt_wdb &wdb, const std::string &name,
	   const std::unordered_map<std::string, std::vector<fastf_t*>> &members,
	   const char *shader_name = NULL, const char *shader_options = NULL,
	   const unsigned char *rgb = NULL)
{
    wmember wmembers;
    BU_LIST_INIT(&wmembers.l);

    for (auto mem : members)
        for (auto mat : mem.second) {
            /* NOTE: we load the matrix vector with either a valid transformation matrix
             * or nullptr if we want the identity. When an object is referenced, there is
             * a redundant 'identity' object that we do not want to write
             */
            if (mem.second.size() == 1 || mat)
                mk_addmember(mem.first.c_str(), &wmembers.l, mat, WMOP_UNION);
            
            /* cleanup memory */
            delete mat;
        }


    if (mk_comb(&wdb, name.c_str(), &wmembers.l, false, shader_name, shader_options,
		rgb, 0, 0, 0, 0, false, false, false))
	bu_bomb("mk_comb() failed");
}

void
import_object(rt_wdb &wdb, const std::string &name,
	      const ON_InstanceRef * const iref,
	      const ONX_Model &model,
	      const char *shader_name, const char *shader_options, const unsigned char *rgb)
{
    ON_ModelComponentReference idef_reference = model.ComponentFromId(ON_ModelComponent::Type::InstanceDefinition, iref->m_instance_definition_uuid);
    const ON_InstanceDefinition* idef = ON_InstanceDefinition::FromModelComponentRef(idef_reference,nullptr);
    if (!idef)
	return;

    mat_t matrix;

    for (std::size_t i = 0; i < 4; ++i)
	for (std::size_t j = 0; j < 4; ++j)
	    matrix[4 * i + j] = iref->m_xform[i][j];

    std::set<std::string> members;
    members.insert(ON_String(idef->Name()).Array());

    write_comb(wdb, name, members, matrix, shader_name, shader_options, rgb);
}

void
import_model_objects(const gcv_opts &gcv_options, rt_wdb &wdb,
		     ONX_Model &model)
{
    size_t total_count = 0;
    size_t success_count = 0;
    std::unordered_map<std::string, ON_UUID>to_remove;
    ON_ModelComponent::Type type = ON_ModelComponent::Type::ModelGeometry;
    ONX_ModelComponentIterator it(model, type);

    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
    {
	total_count++;

	const ON_ModelGeometryComponent *mg = ON_ModelGeometryComponent::Cast(cr.ModelComponent());
	if (!mg)
	    continue;
        
	std::string name = std::string(ON_String(mg->Name()).Array());
        const std::string member_name = name + ".s";

	Shader shader;
	unsigned char rgb[3];
	bool own_shader, own_rgb;

	const ON_3dmObjectAttributes *attributes = mg->Attributes(nullptr);
	get_object_material(attributes, model, shader, rgb, own_shader, own_rgb);

	const ON_Geometry *g = mg->Geometry(nullptr);
	if (g) {
            int write_ret = write_geometry(wdb, member_name, g);

            if (write_ret == 1) {
                std::unordered_map<std::string, std::vector<fastf_t*>>members;
                members[member_name].push_back(nullptr);

                write_layer_comb(wdb, name, members, own_shader ? shader.first.c_str() : NULL,
			   own_shader ? shader.second.c_str() : NULL, own_rgb ? rgb : NULL);

                if (attributes) {
                    ON_String uuid;
                    const char *uuid_str = ON_UuidToString(attributes->m_uuid, uuid);
                    int ret1 = db5_update_attribute(name.c_str(), "rhino::type", mg->ClassId()->ClassName(), wdb.dbip);
                    int ret2 = db5_update_attribute(name.c_str(), "rhino::uuid", uuid_str, wdb.dbip);
                    if (ret1 || ret2)
                        bu_bomb("db5_update_attribute() failed");
                }
                ++success_count;
            } else if (write_ret == 0)
                ++success_count;
              else
                to_remove.insert(std::make_pair(member_name, mg->Id()));
	} else {
	    if (gcv_options.verbosity_level)
		std::cerr << "skipped " << name << ", no geometry associated with ModelGeometryComponent (?)\n";
	}
    }

    /* cleanup the geometry not written */
    for (auto itr : to_remove) {
        if (gcv_options.verbosity_level)
            bu_log("WARNING: cannot handle type [%s] for object [%s] -- skipping\n", 
                    ON_ObjectTypeToString(model.ModelGeometryComponentFromId(itr.second).Geometry(nullptr)->ObjectType()),
                    itr.first.c_str());
        model.RemoveModelComponent(type, itr.second);
    }

    if (gcv_options.verbosity_level && (success_count != total_count))
	std::cerr << "imported " << success_count << " of " << total_count << " objects\n";
}

void
import_idef(rt_wdb &wdb, std::string &name, const ON_InstanceDefinition *idef, const ONX_Model &model)
{
    // TODO - I think this needs to eventually be translated into a scale
    // matrix component over the members, if we have anything other than the
    // global unit...
    //double munit = idef->UnitSystem().MillimetersPerUnit(ON_DBL_QNAN);
    //std::cout << name << " units: " << bu_units_string(munit) << "\n";

    std::unordered_map<std::string, std::vector<fastf_t*>> members;
    const ON_SimpleArray<ON_UUID>& g_ids = idef->InstanceGeometryIdList();
    for (int i = 0; i < g_ids.Count(); i++) {
	const ON_ModelComponentReference& m_cr = model.ComponentFromId(ON_ModelComponent::Type::ModelGeometry, g_ids[i]);
	const ON_ModelGeometryComponent* mg = ON_ModelGeometryComponent::Cast(m_cr.ModelComponent());
	if (!mg)
	    continue;
	const ON_3dmObjectAttributes* attributes = mg->Attributes(nullptr);
	if (!attributes)
	    continue;
	const char *nstr = ON_String(attributes->Name()).Array();
	members[std::string(nstr)].push_back(nullptr);
    }

    write_layer_comb(wdb, name, members);
    ON_String nsu;
    const char *uuidstr = ON_UuidToString(idef->Id(), nsu);
    int ret1 = db5_update_attribute(name.c_str(), "rhino::type", idef->ClassId()->ClassName(), wdb.dbip);
    int ret2 = db5_update_attribute(name.c_str(), "rhino::uuid", uuidstr, wdb.dbip);
    if (ret1 || ret2)
	bu_bomb("db5_update_attribute() failed");
}

void
import_model_idefs(rt_wdb &wdb, const ONX_Model &model)
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

	std::string name(ON_String(idef->Name().Array()));
	import_idef(wdb, name, idef, model);
    }
}

// In OpenNURBS, an instance definition is a unique grouping of objects,
// essentially analogous to the grouping structure provided by BRL-CAD's comb.
// https://developer.rhino3d.com/guides/opennurbs/traverse-instance-definitions/
std::set<std::string>
get_all_idef_members(const ONX_Model &model)
{
    std::set<std::string> result;
    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::InstanceDefinition);
    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
    {
	const ON_InstanceDefinition *idef = ON_InstanceDefinition::Cast(cr.ModelComponent());
	if (!idef)
	    continue;
	ON_String iname = ON_String(idef->Name());
	const char *istr = iname.Array();
	ON_String nsu;
	const char *uuidstr = ON_UuidToString(idef->Id(), nsu);
	std::cout << "Reading idef instance " << uuidstr << ": " << istr << "\n";

	ON_SHA1_Hash chash = idef->ContentHash();
	ON_wString whstr = chash.ToString(false);
	ON_String hstr = ON_String(whstr);
	const char *ch = hstr.Array();
	std::cout << "Content hash: " << ch << "\n";

	const ON_SimpleArray<ON_UUID>& g_ids = idef->InstanceGeometryIdList();
	for (int i = 0; i < g_ids.Count(); i++) {
	    const ON_ModelComponentReference& m_cr = model.ComponentFromId(ON_ModelComponent::Type::ModelGeometry, g_ids[i]);
	    const ON_ModelGeometryComponent* mg = ON_ModelGeometryComponent::Cast(m_cr.ModelComponent());
	    if (!mg)
		continue;
	    result.insert(std::string(ON_String(mg->Name()).Array()));
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
// std::set<std::string>
std::unordered_map<std::string, std::vector<fastf_t*>>
get_layer_members(const ON_Layer *layer, const ONX_Model &model, const std::set<std::string> &UNUSED(model_idef_members))
{
    std::unordered_map<std::string, std::vector<fastf_t*>>members;
    {
	ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::Layer);
	for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
	{
	    const ON_Layer *cl = ON_Layer::Cast(cr.ModelComponent());
	    if (!cl)
		continue;
	    if (cl->ParentLayerId() == layer->ModelObjectId()) {
                members[clean_name(cl->Name())].push_back(nullptr);
	    }
	}
    }

    {
	ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::ModelGeometry);
	for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
	{
	    const ON_ModelGeometryComponent *mg = ON_ModelGeometryComponent::Cast(cr.ModelComponent());
	    if (!mg)
		continue;
	    const ON_3dmObjectAttributes* attributes = mg->Attributes(nullptr);
	    if (!attributes)
		continue;
            /* if the layer has refs, add them in accordingly */
	    if (const ON_InstanceRef* iref = ON_InstanceRef::Cast(mg->Geometry(nullptr))) {
                const ON_InstanceDefinition* idef = ON_InstanceDefinition::FromModelComponentRef(model.ComponentFromId(ON_ModelComponent::Type::InstanceDefinition, iref->m_instance_definition_uuid), nullptr);
                if (idef) {
                    const ON_SimpleArray<ON_UUID> id_list = idef->InstanceGeometryIdList();
                    for (int i = 0; i < id_list.Count(); i++) {
                        const ON_ModelGeometryComponent internal_gc = model.ModelGeometryComponentFromId(id_list[i]);
                        if (internal_gc.Geometry(nullptr)) {
                            ON_String internal_name = internal_gc.Name();
                            if (!internal_name.Array()) {
                                /* if our load_model is working correctly, we should never get here */
                                char cuid[37];
                                ON_UuidToString(internal_gc.Id(), cuid);
                                bu_log("ERROR: unnamed reference to uid: %s\n", cuid);
                                continue;
                            }
                            if (const ON_3dmObjectAttributes* internal_attributes = internal_gc.Attributes(nullptr)) {
                                if (internal_attributes->m_layer_index == layer->Index()) {
                                    const std::string internal_name_stdstr = std::string(internal_name.Array());
                                
                                    fastf_t* matrix = new fastf_t[16];
                                    for (std::size_t row = 0; row < 4; ++row)
                                        for (std::size_t col = 0; col < 4; ++col)
                                            matrix[4 * row + col] = iref->m_xform[row][col];
                                
                                    members[internal_name_stdstr].push_back(matrix);
                                }
                            }
                        }
                    }
                }
                continue;
            }
            ON_String ns(attributes->Name());
	    const char *gname = ns.Array();
	    ON_String nsu;
	    const char *uuidstr = ON_UuidToString(attributes->m_uuid, nsu);
	    if (!gname) {
		gname = uuidstr;
	    }
	    if (attributes->m_layer_index == layer->Index()) {
                members[std::string(gname)].push_back(nullptr);
	    }
	}
    }

//     std::set<std::string> result;
//     std::set_difference(members.begin(), members.end(), model_idef_members.begin(),
// 	    model_idef_members.end(), std::inserter(result, result.end()));

//     return result;
    return members;
}


// Each openNURBS layer is imported into the .g file as a comb
void
import_layer(rt_wdb &wdb, const ON_Layer *l, const ONX_Model &model, const std::set<std::string> &model_idef_members)
{
    const std::string name = clean_name(l->Name());
    ON_String lid;
    const char *lid_str = ON_UuidToString(l->Id(), lid);
    ON_Color wc = (l->Color() == ON_UNSET_COLOR) ? l->PlotColor() : l->Color();
    ON_ModelComponentReference mref = model.RenderMaterialFromIndex(l->RenderMaterialIndex());
    const ON_Material *mp = ON_Material::Cast(mref.ModelComponent());

    // TODO - there's also wc.Alpha() - should translate that to shader settings if not 0
    unsigned char rgb[3];
    rgb[0] = static_cast<unsigned char>(wc.Red());
    rgb[1] = static_cast<unsigned char>(wc.Green());
    rgb[2] = static_cast<unsigned char>(wc.Blue());

    const Shader shader = get_shader(mp);

    auto layer_children = get_layer_members(l, model, model_idef_members);

    write_layer_comb(wdb, name, layer_children, shader.first.c_str(), shader.second.c_str(), rgb);
    int ret1 = db5_update_attribute(name.c_str(), "rhino::type", l->ClassId()->ClassName(), wdb.dbip);
    int ret2 = db5_update_attribute(name.c_str(), "rhino::uuid", lid_str, wdb.dbip);
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
		    const std::string &root_name,
	            const std::set<std::string> &model_idef_members
       	            )
{

    ONX_ModelComponentIterator it(model, ON_ModelComponent::Type::Layer);
    for (ON_ModelComponentReference cr = it.FirstComponentReference(); false == cr.IsEmpty(); cr = it.NextComponentReference())
    {
	const ON_Layer *p = ON_Layer::Cast(cr.ModelComponent());
	if (!p)
	    continue;
        // bu_log("layer: %s -> index: %d\n", std::string(ON_String(p->Name().Array())).c_str(), p->Index());
	import_layer(wdb, p, model, model_idef_members);
    }

    ON_Layer root_layer;
    ON_wString rname(root_name.c_str());
    root_layer.SetName(rname.Array());
    import_layer(wdb, &root_layer, model, model_idef_members);
}


void
polish_output(const gcv_opts &UNUSED(gcv_options), db_i &UNUSED(db))
{
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

    try {
	// Use the openNURBS extenstion to read the whole 3dm file into memory.
	ONX_Model model;
	load_model(gcv_options->default_name, source_path, model);
	
	import_model_objects(*gcv_options, *context->dbip->dbi_wdbp, model);
	// The idef member set is static, but is used many times - generate it
	// once up front.
	const std::set<std::string> model_idef_members; // = get_all_idef_members(model);
	import_model_layers(*context->dbip->dbi_wdbp, model, root_name, model_idef_members);
	//import_model_idefs(*context->dbip->dbi_wdbp, model, model_idef_members);
    } catch (const InvalidRhinoModelError &exception) {
	std::cerr << "invalid input file ('" << exception.what() << "')\n";
	return 0;
    }

    polish_output(*gcv_options, *context->dbip);

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
