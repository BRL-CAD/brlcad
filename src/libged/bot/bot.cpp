/*                         B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file libged/bot/bot.cpp
 *
 * The LIBGED bot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/color.h"
#include "bu/tbl.h"
#include "bg/chull.h"
#include "bg/pca.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_bot.h"
#include "../ged_private.h"

// TODO - I think this may be the same for brep and bot, which suggests it should be
// a common libged utility function of some sort...
int
_bot_face_specifiers(std::set<int> &elements, struct bu_vls *vls, int argc,
	const char * const *argv) {
    for (int i = 0; i < argc; i++) {
	std::string s1(argv[i]);
	size_t pos_dash = s1.find_first_of("-:", 0);
	size_t pos_comma = s1.find_first_of(",/;", 0);
	if (pos_dash != std::string::npos) {
	    // May have a range - find out
	    std::string s2 = s1.substr(0, pos_dash);
	    s1.erase(0, pos_dash + 1);
	    int val1, val2, vtmp;
	    if (!bu_cmd_integer_from_str(&val1, s1.c_str())) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", s1.c_str());
		return BRLCAD_ERROR;
	    }
	    if (!bu_cmd_integer_from_str(&val2, s2.c_str())) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", s2.c_str());
		return BRLCAD_ERROR;
	    }
	    if (val1 > val2) {
		vtmp = val2;
		val2 = val1;
		val1 = vtmp;
	    }
	    for (int j = val1; j <= val2; j++) {
		elements.insert(j);
	    }
	    continue;
	}
	if (pos_comma != std::string::npos) {
	    // May have a set - find out
	while (pos_comma != std::string::npos) {
	    std::string ss = s1.substr(0, pos_comma);
	    int val1;
	    if (!bu_cmd_integer_from_str(&val1, ss.c_str())) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", ss.c_str());
		return BRLCAD_ERROR;
	    } else {
		elements.insert(val1);
	    }
		s1.erase(0, pos_comma + 1);
		pos_comma = s1.find_first_of(",/;", 0);
	}
	if (s1.length()) {
	    int val1;
	    if (!bu_cmd_integer_from_str(&val1, s1.c_str())) {
		bu_vls_printf(vls, "Invalid index specification: %s\n", s1.c_str());
		return BRLCAD_ERROR;
	    }
		elements.insert(val1);
	    }
	    continue;
	}

	// Nothing fancy looking - see if its a number
	int val = 0;
	if (bu_cmd_integer_from_str(&val, argv[i])) {
	    elements.insert(val);
	} else {
	    bu_vls_printf(vls, "Invalid index specification: %s\n", argv[i]);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}


int
_bot_obj_setup(struct _ged_bot_info *gb, const char *name)
{
    gb->dp = db_lookup(gb->gedp->dbip, name, LOOKUP_NOISY);
    if (gb->dp == RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, ": %s is not a solid or does not exist in database", name);
	return BRLCAD_ERROR;
    } else {
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gb->gedp->ged_result_str, ": %s is not a real solid", name);
	    return BRLCAD_ERROR;
	}
    }

    gb->solid_name = std::string(name);

    BU_GET(gb->intern, struct rt_db_internal);

    GED_DB_GET_INTERN(gb->gedp, gb->intern, gb->dp, bn_mat_identity, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(gb->intern);

    if (gb->intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", gb->solid_name.c_str());
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


int
_bot_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

struct bot_help_args {
    int print_help;
};

static int
bot_help_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}

static const char * const bot_get_properties[] = {
    "faces", "orientation", "type", "mode", "vertices", "minEdge",
    "maxEdge", "thickness", NULL
};

/* rt_bot_propget historically accepts an unambiguous leading fragment.  Keep
 * that compatibility in the native validator while completing only the
 * documented canonical property names. */
static int
bot_get_property_validate(struct bu_vls *msg, const char *property)
{
    size_t len;
    static const char * const accepted[] = {
	"faces", "orientation", "type", "mode", "vertices", "minEdge",
	"minedge", "maxEdge", "maxedge", "thickness", NULL
    };

    if (!property || !property[0])
	goto invalid;
    len = strlen(property);
    for (size_t i = 0; accepted[i]; i++) {
	if (bu_strncmp(property, accepted[i], len) == 0)
	    return 0;
    }
invalid:
    if (msg)
	bu_vls_printf(msg, "unknown BoT property: %s\n", property ? property : "");
    return -1;
}

static const struct bu_cmd_option bot_help_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_help_args, print_help, "Print command help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_get_operands[] = {
    {"property", 1, 1, "BoT property to query", BU_CMD_VALUE_STRING,
	bot_get_property_validate, NULL, bot_get_properties, NULL, NULL},
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_get_subcommand_schema = {
    "get", "Query BoT properties", bot_help_options, bot_get_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_get_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_get_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot get [--help] property bot\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot get help");
    }
}


