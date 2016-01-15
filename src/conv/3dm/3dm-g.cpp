/*                       3 D M - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file 3dm-g.cpp
 *
 * Conversion of Rhino models (.3dm files) into BRL-CAD databases.
 *
 */


#ifdef OBJ_BREP

#include "common.h"

/* system headers */
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

/* implementation headers */
#include "bu/getopt.h"
#include "gcv/util.h"
#include "icv.h"
#include "wdb.h"


namespace
{


static const ON_UUID &ROOT_UUID = ON_nil_uuid;
static const char * const DEFAULT_NAME = "noname";


static const struct _initOpenNURBS {
    _initOpenNURBS()
    {
	ON::Begin();
    }


    ~_initOpenNURBS()
    {
	ON::End();
    }
} _init_opennurbs;


static std::string
get_basename(const std::string &path)
{
    std::vector<char> buf(path.size() + 1);
    bu_basename(&buf[0], path.c_str());
    return &buf[0];
}


template <typename T> inline void
autoptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoPtr");
}


template <typename T, typename R = void, R free_fn(T *) = autoptr_wrap_bu_free>
struct AutoPtr {
    explicit AutoPtr(T *vptr = NULL) : ptr(vptr) {}

    ~AutoPtr()
    {
	free();
    }

    void free()
    {
	if (ptr) free_fn(ptr);

	ptr = NULL;
    }


    T *ptr;


private:
    AutoPtr(const AutoPtr &source);
    AutoPtr &operator=(const AutoPtr &source);
};


class Color
{
public:
    static Color random();

    Color(unsigned char red, unsigned char green, unsigned char blue);
    explicit Color(const ON_Color &src);

    bool operator==(const Color &other) const;
    bool operator!=(const Color &other) const;

    const unsigned char *get_rgb() const;


private:
    unsigned char m_rgb[3];
};


Color
Color::random()
{
    unsigned char color[3];

    for (int i = 0; i < 3; ++i)
	color[i] = static_cast<unsigned char>(255.0 * drand48() + 0.5);

    return Color(V3ARGS(color));
}


inline Color::Color(unsigned char red, unsigned char green,
		    unsigned char blue)
{
    VSET(m_rgb, red, green, blue);
}


inline Color::Color(const ON_Color &src)
{
    m_rgb[0] = static_cast<unsigned char>(src.Red());
    m_rgb[1] = static_cast<unsigned char>(src.Green());
    m_rgb[2] = static_cast<unsigned char>(src.Blue());
}


inline bool
Color::operator==(const Color &other) const
{
    return m_rgb[0] == other.m_rgb[0]
	   && m_rgb[1] == other.m_rgb[1]
	   && m_rgb[2] == other.m_rgb[2];
}


inline bool
Color::operator!=(const Color &other) const
{
    return !operator==(other);
}


const unsigned char *
Color::get_rgb() const
{
    if (*this != Color(0, 0, 0) && *this != Color(128, 128, 128))
	return m_rgb;
    else
	return NULL;
}


struct UuidCompare {
    inline bool operator()(const ON_UUID &left, const ON_UUID &right) const
    {
	return ON_UuidCompare(&left, &right) == -1;
    }
};


static std::string
uuid2string(const ON_UUID &uuid)
{
    // UUID buffers must be >= 37 chars per openNURBS API
    const std::size_t UUID_LEN = 37;

    char buf[UUID_LEN];
    return ON_UuidToString(uuid, buf);
}


static std::string
w2string(const ON_wString &source)
{
    if (source)
	return ON_String(source).Array();
    else
	return "";
}


static void
xform2mat(const ON_Xform &source, mat_t dest)
{
    for (int row = 0; row < 4; ++row)
	for (int col = 0; col < 4; ++col)
	    dest[row * 4 + col] = source[row][col];
}


static inline bool
is_toplevel(const ON_Layer &layer)
{
    return layer.m_parent_layer_id == ROOT_UUID && layer.m_layer_id != ROOT_UUID;
}


// used for checking ON_SimpleArray::At(),
// when accessing ONX_Model member objects referenced by index
template <typename T, typename Array>
static const T &
at_ref(const Array &array, int index)
{
    if (const T *ptr = array.At(index))
	return *ptr;
    else
	throw std::out_of_range("invalid index");
}


