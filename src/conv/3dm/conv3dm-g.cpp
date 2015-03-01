/*                   C O N V 3 D M - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file conv3dm-g.cpp
 *
 * Library for conversion of Rhino models (in .3dm files) to BRL-CAD databases.
 *
 */


#ifdef OBJ_BREP

/* interface header */
#include "conv3dm-g.hpp" // includes common.h

/* system headers */
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

/* implementation headers */
#include "bu/getopt.h"
#include "icv.h"
#include "vmath.h"
#include "../../libgcv/bot_solidity.h"


namespace
{


static const ON_UUID &ROOT_UUID = ON_nil_uuid;
static const char * const DEFAULT_NAME = "noname";


static struct InitOpenNURBS {
    InitOpenNURBS()
    {
	ON::Begin();
    }


    ~InitOpenNURBS()
    {
	ON::End();
    }
} init_opennurbs;


static inline void
check_return(int ret, const std::string &message)
{
    if (ret != 0)
	throw std::runtime_error(message);
}


static inline std::string
uuid2string(const ON_UUID &uuid)
{
    // UUID buffers must be >= 37 chars per openNURBS API
    const std::size_t UUID_LEN = 37;

    char buf[UUID_LEN];
    return ON_UuidToString(uuid, buf);
}


static inline std::string
w2string(const ON_wString &source)
{
    if (source)
	return ON_String(source).Array();
    else
	return "";
}


// used for checking ON_SimpleArray::At(),
// when accessing ONX_Model member objects referenced by index
template <typename T, typename A>
static inline const T &
at_ref(const A &array, int index)
{
    if (const T *ptr = array.At(index))
	return *ptr;
    else
	throw std::out_of_range("invalid index");
}


template <typename T, typename R, R(*Destructor)(T *)>
class AutoDestroyer
{
public:
    AutoDestroyer() : ptr(NULL) {}
    AutoDestroyer(T *vptr) : ptr(vptr) {}


    ~AutoDestroyer()
    {
	if (ptr) Destructor(ptr);
    }


    T *ptr;


private:
    AutoDestroyer(const AutoDestroyer &);
    AutoDestroyer &operator=(const AutoDestroyer &);
};


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
					   std::numeric_limits<ON__UINT32>::max());
    result.Data2 = static_cast<ON__UINT16>(drand48() *
					   std::numeric_limits<ON__UINT16>::max());
    result.Data3 = static_cast<ON__UINT16>(drand48() *
					   std::numeric_limits<ON__UINT16>::max());

    for (int i = 0; i < 8; ++i)
	result.Data4[i] = static_cast<unsigned char>(drand48() *
			  std::numeric_limits<unsigned char>::max());

    return result;
}


static inline bool
is_toplevel(const ON_Layer &layer)
{
    return (layer.m_parent_layer_id == ROOT_UUID)
	   && (layer.m_layer_id != ROOT_UUID);
}


