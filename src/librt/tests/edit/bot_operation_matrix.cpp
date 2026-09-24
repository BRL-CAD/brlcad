/*           B O T _ O P E R A T I O N _ M A T R I X . C P P
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
/** @file bot_operation_matrix.cpp
 *
 * Check BOT descriptor edits against the complete mesh in mm and inch
 * databases.  The fixture is shared with the existing BOT edit test.
 */

#include "common.h"

#include <string.h>
#include <vector>

#include "bu/bitv.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/rt_ecmds.h"
#include "rt/primitives/bot.h"

struct directory *make_bot(struct rt_wdb *wdbp);

enum {
    BOT_TEST_VERTICES = 4,
    BOT_TEST_FACES = 4,
    BOT_TEST_COORDS = 3 * BOT_TEST_VERTICES,
    BOT_TEST_INDICES = 3 * BOT_TEST_FACES
};

static const fastf_t inch_to_mm = 25.4;

static const int expected_faces[BOT_TEST_INDICES] = {
    0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3
};

struct bot_expected {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
    std::vector<fastf_t> thickness;
    std::vector<unsigned char> face_mode;
    unsigned char mode;
    unsigned char orientation;
    unsigned char flags;
    bool plate;
};

static struct bot_expected
initial_bot_expected(void)
{
    const fastf_t vertices[BOT_TEST_COORDS] = {
        0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
    };
    struct bot_expected expected = {};
    expected.vertices.assign(vertices, vertices + BOT_TEST_COORDS);
    expected.faces.assign(expected_faces, expected_faces + BOT_TEST_INDICES);
    expected.mode = RT_BOT_SOLID;
    expected.orientation = RT_BOT_CCW;
    expected.flags = 0;
    expected.plate = false;
    return expected;
}

static const struct rt_edit_cmd_desc *
bot_command(int command_id)
{
    if (!EDOBJ[ID_BOT].ft_edit_desc)
        return NULL;
    const struct rt_edit_prim_desc *desc = EDOBJ[ID_BOT].ft_edit_desc();
    if (!desc)
        return NULL;
    for (int i = 0; i < desc->ncmd; ++i) {
        if (desc->cmds[i].cmd_id == command_id)
            return &desc->cmds[i];
    }
    return NULL;
}

static bool
same_bot(const struct rt_edit *edit, const struct bot_expected *expected)
{
    const struct rt_bot_internal *bot =
        (const struct rt_bot_internal *)edit->es_int.idb_ptr;
    if (expected->vertices.size() % 3 || expected->faces.size() % 3 ||
        bot->num_vertices != expected->vertices.size() / 3 ||
        bot->num_faces != expected->faces.size() / 3 ||
        bot->mode != expected->mode ||
        bot->orientation != expected->orientation ||
        bot->bot_flags != expected->flags || !bot->faces || !bot->vertices ||
        memcmp(bot->faces, expected->faces.data(),
               expected->faces.size() * sizeof(int)) != 0) {
        bu_log("BOT topology or properties changed unexpectedly\n");
        return false;
    }
    if (expected->plate != (bot->thickness && bot->face_mode)) {
        bu_log("BOT plate data presence changed unexpectedly\n");
        return false;
    }
    if (expected->plate) {
        if (expected->thickness.size() != bot->num_faces ||
            expected->face_mode.size() != bot->num_faces)
            return false;
        for (size_t i = 0; i < bot->num_faces; ++i) {
            if (!NEAR_EQUAL(bot->thickness[i], expected->thickness[i],
                            VUNITIZE_TOL) ||
                (bool)BU_BITTEST(bot->face_mode, i) != expected->face_mode[i]) {
                bu_log("BOT face property %zu changed unexpectedly\n", i);
                return false;
            }
        }
    } else if (bot->thickness || bot->face_mode ||
               !expected->thickness.empty() || !expected->face_mode.empty()) {
        bu_log("BOT solid retained plate data\n");
        return false;
    }
    for (size_t i = 0; i < expected->vertices.size(); ++i) {
        if (!NEAR_EQUAL(bot->vertices[i], expected->vertices[i],
                        VUNITIZE_TOL)) {
            bu_log("BOT vertex coordinate %zu: expected %.17g, got %.17g\n",
                   i, expected->vertices[i], bot->vertices[i]);
            return false;
        }
    }
    return true;
}

