/*                 O P E R A T I O N _ M A T R I X . C P P
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
/** @file operation_matrix.cpp
 *
 * Exercise native descriptor commands against complete expected geometry
 * in millimeter and inch databases.  Each command starts from the same
 * persisted fixture so a previous edit cannot mask a bad result.
 */

#include "common.h"

#include <stddef.h>
#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/rt_ecmds.h"

static const fastf_t inch_to_mm = 25.4;

struct numeric_field {
    size_t offset;
    size_t count;
    const char *name;
};

#define NUMERIC_FIELD(_type, _field, _count) \
    { offsetof(_type, _field), _count, #_field }

static const struct numeric_field ell_fields[] = {
    NUMERIC_FIELD(struct rt_ell_internal, v, 3),
    NUMERIC_FIELD(struct rt_ell_internal, a, 3),
    NUMERIC_FIELD(struct rt_ell_internal, b, 3),
    NUMERIC_FIELD(struct rt_ell_internal, c, 3)
};

static const struct numeric_field part_fields[] = {
    NUMERIC_FIELD(struct rt_part_internal, part_V, 3),
    NUMERIC_FIELD(struct rt_part_internal, part_H, 3),
    NUMERIC_FIELD(struct rt_part_internal, part_vrad, 1),
    NUMERIC_FIELD(struct rt_part_internal, part_hrad, 1)
};

static const struct numeric_field superell_fields[] = {
    NUMERIC_FIELD(struct rt_superell_internal, v, 3),
    NUMERIC_FIELD(struct rt_superell_internal, a, 3),
    NUMERIC_FIELD(struct rt_superell_internal, b, 3),
    NUMERIC_FIELD(struct rt_superell_internal, c, 3),
    NUMERIC_FIELD(struct rt_superell_internal, n, 1),
    NUMERIC_FIELD(struct rt_superell_internal, e, 1)
};

static const struct numeric_field epa_fields[] = {
    NUMERIC_FIELD(struct rt_epa_internal, epa_V, 3),
    NUMERIC_FIELD(struct rt_epa_internal, epa_H, 3),
    NUMERIC_FIELD(struct rt_epa_internal, epa_Au, 3),
    NUMERIC_FIELD(struct rt_epa_internal, epa_r1, 1),
    NUMERIC_FIELD(struct rt_epa_internal, epa_r2, 1)
};

static const struct numeric_field ehy_fields[] = {
    NUMERIC_FIELD(struct rt_ehy_internal, ehy_V, 3),
    NUMERIC_FIELD(struct rt_ehy_internal, ehy_H, 3),
    NUMERIC_FIELD(struct rt_ehy_internal, ehy_Au, 3),
    NUMERIC_FIELD(struct rt_ehy_internal, ehy_r1, 1),
    NUMERIC_FIELD(struct rt_ehy_internal, ehy_r2, 1),
    NUMERIC_FIELD(struct rt_ehy_internal, ehy_c, 1)
};

#define FIELD_COUNT(_fields) (sizeof(_fields) / sizeof((_fields)[0]))

struct primitive_fixture {
    const char *name;
    int type;
    const void *geometry;
    size_t geometry_size;
    const struct numeric_field *fields;
    size_t field_count;
    fastf_t target_base;
};

enum fixture_id {
    FIXTURE_ELL,
    FIXTURE_PART,
    FIXTURE_SUPERELL,
    FIXTURE_EPA,
    FIXTURE_EHY,
    FIXTURE_COUNT
};

struct operation_case {
    enum fixture_id fixture;
    int command_id;
    size_t affected[3];
    size_t affected_count;
};

#define FIELD_OFFSET(_type, _field) offsetof(_type, _field)

static const struct operation_case operations[] = {
    {FIXTURE_ELL, ECMD_ELL_SCALE_A,
        {FIELD_OFFSET(struct rt_ell_internal, a)}, 1},
    {FIXTURE_ELL, ECMD_ELL_SCALE_B,
        {FIELD_OFFSET(struct rt_ell_internal, b)}, 1},
    {FIXTURE_ELL, ECMD_ELL_SCALE_C,
        {FIELD_OFFSET(struct rt_ell_internal, c)}, 1},
    {FIXTURE_ELL, ECMD_ELL_SCALE_ABC,
        {FIELD_OFFSET(struct rt_ell_internal, a),
         FIELD_OFFSET(struct rt_ell_internal, b),
         FIELD_OFFSET(struct rt_ell_internal, c)}, 3},
    {FIXTURE_PART, ECMD_PART_H,
        {FIELD_OFFSET(struct rt_part_internal, part_H)}, 1},
    {FIXTURE_PART, ECMD_PART_VRAD,
        {FIELD_OFFSET(struct rt_part_internal, part_vrad)}, 1},
    {FIXTURE_PART, ECMD_PART_HRAD,
        {FIELD_OFFSET(struct rt_part_internal, part_hrad)}, 1},
    {FIXTURE_SUPERELL, ECMD_SUPERELL_SCALE_A,
        {FIELD_OFFSET(struct rt_superell_internal, a)}, 1},
    {FIXTURE_SUPERELL, ECMD_SUPERELL_SCALE_B,
        {FIELD_OFFSET(struct rt_superell_internal, b)}, 1},
    {FIXTURE_SUPERELL, ECMD_SUPERELL_SCALE_C,
        {FIELD_OFFSET(struct rt_superell_internal, c)}, 1},
    {FIXTURE_SUPERELL, ECMD_SUPERELL_SCALE_ABC,
        {FIELD_OFFSET(struct rt_superell_internal, a),
         FIELD_OFFSET(struct rt_superell_internal, b),
         FIELD_OFFSET(struct rt_superell_internal, c)}, 3},
    {FIXTURE_EPA, ECMD_EPA_H,
        {FIELD_OFFSET(struct rt_epa_internal, epa_H)}, 1},
    {FIXTURE_EPA, ECMD_EPA_R1,
        {FIELD_OFFSET(struct rt_epa_internal, epa_r1)}, 1},
    {FIXTURE_EPA, ECMD_EPA_R2,
        {FIELD_OFFSET(struct rt_epa_internal, epa_r2)}, 1},
    {FIXTURE_EHY, ECMD_EHY_H,
        {FIELD_OFFSET(struct rt_ehy_internal, ehy_H)}, 1},
    {FIXTURE_EHY, ECMD_EHY_R1,
        {FIELD_OFFSET(struct rt_ehy_internal, ehy_r1)}, 1},
    {FIXTURE_EHY, ECMD_EHY_R2,
        {FIELD_OFFSET(struct rt_ehy_internal, ehy_r2)}, 1},
    {FIXTURE_EHY, ECMD_EHY_C,
        {FIELD_OFFSET(struct rt_ehy_internal, ehy_c)}, 1}
};

static const struct numeric_field *
find_field(const struct primitive_fixture *fixture, size_t offset)
{
    for (size_t i = 0; i < fixture->field_count; ++i) {
        if (fixture->fields[i].offset == offset)
            return &fixture->fields[i];
    }
    return NULL;
}

static bool
set_expected(void *expected, const struct primitive_fixture *fixture,
             const struct operation_case *operation)
{
    for (size_t i = 0; i < operation->affected_count; ++i) {
        const struct numeric_field *field = find_field(fixture, operation->affected[i]);
        if (!field)
            return false;
        fastf_t *value = (fastf_t *)((char *)expected + field->offset);
        if (field->count == 1) {
            value[0] = fixture->target_base;
        } else if (field->count == 3 && !ZERO(MAGNITUDE(value))) {
            VSCALE(value, value, fixture->target_base / MAGNITUDE(value));
        } else {
            return false;
        }
    }
    return true;
}

static bool
same_geometry(const struct primitive_fixture *fixture,
              const void *expected, const void *actual)
{
    for (size_t i = 0; i < fixture->field_count; ++i) {
        const struct numeric_field *field = &fixture->fields[i];
        const fastf_t *want = (const fastf_t *)((const char *)expected + field->offset);
        const fastf_t *got = (const fastf_t *)((const char *)actual + field->offset);
        for (size_t j = 0; j < field->count; ++j) {
            if (!NEAR_EQUAL(want[j], got[j], VUNITIZE_TOL)) {
                bu_log("%s[%zu]: expected %.17g, got %.17g\n",
                       field->name, j, want[j], got[j]);
                return false;
            }
        }
    }
    return true;
}

static const struct rt_edit_cmd_desc *
find_command(int type, int command_id)
{
    const struct rt_edit_prim_desc *desc = EDOBJ[type].ft_edit_desc ?
        EDOBJ[type].ft_edit_desc() : NULL;
    if (!desc)
        return NULL;
    for (int i = 0; i < desc->ncmd; ++i) {
        if (desc->cmds[i].cmd_id == command_id)
            return &desc->cmds[i];
    }
    return NULL;
}

static int
run_unit(const struct primitive_fixture fixtures[FIXTURE_COUNT],
         fastf_t local2base, const char *unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
        return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dirs[FIXTURE_COUNT] = {};
    int failures = 0;

    for (size_t i = 0; i < FIXTURE_COUNT; ++i) {
        const struct primitive_fixture *fixture = &fixtures[i];
        void *geometry = bu_malloc(fixture->geometry_size, "operation matrix geometry");
        memcpy(geometry, fixture->geometry, fixture->geometry_size);
        if (wdb_export(wdbp, fixture->name, geometry, fixture->type, 1.0) != 0) {
            bu_log("Cannot create %s operation fixture\n", fixture->name);
            db_close(dbip);
            return 1;
        }
        dirs[i] = db_lookup(dbip, fixture->name, LOOKUP_QUIET);
        if (dirs[i] == RT_DIR_NULL) {
            bu_log("Cannot find %s operation fixture\n", fixture->name);
            db_close(dbip);
            return 1;
        }
    }

    struct bn_tol tol = BN_TOL_INIT_TOL;
    for (const struct operation_case &operation : operations) {
        const struct primitive_fixture *fixture = &fixtures[operation.fixture];
        const struct rt_edit_cmd_desc *command = find_command(fixture->type, operation.command_id);
        struct db_full_path path;
        db_full_path_init(&path);
        db_add_node_to_full_path(&path, dirs[operation.fixture]);
        struct rt_edit *edit = rt_edit_create(&path, dbip, &tol, NULL);
        void *expected = bu_malloc(fixture->geometry_size, "operation matrix expected geometry");
        memcpy(expected, fixture->geometry, fixture->geometry_size);

        bool ok = command && edit && EDOBJ[fixture->type].ft_set_edit_mode &&
            set_expected(expected, fixture, &operation);
        if (ok) {
            edit->mv_context = 1;
            EDOBJ[fixture->type].ft_set_edit_mode(edit, operation.command_id);
            edit->e_inpara = 1;
            edit->e_para[0] = fixture->target_base / local2base;
            ok = (rt_edit_process(edit) == BRLCAD_OK) &&
                NEAR_EQUAL(edit->e_para[0], fixture->target_base / local2base, VUNITIZE_TOL) &&
                same_geometry(fixture, expected, edit->es_int.idb_ptr);
        }

        bu_log("%s\t%d\t%s\t%s%s%s\n", fixture->name,
               operation.command_id, unit, ok ? "pass" : "fail",
               command && command->label ? "\t" : "",
               command && command->label ? command->label : "");
        if (!ok)
            ++failures;
        bu_free(expected, "operation matrix expected geometry");
        if (edit)
            rt_edit_destroy(edit);
        db_free_full_path(&path);
    }
    db_close(dbip);
    return failures;
}

enum {
    ARBN_INITIAL_PLANES = 6,
    ARBN_MAX_PLANES = 7,
    ARBN_EDITED_PLANE = 2
};

static bool
same_arbn(const struct rt_edit *edit, const plane_t expected[ARBN_MAX_PLANES],
          size_t expected_count)
{
    const struct rt_arbn_internal *arbn =
        (const struct rt_arbn_internal *)edit->es_int.idb_ptr;
    if (arbn->neqn != expected_count) {
        bu_log("ARBN: expected %zu planes, got %zu\n", expected_count, arbn->neqn);
        return false;
    }
    for (size_t i = 0; i < expected_count; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            if (!NEAR_EQUAL(expected[i][j], arbn->eqn[i][j], VUNITIZE_TOL)) {
                bu_log("ARBN plane %zu[%zu]: expected %.17g, got %.17g\n",
                       i, j, expected[i][j], arbn->eqn[i][j]);
                return false;
            }
        }
    }
    return true;
}

