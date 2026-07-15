/*                         A R B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file arb.cpp
 *
 * Top level command for logic specific to ARB8 objects.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rt/geom.h"
#include "bu/cmdschema.h"
#include "../ged_private.h"
#include "ged_arb.h"

extern "C" int
_arb_cmd_create(void *bs, int argc, const char *argv[])
{
    struct _ged_arb_info *gb = (struct _ged_arb_info *)bs;
    struct ged *gedp = gb->gedp;
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_arb_internal *arb;
    int i, j;
    double rota, fb_a;
    vect_t norm1, norm2, norm3;
    static const char *usage = "name rot fb";

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);

    /* get rotation angle */
    if (sscanf(argv[2], "%lf", &rota) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad rotation angle - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    /* get fallback angle */
    if (sscanf(argv[3], "%lf", &fb_a) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad fallback angle - %s", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }

    rota *= DEG2RAD;
    fb_a *= DEG2RAD;

    BU_ALLOC(arb, struct rt_arb_internal);
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_ARB8;
    internal.idb_meth = &OBJ[ID_ARB8];
    internal.idb_ptr = (void *)arb;
    arb->magic = RT_ARB_INTERNAL_MAGIC;

    VSET(arb->pt[0], 0.0, 0.0, 0.0);

    /* calculate normal vector defined by rot, fb_a */
    norm1[0] = cos(fb_a) * cos(rota);
    norm1[1] = cos(fb_a) * sin(rota);
    norm1[2] = sin(fb_a);

    /* find two perpendicular vectors which are perpendicular to norm */
    j = 0;
    for (i = 0; i < 3; i++) {
	if (fabs(norm1[i]) < fabs(norm1[j]))
	    j = i;
    }
    VSET(norm2, 0.0, 0.0, 0.0);
    norm2[j] = 1.0;
    VCROSS(norm3, norm2, norm1);
    VCROSS(norm2, norm3, norm1);

    /* create new rpp 20x20x2 */
    /* the 20x20 faces are in rot, fb plane */
    VUNITIZE(norm2);
    VUNITIZE(norm3);
    VJOIN1(arb->pt[1], arb->pt[0], 508.0, norm2);
    VJOIN1(arb->pt[3], arb->pt[0], -508.0, norm3);
    VJOIN2(arb->pt[2], arb->pt[0], 508.0, norm2, -508.0, norm3);
    for (i = 0; i < 4; i++)
	VJOIN1(arb->pt[i+4], arb->pt[i], -50.8, norm1);

    GED_DB_DIRADD(gedp, dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&internal.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &internal, BRLCAD_ERROR);

    return BRLCAD_OK;
}

static const struct bu_cmd_operand arb_create_operands[] = {
    BU_CMD_OPERAND("name", BU_CMD_VALUE_STRING, 1, 1,
	"New ARB object name", NULL),
    BU_CMD_OPERAND("rotation", BU_CMD_VALUE_NUMBER, 1, 1,
	"Rotation angle in degrees", NULL),
    BU_CMD_OPERAND("fallback", BU_CMD_VALUE_NUMBER, 1, 1,
	"Fallback angle in degrees", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema arb_create_schema = {
    "create", "Create an ARB from rotation and fallback angles", NULL,
    arb_create_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema arb_legacy_schema = {
    "arb", "Create an ARB using the deprecated positional syntax", NULL,
    arb_create_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema arb_root_schema = {
    "arb", "Create or repair ARB objects", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static int
arb_schema_preflight(struct ged *gedp, const struct bu_cmd_schema *schema,
	int argc, const char *argv[])
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    if (bu_cmd_schema_parse_complete(schema, NULL, &msg, argc, argv) >= 0) {
	ret = BRLCAD_OK;
    } else if (bu_vls_strlen(&msg)) {
	bu_vls_vlscat(gedp->ged_result_str, &msg);
    } else {
	bu_vls_printf(gedp->ged_result_str, "Invalid arb %s arguments.", schema->name);
    }
    bu_vls_free(&msg);
    return ret;
}


static int
arb_tree_create(void *data, int argc, const char *argv[])
{
    struct _ged_arb_info info = {(struct ged *)data};

    if (arb_schema_preflight(info.gedp, &arb_create_schema, argc - 1, argv + 1) != BRLCAD_OK)
	return BRLCAD_ERROR;
    return _arb_cmd_create(&info, argc, argv);
}


static int
arb_tree_repair(void *data, int argc, const char *argv[])
{
    struct _ged_arb_info info = {(struct ged *)data};

    return _arb_cmd_repair(&info, argc, argv);
}


static const struct bu_cmd_tree_node arb_subcommands[] = {
    BU_CMD_TREE_NODE(&arb_create_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, arb_tree_create),
    BU_CMD_TREE_NODE(&ged_arb_repair_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, arb_tree_repair),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree ged_arb_tree = {
    &arb_root_schema, arb_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


static void
arb_native_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&ged_arb_tree);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "ARB native tree help");
    }
    bu_vls_strcat(gedp->ged_result_str,
	"\nDeprecated form: arb name rotation fallback\n");
}


extern "C" int
ged_arb_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_ERROR;
    struct _ged_arb_info info = {gedp};

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	arb_native_help(gedp);
	return GED_HELP;
    }
    if (bu_cmd_tree_dispatch(&ged_arb_tree, gedp, argc - 1, argv + 1, &ret) == 0)
	return ret;

    if (arb_schema_preflight(gedp, &arb_legacy_schema, argc - 1, argv + 1) != BRLCAD_OK) {
	arb_native_help(gedp);
	return BRLCAD_ERROR;
    }
    bu_log("WARNING: The 'arb name rot fb' syntax is deprecated and will be removed in a future release.\n");
    bu_log("         Please use 'arb create name rot fb' instead.\n");
    return _arb_cmd_create(&info, argc, argv);
}

#include "../include/plugin.h"

extern "C" int ged_rotate_arb_face_core(struct ged *gedp, int argc, const char *argv[]);

static const struct ged_cmd_native_form arb_native_forms[] = {
    {"subcommands", NULL, &ged_arb_tree},
    {"deprecated_create", &arb_legacy_schema, NULL},
    {NULL, NULL, NULL}
};


static const struct ged_cmd_native_form *
arb_select_native_form(const struct ged *UNUSED(gedp), size_t argc,
	const char * const *argv)
{
    const char *word = argc > 1 ? argv[1] : "";
    size_t length = word ? strlen(word) : 0;

    if (!length || !strncmp("create", word, length) || !strncmp("repair", word, length))
	return &arb_native_forms[0];
    return &arb_native_forms[1];
}


static int
ged_arb_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_native_forms_validate(gedp, arb_native_forms, arb_select_native_form,
	input, cursor_pos, result);
}


static int
ged_arb_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_native_forms_analyze(gedp, arb_native_forms, arb_select_native_form,
	input, analysis);
}


static char *
ged_arb_grammar_json(void)
{
    return ged_cmd_native_forms_describe_json("arb", "Create or repair ARB objects",
	arb_native_forms);
}


static int
ged_arb_grammar_lint(struct bu_vls *msgs)
{
    return ged_cmd_native_forms_lint("arb", arb_native_forms, msgs);
}


static const struct ged_cmd_grammar ged_arb_grammar = {
    "arb", "Create or repair ARB objects", ged_arb_grammar_validate,
    ged_arb_grammar_analyze, ged_arb_grammar_json, ged_arb_grammar_lint
};

#define GED_ARB_COMMANDS(X, XID, N, NID, G, GID) \
    G(arb, ged_arb_core, GED_CMD_DEFAULT, &ged_arb_grammar) \
    N(rotate_arb_face, ged_rotate_arb_face_core, GED_CMD_DEFAULT, &ged_rotate_arb_face_schema)

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_ARB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_arb", 1, GED_ARB_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