static bool
check_bot_params(struct rt_edit *edit, int command_id, int expected_count,
                 const fastf_t *expected, const char *unit)
{
    fastf_t values[3] = {};
    int count = EDOBJ[ID_BOT].ft_edit_get_params(edit, command_id, values);
    bool ok = count == expected_count;
    for (int i = 0; ok && i < expected_count; ++i)
        ok = NEAR_EQUAL(values[i], expected[i], VUNITIZE_TOL);
    if (!ok)
        bu_log("BOT %s parameter getter %d returned %d values\n",
               unit, command_id, count);
    return ok;
}

static void
expected_bot_point(fastf_t *point, const struct bot_expected *expected,
                   const int *vertices, int count, fastf_t base2local)
{
    VSETALL(point, 0.0);
    for (int i = 0; i < count; ++i)
        VADD2(point, point, &expected->vertices[vertices[i] * 3]);
    VSCALE(point, point, base2local / count);
}

static int
run_bot_step(struct rt_edit *edit, int command_id, const fastf_t *params,
             int nparams, const struct bot_expected *expected,
             const char *unit, bool rejected, int selected_count,
             const fastf_t *selected)
{
    const struct rt_edit_cmd_desc *command = bot_command(command_id);
    if (!command || !EDOBJ[ID_BOT].ft_set_edit_mode)
        return 1;

    EDOBJ[ID_BOT].ft_set_edit_mode(edit, command_id);
    edit->e_inpara = nparams;
    for (int i = 0; i < nparams; ++i)
        edit->e_para[i] = params[i];
    int result = rt_edit_process(edit);
    bool ok = ((rejected && result != BRLCAD_OK) ||
               (!rejected && result == BRLCAD_OK)) && same_bot(edit, expected);
    if (ok && selected_count > 0)
        ok = check_bot_params(edit, command_id, selected_count,
                              selected, unit);
    if (!ok)
        bu_log("BOT command %d returned %d\n", command_id, result);
    bu_log("bot\t%d\t%s\t%s\t%s\n", command_id, unit,
           ok ? "pass" : "fail", command->label);
    return ok ? 0 : 1;
}

static int
run_bot_repair_step(struct rt_edit *edit, const struct bn_tol *tol,
                    const struct bot_expected *expected, const char *unit,
                    int expected_result)
{
    struct bu_vls log = BU_VLS_INIT_ZERO;
    int result = EDOBJ[ID_BOT].ft_repair ?
        EDOBJ[ID_BOT].ft_repair(&log, &edit->es_int, tol, 0, NULL) :
        BRLCAD_ERROR;
    bool ok = result == expected_result && same_bot(edit, expected);
    bu_log("bot\trepair\t%s\t%s\t%s\n", unit,
           ok ? "pass" : "fail", bu_vls_cstr(&log));
    bu_vls_free(&log);
    return ok ? 0 : 1;
}

static int
run_bot_repair_request(struct rt_db_internal *ip, const struct bn_tol *tol,
                       const char *unit, const char *name, int argc,
                       const char **argv, int expected_result,
                       const char *expected_text)
{
    struct bu_vls log = BU_VLS_INIT_ZERO;
    void *original = ip->idb_ptr;
    int result = EDOBJ[ID_BOT].ft_repair ?
        EDOBJ[ID_BOT].ft_repair(&log, ip, tol, argc, argv) :
        BRLCAD_ERROR;
    bool ok = result == expected_result && ip->idb_ptr == original &&
        strstr(bu_vls_cstr(&log), expected_text);
    bu_log("bot\trepair %s\t%s\t%s\t%s\n", name, unit,
           ok ? "pass" : "fail", bu_vls_cstr(&log));
    bu_vls_free(&log);
    return ok ? 0 : 1;
}

