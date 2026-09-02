/*                 A N N O T A T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <math.h>
#include <stdio.h>

#include "bu/app.h"
#include "bu/file.h"
#include "raytrace.h"
#include "rt/primitives/annot.h"
#include "wdb.h"

#include "load_g.h"
#include "librender/camera.h"


static void
write_test_database(struct db_i *dbip)
{
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    struct rt_annot_internal annot = {0};
    point2d_t vertices[2];
    struct line_seg line = {0};
    void *segments[1];
    int reverse[1] = {0};
    point_t center = VINIT_ZERO;
    struct wmember members;

    if (wdbp == RT_WDB_NULL)
	bu_exit(1, "unable to open ADRT annotation test database\n");
    annot.magic = RT_ANNOT_INTERNAL_MAGIC;
    VSET(annot.V, 10.0, 0.0, 0.0);
    annot.flags = RT_ANNOT_MODEL_SPACE;
    VSET(annot.u_vec, 0.0, 1.0, 0.0);
    VSET(annot.v_vec, 0.0, 0.0, 1.0);
    annot.vert_count = 2;
    annot.verts = vertices;
    V2SET(vertices[0], -1.0, 0.0);
    V2SET(vertices[1], 1.0, 0.0);
    annot.ant.count = 1;
    annot.ant.reverse = reverse;
    annot.ant.segments = segments;
    line.magic = CURVE_LSEG_MAGIC;
    line.start = 0;
    line.end = 1;
    segments[0] = &line;
    if (rt_annot_validate(&annot, NULL) ||
	    mk_annot(wdbp, "test.annot", &annot) < 0 ||
	    mk_sph(wdbp, "test.sph", center, 1.0) < 0)
	bu_exit(1, "unable to write ADRT annotation test objects\n");

    BU_LIST_INIT(&members.l);
    if (!mk_addmember("test.annot", &members.l, NULL, WMOP_UNION) ||
	    mk_lcomb(wdbp, "annotation-only.c", &members, 0, NULL, NULL,
		NULL, 0))
	bu_exit(1, "unable to write annotation-only test combination\n");

    BU_LIST_INIT(&members.l);
    if (!mk_addmember("test.annot", &members.l, NULL, WMOP_UNION) ||
	    !mk_addmember("test.sph", &members.l, NULL, WMOP_UNION) ||
	    mk_lcomb(wdbp, "mixed.c", &members, 0, NULL, NULL, NULL, 0))
	bu_exit(1, "unable to write mixed test combination\n");
}


static void
check_geometry_filter(struct db_i *dbip)
{
    struct rt_db_internal intern;
    union tree *leaf;

    if (adrt_path_has_geometry(dbip, "test.annot") ||
	    adrt_path_has_geometry(dbip, "annotation-only.c") ||
	    !adrt_path_has_geometry(dbip, "test.sph") ||
	    !adrt_path_has_geometry(dbip, "mixed.c"))
	bu_exit(1, "ADRT annotation geometry filtering failed\n");

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_type = ID_ANNOT;
    leaf = adrt_leaf_tess(NULL, NULL, &intern, NULL);
    if (!leaf || leaf->tr_op != OP_NOP)
	bu_exit(1, "ADRT annotation leaf did not become a no-op\n");
    db_free_tree(leaf);
}


static void
check_camera_fit(struct db_i *dbip)
{
    const char *paths[] = {"test.annot"};
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_annot_scene *scene = rt_annot_scene_create(dbip, 1, paths,
	&ttol, &tol);
    render_camera_t camera = {0};
    struct tie_s tie = {0};
    fastf_t radius;
    const fastf_t combined_radius = 0.5 * sqrt(41.0);

    if (!scene || rt_annot_scene_count(scene) != 1)
	bu_exit(1, "unable to prepare ADRT annotation test scene\n");
    tie.radius = 2.0;
    VSET(tie.mid, 1.0, 2.0, 3.0);
    radius = render_camera_fit_scene(&camera, &tie, NULL, 1.0);
    if (!NEAR_EQUAL(radius, 2.0, BN_TOL_DIST) ||
	    !VNEAR_EQUAL(camera.focus, tie.mid, BN_TOL_DIST) ||
	    !NEAR_EQUAL(camera.pos[X], 2.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(camera.pos[Y], 2.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(camera.pos[Z], 2.0, BN_TOL_DIST))
	bu_exit(1, "ADRT geometry-only camera framing changed\n");

    radius = render_camera_fit_scene(&camera, &tie, scene, 10.0);
    if (radius <= SMALL_FASTF ||
	    !NEAR_EQUAL(camera.focus[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_ZERO(camera.focus[Y], BN_TOL_DIST) ||
	    !NEAR_ZERO(camera.focus[Z], BN_TOL_DIST) ||
	    !NEAR_EQUAL(camera.pos[X] - camera.focus[X], radius, BN_TOL_DIST) ||
	    !NEAR_EQUAL(camera.pos[Y] - camera.focus[Y], radius, BN_TOL_DIST) ||
	    !NEAR_EQUAL(camera.pos[Z] - camera.focus[Z], radius, BN_TOL_DIST))
	bu_exit(1, "ADRT annotation-only camera framing failed: radius=%g "
	    "focus=(%g %g %g) pos=(%g %g %g)\n", radius,
	    V3ARGS(camera.focus), V3ARGS(camera.pos));

    tie.tri_num = 1;
    VSET(tie.amin, -2.0, -2.0, -2.0);
    VSET(tie.amax, 0.0, 2.0, 2.0);
    radius = render_camera_fit_scene(&camera, &tie, scene, 10.0);
    if (!NEAR_EQUAL(radius, combined_radius, BN_TOL_DIST) ||
	    !NEAR_EQUAL(camera.focus[X], -0.5, BN_TOL_DIST) ||
	    !NEAR_ZERO(camera.focus[Y], BN_TOL_DIST) ||
	    !NEAR_ZERO(camera.focus[Z], BN_TOL_DIST))
	bu_exit(1, "ADRT combined geometry and annotation framing failed\n");
    rt_annot_scene_destroy(scene);
}


int
main(int argc, char **argv)
{
    char path[MAXPATHLEN];
    struct db_i *dbip;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    {
	FILE *fp = bu_temp_file(path, MAXPATHLEN);
	if (!fp)
	    bu_exit(1, "unable to create ADRT annotation test path\n");
	fclose(fp);
    }
    dbip = db_create(path, 5);
    if (dbip == DBI_NULL)
	bu_exit(1, "unable to create ADRT annotation test database\n");
    write_test_database(dbip);
    check_geometry_filter(dbip);
    check_camera_fit(dbip);
    db_close(dbip);
    bu_file_delete(path);
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
