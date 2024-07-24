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
	    intern.idb_meth->ft_perturb(&ointern, &intern, 0, 2);
	    wdp = db_diradd(dbip, "rcc_perturb.s", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&ointern->idb_type);
	    rt_db_put_internal(wdp, dbip, ointern, &rt_uniresource);
	    rt_db_free_internal(ointern);
	    BU_PUT(ointern, struct rt_db_internal);
	    db_update_nref(dbip, &rt_uniresource);
	}
    }

    {
	// Initial ARB shape
	struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
	vect_t pts8[8];
#if 0
	VSET(pts8[0], 24.8507, -24.8507, -24.8507 );
	VSET(pts8[1], 24.8507, 24.8507, -24.8507  );
	VSET(pts8[2], 24.8507, 24.8507, 24.8507   );
	VSET(pts8[3], 24.8507, -24.8507, 24.8507  );
	VSET(pts8[4], -24.8507, 0, 0);
	mk_arb5(wdbp, "arb8_1.s", (const fastf_t *)pts8);
#endif
	// If we're not centered around the origin, we're not making a valid ARBN...
#if 0
	VSET(pts8[0], 4.8507, -24.8507, -24.8507 );
	VSET(pts8[1], 4.8507, 24.8507, -24.8507  );
	VSET(pts8[2], 4.8507, 24.8507, 24.8507   );
	VSET(pts8[3], 4.8507, -24.8507, 24.8507  );
	VSET(pts8[4], -45.8507, 0, 0);
	mk_arb5(wdbp, "arb8_1.s", (const fastf_t *)pts8);
#endif
#if 1
	VSET(pts8[0], 3146.95649964142830867786,  -213.734397388383968063863,   967.2554952387024513882352);
	VSET(pts8[1], 4835.946259752023252076469, -1081.969959924687827879097,  339.975176570206258475082 );
	VSET(pts8[2], 5628.127176313613745151088, -857.7799790468159244483104,  2162.662272017370924004354);
	VSET(pts8[3], 3543.046957922220371983713, -101.6394069494533027864236,  1878.599042962285693647573);
	VSET(pts8[4], 3867.902396139355005288962,  1573.975271551538753556088,  434.0283823669653884280706);
	VSET(pts8[5], 5556.892156249954496161081,  705.7397090152405780827394, -193.2519363015293833996111);
	VSET(pts8[6], 5952.982614530745195224881,  817.8346994541723233851371,  718.0916114220510735322023);
	mk_arb7(wdbp, "arb8_1.s", (const fastf_t *)pts8);
#endif
#if 0
	VSET(pts8[0], 24.8507, -24.8507, -24.8507 );
	VSET(pts8[1], 24.8507, 24.8507, -24.8507  );
	VSET(pts8[2], 24.8507, 24.8507, 24.8507   );
	VSET(pts8[3], 24.8507, -24.8507, 24.8507  );
	VSET(pts8[4], -24.8507, -24.8507, -24.8507);
	VSET(pts8[5], -24.8507, 24.8507, -24.8507 );
	VSET(pts8[6], -24.8507, 24.8507, 24.8507  );
	VSET(pts8[7], -24.8507, -24.8507, 24.8507 );
	mk_arb8(wdbp, "arb8_1.s", (const fastf_t *)pts8);
#endif
	db_update_nref(dbip, &rt_uniresource);
    }
    {
	/* Make a perturbed version */
	struct directory *wdp = db_lookup(dbip, "arb8_1.s", LOOKUP_QUIET);
	struct rt_db_internal intern;
	rt_db_get_internal(&intern, wdp, dbip, NULL, &rt_uniresource);

	struct rt_db_internal *ointern = NULL;
	if (intern.idb_meth && intern.idb_meth->ft_perturb) {
	    intern.idb_meth->ft_perturb(&ointern, &intern, 1, 10);
	    wdp = db_diradd(dbip, "arb_perturb.s", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&ointern->idb_type);
	    rt_db_put_internal(wdp, dbip, ointern, &rt_uniresource);
	    rt_db_free_internal(ointern);
	    BU_PUT(ointern, struct rt_db_internal);
	    db_update_nref(dbip, &rt_uniresource);
	}
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
