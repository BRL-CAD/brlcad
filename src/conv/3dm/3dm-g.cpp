/*                           3 D M - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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

#include "vmath.h"		/* BRL-CAD Vector macros */
#include "wdb.h"


/* generic entity name */
#define GENERIC_NAME "rhino"
#define Usage "Usage: 3dm-g [-v vmode] [-r] [-u] -o output_file.g input_file.3dm\n"

/* UUID buffers must be >= 37 chars per openNURBS API */
#define UUID_LEN 50

/* typedefs and global containers for building layer hierarchy */
typedef std::map< std::string, std::string> STR_STR_MAP;
typedef std::map< std::string, int> REGION_CNT_MAP;
typedef std::vector<std::string> MEMBER_VEC;
typedef std::map< std::string, MEMBER_VEC *> MEMBER_MAP;

STR_STR_MAP layer_uuid_name_map;
STR_STR_MAP layer_name_uuid_map;
REGION_CNT_MAP region_cnt_map;
MEMBER_MAP member_map;


static size_t
RegionCnt(std::string &name)
{
    REGION_CNT_MAP::iterator iter = region_cnt_map.find(name);

    if (iter == region_cnt_map.end()) {
	region_cnt_map.insert(std::pair<std::string, int>(name, 1));
	return 1;
    } else {
	size_t cnt = iter->second + 1;
	region_cnt_map.erase(iter);
	region_cnt_map.insert(std::pair<std::string, int>(name, cnt));
	return cnt;
    }
}


static void
MapRegion(ONX_Model &model, std::string &region_name, int layer_index)
{
    char uuidstr[UUID_LEN] = {0};
    std::string parent_uuid;

    const ON_Layer& layer = model.m_layer_table[layer_index];

    parent_uuid = ON_UuidToString(layer.m_layer_id, uuidstr);

    MEMBER_MAP::iterator miter = member_map.find(parent_uuid);
    if (miter != member_map.end()) {
	MEMBER_VEC *vec = (MEMBER_VEC *)miter->second;
	vec->push_back(region_name);
    }
}


static void
MapLayer(std::string &layer_name, std::string &uuid, std::string &parent_uuid)
{
    layer_uuid_name_map.insert(std::pair<std::string, std::string>(uuid, layer_name));
    layer_name_uuid_map.insert(std::pair<std::string, std::string>(layer_name, uuid));
    MEMBER_MAP::iterator iter = member_map.find(uuid);
    if (iter == member_map.end()) {
	MEMBER_VEC *vec = new MEMBER_VEC;
	member_map.insert(std::pair<std::string, MEMBER_VEC *>(uuid, vec));
    }

    iter = member_map.find(parent_uuid);
    if (iter == member_map.end()) {
	MEMBER_VEC *vec = new MEMBER_VEC;
	vec->push_back(layer_name);
	member_map.insert(std::pair<std::string, MEMBER_VEC *>(parent_uuid, vec));
    } else {
	MEMBER_VEC *vec = (MEMBER_VEC *)iter->second;
	vec->push_back(layer_name);
    }
}


static void
BuildHierarchy(struct rt_wdb* outfp, std::string &uuid, ON_TextLog* dump)
{
    static long groupcnt = 1;
    struct wmember members;
    BU_LIST_INIT(&members.l);

    STR_STR_MAP::iterator siter;
    std::string groupname = "";

    if (uuid.compare("00000000-0000-0000-0000-000000000000") == 0) {
	groupname = "all";
    } else {
	siter = layer_uuid_name_map.find(uuid);
	if (siter != layer_uuid_name_map.end()) {
	    groupname = siter->second;
	}
    }
    if (groupname.empty()) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	bu_vls_printf(&str, "g%ld", groupcnt);
        groupname = bu_vls_addr(&str);
	bu_vls_free(&str);
    }

    MEMBER_MAP::iterator iter = member_map.find(uuid);
    if (iter != member_map.end()) {
	MEMBER_VEC *vec = (MEMBER_VEC *)iter->second;
	MEMBER_VEC::iterator viter = vec->begin();
	while (viter != vec->end()) {
	    std::string membername = *viter;
	    (void)mk_addmember(membername.c_str(), &members.l, NULL, WMOP_UNION);

	    siter = layer_name_uuid_map.find(membername);
	    if (siter != layer_name_uuid_map.end()) {
		std::string uuid2 = siter->second;
		BuildHierarchy(outfp, uuid2, dump);
	    }
	    ++viter;
	}
	++iter;
    }
    mk_lcomb(outfp, groupname.c_str(), &members, 0, NULL, NULL, NULL, 0);
}