extern "C" int
_bot_cmd_get(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot get <faces|minEdge|maxEdge|orientation|thickness|type|vertices> <objname>";
    const char *purpose_string = "Report specific information about a BoT shape";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct ged *gedp = gb->gedp;
    struct bot_help_args args = {0};
    int operand_index;
    const char **operands;
    const char *property;

    argc--; argv++;

    if (!argc) {
	bot_get_usage(gedp->ged_result_str);
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_get_subcommand_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_get_usage(gedp->ged_result_str);
	return GED_HELP;
    }
    operands = argv + operand_index;
    property = operands[0];

    if (_bot_obj_setup(gb, operands[1]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    /* Thickness is a special case, since we may need to report multiple values if we have a plate
     * mode bot with non-uniform thickness. */
    if (BU_STR_EQUAL(property, "thickness")) {
	if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	    bu_vls_printf(gedp->ged_result_str, "BoT is not plate mode - thicknesses can only be set on plate-mode BoTs");
	    return BRLCAD_ERROR;
	}

	// Check if our thickness is uniform across the bot.  If it is, return
	// one number.  Otherwise print each face's thickness.  (TODO - the
	// latter would probably be more readable if we printed ranges of face
	// indexes with the same thickness, but deferring doing that until we
	// have a use case that needs it.  (The which command has printing of
	// ranges, so it might be a useful reference.)
	fastf_t seed = bot->thickness[0];
	int uniform = 1;
	for (size_t i = 1; i < bot->num_faces; i++) {
	    if (!NEAR_EQUAL(seed, bot->thickness[i], 0.1*BN_TOL_DIST)) {
		uniform = 0;
		break;
	    }
	}

	if (uniform) {
	    bu_vls_printf(gedp->ged_result_str, "%g", seed);
	    return BRLCAD_OK;
	} else {
	    for (size_t i = 0; i < bot->num_faces; i++) {
		bu_vls_printf(gedp->ged_result_str, "%zd:%g\n", i, bot->thickness[i]);
	    }
	    return BRLCAD_OK;
	}
    }


    fastf_t propVal = rt_bot_propget(bot, property);

    /* print result string */
    if (!EQUAL(propVal, -1.0)) {
	int intprop = (int) propVal;
	fastf_t tmp = (int) propVal;

	if (EQUAL(propVal, tmp)) {
	    /* int result */
	    /* for orientation and mode, print something more informative than just
	     * the number */
	    if (BU_STR_EQUAL(property, "orientation")) {
		switch (intprop) {
		    case RT_BOT_UNORIENTED:
			bu_vls_printf(gedp->ged_result_str, "none");
			break;
		    case RT_BOT_CCW:
			bu_vls_printf(gedp->ged_result_str, "ccw");
			break;
		    case RT_BOT_CW:
			bu_vls_printf(gedp->ged_result_str, "cw");
			break;
		}
	    } else if (BU_STR_EQUAL(property, "type") || BU_STR_EQUAL(property, "mode")) {
		switch (intprop) {
		    case RT_BOT_SURFACE:
			bu_vls_printf(gedp->ged_result_str, "surface");
			break;
		    case RT_BOT_SOLID:
			bu_vls_printf(gedp->ged_result_str, "solid");
			break;
		    case RT_BOT_PLATE:
			bu_vls_printf(gedp->ged_result_str, "plate");
			break;
		    case RT_BOT_PLATE_NOCOS:
			bu_vls_printf(gedp->ged_result_str, "plate_nocos");
			break;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "%d", (int) propVal);
	    }
	} else {
	    /* float result */
	    bu_vls_printf(gedp->ged_result_str, "%f", propVal);
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s is not a valid argument!", property);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * "bot set" is a conditional positional grammar: the final value's
 * vocabulary and shape are selected by the first property word.  Keep that
 * relationship here, next to the executable schema, rather than teaching
 * callers a separate completion table.
 */
static const char * const bot_set_properties[] = {
    "orientation", "type", "thickness", NULL
};
static const char * const bot_set_orientation_values[] = {
    "none", "ccw", "cw", NULL
};
static const char * const bot_set_orientation_accepted[] = {
    "none", "unoriented", "no", "ccw", "counterclockwise", "rh",
    "cw", "clockwise", "lh", NULL
};
static const char * const bot_set_type_values[] = {
    "surface", "solid", "plate", "plate_nocos", NULL
};
static const char * const bot_set_type_accepted[] = {
    "surface", "surf", "solid", "sol", "plate", "plate_nocos", NULL
};


static int
bot_set_property_known(const char *property)
{
    return property && (BU_STR_EQUAL(property, "orientation") ||
	BU_STR_EQUAL(property, "type") || BU_STR_EQUAL(property, "mode") ||
	BU_STR_EQUAL(property, "thickness"));
}


static int
bot_set_keyword_match(const char * const *values, const char *value, int *exact)
{
    size_t value_len;

    if (exact)
	*exact = 0;
    if (!values || !value)
	return 0;
    value_len = strlen(value);
    for (size_t i = 0; values[i]; i++) {
	if (BU_STR_EQUIV(values[i], value)) {
	    if (exact)
		*exact = 1;
	    return 1;
	}
	if (value_len <= strlen(values[i]) &&
	    bu_strncasecmp(values[i], value, value_len) == 0)
	    return 1;
    }
    return 0;
}


static int
bot_set_thickness_valid(const char *value)
{
    const char *colon;
    fastf_t thickness = 0.0;

    if (!value || !value[0])
	return 0;
    colon = strchr(value, ':');
    if (!colon)
	return bu_cmd_number_from_str(&thickness, value);
    if (strchr(colon + 1, ':'))
	return 0;
    std::string face(value, (size_t)(colon - value));
    int face_index = 0;
    if (!bu_cmd_integer_from_str(&face_index, face.c_str()) || face_index < 0 ||
	!bu_cmd_number_from_str(&thickness, colon + 1))
	return 0;
    return 1;
}


static void
bot_set_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t value_type,
	const char *hint, const char *semantic_provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = value_type;
    result->hint = hint;
    result->semantic_provider = semantic_provider;
}


static void
bot_set_property_result(struct bu_cmd_validate_result *result, const char *value,
	size_t token)
{
    int exact = bot_set_property_known(value);
    size_t value_len = value ? strlen(value) : 0;
    int prefix = 0;

    for (size_t i = 0; bot_set_properties[i]; i++) {
	if (value_len <= strlen(bot_set_properties[i]) &&
	    BU_STR_EQUAL(value ? value : "", bot_set_properties[i])) {
	    exact = 1;
	    prefix = 1;
	    break;
	}
	if (value_len <= strlen(bot_set_properties[i]) && value &&
	    strncmp(bot_set_properties[i], value, value_len) == 0)
	    prefix = 1;
    }
    if (BU_STR_EQUAL(value ? value : "", "mode"))
	exact = 1;

    bot_set_validation_result(result, exact ? BU_CMD_VALIDATE_VALID :
	(prefix ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID), token,
	BU_CMD_VALUE_KEYWORD, "BoT property", NULL);
    bu_cmd_keyword_candidates(result, bot_set_properties, value ? value : "");
}


static void
bot_set_keyword_result(struct bu_cmd_validate_result *result,
	const char * const *canonical, const char * const *accepted,
	const char *value, size_t token, const char *hint)
{
    int exact = 0;
    int prefix = bot_set_keyword_match(accepted, value, &exact);

    bot_set_validation_result(result, exact ? BU_CMD_VALIDATE_VALID :
	(prefix ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID), token,
	BU_CMD_VALUE_KEYWORD, hint, NULL);
    bu_cmd_keyword_candidates(result, canonical, value ? value : "");
}


static void
bot_set_value_result(struct bu_cmd_validate_result *result, const char *property,
	const char *value, size_t token)
{
    if (BU_STR_EQUAL(property, "orientation")) {
	bot_set_keyword_result(result, bot_set_orientation_values,
	    bot_set_orientation_accepted, value, token,
	    "orientation must be none, ccw, or cw");
	return;
    }
    if (BU_STR_EQUAL(property, "type") || BU_STR_EQUAL(property, "mode")) {
	bot_set_keyword_result(result, bot_set_type_values, bot_set_type_accepted,
	    value, token, "type must be surface, solid, plate, or plate_nocos");
	return;
    }
    bot_set_validation_result(result, bot_set_thickness_valid(value) ?
	BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID, token, BU_CMD_VALUE_STRING,
	"thickness must be a finite number or face_index:number", "bot.thickness");
}


static int
bot_set_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    const char *operands[4] = {NULL, NULL, NULL, NULL};
    size_t count = 0;
    size_t i = 0;
    int option_phase = 1;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    if (bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result) != 0 ||
	result->state == BU_CMD_VALIDATE_INVALID)
	return 0;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help") ||
	(result->expected & BU_CMD_EXPECT_OPTION_ARG) ||
	(cursor_arg < argc && (result->expected & BU_CMD_EXPECT_OPTION)))
	return 0;

    while (i < cursor_arg) {
	int span = 0;
	if (option_phase && BU_STR_EQUAL(argv[i], "--")) {
	    option_phase = 0;
	    i++;
	    continue;
	}
	if (option_phase)
	    span = bu_cmd_schema_option_span(schema, argc - i, argv + i);
	if (span > 0) {
	    i += (size_t)span;
	    continue;
	}
	if (count < 4)
	    operands[count] = argv[i];
	count++;
	i++;
    }

    if (cursor_arg < argc) {
	const char *current = argv[cursor_arg];
	if (!count) {
	    bot_set_property_result(result, current, cursor_arg);
	    return 0;
	}
	if (!bot_set_property_known(operands[0])) {
	    bot_set_property_result(result, operands[0], 0);
	    return 0;
	}
	if (count == 1) {
	    bot_set_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
		BU_CMD_VALUE_DB_OBJECT, "BoT object", "ged.db_object");
	    return 0;
	}
	if (count == 2) {
	    bot_set_value_result(result, operands[0], current, cursor_arg);
	    return 0;
	}
	bot_set_validation_result(result, BU_CMD_VALIDATE_INVALID, cursor_arg,
	    BU_CMD_VALUE_STRING, "bot set accepts exactly three operands", NULL);
	return 0;
    }

    if (!count) {
	bot_set_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
	    BU_CMD_VALUE_KEYWORD, "BoT property required", NULL);
	bu_cmd_keyword_candidates(result, bot_set_properties, "");
	return 0;
    }
	if (!bot_set_property_known(operands[0])) {
	bot_set_property_result(result, operands[0], 0);
	return 0;
    }
	if (count == 1) {
	bot_set_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
	    BU_CMD_VALUE_DB_OBJECT, "BoT object required", "ged.db_object");
	return 0;
    }
	if (count == 2) {
	if (BU_STR_EQUAL(operands[0], "orientation")) {
	    bot_set_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
		BU_CMD_VALUE_KEYWORD, "orientation value required", NULL);
	    bu_cmd_keyword_candidates(result, bot_set_orientation_values, "");
	} else if (BU_STR_EQUAL(operands[0], "type") || BU_STR_EQUAL(operands[0], "mode")) {
	    bot_set_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
		BU_CMD_VALUE_KEYWORD, "type value required", NULL);
	    bu_cmd_keyword_candidates(result, bot_set_type_values, "");
	} else {
	    bot_set_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
		BU_CMD_VALUE_STRING, "thickness value required", "bot.thickness");
	}
	return 0;
    }
	if (count == 3) {
	bot_set_value_result(result, operands[0], operands[2], 2);
	return 0;
    }
	bot_set_validation_result(result, BU_CMD_VALIDATE_INVALID, cursor_arg,
	    BU_CMD_VALUE_STRING, "bot set accepts exactly three operands", NULL);
	return 0;
}


static const struct bu_cmd_operand bot_set_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("property", BU_CMD_VALUE_STRING, 1, 1,
	"BoT property (mode is a compatibility alias for type)", NULL,
	bot_set_properties),
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"BoT object", "ged.db_object"),
    BU_CMD_OPERAND("value", BU_CMD_VALUE_STRING, 1, 1,
	"Property value", "bot.set_value"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_set_subcommand_schema = {
    "set", "Set BoT properties", bot_help_options, bot_set_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_set_schema_validate, NULL)
};


static void
bot_set_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_set_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot set [--help] property bot value\n"
	"  orientation: none (no), ccw (rh), or cw (lh)\n"
	"  type: surface (surf), solid (sol), plate, or plate_nocos\n"
	"  thickness: number or face_index:number (plate-mode BoTs only)\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot set help");
    }
}


