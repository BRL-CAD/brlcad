/*                        R M A T E R . C
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
/** @file libged/rmater.c
 *
 * The rmater command.
 *
 */

#include "ged.h"


static int
extract_mater_from_line(char *line,
			char *name,
			char *shader,
			int *r,
			int *g,
			int *b,
			int *override,
			int *inherit)
{
    int i, j, k;
    char *str[3];

    str[0] = name;
    str[1] = shader;

    /* Extract first 2 strings. */
    for (i=j=0; i < 2; ++i) {

	/* skip white space */
	while (line[j] == ' ' || line[j] == '\t')
	    ++j;

	if (line[j] == '\0')
	    return TCL_ERROR;

	/* We found a double quote, so use everything between the quotes */
	if (line[j] == '"') {
	    for (k = 0, ++j; line[j] != '"' && line[j] != '\0'; ++j, ++k)
		str[i][k] = line[j];
	} else {
	    for (k = 0; line[j] != ' ' && line[j] != '\t' && line[j] != '\0'; ++j, ++k)
		str[i][k] = line[j];
	}

	if (line[j] == '\0')
	    return TCL_ERROR;

	str[i][k] = '\0';
	++j;
    }

    /* character and/or whitespace deliminted numbers */
    if ((sscanf(line + j, "%d%*c%d%*c%d%*c%d%*c%d", r, g, b, override, inherit)) != 5)
	return TCL_ERROR;

    return TCL_OK;
}


int
ged_rmater(struct ged *gedp, int argc, const char *argv[])
{
#ifndef LINELEN
#define LINELEN 256
#endif
    int status = GED_OK;
    FILE *fp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    char line[LINELEN];
    char name[128];
    char shader[256];
    int r, g, b;
    int override;
    int inherit;
    static const char *usage = "filename";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((fp = fopen(argv[1], "r")) == NULL) {
	bu_vls_printf(gedp->ged_result_str, "ged_rmater: Failed to read file - %s", argv[1]);
	return GED_ERROR;
    }

    while (bu_fgets(line, LINELEN, fp) != NULL) {
	if ((extract_mater_from_line(line, name, shader,
				     &r, &g, &b, &override, &inherit)) == GED_ERROR)
	    continue;

	if ((dp = db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "ged_rmater: Failed to find %s\n", name);
	    status = GED_ERROR;
	    continue;
	}

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting\n");
	    status = GED_ERROR;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	/* Assign new values */
	if (shader[0] == '-')
	    bu_vls_free(&comb->shader);
	else
	    bu_vls_strcpy(&comb->shader, shader);

	comb->rgb[0] = (unsigned char)r;
	comb->rgb[1] = (unsigned char)g;
	comb->rgb[2] = (unsigned char)b;
	comb->rgb_valid = override;
	comb->inherit = inherit;

	/* Write new values to database */
	if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database write error, aborting\n");
	    status = GED_ERROR;
	}
    }

    (void)fclose(fp);
    return status;
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
