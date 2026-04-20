/*                  I N M E M _ T E S T S . C
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */
/** @file inmem_tests.c
 *
 * Regression test for rt_db_put_internal() with in-memory databases.
 * Bug report: https://github.com/BRL-CAD/brlcad/issues/864
 */
#include "common.h"
#include <stdio.h>
#include "bu.h"
#include "vmath.h"
#include "wdb.h"
#include "raytrace.h"

int
main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* exactly the bug reporter's code */
    struct db_i *db = db_create_inmem();

    struct rt_ell_internal *sph = NULL;
    BU_ALLOC(sph, struct rt_ell_internal);
    sph->magic = RT_ELL_INTERNAL_MAGIC;
    VSET(sph->v, 0, 0, 0);
    VSET(sph->a, 1, 0, 0);
    VSET(sph->b, 0, 1, 0);
    VSET(sph->c, 0, 0, 1);

    struct rt_db_internal in;
    RT_DB_INTERNAL_INIT(&in);
    in.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    in.idb_minor_type = ID_SPH;
    in.idb_meth = &OBJ[ID_SPH];
    in.idb_ptr = sph;

    unsigned char minor_type = ID_SPH;
    struct directory *dir = db_diradd(db, "sph.o", RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, &minor_type);
    int ret = rt_db_put_internal(dir, db, &in, &rt_uniresource);

    if (ret < 0) {
        bu_log("FAIL: rt_db_put_internal() returned %d\n", ret);
        db_close(db);
        return 1;
    }

    bu_log("PASS: rt_db_put_internal works with inmem database\n");
    db_close(db);
    return 0;
}
