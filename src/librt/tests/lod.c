/*                           L O D . C
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

#include "vmath.h"
#include "bu/app.h"
#include "bu/time.h"
#include "bu/units.h"
#include "bg.h"
#include "bsg/geometry.h"
#include "bsg/lod.h"
#include "bsg/node.h"
#include "bsg/util.h"
#include "raytrace.h"

int
main(int argc, char *argv[])
{
    int64_t start, elapsed;
    fastf_t seconds;
    struct db_i *dbip;
    struct directory *dp;

    bu_setprogname(argv[0]);

    if (argc != 3) {
	bu_exit(1, "Usage: %s file.g [object]", argv[0]);
    }

    start = bu_gettime();

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    db_update_nref(dbip);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    // Unpack bot
    struct rt_db_internal intern;
    mat_t s_mat;
    MAT_IDN(s_mat);
    if (rt_db_get_internal(&intern, dp, dbip, s_mat) < 0)
	bu_exit(1, "ERROR: %s internal get failed\n", argv[2]);

    struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (!bot->num_faces)
	bu_exit(1, "ERROR: %s - no faces found\n", argv[2]);

    struct bsg_mesh_lod_context *c = bsg_mesh_lod_context_create(argv[1]);

    unsigned long long key = bsg_mesh_lod_cache(c, (const point_t *)bot->vertices, bot->num_vertices, NULL, bot->faces, bot->num_faces, 0, 0.66);
    if (!key)
	bu_exit(1, "ERROR: %s - lod creation failed\n", argv[2]);

    struct bsg_mesh_lod *mlod = bsg_mesh_lod_create(c, key);
    if (!mlod)
	bu_exit(1, "ERROR: %s - lod creation failed\n", argv[2]);

    struct bsg_view *v;
    BU_GET(v, struct bsg_view);
    bsg_view_init(v, NULL);

    bsg_mesh_ref mesh = bsg_mesh_ref_create(v, argv[2]);
    if (!bsg_mesh_ref_set_lod(mesh, mlod))
	bu_exit(1, "ERROR: %s - mesh LoD binding failed\n", argv[2]);
    bsg_node_ref mesh_node = bsg_mesh_ref_as_node(mesh);

    // TODO Set up initial view

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("Initialization time: %f seconds\n", seconds);

    start = bu_gettime();

    int coarse_fcnt = -1;
    int prev_fcnt = -1;
    int saw_face_increase = 0;
    int saw_snapped_points = 0;
    uint64_t start_revision = bsg_geometry_ref_revision(mesh.geometry);

    for (int i = 0; i < 16; i++) {
	int level = bsg_mesh_lod_level_node_ref(mesh_node, i, 0);
	if (level != i)
	    bu_exit(1, "ERROR: requested LoD level %d, got %d\n", i, level);

	if (mlod->fcnt < 0 || (size_t)mlod->fcnt > bot->num_faces)
	    bu_exit(1, "ERROR: LoD level %d reports invalid face count %d (full mesh has %zu)\n", i, mlod->fcnt, bot->num_faces);
	if (mlod->pcnt < 0 || mlod->porig_cnt < 0)
	    bu_exit(1, "ERROR: LoD level %d reports invalid point counts %d/%d\n", i, mlod->pcnt, mlod->porig_cnt);

	if (i == 0)
	    coarse_fcnt = mlod->fcnt;
	if (prev_fcnt >= 0 && mlod->fcnt > prev_fcnt)
	    saw_face_increase = 1;
	if (prev_fcnt >= 0 && mlod->fcnt < prev_fcnt)
	    bu_exit(1, "ERROR: LoD level %d has fewer faces than the previous finer step baseline (%d < %d)\n", i, mlod->fcnt, prev_fcnt);
	prev_fcnt = mlod->fcnt;

	if (!saw_snapped_points && mlod->points && mlod->points_orig) {
	    int pmax = (mlod->pcnt < mlod->porig_cnt) ? mlod->pcnt : mlod->porig_cnt;
	    for (int p = 0; p < pmax; p++) {
		if (!VNEAR_EQUAL(mlod->points[p], mlod->points_orig[p], SQRT_SMALL_FASTF)) {
		    saw_snapped_points = 1;
		    break;
		}
	    }
	}
    }

    if (bot->num_faces > 100) {
	if ((size_t)coarse_fcnt >= bot->num_faces)
	    bu_exit(1, "ERROR: coarse LoD did not reduce face count (%d >= %zu)\n", coarse_fcnt, bot->num_faces);
	if (!saw_face_increase)
	    bu_exit(1, "ERROR: finer LoD levels did not add geometric information\n");
	if (!saw_snapped_points)
	    bu_exit(1, "ERROR: POP LoD levels did not snap points to quantized positions\n");
    }
    if (bsg_geometry_ref_revision(mesh.geometry) <= start_revision)
	bu_exit(1, "ERROR: mesh LoD geometry revision did not change while levels changed\n");

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;
    bu_log("lod level setting: %f sec\n", seconds);

    bsg_node_ref_destroy(mesh_node);
    bsg_view_free(v);
    BU_PUT(v, struct bsg_view);
    bsg_mesh_lod_context_destroy(c);

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
