/*                         G L O B . C
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
/** @file libged/glob.c
 *
 * The glob command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu/cmdschema.h"
#include "bu/glob.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "ged.h"

/* These are general globbing flags. */
#define _GED_GLOB_HIDDEN       0x1    /**< @brief include hidden objects in results */
#define _GED_GLOB_NON_GEOM     0x2    /**< @brief include non-geometry objects in results */
#define _GED_GLOB_SKIP_FIRST   0x4    /**< @brief do not expand the first item */


static const struct bu_cmd_arg_shape glob_command_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_CUSTOM, 1, 1,
	"Quoted command string containing database glob patterns");
static const struct bu_cmd_operand glob_schema_operands[] = {
    BU_CMD_OPERAND_SHAPED("command_string", BU_CMD_VALUE_STRING, 1, 1, NULL,
	"Command string whose arguments may contain database glob patterns", NULL,
	&glob_command_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema glob_cmd_schema = {
    "glob", "Expand a database glob expression", NULL, glob_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};
static const struct bu_cmd_schema db_glob_cmd_schema = {
    "db_glob", "Expand a database glob expression", NULL, glob_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


static void
glob_show_help(struct ged *gedp, const char *command)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s command_string", command);
}

/**
 * unescapes various special characters
 */
static void
_debackslash(struct bu_vls *dest, struct bu_vls *src)
{
    char *ptr;

    ptr = bu_vls_addr(src);
    while (*ptr) {
	if (*ptr == '\\')
	    ++ptr;
	if (*ptr == '\0')
	    break;
	bu_vls_putc(dest, *ptr++);
    }
}

/**
 * escapes various special characters
 */
static void
_backslash_specials(struct bu_vls *dest, struct bu_vls *src)
{
    int backslashed;
    char *ptr, buf[2];

    buf[1] = '\0';
    backslashed = 0;
    for (ptr = bu_vls_addr(src); *ptr; ptr++) {
	if (*ptr == '[' && !backslashed)
	    bu_vls_strcat(dest, "\\[");
	else if (*ptr == ']' && !backslashed)
	    bu_vls_strcat(dest, "\\]");
	else if (backslashed) {
	    bu_vls_strcat(dest, "\\");
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	    backslashed = 0;
	} else if (*ptr == '\\')
	    backslashed = 1;
	else {
	    buf[0] = *ptr;
	    bu_vls_strcat(dest, buf);
	}
    }
}

static int
_ged_expand_str_glob(struct bu_vls *dest, const char *input, struct db_i *dbip, int flags)
{
    char *start, *end;          /* Start and ends of words */
    int is_fnmatch;                 /* Set to TRUE when word is a is_fnmatch */
    int backslashed;
    int match_cnt = 0;
    int firstword = 1;
    int skip_first = (flags & _GED_GLOB_SKIP_FIRST) ? 1 : 0;
    struct bu_vls word = BU_VLS_INIT_ZERO;         /* Current word being processed */
    struct bu_vls temp = BU_VLS_INIT_ZERO;
    char *src = NULL;

    if (dbip == DBI_NULL) {
	bu_vls_sprintf(dest, "%s", input);
	return 0;
    }

    src = bu_strdup(input);

    end = src;
    while (*end != '\0') {
	/* Run through entire string */

	/* First, pass along leading whitespace. */

	start = end;                   /* Begin where last word ended */
	while (*start != '\0') {
	    if (*start == ' '  ||
		    *start == '\t' ||
		    *start == '\n')
		bu_vls_putc(dest, *start++);
	    else
		break;
	}
	if (*start == '\0')
	    break;

	end = start;
	bu_vls_trunc(&word, 0);
	is_fnmatch = 0;
	backslashed = 0;
	while (*end != '\0') {
	    if (*end == ' '  ||
		    *end == '\t' ||
		    *end == '\n')
		break;
	    if ((*end == '*' || *end == '?' || *end == '[') && !backslashed)
		is_fnmatch = 1;
	    if (*end == '\\' && !backslashed)
		backslashed = 1;
	    else
		backslashed = 0;
	    bu_vls_putc(&word, *end++);
	}

	/* If the client told us not to do expansion on the first word
	 * in the stream, disable matching */
	if (firstword && skip_first) {
	    is_fnmatch = 0;
	}

	if (is_fnmatch) {
	    /* Use db_path_glob for pattern expansion against the database */
	    struct bu_glob_context *gp = bu_glob_ctx_create();
	    int i;

	    db_path_glob(gp, bu_vls_addr(&word), BU_GLOB_NOSORT, dbip);

	    bu_vls_trunc(&temp, 0);
	    for (i = 0; i < gp->gl_pathc; i++) {
		struct directory *dp;
		const char *name = bu_vls_cstr(gp->gl_pathv[i]);

		/* Apply hidden/non-geom filters */
		dp = db_lookup(dbip, name, LOOKUP_QUIET);
		if (dp != RT_DIR_NULL) {
		    if (!(flags & _GED_GLOB_HIDDEN) && (dp->d_flags & RT_DIR_HIDDEN)) continue;
		    if (!(flags & _GED_GLOB_NON_GEOM) && (dp->d_flags & RT_DIR_NON_GEOM)) continue;
		}

		if (bu_vls_strlen(&temp) > 0)
		    bu_vls_strcat(&temp, " ");
		bu_vls_strcat(&temp, name);
		match_cnt++;
	    }

	    bu_glob_ctx_destroy(gp);

	    if (match_cnt == 0 || bu_vls_strlen(&temp) == 0) {
		_debackslash(&temp, &word);
		_backslash_specials(dest, &temp);
	    } else {
		bu_vls_vlscat(dest, &temp);
	    }
	} else {
	    _debackslash(dest, &word);
	}

	firstword = 0;
    }

    bu_vls_free(&temp);
    bu_vls_free(&word);
    bu_free(src, "free input cpy");
    return match_cnt;
}

int
ged_glob_core(struct ged *gedp, int argc, const char *argv[])
{
    int flags = 0;
    int operand_index = 0;
    int parse_dummy = 0;
    const struct bu_cmd_schema *schema = NULL;
    const char *expression = NULL;
    flags |= _GED_GLOB_HIDDEN;
    flags |= _GED_GLOB_NON_GEOM;
    flags |= _GED_GLOB_SKIP_FIRST;

    /* Silently return */
    if (gedp == GED_NULL)
	return BRLCAD_ERROR;

    if (argc < 1)
	return BRLCAD_ERROR;

    schema = BU_STR_EQUAL(argv[0], "db_glob") ? &db_glob_cmd_schema : &glob_cmd_schema;

    /* Initialize result. This behavior is depended upon by mged - apparently
     * the interpretation is that if no database is open, all expressions match
     * nothing and the empty string is returned. */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* No database to match against, so return. */
    if (gedp->dbip == DBI_NULL)
	return BRLCAD_OK;

    /* must be wanting help */
    if (argc == 1) {
	glob_show_help(gedp, argv[0]);
	return GED_HELP;
    }

	operand_index = bu_cmd_schema_parse_complete(schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 1) {
	glob_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    expression = argv[operand_index + 1];

    (void)_ged_expand_str_glob(gedp->ged_result_str, expression, gedp->dbip, flags);


    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_GLOB_COMMANDS(X, XID) \
    X(db_glob, ged_glob_core, GED_CMD_DEFAULT, &db_glob_cmd_schema) \
    X(glob, ged_glob_core, GED_CMD_DEFAULT, &glob_cmd_schema)

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_GLOB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_glob", 1, GED_GLOB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
