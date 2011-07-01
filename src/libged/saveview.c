/*                         S A V E V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
#include "bio.h"

#include "solid.h"

#include "./ged_private.h"


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
ged_saveview(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct ged_display_list *next_gdlp;
    int i;
    FILE *fp;
    char *base;
    int c;
    char rtcmd[255] = {'r', 't', 0};
    char outlog[255] = {0};
    char outpix[255] = {0};
    char inputg[255] = {0};
    static const char *usage = "[-e] [-i] [-l] [-o] filename [args]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "e:i:l:o:")) != -1) {
	switch (c) {
	    case 'e':
		snprintf(rtcmd, 255, "%s", bu_optarg);
		break;
	    case 'l':
		snprintf(outlog, 255, "%s", bu_optarg);
		break;
	    case 'o':
		snprintf(outpix, 255, "%s", bu_optarg);
		break;
	    case 'i':
		snprintf(inputg, 255, "%s", bu_optarg);
		break;
	    default: {
		bu_vls_printf(gedp->ged_result_str, "Option '%c' unknown\n", c);
		bu_vls_printf(gedp->ged_result_str, "help saveview");
		return GED_ERROR;
	    }
	}
    }
    argc -= bu_optind-1;
    argv += bu_optind-1;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((fp = fopen(argv[1], "a")) == NULL) {
	perror(argv[1]);
	return GED_ERROR;
    }
    (void)bu_fchmod(fp, 0755);	/* executable */

    if (!gedp->ged_wdbp->dbip->dbi_filename) {
	bu_log("Error: geometry file is not specified\n");
	return GED_ERROR;
    }

    if (!bu_file_exists(gedp->ged_wdbp->dbip->dbi_filename)) {
	bu_log("Error: %s does not exist\n", gedp->ged_wdbp->dbip->dbi_filename);
	return GED_ERROR;
    }

    base = basename_without_suffix(argv[1], ".sh");
    if (outpix[0] == '\0') {
	snprintf(outpix, 255, "%s.pix", base);
    }
    if (outlog[0] == '\0') {
	snprintf(outlog, 255, "%s.log", base);
    }

    /* Do not specify -v option to rt; batch jobs must print everything. -Mike */
    (void)fprintf(fp, "#!/bin/sh\n%s -M ", rtcmd);
    if (gedp->ged_gvp->gv_perspective > 0)
	(void)fprintf(fp, "-p%g ", gedp->ged_gvp->gv_perspective);
    for (i=2; i < argc; i++)
	(void)fprintf(fp, "%s ", argv[i]);

    if (strncmp(rtcmd, "nirt", 4) != 0)
	(void)fprintf(fp, "\\\n -o %s\\\n $*\\\n", outpix);

    if (inputg[0] == '\0') {
	snprintf(inputg, 255, "%s", gedp->ged_wdbp->dbip->dbi_filename);
    }
    (void)fprintf(fp, " %s\\\n ", inputg);

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);
	(void)fprintf(fp, "'%s' ", bu_vls_addr(&gdlp->gdl_path));
	gdlp = next_gdlp;
    }

    (void)fprintf(fp, "\\\n 2>> %s\\\n", outlog);
    (void)fprintf(fp, " <<EOF\n");

    {
	vect_t eye_model;

	_ged_rt_set_eye_model(gedp, eye_model);
	_ged_rt_write(gedp, fp, eye_model);
    }

    (void)fprintf(fp, "\nEOF\n");
    (void)fclose(fp);

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
