/*                         I L L U M . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/illum.c
 *
 * The illum command.
 *
 */

#include "common.h"
#include <string.h>

#include "dm.h" // For labelvert - see if we really need the dm_set_dirty call there...

#include "bu/cmdschema.h"

#include "ged.h"
#include "../ged_private.h"

static const struct bu_cmd_schema *illum_schema(void);
static const struct bu_cmd_schema *labelvert_schema(void);

struct illum_args {
    int disable;
};

/* Usage:  labelvert solid(s) */
int
ged_labelvert_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    struct bv_vlblock*vbp;
    mat_t mat;
    fastf_t scale;
    static const char *usage = "object(s) - label vertices of wireframes of objects";

    if (!gedp || !gedp->dbip)
	return BRLCAD_ERROR;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (bu_cmd_schema_parse_complete(labelvert_schema(), NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
	}

    vbp = rt_vlblock_init();
    MAT_IDN(mat);
    bn_mat_inv(mat, gedp->ged_gvp->gv_rotation);
    scale = gedp->ged_gvp->gv_size / 100;          /* divide by # chars/screen */

    for (i=1; i<argc; i++) {
	struct bv_scene_obj *s;
	struct directory *dp;
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	/* Find uses of this solid in the solid table */
	gdlp = BU_LIST_NEXT(display_list, gedp->i->ged_gdp->gd_headDisplay);
	while (BU_LIST_NOT_HEAD(gdlp, gedp->i->ged_gdp->gd_headDisplay)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    for (BU_LIST_FOR(s, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
		if (!s->s_u_data)
		    continue;
		struct ged_bv_data *bdata = (struct ged_bv_data *)s->s_u_data;
		if (db_full_path_search(&bdata->s_fullpath, dp)) {
		    rt_label_vlist_verts(vbp, &s->s_vlist, mat, scale, gedp->dbip->dbi_base2local);
		}
	    }

	    gdlp = next_gdlp;
	}
    }

    _ged_cvt_vlblock_to_solids(gedp, vbp, "_LABELVERT_", 0);

    bv_vlblock_free(vbp);
    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (dmp)
	dm_set_dirty(dmp, 1);
    return BRLCAD_OK;
}


static int
dl_set_illum(struct display_list *gdlp, const char *obj, int illum)
{
    int found = 0;
    struct bv_scene_obj *sp;

    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	size_t i;
	if (!sp->s_u_data)
	    continue;
	struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	for (i = 0; i < bdata->s_fullpath.fp_len; ++i) {
	    if (*obj == *DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_namep &&
		BU_STR_EQUAL(obj, DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_namep)) {
		found = 1;
		if (illum)
		    sp->s_iflag = UP;
		else
		    sp->s_iflag = DOWN;
	    }
	}
    }
    return found;
}

/*
 * Illuminate/highlight database object
 *
 * Usage:
 * illum [-n] obj
 *
 */
int
ged_illum_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int found = 0;
    int illum = 1;
    static const char *usage = "[-n] obj";
    struct illum_args args = {0};
    int operand_index;
    const char *object;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(illum_schema(), &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
	goto bad;
    object = argv[operand_index + 1];
    illum = args.disable ? 0 : 1;

    gdlp = BU_LIST_NEXT(display_list, gedp->i->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->i->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	found += dl_set_illum(gdlp, object, illum);

	gdlp = next_gdlp;
    }


    if (!found) {
	bu_vls_printf(gedp->ged_result_str, "illum: %s not found", object);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

static int
illum_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t disables = 0;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    for (size_t i = 0; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--")) {
	    bu_cmd_validate_result_clear(result);
	    result->state = BU_CMD_VALIDATE_INVALID;
	    result->token_start = i;
	    result->token_end = i;
	    result->expected = BU_CMD_EXPECT_OPTION;
	    result->completion_type = BU_CMD_VALUE_FLAG;
	    result->hint = "end-of-options marker is not supported";
	    return 0;
	}
	if (BU_STR_EQUAL(argv[i], "-n"))
	    disables++;
    }
    if (disables > 1) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPTION;
	result->completion_type = BU_CMD_VALUE_FLAG;
	result->hint = "-n may be specified only once";
    }
    return 0;
}

static const struct bu_cmd_option illum_schema_options[] = {
    BU_CMD_FLAG("n", NULL, struct illum_args, disable, "Disable illumination"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand illum_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 1, 1,
	"Displayed object path", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand labelvert_schema_operands[] = {
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Objects whose vertices are labeled", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema illum_cmd_schema = {
    "illum", "Set object illumination", illum_schema_options, illum_schema_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, {illum_schema_validate}
};
static const struct bu_cmd_schema labelvert_cmd_schema = {
    "labelvert", "Label wireframe vertices", NULL, labelvert_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

static const struct bu_cmd_schema *
illum_schema(void)
{
    return &illum_cmd_schema;
}

static const struct bu_cmd_schema *
labelvert_schema(void)
{
    return &labelvert_cmd_schema;
}

#define GED_ILLUM_COMMANDS(X, XID) \
    X(illum, ged_illum_core, GED_CMD_DEFAULT, &illum_cmd_schema) \
    X(labelvert, ged_labelvert_core, GED_CMD_DEFAULT, &labelvert_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ILLUM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_illum", 1, GED_ILLUM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