static int
run_bot_solid_repair(fastf_t length, const char *unit)
{
    const fastf_t vertices[BOT_TEST_COORDS] = {
        0, 0, 0, length, 0, 0, 0, length, 0, 0, 0, length
    };
    const int faces[BOT_TEST_INDICES] = {
        0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3
    };
    struct rt_bot_internal *bot;
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->num_vertices = BOT_TEST_VERTICES;
    bot->vertices = (fastf_t *)bu_malloc(sizeof(vertices), "repair BOT vertices");
    memcpy(bot->vertices, vertices, sizeof(vertices));
    bot->num_faces = BOT_TEST_FACES;
    bot->faces = (int *)bu_malloc(sizeof(faces), "repair BOT faces");
    memcpy(bot->faces, faces, sizeof(faces));

    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = ID_BOT;
    ip.idb_meth = &OBJ[ID_BOT];
    ip.idb_ptr = bot;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    int failures = 0;
    const char *json_args[] = {"--options-json"};
    failures += run_bot_repair_request(&ip, &tol, unit, "options-json",
                                       1, json_args, 1, "max-hole-area");
    const char *help_args[] = {"--help"};
    failures += run_bot_repair_request(&ip, &tol, unit, "help",
                                       1, help_args, -1, "max-hole-percent");
    const char *area_args[] = {"--max-hole-area", "100"};
    failures += run_bot_repair_request(&ip, &tol, unit, "max-hole-area",
                                       2, area_args, BRLCAD_OK,
                                       "already manifold");
    const char *percent_args[] = {"--max-hole-percent", "5"};
    failures += run_bot_repair_request(&ip, &tol, unit,
                                       "max-hole-percent", 2,
                                       percent_args, BRLCAD_OK,
                                       "already manifold");
    const char *invalid_args[] = {"--not-a-repair-option"};
    failures += run_bot_repair_request(&ip, &tol, unit, "invalid option",
                                       1, invalid_args, -1,
                                       "Invalid repair options");
    int result = EDOBJ[ID_BOT].ft_repair ?
        EDOBJ[ID_BOT].ft_repair(&log, &ip, &tol, 0, NULL) :
        BRLCAD_ERROR;
    bot = (struct rt_bot_internal *)ip.idb_ptr;
    bool ok = result == BRLCAD_OK && bot &&
        bot->num_vertices == BOT_TEST_VERTICES &&
        bot->num_faces == BOT_TEST_FACES && bot->vertices && bot->faces &&
        bot->mode == RT_BOT_SOLID && bot->orientation == RT_BOT_CCW &&
        bot->bot_flags == 0 && !bot->thickness && !bot->face_mode &&
        bot->num_normals == 0 && !bot->normals &&
        bot->num_uvs == 0 && !bot->uvs &&
        memcmp(bot->faces, faces, sizeof(faces)) == 0;
    for (size_t i = 0; ok && i < BOT_TEST_COORDS; ++i)
        ok = NEAR_EQUAL(bot->vertices[i], vertices[i], VUNITIZE_TOL);
    bu_log("bot\trepair\t%s\t%s\t%s\n", unit,
           ok ? "pass" : "fail", bu_vls_cstr(&log));
    bu_vls_free(&log);
    rt_db_free_internal(&ip);
    return failures + (ok ? 0 : 1);
}

