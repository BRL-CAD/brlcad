/*              C A D _ U S E R _ A G G R E G A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file cad_user_aggregate.c
 *
 * Code using BRL-CAD's aggregate library to simulate an external user
 * of an installed BRL-CAD.  This intentionally uses public headers and
 * links only to the aggregate brlcad library.
 */

#include <common.h>

#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "ged.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "vmath.h"
#include "wdb.h"


static int
fail(const char *msg)
{
    bu_log("FAIL: %s\n", msg);
    return 1;
}


static int
hit(struct application *UNUSED(ap), struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    struct partition *pp;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	if (pp->pt_inhit && pp->pt_outhit && pp->pt_inhit->hit_dist < pp->pt_outhit->hit_dist)
	    return 1;
    }

    return 0;
}


static int
miss(struct application *UNUSED(ap))
{
    return 0;
}


static int
make_model(const char *gfile)
{
    struct rt_wdb *wdbp;
    struct wmember wm;
    point_t center = VINIT_ZERO;

    wdbp = wdb_fopen(gfile);
    if (!wdbp)
	return fail("wdb_fopen failed");

    if (mk_sph(wdbp, "ball.s", center, 100.0) != 0) {
	wdb_close(wdbp);
	return fail("mk_sph failed");
    }

    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("ball.s", &wm.l, NULL, WMOP_UNION)) {
	wdb_close(wdbp);
	return fail("mk_addmember ball.s failed");
    }
    if (mk_lcomb(wdbp, "ball.r", &wm, 1, NULL, NULL, NULL, 0) != 0) {
	wdb_close(wdbp);
	return fail("mk_lcomb ball.r failed");
    }

    BU_LIST_INIT(&wm.l);
    if (!mk_addmember("ball.r", &wm.l, NULL, WMOP_UNION)) {
	wdb_close(wdbp);
	return fail("mk_addmember ball.r failed");
    }
    if (mk_lcomb(wdbp, "all.g", &wm, 0, NULL, NULL, NULL, 0) != 0) {
	wdb_close(wdbp);
	return fail("mk_lcomb all.g failed");
    }

    wdb_close(wdbp);
    return 0;
}


static int
check_ged(const char *gfile)
{
    struct ged *gedp;
    const char *tops_cmd[] = {"tops"};
    const char *search_cmd[] = {"search", "/"};

    gedp = ged_open("db", gfile, 0);
    if (!gedp)
	return fail("ged_open failed");

    if (ged_exec(gedp, 1, tops_cmd) != BRLCAD_OK) {
	ged_close(gedp);
	return fail("ged tops failed");
    }

    if (ged_exec(gedp, 2, search_cmd) != BRLCAD_OK) {
	ged_close(gedp);
	return fail("ged search failed");
    }

    ged_close(gedp);
    return 0;
}


static int
check_raytrace(const char *gfile)
{
    struct rt_i *rtip;
    struct application ap;
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    char title[1024] = {0};
    int ret;

    rtip = rt_dirbuild(gfile, title, sizeof(title));
    if (rtip == RTI_NULL)
	return fail("rt_dirbuild failed");

    if (rt_gettree(rtip, "all.g") < 0) {
	rt_clean(rtip);
	return fail("rt_gettree failed");
    }

    rt_init_resource(&resource, 0, rtip);
    rt_prep_parallel(rtip, 1);

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = &resource;
    ap.a_onehit = 1;
    ap.a_hit = hit;
    ap.a_miss = miss;
    VSET(ap.a_ray.r_pt, 0.0, 0.0, 500.0);
    VSET(ap.a_ray.r_dir, 0.0, 0.0, -1.0);

    ret = rt_shootray(&ap);

    rt_clean_resource_basic(rtip, &resource);
    rt_clean(rtip);

    if (ret != 1)
	return fail("rt_shootray did not hit all.g");

    return 0;
}


static int
check_gcv(const char *gfile, const char *objfile)
{
    struct gcv_context context;
    struct gcv_opts opts;
    const struct bu_ptbl *filters;
    const struct gcv_filter *in_filter;
    const struct gcv_filter *out_filter;
    const char *objects[] = {"all.g"};
    int ret = 0;

    gcv_context_init(&context);
    gcv_opts_default(&opts);
    opts.num_objects = 1;
    opts.object_names = objects;

    filters = gcv_list_filters(&context);
    if (!filters || BU_PTBL_LEN(filters) == 0) {
	ret = fail("gcv_list_filters found no filters");
	goto gcv_done;
    }

    in_filter = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, gfile, &context);
    out_filter = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_OBJ, objfile, &context);
    if (!in_filter) {
	ret = fail("gcv BRL-CAD binary reader filter not found");
	goto gcv_done;
    }
    if (!out_filter) {
	ret = fail("gcv OBJ writer filter not found");
	goto gcv_done;
    }

    if (!gcv_execute(&context, in_filter, &opts, 0, NULL, gfile)) {
	ret = fail("gcv BRL-CAD binary read failed");
	goto gcv_done;
    }

    if (!gcv_execute(&context, out_filter, &opts, 0, NULL, objfile)) {
	ret = fail("gcv OBJ write failed");
	goto gcv_done;
    }

    if (!bu_file_exists(objfile, NULL)) {
	ret = fail("gcv OBJ output file was not written");
	goto gcv_done;
    }

gcv_done:
    gcv_context_destroy(&context);
    return ret;
}


int
main(int ac, char *av[])
{
    char gfile[MAXPATHLEN] = {0};
    char objfile[MAXPATHLEN] = {0};
    size_t dir_len;

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s test_dir\n", av[0]);
	return 1;
    }

    dir_len = strlen(av[1]);
    if (dir_len + 32 >= MAXPATHLEN)
	return fail("test directory path is too long");

    snprintf(gfile, MAXPATHLEN, "%s/aggregate-user.g", av[1]);
    snprintf(objfile, MAXPATHLEN, "%s/aggregate-user.obj", av[1]);

    if (make_model(gfile) != 0)
	return 1;
    if (check_ged(gfile) != 0)
	return 1;
    if (check_raytrace(gfile) != 0)
	return 1;
    if (check_gcv(gfile, objfile) != 0)
	return 1;

    bu_log("Aggregate BRL-CAD library user simulation succeeded\n");
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
