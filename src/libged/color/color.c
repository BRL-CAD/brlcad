/*                         C O L O R . C
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
/** @file libged/color.c
 *
 * The color command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/color.h"
#include "bu/file.h"
#include "ged.h"
#include "rt/db4.h"
#include "raytrace.h"

#include "../ged_private.h"


/**
 * Used to create a database record and get it written out to a granule.
 * In some cases, storage will need to be allocated.
 */
void
color_putrec(struct ged *gedp, struct mater *mp)
{
    struct directory dir;
    union record rec;

    /* we get here only if database is NOT read-only */

    rec.md.md_id = ID_COLORTAB;
    rec.md.md_low = mp->mt_low;
    rec.md.md_hi = mp->mt_high;
    rec.md.md_r = mp->mt_r;
    rec.md.md_g = mp->mt_g;
    rec.md.md_b = mp->mt_b;

    /* Fake up a directory entry for db_* routines */
    RT_DIR_SET_NAMEP(&dir, "color_putrec");
    dir.d_magic = RT_DIR_MAGIC;
    dir.d_flags = 0;

    if (mp->mt_daddr == MATER_NO_ADDR) {
	/* Need to allocate new database space */
	if (db_alloc(gedp->dbip, &dir, 1)) {
	    bu_vls_printf(gedp->ged_result_str, "Database alloc error, aborting");
	    return;
	}
	mp->mt_daddr = dir.d_addr;
    } else {
	dir.d_addr = mp->mt_daddr;
	dir.d_len = 1;
    }

    if (db_put(gedp->dbip, &dir, &rec, 0, 1)) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return;
    }
}


/**
 * Used to release database resources occupied by a material record.
 */
void
color_zaprec(struct ged *gedp, struct mater *mp)
{
    struct directory dir;

    /* we get here only if database is NOT read-only */
    if (mp->mt_daddr == MATER_NO_ADDR)
	return;

    dir.d_magic = RT_DIR_MAGIC;
    RT_DIR_SET_NAMEP(&dir, "color_zaprec");
    dir.d_len = 1;
    dir.d_addr = mp->mt_daddr;
    dir.d_flags = 0;

    if (db_delete(gedp->dbip, &dir) != 0) {
	bu_vls_printf(gedp->ged_result_str, "Database delete error, aborting");
	return;
    }
    mp->mt_daddr = MATER_NO_ADDR;
}


/*
 * used by the 'color' command when provided the -e option
 */
