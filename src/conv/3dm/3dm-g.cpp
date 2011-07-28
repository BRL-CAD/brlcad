/*                           3 D M - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 *  Program to convert a Rhino model (in a .3dm file) to a BRL-CAD .g
 *  file.
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

/* typedefs and global containers for building layer hierarchy */
typedef std::map< std::string, std::string> STR_STR_MAP;
typedef std::map< std::string, int> REGION_CNT_MAP;
typedef std::vector<std::string> MEMBER_VEC;
typedef std::map< std::string, MEMBER_VEC *> MEMBER_MAP;

STR_STR_MAP layer_uuid_name_map;
STR_STR_MAP layer_name_uuid_map;
REGION_CNT_MAP region_cnt_map;
MEMBER_MAP member_map;

char * itoa(int num) {
    static char	line[10];

    sprintf(line, "%d", num);
    return line;
}

void printPoints(struct rt_brep_internal* bi, ON_TextLog* dump) {
    ON_Brep* brep = bi->brep;
    if (brep) {
	const int count = brep->m_V.Count();
	for (int i = 0; i < count; i++) {
	    ON_BrepVertex& bv = brep->m_V[i];
	    bv.Dump(*dump);
	}
    } else {
	dump->Print("brep was NULL!\n");
    }
}

int RegionCnt( std::string &name ) {
    REGION_CNT_MAP::iterator iter = region_cnt_map.find(name);

    if (iter == region_cnt_map.end()) {
	region_cnt_map.insert(std::pair<std::string,int>(name,1));
	return 1;
    } else {
	int cnt = iter->second + 1;
	region_cnt_map.erase(iter);
	region_cnt_map.insert(std::pair<std::string,int>(name,cnt));
	return cnt;
    }
}

void MapRegion(ONX_Model &model, std::string &region_name, int layer_index) {
    char uuidstr[50];
    std::string parent_uuid;

    const ON_Layer& layer = model.m_layer_table[layer_index];

    parent_uuid = ON_UuidToString( layer.m_layer_id, uuidstr );

    MEMBER_MAP::iterator miter = member_map.find(parent_uuid);
    if (miter != member_map.end()) {
	MEMBER_VEC *vec = (MEMBER_VEC *)miter->second;
	vec->push_back(region_name);
    }
}

void MapLayer(std::string &layer_name, std::string &uuid, std::string &parent_uuid) {
    layer_uuid_name_map.insert(std::pair<std::string,std::string>(uuid,layer_name));
    layer_name_uuid_map.insert(std::pair<std::string,std::string>(layer_name,uuid));
    MEMBER_MAP::iterator iter = member_map.find(uuid);
    if (iter == member_map.end()) {
	MEMBER_VEC *vec = new MEMBER_VEC;
	member_map.insert(std::pair<std::string,MEMBER_VEC *>(uuid,vec));
    }

    iter = member_map.find(parent_uuid);
    if (iter == member_map.end()) {
	MEMBER_VEC *vec = new MEMBER_VEC;
	vec->push_back(layer_name);
	member_map.insert(std::pair<std::string,MEMBER_VEC *>(parent_uuid,vec));
    } else {
	MEMBER_VEC *vec = (MEMBER_VEC *)iter->second;
	vec->push_back(layer_name);
    }
}

void BuildHierarchy(struct rt_wdb* outfp, std::string &uuid, ON_TextLog* dump) {
    static int groupcnt = 1;
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
	groupname = "g" + groupcnt;
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
	    viter++;
	}
	iter++;
    }
    mk_lcomb(outfp, groupname.c_str(), &members, 0, NULL, NULL, NULL, 0);
}

void BuildHierarchy(struct rt_wdb* outfp, ON_TextLog* dump) {
    std::string root_uuid = "00000000-0000-0000-0000-000000000000";
    MEMBER_MAP::iterator iter = member_map.find(root_uuid);
    if (iter != member_map.end()) {
	std::string uuid = iter->first;
	BuildHierarchy(outfp, uuid, dump);
    }
}

void ProcessLayers(ONX_Model &model, ON_TextLog* dump) {
    char name[256];
    char uuidstr[50];
    std::string layer_name,uuid,parent_uuid;
    ON_UuidIndex uuidIndex;
    int i, count = model.m_layer_table.Count();
    dump->Print("Number of LAYERS - %d\n.", count);
    for ( i=0; i < count; i++) {
	const ON_Layer& layer = model.m_layer_table[i];
	ON_wString lname = layer.LayerName();
	strncpy(name, ON_String( lname ),255);
	layer_name = name;
	uuid = ON_UuidToString( layer.m_layer_id, uuidstr );
	parent_uuid = ON_UuidToString( layer.m_parent_layer_id, uuidstr );
	MapLayer(layer_name, uuid, parent_uuid);
    }
}