// according to openNURBS documentation, their own ON_CreateUuid()
// only works on Windows, but the implementation shows it's also
// implemented for XCode builds.  when it fails, we pretend to make a
// uuid.  FIXME: this is a great candidate for a bu_uuid() function
// (but with real uuid behavior).
static ON_UUID
generate_uuid()
{
    ON_UUID result;

    if (ON_CreateUuid(result))
	return result;

    // sufficient for our use here, but
    // officially UUIDv4 also requires certain bits to be set
    result.Data1 = static_cast<ON__UINT32>(drand48() *
					   std::numeric_limits<ON__UINT32>::max() + 0.5);
    result.Data2 = static_cast<ON__UINT16>(drand48() *
					   std::numeric_limits<ON__UINT16>::max() + 0.5);
    result.Data3 = static_cast<ON__UINT16>(drand48() *
					   std::numeric_limits<ON__UINT16>::max() + 0.5);

    for (int i = 0; i < 8; ++i)
	result.Data4[i] = static_cast<unsigned char>(drand48() *
			  std::numeric_limits<unsigned char>::max() + 0.5);

    return result;
}


// ONX_Model::Audit() fails to repair UUID issues on some platforms
// because ON_CreateUuid() is not implemented.
// This function repairs nil and duplicate UUIDs.
static void
repair_model_uuids(ONX_Model &model)
{
    std::set<ON_UUID, UuidCompare> uuids;
    uuids.insert(ROOT_UUID);

    for (int i = 0; i < model.m_idef_table.Count(); ++i) {
	ON_UUID &idef_uuid = model.m_idef_table[i].m_uuid;

	while (!uuids.insert(idef_uuid).second)
	    idef_uuid = generate_uuid();
    }

    for (int i = 0; i < model.m_object_table.Count(); ++i) {
	ON_UUID &object_uuid = model.m_object_table[i].m_attributes.m_uuid;

	while (!uuids.insert(object_uuid).second)
	    object_uuid = generate_uuid();
    }

    for (int i = 0; i < model.m_layer_table.Count(); ++i) {
	ON_UUID &layer_uuid = model.m_layer_table[i].m_layer_id;

	while (!uuids.insert(layer_uuid).second)
	    layer_uuid = generate_uuid();
    }
}


static std::string
get_object_suffix(const ON_Object &object)
{
    if (ON_Bitmap::Cast(&object))
	return ".pix";

    if (const ON_Geometry *geom = ON_Geometry::Cast(&object))
	if (geom->HasBrepForm())
	    return ".s";

    switch (object.ObjectType()) {
	case ON::layer_object:
	case ON::instance_definition:
	case ON::instance_reference:
	    return ".c";

	case ON::brep_object:
	case ON::mesh_object:
	    return ".s";

	default:
	    throw std::invalid_argument("no suffix for given object");
    }
}


static std::string
clean_name(const std::string &input)
{
    if (input.empty())
	return DEFAULT_NAME;

    std::string result;
    std::size_t index = 0;

    for (std::string::const_iterator it = input.begin();
	 it != input.end(); ++it, ++index)
	switch (*it) {
	    case '.':
	    case '-':
		result.push_back(index ? *it : '_');
		break;


	    default:
		if (std::isalnum(*it))
		    result.push_back(*it);
		else
		    result.push_back('_');
	}

    return result;
}


static std::string
extract_bitmap(const std::string &dir_path, const std::string &filename,
	       const ON_EmbeddedBitmap &bitmap)
{
    std::string path = dir_path + BU_DIR_SEPARATOR + "extracted_" + filename;
    int counter = 0;

    while (bu_file_exists(path.c_str(), NULL)) {
	std::ostringstream ss;
	ss << dir_path << BU_DIR_SEPARATOR << "extracted_" <<
	   ++counter << "_" << filename;

	path = ss.str();
    }

    std::ofstream file(path.c_str(), std::ofstream::binary);
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    file.write(static_cast<const char *>(bitmap.m_buffer),
	       static_cast<std::streamsize>(bitmap.m_sizeof_buffer));
    file.close();

    return path;
}