extern "C" int
_bot_cmd_set(void *bs, int argc, const char **argv)
{
    bool processed = false;
    const char *usage_string = "bot set <orientation|type> <objname> <val>\nbot set thickness <objname> <#|#:#>";
    const char *purpose_string = "Set BoT object properties";
    struct bot_help_args args = {0};
    int operand_index;
    const char **operands;
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct ged *gedp = gb->gedp;

    argc--; argv++;

    if (!argc) {
	bot_set_usage(gedp->ged_result_str);
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_set_subcommand_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_set_usage(gedp->ged_result_str);
	return GED_HELP;
    }
    operands = argv + operand_index;
    if (argc - operand_index != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    argv = operands;
    argc -= operand_index;

    if (_bot_obj_setup(gb, argv[1]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "orientation")) {

	processed = true;

	int mode = INT_MAX;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (BU_STR_EQUIV(argv[2], "none") || BU_STR_EQUIV(argv[2], "unoriented") || BU_STR_EQUIV(argv[2], "no")) {
	    mode = RT_BOT_UNORIENTED;
	}
	if (BU_STR_EQUIV(argv[2], "ccw") || BU_STR_EQUIV(argv[2], "counterclockwise") || BU_STR_EQUIV(argv[2], "rh")) {
	    mode = RT_BOT_CCW;
	}
	if (BU_STR_EQUIV(argv[2], "cw") || BU_STR_EQUIV(argv[2], "clockwise") || BU_STR_EQUIV(argv[2], "lh")) {
	    mode = RT_BOT_CW;
	}
	if (mode == INT_MAX) {
	    bu_vls_printf(gedp->ged_result_str, "Possible orientations are: none (no), ccw (rh), and cw (lh)");
	    return BRLCAD_ERROR;
	}
	bot->orientation = mode;
    }

    if (BU_STR_EQUAL(argv[0], "type") || BU_STR_EQUAL(argv[0], "mode")) {

	processed = true;

	int mode = INT_MAX;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (BU_STR_EQUIV(argv[2], "surface") || BU_STR_EQUIV(argv[2], "surf")) {
	    mode = RT_BOT_SURFACE;
	}
	if (BU_STR_EQUIV(argv[2], "solid") || BU_STR_EQUIV(argv[2], "sol")) {
	    mode = RT_BOT_SOLID;
	}
	if (BU_STR_EQUIV(argv[2], "plate")) {
	    mode = RT_BOT_PLATE;
	}
	if (BU_STR_EQUIV(argv[2], "plate_nocos")) {
	    mode = RT_BOT_PLATE_NOCOS;
	}
	if (mode == INT_MAX) {
	    bu_vls_printf(gedp->ged_result_str, "Possible types are: surface solid plate plate_nocos");
	    return BRLCAD_ERROR;
	}
	int old_mode = bot->mode;
	bot->mode = mode;
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    if (old_mode != RT_BOT_PLATE && old_mode != RT_BOT_PLATE_NOCOS) {
		/* need to create some thicknesses */
		bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
		bot->face_mode = bu_bitv_new(bot->num_faces);
	    }
	} else {
	    if (old_mode == RT_BOT_PLATE || old_mode == RT_BOT_PLATE_NOCOS) {
		/* free the per face memory */
		bu_free((char *)bot->thickness, "BOT thickness");
		bot->thickness = (fastf_t *)NULL;
		bu_free((char *)bot->face_mode, "BOT face_mode");
		bot->face_mode = (struct bu_bitv *)NULL;
	    }
	}
    }

    if (BU_STR_EQUAL(argv[0], "thickness")) {

	processed = true;

	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	    bu_vls_printf(gedp->ged_result_str, "BoT is not plate mode - thickness can only be set on plate-mode BoTs");
	    return BRLCAD_ERROR;
	}

	// See which argument type we have
	char *targ = bu_strdup(argv[2]);
	char *c = strchr(targ, ':');
	int face_id = 0;
	fastf_t thickness = 0;
	if (c) {
	    // We have a colon separated argument, which should be a face index and thickness value.
	    // Split the string accordingly
	    char *face_id_str = targ;
	    *c = '\0';
	    c++;
	    char *thickness_str = c;

	    if (!bu_cmd_integer_from_str(&face_id, face_id_str)) {
		bu_vls_printf(gedp->ged_result_str, "Invalid face index specification: %s\n", face_id_str);
		bu_free(targ, "targ");
		return BRLCAD_ERROR;
	    }
	    if (!bu_cmd_number_from_str(&thickness, thickness_str)) {
		bu_vls_printf(gedp->ged_result_str, "Invalid index specification: %s\n", thickness_str);
		bu_free(targ, "targ");
		return BRLCAD_ERROR;
	    }
	    if (face_id < 0 || (size_t)face_id >= bot->num_faces) {
		bu_vls_printf(gedp->ged_result_str, "Face index out of range: %d\n", face_id);
		bu_free(targ, "targ");
		return BRLCAD_ERROR;
	    }
	    bot->thickness[face_id] = thickness;
	} else {
	    if (!bu_cmd_number_from_str(&thickness, targ)) {
		bu_vls_printf(gedp->ged_result_str, "Invalid thickness specification: %s\n", targ);
		bu_free(targ, "targ");
		return BRLCAD_ERROR;
	    }
	    for (size_t i = 0; i < bot->num_faces; i++)
		bot->thickness[i] = thickness;
	}
	bu_free(targ, "targ");
    }

    // If we didn't have a subcommand that we knew how to handle, report an error
    if (!processed) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(gb->dp, gedp->dbip, gb->intern) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Failed to update BoT");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static const struct bu_cmd_operand bot_chull_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND("output_name", BU_CMD_VALUE_STRING, 0, 1,
	"Optional output BoT name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_chull_subcommand_schema = {
    "chull", "Generate a convex hull", bot_help_options, bot_chull_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_chull_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_chull_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot chull [--help] input_bot [output_name]\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot chull help");
    }
}