int main(int argc, char** argv) {
    int mcount = 0;
    int verbose_mode = 0;
    int random_colors = 0;
    int use_uuidnames = 0;
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
    while ((c = bu_getopt(argc, argv, "o:dv:t:s:ru")) != -1) {
	switch ( c ) {
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
	    default:
		break;
	}
    }
    argc -= bu_optind;
    argv += bu_optind;
    inputFileName  = argv[0];
    if (outFileName == NULL) {
	dump->Print("\n** Error **\n Need an output file to continue. Syntax: \n");
	dump->Print(" ./3dm-g  -o <output file>.g <input file>.3dm \n** Error **\n\n");
	return 1;
	// strip file suffix and add .g
    }

    dump->Print("\n");
    dump->Print("Input file %s.\n", inputFileName);
    dump->Print("Output file %s.\n", outFileName);

    FILE* archive_fp = ON::OpenFile(inputFileName, "rb");
    if ( !archive_fp ) {
	dump->Print("  Unable to open file.\n" );
    }

    // create achive object from file pointer
    ON_BinaryFile archive( ON::read3dm, archive_fp );

    // read the contents of the file into "model"
    bool rc = model.Read( archive, dump );

    ON::CloseFile( archive_fp );

    outfp = wdb_fopen(outFileName);
    // print diagnostic
    if ( rc )
	dump->Print("Input 3dm file successfully read.\n");
    else
	dump->Print("Errors during reading 3dm file.\n");

    // see if everything is in good shape
    if ( model.IsValid(dump) )
	dump->Print("Model is valid.\n");
    else
	dump->Print("Model is not valid.\n");

    dump->Print("Number of NURBS objects read was %d\n.", model.m_object_table.Count());

    /* process layer table before building regions */
    ProcessLayers(model,dump);

    struct wmember all_regions;
    BU_LIST_INIT(&all_regions.l);

    for (int i = 0; i < model.m_object_table.Count(); i++ ) {
	dump->PushIndent();

	dump->Print("\n\nObject %d of %d:\n\n", i + 1, model.m_object_table.Count());
	
	// object's attibutes
	ON_3dmObjectAttributes myAttributes = model.m_object_table[i].m_attributes;

	std::string geom_base;
	myAttributes.Dump(*dump); // On debug print
	dump->Print("\n");
	
	if (use_uuidnames == 1) {
    	    char uuidstring[37];
    	    ON_UuidToString(myAttributes.m_uuid, uuidstring);
    	    ON_String constr(uuidstring);
	    const char* cstr = constr;
	    geom_base = cstr;
	} else {
    	    ON_String constr(myAttributes.m_name);
	    if (constr == NULL) {
		std::string genName = "";
		char name[256];
		/* Use layer name to help name un-named regions/objects */
		if (myAttributes.m_layer_index > 0) {
		    const ON_Layer& layer = model.m_layer_table[myAttributes.m_layer_index];
		    ON_wString layer_name = layer.LayerName();
		    strncpy(name, ON_String( layer_name ),255);
		    genName = name;
		    if (genName.length() <= 0) {
			genName = "rhino";
		    }
		    dump->Print("\n\nlayername:\"%s\"\n\n", name);
		} else {
		    genName = "rhino";
		}
		/* For layer named regions use layer region count
		 * instead of global region count
		 */
		if (genName.compare("rhino") == 0) {
		genName+=itoa(mcount++);
		geom_base = genName.c_str();
		} else {
		    int region_cnt = RegionCnt(genName);
		    genName+=itoa(region_cnt);
		    geom_base = genName.c_str();
		}
		dump->Print("Object has no name - creating one %s.\n", geom_base.c_str());
    	    } else {
		const char* cstr = constr;
		geom_base = cstr;
    	    }
	}

	std::string geom_name(geom_base+".s");
	std::string region_name(geom_base+".r");

        /* add region to hierarchical containers */
	MapRegion(model,region_name,myAttributes.m_layer_index);

	dump->Print("primitive is %s.\n", geom_name.c_str());
	dump->Print("region created is %s.\n", region_name.c_str());

	/* object definition
        Ah - rather than pulling JUST the geometry from the opennurbs object here, need to
        also get properties from ON_3dmObjectAttributes:
        Everything looks like it is in there - m_name, m_layer_index, etc.
        Will need to parse layers to get info for each object using a parent layer's settings
        Long term, material objects and render objects should be implemented in BRL-CAD
        to support conceptually similar breakouts of materials and shaders.  
        */

        const ON_Geometry* pGeometry = ON_Geometry::Cast(model.m_object_table[i].m_object);
	if ( pGeometry ) {
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
	    int r,g,b;
	    if (random_colors) {
    		r = int(256*drand48() + 1.0);
    		g = int(256*drand48() + 1.0);
    		b = int(256*drand48() + 1.0);
	    } else {
		r = (model.WireframeColor(myAttributes) & 0xFF);
    		g = ((model.WireframeColor(myAttributes)>>8) & 0xFF);
    		b = ((model.WireframeColor(myAttributes)>>16) & 0xFF);
	    }
            bu_log("Color: %d,%d,%d\n", r,g,b);
	    if ((brep = const_cast<ON_Brep * >(ON_Brep::Cast(pGeometry)))) {
		mk_id(outfp, id_name);
		mk_brep(outfp, geom_name.c_str(), brep);
		unsigned char rgb[] = {r,g,b};
		mk_region1(outfp, region_name.c_str(), geom_name.c_str(), "plastic", "", rgb);
		(void)mk_addmember(region_name.c_str(), &all_regions.l, NULL, WMOP_UNION);
		if (verbose_mode > 0) brep->Dump(*dump);
		dump->PopIndent();
     	    } else if (pGeometry->HasBrepForm()) {
		dump->Print("\n\n ***** HasBrepForm. ***** \n\n");
		dump->PopIndent();
	    } else if ((curve = const_cast<ON_Curve * >(ON_Curve::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_Curve. ***** \n\n");
		if (verbose_mode > 1) curve->Dump(*dump);
		dump->PopIndent();
	    } else if ((surface = const_cast<ON_Surface * >(ON_Surface::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_Surface. ***** \n\n");
		if (verbose_mode > 2) surface->Dump(*dump);
		dump->PopIndent();
	    } else if ((mesh = const_cast<ON_Mesh * >(ON_Mesh::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_Mesh. ***** \n\n");
		if (verbose_mode > 4) mesh->Dump(*dump);
		dump->PopIndent();
	    } else if ((revsurf = const_cast<ON_RevSurface * >(ON_RevSurface::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_RevSurface. ***** \n\n");
		if (verbose_mode > 2) revsurf->Dump(*dump);
		dump->PopIndent();
	    } else if ((planesurf = const_cast<ON_PlaneSurface * >(ON_PlaneSurface::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_PlaneSurface. ***** \n\n");
		if (verbose_mode > 2) planesurf->Dump(*dump);
		dump->PopIndent();
	    } else if ((instdef = const_cast<ON_InstanceDefinition * >(ON_InstanceDefinition::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_InstanceDefinition. ***** \n\n");
		if (verbose_mode > 3) instdef->Dump(*dump);
		dump->PopIndent();
	    } else if ((instref = const_cast<ON_InstanceRef * >(ON_InstanceRef::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_InstanceRef. ***** \n\n");
		if (verbose_mode > 3) instref->Dump(*dump);
		dump->PopIndent();
	    } else if ((layer = const_cast<ON_Layer * >(ON_Layer::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_Layer. ***** \n\n");
		if (verbose_mode > 3) layer->Dump(*dump);
		dump->PopIndent();
	    } else if ((light = const_cast<ON_Light * >(ON_Light::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_Light. ***** \n\n");
		if (verbose_mode > 3) light->Dump(*dump);
		dump->PopIndent();
	    } else if ((nurbscage = const_cast<ON_NurbsCage * >(ON_NurbsCage::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_NurbsCage. ***** \n\n");
		if (verbose_mode > 3) nurbscage->Dump(*dump);
		dump->PopIndent();
	    } else if ((morphctrl = const_cast<ON_MorphControl * >(ON_MorphControl::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_MorphControl. ***** \n\n");
		if (verbose_mode > 3) morphctrl->Dump(*dump);
		dump->PopIndent();
	    } else if ((group = const_cast<ON_Group * >(ON_Group::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_Group. ***** \n\n");
		if (verbose_mode > 3) group->Dump(*dump);
		dump->PopIndent();
	    } else if ((geom = const_cast<ON_Geometry * >(ON_Geometry::Cast(pGeometry)))) {
		dump->Print("\n\n ***** ON_Geometry. ***** \n\n");
		if (verbose_mode > 3) geom->Dump(*dump);
		dump->PopIndent();
	    } else {
		dump->Print("\n\n ***** Got a different kind of object than geometry - investigate. ***** \n\n");
	    }
	}
    }

    /* use accumulated layer information to build mged hierarchy */
    BuildHierarchy(outfp, dump);
    mk_lcomb(outfp, outFileName, &all_regions, 0, NULL, NULL, NULL, 0);
    wdb_close(outfp);

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