static void
load_pix(const std::string &path, std::size_t width, std::size_t height)
{
    struct bu_vls c = BU_VLS_INIT_ZERO;
    mime_image_t format = MIME_IMAGE_UNKNOWN;

    if (bu_path_component(&c, path.c_str(), (path_component_t)MIME_IMAGE)) {
	format = (mime_image_t)bu_file_mime_int(bu_vls_addr(&c));
    }
    bu_vls_free(&c);

    AutoPtr<icv_image_t, int, icv_destroy> image(icv_read(path.c_str(), format,
	    width, height));

    if (!image.ptr)
	throw std::runtime_error("icv_read() failed");

    // TODO import the image data after adding support to libicv
}


class RhinoConverter
{
public:
    RhinoConverter(const std::string &output_path, bool verbose_mode);
    ~RhinoConverter();


    void write_model(const std::string &path, bool use_uuidnames,
		     bool random_colors);


private:
    class ObjectManager
    {
    public:
	ObjectManager();

	void add(bool use_uuid, const ON_UUID &uuid, const std::string &prefix,
		 const std::string &suffix);
	void register_member(const ON_UUID &parent_uuid, const ON_UUID &member_uuid);
	void mark_idef_member(const ON_UUID &uuid);

	bool exists(const ON_UUID &uuid) const;
	const std::string &get_name(const ON_UUID &uuid) const;
	const std::set<ON_UUID, UuidCompare> &get_members(const ON_UUID &uuid) const;
	bool is_idef_member(const ON_UUID &uuid) const;


    private:
	struct ModelObject;

	std::string unique_name(const std::string &prefix, const std::string &suffix);

	std::map<ON_UUID, ModelObject, UuidCompare> m_obj_map;
	std::map<std::string, std::size_t> m_name_count_map;
    };


    RhinoConverter(const RhinoConverter &source);
    RhinoConverter &operator=(const RhinoConverter &source);

    void clean_model();
    void map_uuid_names(bool use_uuidnames);
    void create_all_bitmaps();
    void create_all_layers();
    void create_all_idefs();
    void create_all_objects();

    void create_bitmap(const ON_Bitmap &bitmap);
    void create_layer(const ON_Layer &layer);
    void create_idef(const ON_InstanceDefinition &idef);

    void create_object(const ON_Object &object,
		       const ON_3dmObjectAttributes &object_attrs);
    void create_geom_comb(const ON_3dmObjectAttributes &geom_attrs);

    void create_iref(const ON_InstanceRef &iref,
		     const ON_3dmObjectAttributes &iref_attrs);

    void create_brep(const ON_Brep &brep, const ON_3dmObjectAttributes &brep_attrs);

    void create_mesh(ON_Mesh mesh, const ON_3dmObjectAttributes &mesh_attrs);

    Color get_color(const ON_3dmObjectAttributes &obj_attrs) const;
    Color get_color(const ON_Layer &layer) const;

    std::pair<std::string, std::string> get_shader(int index) const;


    const bool m_verbose_mode;
    const std::string m_output_dirname;
    rt_wdb * const m_db;
    ON_TextLog m_log;
    ObjectManager m_objects;

    bool m_random_colors;
    ONX_Model m_model;
};


struct RhinoConverter::ObjectManager::ModelObject {
    std::string m_name;
    std::set<ON_UUID, UuidCompare> m_members;
    bool m_idef_member;

    ModelObject() :
	m_name(),
	m_members(),
	m_idef_member(false)
    {}
};


std::string
RhinoConverter::ObjectManager::unique_name(const std::string &prefix,
	const std::string &suffix)
{
    const std::string name = prefix + suffix;
    const std::size_t number = ++m_name_count_map[name];

    if (number <= 1)
	return name;

    if (number == 2) { // adjust numbering to start from 001
	for (std::map<ON_UUID, ObjectManager::ModelObject, UuidCompare>
	     ::iterator it = m_obj_map.begin(); it != m_obj_map.end(); ++it)
	    if (it->second.m_name == name) {
		it->second.m_name.insert(it->second.m_name.find_last_of('.'), "_001");
		break;
	    }
    }

    std::ostringstream ss;
    ss << prefix << '_' << std::setw(3) << std::setfill('0') << number << suffix;
    return ss.str();
}


inline RhinoConverter::ObjectManager::ObjectManager() :
    m_obj_map(),
    m_name_count_map()
{}


