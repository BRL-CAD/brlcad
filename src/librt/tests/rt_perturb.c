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
#include "raytrace.h"
#include "../librt_private.h"


/* Note - initially, the thought was to use only librt API for this, without
 * pulling in libwdb.  Normally libwdb should be used to create objects of this
 * sort (and there really isn't much point to not doing so here, in retrospect)
 * but since the below code is working leave it for now. Eventually, we'll
 * regularize the libwdb API (probably making it a part of a "libg" library and
 * once we do the below add_* functions can be simplified down to mk_* calls */

int
add_rcc(struct db_i *dbip, const char *name, point_t *v, vect_t *h, double r)
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct rt_tgc_internal *tip;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_TGC;
    intern.idb_meth = &OBJ[ID_TGC];
    BU_ALLOC(intern.idb_ptr, struct rt_tgc_internal);
    tip = (struct rt_tgc_internal *)intern.idb_ptr;
    tip->magic = RT_TGC_INTERNAL_MAGIC;

    VMOVE(tip->v, *v);
    VMOVE(tip->h, *h);

    bn_vec_ortho(tip->a, tip->h);
    VUNITIZE(tip->a);
    VCROSS(tip->b, tip->h, tip->a);
    VUNITIZE(tip->b);

    VSCALE(tip->a, tip->a, r);
    VSCALE(tip->b, tip->b, r);
    VMOVE(tip->c, tip->a);
    VMOVE(tip->d, tip->b);

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&intern);
	bu_log("Error: Cannot add %s to directory\n", name);
	return 1;
    }
    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_log("Error: Database write error, aborting\n");
	return 1;
    }

    return 0;
}

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

    /* Initial RCC shape */
    {
	point_t v;
	vect_t h;
	double r = 10.0;
	VSET(v, -10, -10, 0);
	VSET(h, 0, 5, 20);
	if (add_rcc(dbip, "rcc_1.s", &v, &h, r)) return 1;
    }

    db_update_nref(dbip, &rt_uniresource);

    /* Make a perturbed version */
    struct directory *wdp = db_lookup(dbip, "rcc_1.s", LOOKUP_QUIET);
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, wdp, dbip, NULL, &rt_uniresource);

    struct rt_db_internal *ointern = NULL;
    if (intern.idb_meth && intern.idb_meth->ft_perturb) {
	intern.idb_meth->ft_perturb(&ointern, &intern, 0, 0, 2);
    }

    wdp = db_diradd(dbip, "rcc_perturb.s", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&ointern->idb_type);
    rt_db_put_internal(wdp, dbip, &intern, &rt_uniresource);
    rt_db_free_internal(ointern);
    BU_PUT(ointern, struct rt_db_internal);

    // TODO - test arb8 perturbation


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