static int
run_bot_merged_repair(fastf_t length, const char *unit)
{
    const fastf_t vertices[] = {
        0, 0, 0, length, 0, 0, 0, length, 0,
        0, 0, length, 0, 0, 0
    };
    const int faces[BOT_TEST_INDICES] = {
        0, 2, 1, 4, 1, 3, 0, 3, 2, 1, 2, 3
    };
    const int repaired_faces[BOT_TEST_INDICES] = {
        0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3
    };
    struct rt_bot_internal *bot;
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->num_vertices = BOT_TEST_VERTICES + 1;
    bot->vertices = (fastf_t *)bu_malloc(sizeof(vertices),
                                          "repair BOT duplicate vertices");
    memcpy(bot->vertices, vertices, sizeof(vertices));
    bot->num_faces = BOT_TEST_FACES;
    bot->faces = (int *)bu_malloc(sizeof(faces),
                                  "repair BOT duplicate faces");
    memcpy(bot->faces, faces, sizeof(faces));

    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    ip.idb_type = ID_BOT;
    ip.idb_meth = &OBJ[ID_BOT];
    ip.idb_ptr = bot;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bu_vls log = BU_VLS_INIT_ZERO;
    int result = EDOBJ[ID_BOT].ft_repair ?
        EDOBJ[ID_BOT].ft_repair(&log, &ip, &tol, 0, NULL) :
        BRLCAD_ERROR;
    bot = (struct rt_bot_internal *)ip.idb_ptr;
    bool ok = result == BRLCAD_OK && bot &&
        bot->num_vertices == BOT_TEST_VERTICES &&
        bot->num_faces == BOT_TEST_FACES && bot->vertices && bot->faces &&
        bot->mode == RT_BOT_SOLID && bot->orientation == RT_BOT_CCW &&
        strstr(bu_vls_cstr(&log), "Successfully repaired BoT");
    int mapped[BOT_TEST_VERTICES] = {-1, -1, -1, -1};
    bool used_vertex[BOT_TEST_VERTICES] = {};
    for (int i = 0; ok && i < BOT_TEST_VERTICES; ++i) {
        for (int j = 0; j < BOT_TEST_VERTICES; ++j) {
            if (VNEAR_EQUAL(&bot->vertices[i * 3], &vertices[j * 3],
                            VUNITIZE_TOL)) {
                mapped[i] = j;
                break;
            }
        }
        if (mapped[i] < 0 || used_vertex[mapped[i]])
            ok = false;
        else
            used_vertex[mapped[i]] = true;
    }
    bool used_face[BOT_TEST_FACES] = {};
    for (int i = 0; ok && i < BOT_TEST_FACES; ++i) {
        int face[3];
        for (int corner = 0; corner < 3; ++corner) {
            int vertex = bot->faces[i * 3 + corner];
            if (vertex < 0 || vertex >= BOT_TEST_VERTICES) {
                ok = false;
                break;
            }
            face[corner] = mapped[vertex];
        }
        for (int j = 0; ok && j < BOT_TEST_FACES; ++j) {
            if (used_face[j])
                continue;
            for (int rotation = 0; rotation < 3; ++rotation) {
                if (face[0] == repaired_faces[j * 3 + rotation] &&
                    face[1] == repaired_faces[j * 3 + (rotation + 1) % 3] &&
                    face[2] == repaired_faces[j * 3 + (rotation + 2) % 3]) {
                    used_face[j] = true;
                    break;
                }
            }
            if (used_face[j])
                break;
        }
    }
    for (int i = 0; ok && i < BOT_TEST_FACES; ++i)
        ok = used_face[i];
    bu_log("bot\trepair merged vertex\t%s\t%s\t%s\n", unit,
           ok ? "pass" : "fail", bu_vls_cstr(&log));
    bu_vls_free(&log);
    rt_db_free_internal(&ip);
    return ok ? 0 : 1;
}