extern "C" int
_bot_cmd_chull(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] chull <objname> [output_bot]";
    const char *purpose_string = "Generate the BoT's convex hull and store it in an object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bot_help_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;

    if (!argc) {
	bot_chull_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_chull_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_chull_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) == BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    int retval = 0;
    int fc = 0;
    int vc = 0;
    point_t *vert_array;
    int *faces;
    unsigned char err = 0;

    retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)bot->vertices, (int)bot->num_vertices);

    if (retval != 3) {
	return BRLCAD_ERROR;
    }

    struct bu_vls out_name = BU_VLS_INIT_ZERO;
    if (operand_count > 1) {
	bu_vls_sprintf(&out_name, "%s", operands[1]);
    } else {
	bu_vls_sprintf(&out_name, "%s.hull", gb->dp->d_namep);
    }

    if (db_lookup(gb->gedp->dbip, bu_vls_cstr(&out_name), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", bu_vls_cstr(&out_name));
	bu_vls_free(&out_name);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gb->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    retval = mk_bot(wdbp, bu_vls_cstr(&out_name), RT_BOT_SOLID, RT_BOT_CCW, err, vc, fc, (fastf_t *)vert_array, faces, NULL, NULL);

    bu_vls_free(&out_name);
    bu_free(faces, "free faces");
    bu_free(vert_array, "free verts");

    if (retval) {
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


struct bot_flip_args {
    int test_flipped;
};

static const struct bu_cmd_option bot_flip_options[] = {
    BU_CMD_FLAG("t", "test", struct bot_flip_args, test_flipped,
	"Test whether the specified BoT is inside out"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_flip_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_flip_schema = {
    "flip", "Reverse BoT orientation", bot_flip_options, bot_flip_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

extern "C" int
_bot_cmd_flip(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot flip [-t] <objname>";
    const char *purpose_string = "Flip BoT triangle normal directions (turns the BoT \"inside out\")";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bot_flip_args args = {0};
    int operand_index;
    const char **operands;

    argc--; argv++;

	operand_index = bu_cmd_schema_parse_complete(&bot_flip_schema, &args,
	gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    if (args.test_flipped) {
	int ftest = rt_bot_inside_out(bot);
	if (ftest < 0)
	    return BRLCAD_ERROR;
	if (!ftest) {
	    bu_vls_printf(gb->gedp->ged_result_str, "OK");
	} else {
	    bu_vls_printf(gb->gedp->ged_result_str, "BoT is inside out");
	}
	return BRLCAD_OK;
    }

    rt_bot_flip(bot);

    if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to update BoT");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gb->gedp->ged_result_str, "BoT faces flipped");
    return BRLCAD_OK;
}

static const struct bu_cmd_operand bot_isect_operands[] = {
    BU_CMD_OPERAND("first_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"First BoT object", "ged.db_object"),
    BU_CMD_OPERAND("second_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Second BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_isect_subcommand_schema = {
    "isect", "Intersect BoT geometry", bot_help_options, bot_isect_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_isect_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_isect_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot isect [--help] first_bot second_bot\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot isect help");
    }
}

extern "C" int
_bot_cmd_isect(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] isect <objname> <objname2>";
    const char *purpose_string = "(TODO) Test if BoT <objname> intersects with BoT <objname2>";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bot_help_args args = {0};
    int operand_index;
    const char **operands;

    argc--; argv++;

    if (!argc) {
	bot_isect_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_isect_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_isect_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) == BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)gb->intern->idb_ptr;

    struct directory *bot_dp_2;
    struct rt_db_internal intern_2;
    GED_DB_LOOKUP(gb->gedp, bot_dp_2, operands[1], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gb->gedp, &intern_2, bot_dp_2, bn_mat_identity, BRLCAD_ERROR);
    if (intern_2.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern_2.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", operands[1]);
	rt_db_free_internal(&intern_2);
	return BRLCAD_ERROR;
    }
    struct rt_bot_internal *bot_2 = (struct rt_bot_internal *)intern_2.idb_ptr;

    int fc_1 = (int)bot->num_faces;
    int fc_2 = (int)bot_2->num_faces;
    int vc_1 = (int)bot->num_vertices;
    int vc_2 = (int)bot_2->num_vertices;
    point_t *verts_1 = (point_t *)bot->vertices;
    point_t *verts_2 = (point_t *)bot_2->vertices;
    int *faces_1 = bot->faces;
    int *faces_2 = bot_2->faces;

    (void)bg_trimesh_isect(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			   faces_1, fc_1, verts_1, vc_1, faces_2, fc_2, verts_2, vc_2);

    rt_db_free_internal(&intern_2);

    return BRLCAD_OK;
}

static const struct bu_cmd_operand bot_single_object_operands[] = {
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_sync_subcommand_schema = {
    "sync", "Synchronize BoT orientation", bot_help_options,
    bot_single_object_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};
static const struct bu_cmd_schema bot_split_subcommand_schema = {
    "split", "Split disconnected BoT components", bot_help_options,
    bot_single_object_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_single_object_usage(struct bu_vls *result, const struct bu_cmd_schema *schema,
	const char *command, const char *operand)
{
    char *option_help = bu_cmd_schema_describe(schema);

    bu_vls_sprintf(result, "Usage: %s [--help] %s\n", command, operand);
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot single-object help");
    }
}

extern "C" int
_bot_cmd_sync(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot sync <objname>";
    const char *purpose_string = "Synchronize connected BoT triangle orientations";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    struct bot_help_args args = {0};
    int operand_index;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bot_single_object_usage(gb->gedp->ged_result_str, &bot_sync_subcommand_schema,
	    "bot sync", "objname");
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_sync_subcommand_schema, &args,
	gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_single_object_usage(gb->gedp->ged_result_str, &bot_sync_subcommand_schema,
	    "bot sync", "objname");
	return GED_HELP;
    }
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    int flip_cnt = bg_trimesh_sync(bot->faces, bot->faces, bot->num_faces);
    if (flip_cnt < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to perform BoT sync");
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to update BoT");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gb->gedp->ged_result_str, "Performed %d face flipping operations", flip_cnt);
    return BRLCAD_OK;
}

static void
_bot_vlblock_plot(struct ged *gedp, struct bv_vlblock *vbp, const char *sname)
{
    struct bview *view = gedp->ged_gvp;
    if (gedp->new_cmd_forms) {
	struct bu_vls nroot = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&nroot, "bot::%s", sname);
	bv_vlblock_obj(vbp, view, bu_vls_cstr(&nroot));
	bu_vls_free(&nroot);
    } else {
	_ged_cvt_vlblock_to_solids(gedp, vbp, sname, 0);
    }
}

static const struct bu_cmd_operand bot_index_list_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND("indices", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Index or index-list/range specification", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_plot_subcommand_schema = {
    "plot", "Plot BoT diagnostic faces", bot_help_options, bot_index_list_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};
static const struct bu_cmd_schema bot_vertex_subcommand_schema = {
    "vertex", "Report BoT vertex coordinates", bot_help_options, bot_index_list_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_index_list_usage(struct bu_vls *result, const struct bu_cmd_schema *schema,
	const char *command, const char *tail)
{
    char *option_help = bu_cmd_schema_describe(schema);

    bu_vls_sprintf(result, "Usage: %s [--help] input_bot%s\n", command, tail);
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot index-list help");
    }
}

extern "C" int
_bot_cmd_plot(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot plot <objname> tri_index [tri_index ...]";
    const char *purpose_string = "Plot specified triangle face(s)";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bu_color *color = gb->color;
    struct bv_vlblock *vbp = gb->vbp;
    struct bu_list *vlfree = gb->vlfree;
    struct bot_help_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bot_index_list_usage(gb->gedp->ged_result_str, &bot_plot_subcommand_schema,
	    "bot plot", " [face_index ...]");
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_plot_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_index_list_usage(gb->gedp->ged_result_str, &bot_plot_subcommand_schema,
	    "bot plot", " [face_index ...]");
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    std::set<int> elements;
    if (_bot_face_specifiers(elements, gb->gedp->ged_result_str,
	operand_count - 1, operands + 1) != BRLCAD_OK) {
	return BRLCAD_ERROR;
    }

    unsigned char rgb[3] = {255, 255, 0};
    if (color)
	bu_color_to_rgb_chars(color, rgb);

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    struct bu_list *vhead = bv_vlblock_find(vbp, (int)rgb[0], (int)rgb[1], (int)rgb[2]);

    std::set<int>::iterator f_it;
    for (f_it = elements.begin(); f_it != elements.end(); ++f_it) {
	point_t v[3];
	for (int i = 0; i < 3; i++)
          VMOVE(v[i], &bot->vertices[bot->faces[*f_it*3+i]*3]);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vlfree, vhead, v[1], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[2], BV_VLIST_LINE_DRAW);
	BV_ADD_VLIST(vlfree, vhead, v[0], BV_VLIST_LINE_DRAW);
    }

    _bot_vlblock_plot(gb->gedp, vbp, "_bot_face_plot");

    return BRLCAD_OK;
}

// bg_fit_plane uses this underlying math, but will only return the center
// point and normal vector.  We want all the axis vectors for the coordinate
// system, so we do the SVD calculation here.
//
// Probably this could (and should) be a libbg function that isn't specific to
// BoTs, but doing it here for testing purposes.
static const struct bu_cmd_operand bot_pca_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND("output_name", BU_CMD_VALUE_STRING, 0, 1,
	"Optional transformed-copy name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_pca_subcommand_schema = {
    "pca", "Analyze BoT principal components", bot_help_options, bot_pca_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_pca_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_pca_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot pca [--help] input_bot [output_name]\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot pca help");
    }
}

extern "C" int
_bot_cmd_pca(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot pca <objname> [output_name]";
    const char *purpose_string = "Calculate Principle Component Analysis for BoT";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bot_help_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;

    if (!argc) {
	bot_pca_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_pca_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_pca_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    point_t *vp = (point_t *)bot->vertices;

    // Find the center point
    point_t center = VINIT_ZERO;
    vect_t xaxis, yaxis, zaxis;

    if (bg_pca(&center, &xaxis, &yaxis, &zaxis, bot->num_vertices, vp) != BRLCAD_OK)
	return BRLCAD_ERROR;

    bu_log("X axis:  %g %g %g\n", V3ARGS(xaxis));
    bu_log("Y axis:  %g %g %g\n", V3ARGS(yaxis));
    bu_log("Z axis:  %g %g %g\n", V3ARGS(zaxis));

    // Do what ETO prep does to set up a BRL-CAD rotation matrix
    mat_t R, T, RT;
    // Rotation
    MAT_IDN(R);
    VMOVE(&R[0], xaxis);
    VMOVE(&R[4], yaxis);
    VMOVE(&R[8], zaxis);
    // Translation
    MAT_IDN(T);
    MAT_DELTAS_VEC_NEG(T, center);
    // Combine
    bn_mat_mul(RT, R, T);
    bn_mat_print("Rotation & Translation matrix", RT);

    if (operand_count == 1)
	return BRLCAD_OK;

    // We have another arg - make a relocated copy of the input BoT.
    // First, validate there is no output name collision
    struct directory *dp = db_lookup(gb->gedp->dbip, operands[1], LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists in the database - not writing\n", operands[1]);
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *moved_bot = rt_bot_dup(bot);
    for (size_t i = 0; i < moved_bot->num_vertices; i++) {
	// Set up a vect_t from the BoT vertex array
	vect_t v1;
	VMOVE(v1, &moved_bot->vertices[i*3]);
	// Apply matrix to the vect_t
	vect_t v2;
	MAT4X3PNT(v2, RT, v1);
	// Put the new vertex position back in in the moved_bot vertex array
	VMOVE(&moved_bot->vertices[i*3], v2);
    }

    // Validate output name doesn't collide
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)moved_bot;

    dp = db_diradd(gb->gedp->dbip, operands[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Cannot add %s to directory\n", operands[1]);
	rt_bot_internal_free(moved_bot);
	BU_PUT(moved_bot, struct rt_bot_internal);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    if (rt_db_put_internal(dp, gb->gedp->dbip, &intern) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to write %s to database\n", operands[1]);
	rt_bot_internal_free(moved_bot);
	BU_PUT(moved_bot, struct rt_bot_internal);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_bot_cmd_split(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    const char *usage_string = "bot split <objname>";
    const char *purpose_string = "Split BoT into objects containing topologically connected triangle subsets";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    struct bot_help_args args = {0};
    int operand_index;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bot_single_object_usage(gb->gedp->ged_result_str, &bot_split_subcommand_schema,
	    "bot split", "objname");
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_split_subcommand_schema, &args,
	gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_single_object_usage(gb->gedp->ged_result_str, &bot_split_subcommand_schema,
	    "bot split", "objname");
	return GED_HELP;
    }
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    int **fsets = NULL;
    int *fset_cnts = NULL;

    int split_cnt = bg_trimesh_split(&fsets, &fset_cnts, bot->faces, bot->num_faces);
    if (split_cnt <= 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT split unsuccessful");
	ret = BRLCAD_ERROR;
	goto bot_split_done;
    }

    if (split_cnt == 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "BoT is fully connected topologically, not splitting");
	goto bot_split_done;
    }

    // Two or more triangle sets - time for new bots
    for (int i = 0; i < split_cnt; i++) {
	// Because these are independent objects, we don't want to just make lots of copies
	// of the full original vertex set.  Use bg_trimesh_3d_gc to boil down the data to
	// a minimal representation of this BoT subset
	struct rt_db_internal intern;
	struct directory *dp = RT_DIR_NULL;
	struct bu_vls bname = BU_VLS_INIT_ZERO;
	int *ofaces = NULL;
	point_t *opnts = NULL;
	int n_opnts = 0;
	int n_ofaces = bg_trimesh_3d_gc(&ofaces, &opnts, &n_opnts,
					(const int *)fsets[i], fset_cnts[i], (const point_t *)bot->vertices);
	if (n_ofaces < 0) {
	    ret = BRLCAD_ERROR;
	    goto bot_split_done;
	}
	struct rt_bot_internal *nbot;
	BU_ALLOC(nbot, struct rt_bot_internal);
	nbot->magic = RT_BOT_INTERNAL_MAGIC;
	nbot->mode = bot->mode;
	nbot->orientation = bot->orientation;
	nbot->thickness = NULL;
	nbot->face_mode = NULL;
	nbot->num_faces = n_ofaces;
	nbot->num_vertices = n_opnts;
	nbot->faces = ofaces;
	nbot->vertices = (fastf_t *)opnts;

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)nbot;

	// TODO - more robust name generation
	bu_vls_sprintf(&bname, "%s.%d", gb->dp->d_namep, i);
	dp = db_diradd(gb->gedp->dbip, bu_vls_cstr(&bname), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Cannot add %s to directory\n", bu_vls_cstr(&bname));
	    ret = BRLCAD_ERROR;
	    bu_vls_free(&bname);
	    goto bot_split_done;
	}

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write %s to database\n", bu_vls_cstr(&bname));
	    rt_db_free_internal(&intern);
	    ret = BRLCAD_ERROR;
	    bu_vls_free(&bname);
	    goto bot_split_done;
	}

	bu_vls_free(&bname);
    }

bot_split_done:
    if (fsets) {
	for (int i = 0; i < split_cnt; i++) {
	    if (fsets[i])
		bu_free(fsets[i], "free mesh array");
	}
	bu_free(fsets, "free mesh array container");
    }
    if (fset_cnts)
	bu_free(fset_cnts, "free cnts array");
    if (split_cnt > 1)
	bu_vls_printf(gb->gedp->ged_result_str, "Split into %d objects", split_cnt);
    return ret;
}

static const struct bu_cmd_operand bot_strip_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND("output_name", BU_CMD_VALUE_STRING, 1, 1,
	"Output BoT name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_strip_subcommand_schema = {
    "strip", "Remove degenerate-volume triangles from a BoT", bot_help_options,
    bot_strip_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_strip_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_strip_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot strip [--help] input_bot output_name\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot strip help");
    }
}

extern "C" int
_bot_cmd_strip(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    const char *usage_string = "bot strip <objname> <outputname>";
    const char *purpose_string = "Remove triangles forming degenerate volume from a mesh.";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bot_help_args args = {0};
    int operand_index;
    const char **operands;

    argc--; argv++;
    if (!argc) {
	bot_strip_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_strip_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_strip_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operands = argv + operand_index;

    if (db_lookup(gb->gedp->dbip, operands[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", operands[1]);
	return BRLCAD_ERROR;
    }

    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    struct rt_i *rtip = rt_i_create(gb->gedp->dbip);
    rt_gettree(rtip, operands[0]);
    rt_prep(rtip);
    struct bu_ptbl tfaces = BU_PTBL_INIT_ZERO;
    int have_thin_faces = rt_bot_thin_check(&tfaces, bot, rtip, VUNITIZE_TOL, gb->verbosity);
    rt_i_destroy(rtip);
    if (have_thin_faces) {
	struct rt_bot_internal *nbot = rt_bot_remove_faces(&tfaces, bot);
	struct rt_db_internal intern;
	struct directory *dp = RT_DIR_NULL;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *)nbot;
	dp = db_diradd(gb->gedp->dbip, operands[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Cannot add %s to directory\n", operands[1]);
	    rt_db_free_internal(&intern);
	    ret = BRLCAD_ERROR;
	    goto bot_strip_done;
	}

	if (rt_db_put_internal(dp, gb->gedp->dbip, &intern) < 0) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Failed to write %s to database\n", operands[1]);
	    rt_db_free_internal(&intern);
	    ret = BRLCAD_ERROR;
	    goto bot_strip_done;
	}
    }

bot_strip_done:

    return ret;
}

static void
bot_output(struct bu_tbl *table, struct db_i *dbip, struct directory *dp)
{
    if (!table)
	return;
    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
	return;
    struct rt_db_internal intern;
    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
    RT_DB_INTERNAL_INIT(&intern);
    if (db_get_external(&ext, dp, dbip) < 0)
	return;
    if (rt_db_external5_to_internal5(&intern, &ext, dp->d_namep, dbip, NULL) < 0) {
	bu_free_external(&ext);
	return;
    }
    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_free_external(&ext);
	return;
    }
    struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;

    struct bu_vls str = BU_VLS_INIT_ZERO;

    // Object Path
    //db_path_to_vls(&str, fp);
    bu_tbl_write(table, dp->d_namep);

    // Disk Size
    bu_vls_sprintf(&str, "%zd", dp->d_len);
    bu_tbl_write(table, bu_vls_cstr(&str));

    // Number of vertices
    bu_vls_sprintf(&str, "%zd", bot->num_vertices);
    bu_tbl_write(table, bu_vls_cstr(&str));

    // Number of faces
    bu_vls_sprintf(&str, "%zd", bot->num_faces);
    bu_tbl_write(table, bu_vls_cstr(&str));

    // Number of face normals
    bu_vls_sprintf(&str, "%zd", bot->num_face_normals);
    bu_tbl_write(table, bu_vls_cstr(&str));

    // Number of unit surface normals
    bu_vls_sprintf(&str, "%zd", bot->num_normals);
    bu_tbl_write(table, bu_vls_cstr(&str));

    // Orientation
    switch (bot->orientation) {
	case RT_BOT_CW:
	    bu_vls_sprintf(&str, "CW");
	    break;
	case RT_BOT_CCW:
	    bu_vls_sprintf(&str, "CCW");
	    break;
	default:
	    bu_vls_sprintf(&str, "NONE");
    }
    bu_tbl_write(table, bu_vls_cstr(&str));

    // Mode
    switch (bot->mode) {
	case RT_BOT_SOLID:
	    bu_vls_sprintf(&str, "SOLID");
	    break;
	case RT_BOT_SURFACE:
	    bu_vls_sprintf(&str, "SURFACE");
	    break;
	case RT_BOT_PLATE:
	    bu_vls_sprintf(&str, "PLATE");
	    break;
	case RT_BOT_PLATE_NOCOS:
	    bu_vls_sprintf(&str, "PLATE_NOCOS");
	    break;
	default:
	    bu_vls_trunc(&str, 0);
    }
    bu_tbl_write(table, bu_vls_cstr(&str));

    // UV Vert Cnt
    bu_vls_sprintf(&str, "%zd", bot->num_uvs);
    bu_tbl_write(table, bu_vls_cstr(&str));

    // UV Face Cnt
    bu_vls_sprintf(&str, "%zd", bot->num_face_uvs);
    bu_tbl_write(table, bu_vls_cstr(&str));

    // Attribute size
    struct db5_raw_internal raw;
    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) != NULL) {
	bu_vls_sprintf(&str, "%zd", raw.attributes.ext_nbytes);
    } else {
	bu_vls_trunc(&str, 0);
    }
    bu_tbl_write(table, bu_vls_cstr(&str));

    bu_tbl_style(table, BU_TBL_ROW_END);

    // Have what we need - clean up
    bu_free_external(&ext);
}

static const struct bu_cmd_operand bot_stat_operands[] = {
    BU_CMD_OPERAND("pattern", BU_CMD_VALUE_STRING, 1, 1,
	"Database-object search pattern", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_stat_subcommand_schema = {
    "stat", "Report BoT statistics", bot_help_options, bot_stat_operands,
    BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_help_schema_validate, NULL)
};

static void
bot_stat_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_stat_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot stat [--help] pattern\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot stat help");
    }
}

extern "C" int
_bot_cmd_stat(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_OK;
    const char *usage_string = "bot stat <pattern>";
    const char *purpose_string = "Report information about bot object(s)";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct bot_help_args args = {0};
    int operand_index;
    const char **operands;

    argc--; argv++;

    if (!argc) {
	bot_stat_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_stat_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_stat_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operands = argv + operand_index;

    // Collect full path objects from search pattern
    struct directory **paths;
    int path_cnt = db_ls(gb->gedp->dbip, DB_LS_HIDDEN, operands[0], &paths);

    // Set up table
    struct bu_tbl *table = bu_tbl_create();
    bu_tbl_style(table, BU_TBL_STYLE_LIST);

    bu_tbl_write(table, "Object Path");
    bu_tbl_write(table, "Disk Size");
    bu_tbl_write(table, "Verts");
    bu_tbl_write(table, "Faces");
    bu_tbl_write(table, "Face Normals");
    bu_tbl_write(table, "Surf Normals");
    bu_tbl_write(table, "Orientation");
    bu_tbl_write(table, "Mode");
    bu_tbl_write(table, "UV Vert Cnt");
    bu_tbl_write(table, "UV Face Cnt");
    bu_tbl_write(table, "Attr Size");
    bu_tbl_style(table, BU_TBL_ROW_END);
    bu_tbl_style(table, BU_TBL_ROW_SEPARATOR);

    for (int i = 0; i < path_cnt; i++) {
	bot_output(table, gb->gedp->dbip, paths[i]);
    }

    struct bu_vls tstr = BU_VLS_INIT_ZERO;
    bu_tbl_vls(&tstr, table);
    bu_vls_printf(gb->gedp->ged_result_str, "%s\n", bu_vls_cstr(&tstr));
    bu_vls_free(&tstr);
    bu_tbl_destroy(table);
    bu_free(paths, "paths");
    return ret;
}

static const char * const bot_pick_kinds[] = {"E", "F", "V", NULL};
static const char * const bot_pick_first_values[] = {"--first", NULL};


static void
bot_pick_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t value_type,
	const char *hint, const char *semantic_provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = value_type;
    result->hint = hint;
    result->semantic_provider = semantic_provider;
}


static void
bot_pick_kind_result(struct bu_cmd_validate_result *result, const char *kind,
	size_t token)
{
    int exact = kind && (BU_STR_EQUAL(kind, "E") || BU_STR_EQUAL(kind, "F") ||
	BU_STR_EQUAL(kind, "V"));
    int prefix = kind && (BU_STR_EQUAL(kind, "E") || BU_STR_EQUAL(kind, "F") ||
	BU_STR_EQUAL(kind, "V") || BU_STR_EQUAL(kind, ""));

    bot_pick_validation_result(result, exact ? BU_CMD_VALIDATE_VALID :
	(prefix ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID), token,
	BU_CMD_VALUE_KEYWORD, "edge (E), face (F), or vertex (V) pick", NULL);
    bu_cmd_keyword_candidates(result, bot_pick_kinds, kind ? kind : "");
}


static void
bot_pick_ray_result(struct bu_cmd_validate_result *result,
	const std::vector<const char *> &tail, const char *kind, size_t token)
{
    size_t start = 0;
    size_t count;

    if (!tail.empty() && BU_STR_EQUAL(tail[0], "--first")) {
	if (BU_STR_EQUAL(kind, "F")) {
	    bot_pick_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
		BU_CMD_VALUE_KEYWORD, "--first is valid only for E and V picks", NULL);
	    return;
	}
	start = 1;
    }
    count = tail.size() - start;
    if (!count) {
	bot_pick_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_VALUE_NUMBER, "optional six-coordinate ray", "bot.ray");
	return;
    }
    if (count != 6) {
	bot_pick_validation_result(result, count < 6 ? BU_CMD_VALIDATE_INCOMPLETE :
	    BU_CMD_VALIDATE_INVALID, token, BU_CMD_VALUE_NUMBER,
	    "ray requires exactly six finite coordinates", "bot.ray");
	return;
    }
    for (size_t i = start; i < tail.size(); i++) {
	fastf_t coordinate = 0.0;
	if (!bu_cmd_number_from_str(&coordinate, tail[i])) {
	    bot_pick_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
		BU_CMD_VALUE_NUMBER, "ray requires finite coordinates", "bot.ray");
	    return;
	}
    }
    bot_pick_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	BU_CMD_VALUE_NUMBER, "six-coordinate ray", "bot.ray");
}