void
RhinoConverter::ObjectManager::add(bool use_uuid, const ON_UUID &uuid,
				   const std::string &prefix, const std::string &suffix)
{
    ModelObject object;

    if (use_uuid)
	object.m_name = uuid2string(uuid) + suffix;
    else
	object.m_name = unique_name(prefix, suffix);

    if (!m_obj_map.insert(std::make_pair(uuid, object)).second)
	throw std::invalid_argument("duplicate uuid");
}


void
RhinoConverter::ObjectManager::register_member(
    const ON_UUID &parent_uuid, const ON_UUID &member_uuid)
{
    if (!m_obj_map.at(parent_uuid).m_members.insert(member_uuid).second)
	throw std::invalid_argument("duplicate member uuid");
}


inline void
RhinoConverter::ObjectManager::mark_idef_member(const ON_UUID &uuid)
{
    m_obj_map.at(uuid).m_idef_member = true;
}


inline bool
RhinoConverter::ObjectManager::exists(const ON_UUID &uuid) const
{
    return m_obj_map.count(uuid) > 0;
}


inline const std::string &
RhinoConverter::ObjectManager::get_name(const ON_UUID &uuid) const
{
    return m_obj_map.at(uuid).m_name;
}


inline const std::set<ON_UUID, UuidCompare> &
RhinoConverter::ObjectManager::get_members(const ON_UUID &uuid) const
{
    return m_obj_map.at(uuid).m_members;
}


inline bool
RhinoConverter::ObjectManager::is_idef_member(const ON_UUID &uuid) const
{
    return m_obj_map.at(uuid).m_idef_member;
}


RhinoConverter::RhinoConverter(const std::string &output_path,
			       bool verbose_mode) :
    m_verbose_mode(verbose_mode),
    m_output_dirname(AutoPtr<char>(bu_dirname(output_path.c_str())).ptr),
    m_db(wdb_fopen(output_path.c_str())),
    m_log(),
    m_objects(),
    m_random_colors(false),
    m_model()
{
    if (!m_db)
	throw std::runtime_error("failed to open database");

    mk_id(m_db, "3dm -> g conversion");
}


RhinoConverter::~RhinoConverter()
{
    wdb_close(m_db);
}


void
RhinoConverter::write_model(const std::string &path, bool use_uuidnames,
			    bool random_colors)
{
    m_random_colors = random_colors;

    if (!m_model.Read(path.c_str(), &m_log))
	throw std::runtime_error("failed to read 3dm file");

    m_log.Print("Loaded 3dm model '%s'\n", path.c_str());

    clean_model();

    m_objects.add(use_uuidnames, ROOT_UUID, clean_name(get_basename(path)), ".c");

    map_uuid_names(use_uuidnames);
    create_all_bitmaps();
    create_all_idefs();
    create_all_objects();
    create_all_layers();

    m_model.Destroy();

    m_log.Print("Done.\n");
}


void
RhinoConverter::clean_model()
{
    m_model.Polish(); // fill in defaults

    repair_model_uuids(m_model);
    int repair_count;
    int num_problems = m_model.Audit(true, &repair_count, &m_log, NULL);

    if (num_problems) {
	if (num_problems == repair_count)
	    m_log.Print("Repair successful; model is now valid.\n");
	else
	    m_log.Print("WARNING: Repair of invalid model failed.\n");
    }
}


void
RhinoConverter::map_uuid_names(bool use_uuidnames)
{
    for (int i = 0; i < m_model.m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model.m_layer_table[i];
	const std::string name = clean_name(w2string(layer.m_name));
	m_objects.add(use_uuidnames, layer.m_layer_id, name, get_object_suffix(layer));
    }

    for (int i = 0; i < m_model.m_idef_table.Count(); ++i) {
	const ON_InstanceDefinition &idef = m_model.m_idef_table[i];
	const std::string name = clean_name(w2string(idef.m_name));
	m_objects.add(use_uuidnames, idef.m_uuid, name, get_object_suffix(idef));
    }

    for (int i = 0; i < m_model.m_object_table.Count(); ++i) {
	const ON_3dmObjectAttributes &object_attrs =
	    m_model.m_object_table[i].m_attributes;
	const std::string name = clean_name(w2string(object_attrs.m_name));
	std::string suffix;

	try {
	    suffix = get_object_suffix(*m_model.m_object_table[i].m_object);
	} catch (const std::invalid_argument &) {
	    continue;
	}

	m_objects.add(use_uuidnames, object_attrs.m_uuid, name, suffix);
    }

    for (int i = 0; i < m_model.m_bitmap_table.Count(); ++i) {
	m_model.m_bitmap_table[i]->m_bitmap_id = generate_uuid();
	const ON_Bitmap &bitmap = *m_model.m_bitmap_table[i];

	std::string bitmap_name = clean_name(w2string(bitmap.m_bitmap_name));

	if (bitmap_name == DEFAULT_NAME)
	    bitmap_name = clean_name(get_basename(w2string(bitmap.m_bitmap_filename)));

	m_objects.add(false, bitmap.m_bitmap_id, bitmap_name,
		      get_object_suffix(bitmap));
    }
}


