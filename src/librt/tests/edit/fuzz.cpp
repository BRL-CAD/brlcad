/*                         F U Z Z . C P P
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
/** @file fuzz.cpp
 *
 * Bounded, stateful BOT and ARS edit sequences.  The same entry point is
 * used by the deterministic regression case and the optional libFuzzer
 * executable.
 */

#include "common.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "bu/exit.h"
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"
#include "rt/primitives/bot.h"

struct directory *make_bot(struct rt_wdb *);
struct directory *make_ars(struct rt_wdb *);

#define ECMD_BOT_MOVEV 30064
#define ECMD_BOT_MOVEE 30065
#define ECMD_BOT_MOVET 30066
#define ECMD_BOT_MODE 30067
#define ECMD_BOT_THICK 30069
#define ECMD_BOT_FDEL 30071
#define ECMD_BOT_ESPLIT 30074
#define ECMD_BOT_FSPLIT 30075

#define ECMD_ARS_NEXT_PT 5035
#define ECMD_ARS_PREV_PT 5036
#define ECMD_ARS_MOVE_PT 5039
#define ECMD_ARS_DEL_CRV 5040
#define ECMD_ARS_DEL_COL 5041
#define ECMD_ARS_DUP_CRV 5042
#define ECMD_ARS_DUP_COL 5043
#define ECMD_ARS_MOVE_CRV 5044
#define ECMD_ARS_MOVE_COL 5045
#define ECMD_ARS_SCALE_CRV 5048
#define ECMD_ARS_SCALE_COL 5049
#define ECMD_ARS_INSERT_CRV 5050

static const size_t edit_record_size = 6;
static const size_t max_edit_steps = 32;
static const size_t max_bot_vertices = 64;
static const size_t max_bot_faces = 128;
static const size_t max_ars_curves = 8;
static const size_t max_ars_points = 8;
static const fastf_t inch_to_mm = 25.4;

struct ars_edit_state {
    int curve;
    int column;
    point_t point;
};

static fastf_t
coordinate(uint8_t value)
{
    return (static_cast<int>(value) - 128) / 16.0;
}

static void
check_bot(const struct rt_bot_internal *bot)
{
    RT_BOT_CK_MAGIC(bot);
    if (!bot->vertices || !bot->faces || !bot->num_vertices ||
	!bot->num_faces || bot->num_vertices > max_bot_vertices ||
	bot->num_faces > max_bot_faces)
	bu_bomb("BOT size or storage invariant failed\n");

    for (size_t i = 0; i < bot->num_vertices * 3; ++i)
	if (!std::isfinite(bot->vertices[i]))
	    bu_bomb("BOT vertex is not finite\n");
    for (size_t i = 0; i < bot->num_faces * 3; ++i)
	if (bot->faces[i] < 0 || (size_t)bot->faces[i] >= bot->num_vertices)
	    bu_bomb("BOT face index is invalid\n");
    if ((bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) &&
	(!bot->thickness || !bot->face_mode))
	bu_bomb("BOT plate data is missing\n");
}

static void
check_ars(const struct rt_ars_internal *ars)
{
    RT_ARS_CK_MAGIC(ars);
    if (!ars->curves || ars->ncurves < 2 || ars->ncurves > max_ars_curves ||
	ars->pts_per_curve < 2 || ars->pts_per_curve > max_ars_points)
	bu_bomb("ARS size or storage invariant failed\n");

    for (size_t i = 0; i < ars->ncurves; ++i) {
	if (!ars->curves[i])
	    bu_bomb("ARS curve storage is missing\n");
	for (size_t j = 0; j <= ars->pts_per_curve; ++j)
	    for (size_t k = 0; k < 3; ++k)
		if (!std::isfinite(ars->curves[i][j * 3 + k]))
		    bu_bomb("ARS point is not finite\n");
	if (!VNEAR_EQUAL(ars->curves[i],
		&ars->curves[i][ars->pts_per_curve * 3], VUNITIZE_TOL))
	    bu_bomb("ARS curve is not closed\n");
    }
}

static void
edit_bot(struct rt_edit *s, const uint8_t *step)
{
    static const int commands[] = {
	ECMD_BOT_MOVEV, ECMD_BOT_MOVEE, ECMD_BOT_MOVET,
	ECMD_BOT_ESPLIT, ECMD_BOT_FSPLIT, ECMD_BOT_FDEL,
	ECMD_BOT_MODE, ECMD_BOT_THICK
    };
    struct rt_bot_internal *bot = (struct rt_bot_internal *)s->es_int.idb_ptr;
    struct rt_bot_edit *selection = (struct rt_bot_edit *)s->ipe_ptr;
    int cmd = commands[step[0] % (sizeof(commands) / sizeof(commands[0]))];
    size_t face = step[1] % bot->num_faces;
    int v0 = bot->faces[face * 3];
    int v1 = bot->faces[face * 3 + 1];
    int v2 = bot->faces[face * 3 + 2];

    if ((cmd == ECMD_BOT_ESPLIT || cmd == ECMD_BOT_FSPLIT) &&
	(bot->num_vertices + 1 > max_bot_vertices ||
	 bot->num_faces + 2 > max_bot_faces))
	return;
    if (cmd == ECMD_BOT_FDEL && bot->num_faces <= 1)
	return;

    selection->bot_verts[0] = v0;
    selection->bot_verts[1] = v1;
    selection->bot_verts[2] = v2;
    s->e_inpara = 0;
    VSETALL(s->e_para, 0.0);
    if (cmd == ECMD_BOT_MOVEV || cmd == ECMD_BOT_MOVEE ||
	cmd == ECMD_BOT_MOVET) {
	s->e_inpara = 3;
	VSET(s->e_para, coordinate(step[2]), coordinate(step[3]),
	     coordinate(step[4]));
	if (cmd == ECMD_BOT_MOVEV)
	    selection->bot_verts[1] = selection->bot_verts[2] = -1;
	else if (cmd == ECMD_BOT_MOVEE)
	    selection->bot_verts[2] = -1;
    } else if (cmd == ECMD_BOT_ESPLIT) {
	selection->bot_verts[2] = -1;
    } else if (cmd == ECMD_BOT_MODE) {
	s->e_inpara = 1;
	s->e_para[0] = step[2] & 1 ? RT_BOT_PLATE : RT_BOT_SOLID;
    } else if (cmd == ECMD_BOT_THICK) {
	if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS)
	    return;
	s->e_inpara = 1;
	s->e_para[0] = (step[2] + 1) / 16.0;
    }

    EDOBJ[ID_BOT].ft_set_edit_mode(s, cmd);
    rt_edit_process(s);
    check_bot(bot);
}

