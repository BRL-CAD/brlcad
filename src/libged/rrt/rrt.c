/*                         R R T . C
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
/** @file libged/rrt.c
 *
 * The rrt command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"


#include "../ged_private.h"

static const struct bu_cmd_schema *rrt_schema(void);


int
ged_rrt_core(struct ged *gedp, int argc, const char *argv[])
{
    char **vp;
    int i;
    size_t args;
    char **gd_rt_cmd = NULL;
    int gd_rt_cmd_len = 0;
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(rrt_schema(), &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
	return BRLCAD_ERROR;

    args = argc + 2 + ged_who_argc(gedp);
    gd_rt_cmd = (char **)bu_calloc(args, sizeof(char *), "alloc gd_rt_cmd");

    vp = &gd_rt_cmd[0];
    for (i = 1; i < argc; i++)
	*vp++ = (char *)argv[i];
    *vp++ = gedp->dbip->dbi_filename;

    gd_rt_cmd_len = ged_who_argv(gedp, vp, (const char **)&gd_rt_cmd[args]);

    (void)_ged_run_rt(gedp, gd_rt_cmd_len, (const char **)gd_rt_cmd, -1, NULL, 0, NULL, NULL, NULL);

    bu_free(gd_rt_cmd, "free gd_rt_cmd");

    return BRLCAD_OK;
}


#include "../include/plugin.h"

static const struct bu_cmd_operand rrt_schema_operands[] = {
    BU_CMD_OPERAND("raytrace_arguments", BU_CMD_VALUE_RAW, 0,
	BU_CMD_COUNT_UNLIMITED, "Arguments passed through to the raytracer", NULL),
    BU_CMD_OPERAND_NULL
};
static int
rrt_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    bu_cmd_validate_result_clear(result);
    result->state = BU_CMD_VALIDATE_VALID;
    result->token_start = cursor_arg < argc ? cursor_arg : argc;
    result->token_end = result->token_start;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = BU_CMD_VALUE_RAW;
    result->hint = "raytracer pass-through argument";
    return 0;
}
static const struct bu_cmd_schema rrt_cmd_schema = {
    "rrt", "Run the raytracer on displayed objects", NULL, rrt_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {rrt_schema_validate}
};

static const struct bu_cmd_schema *
rrt_schema(void)
{
    return &rrt_cmd_schema;
}

#define GED_RRT_COMMANDS(X, XID) \
    X(rrt, ged_rrt_core, GED_CMD_DEFAULT, &rrt_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_RRT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_rrt", 1, GED_RRT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
