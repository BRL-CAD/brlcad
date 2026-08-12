/*                 M E T A B A L L _ A D J U S T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"


static size_t
point_count(const struct rt_metaball_internal *mb)
{
    const struct wdb_metaball_pnt *point;
    size_t count = 0;

    for (BU_LIST_FOR(point, wdb_metaball_pnt, &mb->metaball_ctrl_head))
	count++;

    return count;
}


static void
clear_points(struct rt_metaball_internal *mb)
{
    struct wdb_metaball_pnt *point;

    while (BU_LIST_WHILE(point, wdb_metaball_pnt, &mb->metaball_ctrl_head)) {
	BU_LIST_DEQUEUE(&point->l);
	BU_PUT(point, struct wdb_metaball_pnt);
    }
}


int
main(int argc, char **argv)
{
    struct rt_db_internal intern;
    struct rt_metaball_internal mb = {0};
    struct bu_vls log = BU_VLS_INIT_ZERO;
    point_t initial_point = {1.0, 2.0, 3.0};
    const char *odd_args[] = {"1", "1", "1"};
    const char *invalid_args[] = {"method", "2", "thresh", "3.5", "PL", "{9 8}"};
    const char *valid_args[] = {"method", "2", "thresh", "3.5", "PL", "{9 8 7 6 5} {4 3 2 1 0.5}"};
    struct wdb_metaball_pnt *point;
    int ret = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	bu_exit(1, "Usage: %s\n", argv[0]);

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_METABALL;
    intern.idb_meth = &OBJ[ID_METABALL];
    intern.idb_ptr = &mb;

    mb.magic = RT_METABALL_INTERNAL_MAGIC;
    mb.method = 1;
    mb.threshold = 1.0;
    BU_LIST_INIT(&mb.metaball_ctrl_head);
    rt_metaball_add_point(&mb, &initial_point, 4.0, 5.0);

    if (OBJ[ID_METABALL].ft_adjust(&log, &intern, 3, odd_args) != BRLCAD_ERROR ||
	point_count(&mb) != 1) {
	bu_log("Odd argument count modified the metaball\n");
	ret = 1;
    }

    bu_vls_trunc(&log, 0);
    if (OBJ[ID_METABALL].ft_adjust(&log, &intern, 6, invalid_args) != BRLCAD_ERROR ||
	mb.method != 1 || !NEAR_EQUAL(mb.threshold, 1.0, SMALL_FASTF) ||
	point_count(&mb) != 1) {
	bu_log("Invalid point list partially modified the metaball\n");
	ret = 1;
    }

    point = BU_LIST_FIRST(wdb_metaball_pnt, &mb.metaball_ctrl_head);
    if (!VNEAR_EQUAL(point->coord, initial_point, SMALL_FASTF) ||
	!NEAR_EQUAL(point->field_strength, 4.0, SMALL_FASTF) ||
	!NEAR_EQUAL(point->blobbiness, 5.0, SMALL_FASTF)) {
	bu_log("Invalid adjustment changed the original control point\n");
	ret = 1;
    }

    bu_vls_trunc(&log, 0);
    if (OBJ[ID_METABALL].ft_adjust(&log, &intern, 6, valid_args) != BRLCAD_OK ||
	mb.method != 2 || !NEAR_EQUAL(mb.threshold, 3.5, SMALL_FASTF) ||
	point_count(&mb) != 2) {
	bu_log("Valid metaball adjustment failed: %s\n", bu_vls_cstr(&log));
	ret = 1;
    }

    clear_points(&mb);
    bu_vls_free(&log);
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
