/*                           3 D M - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
 * Program to convert a Rhino model (in a .3dm file) to a BRL-CAD .g
 * file.
 *
 */

#include "common.h"

/* without OBJ_BREP, this entire procedural example is disabled */
#ifdef OBJ_BREP

#include <string>
#include <vector>
#include <map>
#include <sstream>

#include "bu/getopt.h"
#include "vmath.h"		/* BRL-CAD Vector macros */
#include "wdb.h"


namespace
{


static const char * const USAGE = "USAGE: 3dm-g [-v vmode] [-r] [-u] -o output_file.g input_file.3dm\n";

/* generic entity name */
static const char * const GENERIC_NAME = "rhino";
static const char * const ROOT_UUID = "00000000-0000-0000-0000-000000000000";

/* UUID buffers must be >= 37 chars per openNURBS API */
static const std::size_t UUID_LEN = 37;



/* typedefs and global containers for building layer hierarchy */
typedef std::map<std::string, std::string> STR_STR_MAP;
typedef std::vector<std::string> MEMBER_VEC;
typedef std::map<std::string, MEMBER_VEC> MEMBER_MAP;


struct LayerMaps {
    STR_STR_MAP layer_uuid_name_map, layer_name_uuid_map;
    MEMBER_MAP member_map;
};


static inline std::string
UUIDstr(const ON_UUID &uuid)
{
    char buf[UUID_LEN];
    return ON_UuidToString(uuid, buf);
}


static void xform2mat_t(const ON_Xform &source, mat_t dest)
{
    const std::size_t dmax = 4;
    for (std::size_t row = 0; row < dmax; ++row)
	for (std::size_t col = 0; col < dmax; ++col)
	    dest[row*dmax + col] = source[row][col];
}


static void
create_instance_reference(rt_wdb *outfp, STR_STR_MAP &uuid_map,
	const ON_InstanceRef &iref, const std::string &comb_name)
{
    mat_t matrix;
    xform2mat_t(iref.m_xform, matrix);
    wmember members;
    BU_LIST_INIT(&members.l);
    const std::string member_name = uuid_map[UUIDstr(iref.m_instance_definition_uuid)] + ".c";
    mk_addmember(member_name.c_str(), &members.l, matrix, WMOP_UNION);
    mk_lfcomb(outfp, comb_name.c_str(), &members, 0); // FIXME create region instead
}


static void
create_instance_definition(rt_wdb *outfp, const ONX_Model &model,
	ON_TextLog &dump, STR_STR_MAP &uuid_map, const ON_InstanceDefinition &idef,
	const std::string &comb_name)
{
    wmember members;
    BU_LIST_INIT(&members.l);

    for (int i = 0; i < idef.m_object_uuid.Count(); ++i) {
	const ON_UUID member_uuid = idef.m_object_uuid[i];
	const int geom_index = model.ObjectIndex(member_uuid);
	if (geom_index == -1) {
	    dump.Print("referenced uuid=%s does not exist\n", UUIDstr(member_uuid).c_str());
	    continue;
	}

	const ON_Geometry *pGeometry = ON_Geometry::Cast(model.m_object_table[geom_index].m_object);
	if (!pGeometry) {
	    dump.Print("referenced uuid=%s is not geometry\n", UUIDstr(member_uuid).c_str());
	    continue;
	}

	std::string member_name;
	if (ON_InstanceRef::Cast(pGeometry))
	    member_name = uuid_map[UUIDstr(member_uuid)] + ".r";
	else
	    member_name = uuid_map[UUIDstr(member_uuid)] + ".s";

	mk_addmember(member_name.c_str(), &members.l, NULL, WMOP_UNION);
    }

    mk_lfcomb(outfp, comb_name.c_str(), &members, 0);
}


static void
MapRegion(const ONX_Model &model, const std::string &region_name, int layer_index, MEMBER_MAP &member_map)
{
    const ON_Layer& layer = model.m_layer_table[layer_index];
    std::string parent_uuid = UUIDstr(layer.m_layer_id);

    MEMBER_MAP::iterator miter = member_map.find(parent_uuid);
    if (miter != member_map.end())
	miter->second.push_back(region_name);
}


static void
MapLayer(const std::string &layer_name, const std::string &uuid, const std::string &parent_uuid, LayerMaps &lmaps)
{
    lmaps.layer_uuid_name_map.insert(std::pair<std::string, std::string>(uuid, layer_name));
    lmaps.layer_name_uuid_map.insert(std::pair<std::string, std::string>(layer_name, uuid));

    lmaps.member_map[uuid]; // create if it doesn't exist

    lmaps.member_map[parent_uuid].push_back(layer_name);
}


static void
BuildHierarchy(struct rt_wdb* outfp, const std::string &uuid, ON_TextLog &dump, const LayerMaps &lmaps,
	long &groupcnt)
{
    struct wmember members;
    BU_LIST_INIT(&members.l);

    STR_STR_MAP::const_iterator siter;
    std::string groupname = "";

    if (uuid.compare(ROOT_UUID) == 0) {
	groupname = "all";
    } else {
	siter = lmaps.layer_uuid_name_map.find(uuid);
	if (siter != lmaps.layer_uuid_name_map.end()) {
	    groupname = siter->second;
	}
    }

    if (groupname.empty()) {
	std::ostringstream converter;
	converter << groupcnt;
	groupname = "g" + converter.str();
    }

    MEMBER_MAP::const_iterator iter = lmaps.member_map.find(uuid);
    if (iter != lmaps.member_map.end()) {
	const MEMBER_VEC &vec = iter->second;
	MEMBER_VEC::const_iterator viter = vec.begin();
	while (viter != vec.end()) {
	    std::string membername = *viter;
	    (void)mk_addmember(membername.c_str(), &members.l, NULL, WMOP_UNION);

	    siter = lmaps.layer_name_uuid_map.find(membername);
	    if (siter != lmaps.layer_name_uuid_map.end()) {
		std::string uuid2 = siter->second;
		BuildHierarchy(outfp, uuid2, dump, lmaps, groupcnt);
	    }
	    ++viter;
	}
	++iter;
    }
    mk_lcomb(outfp, groupname.c_str(), &members, 0, NULL, NULL, NULL, 0);
}


static void
BuildHierarchy(struct rt_wdb* outfp, ON_TextLog &dump, const LayerMaps &lmaps)
{
    MEMBER_MAP::const_iterator iter = lmaps.member_map.find(ROOT_UUID);

    if (iter != lmaps.member_map.end()) {
	std::string uuid = iter->first;
	long groupcnt = 1;
	BuildHierarchy(outfp, uuid, dump, lmaps, groupcnt);
    }
}


static void
ProcessLayers(const ONX_Model &model, ON_TextLog &dump, LayerMaps &lmaps)
{
    ON_UuidIndex uuidIndex;
    int count = model.m_layer_table.Count();

    dump.Print("Number of layers: %d\n", count);
    for (int i = 0; i < count; ++i) {
	const ON_Layer& layer = model.m_layer_table[i];
	std::string layer_name = ON_String(layer.LayerName()).Array();
	std::string uuid = UUIDstr(layer.m_layer_id);
	std::string parent_uuid = UUIDstr(layer.m_parent_layer_id);
	MapLayer(layer_name, uuid, parent_uuid, lmaps);
    }
}


// Removes all leading and trailing non alpha-numeric characters,
// then replaces all remaining non alpha-numeric characters with
// the '_' character. The allow string is an exception list where
// these characters are allowed, but not leading or trailing, in
// the name.
static bool
CleanName(ON_wString &name)
{
    ON_wString allow(".-_");
    ON_wString new_name;
    bool was_cleaned = false;

    bool found_first = false;
    int idx_first = 0, idx_last = 0;

    for (int j = 0; j < name.Length(); j++) {
	wchar_t c = name.GetAt(j);

	bool ok_char = false;
	if (isalnum(c)) {
	    if (!found_first) {
		idx_first = idx_last = j;
		found_first = true;
	    } else {
		idx_last = j;
	    }
	    ok_char = true;
	} else {
	    for (int k = 0 ; k < allow.Length() ; k++) {
		if (c == allow.GetAt(k)) {
		    ok_char = true;
		    break;
		}
	    }
	}
	if (!ok_char) {
	    c = L'_';
	    was_cleaned = true;
	}
	new_name += c;
    }
    if (idx_first != 0 || name.Length() != ((idx_last - idx_first) + 1)) {
	new_name = new_name.Mid(idx_first, (idx_last - idx_first) + 1);
	was_cleaned = true;
    }
    if (was_cleaned) {
	name.Destroy();
	name = new_name;
    }

    return was_cleaned;
}


static bool
NameIsUnique(const ON_wString &name, const ONX_Model &model)
{
    bool found_once = false;
    for (int i = 0; i < model.m_object_table.Count(); i++) {
	if (name == model.m_object_table[i].m_attributes.m_name) {
	    if (found_once) {
		return false;
	    } else {
		found_once = true;
	    }
	}
    }
    return true;
}


// Cleans names in 3dm model and makes them unique.
static void
MakeCleanUniqueNames(ONX_Model &model)
{
    size_t obj_counter = 0;
    int cnt = model.m_object_table.Count();
    for (int i = 0; i < cnt; i++) {
	bool changed = CleanName(model.m_object_table[i].m_attributes.m_name);
	ON_wString name(model.m_object_table[i].m_attributes.m_name);
	ON_wString base(name);
	if (name.Length() == 0 || !NameIsUnique(name, model)) {
	    if (name.Length() == 0) {
		base = "noname";
	    }

	    std::ostringstream converter;
	    converter << ++obj_counter;
	    name = base + "." + converter.str().c_str();
	    changed = true;
	}
	if (changed) {
	    model.m_object_table[i].m_attributes.m_name.Destroy();
	    model.m_object_table[i].m_attributes.m_name = name;
	}
    }


    // FIXME code duplication
    for (int i = 0; i < model.m_idef_table.Count(); ++i) {
	bool changed = CleanName(model.m_idef_table[i].m_name);
	ON_wString name(model.m_idef_table[i].m_name);
	ON_wString base(name);
	if (name.Length() == 0 || !NameIsUnique(name, model)) {
	    if (name.Length() == 0) {
		base = "noname";
	    }

	    std::ostringstream converter;
	    converter << ++obj_counter;
	    name = base + "." + converter.str().c_str();
	    changed = true;
	}
	if (changed) {
	    model.m_idef_table[i].m_name.Destroy();
	    model.m_idef_table[i].m_name = name;
	}
    }
}


}