static int
bot_pick_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    std::vector<const char *> operands;
    size_t i = 0;
    int option_phase = 1;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    if (bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result) != 0 ||
	result->state == BU_CMD_VALIDATE_INVALID)
	return 0;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help") ||
	(result->expected & BU_CMD_EXPECT_OPTION_ARG) ||
	(cursor_arg < argc && (result->expected & BU_CMD_EXPECT_OPTION)))
	return 0;

    while (i < cursor_arg) {
	int span = 0;
	if (option_phase && BU_STR_EQUAL(argv[i], "--")) {
	    option_phase = 0;
	    i++;
	    continue;
	}
	if (option_phase)
	    span = bu_cmd_schema_option_span(schema, argc - i, argv + i);
	if (span > 0) {
	    i += (size_t)span;
	    continue;
	}
	operands.push_back(argv[i]);
	/* The schema deliberately stops option recognition at the BoT object;
	 * --first is then a typed child-language word, not a root option. */
	option_phase = 0;
	i++;
    }

    if (cursor_arg < argc) {
	const char *current = argv[cursor_arg];
	if (operands.empty()) {
	    bot_pick_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
		BU_CMD_VALUE_DB_OBJECT, "BoT object", "ged.db_object");
	    return 0;
	}
	if (operands.size() == 1) {
	    bot_pick_kind_result(result, current, cursor_arg);
	    return 0;
	}
	if (!BU_STR_EQUAL(operands[1], "E") && !BU_STR_EQUAL(operands[1], "F") &&
	    !BU_STR_EQUAL(operands[1], "V")) {
	    bot_pick_kind_result(result, operands[1], 1);
	    return 0;
	}
	std::vector<const char *> tail(operands.begin() + 2, operands.end());
	if (tail.empty() && !BU_STR_EQUAL(operands[1], "F") && current &&
	    strlen(current) <= strlen("--first") &&
	    strncmp("--first", current, strlen(current)) == 0) {
	    int exact = BU_STR_EQUAL(current, "--first");
	    bot_pick_validation_result(result, exact ? BU_CMD_VALIDATE_VALID :
		BU_CMD_VALIDATE_INCOMPLETE, cursor_arg, BU_CMD_VALUE_KEYWORD,
		"optional first-along-ray selector", NULL);
	    bu_cmd_keyword_candidates(result, bot_pick_first_values, current ? current : "");
	    return 0;
	}
	tail.push_back(current);
	bot_pick_ray_result(result, tail, operands[1], cursor_arg);
	return 0;
    }

    if (operands.empty()) {
	bot_pick_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
	    BU_CMD_VALUE_DB_OBJECT, "BoT object required", "ged.db_object");
	return 0;
    }
	if (operands.size() == 1) {
	bot_pick_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
	    BU_CMD_VALUE_KEYWORD, "pick kind required", NULL);
	bu_cmd_keyword_candidates(result, bot_pick_kinds, "");
	return 0;
    }
	if (!BU_STR_EQUAL(operands[1], "E") && !BU_STR_EQUAL(operands[1], "F") &&
	    !BU_STR_EQUAL(operands[1], "V")) {
	bot_pick_kind_result(result, operands[1], 1);
	return 0;
    }
	std::vector<const char *> tail(operands.begin() + 2, operands.end());
	if (tail.empty() && !BU_STR_EQUAL(operands[1], "F")) {
	bot_pick_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    BU_CMD_VALUE_KEYWORD, "optional first-along-ray selector or ray", NULL);
	bu_cmd_keyword_candidates(result, bot_pick_first_values, "");
	return 0;
    }
	bot_pick_ray_result(result, tail, operands[1], cursor_arg);
	return 0;
}


