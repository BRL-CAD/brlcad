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
#include <cctype> // for isalnum()
#include <limits>
#include <sstream>
#include <stdexcept>

/* implementation headers */
#include "bu/getopt.h"
#include "icv.h"
#include "vmath.h"
#include "wdb.h"


namespace
{


static const ON_UUID &ROOT_UUID = ON_nil_uuid;
static const std::string DEFAULT_NAME = "noname";
static const std::pair<std::string, std::string> DEFAULT_SHADER("plastic", "");


static struct _InitOpenNURBS {
    _InitOpenNURBS()
    {
	ON::Begin();
    }

    ~_InitOpenNURBS()
    {
	ON::End();
    }
} _init_opennurbs;


static inline std::string
UUIDstr(const ON_UUID &uuid)
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
// when accessing ONX_Model objects referenced by index
template <typename T>
static inline T &ref(T *ptr)
{
    if (!ptr)
	throw std::out_of_range("invalid index");

    return *ptr;
}


template <typename T, typename R, R(*Destructor)(T*)>
struct AutoDestroyer {
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
    return (layer.m_parent_layer_id == ROOT_UUID) && (layer.m_layer_id != ROOT_UUID);
}


static std::string
strbasename(const std::string &path)
{
    std::vector<char> buf(path.size() + 1);
    bu_basename(&buf[0], path.c_str());
    return &buf[0];
}


static std::string
unique_name(std::map<std::string, int> &count_map,
	    const std::string &prefix,
	    const std::string &suffix)
{
    std::string name = prefix + suffix;
    int count = count_map[name]++;

    if (count) {
	std::ostringstream ss;
	ss << prefix << count << suffix;
	return ss.str();
    } else
	return name;
}


static void
xform2mat_t(const ON_Xform &source, mat_t dest)
{
    const int dmax = 4;
    for (int row = 0; row < dmax; ++row)
	for (int col = 0; col < dmax; ++col)
	    dest[row*dmax + col] = source[row][col];
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

    AutoDestroyer<icv_image_t, int, icv_destroy> image(icv_read(path.c_str(), format, width, height));
    if (image.ptr) {
	// TODO import the image data after adding support to libicv
    } else
	throw std::runtime_error("icv_read() failed");
}


static std::string
clean_name(const std::string &input)
{
    if (input.empty())
	return DEFAULT_NAME;

    std::ostringstream ss;
    int index = -1;
    for (std::string::const_iterator it = input.begin();
	 it != input.end(); ++it) {
	++index;
	switch (*it) {
	    case '.':
	    case '-':
	    case '_':
		ss.put(index > 0 ? *it : '_');
		break;


	    default:
		if (std::isalnum(*it))
		    ss.put(*it);
		else
		    ss.put('_');
	}
    }

    return ss.str();
}


}


namespace conv3dm
{


class RhinoConverter::Color
{
public:
    static Color random();


    Color();
    Color(unsigned char red, unsigned char green, unsigned char blue);
    Color(const ON_Color &src);

    bool operator==(const Color &rhs) const;

    const unsigned char *get_rgb() const;


private:
    unsigned char m_rgb[3];
};


struct RhinoConverter::ModelObject {
    std::string m_name;
    std::vector<std::string> m_children;
    bool m_idef_member;

