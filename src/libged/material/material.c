/*                         M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2021-2025 United States Government as represented by
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

#include <stdio.h>
#include "parser.h"

typedef enum {
    MATERIAL_ASSIGN,
    MATERIAL_CREATE,
    MATERIAL_DESTROY,
    MATERIAL_GET,
    MATERIAL_HELP,
    MATERIAL_IMPORT,
    MATERIAL_EXPORT,
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
    "material export {fileName}\n\n"
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
    else if (BU_STR_EQUIV("export", arg))
    return MATERIAL_EXPORT;
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
    printf("material: import_materials");
    const char* fileName;
    const char* flag;
    char buffer[BUFSIZ] = {0};

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

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    while (bu_fgets(buffer, BUFSIZ, densityTable)) {
	char *p;
	double density = -1;
	int have_density = 0;
	int idx = 0;
	int aborted;
	struct bu_vls name = BU_VLS_INIT_ZERO;

	/* reset every pass */
	p = buffer;
	aborted = 0;

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
	    char *q = NULL;

	    if (*p == '#') {
		while (*p && *p != '\n')
		    p++;

		/* Skip whitespace */
		while (*p && (*p == '\t' || *p == ' ' || *p == '\n' || *p == '\r'))
		    p++;
		continue;
	    }

	    if (have_density) {
		aborted = 1;
		bu_vls_printf(gedp->ged_result_str, "ERROR: Extra content after density entry\n");
		break;
	    }
	    idx = strtol(p, &q, 10);
	    if (idx < 0) {
		aborted = 1;
		bu_vls_printf(gedp->ged_result_str, "ERROR: Bad density index\n");
		break;
	    }
	    density = strtod(q, &p);
	    if (q == p) {
		aborted = 1;
		bu_vls_printf(gedp->ged_result_str, "ERROR: Could not convert density\n");
		break;
	    }

	    if (density < 0.0) {
		aborted = 1;
		bu_vls_printf(gedp->ged_result_str, "ERROR: Bad Density\n");
		break;
	    }
	    while (*p && (*p == '\t' || *p == ' ')) p++;
	    if (!*p) {
		aborted = 1;
		bu_vls_printf(gedp->ged_result_str, "ERROR: Missing name\n");
		break;
	    }

	    while (*(p + len) && !(*(p + len) == '\n' || *(p+len) == '#')) {
		len++;
	    }

	    while (!((*(p + len) >= 'A' && *(p + len) <= 'Z') ||  (*(p + len) >= 'a' && *(p + len) <= 'z') || (*(p + len) >= '1' && *(p + len) <= '9'))) {
		len--;
	    }

	    bu_vls_strncpy(&name, p, len+1);
	    break;
	}

	if (aborted) {
	    bu_free(buffer, "free buffer copy");
	    bu_vls_free(&name);
	    return BRLCAD_ERROR;
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

	struct bu_vls idxChar = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&idxChar, "%d", idx);

	struct bu_vls densityChar = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&densityChar, "%.3f", density);

	bu_avs_add(&physicalProperties, "density", bu_vls_cstr(&densityChar));
	bu_avs_add(&physicalProperties, "id", bu_vls_cstr(&idxChar));

	if (BU_STR_EQUAL("--id", flag)) {
	    struct bu_vls mat_with_id = BU_VLS_INIT_ZERO;

	    bu_vls_strcat(&mat_with_id, "matl");
	    bu_vls_vlscat(&mat_with_id, &idxChar);

	    mk_material(wdbp,
			bu_vls_cstr(&mat_with_id),
			bu_vls_cstr(&name),
			"",
			"",
			&physicalProperties,
			&mechanicalProperties,
			&opticalProperties,
			&thermalProperties);
	    bu_vls_free(&mat_with_id);
	} else {
	    mk_material(wdbp,
			bu_vls_cstr(&name),
			bu_vls_cstr(&name),
			"",
			"",
			&physicalProperties,
			&mechanicalProperties,
			&opticalProperties,
			&thermalProperties);
	}
	bu_vls_free(&idxChar);
	bu_vls_free(&densityChar);
	bu_vls_free(&name);

	memset(buffer, 0, BUFSIZ);
    }

    return 0;
}

// Export all materials in a file to the filename provided
static int
export_materials(struct ged *gedp, int argc, const char *argv[])
{
    printf("material: export_matprop_file");
    const char* fileName;
    FILE* file;

    if (argc < 3) {
        bu_vls_printf(gedp->ged_result_str, "ERROR: Not enough arguments.\nUsage: material export <filename>\n");
        return BRLCAD_ERROR;
    }
    fileName = argv[2]; 

    file = fopen(fileName, "w");
    if (file == NULL) {
        bu_vls_printf(gedp->ged_result_str, "ERROR: Could not open file '%s'\n", fileName);
        return BRLCAD_ERROR;
    }

    for (int i = 0; i < RT_DBNHASH; i++) {
		for (struct directory *dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
            // Skip non-BRLCAD objects
            if (!(dp->d_major_type == DB5_MAJORTYPE_BRLCAD)) {
                continue;
            }
            
            struct rt_db_internal intern;
            if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) >= 0) {
                if (BU_STR_EQUIV(intern.idb_meth->ft_label, "material")) {
                    fprintf(file, "%s\n{\n", dp->d_namep);
                    struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

                    struct bu_attribute_value_set* avs_list[] = {
                        &material->opticalProperties,
                        &material->thermalProperties,
                        &material->physicalProperties,
                        &material->mechanicalProperties,
                    };

                    for (int j = 0; j < 4; j++) {
                        struct bu_attribute_value_set *avs = avs_list[j];
                        for (size_t i = 0; i < avs->count; i++) {
                            const char *key = avs->avp[i].name;
                            const char *value = avs->avp[i].value;
                            fprintf(file, "\t%s = %s\n", key, value);
                        }
                    }
                    fprintf(file, "}\n\n");
                }
                rt_db_free_internal(&intern);
            }
        }
    }
    fclose(file);
    bu_vls_printf(gedp->ged_result_str, "Exported materials to %s\n", fileName);
    return BRLCAD_OK;
}

// Import matprop file
// material import --type matprop  <filename>
static int
import_matprop_file(struct ged *gedp, int argc, const char *argv[])
{
    const char* fileName;
    FILE* file;
    ParseResult result;
    int import_count = 0;
    int i, j;

    if (argc < 3) {
        bu_vls_printf(gedp->ged_result_str, "ERROR: Not enough arguments.\nUsage: material import --type matprop <filename>\n");
        return BRLCAD_ERROR;
    }
    fileName = argv[2]; 

    file = fopen(fileName, "r");
    if (file == NULL) {
        bu_vls_printf(gedp->ged_result_str, "ERROR: Could not open file '%s'\n", fileName);
        return BRLCAD_ERROR;
    }

    // parser from parser.c
    result = parse_matprop(file);
    fclose(file);

    if (result.error_message) {
        bu_vls_printf(gedp->ged_result_str, "ERROR: Failed to parse '%s':\n%s\n", fileName, result.error_message);
        free_parse_result(&result); // MUST free memory even on error
        return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    
    for (i = 0; i < result.mat_count; i++) {
        Material* mat = &result.materials[i];
        
        struct bu_attribute_value_set physicalProperties;
        struct bu_attribute_value_set mechanicalProperties;
        struct bu_attribute_value_set opticalProperties;
        struct bu_attribute_value_set thermalProperties;

        bu_avs_init_empty(&physicalProperties);
        bu_avs_init_empty(&mechanicalProperties);
        bu_avs_init_empty(&opticalProperties);
        bu_avs_init_empty(&thermalProperties);

        const char* known_optical_keys[] = {
            "transparency",
            "reflectivity",
            "index_of_refraction",
            "shine",
            "emission"
        };
        const int known_optical_count = sizeof(known_optical_keys) / sizeof(known_optical_keys[0]);

        const char* known_mechanical_keys[] = {
            "youngs_modulus",
            "poissons_ratio",
            "hardness",
            "tensile_strength",
            "yield_strength",
            "density",
            "brinell_hardness",
            "ultimate_strength",
            "shear_strength",
            "bulk_modulus"
        };
        const int known_mechanical_count = sizeof(known_mechanical_keys) / sizeof(known_mechanical_keys[0]);

        const char* known_thermal_keys[] = {
            "thermal_conductivity",
            "specific_heat",
            "melting_point",
            "boiling_point",
            "thermal_expansion"
        };
        const int known_thermal_count = sizeof(known_thermal_keys) / sizeof(known_thermal_keys[0]);

        for (j = 0; j < mat->prop_count; j++) {
            Property* prop = &mat->properties[j];

            // Check if the property key is a known optical property
            int is_known_optical = 0;
            for (int k = 0; k < known_optical_count; k++) {
                if (BU_STR_EQUIV(prop->key, known_optical_keys[k])) {
                    is_known_optical = 1;
                    break;
                }
            }
            // Check if the property key is a known mechanical property
            int is_known_mechanical = 0;
            if (!is_known_optical) {
                for (int k = 0; k < known_mechanical_count; k++) {
                    if (BU_STR_EQUIV(prop->key, known_mechanical_keys[k])) {
                        is_known_mechanical = 1;
                        break;
                    }
                }
            }
        
            // Check if the property key is a known thermal property
            int is_known_thermal = 0;
            if (!is_known_optical && !is_known_mechanical) {
                for (int k = 0; k < known_thermal_count; k++) {
                    if (BU_STR_EQUIV(prop->key, known_thermal_keys[k])) {
                        is_known_thermal = 1;
                        break;
                    }
                }
            }

            // Assign to the correct AVS container
            if (is_known_optical) {
                bu_avs_add(&opticalProperties, prop->key, prop->value);
            } else if (is_known_mechanical) {
                bu_avs_add(&mechanicalProperties, prop->key, prop->value);
            } else if (is_known_thermal) {
                bu_avs_add(&thermalProperties, prop->key, prop->value);
            } else {
                bu_avs_add(&physicalProperties, prop->key, prop->value);
            }
        }

        int material_creation = mk_material(wdbp, 
            mat->name, 
            mat->name, 
            "", 
            "", 
            &physicalProperties, 
            &mechanicalProperties, 
            &opticalProperties, 
            &thermalProperties
        );
        
        bu_avs_free(&physicalProperties);
        bu_avs_free(&mechanicalProperties);
        bu_avs_free(&opticalProperties);
        bu_avs_free(&thermalProperties);

        if (material_creation == 1) {
            bu_vls_printf(gedp->ged_result_str, "ERROR: Could not create material object '%s'\n", mat->name);
            continue; // Skip and try next material
        }

        import_count++;
    }

    free_parse_result(&result);

    bu_vls_printf(gedp->ged_result_str, "Successfully imported %d new materials from '%s'.\n", import_count, fileName);

    return BRLCAD_OK;
}

static int
import_file_type(struct ged *gedp, int argc, const char *argv[])
{
    struct bu_vls file_type = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[2];
    BU_OPT(d[0],  "", "type",         "file_type",         &bu_opt_vls,       &file_type, "Import file based on file type");
    BU_OPT_NULL(d[1]);

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);
    argc = opt_ret;

    if (BU_STR_EQUAL(bu_vls_cstr(&file_type), "matprop")) {
        import_matprop_file(gedp, argc, argv);
    } else {
        import_materials(gedp, argc, argv);
    }
    return BRLCAD_OK;
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

    // Initialize AVS stores
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

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    mk_material(wdbp,
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

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int ret = wdb_put_internal(wdbp, argv[2], &intern, mk_conv2mm);
    return ret;
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

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int ret = wdb_put_internal(wdbp, argv[2], &intern, mk_conv2mm);
    return ret;
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
        return GED_HELP;
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
        import_file_type(gedp, argc, argv);
    } else if (scmd == MATERIAL_EXPORT){
        //export routine
        export_materials(gedp, argc, argv);
    }
    else if (scmd == MATERIAL_GET) {
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

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
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