static int
run_arbn_step(struct rt_edit *edit, int command_id, const fastf_t *params,
              int nparams, const plane_t expected[ARBN_MAX_PLANES],
              size_t expected_count, const char *unit, bool rejected)
{
    const struct rt_edit_cmd_desc *command = find_command(ID_ARBN, command_id);
    if (!command || !EDOBJ[ID_ARBN].ft_set_edit_mode)
        return 1;

    EDOBJ[ID_ARBN].ft_set_edit_mode(edit, command_id);
    edit->e_inpara = nparams;
    for (int i = 0; i < nparams; ++i)
        edit->e_para[i] = params[i];
    int result = rt_edit_process(edit);
    bool ok = ((rejected && result != BRLCAD_OK) ||
               (!rejected && result == BRLCAD_OK)) &&
        same_arbn(edit, expected, expected_count);
    if (ok && command_id == ECMD_ARBN_PLANE_SELECT) {
        fastf_t selected[4] = {};
        ok = EDOBJ[ID_ARBN].ft_edit_get_params(edit, command_id, selected) == 1 &&
            NEAR_EQUAL(selected[0], ARBN_EDITED_PLANE, VUNITIZE_TOL);
    }
    if (!ok)
        bu_log("ARBN command %d returned %d: %s\n", command_id, result,
               bu_vls_cstr(edit->log_str));
    bu_log("arbn\t%d\t%s\t%s\t%s\n", command_id, unit,
           ok ? "pass" : "fail", command->label);
    return ok ? 0 : 1;
}