static const struct bu_cmd_operand bot_pick_operands[] = {
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"BoT object", "ged.db_object"),
    BU_CMD_OPERAND_KEYWORDS("kind", BU_CMD_VALUE_STRING, 1, 1,
	"Edge (E), face (F), or vertex (V) pick", NULL, bot_pick_kinds),
    BU_CMD_OPERAND("ray_argument", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Optional --first and six-coordinate ray", "bot.ray"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_pick_subcommand_schema = {
    "pick", "Pick BoT elements", bot_help_options, bot_pick_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_pick_schema_validate, NULL)
};


static void
bot_pick_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_pick_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot pick [--help] bot {E|F|V} [--first] [px py pz dx dy dz]\n"
	"F does not accept --first.  Omit the six coordinates to use the current\n"
	"viewport ray.\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot pick help");
    }
}


extern "C" int
_bot_cmd_pick(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot pick <objname> F|V|E [--first] [px py pz dx dy dz]";
    const char *purpose_string = "graphically identify components of the BoT object";
    struct bot_help_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (!argc) {
	bot_pick_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&bot_pick_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_pick_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;
    if (operand_count < 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    return bot_pick(gb, operand_count - 1, operands + 1);
}


extern "C" int
_bot_cmd_vertex(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> vertex [idx ...]";
    const char *purpose_string = "translate vertex indices to 3D point coordinates";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct ged *gedp = gb->gedp;
    struct bot_help_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;

    argc--; argv++;

    if (!argc) {
	bot_index_list_usage(gedp->ged_result_str, &bot_vertex_subcommand_schema,
	    "bot vertex", " [vertex_index ...]");
	return GED_HELP;
    }
    operand_index = bu_cmd_schema_parse_complete(&bot_vertex_subcommand_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_index_list_usage(gedp->ged_result_str, &bot_vertex_subcommand_schema,
	    "bot vertex", " [vertex_index ...]");
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;

    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    if (operand_count == 1) {
	/* No indices given - list all vertices */
	for (size_t i = 0; i < bot->num_vertices; i++) {
	    bu_vls_printf(gedp->ged_result_str, "%zu: %g %g %g\n", i,
			 bot->vertices[3*i+0],
			 bot->vertices[3*i+1],
			 bot->vertices[3*i+2]);
	}
	return BRLCAD_OK;
    }

    /* Print coordinates for the specified vertex indices */
    for (int i = 1; i < operand_count; i++) {
	int idx = 0;
	if (!bu_cmd_integer_from_str(&idx, operands[i])
	    || idx < 0
	    || (size_t)idx >= bot->num_vertices)
	{
	    bu_vls_printf(gedp->ged_result_str,
			 "invalid vertex index: %s (valid range 0..%zu)\n",
		 operands[i], bot->num_vertices - 1);
	    return BRLCAD_ERROR;
	}
	bu_vls_printf(gedp->ged_result_str, "%d: %g %g %g\n", idx,
		      bot->vertices[3*idx+0],
		      bot->vertices[3*idx+1],
		      bot->vertices[3*idx+2]);
    }

    return BRLCAD_OK;
}


static const char * const bot_info_kinds[] = {"F", "V", NULL};


static void
bot_info_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t value_type,
	const char *hint, const char *semantic_provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = value_type;
    result->hint = hint;
    result->semantic_provider = semantic_provider;
}


static void
bot_info_kind_result(struct bu_cmd_validate_result *result, const char *kind,
	size_t token)
{
    int exact = kind && (BU_STR_EQUAL(kind, "F") || BU_STR_EQUAL(kind, "V"));
    int prefix = kind && (BU_STR_EQUAL(kind, "F") || BU_STR_EQUAL(kind, "V") ||
	BU_STR_EQUAL(kind, ""));

    bot_info_validation_result(result, exact ? BU_CMD_VALIDATE_VALID :
	(prefix ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID), token,
	BU_CMD_VALUE_KEYWORD, "face (F) or vertex (V) information", NULL);
    bu_cmd_keyword_candidates(result, bot_info_kinds, kind ? kind : "");
}


static int
bot_info_indices_valid(const std::vector<const char *> &indices)
{
    std::set<int> elements;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = _bot_face_specifiers(elements, &msg, (int)indices.size(),
	indices.empty() ? NULL : indices.data());

    bu_vls_free(&msg);
    return ret == BRLCAD_OK;
}


static int
bot_info_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    std::vector<const char *> operands;
    size_t i = 0;
    int option_phase = 1;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    if (bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result) != 0 ||
	result->state == BU_CMD_VALIDATE_INVALID)
	return 0;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help") ||
	(result->expected & BU_CMD_EXPECT_OPTION_ARG) ||
	(cursor_arg < argc && (result->expected & BU_CMD_EXPECT_OPTION)))
	return 0;

    while (i < cursor_arg) {
	int span = 0;
	if (option_phase && BU_STR_EQUAL(argv[i], "--")) {
	    option_phase = 0;
	    i++;
	    continue;
	}
	if (option_phase)
	    span = bu_cmd_schema_option_span(schema, argc - i, argv + i);
	if (span > 0) {
	    i += (size_t)span;
	    continue;
	}
	operands.push_back(argv[i]);
	i++;
    }

    if (cursor_arg < argc) {
	const char *current = argv[cursor_arg];
	if (operands.empty()) {
	    bot_info_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
		BU_CMD_VALUE_DB_OBJECT, "BoT object", "ged.db_object");
	    return 0;
	}
	if (operands.size() == 1) {
	    bot_info_kind_result(result, current, cursor_arg);
	    return 0;
	}
	if (!BU_STR_EQUAL(operands[1], "F") && !BU_STR_EQUAL(operands[1], "V")) {
	    bot_info_kind_result(result, operands[1], 1);
	    return 0;
	}
	std::vector<const char *> indices(operands.begin() + 2, operands.end());
	indices.push_back(current);
	bot_info_validation_result(result, bot_info_indices_valid(indices) ?
	    BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID, cursor_arg,
	    BU_CMD_VALUE_RAW, "integer index, range, or index list", "bot.index_list");
	return 0;
    }

    if (operands.empty()) {
	bot_info_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
	    BU_CMD_VALUE_DB_OBJECT, "BoT object required", "ged.db_object");
	return 0;
    }
	if (operands.size() == 1) {
	bot_info_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    BU_CMD_VALUE_KEYWORD, "optional face (F) or vertex (V) information", NULL);
	bu_cmd_keyword_candidates(result, bot_info_kinds, "");
	return 0;
    }
	if (!BU_STR_EQUAL(operands[1], "F") && !BU_STR_EQUAL(operands[1], "V")) {
	bot_info_kind_result(result, operands[1], 1);
	return 0;
    }
	if (operands.size() == 2) {
	bot_info_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	    BU_CMD_VALUE_RAW, "optional integer index, range, or index list",
	    "bot.index_list");
	return 0;
    }
	std::vector<const char *> indices(operands.begin() + 2, operands.end());
	bot_info_validation_result(result, bot_info_indices_valid(indices) ?
	    BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID, cursor_arg,
	    BU_CMD_VALUE_RAW, "integer index, range, or index list", "bot.index_list");
	return 0;
}