void
RhinoConverter::create_all_bitmaps()
{
    m_log.Print("Creating bitmaps...\n");

    for (int i = 0; i < m_model.m_bitmap_table.Count(); ++i) {
	const ON_Bitmap &bitmap = *m_model.m_bitmap_table[i];
	const std::string &bitmap_name = m_objects.get_name(bitmap.m_bitmap_id);

	if (m_verbose_mode)
	    m_log.Print("Creating bitmap '%s'\n", bitmap_name.c_str());

	create_bitmap(bitmap);
    }
}


void
RhinoConverter::create_bitmap(const ON_Bitmap &bmap)
{
    if (const ON_EmbeddedBitmap *bitmap = ON_EmbeddedBitmap::Cast(&bmap)) {
	const std::string filename = get_basename(w2string(bitmap->m_bitmap_filename));
	const std::string path = extract_bitmap(m_output_dirname, filename, *bitmap);

	try {
	    load_pix(path, static_cast<std::size_t>(bitmap->Width()),
		     static_cast<std::size_t>(bitmap->Height()));
	} catch (const std::runtime_error &) {
	    m_log.Print("Couldn't convert bitmap to pix"
			" -- extracted embedded bitmap to '%s'\n", path.c_str());
	    return;
	}

	std::remove(path.c_str());
    } else {
	try {
	    load_pix(w2string(bmap.m_bitmap_filename),
		     static_cast<std::size_t>(bmap.Width()),
		     static_cast<std::size_t>(bmap.Height()));
	} catch (const std::runtime_error &) {
	    m_log.Print("Couldn't convert bitmap to pix.\n");
	    return;
	}
    }

    if (m_verbose_mode)
	m_log.Print("Successfully read bitmap.\n");
}


void
RhinoConverter::create_all_layers()
{
    m_log.Print("Creating layers...\n");

    // nest layers
    for (int i = 0; i < m_model.m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model.m_layer_table[i];
	m_objects.register_member(layer.m_parent_layer_id, layer.m_layer_id);
    }

    for (int i = 0; i < m_model.m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model.m_layer_table[i];
	const std::string &layer_name = m_objects.get_name(layer.m_layer_id);

	if (m_verbose_mode)
	    m_log.Print("Creating layer '%s'\n", layer_name.c_str());

	create_layer(layer);
    }

    // create root layer
    create_layer(ON_Layer());
}


void
RhinoConverter::create_layer(const ON_Layer &layer)
{
    const std::set<ON_UUID, UuidCompare> &members =
	m_objects.get_members(layer.m_layer_id);

    wmember wmembers;
    BU_LIST_INIT(&wmembers.l);

    for (std::set<ON_UUID, UuidCompare>::const_iterator it = members.begin();
	 it != members.end(); ++it) {
	if (m_objects.is_idef_member(*it)) continue;

	mk_addmember(m_objects.get_name(*it).c_str(), &wmembers.l, NULL, WMOP_UNION);
    }

    const std::pair<std::string, std::string> shader =
	get_shader(layer.m_material_index);

    const bool do_inherit = !m_random_colors && is_toplevel(layer);
    const int ret = mk_comb(m_db, m_objects.get_name(layer.m_layer_id).c_str(),
			    &wmembers.l, false,
			    shader.first.c_str(), shader.second.c_str(), get_color(layer).get_rgb(),
			    0, 0, 0, 0, do_inherit, false, false);

    if (ret)
	throw std::runtime_error("mk_comb() failed");
}