static int
run_arbn_unit(fastf_t local2base, const char *unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
        return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct rt_arbn_internal *geometry;
    BU_ALLOC(geometry, struct rt_arbn_internal);
    geometry->magic = RT_ARBN_INTERNAL_MAGIC;
    geometry->neqn = ARBN_INITIAL_PLANES;
    geometry->eqn = (plane_t *)bu_calloc(geometry->neqn, sizeof(plane_t),
                                        "operation matrix ARBN planes");
    HSET(geometry->eqn[0], 1, 0, 0, inch_to_mm);
    HSET(geometry->eqn[1], -1, 0, 0, inch_to_mm);
    HSET(geometry->eqn[2], 0, 1, 0, inch_to_mm);
    HSET(geometry->eqn[3], 0, -1, 0, inch_to_mm);
    HSET(geometry->eqn[4], 0, 0, 1, inch_to_mm);
    HSET(geometry->eqn[5], 0, 0, -1, inch_to_mm);

    plane_t expected[ARBN_MAX_PLANES] = {};
    for (size_t i = 0; i < ARBN_INITIAL_PLANES; ++i)
        HMOVE(expected[i], geometry->eqn[i]);

    if (wdb_export(wdbp, "arbn", geometry, ID_ARBN, 1.0) != 0) {
        bu_log("Cannot create ARBN operation fixture\n");
        db_close(dbip);
        return 1;
    }
    struct directory *dp = db_lookup(dbip, "arbn", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
        db_close(dbip);
        return 1;
    }
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_edit *edit = rt_edit_create(&path, dbip, &tol, NULL);
    int failures = 0;
    if (!edit) {
        failures = 1;
    } else {
        edit->mv_context = 1;
        const fastf_t select[] = {ARBN_EDITED_PLANE};
        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_SELECT, select, 1,
                                  expected, ARBN_INITIAL_PLANES, unit, false);

        const fastf_t distance[] = {3 * inch_to_mm / local2base};
        expected[ARBN_EDITED_PLANE][3] = 3 * inch_to_mm;
        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_SET_DIST, distance, 1,
                                  expected, ARBN_INITIAL_PLANES, unit, false);

        const fastf_t normal[] = {1, 0, 0};
        HSET(expected[ARBN_EDITED_PLANE], 1, 0, 0, 3 * inch_to_mm);
        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_SET_NORM, normal, 3,
                                  expected, ARBN_INITIAL_PLANES, unit, false);

        const fastf_t nonunit_normal[] = {2, 0, 0};
        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_SET_NORM,
                                  nonunit_normal, 3, expected,
                                  ARBN_INITIAL_PLANES, unit, false);

        const fastf_t zero_normal[] = {0, 0, 0};
        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_SET_NORM, zero_normal, 3,
                                  expected, ARBN_INITIAL_PLANES, unit, true);

        const fastf_t rotation[] = {0, 0, 90};
        HSET(expected[ARBN_EDITED_PLANE], 0, 1, 0, 3 * inch_to_mm);
        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_ROTATE, rotation, 3,
                                  expected, ARBN_INITIAL_PLANES, unit, false);

        const fastf_t added[] = {0, 0, 2, 4 * inch_to_mm / local2base};
        HSET(expected[ARBN_INITIAL_PLANES], 0, 0, 1, 2 * inch_to_mm);
        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_ADD, added, 4,
                                  expected, ARBN_MAX_PLANES, unit, false);

        failures += run_arbn_step(edit, ECMD_ARBN_PLANE_DEL, NULL, 0,
                                  expected, ARBN_INITIAL_PLANES, unit, false);
        rt_edit_destroy(edit);
    }
    db_free_full_path(&path);
    db_close(dbip);
    return failures;
}