std::string gen_geom_name(const ONX_Model &model, ON_TextLog &dump, const ON_3dmObjectAttributes &myAttributes, int verbose_mode, bool use_uuidnames, std::size_t &mcount, std::map<std::string, std::size_t> &region_cnt_map)
{
    std::string geom_base;
    if (use_uuidnames) {
	char uuidstring[UUID_LEN] = {0};
	ON_UuidToString(myAttributes.m_uuid, uuidstring);
	ON_String constr(uuidstring);
	const char* cstr = constr;
	geom_base = cstr;
    } else {
	ON_String constr(myAttributes.m_name);
	if (constr == NULL) {
	    std::string genName = "";
	    struct bu_vls name = BU_VLS_INIT_ZERO;

	    /* Use layer name to help name un-named regions/objects */
	    if (myAttributes.m_layer_index > 0) {
		const ON_Layer& layer = model.m_layer_table[myAttributes.m_layer_index];
		ON_wString layer_name = layer.LayerName();
		bu_vls_strcpy(&name, ON_String(layer_name));
		genName = bu_vls_addr(&name);
		if (genName.length() <= 0) {
		    genName = GENERIC_NAME;
		}
		if (verbose_mode) {
		    dump.Print("\n\nlayername:\"%s\"\n\n", bu_vls_addr(&name));
		}
	    } else {
		genName = GENERIC_NAME;
	    }

	    /* For layer named regions use layer region count
	     * instead of global region count
	     */
	    if (genName.compare(GENERIC_NAME) == 0) {
		bu_vls_printf(&name, "%lu", (long unsigned int)mcount++);
		genName += bu_vls_addr(&name);
		geom_base = genName;
	    } else {
		size_t region_cnt = ++region_cnt_map[genName];
		bu_vls_printf(&name, "%lu", (long unsigned int)region_cnt);
		genName += bu_vls_addr(&name);
		geom_base = genName;
	    }

	    if (verbose_mode) {
		dump.Print("Object has no name - creating one %s.\n", geom_base.c_str());
	    }
	    bu_vls_free(&name);
	} else {
	    const char* cstr = constr;
	    geom_base = cstr;
	}
    }

    return geom_base;
}