static int
_edcolor(struct ged *gedp, const char *command, const char *editstring)
{
    struct mater *mp;
    struct mater *zot;
    FILE *fp;
    char line[128];
    static char hdr[] = "LOW\tHIGH\tRed\tGreen\tBlue\n";
    char tmpfil[MAXPATHLEN];
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* build temp file path */
    bu_dir(tmpfil, MAXPATHLEN, BU_DIR_TEMP, bu_temp_file_name(NULL, 0), NULL);

    fp = fopen(tmpfil, "w+");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: could not create tmp file", command);
	return BRLCAD_ERROR;
    }

    fprintf(fp, "%s", hdr);
    for (mp = db_mater_head(gedp->dbip); mp != MATER_NULL; mp = mp->mt_forw) {
	fprintf(fp, "%ld\t%ld\t%3d\t%3d\t%3d",
		      mp->mt_low, mp->mt_high,
		      mp->mt_r, mp->mt_g, mp->mt_b);
	fprintf(fp, "\n");
    }
    (void)fclose(fp);

    if (!_ged_editit(gedp, editstring, (const char *)tmpfil)) {
	bu_vls_printf(gedp->ged_result_str, "%s: editor returned bad status. Aborted\n", command);
	return BRLCAD_ERROR;
    }

    /* Read file and process it */
    fp = fopen(tmpfil, "r");
    if (fp == NULL) {
	perror(tmpfil);
	return BRLCAD_ERROR;
    }

    if (bu_fgets(line, sizeof (line), fp) == NULL ||
	line[0] != hdr[0]) {
	bu_vls_printf(gedp->ged_result_str, "%s: Header line damaged, aborting\n", command);
	(void)fclose(fp);
	return BRLCAD_ERROR;
    }

    if (db_version(gedp->dbip) < 5) {
	/* Zap all the current records, both in core and on disk */
	while (db_mater_head(gedp->dbip) != MATER_NULL) {
	    zot = db_mater_head(gedp->dbip);
	    db_mater_set_head(gedp->dbip, zot->mt_forw);
	    color_zaprec(gedp, zot);
	    bu_free((void *)zot, "mater rec");
	}

	while (bu_fgets(line, sizeof (line), fp) != NULL) {
	    int cnt;
	    int low, hi, r, g, b;

	    /* character-separated numbers (ideally a space) */
	    cnt = sscanf(line, "%d%*c%d%*c%d%*c%d%*c%d",
			 &low, &hi, &r, &g, &b);
	    if (cnt != 9) {
		bu_vls_printf(gedp->ged_result_str, "%s: Discarding %s\n", command, line);
		continue;
	    }
	    BU_ALLOC(mp, struct mater);
	    mp->mt_low = low;
	    mp->mt_high = hi;
	    mp->mt_r = r;
	    mp->mt_g = g;
	    mp->mt_b = b;
	    mp->mt_daddr = MATER_NO_ADDR;
	    db_mater_insert(gedp->dbip, mp);
	    color_putrec(gedp, mp);
	}
    } else {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	/* free colors in db_mater_head */
	db_mater_free(gedp->dbip);

	while (bu_fgets(line, sizeof (line), fp) != NULL) {
	    int cnt;
	    int low, hi, r, g, b;

	    /* character-separated numbers (ideally a space) */
	    cnt = sscanf(line, "%d%*c%d%*c%d%*c%d%*c%d",
			 &low, &hi, &r, &g, &b);

	    /* check to see if line is reasonable */
	    if (cnt != 5) {
		bu_vls_printf(gedp->ged_result_str, "%s: Discarding %s\n", command, line);
		continue;
	    }
	    bu_vls_printf(&vls, "{%d %d %d %d %d} ", low, hi, r, g, b);
	}

	db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&vls), gedp->dbip);
	db5_import_color_table(gedp->dbip, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    (void)fclose(fp);
    bu_file_delete(tmpfil);

    /* if there are drawables, update their colors */
    dl_color_soltab((struct bu_list *)ged_dl(gedp), gedp->dbip);

    return BRLCAD_OK;
}


struct color_args {
    int edit;
};

struct edcolor_args {
    const char *editor;
    int help;
};