    ModelObject() :
	m_name(),
	m_children(),
	m_idef_member(false)
    {}
};


RhinoConverter::Color
RhinoConverter::Color::random()
{
    unsigned char red = static_cast<unsigned char>(255 * drand48());
    unsigned char green = static_cast<unsigned char>(255 * drand48());
    unsigned char blue = static_cast<unsigned char>(255 * drand48());

    return Color(red, green, blue);
}


RhinoConverter::Color::Color()
{
    m_rgb[0] = m_rgb[1] = m_rgb[2] = 0;
}


RhinoConverter::Color::Color(unsigned char red, unsigned char green,
			     unsigned char blue)
{
    m_rgb[0] = red;
    m_rgb[1] = green;
    m_rgb[2] = blue;
}


RhinoConverter::Color::Color(const ON_Color &src)
{
    if (src.Red() < 0 || src.Red() > 255
	|| src.Green() < 0 || src.Green() > 255
	|| src.Blue() < 0 || src.Blue() > 255)
	throw std::invalid_argument("invalid ON_Color");

    m_rgb[0] = static_cast<unsigned char>(src.Red());
    m_rgb[1] = static_cast<unsigned char>(src.Green());
    m_rgb[2] = static_cast<unsigned char>(src.Blue());
}


bool
RhinoConverter::Color::operator==(const Color &rhs) const
{
    return m_rgb[0] == rhs.m_rgb[0]
	   && m_rgb[1] == rhs.m_rgb[1]
	   && m_rgb[2] == rhs.m_rgb[2];
}


const unsigned char *
RhinoConverter::Color::get_rgb() const
{
    return m_rgb;
}


RhinoConverter::RhinoConverter(const std::string &output_path,
			       bool verbose_mode) :
    m_verbose_mode(verbose_mode),
    m_use_uuidnames(false),
    m_random_colors(false),
    m_output_dirname(),
    m_obj_map(),
    m_name_count_map(),
    m_log(new ON_TextLog),
    m_model(new ONX_Model),
    m_db(NULL)
{
    char *buf = bu_dirname(output_path.c_str());
    m_output_dirname = buf;
    bu_free(buf, "bu_dirname buffer");

    m_db = wdb_fopen(output_path.c_str());
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
    m_use_uuidnames = use_uuidnames;
    m_random_colors = random_colors;

    if (!m_model->Read(path.c_str(), m_log.get()))
	throw std::runtime_error("failed to read 3dm file");

    m_log->Print("Loaded 3dm model '%s'\n", path.c_str());

    clean_model();

    if (m_verbose_mode)
	m_log->Print("Number of NURBS objects read: %d\n\n",
		     m_model->m_object_table.Count());

    m_obj_map[UUIDstr(ROOT_UUID)].m_name = strbasename(path);

    map_uuid_names();
    create_all_bitmaps();
    create_all_idefs();
    create_all_geometry();
    create_all_layers();

    m_model->Destroy();

    m_log->Print("Done.\n");
}


void
RhinoConverter::clean_model()
{
    if (!m_model->IsValid(m_log.get())) {
	m_log->Print("WARNING: Model is NOT valid. Attempting repairs.\n");

	m_model->Polish(); // fill in defaults

	int repair_count = 0;
	ON_SimpleArray<int> warnings;
	m_model->Audit(true, &repair_count, m_log.get(), &warnings); // repair

	m_log->Print("Repaired %d objects.\n", repair_count);
	for (int warn_i = 0; warn_i < warnings.Count(); ++warn_i)
	    m_log->Print("%s\n", warnings[warn_i]);

	if (m_model->IsValid(m_log.get()))
	    m_log->Print("Repair successful, model is now valid.\n");
	else
	    m_log->Print("WARNING: Repair unsuccessful, model is still NOT valid.\n");
    }
}


void
RhinoConverter::map_uuid_names()
{
    for (int i = 0; i < m_model->m_object_table.Count(); ++i) {
	const ON_Object *geom = m_model->m_object_table[i].m_object;
	const ON_3dmObjectAttributes &geom_attrs =
	    m_model->m_object_table[i].m_attributes;
	const std::string geom_uuid = UUIDstr(geom_attrs.m_uuid);

	std::string suffix = ".s";
	if (geom->ObjectType() == ON::instance_reference)
	    suffix = ".c";

	if (m_use_uuidnames)
	    m_obj_map[geom_uuid].m_name = geom_uuid + suffix;
	else
	    m_obj_map[geom_uuid].m_name =
		unique_name(m_name_count_map, clean_name(w2string(geom_attrs.m_name)), suffix);

    }


    for (int i = 0; i < m_model->m_idef_table.Count(); ++i) {
	const ON_InstanceDefinition &idef = m_model->m_idef_table[i];
	const std::string idef_uuid = UUIDstr(idef.m_uuid);

	if (m_use_uuidnames)
	    m_obj_map[idef_uuid].m_name = idef_uuid + ".c";
	else
	    m_obj_map[idef_uuid].m_name =
		unique_name(m_name_count_map, clean_name(w2string(idef.Name())), ".c");
    }


    for (int i = 0; i < m_model->m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model->m_layer_table[i];
	const std::string layer_uuid = UUIDstr(layer.m_layer_id);

	std::string suffix = ".c";
	if (!m_random_colors && is_toplevel(layer))
	    suffix = ".r";

	if (m_use_uuidnames)
	    m_obj_map[layer_uuid].m_name = layer_uuid + suffix;
	else
	    m_obj_map[layer_uuid].m_name =
		unique_name(m_name_count_map, clean_name(w2string(layer.m_name)), suffix);
    }


    for (int i = 0; i < m_model->m_bitmap_table.Count(); ++i) {
	m_model->m_bitmap_table[i]->m_bitmap_id = generate_uuid();

	const ON_Bitmap *bitmap = m_model->m_bitmap_table[i];
	const std::string bitmap_uuid = UUIDstr(bitmap->m_bitmap_id);

	if (m_use_uuidnames)
	    m_obj_map[bitmap_uuid].m_name = bitmap_uuid + ".pix";
	else {
	    std::string bitmap_name = clean_name(w2string(bitmap->m_bitmap_name));
	    if (bitmap_name == DEFAULT_NAME)
		bitmap_name = clean_name(strbasename(w2string(bitmap->m_bitmap_filename)));

	    m_obj_map[bitmap_uuid].m_name =
		unique_name(m_name_count_map, bitmap_name, ".pix");
	}
    }
}


void
RhinoConverter::create_all_bitmaps()
{
    m_log->Print("Creating bitmaps...\n");

    for (int i = 0; i < m_model->m_bitmap_table.Count(); ++i) {
	const ON_Bitmap *bitmap = m_model->m_bitmap_table[i];
	const std::string bitmap_uuid = UUIDstr(bitmap->m_bitmap_id);
	const std::string &bitmap_name = m_obj_map.at(bitmap_uuid).m_name;

	m_log->Print("Creating bitmap '%s'\n", bitmap_name.c_str());
	create_bitmap(bitmap);
    }
}


void
RhinoConverter::create_bitmap(const ON_Bitmap *bmap)
{
    if (const ON_EmbeddedBitmap *bitmap = ON_EmbeddedBitmap::Cast(bmap)) {
	const std::string filename = strbasename(w2string(bitmap->m_bitmap_filename));
	const std::string path = extract_bitmap(m_output_dirname, filename, *bitmap);

	try {
	    load_pix(path, bitmap->Width(), bitmap->Height());
	} catch (const std::runtime_error &) {
	    m_log->Print("Couldn't convert bitmap to pix\n");
	    m_log->Print("-- Extracted embedded bitmap to '%s'\n", path.c_str());
	    return;
	}

	std::remove(path.c_str());
    } else {
	try {
	    load_pix(w2string(bmap->m_bitmap_filename),
		     bitmap->Width(), bitmap->Height());
	} catch (const std::runtime_error &) {
	    m_log->Print("Couldn't convert bitmap to pix");
	    return;
	}
    }

    if (m_verbose_mode)
	m_log->Print("Successfully read bitmap\n");
}


void
RhinoConverter::nest_all_layers()
{
    for (int i = 0; i < m_model->m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model->m_layer_table[i];
	const std::string layer_uuid = UUIDstr(layer.m_layer_id);
	const std::string parent_layer_uuid = UUIDstr(layer.m_parent_layer_id);

	m_obj_map.at(parent_layer_uuid).m_children.push_back(layer_uuid);
    }
}


void
RhinoConverter::create_all_layers()
{
    m_log->Print("Creating layers...\n");

    nest_all_layers();

    for (int i = 0; i < m_model->m_layer_table.Count(); ++i) {
	const ON_Layer &layer = m_model->m_layer_table[i];
	const std::string &layer_name =
	    m_obj_map.at(UUIDstr(layer.m_layer_id)).m_name;

	m_log->Print("Creating layer '%s'\n", layer_name.c_str());
	create_layer(layer);
    }

    // create root layer
    create_layer(ON_Layer());
}


void
RhinoConverter::create_layer(const ON_Layer &layer)
{
    const std::string layer_uuid = UUIDstr(layer.m_layer_id);
    const ModelObject &layer_obj = m_obj_map.at(layer_uuid);
    const bool is_region = !m_random_colors && is_toplevel(layer);

    wmember members;
    BU_LIST_INIT(&members.l);
    for (std::vector<std::string>::const_iterator it = layer_obj.m_children.begin();
	 it != layer_obj.m_children.end(); ++it) {
	const ModelObject &obj = m_obj_map.at(*it);
	if (obj.m_idef_member) continue;
	mk_addmember(obj.m_name.c_str(), &members.l, NULL, WMOP_UNION);
    }

    const std::pair<std::string, std::string> shader =
	get_shader(layer.m_material_index);

    // FIXME code duplication
    Color color;
    if (m_random_colors)
	color = Color::random();
    else
	color = layer.m_color;

    if (color == Color(0, 0, 0)) {
	m_log->Print("Object has no color; setting color to red\n");
	color = Color(255, 0, 0);
    }

    const bool do_inherit = false;
    mk_comb(m_db, layer_obj.m_name.c_str(), &members.l, is_region,
	    shader.first.c_str(), shader.second.c_str(), color.get_rgb(),
	    0, 0, 0, 0, do_inherit, false, false);
}


void
RhinoConverter::create_all_idefs()
{
    m_log->Print("Creating instance definitions...\n");

    for (int i = 0; i < m_model->m_idef_table.Count(); ++i) {
	const ON_InstanceDefinition &idef = m_model->m_idef_table[i];
	const std::string &idef_name =
	    m_obj_map.at(UUIDstr(idef.m_uuid)).m_name;

	m_log->Print("Creating instance definition '%s'\n", idef_name.c_str());
	create_idef(idef);
    }
}


void
RhinoConverter::create_idef(const ON_InstanceDefinition &idef)
{
    wmember members;
    BU_LIST_INIT(&members.l);

    for (int i = 0; i < idef.m_object_uuid.Count(); ++i) {
	const std::string member_uuid = UUIDstr(idef.m_object_uuid[i]);
	ModelObject &member_obj = m_obj_map.at(member_uuid);

	mk_addmember(member_obj.m_name.c_str(), &members.l, NULL, WMOP_UNION);
	member_obj.m_idef_member = true;
    }

    const std::string &idef_name =
	m_obj_map.at(UUIDstr(idef.m_uuid)).m_name;
    mk_lfcomb(m_db, idef_name.c_str(), &members, false);
}


void
RhinoConverter::create_iref(const ON_InstanceRef &iref,
			    const ON_3dmObjectAttributes &iref_attrs)
{
    const std::string iref_uuid = UUIDstr(iref_attrs.m_uuid);
    const std::string &iref_name = m_obj_map.at(iref_uuid).m_name;
    const std::pair<std::string, std::string> shader =
	get_shader(iref_attrs.m_material_index);

    const std::string member_uuid = UUIDstr(iref.m_instance_definition_uuid);
    const std::string &member_name = m_obj_map.at(member_uuid).m_name;


    mat_t matrix;
    xform2mat_t(iref.m_xform, matrix);

    wmember members;
    BU_LIST_INIT(&members.l);
    mk_addmember(member_name.c_str(), &members.l, matrix, WMOP_UNION);

    const bool do_inherit = false;
    mk_comb(m_db, iref_name.c_str(), &members.l, false,
	    shader.first.c_str(), shader.second.c_str(),
	    get_color(iref_attrs).get_rgb(),
	    0, 0, 0, 0, do_inherit, false, false);

    const std::string parent_uuid =
	UUIDstr(ref(m_model->m_layer_table.At(iref_attrs.m_layer_index)).m_layer_id);
    m_obj_map.at(parent_uuid).m_children.push_back(iref_uuid);
}


RhinoConverter::Color
RhinoConverter::get_color(const ON_3dmObjectAttributes &obj_attrs) const
{
    Color color;
    if (m_random_colors)
	return Color::random();
    else
	switch (obj_attrs.ColorSource()) {
	    case ON::color_from_parent:
	    case ON::color_from_layer:
		color = ref(m_model->m_layer_table.At(obj_attrs.m_layer_index)).m_color;
		break;

	    case ON::color_from_object:
		color = obj_attrs.m_color;
		break;

	    case ON::color_from_material:
		color = ref(m_model->m_material_table.At(obj_attrs.m_material_index)).m_ambient;
		break;

	    default:
		throw std::logic_error("unknown color source");
	}


    if (color == Color(0, 0, 0)) {
	m_log->Print("Object has no color; setting color to red\n");
	return Color(255, 0, 0);
    } else
	return color;
}


std::pair<std::string, std::string>
RhinoConverter::get_shader(int index) const
{
    const ON_Material *material = m_model->m_material_table.At(index);

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
	ref(m_model->m_layer_table.At(geom_attrs.m_layer_index));

    const std::string parent_layer_uuid = UUIDstr(parent_layer.m_layer_id);
    const std::string geom_uuid = UUIDstr(geom_attrs.m_uuid);
    const ModelObject &geom_obj = m_obj_map.at(geom_uuid);

    if (geom_obj.m_idef_member || !is_toplevel(parent_layer)) {
	m_obj_map.at(parent_layer_uuid).m_children.push_back(geom_uuid);
	return;
    }


    // stand-alone geometry at high levels of the hierarchy
    // so create material information for it

    if (m_verbose_mode)
	m_log->Print("Creating comb for high-level geometry\n");

    const std::string comb_name =
	unique_name(m_name_count_map, geom_obj.m_name, ".c");
    const std::string comb_uuid = UUIDstr(generate_uuid());
    const std::pair<std::string, std::string> shader
	= get_shader(geom_attrs.m_material_index);

    wmember members;
    BU_LIST_INIT(&members.l);
    mk_addmember(geom_obj.m_name.c_str(), &members.l, NULL, WMOP_UNION);

    const bool do_inherit = false;
    mk_comb(m_db, comb_name.c_str(), &members.l, false,
	    shader.first.c_str(), shader.second.c_str(),
	    get_color(geom_attrs).get_rgb(),
	    0, 0, 0, 0, do_inherit, false, false);

    m_obj_map[comb_uuid].m_name = comb_name;
    m_obj_map.at(parent_layer_uuid).m_children.push_back(comb_uuid);
}


void
RhinoConverter::create_brep(const ON_Brep &brep,
			    const ON_3dmObjectAttributes &brep_attrs)
{
    const std::string brep_uuid = UUIDstr(brep_attrs.m_uuid);
    const std::string &brep_name = m_obj_map.at(brep_uuid).m_name;

    if (m_verbose_mode)
	m_log->Print("Creating BREP '%s'\n", brep_name.c_str());

    mk_brep(m_db, brep_name.c_str(), const_cast<ON_Brep *>(&brep));
    create_geom_comb(brep_attrs);
}


void
RhinoConverter::create_mesh(ON_Mesh mesh,
			    const ON_3dmObjectAttributes &mesh_attrs)
{
    const std::string mesh_uuid = UUIDstr(mesh_attrs.m_uuid);
    const std::string &mesh_name = m_obj_map.at(mesh_uuid).m_name;

    if (m_verbose_mode)
	m_log->Print("Creating Mesh '%s'\n", mesh_name.c_str());

    mesh.ConvertQuadsToTriangles();
    mesh.CombineIdenticalVertices();
    mesh.Compact();
    mesh.UnitizeFaceNormals();

    const std::size_t num_vertices = static_cast<std::size_t>(mesh.m_V.Count());
    const std::size_t num_faces = static_cast<std::size_t>(mesh.m_F.Count());
    unsigned char mode = mesh.IsSolid() ? RT_BOT_SOLID : RT_BOT_PLATE;

    unsigned char orientation;
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

    if (num_vertices == 0 || num_faces == 0) {
	m_log->Print("-- Mesh has no content; skipping...\n");
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

    std::vector<fastf_t> thicknesses;


    AutoDestroyer<bu_bitv, void, bu_bitv_free> mbitv;

    if (mode == RT_BOT_PLATE) {
	thicknesses.assign(num_faces, 2);
	mbitv.ptr = bu_bitv_new(num_faces);
    }

    if (mesh.m_FN.Count() != mesh.m_F.Count()) {
	int r = mk_bot(m_db, mesh_name.c_str(), mode, orientation, 0,
		       num_vertices, num_faces, &vertices[0], &faces[0],
		       thicknesses.empty() ? NULL : &thicknesses[0],
		       mbitv.ptr);

	if (r) throw std::runtime_error("mk_bot() failed");
    } else {
	std::vector<fastf_t> normals(num_faces * 3);
	for (std::size_t i = 0; i < num_faces; ++i) {
	    const ON_3fVector &vec = mesh.m_FN[static_cast<int>(i)];
	    normals[i * 3] = vec.x;
	    normals[i * 3 + 1] = vec.y;
	    normals[i * 3 + 2] = vec.z;
	}

	std::vector<int> face_normals(num_faces * 3);
	for (std::size_t i = 0; i < num_faces; ++i) {
	    face_normals[i * 3] = i;
	    face_normals[i * 3 + 1] = i;
	    face_normals[i * 3 + 2] = i;
	}

	int r = mk_bot_w_normals(m_db, mesh_name.c_str(), mode, orientation,
				 RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS,
				 num_vertices, num_faces, &vertices[0], &faces[0],
				 thicknesses.empty() ? NULL : &thicknesses[0],
				 mbitv.ptr, num_faces, &normals[0], &face_normals[0]);

	if (r) throw std::runtime_error("mk_bot_w_normals() failed");
    }

    create_geom_comb(mesh_attrs);
}


void
RhinoConverter::create_all_geometry()
{
    m_log->Print("Creating geometry...\n");

    for (int i = 0; i < m_model->m_object_table.Count(); ++i) {
	const ON_3dmObjectAttributes &geom_attrs =
	    m_model->m_object_table[i].m_attributes;
	const std::string geom_uuid = UUIDstr(geom_attrs.m_uuid);
	const std::string &geom_name = m_obj_map.at(geom_uuid).m_name;

	if (m_verbose_mode)
	    m_log->Print("Object %d of %d...\n", i + 1,
			 m_model->m_object_table.Count());

	const ON_Geometry *geom =
	    ON_Geometry::Cast(m_model->m_object_table[i].m_object);

	if (geom)
	    create_geometry(geom, geom_attrs);
	else
	    m_log->Print("WARNING: Skipping non-Geometry entity '%s'\n", geom_name.c_str());
    }
}


void
RhinoConverter::create_geometry(const ON_Geometry *geom,
				const ON_3dmObjectAttributes &geom_attrs)
{
    if (const ON_Brep *brep = ON_Brep::Cast(geom)) {
	create_brep(*brep, geom_attrs);
    } else if (geom->HasBrepForm()) {
	ON_Brep *new_brep = geom->BrepForm();
	create_brep(*new_brep, geom_attrs);
	delete new_brep;
    } else if (const ON_Curve *curve = ON_Curve::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_Curve\n");
	if (m_verbose_mode) curve->Dump(*m_log);
    } else if (const ON_Surface *surface = ON_Surface::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_Surface\n");
	if (m_verbose_mode) surface->Dump(*m_log);
    } else if (const ON_Mesh *mesh = ON_Mesh::Cast(geom)) {

	create_mesh(*mesh, geom_attrs);

    } else if (const ON_RevSurface *revsurf = ON_RevSurface::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_RevSurface\n");
	if (m_verbose_mode) revsurf->Dump(*m_log);
    } else if (const ON_PlaneSurface *planesurf = ON_PlaneSurface::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_PlaneSurface\n");
	if (m_verbose_mode) planesurf->Dump(*m_log);
    } else if (const ON_InstanceDefinition *instdef = ON_InstanceDefinition::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_InstanceDefinition\n");
	if (m_verbose_mode) instdef->Dump(*m_log);
    } else if (const ON_InstanceRef *instref = ON_InstanceRef::Cast(geom)) {

	create_iref(*instref, geom_attrs);

    } else if (const ON_Layer *layer = ON_Layer::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_Layer\n");
	if (m_verbose_mode) layer->Dump(*m_log);
    } else if (const ON_Light *light = ON_Light::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_Light\n");
	if (m_verbose_mode) light->Dump(*m_log);
    } else if (const ON_NurbsCage *nurbscage = ON_NurbsCage::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_NurbsCage\n");
	if (m_verbose_mode) nurbscage->Dump(*m_log);
    } else if (const ON_MorphControl *morphctrl = ON_MorphControl::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_MorphControl\n");
	if (m_verbose_mode) morphctrl->Dump(*m_log);
    } else if (const ON_Group *group = ON_Group::Cast(geom)) {
	m_log->Print("-- Skipping: Type: ON_Group\n");
	if (m_verbose_mode) group->Dump(*m_log);
    } else m_log->Print("-- Skipping unknown object type\n");

    if (m_verbose_mode) {
	m_log->PopIndent();
	m_log->Print("\n");
    }
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