// ONX_Model::Audit() fails to repair UUID issues on some platforms
// because ON_CreateUuid() is not implemented.
// This function repairs nil and duplicate UUIDs.
static void
repair_model_uuids(ONX_Model &model)
{
    std::set<ON_UUID, conv3dm::UuidCompare> uuids;
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


static const char *
get_object_suffix(const ON_Object &object)
{
    if (const ON_Geometry *geom = ON_Geometry::Cast(&object))
	if (geom->HasBrepForm())
	    return ".s";

    if (ON_Bitmap::Cast(&object))
	return ".pix";

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
get_basename(const std::string &path)
{
    std::vector<char> buf(path.size() + 1);
    bu_basename(&buf[0], path.c_str());
    return &buf[0];
}


static std::string
get_dirname(const std::string &path)
{
    char *buf = bu_dirname(path.c_str());
    std::string result = buf;
    bu_free(buf, "bu_dirname buffer");
    return result;
}


static void
xform2mat(const ON_Xform &source, mat_t dest)
{
    for (int row = 0; row < 4; ++row)
	for (int col = 0; col < 4; ++col)
	    dest[row * 4 + col] = source[row][col];
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
load_pix(const std::string &path, int width, int height)
{
    char buf[BUFSIZ]; // libicv currently requires BUFSIZ
    ICV_IMAGE_FORMAT format = icv_guess_file_format(path.c_str(), buf);
    AutoDestroyer<icv_image_t, int, icv_destroy> image(
	icv_read(path.c_str(), format, width, height));

    if (!image.ptr)
	throw std::runtime_error("icv_read() failed");

    // TODO import the image data after adding support to libicv
}


static std::string
clean_name(const std::string &input)
{
    if (input.empty())
	return DEFAULT_NAME;

    std::ostringstream ss;
    int index = 0;

    for (std::string::const_iterator it = input.begin();
	 it != input.end(); ++it, ++index)
	switch (*it) {
	    case '.':
	    case '-':
		ss.put(index > 0 ? *it : '_');
		break;


	    default:
		if (std::isalnum(*it))
		    ss.put(*it);
		else
		    ss.put('_');
	}

    return ss.str();
}


}


namespace conv3dm
{


inline bool
UuidCompare::operator()(const ON_UUID &left,
			const ON_UUID &right) const
{
    return ON_UuidCompare(&left, &right) == -1;
}


class RhinoConverter::Color
{
public:
    static Color random();

    Color();
    Color(unsigned char red, unsigned char green, unsigned char blue);
    explicit Color(const ON_Color &src);

    bool operator==(const Color &other) const;
    bool operator!=(const Color &other) const;

    const unsigned char *get_rgb() const;


private:
    unsigned char m_rgb[3];
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
RhinoConverter::ObjectManager::unique_name(std::string prefix,
	const char *suffix)
{
    if (std::isdigit(*prefix.rbegin()))
	prefix += '_';

    std::string name = prefix + suffix;
    int number = ++m_name_count_map[name];

    if (number <= 1)
	return name;

    if (number == 2) { // adjust numbering to start from 001
	for (std::map<ON_UUID, ObjectManager::ModelObject, UuidCompare>
	     ::iterator it = m_obj_map.begin(); it != m_obj_map.end(); ++it)
	    if (it->second.m_name == name) {
		it->second.m_name.insert(it->second.m_name.find_last_of('.'), "001");
		break;
	    }
    }

    std::ostringstream ss;
    ss << prefix << std::setw(3) << std::setfill('0') << number << suffix;
    return ss.str();
}


inline RhinoConverter::ObjectManager::ObjectManager() :
    m_obj_map(),
    m_name_count_map()
{}


void
RhinoConverter::ObjectManager::add(bool use_uuid, const ON_UUID &uuid,
				   const std::string &prefix, const char *suffix)
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
    if (!m_obj_map[parent_uuid].m_members.insert(member_uuid).second)
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
    return m_obj_map.find(uuid) != m_obj_map.end();
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


RhinoConverter::Color
RhinoConverter::Color::random()
{
    unsigned char red = static_cast<unsigned char>(255 * drand48());
    unsigned char green = static_cast<unsigned char>(255 * drand48());
    unsigned char blue = static_cast<unsigned char>(255 * drand48());

    return Color(red, green, blue);
}


inline RhinoConverter::Color::Color()
{
    m_rgb[0] = m_rgb[1] = m_rgb[2] = 0;
}


inline RhinoConverter::Color::Color(unsigned char red, unsigned char green,
				    unsigned char blue)
{
    m_rgb[0] = red;
    m_rgb[1] = green;
    m_rgb[2] = blue;
}


inline RhinoConverter::Color::Color(const ON_Color &src)
{
    m_rgb[0] = static_cast<unsigned char>(src.Red());
    m_rgb[1] = static_cast<unsigned char>(src.Green());
    m_rgb[2] = static_cast<unsigned char>(src.Blue());
}


inline bool
RhinoConverter::Color::operator==(const Color &other) const
{
    return m_rgb[0] == other.m_rgb[0]
	   && m_rgb[1] == other.m_rgb[1]
	   && m_rgb[2] == other.m_rgb[2];
}


inline bool
RhinoConverter::Color::operator!=(const Color &other) const
{
    return !operator==(other);
}


const unsigned char *
RhinoConverter::Color::get_rgb() const
{
    if (*this != Color(0, 0, 0))
	return m_rgb;
    else
	return NULL;
}


RhinoConverter::RhinoConverter(const std::string &output_path,
			       bool verbose_mode) :
    m_verbose_mode(verbose_mode),
    m_output_dirname(get_dirname(output_path)),
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

    int repair_count = 0;
    int num_problems = m_model.Audit(true, &repair_count, &m_log, NULL);

    if (!num_problems)
	return;

    m_log.Print("WARNING: Model is NOT valid. Attempting repairs.\n");

    // repair remaining UUID issues
    if (num_problems != repair_count) {
	repair_model_uuids(m_model);
	num_problems = m_model.Audit(true, &repair_count, NULL, NULL);
    }

    if (num_problems != repair_count) {
	m_log.Print("Repair failed. Remaining problems:\n");
	m_model.Audit(false, NULL, &m_log, NULL);
    } else
	m_log.Print("Repair successful; model is now valid.\n");
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
	const char *suffix;

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

	m_log.Print("Creating bitmap: %s\n", bitmap_name.c_str());
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
	    load_pix(path, bitmap->Width(), bitmap->Height());
	} catch (const std::runtime_error &) {
	    m_log.Print("Couldn't convert bitmap to pix\n");
	    m_log.Print("Extracted embedded bitmap to '%s'\n", path.c_str());
	    return;
	}

	std::remove(path.c_str());
    } else {
	try {
	    load_pix(w2string(bmap.m_bitmap_filename),
		     bmap.Width(), bmap.Height());
	} catch (const std::runtime_error &) {
	    m_log.Print("Couldn't convert bitmap to pix\n");
	    return;
	}
    }

    if (m_verbose_mode)
	m_log.Print("Successfully read bitmap\n");
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

	m_log.Print("Creating layer: %s\n", layer_name.c_str());
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
    int ret = mk_comb(m_db, m_objects.get_name(layer.m_layer_id).c_str(),
		      &wmembers.l, false,
		      shader.first.c_str(), shader.second.c_str(), get_color(layer).get_rgb(),
		      0, 0, 0, 0, do_inherit, false, false);

    check_return(ret, "mk_comb()");
}


void
RhinoConverter::create_all_idefs()
{
    m_log.Print("Creating instance definitions...\n");

    for (int i = 0; i < m_model.m_idef_table.Count(); ++i) {
	const ON_InstanceDefinition &idef = m_model.m_idef_table[i];
	const std::string &idef_name = m_objects.get_name(idef.m_uuid);

	m_log.Print("Creating instance definition: %s\n", idef_name.c_str());
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
    int ret = mk_comb(m_db, idef_name.c_str(), &wmembers.l, false,
		      NULL, NULL, NULL, 0, 0, 0, 0, do_inherit, false, false);

    check_return(ret, "mk_comb()");
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
    int ret = mk_comb(m_db, iref_name.c_str(), &members.l, false,
		      shader.first.c_str(), shader.second.c_str(),
		      get_color(iref_attrs).get_rgb(),
		      0, 0, 0, 0, do_inherit, false, false);

    check_return(ret, "mk_comb()");

    const ON_UUID &parent_uuid =
	at_ref<ON_Layer>(m_model.m_layer_table, iref_attrs.m_layer_index).m_layer_id;
    m_objects.register_member(parent_uuid, iref_attrs.m_uuid);
}


RhinoConverter::Color
RhinoConverter::get_color(const ON_3dmObjectAttributes &obj_attrs) const
{
    if (m_random_colors)
	return Color::random();

    const Color UNSET_COLOR(128, 128, 128);
    Color result = Color(m_model.WireframeColor(obj_attrs));
    return result != UNSET_COLOR ? result : Color(0, 0, 0);
}


RhinoConverter::Color
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
	m_log.Print("Creating comb for high-level geometry\n");

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
    int ret = mk_comb(m_db, comb_name.c_str(), &members.l, false,
		      shader.first.c_str(), shader.second.c_str(),
		      get_color(geom_attrs).get_rgb(),
		      0, 0, 0, 0, do_inherit, false, false);

    check_return(ret, "mk_comb()");

    m_objects.register_member(parent_layer.m_layer_id, comb_uuid);
}


void
RhinoConverter::create_brep(const ON_Brep &brep,
			    const ON_3dmObjectAttributes &brep_attrs)
{
    const std::string &brep_name = m_objects.get_name(brep_attrs.m_uuid);

    int ret = mk_brep(m_db, brep_name.c_str(), const_cast<ON_Brep *>(&brep));
    check_return(ret, "mk_brep()");
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

    if (num_vertices == 0 || num_faces == 0) {
	m_log.Print("Mesh has no content; skipping...\n");
	return;
    }

    std::vector<fastf_t> vertices(num_vertices * 3);

    for (std::size_t i = 0; i < num_vertices; ++i) {
	const ON_3fPoint &point = mesh.m_V[static_cast<int>(i)];
	vertices[i * 3] = point.x;
	vertices[i * 3 + 1] = point.y;
	vertices[i * 3 + 2] = point.z;
    }

    std::vector<int> faces(num_faces * 3);

    for (std::size_t i = 0; i < num_faces; ++i) {
	const ON_MeshFace &face = mesh.m_F[static_cast<int>(i)];
	faces[i * 3] = face.vi[0];
	faces[i * 3 + 1] = face.vi[1];
	faces[i * 3 + 2] = face.vi[2];
    }

    unsigned char mode;
    {
	rt_bot_internal bot;
	bot.num_faces = num_faces;
	bot.num_vertices = num_vertices;
	bot.faces = &faces[0];
	bot.vertices = &vertices[0];
	mode = gcv_bot_is_solid(&bot) ? RT_BOT_SOLID : RT_BOT_PLATE;
    }

    std::vector<fastf_t> thicknesses;
    AutoDestroyer<bu_bitv, void, bu_bitv_free> mbitv;

    if (mode == RT_BOT_PLATE) {
	const fastf_t PLATE_THICKNESS = 1;
	thicknesses.assign(num_faces, PLATE_THICKNESS);
	mbitv.ptr = bu_bitv_new(num_faces);
    }

    if (mesh.m_FN.Count() != mesh.m_F.Count()) {
	int ret = mk_bot(m_db, mesh_name.c_str(), mode, orientation, 0,
			 num_vertices, num_faces, &vertices[0], &faces[0],
			 thicknesses.empty() ? NULL : &thicknesses[0],
			 mbitv.ptr);

	check_return(ret, "mk_bot()");
    } else {
	mesh.UnitizeFaceNormals();
	std::vector<fastf_t> normals(num_faces * 3);

	for (std::size_t i = 0; i < num_faces; ++i) {
	    const ON_3fVector &vec = mesh.m_FN[static_cast<int>(i)];
	    normals[i * 3] = vec.x;
	    normals[i * 3 + 1] = vec.y;
	    normals[i * 3 + 2] = vec.z;
	}

	std::vector<int> face_normals(num_faces * 3);

	for (std::size_t i = 0; i < num_faces; ++i) {
	    face_normals[i * 3] = static_cast<int>(i);
	    face_normals[i * 3 + 1] = static_cast<int>(i);
	    face_normals[i * 3 + 2] = static_cast<int>(i);
	}

	int ret = mk_bot_w_normals(m_db, mesh_name.c_str(), mode, orientation,
				   RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS,
				   num_vertices, num_faces, &vertices[0], &faces[0],
				   thicknesses.empty() ? NULL : &thicknesses[0],
				   mbitv.ptr, num_faces, &normals[0], &face_normals[0]);

	check_return(ret, "mk_bot_w_normals()");
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
	    m_log.Print("Skipping object of type %s\n", object.ClassId()->ClassName());

	    if (m_verbose_mode) {
		object.Dump(m_log);
		m_log.PopIndent();
		m_log.Print("\n");
	    }
	}
    }

    if (num_created != num_objects)
	m_log.Print("Created %d of %d objects\n", num_created, num_objects);
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
	    create_brep(*brep, object_attrs);
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


#endif //OBJ_BREP

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