void
RhinoConverter::create_all_idefs()
{
    m_log.Print("Creating instance definitions...\n");

    for (int i = 0; i < m_model.m_idef_table.Count(); ++i) {
	const ON_InstanceDefinition &idef = m_model.m_idef_table[i];
	const std::string &idef_name = m_objects.get_name(idef.m_uuid);

	if (m_verbose_mode)
	    m_log.Print("Creating instance definition '%s'\n", idef_name.c_str());

	create_idef(idef);
    }
}


void
RhinoConverter::create_idef(const ON_InstanceDefinition &idef)
{
    wmember wmembers;
    BU_LIST_INIT(&wmembers.l);

    for (int i = 0; i < idef.m_object_uuid.Count(); ++i) {
	if (!m_objects.exists(idef.m_object_uuid[i]))
	    continue;

	const std::string &member_name = m_objects.get_name(idef.m_object_uuid[i]);
	m_objects.mark_idef_member(idef.m_object_uuid[i]);
	mk_addmember(member_name.c_str(), &wmembers.l, NULL, WMOP_UNION);
    }

    const std::string &idef_name = m_objects.get_name(idef.m_uuid);
    const bool do_inherit = false;
    const int ret = mk_comb(m_db, idef_name.c_str(), &wmembers.l, false,
			    NULL, NULL, NULL, 0, 0, 0, 0, do_inherit, false, false);

    if (ret)
	throw std::runtime_error("mk_comb() failed");
}


void
RhinoConverter::create_iref(const ON_InstanceRef &iref,
			    const ON_3dmObjectAttributes &iref_attrs)
{
    const std::string &iref_name = m_objects.get_name(iref_attrs.m_uuid);
    const std::pair<std::string, std::string> shader =
	get_shader(iref_attrs.m_material_index);

    const std::string &member_name =
	m_objects.get_name(iref.m_instance_definition_uuid);


    mat_t matrix;
    xform2mat(iref.m_xform, matrix);

    wmember members;
    BU_LIST_INIT(&members.l);
    mk_addmember(member_name.c_str(), &members.l, matrix, WMOP_UNION);

    const bool do_inherit = false;
    const int ret = mk_comb(m_db, iref_name.c_str(), &members.l, false,
			    shader.first.c_str(), shader.second.c_str(),
			    get_color(iref_attrs).get_rgb(),
			    0, 0, 0, 0, do_inherit, false, false);

    if (ret)
	throw std::runtime_error("mk_comb() failed");

    const ON_UUID &parent_uuid =
	at_ref<ON_Layer>(m_model.m_layer_table, iref_attrs.m_layer_index).m_layer_id;
    m_objects.register_member(parent_uuid, iref_attrs.m_uuid);
}


Color
RhinoConverter::get_color(const ON_3dmObjectAttributes &obj_attrs) const
{
    if (m_random_colors)
	return Color::random();

    return Color(m_model.WireframeColor(obj_attrs));
}


Color
RhinoConverter::get_color(const ON_Layer &layer) const
{
    if (m_random_colors)
	return Color::random();

    return Color(layer.m_color);
}


std::pair<std::string, std::string>
RhinoConverter::get_shader(int index) const
{
    const std::pair<std::string, std::string> DEFAULT_SHADER("plastic", "");
    const ON_Material *material = m_model.m_material_table.At(index);

    if (!material)
	return DEFAULT_SHADER;

    std::ostringstream args;
    args << "{";
    args << " tr " << material->m_transparency;
    args << " re " << material->m_reflectivity;
    args << " sp " << material->m_specular;
    args << " di " << material->m_diffuse;
    args << " ri " << material->m_index_of_refraction;
    args << " sh " << material->m_shine;
    // args << " ex " << ??;
    args << " em " << material->m_emission;
    args << " }";

    return std::make_pair(DEFAULT_SHADER.first, args.str());
}


