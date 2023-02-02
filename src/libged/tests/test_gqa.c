/*                     T E S T _ G Q A . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2023 United States Government as represented by
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
/** @file test_gqa.c
 *
 * Extract plotting data from a gqa run
 *
 */

#include "common.h"

#include <stdio.h>
#include <bu.h>
#include <bv.h>
#include <ged.h>

int
main(int ac, char *av[]) {
    struct ged *gedp;
    const char *gqa_plot_fname = "gqa_ovlps.plot3";
    const char *gqa[4] = {"gqa", "-Aop", "ovlp", NULL};

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    gedp = ged_open("db", av[1], 1);
    ged_exec(gedp, 3, gqa);
    printf("%s\n", bu_vls_cstr(gedp->ged_result_str));

    // Example of programmatically extracting the resulting plot data (assuming
    // we're only after ffff00 colored data)
    //
    // (TODO - this will need to be redone if/when the new drawing setup takes
    // over - there (at least for now) we would do a bv_find_obj on the view
    // with the gqa overlap view object's name (gqa:overlaps) to find the
    // bv_scene_obj, and then iterate over that object's child objects to get
    // the different colored vlists...)
    struct display_list *gdlp;
    struct bv_scene_obj *vdata = NULL;
    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
	if (!BU_STR_EQUAL(bu_vls_cstr(&gdlp->dl_path), "OVERLAPSffff00"))
	    continue;
	printf("found %s;\n", bu_vls_cstr(&gdlp->dl_path));
	vdata = BU_LIST_NEXT(bv_scene_obj, &gdlp->dl_head_scene_obj);
	break;
    }

    if (vdata) {
	FILE *fp;
	fp = fopen(gqa_plot_fname, "wb");
	if (!fp)
	    bu_exit(EXIT_FAILURE, "Could not open %s for writing\n", gqa_plot_fname);
	printf("Writing plot data to %s for inspection with overlay command\n", gqa_plot_fname);
	bv_vlist_to_uplot(fp, &vdata->s_vlist);
	fclose(fp);
    } else {
	bu_exit(EXIT_FAILURE, "No GQA plotting data found.\n");
    }

    ged_close(gedp);

    return 0;
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
