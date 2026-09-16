/*                 T E S T _ R T A R E A _ D B . C
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

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "vmath.h"
#include "wdb.h"


static int
make_region(struct rt_wdb *wdbp, const char *region_name, const char *solid_name)
{
    return mk_region1(wdbp, region_name, solid_name, NULL, NULL, NULL);
}


static int
make_group(struct rt_wdb *wdbp, const char *group_name, const char *first_region, const char *second_region)
{
    struct wmember members;

    BU_LIST_INIT(&members.l);
    if (!mk_addmember(first_region, &members.l, NULL, WMOP_UNION) ||
	!mk_addmember(second_region, &members.l, NULL, WMOP_UNION)) {
	mk_freemembers(&members.l);
	return 1;
    }

    return mk_lcomb(wdbp, group_name, &members, 0, NULL, NULL, NULL, 0);
}


int
main(int argc, char **argv)
{
    const fastf_t sphere_radius = 50.0;
    const fastf_t cube_half_width = 50.0;
    const fastf_t occluded_sphere_offset = 60.0;
    struct rt_wdb *wdbp;
    point_t center = VINIT_ZERO;
    point_t minimum;
    point_t maximum;
    int failed = 0;

    bu_setprogname(argv[0]);
    if (argc != 2) {
	bu_log("Usage: %s output.g\n", argv[0]);
	return 1;
    }

    wdbp = wdb_fopen(argv[1]);
    if (!wdbp) {
	bu_log("Unable to create %s\n", argv[1]);
	return 1;
    }

    failed |= mk_id_units(wdbp, "rtarea analytic regression geometry", "mm");

    failed |= mk_sph(wdbp, "sphere.s", center, sphere_radius);
    failed |= make_region(wdbp, "sphere.r", "sphere.s");

    VSETALL(minimum, -cube_half_width);
    VSETALL(maximum, cube_half_width);
    failed |= mk_rpp(wdbp, "cube.s", minimum, maximum);
    failed |= make_region(wdbp, "cube.r", "cube.s");

    VSET(center, occluded_sphere_offset, 0.0, 0.0);
    failed |= mk_sph(wdbp, "front.s", center, sphere_radius);
    failed |= make_region(wdbp, "front.r", "front.s");

    VSET(center, -occluded_sphere_offset, 0.0, 0.0);
    failed |= mk_sph(wdbp, "back.s", center, sphere_radius);
    failed |= make_region(wdbp, "back.r", "back.s");
    failed |= make_group(wdbp, "occluded.g", "front.r", "back.r");

    wdb_close(wdbp);
    if (failed)
	bu_log("Failed to create rtarea analytic regression geometry\n");

    return failed ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