static const struct bu_cmd_operand bot_info_operands[] = {
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"BoT object", "ged.db_object"),
    BU_CMD_OPERAND_KEYWORDS("kind", BU_CMD_VALUE_STRING, 0, 1,
	"Optional face (F) or vertex (V) information", NULL, bot_info_kinds),
    BU_CMD_OPERAND("index", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Optional integer index, range, or index list", "bot.index_list"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema bot_info_subcommand_schema = {
    "info", "Report BoT vertex and face information", bot_help_options,
    bot_info_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_info_schema_validate, NULL)
};


static void
bot_info_usage(struct bu_vls *result)
{
    char *option_help = bu_cmd_schema_describe(&bot_info_subcommand_schema);

    bu_vls_sprintf(result, "Usage: bot info [--help] bot [F|V] [index_or_range ...]\n"
	"With no F or V selector, report BoT summary counts.  Indexes may be\n"
	"integers, ranges (first-last or first:last), or separated lists.\n");
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "bot info help");
    }
}


extern "C" int
_bot_cmd_info(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot info <objname> [V|F] [index ...]";
    const char *purpose_string = "report detailed information about BoT vertices and faces";
    struct bot_help_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands;
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (!argc) {
	bot_info_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&bot_info_subcommand_schema,
	&args, gb->gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_info_usage(gb->gedp->ged_result_str);
	return GED_HELP;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;
    if (operand_count < 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return BRLCAD_ERROR;
    }
    if (_bot_obj_setup(gb, operands[0]) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    return bot_info(gb, operand_count - 1, operands + 1);
}


const struct bu_cmdtab _bot_cmds[] = {
    { "check",      _bot_cmd_check},
    { "chull",      _bot_cmd_chull},
    { "decimate",   _bot_cmd_decimate},
    { "dump",       _bot_cmd_dump},
    { "exterior",   _bot_cmd_exterior},
    { "extrude",    _bot_cmd_extrude},
    { "flip",       _bot_cmd_flip},
    { "get",        _bot_cmd_get},
    { "info",       _bot_cmd_info},
    { "isect",      _bot_cmd_isect},
    { "pca",        _bot_cmd_pca},
    { "pick",       _bot_cmd_pick},
    { "plot",       _bot_cmd_plot},
    { "remesh",     _bot_cmd_remesh},
    { "repair",     _bot_cmd_repair},
    { "set",        _bot_cmd_set},
    { "smooth",     _bot_cmd_smooth},
    { "split",      _bot_cmd_split},
    { "stat",       _bot_cmd_stat},
    { "strip",      _bot_cmd_strip},
    { "subd",       _bot_cmd_subd},
    { "sync",       _bot_cmd_sync},
    { "vertex",     _bot_cmd_vertex},
    { (char *)NULL,      NULL}
};


struct bot_root_args {
    int print_help;
    long verbosity;
    int visualize;
    struct bu_color color;
};

static const struct bu_cmd_option bot_root_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_root_args, print_help,
	"Print command help"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", struct bot_root_args, verbosity,
	"Increase output detail"),
    BU_CMD_FLAG("V", "visualize", struct bot_root_args, visualize,
	"Visualize results when supported"),
    BU_CMD_COLOR_COMPAT("C", "color", struct bot_root_args, color, "r/g/b",
	"Plotting color"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema bot_root_schema = {
    "bot", "Inspect and edit BoT objects", bot_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int bot_tree_execute(void *data, int argc, const char *argv[]);
static const struct bu_cmd_tree_node bot_subcommands[] = {
    BU_CMD_TREE_NODE(&ged_bot_check_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_chull_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_decimate_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_dump_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_exterior_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_extrude_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_flip_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_get_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_info_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_isect_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_pca_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_pick_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_plot_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_remesh_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_repair_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_set_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_smooth_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_split_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_stat_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_strip_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&ged_bot_subd_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_sync_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE(&bot_vertex_subcommand_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, bot_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_bot_tree = {
    &bot_root_schema, bot_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static void
bot_tree_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&ged_bot_tree);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "bot native tree help");
    }
}

static int
bot_tree_execute(void *data, int argc, const char *argv[])
{
    struct _ged_bot_info *gb = (struct _ged_bot_info *)data;
    int child_result = BRLCAD_ERROR;

    if (!gb || bu_cmd(_bot_cmds, argc, argv, 0, gb, &child_result) != BRLCAD_OK)
	return BRLCAD_ERROR;

    /* Preserve the historical parent-command contract: a recognized child
     * owns its diagnostic/result text, while `bot` itself reports success. */
    return BRLCAD_OK;
}

static int
bot_tree_show_subcommand_help(struct _ged_bot_info *gb, int argc, const char *argv[])
{
    const struct bu_cmd_tree_node *node;
    const char **help_argv;
    int result = BRLCAD_ERROR;

    if (!gb || argc < 1 || !argv || !argv[0])
	return BRLCAD_ERROR;
    node = bu_cmd_tree_find_subcommand(&ged_bot_tree, argv[0]);
    if (!node)
	return BRLCAD_ERROR;
    help_argv = (const char **)bu_calloc((size_t)argc + 1, sizeof(const char *),
	"bot subcommand help argv");
    help_argv[0] = node->schema->name;
    help_argv[1] = HELPFLAG;
    for (int i = 1; i < argc; i++)
	help_argv[i + 1] = argv[i];
    (void)bu_cmd(_bot_cmds, argc + 1, help_argv, 0, gb, &result);
    bu_free((void *)help_argv, "bot subcommand help argv");
    return result;
}

static int
ged_bot_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_bot_tree, input, cursor_pos, result);
}

static int
ged_bot_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_bot_tree, input, analysis);
}

