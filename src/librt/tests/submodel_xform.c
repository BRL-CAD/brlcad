/*                 S U B M O D E L _ X F O R M . C
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

#include <string.h>

#include "bn.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "rt/misc.h"
#include "wdb.h"


static void
init_submodel(struct rt_db_internal *intern, const mat_t mat, const struct db_i *dbip)
{
    struct rt_submodel_internal *submodel;

    RT_DB_INTERNAL_INIT(intern);
    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_SUBMODEL;
    intern->idb_meth = &OBJ[ID_SUBMODEL];
    BU_ALLOC(submodel, struct rt_submodel_internal);
    intern->idb_ptr = submodel;

    submodel->magic = RT_SUBMODEL_INTERNAL_MAGIC;
    BU_VLS_INIT(&submodel->file);
    BU_VLS_INIT(&submodel->treetop);
    bu_vls_strcpy(&submodel->treetop, "leaf.r");
    submodel->meth = 0;
    MAT_COPY(submodel->root2leaf, mat);
    submodel->dbip = dbip;
}


static int
expect_matrix(const char *label, const mat_t actual, const mat_t expected)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;

    if (bn_mat_is_equal(actual, expected, &tol))
	return 0;

    bu_log("%s: matrix mismatch\n", label);
    bn_mat_print("actual", actual);
    bn_mat_print("expected", expected);
    return 1;
}


static int
expect_submodel(const char *label, const struct rt_db_internal *intern, const mat_t expected)
{
    const struct rt_submodel_internal *submodel;
    int failures = 0;

    if (intern->idb_type != ID_SUBMODEL || !intern->idb_ptr) {
	bu_log("%s: import did not produce a submodel\n", label);
	return 1;
    }

    submodel = (const struct rt_submodel_internal *)intern->idb_ptr;
    RT_SUBMODEL_CK_MAGIC(submodel);
    if (!BU_STR_EQUAL(bu_vls_cstr(&submodel->treetop), "leaf.r")) {
	bu_log("%s: treetop changed to '%s'\n", label, bu_vls_cstr(&submodel->treetop));
	failures++;
    }
    failures += expect_matrix(label, submodel->root2leaf, expected);

    return failures;
}


static int
test_round_trip(struct db_i *dbip, int version, const mat_t stored, const mat_t applied, const mat_t expected)
{
    struct rt_db_internal source;
    struct rt_db_internal imported;
    struct bu_external external;
    int failures = 0;
    int ret;

    init_submodel(&source, stored, dbip);
    RT_DB_INTERNAL_INIT(&imported);
    BU_EXTERNAL_INIT(&external);

    if (version == 4)
	ret = OBJ[ID_SUBMODEL].ft_export4(&external, &source, 1.0, dbip);
    else
	ret = OBJ[ID_SUBMODEL].ft_export5(&external, &source, 1.0, dbip);

    if (ret < 0) {
	bu_log("v%d export failed\n", version);
	failures++;
	goto cleanup;
    }

    if (version == 4)
	ret = OBJ[ID_SUBMODEL].ft_import4(&imported, &external, applied, dbip);
    else
	ret = OBJ[ID_SUBMODEL].ft_import5(&imported, &external, applied, dbip);

    if (ret < 0) {
	bu_log("v%d import failed\n", version);
	failures++;
	goto cleanup;
    }

    failures += expect_submodel(version == 4 ? "v4 round trip" : "v5 round trip", &imported, expected);

cleanup:
    if (imported.idb_ptr)
	rt_db_free_internal(&imported);
    bu_free_external(&external);
    rt_db_free_internal(&source);
    return failures;
}


static int
test_legacy_import(struct db_i *dbip, int version, const mat_t applied)
{
    const char legacy_record[] = "file={} treetop=leaf.r meth=0";
    struct rt_db_internal imported;
    struct bu_external external;
    union record *record;
    int failures = 0;
    int ret;

    RT_DB_INTERNAL_INIT(&imported);
    BU_EXTERNAL_INIT(&external);

    if (version == 4) {
	external.ext_nbytes = sizeof(union record) * DB_SS_NGRAN;
	external.ext_buf = (uint8_t *)bu_calloc(1, external.ext_nbytes, "legacy v4 submodel record");
	record = (union record *)external.ext_buf;
	record->ss.ss_id = DBID_STRSOL;
	bu_strlcpy(record->ss.ss_keyword, "submodel", sizeof(record->ss.ss_keyword));
	bu_strlcpy(record->ss.ss_args, legacy_record, DB_SS_LEN);
	ret = OBJ[ID_SUBMODEL].ft_import4(&imported, &external, applied, dbip);
    } else {
	external.ext_nbytes = sizeof(legacy_record);
	external.ext_buf = (uint8_t *)bu_malloc(external.ext_nbytes, "legacy v5 submodel record");
	memcpy(external.ext_buf, legacy_record, external.ext_nbytes);
	ret = OBJ[ID_SUBMODEL].ft_import5(&imported, &external, applied, dbip);
    }

    if (ret < 0) {
	bu_log("legacy v%d import failed\n", version);
	failures++;
    } else {
	failures += expect_submodel(version == 4 ? "legacy v4 import" : "legacy v5 import", &imported, applied);
    }

    if (imported.idb_ptr)
	rt_db_free_internal(&imported);
    bu_free_external(&external);
    return failures;
}


static int
test_generic_xform(struct db_i *dbip, const mat_t stored, const mat_t applied, const mat_t expected)
{
    struct rt_db_internal source;
    struct rt_db_internal transformed;
    int failures = 0;

    init_submodel(&source, stored, dbip);
    RT_DB_INTERNAL_INIT(&transformed);

    if (rt_generic_xform(&transformed, applied, &source, 0, dbip) < 0) {
	bu_log("generic transform failed\n");
	failures++;
    } else {
	failures += expect_submodel("generic transform", &transformed, expected);
    }

    if (transformed.idb_ptr)
	rt_db_free_internal(&transformed);
    rt_db_free_internal(&source);
    return failures;
}


static int
test_mat_callback(struct db_i *dbip, const mat_t stored, const mat_t applied, const mat_t expected)
{
    struct rt_db_internal intern;
    int failures = 0;

    init_submodel(&intern, stored, dbip);
    if (OBJ[ID_SUBMODEL].ft_mat(&intern, applied, &intern) != BRLCAD_OK) {
	bu_log("matrix callback failed\n");
	failures++;
    } else {
	failures += expect_submodel("matrix callback", &intern, expected);
    }

    rt_db_free_internal(&intern);
    return failures;
}


static int
test_make_defaults(void)
{
    struct rt_db_internal intern;
    struct rt_submodel_internal *submodel;
    point_t origin = VINIT_ZERO;
    int failures = 0;

    RT_DB_INTERNAL_INIT(&intern);
    if (OBJ[ID_SUBMODEL].ft_make(&OBJ[ID_SUBMODEL], &intern, NULL, origin, 1.0) != BRLCAD_OK) {
	bu_log("submodel make failed\n");
	return 1;
    }

    submodel = (struct rt_submodel_internal *)intern.idb_ptr;
    failures += expect_matrix("make default", submodel->root2leaf, bn_mat_identity);
    rt_db_free_internal(&intern);
    return failures;
}


static int
test_libwdb_defaults(struct db_i *dbip)
{
    struct rt_db_internal intern;
    struct rt_wdb *wdbp;
    struct directory *dp;
    int failures = 0;

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp || mk_submodel(wdbp, "reference.s", NULL, "leaf.r", 0) < 0) {
	bu_log("mk_submodel failed\n");
	return 1;
    }

    dp = db_lookup(dbip, "reference.s", LOOKUP_QUIET);
    RT_DB_INTERNAL_INIT(&intern);
    if (dp == RT_DIR_NULL || rt_db_get_internal(&intern, dp, dbip, bn_mat_identity) < 0) {
	bu_log("unable to read mk_submodel output\n");
	return 1;
    }

    failures += expect_submodel("mk_submodel default", &intern, bn_mat_identity);
    rt_db_free_internal(&intern);
    return failures;
}


int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    mat_t applied;
    mat_t expected;
    mat_t stored;
    int failures = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	bu_exit(1, "unable to create in-memory database\n");

    MAT_IDN(stored);
    MAT_DELTAS(stored, 2.0, 3.0, 4.0);

    MAT_IDN(applied);
    applied[0] = 0.0;
    applied[1] = -1.0;
    applied[4] = 1.0;
    applied[5] = 0.0;
    MAT_DELTAS(applied, 10.0, 20.0, 30.0);

    bn_mat_mul(expected, applied, stored);

    failures += test_round_trip(dbip, 4, stored, applied, expected);
    failures += test_round_trip(dbip, 5, stored, applied, expected);
    failures += test_legacy_import(dbip, 4, applied);
    failures += test_legacy_import(dbip, 5, applied);
    failures += test_generic_xform(dbip, stored, applied, expected);
    failures += test_mat_callback(dbip, stored, applied, expected);
    failures += test_make_defaults();
    failures += test_libwdb_defaults(dbip);

    db_close(dbip);

    if (failures)
	bu_log("submodel transform tests: %d failure(s)\n", failures);

    return failures ? BRLCAD_ERROR : BRLCAD_OK;
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