static void
edit_ars(struct rt_edit *s, const uint8_t *step)
{
    static const int commands[] = {
	ECMD_ARS_MOVE_PT, ECMD_ARS_MOVE_CRV, ECMD_ARS_MOVE_COL,
	ECMD_ARS_SCALE_CRV, ECMD_ARS_SCALE_COL,
	ECMD_ARS_DUP_CRV, ECMD_ARS_DUP_COL, ECMD_ARS_INSERT_CRV,
	ECMD_ARS_DEL_CRV, ECMD_ARS_DEL_COL,
	ECMD_ARS_NEXT_PT, ECMD_ARS_PREV_PT
    };
    struct rt_ars_internal *ars = (struct rt_ars_internal *)s->es_int.idb_ptr;
    struct ars_edit_state *selection = (struct ars_edit_state *)s->ipe_ptr;
    int cmd = commands[step[0] % (sizeof(commands) / sizeof(commands[0]))];

    if ((cmd == ECMD_ARS_DUP_CRV || cmd == ECMD_ARS_INSERT_CRV) &&
	ars->ncurves >= max_ars_curves)
	return;
    if (cmd == ECMD_ARS_DUP_COL && ars->pts_per_curve >= max_ars_points)
	return;

    selection->curve = step[1] % ars->ncurves;
    selection->column = step[2] % ars->pts_per_curve;
    VMOVE(selection->point,
	  &ars->curves[selection->curve][selection->column * 3]);
    s->e_inpara = 0;
    VSETALL(s->e_para, 0.0);
    if (cmd == ECMD_ARS_MOVE_PT || cmd == ECMD_ARS_MOVE_CRV ||
	cmd == ECMD_ARS_MOVE_COL) {
	s->e_inpara = 3;
	VSET(s->e_para, coordinate(step[3]), coordinate(step[4]),
	     coordinate(step[5]));
    } else if (cmd == ECMD_ARS_SCALE_CRV || cmd == ECMD_ARS_SCALE_COL) {
	s->e_inpara = 1;
	s->e_para[0] = 0.5 + step[3] / 128.0;
    }

    EDOBJ[ID_ARS].ft_set_edit_mode(s, cmd);
    rt_edit_process(s);
    check_ars(ars);
}

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (!data || size < edit_record_size + 1)
	return 0;

    const int type = (data[0] & 1) ? ID_ARS : ID_BOT;
    struct db_i *dbip = db_open_inmem();
    if (!dbip)
	bu_exit(1, "Cannot create in-memory database\n");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp)
	bu_exit(1, "Cannot open in-memory database writer\n");
    struct directory *dp = type == ID_ARS ? make_ars(wdbp) : make_bot(wdbp);

    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bview *view;
    BU_GET(view, struct bview);
    bv_init(view, NULL);
    VSET(view->gv_aet, 45, 35, 0);
    bv_mat_aet(view);
    view->gv_size = 73.3197;
    view->gv_isize = 1.0 / view->gv_size;
    view->gv_scale = 0.5 * view->gv_size;
    bv_update(view);

    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_edit *s = rt_edit_create(&path, dbip, &tol, view);
    if (!s)
	bu_exit(1, "Cannot create primitive edit session\n");
    s->mv_context = 1;
    s->local2base = (data[0] & 2) ? inch_to_mm : 1.0;
    s->base2local = 1.0 / s->local2base;

    size_t steps = (size - 1) / edit_record_size;
    if (steps > max_edit_steps)
	steps = max_edit_steps;
    for (size_t i = 0; i < steps; ++i) {
	const uint8_t *step = data + 1 + i * edit_record_size;
	if (type == ID_BOT)
	    edit_bot(s, step);
	else
	    edit_ars(s, step);
    }

    rt_edit_destroy(s);
    db_free_full_path(&path);
    bv_free(view);
    db_close(dbip);
    return 0;
}

int
rt_edit_test_fuzz(void)
{
    uint8_t input[1 + 2 * edit_record_size] = {};
    for (int type = 0; type < 2; ++type) {
	for (int units = 0; units < 2; ++units) {
	    input[0] = type | (units << 1);
	    for (unsigned op = 0; op < 12; ++op) {
		input[1] = static_cast<uint8_t>(op);
		input[2] = static_cast<uint8_t>(op);
		input[3] = 1;
		input[4] = 140;
		input[5] = 150;
		input[6] = 160;
		input[7] = static_cast<uint8_t>(op + 1);
		input[8] = 0;
		input[9] = 0;
		input[10] = 128;
		input[11] = 128;
		input[12] = 128;
		LLVMFuzzerTestOneInput(input, sizeof(input));
	    }
	}
    }
    return 0;
}