static int
run_bot_unit(fastf_t local2base, const char *unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
        return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_bot(wdbp);
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
        struct bot_expected expected = initial_bot_expected();
        const int vertex_indices[] = {1};
        const int edge_indices[] = {0, 1};
        const int face_indices[] = {0, 1, 2};
        fastf_t point[3];
        if (!check_bot_params(edit, ECMD_BOT_MOVEV, 0, NULL, unit) ||
            !check_bot_params(edit, ECMD_BOT_MOVEE, 0, NULL, unit) ||
            !check_bot_params(edit, ECMD_BOT_MOVET, 0, NULL, unit))
            ++failures;
        const fastf_t invalid_vertex[] = {BOT_TEST_VERTICES};
        failures += run_bot_step(edit, ECMD_BOT_PICKV, invalid_vertex, 1,
                                 &expected, unit, true, 0, NULL);
        const fastf_t vertex[] = {1};
        failures += run_bot_step(edit, ECMD_BOT_PICKV, vertex, 1,
                                 &expected, unit, false, 1, vertex);
        expected_bot_point(point, &expected, vertex_indices, 1,
                           dbip->dbi_base2local);
        if (!check_bot_params(edit, ECMD_BOT_MOVEV, 3, point, unit))
            ++failures;

        const fastf_t move_vertex[] = {2 * inch_to_mm / local2base, 0, 0};
        expected.vertices[3] = 2 * inch_to_mm;
        failures += run_bot_step(edit, ECMD_BOT_MOVEV, move_vertex, 3,
                                 &expected, unit, false, 3, move_vertex);

        const fastf_t move_list[] = {0, inch_to_mm / local2base, 0, 0, 2};
        expected.vertices[1] += inch_to_mm;
        expected.vertices[7] += inch_to_mm;
        failures += run_bot_step(edit, ECMD_BOT_MOVEV_LIST, move_list, 5,
                                 &expected, unit, false, 0, NULL);

        const fastf_t invalid_list[] = {
            0, inch_to_mm / local2base, 0, 0, BOT_TEST_VERTICES
        };
        failures += run_bot_step(edit, ECMD_BOT_MOVEV_LIST, invalid_list, 5,
                                 &expected, unit, true, 0, NULL);

        const fastf_t fractional_list[] = {0, inch_to_mm / local2base, 0, 0.5};
        failures += run_bot_step(edit, ECMD_BOT_MOVEV_LIST, fractional_list, 4,
                                 &expected, unit, true, 0, NULL);

        const fastf_t edge[] = {0, 1};
        failures += run_bot_step(edit, ECMD_BOT_PICKE, edge, 2,
                                 &expected, unit, false, 2, edge);
        expected_bot_point(point, &expected, edge_indices, 2,
                           dbip->dbi_base2local);
        if (!check_bot_params(edit, ECMD_BOT_MOVEE, 3, point, unit))
            ++failures;

        const fastf_t move_edge[] = {0, 2 * inch_to_mm / local2base, 0};
        expected.vertices[1] += inch_to_mm;
        expected.vertices[4] += inch_to_mm;
        expected_bot_point(point, &expected, edge_indices, 2,
                           dbip->dbi_base2local);
        failures += run_bot_step(edit, ECMD_BOT_MOVEE, move_edge, 3,
                                 &expected, unit, false, 3, point);

        const fastf_t face[] = {0, 1, 2};
        failures += run_bot_step(edit, ECMD_BOT_PICKT, face, 3,
                                 &expected, unit, false, 3, face);
        expected_bot_point(point, &expected, face_indices, 3,
                           dbip->dbi_base2local);
        if (!check_bot_params(edit, ECMD_BOT_MOVET, 3, point, unit))
            ++failures;

        const fastf_t move_face[] = {0, 3 * inch_to_mm / local2base, 0};
        expected.vertices[1] += inch_to_mm;
        expected.vertices[4] += inch_to_mm;
        expected.vertices[7] += inch_to_mm;
        expected_bot_point(point, &expected, face_indices, 3,
                           dbip->dbi_base2local);
        failures += run_bot_step(edit, ECMD_BOT_MOVET, move_face, 3,
                                 &expected, unit, false, 3, point);

        const fastf_t plate[] = {RT_BOT_PLATE};
        expected.mode = RT_BOT_PLATE;
        expected.plate = true;
        expected.thickness.resize(expected.faces.size() / 3);
        expected.face_mode.resize(expected.faces.size() / 3);
        failures += run_bot_step(edit, ECMD_BOT_MODE, plate, 1,
                                 &expected, unit, false, 1, plate);
        failures += run_bot_repair_step(edit, &tol, &expected, unit, -1);

        const fastf_t thickness[] = {inch_to_mm / local2base};
        expected.thickness[0] = inch_to_mm;
        failures += run_bot_step(edit, ECMD_BOT_THICK, thickness, 1,
                                 &expected, unit, false, 1, thickness);

        const fastf_t invalid_thickness[] = {0};
        failures += run_bot_step(edit, ECMD_BOT_THICK, invalid_thickness, 1,
                                 &expected, unit, true, 0, NULL);

        const fastf_t face_mode[] = {1};
        expected.face_mode[0] = true;
        failures += run_bot_step(edit, ECMD_BOT_FMODE, face_mode, 1,
                                 &expected, unit, false, 1, face_mode);

        const fastf_t clockwise[] = {RT_BOT_CW};
        expected.orientation = RT_BOT_CW;
        failures += run_bot_step(edit, ECMD_BOT_ORIENT, clockwise, 1,
                                 &expected, unit, false, 1, clockwise);

        const fastf_t flags[] = {RT_BOT_USE_FLOATS};
        expected.flags = RT_BOT_USE_FLOATS;
        failures += run_bot_step(edit, ECMD_BOT_FLAGS, flags, 1,
                                 &expected, unit, false, 1, flags);

        const fastf_t solid[] = {RT_BOT_SOLID};
        expected.mode = RT_BOT_SOLID;
        expected.plate = false;
        expected.thickness.clear();
        expected.face_mode.clear();
        failures += run_bot_step(edit, ECMD_BOT_MODE, solid, 1,
                                 &expected, unit, false, 1, solid);
        rt_edit_destroy(edit);
    }
    db_free_full_path(&path);
    db_close(dbip);
    return failures;
}

