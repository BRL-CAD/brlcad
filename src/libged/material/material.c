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

static const char *usage = " [-h] \n\n"
    "material create {objectName} {materialName} [parentName] [source] [[physicalProperties] . [mechanicalProperties] . [opticalProperties] . [thermalProperties] .]\n\n"
    "material destroy {object}\n\n"
    "material import {fileName} [--asid {materialId} ]\n"
    "material import {fileName} [--asname {materialName} ]\n\n"
    "material get {object} {propertyName}\n"
    "material get {object} {propertyGroupName} {propertyName}\n\n"
    "material set [-r] {object} {propertyName} [newPropertyValue]\n"
    "material set [-r] {object} {propertyGroupName} {propertyName} [newPropertyValue]\n\n"
    "- h         - Prints this help message.\n\n"
    "--asid      - Specifies the id the material will be imported with\n\n"
    "--asname    - Specifies the name the material will be imported with\n\n"
    "- r         - Removes a property from the object. (In the case of name, source, parent it merely sets the value to null.\n\n"
    "* Property arguments passed to the material commands are case sensitive.";

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

// Routine handles the import of a density table
int import_materials(struct ged *gedp, int argc, const char *argv[]){
    const char* fileName;
    const char* flag;
    if (argc < 3){
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
    }

    flag = argv[2];
    fileName = argv[3];

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
            char* num = strtok(buffer, "\t");
			char* material_name = strtok(NULL, "\t");
			char* density = strtok(NULL, "\t");
			(void)bu_avs_add(&physicalProperties, "density", density);
            (void)bu_avs_add(&physicalProperties, "id", num);
            if(strcmp("--asid", flag)==0){
                char mat_with_id[40];
                strcat(mat_with_id, "matl");
                strcat(mat_with_id, num);
                mk_material(gedp->ged_wdbp,
                    mat_with_id,
                    material_name,
                    "",
                    "",
                    &physicalProperties,
                    &mechanicalProperties,
                    &opticalProperties,
                    &thermalProperties);
                memset(mat_with_id, 0x00, 40);
            }
            else{
                mk_material(gedp->ged_wdbp,
                    material_name,
                    material_name,
                    "",
                    "",
                    &physicalProperties,
                    &mechanicalProperties,
                    &opticalProperties,
                    &thermalProperties);
            }
            memset(buffer, 0x00, 256);
		}
	}
    else{
        bu_vls_printf(gedp->ged_result_str, "ERROR: File does not exist.\n");
        return GED_ERROR;
    }
    return 0;
}

void print_avs_value(struct ged *gedp, const struct bu_attribute_value_set * avp, const char * name, const char * avsName){
    const char * val = bu_avs_get(avp, name);

    if (val != NULL){
        bu_vls_printf(gedp->ged_result_str, "%s", val);
    } else {
        bu_vls_printf(gedp->ged_result_str, "Error: unable to find the %s property %s.", avsName, name);
    }
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

    // Intialize AVS stores
    bu_avs_init_empty(&physicalProperties);
    bu_avs_init_empty(&mechanicalProperties);
    bu_avs_init_empty(&opticalProperties);
    bu_avs_init_empty(&thermalProperties);

    if (argc < 4){
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
        return GED_ERROR;
    }

    db_name = argv[2];
    name = argv[3];
    parent = NULL;
    source = NULL;
    
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

// routine handles getting individual properties of the material
int get_material(struct ged *gedp, int argc, const char *argv[]){
    struct directory *dp;
    struct rt_db_internal intern;

    if (argc < 4){
        bu_vls_printf(gedp->ged_result_str, "you must provide at least four arguments.");
        return GED_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[3], "name")){
            bu_vls_printf(gedp->ged_result_str, "%s", material->name.vls_str);
        } else if (BU_STR_EQUAL(argv[3], "parent")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->parent.vls_str);
        } else if (BU_STR_EQUAL(argv[3], "source")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->source.vls_str);
        } else {
            if (argc == 4){
                bu_vls_printf(gedp->ged_result_str, "the property you requested: %s, could not be found.", argv[3]);
                return GED_ERROR;
            } else if (BU_STR_EQUAL(argv[3], "physical")) {
                print_avs_value(gedp, &material->physicalProperties, argv[4], argv[3]);
            }  else if (BU_STR_EQUAL(argv[3], "mechanical")) {
                print_avs_value(gedp, &material->mechanicalProperties, argv[4], argv[3]);
            } else if (BU_STR_EQUAL(argv[3], "optical")) {
                print_avs_value(gedp, &material->opticalProperties, argv[4], argv[3]);
            } else if (BU_STR_EQUAL(argv[3], "thermal")) {
                print_avs_value(gedp, &material->thermalProperties, argv[4], argv[3]);
            } else {
                bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material property group:  %s", argv[3]);
                return GED_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[2]);
        return GED_ERROR;
    }

    return GED_OK;
}