void
RhinoConverter::create_geom_comb(const ON_3dmObjectAttributes &geom_attrs)
{
    const ON_Layer &parent_layer =
	at_ref<ON_Layer>(m_model.m_layer_table, geom_attrs.m_layer_index);

    if (m_objects.is_idef_member(geom_attrs.m_uuid) || !is_toplevel(parent_layer)) {
	m_objects.register_member(parent_layer.m_layer_id, geom_attrs.m_uuid);
	return;
    }

    // stand-alone geometry at high levels of the hierarchy
    // so create material information for it

    if (m_verbose_mode)
	m_log.Print("Creating comb for high-level geometry.\n");

    const std::string &geom_name = m_objects.get_name(geom_attrs.m_uuid);
    const ON_UUID comb_uuid = generate_uuid();
    m_objects.add(false, comb_uuid, geom_name, ".c");
    const std::string &comb_name = m_objects.get_name(comb_uuid);
    const std::pair<std::string, std::string> shader
	= get_shader(geom_attrs.m_material_index);

    wmember members;
    BU_LIST_INIT(&members.l);
    mk_addmember(geom_name.c_str(), &members.l, NULL, WMOP_UNION);

    const bool do_inherit = false;
    const int ret = mk_comb(m_db, comb_name.c_str(), &members.l, false,
			    shader.first.c_str(), shader.second.c_str(),
			    get_color(geom_attrs).get_rgb(),
			    0, 0, 0, 0, do_inherit, false, false);

    if (ret)
	throw std::runtime_error("mk_comb() failed");

    m_objects.register_member(parent_layer.m_layer_id, comb_uuid);
}


void
RhinoConverter::create_brep(const ON_Brep &brep,
			    const ON_3dmObjectAttributes &brep_attrs)
{
    const std::string &brep_name = m_objects.get_name(brep_attrs.m_uuid);

    if (mk_brep(m_db, brep_name.c_str(), const_cast<ON_Brep *>(&brep)))
	throw std::runtime_error("mk_brep() failed");

    create_geom_comb(brep_attrs);
}


void
RhinoConverter::create_mesh(ON_Mesh mesh,
			    const ON_3dmObjectAttributes &mesh_attrs)
{
    const std::string &mesh_name = m_objects.get_name(mesh_attrs.m_uuid);

    mesh.ConvertQuadsToTriangles();
    mesh.CombineIdenticalVertices();
    mesh.Compact();

    const std::size_t num_vertices = static_cast<std::size_t>(mesh.m_V.Count());
    const std::size_t num_faces = static_cast<std::size_t>(mesh.m_F.Count());

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
	    bu_bomb("logic error: unknown orientation");
    }

    if (!num_vertices || !num_faces) {
	m_log.Print("Mesh has no content; skipping...\n");
	return;
    }

    std::vector<fastf_t> vertices(num_vertices * 3);

    for (std::size_t i = 0; i < num_vertices; ++i) {
	const ON_3fPoint &current_vertex = mesh.m_V[static_cast<int>(i)];
	VMOVE(&vertices[i * 3], current_vertex);
    }

    std::vector<int> faces(num_faces * 3);

    for (std::size_t i = 0; i < num_faces; ++i) {
	const ON_MeshFace &current_face = mesh.m_F[static_cast<int>(i)];
	VMOVE(&faces[i * 3], current_face.vi);
    }

    unsigned char mode;
    {
	rt_bot_internal bot;
	bot.magic = RT_BOT_INTERNAL_MAGIC;
	bot.orientation = orientation;
	bot.num_faces = num_faces;
	bot.num_vertices = num_vertices;
	bot.faces = &faces[0];
	bot.vertices = &vertices[0];
	mode = gcv_bot_is_solid(&bot) ? RT_BOT_SOLID : RT_BOT_PLATE;
    }

    std::vector<fastf_t> thicknesses;
    AutoPtr<bu_bitv, void, bu_bitv_free> mbitv;

    if (mode == RT_BOT_PLATE) {
	const fastf_t PLATE_THICKNESS = 1;
	thicknesses.assign(num_faces, PLATE_THICKNESS);
	mbitv.ptr = bu_bitv_new(num_faces);
    }

    if (mesh.m_FN.Count() != mesh.m_F.Count()) {
	const int ret = mk_bot(m_db, mesh_name.c_str(), mode, orientation, 0,
			       num_vertices, num_faces, &vertices[0], &faces[0],
			       thicknesses.empty() ? NULL : &thicknesses[0],
			       mbitv.ptr);

	if (ret)
	    throw std::runtime_error("mk_bot() failed");
    } else {
	mesh.UnitizeFaceNormals();
	std::vector<fastf_t> normals(num_faces * 3);

	for (std::size_t i = 0; i < num_faces; ++i) {
	    const ON_3fVector &current_normal = mesh.m_FN[static_cast<int>(i)];
	    VMOVE(&normals[i * 3], current_normal);
	}

	std::vector<int> face_normals(num_faces * 3);

	for (std::size_t i = 0; i < num_faces; ++i)
	    VSETALL(&face_normals[i * 3], static_cast<int>(i));

	const int ret = mk_bot_w_normals(m_db, mesh_name.c_str(), mode, orientation,
					 RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS,
					 num_vertices, num_faces, &vertices[0], &faces[0],
					 thicknesses.empty() ? NULL : &thicknesses[0],
					 mbitv.ptr, num_faces, &normals[0], &face_normals[0]);

	if (ret)
	    throw std::runtime_error("mk_bot_w_normals() failed");
    }

    create_geom_comb(mesh_attrs);
}


