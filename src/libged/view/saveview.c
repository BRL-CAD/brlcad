/*                         S A V E V I E W . C
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
/** @file libged/saveview.c
 *
 * The saveview command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/file.h"


#include "../ged_private.h"


struct saveview_args {
    const char *rt_command;
    const char *input_geometry;
    const char *log_file;
    const char *pix_file;
};


static const struct bu_cmd_option saveview_schema_options[] = {
    BU_CMD_STRING("e", NULL, struct saveview_args, rt_command, "command",
	"Raytrace command to invoke"),
    BU_CMD_FILE("i", NULL, struct saveview_args, input_geometry, "file",
	"Input geometry file"),
    BU_CMD_FILE("l", NULL, struct saveview_args, log_file, "file",
	"Write diagnostics to file"),
    BU_CMD_FILE("o", NULL, struct saveview_args, pix_file, "file",
	"Write pixels to file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand saveview_schema_operands[] = {
    BU_CMD_OPERAND("filename", BU_CMD_VALUE_FILE, 1, 1,
	"Shell script to create", NULL),
    /* Everything following the script name is passed through to the selected
     * raytrace program.  Options must therefore precede filename. */
    BU_CMD_OPERAND("args", BU_CMD_VALUE_STRING, 0, BU_CMD_COUNT_UNLIMITED,
	"Additional raytrace arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema saveview_native_schema = {
    "saveview", "Write a raytrace shell script for the view",
    saveview_schema_options, saveview_schema_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


/**
 * Return basename of path, removing leading slashes and trailing suffix.
 */
static char *
basename_without_suffix(const char *p1, const char *suff)
{
    char *p2, *p3;
    static char buf[128];

    /* find the basename */
    p2 = (char *)p1;
    while (*p1) {
	if (*p1++ == '/')
	    p2 = (char *)p1;
    }

    /* find the end of suffix */
    for (p3=(char *)suff; *p3; p3++)
	;

    /* early out */
    while (p1>p2 && p3>suff) {
	if (*--p3 != *--p1)
	    return p2;
    }

    /* stash and return filename, sans suffix */
    bu_strlcpy(buf, p2, p1-p2+1);
    return buf;
}


int
ged_saveview_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    int i;
    FILE *fp;
    char *base;
    char rtcmd[255] = {'r', 't', 0};
    char outlog[255] = {0};
    char outpix[255] = {0};
    char inputg[255] = {0};
    const char *cmdname = argv[0];
    struct saveview_args args = {0, 0, 0, 0};
    int operand_index;
    static const char *usage = "[-e] [-i] [-l] [-o] filename [args]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdname, usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&saveview_native_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdname, usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    if (args.rt_command)
	snprintf(rtcmd, 255, "%s", args.rt_command);
    if (args.log_file)
	snprintf(outlog, 255, "%s", args.log_file);
    if (args.pix_file)
	snprintf(outpix, 255, "%s", args.pix_file);
    if (args.input_geometry)
	snprintf(inputg, 255, "%s", args.input_geometry);

    fp = fopen(argv[0], "a");
    if (fp == NULL) {
	perror(argv[0]);
	return BRLCAD_ERROR;
    }
    (void)bu_fchmod(fileno(fp), 0755);	/* executable */

    if (!gedp->dbip->dbi_filename) {
	bu_log("Error: geometry file is not specified\n");
	fclose(fp);
	return BRLCAD_ERROR;
    }

    if (!bu_file_exists(gedp->dbip->dbi_filename, NULL)) {
	bu_log("Error: %s does not exist\n", gedp->dbip->dbi_filename);
	fclose(fp);
	return BRLCAD_ERROR;
    }

    base = basename_without_suffix(argv[0], ".sh");
    if (outpix[0] == '\0') {
	snprintf(outpix, 255, "%s.pix", base);
    }
    if (outlog[0] == '\0') {
	snprintf(outlog, 255, "%s.log", base);
    }

    /* Do not specify -v option to rt; batch jobs must print everything. -Mike */
    fprintf(fp, "#!/bin/sh\n%s -M ", rtcmd);
    if (gedp->ged_gvp->gv_perspective > 0)
	fprintf(fp, "-p%g ", gedp->ged_gvp->gv_perspective);
    for (i = 1; i < argc; i++)
	fprintf(fp, "%s ", argv[i]);

    if (bu_strncmp(rtcmd, "nirt", 4) != 0)
	fprintf(fp, "\\\n -o %s\\\n $*\\\n", outpix);

    if (inputg[0] == '\0') {
	snprintf(inputg, 255, "%s", gedp->dbip->dbi_filename);
    }
    fprintf(fp, " '%s'\\\n ", inputg);

    gdlp = BU_LIST_NEXT(display_list, gedp->i->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->i->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);
	fprintf(fp, "'%s' ", bu_vls_addr(&gdlp->dl_path));
	gdlp = next_gdlp;
    }

    fprintf(fp, "\\\n 2>> %s\\\n", outlog);
    fprintf(fp, " <<EOF\n");

    {
	vect_t eye_model;

	_ged_rt_set_eye_model(gedp, eye_model);
	_ged_rt_write(gedp, fp, eye_model, -1, NULL);
    }

    fprintf(fp, "\nEOF\n");
    (void)fclose(fp);

    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
