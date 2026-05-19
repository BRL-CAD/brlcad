/*                  B V _ P O L Y _ S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bv.h"
#include "bg/plane.h"
#include "raytrace.h"
#include "rt/primitives/sketch.h"

#define TEST_DB_VERSION 5

static void
temp_g_path(char *ofile)
{
    FILE *fp = bu_temp_file(ofile, MAXPATHLEN);
    if (!fp)
	bu_exit(EXIT_FAILURE, "Failed to create temporary file\n");
    fclose(fp);
    bu_file_delete(ofile);
}

/* Compare model-space points using the standard BN distance tolerance. */
static int
points_within_tolerance(point_t a, point_t b)
{
    return NEAR_ZERO(DIST_PNT_PNT(a, b), BN_TOL_DIST);
}

static void
compare_scene_polygons(struct bv_scene_obj *orig, struct bv_scene_obj *rt, const char *msg)
{
    struct bv_polygon *op = (struct bv_polygon *)orig->s_i_data;
    struct bv_polygon *rp = (struct bv_polygon *)rt->s_i_data;

    if (op->polygon.num_contours != rp->polygon.num_contours)
	bu_exit(EXIT_FAILURE, "%s: contour count changed\n", msg);

    for (size_t i = 0; i < op->polygon.num_contours; i++) {
	size_t pcnt = op->polygon.contour[i].num_points;
	if (pcnt != rp->polygon.contour[i].num_points)
	    bu_exit(EXIT_FAILURE, "%s: contour point count changed\n", msg);

	int contour_match = 0;
	for (size_t offset = 0; offset < pcnt; offset++) {
	    int match = 1;
	    for (size_t j = 0; j < pcnt; j++) {
		size_t rj = (offset + j) % pcnt;
		if (!points_within_tolerance(op->polygon.contour[i].point[j], rp->polygon.contour[i].point[rj])) {
		    match = 0;
		    break;
		}
	    }
	    if (match) {
		contour_match = 1;
		break;
	    }

	    match = 1;
	    for (size_t j = 0; j < pcnt; j++) {
		size_t rj = (offset + pcnt - j) % pcnt;
		if (!points_within_tolerance(op->polygon.contour[i].point[j], rp->polygon.contour[i].point[rj])) {
		    match = 0;
		    break;
		}
	    }
	    if (match) {
		contour_match = 1;
		break;
	    }
	}

	if (!contour_match)
	    bu_exit(EXIT_FAILURE, "%s: contour %zu points moved\n", msg, i);
    }
}

static void
test_non_origin_plane_roundtrip(void)
{
    char ofile[MAXPATHLEN];
    temp_g_path(ofile);
    struct rt_wdb *wfp = wdb_fopen_v(ofile, TEST_DB_VERSION);
    if (!wfp)
	bu_exit(EXIT_FAILURE, "Failed to create output database %s\n", ofile);

    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);

    struct bv_polygon *p;
    BU_GET(p, struct bv_polygon);
    memset(p, 0, sizeof(struct bv_polygon));
    p->type = BV_POLYGON_GENERAL;
    p->polygon.num_contours = 1;
    p->polygon.hole = (int *)bu_calloc(1, sizeof(int), "gp_hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "gp_contour");
    p->polygon.contour[0].num_points = 3;
    p->polygon.contour[0].point = (point_t *)bu_calloc(3, sizeof(point_t), "gpc_point");

    VSET(p->polygon.contour[0].point[0], 11.0, 21.0, 30.0);
    VSET(p->polygon.contour[0].point[1], 15.0, 21.0, 30.0);
    VSET(p->polygon.contour[0].point[2], 11.0, 26.0, 30.0);

    /* Z=30 gives a non-zero plane origin, verifying exported axes are relative
     * to the sketch origin rather than world origin. */
    point_t plane_pt = {0.0, 0.0, 30.0};
    vect_t plane_n = {0.0, 0.0, 1.0};

    struct bv_scene_obj *pobj = bv_create_polygon_obj(v, 0, p);
    /* bv_create_polygon_obj initializes from the view; reset to the test plane. */
    bg_plane_pt_nrml(&p->vp, plane_pt, plane_n);
    bv_polygon_vlist(pobj);

    struct directory *odp = db_scene_obj_to_sketch(wfp->dbip, "roundtrip.s", pobj);
    if (odp == RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "Failed to write non-origin scene obj polygon to output database %s\n", ofile);

    struct bv_scene_obj *rtobj = db_sketch_to_scene_obj("roundtrip", wfp->dbip, odp, v, 0);
    if (!rtobj)
	bu_exit(EXIT_FAILURE, "Failed to recreate non-origin scene polygon from sketch\n");

    compare_scene_polygons(pobj, rtobj, "non-origin plane polygon roundtrip");

    db_close(wfp->dbip);
    bu_file_delete(ofile);
}

int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;

    bu_setprogname(argv[0]);

    if (argc != 2) {
	bu_exit(EXIT_FAILURE, "Usage: %s file.g", argv[0]);
    }

    // First, open the database and make sure we have poly.s

    dbip = db_open(argv[1], DB_OPEN_READONLY);
    if (dbip == DBI_NULL) {
	bu_exit(EXIT_FAILURE, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0) {
	bu_exit(EXIT_FAILURE, "ERROR: Unable to read from %s\n", argv[1]);
    }

    db_update_nref(dbip);

    dp = db_lookup(dbip, "poly.s", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "ERROR: Unable to look up object poly.s\n");

    // Create the view
    struct bview *v;
    BU_GET(v, struct bview);
    bv_init(v, NULL);

    struct bv_scene_obj *pobj = db_sketch_to_scene_obj("poly", dbip, dp, v, 0);

    if (!pobj)
	bu_exit(EXIT_FAILURE, "Failed to create scene object from poly.s\n");

    char ofile[MAXPATHLEN];
    temp_g_path(ofile);
    struct rt_wdb *wfp = wdb_fopen_v(ofile, TEST_DB_VERSION);
    if (!wfp)
	bu_exit(EXIT_FAILURE, "Failed to create output database %s\n", ofile);

    struct directory *odp = db_scene_obj_to_sketch(wfp->dbip, "poly.s", pobj);
    if (odp == RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "Failed to write scene obj polygon to output database %s\n", ofile);

    struct bv_scene_obj *opobj = db_sketch_to_scene_obj("poly_out", wfp->dbip, odp, v, 0);
    if (!opobj)
	bu_exit(EXIT_FAILURE, "Failed to create scene object from exported poly.s\n");

    compare_scene_polygons(pobj, opobj, "imported sketch polygon roundtrip");

    db_close(dbip);
    db_close(wfp->dbip);
    bu_file_delete(ofile);

    test_non_origin_plane_roundtrip();

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