static char *
ged_bot_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_bot_tree);
}

static int
ged_bot_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_bot_tree, msgs);
}

static const struct ged_cmd_grammar ged_bot_grammar = {
    "bot", "Inspect and edit BoT objects", ged_bot_grammar_validate,
    ged_bot_grammar_analyze, ged_bot_grammar_json, ged_bot_grammar_lint
};


extern "C" int
ged_bot_core(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_bot_info gb;
    gb.gedp = gedp;
    gb.cmds = _bot_cmds;
    gb.verbosity = 0;
    gb.visualize = 0;
    gb.vlfree = &rt_vlfree;
    struct bu_color *color = NULL;
    struct bot_root_args args = {0, 0, 0, {0.0, 0.0, 0.0}};
    int child_index;
    int color_set;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the bot command - start processing args
    argc--; argv++;

    if (!argc) {
	bot_tree_show_help(gedp);
	return BRLCAD_OK;
    }

    child_index = bu_cmd_schema_parse(&bot_root_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (child_index < 0) {
	bot_tree_show_help(gedp);
	return BRLCAD_ERROR;
	}
    color_set = bu_cmd_schema_option_present(&bot_root_schema, (size_t)child_index,
	argv, "color");
    gb.verbosity = args.verbosity;
    gb.visualize = args.visualize;
    if (args.print_help) {
	if (child_index < argc) {
	    argc -= child_index;
	    argv += child_index;
	    if (bot_tree_show_subcommand_help(&gb, argc, argv) != BRLCAD_OK)
		bot_tree_show_help(gedp);
	} else {
	    bot_tree_show_help(gedp);
	}
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (child_index >= argc) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	bot_tree_show_help(gedp);
	return BRLCAD_ERROR;
    }
    argc -= child_index;
    argv += child_index;

    if (color_set) {
	BU_GET(color, struct bu_color);
	*color = args.color;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    if (gb.visualize || BU_STR_EQUAL(argv[0], "plot")) {
	GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
	gb.vbp = bv_vlblock_init(gb.vlfree, 32);
    }
    gb.color = color;

    int ret = BRLCAD_ERROR;
    if (bu_cmd_tree_dispatch(&ged_bot_tree, (void *)&gb, argc, argv, &ret) == 0) {
	ret = BRLCAD_OK;
	goto bot_cleanup;
    }

    bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);

bot_cleanup:
    if (gb.intern) {
	rt_db_free_internal(gb.intern);
	BU_PUT(gb.intern, struct rt_db_internal);
    }
    if (gb.visualize) {
	bv_vlblock_free(gb.vbp);
	gb.vbp = (struct bv_vlblock *)NULL;
    }
    if (color) {
	BU_PUT(color, struct bu_color);
    }
    return ret;
}

#include "../include/plugin.h"

#define GED_BOT_COMMANDS(X, XID, N, NID, G, GID) \
    G(bot,              ged_bot_core,                      GED_CMD_DEFAULT, &ged_bot_grammar) \
    N(bot_condense,     ged_bot_condense_core,             GED_CMD_DEFAULT, &ged_bot_condense_schema) \
    N(bot_decimate,     ged_bot_decimate_core,             GED_CMD_DEFAULT, &ged_bot_decimate_schema) \
    N(bot_dump,         ged_bot_dump_core,                 GED_CMD_DEFAULT, &ged_bot_dump_schema) \
    N(bot_exterior,     ged_bot_exterior,                  GED_CMD_DEFAULT, &ged_bot_exterior_schema) \
    N(bot_face_fuse,    ged_bot_face_fuse_core,            GED_CMD_DEFAULT, &ged_bot_face_fuse_schema) \
    N(bot_face_sort,    ged_bot_face_sort_core,            GED_CMD_DEFAULT, &ged_bot_face_sort_schema) \
    N(bot_flip,         ged_bot_flip_core,                 GED_CMD_DEFAULT, &ged_bot_flip_schema) \
    N(bot_fuse,         ged_bot_fuse_core,                 GED_CMD_DEFAULT, &ged_bot_fuse_schema) \
    N(bot_merge,        ged_bot_merge_core,                GED_CMD_DEFAULT, &ged_bot_merge_schema) \
    N(bot_smooth,       ged_bot_smooth_core,               GED_CMD_DEFAULT, &ged_bot_smooth_schema) \
    N(bot_split,        ged_bot_split_core,                GED_CMD_DEFAULT, &ged_bot_split_schema) \
    N(bot_sync,         ged_bot_sync_core,                 GED_CMD_DEFAULT, &ged_bot_sync_schema) \
    N(bot_vertex_fuse,  ged_bot_vertex_fuse_core,          GED_CMD_DEFAULT, &ged_bot_vertex_fuse_schema) \
    N(dbot_dump,        ged_dbot_dump_core,                GED_CMD_DEFAULT, &ged_dbot_dump_schema) \
    N(find_bot_edge,    ged_find_bot_edge_nearest_pnt_core, GED_CMD_DEFAULT, &ged_find_bot_edge_schema) \
    N(find_bot_pnt,     ged_find_bot_pnt_nearest_pnt_core,  GED_CMD_DEFAULT, &ged_find_bot_pnt_schema) \
    N(get_bot_edges,    ged_get_bot_edges_core,             GED_CMD_DEFAULT, &ged_get_bot_edges_schema)

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_BOT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_bot", 1, GED_BOT_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
