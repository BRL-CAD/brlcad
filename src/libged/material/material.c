/*                         M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/material.c
 *
 * The material command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"
#include "wdb.h"

typedef enum {
    MATERIAL_CREATE,
    MATERIAL_DESTROY,
    MATERIAL_GET,
    MATERIAL_IMPORT,
    MATERIAL_SET,
    ATTR_UNKNOWN
} material_cmd_t;

static const char *usage = " [create] [destroy] [get] [import] [set] [options] object [args] \n";

HIDDEN material_cmd_t
get_material_cmd(const char* arg)
{
    /* sub-commands */
    const char CREATE[] = "create";
    const char DESTROY[]   = "destroy";
    const char GET[]    = "get";
    const char IMPORT[] = "import";
    const char SET[]    = "set";

    /* alphabetical order */
    if (BU_STR_EQUIV(CREATE, arg))
	return MATERIAL_CREATE;
    else if (BU_STR_EQUIV(DESTROY, arg))
	return MATERIAL_DESTROY;
    else if (BU_STR_EQUIV(SET, arg))
	return MATERIAL_SET;
    else if (BU_STR_EQUIV(GET, arg))
	return MATERIAL_GET;
    else if (BU_STR_EQUIV(IMPORT, arg))
    return MATERIAL_IMPORT;
    else
    return ATTR_UNKNOWN;
}
//Routine handles the import of a density table
int import_materials(struct ged *gedp, int argc, const char *argv[]){
    const char* fileName;

    fileName = argv[2];

    if (argc > 3){
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
    }

    FILE *densityTable = fopen(fileName, "r");
	if(densityTable != NULL){
		char buffer[256];
		while(fgets(buffer, 256, densityTable)){
            struct bu_attribute_value_set physicalProperties;
            struct bu_attribute_value_set mechanicalProperties;
            struct bu_attribute_value_set opticalProperties;
            struct bu_attribute_value_set thermalProperties;
            bu_avs_init_empty(&physicalProperties);
            bu_avs_init_empty(&mechanicalProperties);
            bu_avs_init_empty(&opticalProperties);
            bu_avs_init_empty(&thermalProperties);
            if(buffer[strlen(buffer)-1] == '\n'){
				buffer[strlen(buffer)-1] = '\0';
                buffer[strlen(buffer)-1] = '\0';
			}
            char* num __attribute__((unused))= strtok(buffer, "\t");
			char* material_name = strtok(NULL, "\t");
			char* density = strtok(NULL, "\t");
			(void)bu_avs_add(&physicalProperties, material_name, density);
            mk_material(gedp->ged_wdbp,
                material_name,
                material_name,
                "",
                "",
                &physicalProperties,
                &mechanicalProperties,
                &opticalProperties,
                &thermalProperties);
            memset(buffer, 0x00, 256);
		}
	}
    else{
        bu_vls_printf(gedp->ged_result_str, "ERROR: File does not exist.\n");
        return GED_ERROR;
    }
    return 0;
}