int
rt_edit_test_operation_matrix(void)
{
    struct rt_ell_internal ell = {};
    ell.magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell.v, inch_to_mm, 0, 0);
    VSET(ell.a, 2 * inch_to_mm, 0, 0);
    VSET(ell.b, 0, 3 * inch_to_mm, 0);
    VSET(ell.c, 0, 0, 4 * inch_to_mm);

    struct rt_part_internal part = {};
    part.part_magic = RT_PART_INTERNAL_MAGIC;
    VSET(part.part_V, inch_to_mm, 0, 0);
    VSET(part.part_H, 0, 0, 3 * inch_to_mm);
    part.part_vrad = 2 * inch_to_mm;
    part.part_hrad = inch_to_mm;

    struct rt_superell_internal superell = {};
    superell.magic = RT_SUPERELL_INTERNAL_MAGIC;
    VSET(superell.v, inch_to_mm, 0, 0);
    VSET(superell.a, 2 * inch_to_mm, 0, 0);
    VSET(superell.b, 0, 3 * inch_to_mm, 0);
    VSET(superell.c, 0, 0, 4 * inch_to_mm);
    superell.n = 1.5;
    superell.e = 0.75;

    struct rt_epa_internal epa = {};
    epa.epa_magic = RT_EPA_INTERNAL_MAGIC;
    VSET(epa.epa_V, inch_to_mm, 0, 0);
    VSET(epa.epa_H, 0, 0, 3 * inch_to_mm);
    VSET(epa.epa_Au, 1, 0, 0);
    epa.epa_r1 = 2 * inch_to_mm;
    epa.epa_r2 = inch_to_mm;

    struct rt_ehy_internal ehy = {};
    ehy.ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VSET(ehy.ehy_V, inch_to_mm, 0, 0);
    VSET(ehy.ehy_H, 0, 0, 3 * inch_to_mm);
    VSET(ehy.ehy_Au, 1, 0, 0);
    ehy.ehy_r1 = 2 * inch_to_mm;
    ehy.ehy_r2 = inch_to_mm;
    ehy.ehy_c = inch_to_mm / 2;

    const struct primitive_fixture fixtures[FIXTURE_COUNT] = {
        {"ell", ID_ELL, &ell, sizeof(ell), ell_fields,
            FIELD_COUNT(ell_fields), 5 * inch_to_mm},
        {"part", ID_PARTICLE, &part, sizeof(part), part_fields,
            FIELD_COUNT(part_fields), 1.5 * inch_to_mm},
        {"superell", ID_SUPERELL, &superell, sizeof(superell), superell_fields,
            FIELD_COUNT(superell_fields), 5 * inch_to_mm},
        {"epa", ID_EPA, &epa, sizeof(epa), epa_fields,
            FIELD_COUNT(epa_fields), 1.5 * inch_to_mm},
        {"ehy", ID_EHY, &ehy, sizeof(ehy), ehy_fields,
            FIELD_COUNT(ehy_fields), 1.5 * inch_to_mm}
    };

    bu_log("primitive\tcommand_id\tunits\tresult\tcommand\n");
    int failures = run_unit(fixtures, 1.0, "mm");
    failures += run_unit(fixtures, inch_to_mm, "in");
    failures += run_arbn_unit(1.0, "mm");
    failures += run_arbn_unit(inch_to_mm, "in");
    return failures ? BRLCAD_ERROR : BRLCAD_OK;
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