STR_STR_MAP create_geometry_names(rt_wdb *outfp, const ONX_Model &model, ON_TextLog &dump, int verbose_mode, bool use_uuidnames)
{
    STR_STR_MAP uuid_map;
    for (int i = 0; i < model.m_object_table.Count(); ++i) {
	// object's attributes
	ON_3dmObjectAttributes myAttributes = model.m_object_table[i].m_attributes;

	std::size_t mcount = 0;
	std::map<std::string, std::size_t> region_cnt_map;
	std::string geom_base = gen_geom_name(model, dump, myAttributes, verbose_mode, use_uuidnames,
		mcount, region_cnt_map);
	uuid_map[UUIDstr(myAttributes.m_uuid)] = geom_base;
    }

    for (int i = 0; i < model.m_idef_table.Count(); ++i) {
	std::string geom_base;
	if (use_uuidnames)
	    geom_base = UUIDstr(model.m_idef_table[i].m_uuid);
	else
	    geom_base = ON_String(model.m_idef_table[i].Name()).Array();

	uuid_map[UUIDstr(model.m_idef_table[i].m_uuid)] = geom_base;
	create_instance_definition(outfp, model, dump, uuid_map, model.m_idef_table[i], geom_base+".c");
    }

    return uuid_map;
}


int
main(int argc, char** argv)
{
    int verbose_mode = 0;
    bool random_colors = false;
    bool use_uuidnames = false;
    struct rt_wdb* outfp;
    ON_TextLog error_log;
    const char* id_name = "3dm -> g conversion";
    char* outFileName = NULL;
    const char* inputFileName;
    ONX_Model model;

    ON::Begin();
    ON_TextLog dump;

    int c;
    while ((c = bu_getopt(argc, argv, "o:dv:t:s:ruh?")) != -1) {
	switch (c) {
	    case 's':	/* scale factor */
		break;
	    case 'o':	/* specify output file name */
		outFileName = bu_optarg;
		break;
	    case 'd':	/* debug */
		break;
	    case 't':	/* tolerance */
		break;
	    case 'v':	/* verbose */
		int tmpint;
		sscanf(bu_optarg, "%d", &tmpint);
		verbose_mode = tmpint;
		break;
	    case 'r':  /* randomize colors */
		random_colors = true;
		break;
	    case 'u':
		use_uuidnames = true;
		break;
	    default:
		dump.Print(USAGE);
		return 1;
	}
    }

    argc -= bu_optind;
    argv += bu_optind;
    inputFileName  = argv[0];
    if (outFileName == NULL) {
	dump.Print(USAGE);
	return 1;
	// strip file suffix and add .g
    }

    dump.Print("\n");
    dump.Print(" Input file: %s\n", inputFileName);
    dump.Print("Output file: %s\n", outFileName);

    // read the contents of the file into "model"
    bool rc = model.Read(inputFileName, &dump); //archive, dump);

    outfp = wdb_fopen(outFileName);
    mk_id(outfp, id_name);

    // print diagnostic
    if (rc)
	dump.Print("Input 3dm file successfully read.\n");
    else
	dump.Print("Errors during reading 3dm file.\n");

    if (!use_uuidnames) {
	dump.Print("\nMaking names in 3DM model table \"m_object_table\" BRL-CAD compliant ...\n");
	MakeCleanUniqueNames(model);
	dump.Print("Name changes done.\n\n");
    }

    // see if everything is in good shape, be quiet first time around
    if (model.IsValid(&dump)) {
	dump.Print("Model is VALID\n");
    } else {
	int repair_count = 0;
	ON_SimpleArray<int> warnings;

	dump.Print("Model is NOT valid.  Attempting repairs.\n");

	model.Polish(); // fill in defaults
	model.Audit(true, &repair_count, &dump, &warnings); // repair

	dump.Print("%d objects were repaired.\n", repair_count);
	for (int warn_i = 0; warn_i < warnings.Count(); ++warn_i) {
	    dump.Print("%s\n", warnings[warn_i]);
	}

	if (model.IsValid(&dump))
	    dump.Print("Repair successful, model is now valid.\n");
	else
	    dump.Print("Repair unsuccessful, model is still NOT valid.\n");
    }

    dump.Print("Number of NURBS objects read: %d\n", model.m_object_table.Count());

    /* process layer table before building regions */
    LayerMaps lmaps;
    ProcessLayers(model, dump, lmaps);

    struct wmember all_regions;
    BU_LIST_INIT(&all_regions.l);


    dump.Print("\n");
    STR_STR_MAP uuid_map = create_geometry_names(outfp, model, dump, verbose_mode, use_uuidnames);
    for (int i = 0; i < model.m_object_table.Count(); ++i) {

	dump.Print("Object %d of %d...", i + 1, model.m_object_table.Count());

	if (verbose_mode) {
	    dump.Print("\n\n");
	    dump.PushIndent();
	}

	// object's attributes
	ON_3dmObjectAttributes myAttributes = model.m_object_table[i].m_attributes;

	if (verbose_mode) {
	    myAttributes.Dump(dump); // On debug print
	    dump.Print("\n");
	}

	std::string geom_base = uuid_map[UUIDstr(myAttributes.m_uuid)];
	std::string geom_name = geom_base+".s";
	std::string region_name(geom_name+".r");

	/* add region to hierarchical containers */
	MapRegion(model, region_name, myAttributes.m_layer_index, lmaps.member_map);

	/* object definition
	   Ah - rather than pulling JUST the geometry from the opennurbs object here, need to
	   also get properties from ON_3dmObjectAttributes:
	   Everything looks like it is in there - m_name, m_layer_index, etc.
	   Will need to parse layers to get info for each object using a parent layer's settings
	   Long term, material objects and render objects should be implemented in BRL-CAD
	   to support conceptually similar breakouts of materials and shaders.
	   */

	const ON_Geometry* pGeometry = ON_Geometry::Cast(model.m_object_table[i].m_object);
	if (pGeometry) {
	    ON_Brep *brep;
	    const ON_Curve *curve;
	    const ON_Surface *surface;
	    const ON_Mesh *mesh;
	    const ON_RevSurface *revsurf;
	    const ON_PlaneSurface *planesurf;
	    const ON_InstanceDefinition *instdef;
	    const ON_InstanceRef *instref;
	    const ON_Layer *layer;
	    const ON_Light *light;
	    const ON_NurbsCage *nurbscage;
	    const ON_MorphControl *morphctrl;
	    const ON_Group *group;
	    const ON_Geometry *geom;
	    int r, g, b;

	    if (random_colors) {
		r = int(256*drand48() + 1.0);
		g = int(256*drand48() + 1.0);
		b = int(256*drand48() + 1.0);
	    } else {
		r = (model.WireframeColor(myAttributes) & 0xFF);
		g = ((model.WireframeColor(myAttributes)>>8) & 0xFF);
		b = ((model.WireframeColor(myAttributes)>>16) & 0xFF);

		// If the geometry color is black, set it to red.
		if (r == 0 && g == 0 && b == 0) {
		    r = 255;
		}
	    }

	    if (verbose_mode) {
		dump.Print("Color: %d, %d, %d\n", r, g, b);
	    }

	    if ((brep = const_cast<ON_Brep * >(ON_Brep::Cast(pGeometry)))) {

		if (verbose_mode) {
		    dump.Print("primitive is %s.\n", geom_name.c_str());
		    dump.Print("region created is %s.\n", region_name.c_str());
		}
		mk_brep(outfp, geom_name.c_str(), brep);

		unsigned char rgb[3];
		rgb[RED] = (unsigned char)r;
		rgb[GRN] = (unsigned char)g;
		rgb[BLU] = (unsigned char)b;
		mk_region1(outfp, region_name.c_str(), geom_name.c_str(), "plastic", "", rgb);

		(void)mk_addmember(region_name.c_str(), &all_regions.l, NULL, WMOP_UNION);
		if (verbose_mode > 0)
		    brep->Dump(dump);
	    } else if (pGeometry->HasBrepForm()) {
		if (verbose_mode > 0)
		    dump.Print("Type: HasBrepForm\n");

		ON_Brep *new_brep = pGeometry->BrepForm();

		if (verbose_mode) {
		    dump.Print("primitive is %s.\n", geom_name.c_str());
		    dump.Print("region created is %s.\n", region_name.c_str());
		}

		mk_brep(outfp, geom_name.c_str(), new_brep);

		unsigned char rgb[3];
		rgb[RED] = (unsigned char)r;
		rgb[GRN] = (unsigned char)g;
		rgb[BLU] = (unsigned char)b;
		mk_region1(outfp, region_name.c_str(), geom_name.c_str(), "plastic", "", rgb);

		(void)mk_addmember(region_name.c_str(), &all_regions.l, NULL, WMOP_UNION);
		if (verbose_mode > 0)
		    new_brep->Dump(dump);

		delete new_brep;

	    } else if ((curve = ON_Curve::Cast(pGeometry))) {
		if (verbose_mode > 0)
		    dump.Print("Type: ON_Curve\n");
		if (verbose_mode > 1) curve->Dump(dump);
	    } else if ((surface = ON_Surface::Cast(pGeometry))) {
		if (verbose_mode > 0)
		    dump.Print("Type: ON_Surface\n");
		if (verbose_mode > 2) surface->Dump(dump);
	    } else if ((mesh = ON_Mesh::Cast(pGeometry))) {
		dump.Print("Type: ON_Mesh\n");
		if (verbose_mode > 4) mesh->Dump(dump);
	    } else if ((revsurf = ON_RevSurface::Cast(pGeometry))) {
		dump.Print("Type: ON_RevSurface\n");
		if (verbose_mode > 2) revsurf->Dump(dump);
	    } else if ((planesurf = ON_PlaneSurface::Cast(pGeometry))) {
		dump.Print("Type: ON_PlaneSurface\n");
		if (verbose_mode > 2) planesurf->Dump(dump);
	    } else if ((instdef = ON_InstanceDefinition::Cast(pGeometry))) {
		dump.Print("Type: ON_InstanceDefinition\n");
		if (verbose_mode > 3) instdef->Dump(dump);
	    } else if ((instref = ON_InstanceRef::Cast(pGeometry))) {
		if (verbose_mode > 0)
		    dump.Print("Type: ON_InstanceRef\n");
		if (verbose_mode > 3) instref->Dump(dump);

		create_instance_reference(outfp, uuid_map, *instref, geom_base + ".r");
	    } else if ((layer = ON_Layer::Cast(pGeometry))) {
		dump.Print("Type: ON_Layer\n");
		if (verbose_mode > 3) layer->Dump(dump);
	    } else if ((light = ON_Light::Cast(pGeometry))) {
		dump.Print("Type: ON_Light\n");
		if (verbose_mode > 3) light->Dump(dump);
	    } else if ((nurbscage = ON_NurbsCage::Cast(pGeometry))) {
		dump.Print("Type: ON_NurbsCage\n");
		if (verbose_mode > 3) nurbscage->Dump(dump);
	    } else if ((morphctrl = ON_MorphControl::Cast(pGeometry))) {
		dump.Print("Type: ON_MorphControl\n");
		if (verbose_mode > 3) morphctrl->Dump(dump);
	    } else if ((group = ON_Group::Cast(pGeometry))) {
		dump.Print("Type: ON_Group\n");
		if (verbose_mode > 3) group->Dump(dump);
	    } else if ((geom = ON_Geometry::Cast(pGeometry))) {
		if (verbose_mode > 0)
		    dump.Print("Type: ON_Geometry\n");
		if (verbose_mode > 3) geom->Dump(dump);
	    } else {
		dump.Print("WARNING: Encountered an unexpected kind of object.  Please report to devs@brlcad.org\n");
	    }
	} else {
	    dump.Print("WARNING: Skipping non-Geometry entity: %s\n", geom_base.c_str());
	}
	if (verbose_mode > 0) {
	    dump.PopIndent();
	    dump.Print("\n\n");
	} else {
	    dump.Print("\n");
	}
    }

    /* use accumulated layer information to build mged hierarchy */
    char *toplevel = (char *)bu_calloc(strlen(outFileName), sizeof(char), "3dm-g toplevel");
    bu_basename(toplevel, outFileName);
    BuildHierarchy(outfp, dump, lmaps);
    mk_lcomb(outfp, toplevel, &all_regions, 0, NULL, NULL, NULL, 0);
    bu_free(toplevel, "bu_basename toplevel");
    wdb_close(outfp);

    /* let them know */
    dump.Print("Done.\n");

    model.Destroy();
    ON::End();

    return 0;
}


#else /* !OBJ_BREP */

#include <cstdio>


    int
main()
{
    std::printf("ERROR: Boundary Representation object support is not available with\n"
	    "       this compilation of BRL-CAD.\n");

    return 1;
}


#endif /* OBJ_BREP */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