static void
BuildHierarchy(struct rt_wdb* outfp, ON_TextLog* dump)
{
    std::string root_uuid = "00000000-0000-0000-0000-000000000000";
    MEMBER_MAP::iterator iter = member_map.find(root_uuid);

    if (iter != member_map.end()) {
	std::string uuid = iter->first;
	BuildHierarchy(outfp, uuid, dump);
    }
}


static void
ProcessLayers(ONX_Model &model, ON_TextLog* dump)
{
    struct bu_vls name = BU_VLS_INIT_ZERO;
    char uuidstr[UUID_LEN] = {0};
    std::string layer_name, uuid, parent_uuid;
    ON_UuidIndex uuidIndex;
    int i, count = model.m_layer_table.Count();

    dump->Print("Number of layers: %d\n", count);
    for (i=0; i < count; ++i) {
	const ON_Layer& layer = model.m_layer_table[i];
	ON_wString lname = layer.LayerName();

	bu_vls_strcpy(&name, ON_String(lname));
	layer_name = bu_vls_addr(&name);
	uuid = ON_UuidToString(layer.m_layer_id, uuidstr);
	parent_uuid = ON_UuidToString(layer.m_parent_layer_id, uuidstr);
	MapLayer(layer_name, uuid, parent_uuid);
    }
}


// Removes all leading and trailing non alpha-numeric characters,
// then replaces all remaining non alpha-numeric characters with
// the '_' character. The allow string is an exception list where
// these characters are allowed, but not leading or trailing, in
// the name.
bool
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


bool
NameIsUnique(ON_wString &name, ONX_Model &model)
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
void
MakeCleanUniqueNames(ONX_Model &model)
{
    int cnt;
    struct bu_vls num_str = BU_VLS_INIT_ZERO;
    size_t obj_counter = 0;
    bool changed = false;

    cnt = model.m_object_table.Count();
    for (int i = 0; i < cnt; i++) {
	changed = CleanName(model.m_object_table[i].m_attributes.m_name);
	ON_wString name(model.m_object_table[i].m_attributes.m_name);
	ON_wString base(name);
	if (name.Length() == 0 || !NameIsUnique(name, model)) {
	    if (name.Length() == 0) {
		base = "noname";
	    }
	    obj_counter++;
	    bu_vls_printf(&num_str, "%zx", obj_counter);
	    name = base + "." + ON_wString(bu_vls_addr(&num_str));
	    bu_vls_trunc(&num_str, 0);
	    changed = true;
	}
	if (changed) {
	    model.m_object_table[i].m_attributes.m_name.Destroy();
	    model.m_object_table[i].m_attributes.m_name = name;
	}
    }
    bu_vls_free(&num_str);
}


