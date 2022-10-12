/*                           D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file draw.c
 *
 * Testing routines for new drawing logic
 *
 */

#include "common.h"

#include <stdio.h>
#include <bu.h>
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

void
img_cmp(int id, const char *cdir)
{
    icv_image_t *ctrl, *timg;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tname, "v%03d.png", id);
    bu_vls_sprintf(&cname, "%s/v%03d_ctrl.png", cdir, id);

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_AUTO, 0, 0);
    if (!timg)
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&tname));
    ctrl = icv_read(bu_vls_cstr(&cname), BU_MIME_IMAGE_AUTO, 0, 0);
    if (!ctrl)
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&cname));
    if (icv_diff(NULL, NULL, NULL, ctrl,timg))
	bu_exit(EXIT_FAILURE, "Default 35/25 moss all.g wireframe diff failed\n");
    icv_destroy(ctrl);
    icv_destroy(timg);
    bu_vls_free(&tname);
    bu_vls_free(&cname);
}

int
main(int ac, char *av[]) {
    struct ged *dbp;

    bu_setprogname(av[0]);

    if (ac != 3) {
	printf("Usage: %s file.g ctrl_img_dir\n", av[0]);
	return 1;
    }

    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    if (!bu_file_directory(av[2])) {
	printf("ERROR: [%s] is not a directory.  Expecting control image directory\n", av[2]);
	return 2;
    }

    /* Enable all the experimental logic */
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    bu_setenv("LIBGED_DBI_STATE", "1", 1);

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);


    dbp = ged_open("db", av[1], 1);
    BU_GET(dbp->ged_gvp, struct bview);
    bv_init(dbp->ged_gvp, &dbp->ged_views);
    bu_vls_sprintf(&dbp->ged_gvp->gv_name, "default");
    bv_set_add_view(&dbp->ged_views, dbp->ged_gvp);
    struct bview *v = dbp->ged_gvp;

    /* To generate images that will allow us to check if the drawing
     * is proceeding as expected, we use the swrast off-screen dm. */
    const char *s_av[5] = {NULL, NULL, NULL, NULL, NULL};

    s_av[0] = "dm";
    s_av[1] = "attach";
    s_av[2] = "swrast";
    s_av[3] = "SW";
    ged_exec(dbp, 4, s_av);

    struct dm *dmp = (struct dm *)dbp->ged_gvp->dmp;
    dm_set_width(dmp, 512);
    dm_set_height(dmp, 512);

    s_av[0] = "draw";
    s_av[1] = "all.g";
    ged_exec(dbp, 2, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    ged_exec(dbp, 3, s_av);

    dm_draw_objs(v, v->gv_base2local, v->gv_local2base, NULL, NULL);

    s_av[0] = "screengrab";
    s_av[1] = "v001.png";
    ged_exec(dbp, 2, s_av);

    img_cmp(1, av[2]);

    ged_close(dbp);

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
