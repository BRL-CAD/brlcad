/*                         E D C O L O R . C
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
/** @file edcolor.c
 *
 * The edcolor command.
 *
 */

#include "common.h"
#include "bio.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "mater.h"
#include "ged_private.h"

int
ged_edcolor(struct ged *gedp, int argc, const char *argv[])
{
    register struct mater *mp;
    register struct mater *zot;
    register FILE *fp;
    char line[128];
    static char hdr[] = "LOW\tHIGH\tRed\tGreen\tBlue\n";
    char tmpfil[MAXPATHLEN];
    static const char *usage = "";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    if (argc != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    fp = bu_temp_file(tmpfil, MAXPATHLEN);
    if (fp == NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: could not create tmp file", argv[0]);
	return BRLCAD_ERROR;
    }

    fprintf( fp, "%s", hdr );
    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
	(void)fprintf( fp, "%d\t%d\t%3d\t%3d\t%3d",
		       mp->mt_low, mp->mt_high,
		       mp->mt_r, mp->mt_g, mp->mt_b);
	(void)fprintf(fp, "\n");
    }
    (void)fclose(fp);

    if (!ged_editit( tmpfil)) {
	bu_vls_printf(&gedp->ged_result_str, "%s: editor returned bad status. Aborted\n", argv[0]);
	return BRLCAD_ERROR;
    }

    /* Read file and process it */
    if ((fp = fopen( tmpfil, "r")) == NULL) {
	perror(tmpfil);
	return BRLCAD_ERROR;
    }

    if (bu_fgets(line, sizeof (line), fp) == NULL ||
	line[0] != hdr[0]) {
	bu_vls_printf(&gedp->ged_result_str, "%s: Header line damaged, aborting\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (gedp->ged_wdbp->dbip->dbi_version < 5) {
	/* Zap all the current records, both in core and on disk */
	while (rt_material_head() != MATER_NULL) {
	    zot = rt_material_head();
	    rt_new_material_head(zot->mt_forw);
	    ged_color_zaprec(gedp, zot);
	    bu_free((genptr_t)zot, "mater rec");
	}

	while (bu_fgets(line, sizeof (line), fp) != NULL) {
	    int cnt;
	    int low, hi, r, g, b;

	    cnt = sscanf(line, "%d %d %d %d %d",
			 &low, &hi, &r, &g, &b);
	    if (cnt != 5) {
		bu_vls_printf(&gedp->ged_result_str, "%s: Discarding %s\n", argv[0], line);
		continue;
	    }
	    BU_GETSTRUCT(mp, mater);
	    mp->mt_low = low;
	    mp->mt_high = hi;
	    mp->mt_r = r;
	    mp->mt_g = g;
	    mp->mt_b = b;
	    mp->mt_daddr = MATER_NO_ADDR;
	    rt_insert_color(mp);
	    ged_color_putrec(gedp, mp);
	}
    } else {
	struct bu_vls vls;

	/* free colors in rt_material_head */
	rt_color_free();

	bu_vls_init(&vls);

	while (bu_fgets(line, sizeof (line), fp) != NULL) {
	    int cnt;
	    int low, hi, r, g, b;

	    /* check to see if line is reasonable */
	    cnt = sscanf(line, "%d %d %d %d %d",
			 &low, &hi, &r, &g, &b);
	    if (cnt != 5) {
		bu_vls_printf(&gedp->ged_result_str, "%s: Discarding %s\n", argv[0], line);
		continue;
	    }
	    bu_vls_printf(&vls, "{%d %d %d %d %d} ", low, hi, r, g, b);
	}

	db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&vls), gedp->ged_wdbp->dbip);
	db5_import_color_table(bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    (void)fclose(fp);
    (void)unlink(tmpfil);

    ged_color_soltab((struct solid *)&gedp->ged_gdp->gd_headSolid);

    return BRLCAD_OK;
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