int
main(int argc, char** argv)
{
    size_t mcount = 0;
    int verbose_mode = 0;
    int random_colors = 0;
    int use_uuidnames = 0;
    int clean_names = 0;
    struct rt_wdb* outfp;
    ON_TextLog error_log;
    const char* id_name = "3dm -> g conversion";
    char* outFileName = NULL;
    const char* inputFileName;
    ONX_Model model;

    ON::Begin();
    ON_TextLog dump_to_stdout;
    ON_TextLog* dump = &dump_to_stdout;

    int c;
    while ((c = bu_getopt(argc, argv, "o:dv:t:s:ruhc?")) != -1) {
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
		random_colors = 1;
		break;
	    case 'u':
		use_uuidnames = 1;
		break;
	    case 'c':  /* make names unique and brlcad compliant */
		clean_names = 1;
		break;
	    default:
		dump->Print(Usage);
		return 1;
	}
    }
    if (use_uuidnames) {
	clean_names = 0;
    }

    argc -= bu_optind;
    argv += bu_optind;
    inputFileName  = argv[0];
    if (outFileName == NULL) {
	dump->Print(Usage);
	return 1;
	// strip file suffix and add .g
    }

    dump->Print("\n");
    dump->Print(" Input file: %s\n", inputFileName);
    dump->Print("Output file: %s\n", outFileName);

    // read the contents of the file into "model"
    bool rc = model.Read(inputFileName, dump); //archive, dump);

    outfp = wdb_fopen(outFileName);
    mk_id(outfp, id_name);

    // print diagnostic
    if (rc)
	dump->Print("Input 3dm file successfully read.\n");
    else
	dump->Print("Errors during reading 3dm file.\n");

    if (clean_names) {
	dump->Print("\nMaking names in 3DM model table \"m_object_table\" BRL-CAD compliant ...\n");
	MakeCleanUniqueNames(model);
	dump->Print("Name changes done.\n\n");
    }

    // see if everything is in good shape, be quiet first time around
    if (model.IsValid(dump)) {
	dump->Print("Model is VALID\n");
    } else {
	int warn_i;
	int repair_count = 0;
	ON_SimpleArray<int> warnings;

	dump->Print("Model is NOT valid.  Attempting repairs.\n");

	model.Polish(); // fill in defaults
	model.Audit(true, &repair_count, dump, &warnings); // repair

	dump->Print("%d objects were repaired.\n", repair_count);
	for (warn_i=0; warn_i < warnings.Count(); ++warn_i) {
	    dump->Print("%s\n", warnings[warn_i]);
	}

	if (model.IsValid(dump))
	    dump->Print("Repair successful, model is now valid.\n");
	else
	    dump->Print("Repair unsuccessful, model is still NOT valid.\n");
    }

    dump->Print("Number of NURBS objects read: %d\n", model.m_object_table.Count());

    /* process layer table before building regions */
    ProcessLayers(model, dump);

    struct wmember all_regions;
    BU_LIST_INIT(&all_regions.l);

    dump->Print("\n");
    for (int i = 0; i < model.m_object_table.Count(); ++i) {

	dump->Print("Object %d of %d:\n\n", i + 1, model.m_object_table.Count());

	dump->PushIndent();

	// object's attributes
	ON_3dmObjectAttributes myAttributes = model.m_object_table[i].m_attributes;

	std::string geom_base;
	myAttributes.Dump(*dump); // On debug print
	dump->Print("\n");

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
		    dump->Print("\n\nlayername:\"%s\"\n\n", bu_vls_addr(&name));
		} else {
		    genName = GENERIC_NAME;
		}

		/* For layer named regions use layer region count
		 * instead of global region count
		 */
		if (genName.compare(GENERIC_NAME) == 0) {
		    bu_vls_printf(&name, "%zd", mcount++);
		    genName += bu_vls_addr(&name);
		    geom_base = genName.c_str();
		} else {
		    size_t region_cnt = RegionCnt(genName);
		    bu_vls_printf(&name, "%zd", region_cnt);
		    genName += bu_vls_addr(&name);
		    geom_base = genName.c_str();
		}

		dump->Print("Object has no name - creating one %s.\n", geom_base.c_str());
		bu_vls_free(&name);
	    } else {
		const char* cstr = constr;
		geom_base = cstr;
	    }
	}

	std::string geom_name(geom_base+".s");
	std::string region_name(geom_base+".r");

	/* add region to hierarchical containers */
	MapRegion(model, region_name, myAttributes.m_layer_index);

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
	    ON_Curve *curve;
	    ON_Surface *surface;
	    ON_Mesh *mesh;
	    ON_RevSurface *revsurf;
	    ON_PlaneSurface *planesurf;
	    ON_InstanceDefinition *instdef;
	    ON_InstanceRef *instref;
	    ON_Layer *layer;
	    ON_Light *light;
	    ON_NurbsCage *nurbscage;
	    ON_MorphControl *morphctrl;
	    ON_Group *group;
	    ON_Geometry *geom;
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

	    dump->Print("Color: %d, %d, %d\n", r, g, b);

	    if ((brep = const_cast<ON_Brep * >(ON_Brep::Cast(pGeometry)))) {

		dump->Print("primitive is %s.\n", geom_name.c_str());
		dump->Print("region created is %s.\n", region_name.c_str());

		mk_brep(outfp, geom_name.c_str(), brep);

		unsigned char rgb[3];
		rgb[RED] = (unsigned char)r;
		rgb[GRN] = (unsigned char)g;
		rgb[BLU] = (unsigned char)b;
		mk_region1(outfp, region_name.c_str(), geom_name.c_str(), "plastic", "", rgb);

		(void)mk_addmember(region_name.c_str(), &all_regions.l, NULL, WMOP_UNION);
		if (verbose_mode > 0)
		    brep->Dump(*dump);
	    } else if (pGeometry->HasBrepForm()) {
		dump->Print("Type: HasBrepForm\n");

		ON_Brep *new_brep = pGeometry->BrepForm();

		dump->Print("primitive is %s.\n", geom_name.c_str());
		dump->Print("region created is %s.\n", region_name.c_str());

		mk_brep(outfp, geom_name.c_str(), new_brep);

		unsigned char rgb[3];
		rgb[RED] = (unsigned char)r;
		rgb[GRN] = (unsigned char)g;
		rgb[BLU] = (unsigned char)b;
		mk_region1(outfp, region_name.c_str(), geom_name.c_str(), "plastic", "", rgb);

		(void)mk_addmember(region_name.c_str(), &all_regions.l, NULL, WMOP_UNION);
		if (verbose_mode > 0)
		    new_brep->Dump(*dump);

		delete new_brep;

	    } else if ((curve = const_cast<ON_Curve * >(ON_Curve::Cast(pGeometry)))) {
		dump->Print("Type: ON_Curve\n");
		if (verbose_mode > 1) curve->Dump(*dump);
	    } else if ((surface = const_cast<ON_Surface * >(ON_Surface::Cast(pGeometry)))) {
		dump->Print("Type: ON_Surface\n");
		if (verbose_mode > 2) surface->Dump(*dump);
	    } else if ((mesh = const_cast<ON_Mesh * >(ON_Mesh::Cast(pGeometry)))) {
		dump->Print("Type: ON_Mesh\n");
		if (verbose_mode > 4) mesh->Dump(*dump);
	    } else if ((revsurf = const_cast<ON_RevSurface * >(ON_RevSurface::Cast(pGeometry)))) {
		dump->Print("Type: ON_RevSurface\n");
		if (verbose_mode > 2) revsurf->Dump(*dump);
	    } else if ((planesurf = const_cast<ON_PlaneSurface * >(ON_PlaneSurface::Cast(pGeometry)))) {
		dump->Print("Type: ON_PlaneSurface\n");
		if (verbose_mode > 2) planesurf->Dump(*dump);
	    } else if ((instdef = const_cast<ON_InstanceDefinition * >(ON_InstanceDefinition::Cast(pGeometry)))) {
		dump->Print("Type: ON_InstanceDefinition\n");
		if (verbose_mode > 3) instdef->Dump(*dump);
	    } else if ((instref = const_cast<ON_InstanceRef * >(ON_InstanceRef::Cast(pGeometry)))) {
		dump->Print("Type: ON_InstanceRef\n");
		if (verbose_mode > 3) instref->Dump(*dump);
	    } else if ((layer = const_cast<ON_Layer * >(ON_Layer::Cast(pGeometry)))) {
		dump->Print("Type: ON_Layer\n");
		if (verbose_mode > 3) layer->Dump(*dump);
	    } else if ((light = const_cast<ON_Light * >(ON_Light::Cast(pGeometry)))) {
		dump->Print("Type: ON_Light\n");
		if (verbose_mode > 3) light->Dump(*dump);
	    } else if ((nurbscage = const_cast<ON_NurbsCage * >(ON_NurbsCage::Cast(pGeometry)))) {
		dump->Print("Type: ON_NurbsCage\n");
		if (verbose_mode > 3) nurbscage->Dump(*dump);
	    } else if ((morphctrl = const_cast<ON_MorphControl * >(ON_MorphControl::Cast(pGeometry)))) {
		dump->Print("Type: ON_MorphControl\n");
		if (verbose_mode > 3) morphctrl->Dump(*dump);
	    } else if ((group = const_cast<ON_Group * >(ON_Group::Cast(pGeometry)))) {
		dump->Print("Type: ON_Group\n");
		if (verbose_mode > 3) group->Dump(*dump);
	    } else if ((geom = const_cast<ON_Geometry * >(ON_Geometry::Cast(pGeometry)))) {
		dump->Print("Type: ON_Geometry\n");
		if (verbose_mode > 3) geom->Dump(*dump);
	    } else {
		dump->Print("WARNING: Encountered an unexpected kind of object.  Please report to devs@brlcad.org\n");
	    }
	} else {
	    dump->Print("WARNING: Skipping non-Geometry entity: %s\n", geom_base.c_str());
	}
	dump->PopIndent();
	dump->Print("\n\n");
    }

    /* use accumulated layer information to build mged hierarchy */
    char *toplevel = bu_basename(outFileName);
    BuildHierarchy(outfp, dump);
    mk_lcomb(outfp, toplevel, &all_regions, 0, NULL, NULL, NULL, 0);
    bu_free(toplevel, "bu_basename toplevel");
    wdb_close(outfp);

    /* let them know */
    dump->Print("Done.\n");

    model.Destroy();
    ON::End();

    return 0;
}


#else /* !OBJ_BREP */

int
main(int argc, char *argv[])
{
    printf("ERROR: Boundary Representation object support is not available with\n"
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