void
RhinoConverter::create_all_objects()
{
    m_log.Print("Creating objects...\n");

    const int num_objects = m_model.m_object_table.Count();
    int num_created = 0;

    for (int i = 0; i < num_objects; ++i) {
	const ON_Object &object = *m_model.m_object_table[i].m_object;
	const ON_3dmObjectAttributes &object_attrs =
	    m_model.m_object_table[i].m_attributes;

	if (m_verbose_mode)
	    m_log.Print("Object %d of %d...\n", i + 1, num_objects);

	if (m_objects.exists(object_attrs.m_uuid)) {
	    create_object(object, object_attrs);
	    ++num_created;
	} else {
	    m_log.Print("Skipping object of type %s. Name: '%s'\n",
			object.ClassId()->ClassName(),
			w2string(object_attrs.m_name).c_str());

	    if (m_verbose_mode) {
		object.Dump(m_log);
		m_log.PopIndent();
		m_log.Print("\n");
	    }
	}
    }

    if (num_created != num_objects)
	m_log.Print("Created %d of %d objects.\n", num_created, num_objects);
}


void
RhinoConverter::create_object(const ON_Object &object,
			      const ON_3dmObjectAttributes &object_attrs)
{
    if (const ON_Brep *brep = ON_Brep::Cast(&object)) {
	create_brep(*brep, object_attrs);
	return;
    }

    if (const ON_Geometry *geom = ON_Geometry::Cast(&object))
	if (geom->HasBrepForm()) {
	    ON_Brep *brep = geom->BrepForm();

	    try {
		create_brep(*brep, object_attrs);
	    } catch (const std::runtime_error &) {
		delete brep;
		throw;
	    }

	    delete brep;
	    return;
	}

    if (const ON_Mesh *mesh = ON_Mesh::Cast(&object)) {
	create_mesh(*mesh, object_attrs);
	return;
    }

    if (const ON_InstanceRef *instref = ON_InstanceRef::Cast(&object)) {
	create_iref(*instref, object_attrs);
	return;
    }

    bu_bomb("logic error: should never reach here");
}


}


int
main(int argc, char **argv)
{
    const char * const usage =
	"Usage: 3dm-g [-v] [-r] [-u] -o output_file.g input_file.3dm\n";

    bool verbose_mode = false;
    bool random_colors = false;
    bool use_uuidnames = false;
    const char *output_path = NULL;
    const char *input_path;

    int c;

    while ((c = bu_getopt(argc, argv, "o:vruh?")) != -1) {
	switch (c) {
	    case 'o':
		output_path = bu_optarg;
		break;

	    case 'v':
		verbose_mode = true;
		break;

	    case 'r':
		random_colors = true;
		break;

	    case 'u':
		use_uuidnames = true;
		break;

	    default:
		std::cerr << usage;
		return 1;
	}
    }

    if (bu_optind != argc - 1 || !output_path) {
	std::cerr << usage;
	return 1;
    }

    input_path = argv[bu_optind];

    try {
	RhinoConverter converter(output_path, verbose_mode);
	converter.write_model(input_path, use_uuidnames, random_colors);
    } catch (const std::logic_error &e) {
	std::cerr << "Conversion failed: " << e.what() << '\n';
	return 1;
    } catch (const std::runtime_error &e) {
	std::cerr << "Conversion failed: " << e.what() << '\n';
	return 1;
    }

    return 0;
}


#else


#include "common.h"

#include <iostream>


int
main()
{
    std::cerr <<
	      "ERROR: Boundary Representation object support is not available"
	      "with this compilation of BRL-CAD.\n";
    return 1;
}


#endif


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
