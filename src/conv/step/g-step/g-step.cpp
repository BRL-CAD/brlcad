/*                    G -  S T E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file step/g-step.cpp
 *
 * C++ main() for g-step converter.
 *
 */

#include "AP203.h"

#include "bu.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

#include <iostream>

// step-g related headers
#include "SdaiHeaderSchema.h"
#include "schema.h"

#include "G_Objects.h"
#include "Default_Geometric_Context.h"

void
usage()
{
    std::cerr << "Usage: g-step -o outfile.stp infile.g \n" << std::endl;
}


int
main(int argc, char *argv[])
{
    STEPentity *shape;
    STEPentity *product;
    int ret = 0;
    int convert_tops_list = 0;
    struct directory **paths;
    int path_cnt = 0;
    int flip_transforms = 0;
    AP203_Contents *sc = new AP203_Contents;

    // process command line arguments
    int c;
    char *output_file = (char *)NULL;
    while ((c = bu_getopt(argc, argv, "fo:")) != -1) {
	switch (c) {
	    case 'o':
		output_file = bu_optarg;
		break;
	    case 'f':
		flip_transforms = 1;
		break;
	    default:
		usage();
		bu_exit(1, NULL);
		break;
	}
    }

    if (bu_optind >= argc || output_file == (char *)NULL) {
	usage();
	bu_exit(1, NULL);
    }

    argc -= bu_optind;
    argv += bu_optind;

    /* check our inputs/outputs */
    if (bu_file_exists(output_file, NULL)) {
	bu_exit(1, "ERROR: refusing to overwrite existing output file:\"%s\". Please remove file or change output file name and try again.", output_file);
    }

    if (!bu_file_exists(argv[0], NULL) && !BU_STR_EQUAL(argv[0], "-")) {
	bu_exit(2, "ERROR: unable to read input \"%s\" .g file", argv[0]);
    }

    if (argc < 2) {
	convert_tops_list = 1;
    }


    std::string iflnm = argv[0];
    std::string oflnm = output_file;

    /* load .g file */
    BRLCADWrapper *dotg  = new BRLCADWrapper();
    if (!dotg) {
	std::cerr << "ERROR: unable to create BRL-CAD instance" << std::endl;
	return 3;
    }
    if (!dotg->load(iflnm)) {
	std::cerr << "ERROR: unable to open BRL-CAD input file [" << oflnm << "]" << std::endl;
	delete dotg;
	return 2;
    }

    struct db_i *dbip = dotg->GetDBIP();
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);

    if (convert_tops_list) {
	/* Need db_update_nref for DB_LS_TOPS to work */
	db_update_nref(dbip, &rt_uniresource);
	path_cnt = db_ls(dbip, DB_LS_TOPS, &paths);
	if (!path_cnt) {
	    std::cerr << "ERROR: no objects found in .g file" << "\n" << std::endl;
	    delete dotg;
	    return 1;
	}
    } else {
	int i = 1;
	paths = (struct directory **)bu_malloc(sizeof(struct directory *) * argc, "dp array");
	while (i < argc) {
	    bu_log("%d: %s\n", i, argv[i]);
	    struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		std::cerr << "ERROR: cannot find " << argv[i] << "\n" << std::endl;
		delete dotg;
		bu_free(paths, "free path memory");
		return 1;
	    } else {
		paths[i-1] = dp;
		path_cnt++;
		i++;
	    }
	}
	paths[i-1] = RT_DIR_NULL;
    }


    struct bu_vls scratch_string;
    bu_vls_init(&scratch_string);

    Registry *registry = new Registry(SchemaInit);
    InstMgr instance_list;
    STEPfile *sfile = new STEPfile(*registry, instance_list);

    registry->ResetSchemas();
    registry->ResetEntities();

    /* Populate the header instances */
    InstMgr *header_instances = sfile->HeaderInstances();

    /* 1 - Populate File_Name */
    SdaiFile_name * fn = (SdaiFile_name *)sfile->HeaderDefaultFileName();
    bu_vls_sprintf(&scratch_string, "'%s'", output_file);
    fn->name_(bu_vls_addr(&scratch_string));
    fn->time_stamp_("");
    StringAggregate_ptr author_tmp = new StringAggregate;
    author_tmp->AddNode(new StringNode("''"));
    fn->author_(author_tmp);
    StringAggregate_ptr org_tmp = new StringAggregate;
    org_tmp->AddNode(new StringNode("''"));
    fn->organization_(org_tmp);
    fn->preprocessor_version_("'BRL-CAD g-step exporter'");
    fn->originating_system_("''");
    fn->authorization_("''");
    header_instances->Append((SDAI_Application_instance *)fn, completeSE);

    /* 2 - Populate File_Description */
    SdaiFile_description * fd = (SdaiFile_description *)sfile->HeaderDefaultFileDescription();
    StringAggregate_ptr description_tmp = new StringAggregate;
    description_tmp->AddNode(new StringNode("''"));
    fd->description_(description_tmp);
    fd->implementation_level_("'2;1'");
    header_instances->Append((SDAI_Application_instance *)fd, completeSE);

    /* 3 - Populate File_Schema */
    SdaiFile_schema *fs = (SdaiFile_schema *)sfile->HeaderDefaultFileSchema();
    StringAggregate_ptr schema_tmp = new StringAggregate;
    schema_tmp->AddNode(new StringNode("'CONFIG_CONTROL_DESIGN'"));
    fs->schema_identifiers_(schema_tmp);
    header_instances->Append((SDAI_Application_instance *)fs, completeSE);

    sc->registry = registry;
    sc->instance_list = &instance_list;

    sc->default_context = Add_Default_Geometric_Context(sc);

    sc->application_context = (SdaiApplication_context *)sc->registry->ObjCreate("APPLICATION_CONTEXT");
    sc->instance_list->Append((STEPentity *)sc->application_context, completeSE);
    sc->application_context->application_("'CONFIGURATION CONTROLLED 3D DESIGNS OF MECHANICAL PARTS AND ASSEMBLIES'");

    sc->design_context = (SdaiDesign_context *)sc->registry->ObjCreate("DESIGN_CONTEXT");
    sc->instance_list->Append((STEPentity *)sc->design_context, completeSE);
    sc->design_context->name_("''");
    sc->design_context->life_cycle_stage_("'design'");
    sc->design_context->frame_of_reference_(sc->application_context);

    sc->solid_to_step = new std::map<struct directory *, STEPentity *>;
    sc->solid_to_step_shape = new std::map<struct directory *, STEPentity *>;
    sc->comb_to_step = new std::map<struct directory *, STEPentity *>;
    sc->comb_to_step_shape = new std::map<struct directory *, STEPentity *>;

    sc->flip_transforms = flip_transforms;

    for (int i = 0; i < path_cnt; i++) {
	/* Now, add actual DATA */
	struct directory *dp = paths[i];
	struct rt_db_internal intern;
	rt_db_get_internal(&intern, dp, dbip, bn_mat_identity, &rt_uniresource);
	RT_CK_DB_INTERNAL(&intern);
	Object_To_STEP(dp, &intern, wdbp, sc);
	rt_db_free_internal(&intern);
    }

    /* Write STEP file */
    if (!bu_file_exists(output_file, NULL)) {
	std::ofstream stepout(output_file);
	sfile->WriteExchangeFile(stepout);
    }

    /* Free memory */
    header_instances->DeleteInstances();
    instance_list.DeleteInstances();
    delete dotg;
    delete registry;
    delete sfile;
    delete sc->solid_to_step;
    delete sc->solid_to_step_shape;
    delete sc->comb_to_step;
    delete sc->comb_to_step_shape;
    delete sc;
    bu_vls_free(&scratch_string);
    bu_free(paths, "free dp list");

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
