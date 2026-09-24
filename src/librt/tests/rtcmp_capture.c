/*                    R T C M P _ C A P T U R E . C
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

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/log.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "raytrace.h"
#include "wdb.h"


#define TEST_FLUSH_WAIT_SECONDS 3


static int
shot_hit(struct application *UNUSED(ap), struct partition *parts, struct seg *UNUSED(segs))
{
    return parts->pt_forw != parts;
}


static int
shot_miss(struct application *UNUSED(ap))
{
    return 0;
}


static int
shoot(struct application *ap, fastf_t y)
{
    VSET(ap->a_ray.r_pt, -2.0, y, 0.0);
    VSET(ap->a_ray.r_dir, 1.0, 0.0, 0.0);
    return rt_shootray(ap);
}


int
main(int UNUSED(argc), const char **argv)
{
    char path[MAXPATHLEN];
    char line[BUFSIZ];
    struct db_i *dbip = NULL;
    struct rt_wdb *wdbp;
    struct rt_i *rtip = NULL;
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    struct application ap;
    struct wmember members;
    point_t center = VINIT_ZERO;
    FILE *file = NULL;
    int failures = 0;
    int records = 0;

    bu_setprogname(argv[0]);
    file = bu_temp_file(path, sizeof(path));
    if (!file) return 1;
    if (fputs("{}\n", file) == EOF || fclose(file) != 0) return 1;
    file = NULL;
    if (bu_setenv("LIBRT_RTCMP_FILE", path, 1) != 0) return 1;

    dbip = db_create_inmem();
    if (!dbip) return 1;
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp || mk_sph(wdbp, "sphere.s", center, 1.0)) return 1;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("sphere.s", &members.l, NULL, WMOP_UNION) ||
	mk_lcomb(wdbp, "sphere.r", &members, 1, NULL, NULL, NULL, 0)) return 1;

    rtip = rt_i_create(dbip);
    if (!rtip || rt_gettree(rtip, "sphere.r")) return 1;
    rt_prep(rtip);
    rt_init_resource(&resource, 0, rtip);
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = &resource;
    ap.a_hit = shot_hit;
    ap.a_miss = shot_miss;

    rt_debug = RT_DEBUG_RTCMP;
    if (shoot(&ap, 0.0) != 1 || shoot(&ap, 2.0) != 0 ||
	shoot(&ap, 0.5) != 1) failures++;
    rt_debug = 0;

    if (bu_snooze(BU_SEC2USEC(TEST_FLUSH_WAIT_SECONDS)) != 0) failures++;
    file = fopen(path, "rb");
    if (!file) {
	failures++;
    } else {
	if (!bu_fgets(line, sizeof(line), file) || bu_strcmp(line, "{}\n")) failures++;
	while (bu_fgets(line, sizeof(line), file)) {
	    if (!strstr(line, "\"partitions\":[{") ||
		!strstr(line, "\"ray_dir\":{") ||
		!strstr(line, "\"ray_pt\":{") ||
		!strstr(line, "\"region\":\"/sphere.r\"")) {
		bu_log("unexpected capture record: %s", line);
		failures++;
	    }
	    records++;
	}
	fclose(file);
    }
    if (records != 2) failures++;

    rt_i_destroy(rtip);
    db_close(dbip);
    if (failures) bu_log("rtcmp capture test failed (%d failures, %d records)\n", failures, records);
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
