/*                      R T _ P E R T U R B . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2024 United States Government as represented by
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
#include "bu/avs.h"
#include "bu/malloc.h"
#include "wdb.h"
#include "raytrace.h"
#include "../librt_private.h"

int
main(int UNUSED(argc), char *argv[])
{
    struct db_i *dbip = DBI_NULL;

    bu_setprogname(argv[0]);

    /* Manually turn the temp file into a .g */
    dbip = db_create("perturb_test.g", 5);
    if (dbip == DBI_NULL) {
	bu_log("error: db_create of perturb_test.g failed\n");
	return 1;
    }
    RT_CK_DBI(dbip);


    {
	/* Initial RCC shape */
	struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
	point_t v;
	vect_t h;
	double r = 10.0;
	VSET(v, -10, -10, 0);
	VSET(h, 0, 5, 20);
	mk_rcc(wdbp, "rcc_1.s", v, h, r);
	db_update_nref(dbip, &rt_uniresource);
    }

    {
	/* Make a perturbed version */
	struct directory *wdp = db_lookup(dbip, "rcc_1.s", LOOKUP_QUIET);
	struct rt_db_internal intern;
	rt_db_get_internal(&intern, wdp, dbip, NULL, &rt_uniresource);

	struct rt_db_internal *ointern = NULL;
	if (intern.idb_meth && intern.idb_meth->ft_perturb) {
	    intern.idb_meth->ft_perturb(&ointern, &intern, 0, 0, 2);
	}

	wdp = db_diradd(dbip, "rcc_perturb.s", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&ointern->idb_type);
	rt_db_put_internal(wdp, dbip, ointern, &rt_uniresource);
	rt_db_free_internal(ointern);
	BU_PUT(ointern, struct rt_db_internal);
	db_update_nref(dbip, &rt_uniresource);
    }

    {
	// Initial ARB7 shape
	struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
	vect_t pts7[7];
	VSET(pts7[0], 1822.8,  1039.51, -1822.57);
	VSET(pts7[1], 2610.28, 1199.34, -3654.06);
	VSET(pts7[2], 4322.88, 411.084, -2986.49);
	VSET(pts7[3], 2679.1,  645.381, -1488.79);
	VSET(pts7[4], 2491.29, 2870.66, -1375.34);
	VSET(pts7[5], 3278.76, 3030.49, -3206.83);
	VSET(pts7[6], 4135.06, 2636.36, -2873.04);
	mk_arb8(wdbp, "arb7_1.s", (const fastf_t *)pts7);
	db_update_nref(dbip, &rt_uniresource);
    }
    {
	/* Make a perturbed version */
	struct directory *wdp = db_lookup(dbip, "arb7_1.s", LOOKUP_QUIET);
	struct rt_db_internal intern;
	rt_db_get_internal(&intern, wdp, dbip, NULL, &rt_uniresource);

	struct rt_db_internal *ointern = NULL;
	if (intern.idb_meth && intern.idb_meth->ft_perturb) {
	    intern.idb_meth->ft_perturb(&ointern, &intern, 0, 0, 100);
	}

	wdp = db_diradd(dbip, "arb_perturb.s", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&ointern->idb_type);
	rt_db_put_internal(wdp, dbip, ointern, &rt_uniresource);
	rt_db_free_internal(ointern);
	BU_PUT(ointern, struct rt_db_internal);
	db_update_nref(dbip, &rt_uniresource);
    }

    db_close(dbip);
    //bu_file_delete("perturb_test.g");

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
