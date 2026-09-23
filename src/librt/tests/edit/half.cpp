/*                       H A L F . C P P
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
/** @file half.cpp
 *
 * Exercise halfspace parameter editing in a non-millimeter database.
 */

#include "common.h"

#include "vmath.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/edit.h"
#include "rt/geom.h"
#include "wdb.h"

#define ECMD_HALF_SET_D 6001

static void
check_half_units(const fastf_t local2base)
{
    const fastf_t inch = 25.4;
    const fastf_t initial_local = inch / local2base;
    const fastf_t edited_base = 2.5 * inch;
    const fastf_t edited_local = edited_base / local2base;
    const vect_t normal = {0.0, 0.0, 1.0};
    struct db_i *dbip = db_open_inmem();
    if (!dbip)
        bu_exit(1, "Unable to create halfspace test database\n");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (mk_half(wdbp, "half", normal, inch) != 0)
        bu_exit(1, "Unable to create halfspace\n");

    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct directory *dp = db_lookup(dbip, "half", LOOKUP_QUIET);
    if (!dp || dp->d_minor_type != ID_HALF)
        bu_exit(1, "Halfspace has wrong database type\n");

    struct db_full_path fp;
    db_full_path_init(&fp);
    db_add_node_to_full_path(&fp, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_edit *s = rt_edit_create(&fp, dbip, &tol, NULL);
    if (!s)
        bu_exit(1, "Unable to create halfspace edit state\n");

    const struct rt_edit_prim_desc *desc = EDOBJ[ID_HALF].ft_edit_desc();
    if (!desc || desc->ncmd != 1 || desc->cmds[0].cmd_id != ECMD_HALF_SET_D)
        bu_exit(1, "Halfspace edit descriptor is incomplete\n");

    struct rt_half_internal *half = (struct rt_half_internal *)s->es_int.idb_ptr;
    RT_HALF_CK_MAGIC(half);
    fastf_t values[3] = {0.0, 0.0, 0.0};
    if (EDOBJ[ID_HALF].ft_edit_get_params(s, ECMD_HALF_SET_D, values) != 1 ||
        !NEAR_EQUAL(values[0], initial_local, VUNITIZE_TOL))
        bu_exit(1, "Halfspace getter did not return local units\n");

    EDOBJ[ID_HALF].ft_set_edit_mode(s, ECMD_HALF_SET_D);
    s->e_inpara = 1;
    s->e_para[0] = edited_local;
    rt_edit_process(s);
    if (!NEAR_EQUAL(half->eqn[W], edited_base, VUNITIZE_TOL))
        bu_exit(1, "Halfspace distance was not converted to base units\n");
    if (EDOBJ[ID_HALF].ft_edit_get_params(s, ECMD_HALF_SET_D, values) != 1 ||
        !NEAR_EQUAL(values[0], edited_local, VUNITIZE_TOL))
        bu_exit(1, "Halfspace getter did not round-trip local units\n");

    s->e_inpara = 0;
    rt_edit_process(s);
    if (!NEAR_EQUAL(half->eqn[W], edited_base, VUNITIZE_TOL))
        bu_exit(1, "Halfspace edit changed geometry without a parameter\n");

    struct bu_vls params = BU_VLS_INIT_ZERO;
    EDOBJ[ID_HALF].ft_write_params(&params, &s->es_int, &tol, s->base2local);
    half->eqn[W] = 0.0;
    if (EDOBJ[ID_HALF].ft_read_params(&s->es_int, bu_vls_cstr(&params),
                                     &tol, s->local2base) != BRLCAD_OK ||
        !NEAR_EQUAL(half->eqn[W], edited_base, VUNITIZE_TOL))
        bu_exit(1, "Halfspace parameter text did not round-trip\n");
    bu_vls_free(&params);

    if (EDOBJ[ID_HALF].ft_edit_get_params(s, -1, values) != 0 ||
        EDOBJ[ID_HALF].ft_edit_get_params(s, ECMD_HALF_SET_D, NULL) >= 0)
        bu_exit(1, "Halfspace getter accepted invalid arguments\n");

    rt_edit_destroy(s);
    db_free_full_path(&fp);
    db_close(dbip);
}

int
rt_edit_test_half(void)
{
    const fastf_t mm_to_base = 1.0;
    const fastf_t inch_to_base = 25.4;
    check_half_units(mm_to_base);
    check_half_units(inch_to_base);
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