static int
run_bot_topology_case(struct db_full_path *path, struct db_i *dbip,
                      struct bn_tol *tol, int command_id, const char *unit,
                      bool plate)
{
    struct rt_edit *edit = rt_edit_create(path, dbip, tol, NULL);
    if (!edit)
        return 1;
    edit->mv_context = 1;
    struct bot_expected expected = initial_bot_expected();
    struct rt_bot_internal *bot =
        (struct rt_bot_internal *)edit->es_int.idb_ptr;
    int failures = 0;

    if (plate) {
        const fastf_t mode[] = {RT_BOT_PLATE};
        expected.mode = RT_BOT_PLATE;
        expected.plate = true;
        expected.thickness.resize(expected.faces.size() / 3);
        expected.face_mode.resize(expected.faces.size() / 3);
        failures += run_bot_step(edit, ECMD_BOT_MODE, mode, 1,
                                 &expected, unit, false, 0, NULL);

        const fastf_t thickness[] = {
            inch_to_mm / dbip->dbi_local2base
        };
        expected.thickness.assign(expected.faces.size() / 3, inch_to_mm);
        failures += run_bot_step(edit, ECMD_BOT_THICK, thickness, 1,
                                 &expected, unit, false, 0, NULL);

        const fastf_t face[] = {0, 1, 2};
        failures += run_bot_step(edit, ECMD_BOT_PICKT, face, 3,
                                 &expected, unit, false, 3, face);
        const fastf_t face_mode[] = {1};
        expected.face_mode[0] = 1;
        failures += run_bot_step(edit, ECMD_BOT_FMODE, face_mode, 1,
                                 &expected, unit, false, 0, NULL);
    }

    switch (command_id) {
        case ECMD_BOT_ESPLIT: {
            if (!plate) {
                failures += run_bot_step(edit, command_id, NULL, 0,
                                         &expected, unit, true, 0, NULL);
                const fastf_t invalid_edge[] = {0, BOT_TEST_VERTICES};
                failures += run_bot_step(edit, ECMD_BOT_PICKE, invalid_edge, 2,
                                         &expected, unit, true, 0, NULL);
                const fastf_t degenerate_edge[] = {0, 0};
                failures += run_bot_step(edit, ECMD_BOT_PICKE,
                                         degenerate_edge, 2, &expected,
                                         unit, true, 0, NULL);
            }
            const fastf_t edge[] = {0, 1};
            failures += run_bot_step(edit, ECMD_BOT_PICKE, edge, 2,
                                     &expected, unit, false, 2, edge);
            expected.vertices.insert(expected.vertices.end(), {0.5, 0, 0});
            expected.faces = {
                0, 4, 2, 0, 4, 3, 0, 2, 3, 1, 2, 3, 4, 1, 2, 4, 1, 3
            };
            if (plate) {
                expected.thickness.push_back(expected.thickness[0]);
                expected.thickness.push_back(expected.thickness[1]);
                expected.face_mode.push_back(expected.face_mode[0]);
                expected.face_mode.push_back(expected.face_mode[1]);
            }
            break;
        }
        case ECMD_BOT_FSPLIT: {
            if (!plate) {
                failures += run_bot_step(edit, command_id, NULL, 0,
                                         &expected, unit, true, 0, NULL);
                const fastf_t invalid_face[] = {0, 1, BOT_TEST_VERTICES};
                failures += run_bot_step(edit, ECMD_BOT_PICKT, invalid_face, 3,
                                         &expected, unit, true, 0, NULL);
                const fastf_t degenerate_face[] = {0, 0, 1};
                failures += run_bot_step(edit, ECMD_BOT_PICKT,
                                         degenerate_face, 3, &expected,
                                         unit, true, 0, NULL);
                const fastf_t face[] = {0, 1, 2};
                failures += run_bot_step(edit, ECMD_BOT_PICKT, face, 3,
                                         &expected, unit, false, 3, face);
            }
            expected.vertices.insert(expected.vertices.end(),
                                     {1.0 / 3.0, 1.0 / 3.0, 0});
            expected.faces = {
                0, 1, 4, 0, 1, 3, 0, 2, 3, 1, 2, 3, 1, 2, 4, 2, 0, 4
            };
            if (plate) {
                expected.thickness.push_back(expected.thickness[0]);
                expected.thickness.push_back(expected.thickness[0]);
                expected.face_mode.push_back(expected.face_mode[0]);
                expected.face_mode.push_back(expected.face_mode[0]);
            }
            break;
        }
        case ECMD_BOT_FDEL: {
            if (!plate) {
                failures += run_bot_step(edit, command_id, NULL, 0,
                                         &expected, unit, true, 0, NULL);
                const fastf_t face[] = {0, 1, 2};
                failures += run_bot_step(edit, ECMD_BOT_PICKT, face, 3,
                                         &expected, unit, false, 3, face);
            }
            expected.faces.erase(expected.faces.begin(),
                                 expected.faces.begin() + 3);
            if (plate) {
                expected.thickness.erase(expected.thickness.begin());
                expected.face_mode.erase(expected.face_mode.begin());
            }
            break;
        }
        case ECMD_BOT_VERTEX_FUSE:
            bot->vertices = (fastf_t *)bu_realloc(
                bot->vertices, (bot->num_vertices + 1) * 3 * sizeof(fastf_t),
                "operation matrix duplicate BOT vertex");
            VMOVE(&bot->vertices[bot->num_vertices * 3], &bot->vertices[0]);
            bot->faces[9] = (int)bot->num_vertices;
            ++bot->num_vertices;
            expected.faces[9] = 0;
            break;
        case ECMD_BOT_FACE_FUSE:
            bot->faces = (int *)bu_realloc(
                bot->faces, (bot->num_faces + 1) * 3 * sizeof(int),
                "operation matrix duplicate BOT face");
            memcpy(&bot->faces[bot->num_faces * 3], bot->faces,
                   3 * sizeof(int));
            if (plate) {
                bot->thickness = (fastf_t *)bu_realloc(
                    bot->thickness, (bot->num_faces + 1) * sizeof(fastf_t),
                    "operation matrix duplicate BOT thickness");
                bot->thickness[bot->num_faces] = bot->thickness[0];
                struct bu_bitv *new_face_mode = bu_bitv_new(bot->num_faces + 1);
                for (size_t i = 0; i < bot->num_faces; ++i) {
                    if (BU_BITTEST(bot->face_mode, i))
                        BU_BITSET(new_face_mode, i);
                }
                if (BU_BITTEST(bot->face_mode, 0))
                    BU_BITSET(new_face_mode, bot->num_faces);
                bu_bitv_free(bot->face_mode);
                bot->face_mode = new_face_mode;
            }
            ++bot->num_faces;
            break;
        default:
            rt_edit_destroy(edit);
            return 1;
    }

    failures += run_bot_step(edit, command_id, NULL, 0,
                             &expected, unit, false, 0, NULL);
    rt_edit_destroy(edit);
    return failures;
}

