/*                         M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2021-2022 United States Government as represented by
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
    MATERIAL_ASSIGN,
    MATERIAL_CREATE,
    MATERIAL_DESTROY,
    MATERIAL_GET,
    MATERIAL_HELP,
    MATERIAL_IMPORT,
    MATERIAL_REMOVE,
    MATERIAL_SET,
    ATTR_UNKNOWN
} material_cmd_t;

static const char *usage = " help \n\n"
    "material create {objectName} {materialName}\n\n"
    "material destroy {object}\n\n"
    "material assign {object} {materialName}\n\n"
    "material get {object} [propertyGroupName] {propertyName}\n\n"
    "material set {object} [propertyGroupName] {propertyName} [newPropertyValue]\n\n"
    "material remove {object} [propertyGroupName] {propertyName}\n\n"
    "material import [--id | --name] {fileName}\n\n"
    "  --id       - Specifies the id the material will be imported with\n\n"
    "  --name     - Specifies the name the material will be imported with\n\n"
    "Note: Object, property, and group names are case sensitive.";

static const char *possibleProperties = "The following are properties of material objects that can be set/modified: \n"
    "- name\n"
    "- source\n"
    "- parent\n\n"
    "The following are property groups (utilizable in [propertyGroupName] for materials): \n"
    "- physical\n"
    "- mechanical\n"
    "- optical\n"
    "- thermal\n";


static material_cmd_t
get_material_cmd(const char* arg)
{
    /* sub-commands */
    if (BU_STR_EQUIV("assign", arg))
	return MATERIAL_ASSIGN;
    else if (BU_STR_EQUIV("create", arg))
	return MATERIAL_CREATE;
    else if (BU_STR_EQUIV("destroy", arg))
	return MATERIAL_DESTROY;
    else if (BU_STR_EQUIV("set", arg))
	return MATERIAL_SET;
    else if (BU_STR_EQUIV("get", arg))
	return MATERIAL_GET;
    else if (BU_STR_EQUIV("help", arg))
	return MATERIAL_HELP;
    else if (BU_STR_EQUIV("import", arg))
	return MATERIAL_IMPORT;
    else if (BU_STR_EQUIV("remove", arg))
	return MATERIAL_REMOVE;
    else
	return ATTR_UNKNOWN;
}


