/*                         E D C O D E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file edcodes.c
 *
 * The edcodes command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged_private.h"

#define ABORTED		-99

static int ged_id_compare(const void *p1, const void *p2);
static int ged_reg_compare(const void *p1, const void *p2);

static int regflag;
static int lastmemb;
static char tmpfil[MAXPATHLEN] = {0};

int
ged_edcodes(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status;
    int sort_by_ident=0;
    int sort_by_region=0;
    int c;
    char **av;
    FILE *fp = NULL;

    static const char *usage = "object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "ir")) != EOF) {
	switch( c ) {
	    case 'i':
		sort_by_ident = 1;
		break;
	    case 'r':
		sort_by_region = 1;
		break;
	}
    }

    if ((sort_by_ident + sort_by_region) > 1) {
	bu_vls_printf(&gedp->ged_result_str, "%s: can only sort by region or ident, not both\n", argv[0]);
	return BRLCAD_ERROR;
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (!fp)
	return BRLCAD_ERROR;

    av = (char **)bu_malloc(sizeof(char *)*(argc + 2), "ged_edcodes: av");
    av[0] = "wcodes";
    av[1] = tmpfil;
    for (i = 2; i < argc + 1; ++i)
	av[i] = (char *)argv[i-1];

    av[i] = NULL;

    if (ged_wcodes(gedp, argc + 1, (const char **)av) == BRLCAD_ERROR) {
	(void)unlink(tmpfil);
	bu_free((genptr_t)av, "ged_edcodes: av");
	return BRLCAD_ERROR;
    }

    (void)fclose(fp);

    if (regflag == ABORTED) {
	bu_vls_printf(&gedp->ged_result_str, "%s: nesting is too deep\n", argv[0]);
	(void)unlink(tmpfil);
	return BRLCAD_ERROR;
    }

    if (sort_by_ident || sort_by_region) {
	char **line_array;
	char aline[256];
	FILE *f_srt;
	int line_count=0;
	int j;

	if ((f_srt=fopen(tmpfil, "r+" )) == NULL) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: Failed to open temp file for sorting\n", argv[0]);
	    (void)unlink(tmpfil);
	    return BRLCAD_ERROR;
	}

	/* count lines */
	while (bu_fgets(aline, 256, f_srt )) {
	    line_count++;
	}

	/* build array of lines */
	line_array = (char **)bu_calloc(line_count, sizeof(char *), "edcodes line array");

	/* read lines and save into the array */
	rewind(f_srt);
	line_count = 0;
	while (bu_fgets(aline, 256, f_srt)) {
	    line_array[line_count] = bu_strdup(aline);
	    line_count++;
	}

	/* sort the array of lines */
	if (sort_by_ident) {
	    qsort(line_array, line_count, sizeof( char *), ged_id_compare);
	} else {
	    qsort(line_array, line_count, sizeof( char *), ged_reg_compare);
	}

	/* rewrite the temp file using the sorted lines */
	rewind(f_srt);
	for (j=0; j<line_count; j++) {
	    fprintf(f_srt, "%s", line_array[j]);
	    bu_free(line_array[j], "ged_edcodes line array element");
	}
	bu_free((char *)line_array, "ged_edcodes line array");
	fclose(f_srt);
    }

    if (ged_editit(tmpfil)) {
	regflag = lastmemb = 0;
	av[0] = "rcodes";
	av[2] = NULL;
	status = ged_rcodes(gedp, 2, (const char **)av);
    } else
	status = BRLCAD_ERROR;

    unlink(tmpfil);
    bu_free((genptr_t)av, "ged_edcodes: av");
    return status;
}

static int
ged_id_compare(const void *p1, const void *p2)
{
    int id1, id2;

    id1 = atoi(*(char **)p1);
    id2 = atoi(*(char **)p2);

    return (id1 - id2);
}

static int
ged_reg_compare(const void *p1, const void *p2)
{
    char *reg1, *reg2;

    reg1 = strchr(*(char **)p1, '/');
    reg2 = strchr(*(char **)p2, '/');

    return strcmp(reg1, reg2);
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