// Routine handles the setting of a material property to a value
int set_material(struct ged *gedp, int argc, const char *argv[]){
    struct directory *dp;
    struct rt_db_internal intern;
    int removeProperty = 0;
    int propertyArg = 3;
    int subpropertyArg = 4;

    if (argc < 5){
        bu_vls_printf(gedp->ged_result_str, "you must provide at least five arguments.");
        return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[3], "-r")){
        removeProperty = 1;
        propertyArg++;
        subpropertyArg++;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[propertyArg], "name")){
            BU_VLS_INIT(&material->name);

            if (removeProperty) {
                bu_vls_strcpy(&material->name, NULL);
            } else {
                bu_vls_strcpy(&material->name, argv[4]);
            }
            
        } else if (BU_STR_EQUAL(argv[propertyArg], "parent")) {
            BU_VLS_INIT(&material->parent);

            if (removeProperty) {
                bu_vls_strcpy(&material->parent, NULL);
            } else {
                bu_vls_strcpy(&material->parent, argv[4]);
            }
        } else if (BU_STR_EQUAL(argv[propertyArg], "source")) {
            BU_VLS_INIT(&material->source);

            if (removeProperty) {
                bu_vls_strcpy(&material->source, NULL);
            } else {
                bu_vls_strcpy(&material->source, argv[4]);
            }
        } else {
            if (argc == 4 && !removeProperty){
                bu_vls_printf(gedp->ged_result_str, "the property you requested: %s, could not be found.", argv[3]);
                return GED_ERROR;
            } else if (BU_STR_EQUAL(argv[propertyArg], "physical")) {
                if (bu_avs_get(&material->physicalProperties, argv[subpropertyArg]) != NULL) {
                    bu_avs_remove(&material->physicalProperties, argv[subpropertyArg]);
                }
                
                if (!removeProperty) {
                    bu_avs_add(&material->physicalProperties, argv[4], argv[5]);
                }
            }  else if (BU_STR_EQUAL(argv[propertyArg], "mechanical")) {
                if (bu_avs_get(&material->mechanicalProperties, argv[subpropertyArg]) != NULL) {
                    bu_avs_remove(&material->mechanicalProperties, argv[subpropertyArg]);
                }

                if (!removeProperty) {
                    bu_avs_add(&material->mechanicalProperties, argv[4], argv[5]);
                }
            } else if (BU_STR_EQUAL(argv[propertyArg], "optical")) {
                if (bu_avs_get(&material->opticalProperties, argv[subpropertyArg]) != NULL) {
                    bu_avs_remove(&material->opticalProperties, argv[subpropertyArg]);
                }
                
                if (!removeProperty) {
                    bu_avs_add(&material->opticalProperties, argv[4], argv[5]);
                }
            } else if (BU_STR_EQUAL(argv[propertyArg], "thermal")) {
                if (bu_avs_get(&material->thermalProperties, argv[subpropertyArg]) != NULL) {
                    bu_avs_remove(&material->thermalProperties, argv[subpropertyArg]);
                }

                if (!removeProperty) {
                    bu_avs_add(&material->thermalProperties, argv[4], argv[5]);
                }
            } else {
                bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material property group:  %s", argv[propertyArg]);
                return GED_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[2]);
        return GED_ERROR;
    }

    return wdb_put_internal(gedp->ged_wdbp, argv[2], &intern, mk_conv2mm);
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

    if (scmd == MATERIAL_CREATE) {
        // create routine
        create_material(gedp, argc, argv);
    } else if (scmd == MATERIAL_DESTROY) {
        // destroy routine
        destroy_material(gedp, argc, argv);
    } else if(scmd == MATERIAL_IMPORT) {
        // import routine
        import_materials(gedp, argc, argv);
    } else if (scmd == MATERIAL_GET) {
        // get routine
        get_material(gedp, argc, argv);
    } else if (scmd == MATERIAL_SET) {
        // set routine
        set_material(gedp, argc, argv);
    } else {
        bu_vls_printf(gedp->ged_result_str, "Error: %s is not a valid subcommand.\n", argv[1]);
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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