static int
run_bot_topology_unit(fastf_t local2base, const char *unit,
                      const char *plate_unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
        return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = make_bot(wdbp);
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    const int commands[] = {
        ECMD_BOT_ESPLIT, ECMD_BOT_FSPLIT, ECMD_BOT_FDEL,
        ECMD_BOT_VERTEX_FUSE, ECMD_BOT_FACE_FUSE
    };
    int failures = 0;
    for (int command_id : commands) {
        failures += run_bot_topology_case(&path, dbip, &tol,
                                          command_id, unit, false);
        failures += run_bot_topology_case(&path, dbip, &tol,
                                          command_id, plate_unit, true);
    }
    db_free_full_path(&path);
    db_close(dbip);
    return failures;
}

int
rt_edit_test_bot_operation_matrix(void)
{
    bu_log("primitive\tcommand_id\tunits\tresult\tcommand\n");
    int failures = run_bot_unit(1.0, "mm");
    failures += run_bot_unit(inch_to_mm, "in");
    failures += run_bot_topology_unit(1.0, "mm", "mm-plate");
    failures += run_bot_topology_unit(inch_to_mm, "in", "in-plate");
    failures += run_bot_solid_repair(1.0, "mm");
    failures += run_bot_solid_repair(inch_to_mm, "in");
    failures += run_bot_merged_repair(1.0, "mm");
    failures += run_bot_merged_repair(inch_to_mm, "in");
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
