/*                    R A Y D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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

#include <string.h>

#include "bu/app.h"
#include "raytrace.h"
#include "../analyze_private.h"
#include "analyze.h"


int
main(int argc, char **argv)
{
    int count = 0;
    fastf_t *rays;
    size_t ncpus = bu_avail_cpus();
    struct db_i *dbip = DBI_NULL;
    struct directory *dp = RT_DIR_NULL;
    struct bn_tol tol = BG_TOL_INIT;
    struct bn_tol rtol = {BN_TOL_MAGIC, 10, 0.5 * 0.5, 1.0e-6, 1.0 - 1.0e-6 };
    struct resource resp;
    struct rt_gen_worker_vars state;
    struct rt_i *rtip;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;

    bu_setprogname(argv[0]);

    if (argc != 3) {
	bu_log("Error - please specify a .g file and one object\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if ((dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }

    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);

    if (dp == RT_DIR_NULL)
	return 1;

    /* TODO: call rt_bound_internal instead of prep directly to get
     * the bounding box.
     */
    rtip = rt_new_rti(dbip);
    state.rtip = rtip;
    state.resp = &resp;
    rt_init_resource(state.resp, 0, rtip);
    if (rt_gettree(rtip, argv[2]) < 0)
	return -1;
    rt_prep_parallel(rtip, 1);

    count = analyze_get_bbox_rays(&rays, rtip->mdl_min, rtip->mdl_max, &rtol);

    analyze_get_solid_partitions(&results, NULL, rays, count, dbip, argv[2], &tol, ncpus, 1);

    db_close(dbip);
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