static const struct bu_arg_option color_options[] = {
    BU_ARG_FLAG("e", NULL, struct color_args, edit,
	"Edit the color table interactively"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand color_record_operands[] = {
    BU_ARG_OPERAND("low", BU_ARG_VALUE_INTEGER, 1, 1,
	"Lowest region identifier", NULL),
    BU_ARG_OPERAND("high", BU_ARG_VALUE_INTEGER, 1, 1,
	"Highest region identifier", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("red", 1, 1, 1, 0, 1, 255,
	"Red channel", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("green", 1, 1, 1, 0, 1, 255,
	"Green channel", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("blue", 1, 1, 1, 0, 1, 255,
	"Blue channel", NULL),
    BU_ARG_OPERAND_NULL
};
static const char * const color_edit_option[] = {"e", NULL};
static const struct bu_arg_schema_case color_cases[] = {
    BU_ARG_SCHEMA_CASE("edit", "Edit the color table instead of adding a record",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, color_edit_option, NULL, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("record", "Add a numeric color-table record",
	color_record_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};
static const struct bu_arg_schema color_cmd_schema =
    BU_ARG_SCHEMA_CASED("color", "Set or edit region color records",
	color_options, BU_ARG_PARSE_OPTIONS_FIRST, color_cases);

static const struct bu_arg_option edcolor_options[] = {
    BU_ARG_VALUE("E", NULL, struct edcolor_args, editor, BU_ARG_VALUE_STRING,
	BU_ARG_REQUIRED, "editor", "Editor command"),
    BU_ARG_FLAG("h", "help", struct edcolor_args, help,
	"Print command help"),
    BU_ARG_ALIAS("?", NULL, "help", 0),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_schema edcolor_cmd_schema =
    BU_ARG_SCHEMA("edcolor", "Edit the region color table", edcolor_options,
	NULL, BU_ARG_PARSE_OPTIONS_FIRST,
	BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static void
color_show_help(struct ged *gedp, const char *command)
{
    char *help = ged_cmd_help(command, command);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "color standard help");
    }
}

static void
edcolor_show_help(struct ged *gedp, const char *command)
{
    char *help = ged_cmd_help(command, command);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "edcolor standard help");
    }
}


int
ged_edcolor_core(struct ged *gedp, int argc, const char *argv[])
{
    struct edcolor_args args = {NULL, 0};
    const char *command = argv[0];
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);
    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&edcolor_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	edcolor_show_help(gedp, command);
	return BRLCAD_ERROR;
    }
    if (args.help) {
	edcolor_show_help(gedp, command);
	return GED_HELP;
    }
    return _edcolor(gedp, command, args.editor);
}


int
ged_color_core(struct ged *gedp, int argc, const char *argv[])
{
    struct color_args args = {0};
    struct mater *newp;
    struct mater *mp;
    struct mater *next_mater;
    unsigned char rgb[3] = {0, 0, 0};
    int low = 0;
    int high = 0;
    int operand_index;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc == 1) {
	color_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&color_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	color_show_help(gedp, command);
	return BRLCAD_ERROR;
    }
    argv += operand_index;
    if (args.edit) {
	return _edcolor(gedp, command, NULL);
    }

    if (!bu_arg_integer_from_str(NULL, &low, argv[0], 0) ||
	!bu_arg_integer_from_str(NULL, &high, argv[1], 0) ||
	!bu_rgb_from_argv(rgb, 3, argv + 2)) {
	color_show_help(gedp, command);
	return BRLCAD_ERROR;
    }

    if (db_version(gedp->dbip) < 5) {
	/* Delete all color records from the database */
	mp = db_mater_head(gedp->dbip);
	while (mp != MATER_NULL) {
	    next_mater = mp->mt_forw;
	    color_zaprec(gedp, mp);
	    mp = next_mater;
	}

	BU_ALLOC(newp, struct mater);
	newp->mt_low = low;
	newp->mt_high = high;
	newp->mt_r = rgb[RED];
	newp->mt_g = rgb[GRN];
	newp->mt_b = rgb[BLU];
	newp->mt_daddr = MATER_NO_ADDR;

	db_mater_insert(gedp->dbip, newp);

	mp = db_mater_head(gedp->dbip);
	while (mp != MATER_NULL) {
	    next_mater = mp->mt_forw;
	    color_putrec(gedp, mp);
	    mp = next_mater;
	}
    } else {
	struct bu_vls colors = BU_VLS_INIT_ZERO;

	BU_ALLOC(newp, struct mater);
	newp->mt_low = low;
	newp->mt_high = high;
	newp->mt_r = rgb[RED];
	newp->mt_g = rgb[GRN];
	newp->mt_b = rgb[BLU];
	newp->mt_daddr = MATER_NO_ADDR;

	db_mater_insert(gedp->dbip, newp);
	db_mater_to_vls(&colors, gedp->dbip);
	db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&colors), gedp->dbip);
	bu_vls_free(&colors);
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_COLOR_COMMANDS(X, XID) \
    X(color, ged_color_core, GED_CMD_DEFAULT, &color_cmd_schema) \
    X(edcolor, ged_edcolor_core, GED_CMD_DEFAULT, &edcolor_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COLOR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_color", 1, GED_COLOR_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