static int
assign_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct bu_attribute_value_set avs;

    if (argc < 4) {
        bu_vls_printf(gedp->ged_result_str, "you must provide at least four arguments.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
        bu_avs_init_empty(&avs);

        if (db5_get_attributes(gedp->dbip, &avs, dp)) {
            bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
            return BRLCAD_ERROR;
        } else {
            bu_avs_add(&avs, "material_name", argv[3]);
            bu_avs_add(&avs, "material_id", "1");
        }

        if (db5_update_attributes(dp, &avs, gedp->dbip)) {
            bu_vls_printf(gedp->ged_result_str, "Error: failed to update attributes\n");
            return BRLCAD_ERROR;
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "Cannot get object %s\n", argv[2]);
        return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/* Routine handles the import of a density table.
 *
 * FIXME: this routine is derived from libanalyze, but it would be
 * better to call analyze_densities_load() to ensure consistent
 * density file parsing and reduced code.
 */
static int
import_materials(struct ged *gedp, int argc, const char *argv[])
{
    const char* fileName;
    const char* flag;
    char buffer[256] = {0};

    if (argc < 3) {
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
    }

    flag = argv[2];
    fileName = argv[3];

    FILE *densityTable = fopen(fileName, "r");
    if (!densityTable) {
        bu_vls_printf(gedp->ged_result_str, "ERROR: File does not exist.\n");
        return BRLCAD_ERROR;
    }

    while (bu_fgets(buffer, 256, densityTable)) {
	char *p = buffer;
	char *q;
	buffer[strlen(buffer)] = '\0';
	char name[30];
	double density = -1;
	int have_density = 0;
	int idx = 0;
	p = buffer;

	/* Skip initial whitespace */
	while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
	    p++;

	/* Skip initial comments */
	while (*p == '#') {
	    /* Skip comment */
	    while (*p && *p != '\n') {
		p++;
	    }
	}

	/* Skip whitespace */
	while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
	    p++;

	while (*p) {
	    int len = 0;

	    if (*p == '#') {
		while (*p && *p != '\n')
		    p++;

		/* Skip whitespace */
		while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
		    p++;
		continue;
	    }

	    if (have_density) {
		bu_free(buffer, "free buffer copy");
		bu_free(name, "free name copy");
		bu_vls_printf(gedp->ged_result_str, "Error processing: Extra content after density entry\n");
		return BRLCAD_ERROR;
	    }
	    idx = strtol(p, &q, 10);
	    if (idx < 0) {
		bu_free(buffer, "free buffer copy");
		bu_vls_printf(gedp->ged_result_str, "Error processing: Bad density index\n");
		return BRLCAD_ERROR;
	    }
	    density = strtod(q, &p);
	    if (q == p) {
		bu_free(buffer, "free buffer copy");
		bu_vls_printf(gedp->ged_result_str, "Error processing: Could not convert density\n");
		return BRLCAD_ERROR;
	    }

	    if (density < 0.0) {
		bu_free(buffer, "Free buffer copy");
		bu_vls_printf(gedp->ged_result_str, "Error processing: Bad Density\n");
		return BRLCAD_ERROR;
	    }
	    while (*p && (*p == '\t' || *p == ' ')) p++;
	    if (!*p) {
		bu_vls_printf(gedp->ged_result_str, "Error processing: Missing name\n");
		return BRLCAD_ERROR;
	    }

	    while (*(p + len) && !(*(p + len) == '\n' || *(p+len) == '#')) {
		len++;
	    }

	    while (!((*(p + len) >= 'A' && *(p + len) <= 'Z') ||  (*(p + len) >= 'a' && *(p + len) <= 'z') || (*(p + len) >= '1' && *(p + len) <= '9'))) {
		len--;
	    }
	    strncpy(name, p, len+1);
	    break;

	}

	if (idx == 0) {
	    continue;
	}

	struct bu_attribute_value_set physicalProperties;
	struct bu_attribute_value_set mechanicalProperties;
	struct bu_attribute_value_set opticalProperties;
	struct bu_attribute_value_set thermalProperties;

	bu_avs_init_empty(&physicalProperties);
	bu_avs_init_empty(&mechanicalProperties);
	bu_avs_init_empty(&opticalProperties);
	bu_avs_init_empty(&thermalProperties);

	char idxChar[6];
	sprintf(idxChar, "%d", idx);

	char densityChar[50];
	sprintf(densityChar, "%.3f", density);

	bu_avs_add(&physicalProperties, "density", densityChar);
	bu_avs_add(&physicalProperties, "id", idxChar);

	if (BU_STR_EQUAL("--id", flag)) {
	    char mat_with_id[40];

	    strcat(mat_with_id, "matl");
	    strcat(mat_with_id, idxChar);

	    mk_material(gedp->ged_wdbp,
			mat_with_id,
			name,
			"",
			"",
			&physicalProperties,
			&mechanicalProperties,
			&opticalProperties,
			&thermalProperties);
	    memset(mat_with_id, 0x00, 40);
	} else {
	    mk_material(gedp->ged_wdbp,
			name,
			name,
			"",
			"",
			&physicalProperties,
			&mechanicalProperties,
			&opticalProperties,
			&thermalProperties);
	}
	memset(buffer, 0, 256);
	memset(name, 0, 30);
    }

    return 0;
}


static void
print_avs_value(struct ged *gedp, const struct bu_attribute_value_set * avp, const char * name, const char * avsName)
{
    const char * val = bu_avs_get(avp, name);

    if (val != NULL) {
        bu_vls_printf(gedp->ged_result_str, "%s", val);
    } else {
        bu_vls_printf(gedp->ged_result_str, "Error: unable to find the %s property %s.", avsName, name);
    }
}


// Routine handles the creation of a material
static int
create_material(struct ged *gedp, int argc, const char *argv[])
{
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

    if (argc < 4) {
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
        return BRLCAD_ERROR;
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
static int
destroy_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if (argc != 3) {
        bu_vls_printf(gedp->ged_result_str, "ERROR, incorrect number of arguments.");
        return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    _dl_eraseAllNamesFromDisplay(gedp, argv[2], 0);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
            bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[2]);
	    return BRLCAD_ERROR;
	}

        if (db_delete(gedp->dbip, dp) != 0 || db_dirdelete(gedp->dbip, dp) != 0) {
	    /* Abort kill processing on first error */
            bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[2]);
            return BRLCAD_ERROR;
	}
    }

    /* Update references. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    return BRLCAD_OK;
}


// routine handles getting individual properties of the material
static int
get_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;

    if (argc < 4) {
        bu_vls_printf(gedp->ged_result_str, "you must provide at least four arguments.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, BRLCAD_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[3], "name")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->name.vls_str);
        } else if (BU_STR_EQUAL(argv[3], "parent")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->parent.vls_str);
        } else if (BU_STR_EQUAL(argv[3], "source")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->source.vls_str);
        } else {
            if (argc == 4) {
                bu_vls_printf(gedp->ged_result_str, "the property you requested: %s, could not be found.", argv[3]);
                return BRLCAD_ERROR;
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
                return BRLCAD_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[2]);
        return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


// Routine handles the setting of a material property to a value
static int
set_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;

    if (argc < 5) {
        bu_vls_printf(gedp->ged_result_str, "you must provide at least five arguments.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, BRLCAD_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[3], "name")) {
            BU_VLS_INIT(&material->name);
            bu_vls_strcpy(&material->name, argv[4]);
        } else if (BU_STR_EQUAL(argv[3], "parent")) {
            BU_VLS_INIT(&material->parent);
            bu_vls_strcpy(&material->parent, argv[4]);
        } else if (BU_STR_EQUAL(argv[3], "source")) {
            BU_VLS_INIT(&material->source);
            bu_vls_strcpy(&material->source, argv[4]);
        } else {
            if (BU_STR_EQUAL(argv[3], "physical")) {
                bu_avs_remove(&material->physicalProperties, argv[4]);
                bu_avs_add(&material->physicalProperties, argv[4], argv[5]);
            }  else if (BU_STR_EQUAL(argv[3], "mechanical")) {
                bu_avs_remove(&material->mechanicalProperties, argv[4]);
                bu_avs_add(&material->mechanicalProperties, argv[4], argv[5]);
            } else if (BU_STR_EQUAL(argv[3], "optical")) {
                bu_avs_remove(&material->opticalProperties, argv[4]);
                bu_avs_add(&material->opticalProperties, argv[4], argv[5]);
            } else if (BU_STR_EQUAL(argv[3], "thermal")) {
                bu_avs_remove(&material->thermalProperties, argv[4]);
                bu_avs_add(&material->thermalProperties, argv[4], argv[5]);
            } else {
                bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material property group:  %s", argv[3]);
                return BRLCAD_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[2]);
        return BRLCAD_ERROR;
    }

    return wdb_put_internal(gedp->ged_wdbp, argv[2], &intern, mk_conv2mm);
}


// Routine handles the removal of a material property
static int
remove_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;

    if (argc < 4) {
        bu_vls_printf(gedp->ged_result_str, "you must provide at least four arguments.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[2], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, BRLCAD_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[3], "name")) {
            BU_VLS_INIT(&material->name);
            bu_vls_strcpy(&material->name, NULL);
        } else if (BU_STR_EQUAL(argv[3], "parent")) {
            BU_VLS_INIT(&material->parent);
            bu_vls_strcpy(&material->parent, NULL);
        } else if (BU_STR_EQUAL(argv[3], "source")) {
            BU_VLS_INIT(&material->source);
            bu_vls_strcpy(&material->source, NULL);
        } else {
            if (BU_STR_EQUAL(argv[3], "physical")) {
                bu_avs_remove(&material->physicalProperties, argv[4]);
            }  else if (BU_STR_EQUAL(argv[3], "mechanical")) {
                bu_avs_remove(&material->mechanicalProperties, argv[4]);
            } else if (BU_STR_EQUAL(argv[3], "optical")) {
                bu_avs_remove(&material->opticalProperties, argv[4]);
            } else if (BU_STR_EQUAL(argv[3], "thermal")) {
                bu_avs_remove(&material->thermalProperties, argv[4]);
            } else {
                bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material property group:  %s", argv[3]);
                return BRLCAD_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[2]);
        return BRLCAD_ERROR;
    }

    return wdb_put_internal(gedp->ged_wdbp, argv[2], &intern, mk_conv2mm);
}


static int
ged_material_core(struct ged *gedp, int argc, const char *argv[])
{
    material_cmd_t scmd;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialization */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* incorrect arguments */
    if (argc < 2) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return BRLCAD_HELP;
    }

    scmd = get_material_cmd(argv[1]);

    if (scmd == MATERIAL_ASSIGN) {
        // assign routine
        assign_material(gedp, argc, argv);
    } else if (scmd == MATERIAL_CREATE) {
        // create routine
        create_material(gedp, argc, argv);
    } else if (scmd == MATERIAL_DESTROY) {
        // destroy routine
        destroy_material(gedp, argc, argv);
    } else if (scmd == MATERIAL_IMPORT) {
        // import routine
        import_materials(gedp, argc, argv);
    } else if (scmd == MATERIAL_GET) {
        // get routine
        get_material(gedp, argc, argv);
    } else if (scmd == MATERIAL_HELP) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n\n\n", argv[0], usage);
        bu_vls_printf(gedp->ged_result_str, "%s", possibleProperties);
    }
    else if (scmd == MATERIAL_REMOVE) {
        // set routine
        remove_material(gedp, argc, argv);
    }
    else if (scmd == MATERIAL_SET) {
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

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