// Routine handles the creation of a material
int create_material(struct ged *gedp, int argc, const char *argv[]){
    const char* db_name;
    const char* name;
    const char* parent;
    const char* source;
    struct bu_attribute_value_set physicalProperties;
    struct bu_attribute_value_set mechanicalProperties;
    struct bu_attribute_value_set opticalProperties;
    struct bu_attribute_value_set thermalProperties;

    if (argc < 13){
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
    }

    db_name = argv[2];
    name = argv[3];
    parent = argv[4];
    source = argv[5];

    // Intialize AVS stores
    bu_avs_init_empty(&physicalProperties);
    bu_avs_init_empty(&mechanicalProperties);
    bu_avs_init_empty(&opticalProperties);
    bu_avs_init_empty(&thermalProperties);

    int arg_idx = 6;
    int arg_ptr = 7;
    
    while (1) {
        if (argc < arg_ptr) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx]) && arg_ptr % 2 == 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx])) {
            // increment counters to get key values in next loop
            arg_idx += 1;
            arg_ptr += 1;
            break;
        }

        // have to check the next arg after we know it is not '.'
        if (argc < arg_ptr + 1) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx + 1]) && (arg_ptr + 1) % 2 != 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        // if we make it here we have a valid key value set for argv[arg_idx] and argv[arg_idx + 1]
        (void)bu_avs_add(&physicalProperties, argv[arg_idx], argv[arg_idx + 1]);

        // increment the counters by two so we can get the next pair
        arg_idx += 2;
        arg_ptr += 2;
    }

    while (1) {
        if (argc < arg_ptr) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx]) && arg_ptr % 2 != 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx])) {
            // increment counters to get key values in next loop
            arg_idx += 1;
            arg_ptr += 1;
            break;
        }

        // have to check the next arg after we know it is not '.'
        if (argc < arg_ptr + 1) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx + 1]) && (arg_ptr + 1) % 2 == 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        // if we make it here we have a valid key value set for argv[arg_idx] and argv[arg_idx + 1]
        (void)bu_avs_add(&mechanicalProperties, argv[arg_idx], argv[arg_idx + 1]);

        // increment the counters by two so we can get the next pair
        arg_idx += 2;
        arg_ptr += 2;
    }

    while (1) {
        if (argc < arg_ptr) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx]) && arg_ptr % 2 == 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx])) {
            // increment counters to get key values in next loop
            arg_idx += 1;
            arg_ptr += 1;
            break;
        }

        // have to check the next arg after we know it is not '.'
        if (argc < arg_ptr + 1) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx + 1]) && (arg_ptr + 1) % 2 != 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        // if we make it here we have a valid key value set for argv[arg_idx] and argv[arg_idx + 1]
        (void)bu_avs_add(&opticalProperties, argv[arg_idx], argv[arg_idx + 1]);

        // increment the counters by two so we can get the next pair
        arg_idx += 2;
        arg_ptr += 2;
    }

    while (1) {
        if (argc < arg_ptr) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx]) && arg_ptr % 2 != 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx])) {
            arg_idx += 1;
            arg_ptr += 1;
            break;
        }

        // have to check the next arg after we know it is not '.'
        if (argc < arg_ptr + 1) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	        return GED_ERROR;
        }

        if (BU_STR_EQUAL(".", argv[arg_idx + 1]) && (arg_ptr + 1) % 2 == 0) {
            bu_vls_printf(gedp->ged_result_str, "ERROR, key value pairs entered incorrectly!\n");
	        return GED_ERROR;
        }

        // if we make it here we have a valid key value set for argv[arg_idx] and argv[arg_idx + 1]
        (void)bu_avs_add(&thermalProperties, argv[arg_idx], argv[arg_idx + 1]);

        // increment the counters by two so we can get the next pair
        arg_idx += 2;
        arg_ptr += 2;
    }
    
    mk_material(gedp->ged_wdbp,
            db_name,
            name,
            parent,
            source,
            &physicalProperties,
            &mechanicalProperties,
            &opticalProperties,
            &thermalProperties);

    return 0;
}

// Routine handles the deletion of a material
int destroy_material(struct ged *gedp, int argc, const char *argv[]){
struct directory *dp;
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (argc != 3){
        bu_vls_printf(gedp->ged_result_str, "ERROR, incorrect number of arguments.");
        return GED_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    _dl_eraseAllNamesFromDisplay(gedp, argv[2], 0);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
	    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
            bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[2]);
		    return GED_ERROR;
	    }

        if (db_delete(gedp->dbip, dp) != 0 || db_dirdelete(gedp->dbip, dp) != 0) {
		/* Abort kill processing on first error */
            bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[2]);
            return GED_ERROR;
	    }
    }

    /* Update references. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    return GED_OK;
}

int
ged_material_core(struct ged *gedp, int argc, const char *argv[]){
    material_cmd_t scmd;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialization */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* incorrect arguments */
    if (argc < 3) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return GED_HELP;
    }

    scmd = get_material_cmd(argv[1]);

    if (scmd == MATERIAL_CREATE){
        // create routine
        create_material(gedp, argc, argv);
    } else if (scmd == MATERIAL_DESTROY) {
        // destroy routine
        destroy_material(gedp, argc, argv);
    } else if(scmd == MATERIAL_IMPORT){
        //import routine
        import_materials(gedp, argc, argv);
    } else if (scmd == MATERIAL_GET) {
        // get routine
        bu_vls_printf(gedp->ged_result_str, "Trying: get");
    } else if (scmd == MATERIAL_SET) {
        // set routine
        bu_vls_printf(gedp->ged_result_str, "Trying: set");
    } 

    return 0;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl material_cmd_impl = {
    "material",
    ged_material_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd material_cmd = { &material_cmd_impl };
const struct ged_cmd *material_cmds[] = { &material_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  material_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */