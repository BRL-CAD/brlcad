/*                         M A T E R I A L . C
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"
#include "wdb.h"

static const char *possibleProperties = "The following are properties of material objects that can be set/modified: \n"
    "- name\n"
    "- source\n"
    "- parent\n\n"
    "The following are property groups (utilizable in [propertyGroupName] for materials): \n"
    "- physical\n"
    "- mechanical\n"
    "- optical\n"
    "- thermal\n";


static int
assign_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct bu_attribute_value_set avs;

    if (argc != 3) {
        bu_vls_printf(gedp->ged_result_str, "assign requires an object and material name.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[1], 0)) != RT_DIR_NULL) {
        bu_avs_init_empty(&avs);

        if (db5_get_attributes(gedp->dbip, &avs, dp)) {
            bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
            return BRLCAD_ERROR;
        } else {
            bu_avs_add(&avs, "material_name", argv[2]);
            bu_avs_add(&avs, "material_id", "1");
        }

        if (db5_update_attributes(dp, &avs, gedp->dbip)) {
            bu_vls_printf(gedp->ged_result_str, "Error: failed to update attributes\n");
            return BRLCAD_ERROR;
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "Cannot get object %s\n", argv[1]);
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
    char buffer[BUFSIZ] = {0};

    if (argc != 3) {
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
	return BRLCAD_ERROR;
    }

    flag = argv[1];
    fileName = argv[2];

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
	    bu_vls_free(&name);
	    fclose(densityTable);
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

    fclose(densityTable);
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

    // Initialize AVS stores
    bu_avs_init_empty(&physicalProperties);
    bu_avs_init_empty(&mechanicalProperties);
    bu_avs_init_empty(&opticalProperties);
    bu_avs_init_empty(&thermalProperties);

    if (argc != 3) {
        bu_vls_printf(gedp->ged_result_str, "ERROR, not enough arguments!\n");
        return BRLCAD_ERROR;
    }

    db_name = argv[1];
    name = argv[2];
    parent = NULL;
    source = NULL;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int ret = mk_material(wdbp,
		db_name,
		name,
		parent,
		source,
		&physicalProperties,
		&mechanicalProperties,
		&opticalProperties,
		&thermalProperties);

    return ret;
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

    if (argc != 2) {
        bu_vls_printf(gedp->ged_result_str, "ERROR, incorrect number of arguments.");
        return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    _dl_eraseAllNamesFromDisplay(gedp, argv[1], 0);


    if ((dp = db_lookup(gedp->dbip,  argv[1], 0)) != RT_DIR_NULL) {
	if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
            bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[1]);
	    return BRLCAD_ERROR;
	}

        if (db_delete(gedp->dbip, dp) != 0 || db_dirdelete(gedp->dbip, dp) != 0) {
	    /* Abort kill processing on first error */
            bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[1]);
            return BRLCAD_ERROR;
	}
    }

    /* Update references. */
    db_update_nref(gedp->dbip);

    return BRLCAD_OK;
}


// routine handles getting individual properties of the material
static int
get_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;

    if (argc != 3 && argc != 4) {
        bu_vls_printf(gedp->ged_result_str, "get requires a material object and property.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[1], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[2], "name")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->name.vls_str);
        } else if (BU_STR_EQUAL(argv[2], "parent")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->parent.vls_str);
        } else if (BU_STR_EQUAL(argv[2], "source")) {
            bu_vls_printf(gedp->ged_result_str, "%s", material->source.vls_str);
        } else {
            if (argc == 3) {
                bu_vls_printf(gedp->ged_result_str, "the property you requested: %s, could not be found.", argv[2]);
                return BRLCAD_ERROR;
            } else if (BU_STR_EQUAL(argv[2], "physical")) {
                print_avs_value(gedp, &material->physicalProperties, argv[3], argv[2]);
            }  else if (BU_STR_EQUAL(argv[2], "mechanical")) {
                print_avs_value(gedp, &material->mechanicalProperties, argv[3], argv[2]);
            } else if (BU_STR_EQUAL(argv[2], "optical")) {
                print_avs_value(gedp, &material->opticalProperties, argv[3], argv[2]);
            } else if (BU_STR_EQUAL(argv[2], "thermal")) {
                print_avs_value(gedp, &material->thermalProperties, argv[3], argv[2]);
            } else {
                bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material property group:  %s", argv[2]);
                return BRLCAD_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[1]);
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

    if (argc != 4 && argc != 5) {
        bu_vls_printf(gedp->ged_result_str, "set requires a material object, property, and value.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[1], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[2], "name")) {
            BU_VLS_INIT(&material->name);
            bu_vls_strcpy(&material->name, argv[3]);
        } else if (BU_STR_EQUAL(argv[2], "parent")) {
            BU_VLS_INIT(&material->parent);
            bu_vls_strcpy(&material->parent, argv[3]);
        } else if (BU_STR_EQUAL(argv[2], "source")) {
            BU_VLS_INIT(&material->source);
            bu_vls_strcpy(&material->source, argv[3]);
        } else {
            if (BU_STR_EQUAL(argv[2], "physical")) {
                bu_avs_remove(&material->physicalProperties, argv[3]);
                bu_avs_add(&material->physicalProperties, argv[3], argv[4]);
            }  else if (BU_STR_EQUAL(argv[2], "mechanical")) {
                bu_avs_remove(&material->mechanicalProperties, argv[3]);
                bu_avs_add(&material->mechanicalProperties, argv[3], argv[4]);
            } else if (BU_STR_EQUAL(argv[2], "optical")) {
                bu_avs_remove(&material->opticalProperties, argv[3]);
                bu_avs_add(&material->opticalProperties, argv[3], argv[4]);
            } else if (BU_STR_EQUAL(argv[2], "thermal")) {
                bu_avs_remove(&material->thermalProperties, argv[3]);
                bu_avs_add(&material->thermalProperties, argv[3], argv[4]);
            } else {
                bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material property group:  %s", argv[2]);
                return BRLCAD_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[1]);
        return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int ret = wdb_put_internal(wdbp, argv[1], &intern, mk_conv2mm);
    return ret;
}


// Routine handles the removal of a material property
static int
remove_material(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;

    if (argc != 3 && argc != 4) {
        bu_vls_printf(gedp->ged_result_str, "remove requires a material object and property.");
        return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if ((dp = db_lookup(gedp->dbip,  argv[1], 0)) != RT_DIR_NULL) {
        GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);

        struct rt_material_internal *material = (struct rt_material_internal *)intern.idb_ptr;

        if (BU_STR_EQUAL(argv[2], "name")) {
            bu_vls_trunc(&material->name, 0);
        } else if (BU_STR_EQUAL(argv[2], "parent")) {
            bu_vls_trunc(&material->parent, 0);
        } else if (BU_STR_EQUAL(argv[2], "source")) {
            bu_vls_trunc(&material->source, 0);
        } else {
            if (BU_STR_EQUAL(argv[2], "physical")) {
                bu_avs_remove(&material->physicalProperties, argv[3]);
            }  else if (BU_STR_EQUAL(argv[2], "mechanical")) {
                bu_avs_remove(&material->mechanicalProperties, argv[3]);
            } else if (BU_STR_EQUAL(argv[2], "optical")) {
                bu_avs_remove(&material->opticalProperties, argv[3]);
            } else if (BU_STR_EQUAL(argv[2], "thermal")) {
                bu_avs_remove(&material->thermalProperties, argv[3]);
            } else {
                bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material property group:  %s", argv[2]);
                return BRLCAD_ERROR;
            }
        }
    } else {
        bu_vls_printf(gedp->ged_result_str, "an error occurred finding the material:  %s", argv[1]);
        return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int ret = wdb_put_internal(wdbp, argv[1], &intern, mk_conv2mm);
    return ret;
}


#include "../include/plugin.h"

/* A material property is either a direct named field or a key in one of the
 * four attribute-value property groups.  Keep the vocabulary in the schema:
 * it drives completion, highlighting, JSON, help, and execution validation. */
static const char * const material_property_or_group_values[] = {
    "name", "parent", "source", "physical", "mechanical", "optical", "thermal", NULL
};
static const char * const material_import_mode_options[] = {"id", "name", NULL};

static int
material_is_property_group(const char *property)
{
    return BU_STR_EQUAL(property, "physical") || BU_STR_EQUAL(property, "mechanical") ||
	BU_STR_EQUAL(property, "optical") || BU_STR_EQUAL(property, "thermal");
}


static int
material_is_direct_property(const char *property)
{
    return BU_STR_EQUAL(property, "name") || BU_STR_EQUAL(property, "parent") ||
	BU_STR_EQUAL(property, "source");
}


static void
material_property_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, const char *hint)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = BU_CMD_VALUE_STRING;
    result->hint = hint;
}


static int
material_property_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operand_count;
    size_t supplied_tail;
    size_t expected_tail;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    operand_count = bu_cmd_schema_operand_count(schema, argc, argv);
    if (operand_count < 2)
	return ret;

    supplied_tail = operand_count - 2;
    if (material_is_property_group(argv[1])) {
	expected_tail = BU_STR_EQUAL(schema->name, "set") ? 2 : 1;
    } else if (material_is_direct_property(argv[1])) {
	expected_tail = BU_STR_EQUAL(schema->name, "set") ? 1 : 0;
    } else {
	/* The keyword operand reports this case first, but retain a complete
	 * result if a future schema changes that operand to a semantic provider. */
	material_property_validation_result(result, BU_CMD_VALIDATE_INVALID,
	    cursor_arg < argc ? cursor_arg : argc,
	    "material property must be name, parent, source, or a property group");
	return 0;
    }
    if (supplied_tail == expected_tail)
	return ret;

    material_property_validation_result(result,
	supplied_tail < expected_tail ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID,
	cursor_arg < argc ? cursor_arg : argc,
	supplied_tail < expected_tail ? "additional material property argument required" :
	"too many material property arguments");
    return 0;
}


static const struct bu_cmd_operand material_create_operands[] = {
    BU_CMD_OPERAND("material_object", BU_CMD_VALUE_STRING, 1, 1,
	"New material database object name", NULL),
    BU_CMD_OPERAND("material_name", BU_CMD_VALUE_STRING, 1, 1,
	"Initial material name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand material_assign_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Geometry object to receive the material", NULL),
    BU_CMD_OPERAND("material_name", BU_CMD_VALUE_STRING, 1, 1,
	"Assigned material name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand material_object_operands[] = {
    BU_CMD_OPERAND("material_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Material database object", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand material_get_operands[] = {
    BU_CMD_OPERAND("material_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Material database object", NULL),
    BU_CMD_OPERAND_KEYWORDS("property_or_group", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Direct property or property group", NULL, material_property_or_group_values),
    BU_CMD_OPERAND("group_property", BU_CMD_VALUE_STRING, 0, 1,
	"Property key within a property group", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand material_set_operands[] = {
    BU_CMD_OPERAND("material_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Material database object", NULL),
    BU_CMD_OPERAND_KEYWORDS("property_or_group", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Direct property or property group", NULL, material_property_or_group_values),
    BU_CMD_OPERAND("property_or_value", BU_CMD_VALUE_STRING, 1, 1,
	"Direct-property value or property-group key", NULL),
    BU_CMD_OPERAND("group_value", BU_CMD_VALUE_STRING, 0, 1,
	"New value for a property-group key", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand material_import_operands[] = {
    BU_CMD_OPERAND("density_table", BU_CMD_VALUE_FILE, 1, 1,
	"Density table file", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_option material_import_options[] = {
    BU_CMD_FLAG_UNBOUND(NULL, "id", "id", "Name imported material objects by density-table ID"),
    BU_CMD_FLAG_UNBOUND(NULL, "name", "name", "Name imported material objects by density-table name"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_constraint material_import_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(material_import_mode_options, 1, 1,
	"exactly one of --id or --name is required"),
    BU_CMD_CONSTRAINT_NULL
};

#define MATERIAL_TREE_SCHEMA(_name, _help, _options, _operands, _validator) \
    static const struct bu_cmd_schema material_##_name##_schema = { \
	#_name, _help, _options, _operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, \
	BU_CMD_SCHEMA_CONSTRAINTS(_validator, NULL) \
    }
MATERIAL_TREE_SCHEMA(create, "Create a material object", NULL, material_create_operands, NULL);
MATERIAL_TREE_SCHEMA(assign, "Assign a material name to geometry", NULL, material_assign_operands, NULL);
MATERIAL_TREE_SCHEMA(destroy, "Destroy a material object", NULL, material_object_operands, NULL);
MATERIAL_TREE_SCHEMA(get, "Retrieve a material property", NULL, material_get_operands, material_property_schema_validate);
MATERIAL_TREE_SCHEMA(set, "Set a material property", NULL, material_set_operands, material_property_schema_validate);
MATERIAL_TREE_SCHEMA(remove, "Remove a material property", NULL, material_get_operands, material_property_schema_validate);
MATERIAL_TREE_SCHEMA(help, "Print material command help", NULL, NULL, NULL);
#undef MATERIAL_TREE_SCHEMA
static const struct bu_cmd_schema material_import_schema = {
    "import", "Import a density table", material_import_options, material_import_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, material_import_constraints)
};
static const struct bu_cmd_schema material_root_schema = {
    "material", "Manage material objects and assignments", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int material_tree_execute(void *data, int argc, const char *argv[]);
static const struct bu_cmd_tree_node material_subcommands[] = {
    BU_CMD_TREE_NODE(&material_assign_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE(&material_create_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE(&material_destroy_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE(&material_get_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE(&material_import_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE(&material_remove_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE(&material_set_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE(&material_help_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, material_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_material_tree = {
    &material_root_schema, material_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


static const struct bu_cmd_schema *
material_schema_for_command(const char *command)
{
    const struct bu_cmd_tree_node *node = bu_cmd_tree_find_subcommand(&ged_material_tree, command);
    return node ? node->schema : NULL;
}


static int
material_tree_parse(struct ged *gedp, const struct bu_cmd_schema *schema,
	int argc, const char *argv[])
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    if (bu_cmd_schema_parse_complete(schema, NULL, &msg, argc - 1, argv + 1) >= 0) {
	ret = BRLCAD_OK;
    } else if (bu_vls_strlen(&msg)) {
	bu_vls_vlscat(gedp->ged_result_str, &msg);
    } else {
	bu_vls_printf(gedp->ged_result_str, "Invalid material %s arguments.", schema->name);
    }
    bu_vls_free(&msg);
    return ret;
}


static void
material_tree_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&ged_material_tree);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "material native tree help");
    }
    bu_vls_printf(gedp->ged_result_str, "\n%s", possibleProperties);
}


static int
material_tree_execute(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    const struct bu_cmd_schema *schema = material_schema_for_command(argv[0]);

    if (!schema || material_tree_parse(gedp, schema, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (BU_STR_EQUAL(argv[0], "assign"))
	return assign_material(gedp, argc, argv);
    if (BU_STR_EQUAL(argv[0], "create"))
	return create_material(gedp, argc, argv);
    if (BU_STR_EQUAL(argv[0], "destroy"))
	return destroy_material(gedp, argc, argv);
    if (BU_STR_EQUAL(argv[0], "get"))
	return get_material(gedp, argc, argv);
    if (BU_STR_EQUAL(argv[0], "import"))
	return import_materials(gedp, argc, argv);
    if (BU_STR_EQUAL(argv[0], "remove"))
	return remove_material(gedp, argc, argv);
    if (BU_STR_EQUAL(argv[0], "set"))
	return set_material(gedp, argc, argv);
    if (BU_STR_EQUAL(argv[0], "help")) {
	material_tree_show_help(gedp);
	return GED_HELP;
    }
    return BRLCAD_ERROR;
}


static int
ged_material_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_ERROR;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc == 1) {
	material_tree_show_help(gedp);
	return GED_HELP;
    }
    if (bu_cmd_tree_dispatch(&ged_material_tree, gedp, argc - 1, argv + 1, &ret) == 0)
	return ret;

    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a valid material subcommand.\n", argv[1]);
    material_tree_show_help(gedp);
    return BRLCAD_ERROR;
}


static int
ged_material_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_material_tree, input, cursor_pos, result);
}


static int
ged_material_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_material_tree, input, analysis);
}


static char *
ged_material_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_material_tree);
}


static int
ged_material_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_material_tree, msgs);
}


static const struct ged_cmd_grammar ged_material_grammar = {
    "material", "Manage material objects and assignments", ged_material_grammar_validate,
    ged_material_grammar_analyze, ged_material_grammar_json, ged_material_grammar_lint
};

#define GED_MATERIAL_COMMANDS(X, XID) \
    X(material, ged_material_core, GED_CMD_DEFAULT, &ged_material_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_MATERIAL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_material", 1, GED_MATERIAL_COMMANDS)

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
