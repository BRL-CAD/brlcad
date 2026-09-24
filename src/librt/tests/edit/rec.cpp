/*                        R E C . C P P
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
/** @file rec.cpp
 *
 * Exercise each REC-specific operation and its dimensional contract.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/edit.h"
#include "rt/geom.h"
#include "wdb.h"

#define ECMD_REC_SET_V    7001
#define ECMD_REC_SET_H    7002
#define ECMD_REC_SCALE_R1 7003
#define ECMD_REC_SCALE_R2 7004
#define ECMD_REC_SCALE_R  7005

static void
check_getter(struct rt_edit *s, int cmd, int count, const fastf_t *expected)
{
    fastf_t values[3] = {0.0, 0.0, 0.0};
    if (EDOBJ[ID_REC].ft_edit_get_params(s, cmd, values) != count)
        bu_exit(1, "REC getter returned wrong value count for %d\n", cmd);
    for (int i = 0; i < count; i++) {
        if (!NEAR_EQUAL(values[i], expected[i], VUNITIZE_TOL))
            bu_exit(1, "REC getter %d value %d: got %g, expected %g\n",
                    cmd, i, values[i], expected[i]);
    }
}

static void
check_rec_units(const fastf_t local2base)
{
    const fastf_t inch = 25.4;
    const fastf_t inch_in_local = inch / local2base;
    struct db_i *dbip = db_open_inmem();
    if (!dbip)
        bu_exit(1, "Unable to create REC test database\n");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct rt_tgc_internal *rec;
    BU_ALLOC(rec, struct rt_tgc_internal);
    rec->magic = RT_TGC_INTERNAL_MAGIC;
    VSET(rec->v, inch, 0.0, 0.0);
    VSET(rec->h, 0.0, 0.0, 2.0 * inch);
    VSET(rec->a, inch, 0.0, 0.0);
    VSET(rec->b, 0.0, 2.0 * inch, 0.0);
    VMOVE(rec->c, rec->a);
    VMOVE(rec->d, rec->b);
    if (wdb_export(wdbp, "rec", rec, ID_REC, 1.0) != 0)
        bu_exit(1, "Unable to create REC object\n");

    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct directory *dp = db_lookup(dbip, "rec", LOOKUP_QUIET);
    if (!dp || dp->d_minor_type != ID_REC)
        bu_exit(1, "REC has wrong database type\n");

    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, NULL);
    if (!s)
        bu_exit(1, "Unable to create REC edit state\n");
    rec = (struct rt_tgc_internal *)s->es_int.idb_ptr;
    RT_TGC_CK_MAGIC(rec);

    const struct rt_edit_prim_desc *desc = EDOBJ[ID_REC].ft_edit_desc();
    if (!desc || desc->ncmd != 5)
        bu_exit(1, "REC edit descriptor is incomplete\n");
    for (int i = 0; i < desc->ncmd; i++) {
        if (desc->cmds[i].cmd_id != ECMD_REC_SET_V + i)
            bu_exit(1, "REC descriptor command %d is missing\n", i);
    }

    const fastf_t original_v[] = {inch_in_local, 0.0, 0.0};
    check_getter(s, ECMD_REC_SET_V, 3, original_v);

    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SET_V);
    s->e_inpara = 3;
    VSET(s->e_para, 2.0 * inch_in_local, 3.0 * inch_in_local,
         4.0 * inch_in_local);
    rt_edit_process(s);
    const fastf_t moved_v[] = {2.0 * inch_in_local,
                               3.0 * inch_in_local,
                               4.0 * inch_in_local};
    point_t expected_v;
    VSCALE(expected_v, moved_v, local2base);
    if (!VNEAR_EQUAL(rec->v, expected_v, VUNITIZE_TOL))
        bu_exit(1, "REC center did not use local units\n");
    check_getter(s, ECMD_REC_SET_V, 3, moved_v);

    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SET_H);
    s->e_inpara = 3;
    VSET(s->e_para, 0.0, 0.0, 3.0 * inch_in_local);
    rt_edit_process(s);
    const fastf_t expected_h_local[] = {0.0, 0.0,
                                        3.0 * inch_in_local};
    check_getter(s, ECMD_REC_SET_H, 3, expected_h_local);
    if (!NEAR_ZERO(VDOT(rec->a, rec->h), VUNITIZE_TOL) ||
        !NEAR_ZERO(VDOT(rec->b, rec->h), VUNITIZE_TOL) ||
        !VNEAR_EQUAL(rec->c, rec->a, VUNITIZE_TOL) ||
        !VNEAR_EQUAL(rec->d, rec->b, VUNITIZE_TOL))
        bu_exit(1, "REC height edit violated orthogonality or end radii\n");

    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SCALE_R1);
    s->e_inpara = 1;
    s->e_para[0] = 4.0 * inch_in_local;
    rt_edit_process(s);
    const fastf_t radius1[] = {4.0 * inch_in_local};
    check_getter(s, ECMD_REC_SCALE_R1, 1, radius1);
    if (!NEAR_EQUAL(MAGNITUDE(rec->a), 4.0 * inch, VUNITIZE_TOL) ||
        !VNEAR_EQUAL(rec->c, rec->a, VUNITIZE_TOL))
        bu_exit(1, "REC radius 1 edit changed the wrong geometry\n");

    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SCALE_R2);
    s->e_inpara = 1;
    s->e_para[0] = 5.0 * inch_in_local;
    rt_edit_process(s);
    const fastf_t radius2[] = {5.0 * inch_in_local};
    check_getter(s, ECMD_REC_SCALE_R2, 1, radius2);
    if (!NEAR_EQUAL(MAGNITUDE(rec->b), 5.0 * inch, VUNITIZE_TOL) ||
        !VNEAR_EQUAL(rec->d, rec->b, VUNITIZE_TOL))
        bu_exit(1, "REC radius 2 edit changed the wrong geometry\n");

    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SCALE_R);
    s->e_inpara = 1;
    s->e_para[0] = 2.5 * inch_in_local;
    rt_edit_process(s);
    const fastf_t radius[] = {2.5 * inch_in_local};
    check_getter(s, ECMD_REC_SCALE_R, 1, radius);
    if (!NEAR_EQUAL(MAGNITUDE(rec->a), 2.5 * inch, VUNITIZE_TOL) ||
        !NEAR_EQUAL(MAGNITUDE(rec->b), 2.5 * inch, VUNITIZE_TOL))
        bu_exit(1, "REC equal-radius edit did not use local units\n");

    const struct rt_tgc_internal saved_rec = *rec;
    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SET_V);
    s->e_inpara = 3;
    VSET(s->e_para, NAN, 0.0, 0.0);
    if (EDOBJ[ID_REC].ft_edit(s) != BRLCAD_ERROR ||
	memcmp(rec, &saved_rec, sizeof(saved_rec)))
	bu_exit(1, "REC accepted an invalid center or changed geometry\n");

    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SET_H);
    VSET(s->e_para, 0.0, 0.0, tol.dist * 0.5 / local2base);
    if (EDOBJ[ID_REC].ft_edit(s) != BRLCAD_ERROR ||
	memcmp(rec, &saved_rec, sizeof(saved_rec)))
	bu_exit(1, "REC accepted a near-zero height or changed geometry\n");

    const int radius_commands[] = {
	ECMD_REC_SCALE_R1, ECMD_REC_SCALE_R2, ECMD_REC_SCALE_R
    };
    for (size_t i = 0; i < sizeof(radius_commands) / sizeof(radius_commands[0]); i++) {
	EDOBJ[ID_REC].ft_set_edit_mode(s, radius_commands[i]);
	s->e_inpara = 1;
	s->e_para[0] = NAN;
	if (EDOBJ[ID_REC].ft_edit(s) != BRLCAD_ERROR ||
	    memcmp(rec, &saved_rec, sizeof(saved_rec)))
	    bu_exit(1, "REC accepted an invalid radius or changed geometry\n");
    }

    s->e_inpara = 1;
    s->e_para[0] = 0.0;
    if (EDOBJ[ID_REC].ft_edit(s) != BRLCAD_ERROR ||
        !NEAR_EQUAL(MAGNITUDE(rec->a), 2.5 * inch, VUNITIZE_TOL) ||
        !NEAR_EQUAL(MAGNITUDE(rec->b), 2.5 * inch, VUNITIZE_TOL))
        bu_exit(1, "REC accepted a zero radius or changed geometry\n");
    vect_t saved_h;
    VMOVE(saved_h, rec->h);
    EDOBJ[ID_REC].ft_set_edit_mode(s, ECMD_REC_SET_H);
    s->e_inpara = 3;
    VSETALL(s->e_para, 0.0);
    if (EDOBJ[ID_REC].ft_edit(s) != BRLCAD_ERROR ||
        !VNEAR_EQUAL(rec->h, saved_h, VUNITIZE_TOL))
        bu_exit(1, "REC accepted a zero height or changed geometry\n");

    if (EDOBJ[ID_REC].ft_edit_get_params(s, -1, s->e_para) != 0 ||
        EDOBJ[ID_REC].ft_edit_get_params(s, ECMD_REC_SCALE_R, NULL) >= 0)
        bu_exit(1, "REC getter accepted invalid arguments\n");

    point_t saved_center;
    VMOVE(saved_center, rec->v);
    if (rt_edit_checkpoint(s) != BRLCAD_OK)
	bu_exit(1, "REC checkpoint failed\n");
    rec->v[X] += inch;
    if (rt_edit_revert(s) != BRLCAD_OK || s->es_int.idb_type != ID_REC ||
	!VNEAR_EQUAL(((struct rt_tgc_internal *)s->es_int.idb_ptr)->v,
	    saved_center, VUNITIZE_TOL))
	bu_exit(1, "REC revert lost its type or geometry\n");

    rt_edit_destroy(s);
    db_free_full_path(&fp);
    db_close(dbip);
}

int
rt_edit_test_rec(void)
{
    const fastf_t mm_to_base = 1.0;
    const fastf_t inch_to_base = 25.4;
    check_rec_units(mm_to_base);
    check_rec_units(inch_to_base);
    return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
