/*                         S E T U P . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/setup.c
 *
 * routines to initialize mged
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <tcl.h>
#include <string.h>

/* common headers */
#include "vmath.h"
#include "bu/app.h"
#include "bn.h"
#include "bv/util.h"
#include "tclcad.h"
#include "ged.h"

/* local headers */
#include "./mged.h"
#include "./cmd.h"
#include "./f_cmd.h"

/* catch auto-formatting errors in this file.  be careful as there are
 * mged_display() vars that use comma too.
 */
#define COMMA ','

/* Defined in f_db.c */
extern int mged_pre_opendb_clbk(int ac, const char **av, void *gedp, void *ctx);
extern int mged_post_opendb_clbk(int ac, const char **av, void *gedp, void *ctx);
extern int mged_pre_closedb_clbk(int ac, const char **av, void *gedp, void *ctx);
extern int mged_post_closedb_clbk(int ac, const char **av, void *gedp, void *ctx);

/* Ew. Global. */
extern Tk_Window tkwin; /* in cmd.c */

extern void init_qray(void);
extern void mged_global_variable_setup(struct mged_state *s);

const char cmd3525[] = {'3', '5', COMMA, '2', '5', '\0'};
const char cmd4545[] = {'4', '5', COMMA, '4', '5', '\0'};

// We need to trigger MGED operations when opening and closing
// database files.  However, some commands like garbage_collect
// also need to do these operations, and they have no awareness
// of the extra steps MGED takes with f_opendb/f_closedb.  To
// allow both MGED and GED to do what they need, we define
// default callbacks in GEDP with MGED functions and data that
// will do the necessary work if the opendb/closedb functions
// are called at lower levels.
struct mged_opendb_ctx mged_global_db_ctx;

static struct cmdtab mged_cmdtab[] = {
    {MGED_CMD_MAGIC, "%", f_comm, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, cmd3525, f_bv_35_25, GED_FUNC_PTR_NULL, NULL}, /* 35,25 */
    {MGED_CMD_MAGIC, "3ptarb", cmd_ged_more_wrapper, ged_exec_3ptarb, NULL},
    {MGED_CMD_MAGIC, cmd4545, f_bv_45_45, GED_FUNC_PTR_NULL, NULL}, /* 45,45 */
    {MGED_CMD_MAGIC, "B", cmd_blast, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "accept", f_be_accept, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "adc", f_adc, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "adjust", cmd_ged_plain_wrapper, ged_exec_adjust, NULL},
    {MGED_CMD_MAGIC, "ae", cmd_ged_view_wrapper, ged_exec_ae, NULL},
    {MGED_CMD_MAGIC, "ae2dir", cmd_ged_plain_wrapper, ged_exec_ae2dir, NULL},
    {MGED_CMD_MAGIC, "aip", f_aip, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "analyze", cmd_ged_info_wrapper, ged_exec_analyze, NULL},
    {MGED_CMD_MAGIC, "annotate", cmd_ged_plain_wrapper, ged_exec_annotate, NULL},
    {MGED_CMD_MAGIC, "arb", cmd_ged_plain_wrapper, ged_exec_arb, NULL},
    {MGED_CMD_MAGIC, "arced", cmd_ged_plain_wrapper, ged_exec_arced, NULL},
    {MGED_CMD_MAGIC, "area", f_area, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "arot", cmd_arot, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "art", cmd_rt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "attach", f_attach, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "attr", cmd_ged_plain_wrapper, ged_exec_attr, NULL},
    {MGED_CMD_MAGIC, "autoview", cmd_autoview, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "bb", cmd_ged_plain_wrapper, ged_exec_bb, NULL},
    {MGED_CMD_MAGIC, "bev", cmd_ged_plain_wrapper, ged_exec_bev, NULL},
    {MGED_CMD_MAGIC, "bo", cmd_ged_plain_wrapper, ged_exec_bo, NULL},
    {MGED_CMD_MAGIC, "bomb", f_bomb, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "bot", cmd_ged_plain_wrapper, ged_exec_bot, NULL},
    {MGED_CMD_MAGIC, "bot_condense", cmd_ged_plain_wrapper, ged_exec_bot_condense, NULL},
    {MGED_CMD_MAGIC, "bot_decimate", cmd_ged_plain_wrapper, ged_exec_bot_decimate, NULL},
    {MGED_CMD_MAGIC, "bot_dump", cmd_ged_plain_wrapper, ged_exec_bot_dump, NULL},
    {MGED_CMD_MAGIC, "bot_exterior", cmd_ged_plain_wrapper, ged_exec_bot_exterior, NULL},
    {MGED_CMD_MAGIC, "bot_face_fuse", cmd_ged_plain_wrapper, ged_exec_bot_face_fuse, NULL},
    {MGED_CMD_MAGIC, "bot_face_sort", cmd_ged_plain_wrapper, ged_exec_bot_face_sort, NULL},
    {MGED_CMD_MAGIC, "bot_flip", cmd_ged_plain_wrapper, ged_exec_bot_flip, NULL},
    {MGED_CMD_MAGIC, "bot_fuse", cmd_ged_plain_wrapper, ged_exec_bot_fuse, NULL},
    {MGED_CMD_MAGIC, "bot_merge", cmd_ged_plain_wrapper, ged_exec_bot_merge, NULL},
    {MGED_CMD_MAGIC, "bot_smooth", cmd_ged_plain_wrapper, ged_exec_bot_smooth, NULL},
    {MGED_CMD_MAGIC, "bot_split", cmd_ged_plain_wrapper, ged_exec_bot_split, NULL},
    {MGED_CMD_MAGIC, "bot_sync", cmd_ged_plain_wrapper, ged_exec_bot_sync, NULL},
    {MGED_CMD_MAGIC, "bot_vertex_fuse", cmd_ged_plain_wrapper, ged_exec_bot_vertex_fuse, NULL},
    {MGED_CMD_MAGIC, "bottom", f_bv_bottom, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "brep", cmd_ged_view_wrapper, ged_exec_brep, NULL},
    {MGED_CMD_MAGIC, "c", cmd_ged_plain_wrapper, ged_exec_c, NULL},
    {MGED_CMD_MAGIC, "cat", cmd_ged_info_wrapper, ged_exec_cat, NULL},
    {MGED_CMD_MAGIC, "cc", cmd_ged_plain_wrapper, ged_exec_cc, NULL},
    {MGED_CMD_MAGIC, "center", cmd_center, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "check", cmd_ged_plain_wrapper, ged_exec_check, NULL},
    {MGED_CMD_MAGIC, "clone", cmd_ged_edit_wrapper, ged_exec_clone, NULL},
    {MGED_CMD_MAGIC, "closedb", f_closedb, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "cmd_win", cmd_cmd_win, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "coil", cmd_ged_plain_wrapper, ged_exec_coil, NULL},
    {MGED_CMD_MAGIC, "color", cmd_ged_plain_wrapper, ged_exec_color, NULL},
    {MGED_CMD_MAGIC, "comb", cmd_ged_plain_wrapper, ged_exec_comb, NULL},
    {MGED_CMD_MAGIC, "comb_color", cmd_ged_plain_wrapper, ged_exec_comb_color, NULL},
    {MGED_CMD_MAGIC, "constraint", cmd_ged_plain_wrapper, ged_exec_constraint, NULL},
    {MGED_CMD_MAGIC, "copyeval", cmd_ged_plain_wrapper, ged_exec_copyeval, NULL},
    {MGED_CMD_MAGIC, "copymat", cmd_ged_plain_wrapper, ged_exec_copymat, NULL},
    {MGED_CMD_MAGIC, "cp", cmd_ged_plain_wrapper, ged_exec_cp, NULL},
    {MGED_CMD_MAGIC, "cpi", f_copy_inv, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "d", cmd_ged_erase_wrapper, ged_exec_d, NULL},
    {MGED_CMD_MAGIC, "db", cmd_stub, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "db_glob", cmd_ged_plain_wrapper, ged_exec_db_glob, NULL},
    {MGED_CMD_MAGIC, "dbconcat", cmd_ged_plain_wrapper, ged_exec_dbconcat, NULL},
    {MGED_CMD_MAGIC, "dbfind", cmd_ged_info_wrapper, ged_exec_dbfind, NULL},
    {MGED_CMD_MAGIC, "dbip", cmd_ged_plain_wrapper, ged_exec_dbip, NULL},  // TODO - this needs to go away
    {MGED_CMD_MAGIC, "dbversion", cmd_ged_plain_wrapper, ged_exec_dbversion, NULL},
    {MGED_CMD_MAGIC, "debug", cmd_ged_plain_wrapper, ged_exec_debug, NULL},
    {MGED_CMD_MAGIC, "debugbu", cmd_ged_plain_wrapper, ged_exec_debugbu, NULL},
    {MGED_CMD_MAGIC, "debugdir", cmd_ged_plain_wrapper, ged_exec_debugdir, NULL},
    {MGED_CMD_MAGIC, "debuglib", cmd_ged_plain_wrapper, ged_exec_debuglib, NULL},
    {MGED_CMD_MAGIC, "debugnmg", cmd_ged_plain_wrapper, ged_exec_debugnmg, NULL},
    {MGED_CMD_MAGIC, "decompose", cmd_ged_plain_wrapper, ged_exec_decompose, NULL},
    {MGED_CMD_MAGIC, "delay", cmd_ged_plain_wrapper, ged_exec_delay, NULL},
    {MGED_CMD_MAGIC, "dir2ae", cmd_ged_plain_wrapper, ged_exec_dir2ae, NULL},
    {MGED_CMD_MAGIC, "dump", cmd_ged_plain_wrapper, ged_exec_dump, NULL},
    {MGED_CMD_MAGIC, "dm", f_dm, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "draw", cmd_draw, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "dsp", cmd_ged_plain_wrapper, ged_exec_dsp, NULL},
    {MGED_CMD_MAGIC, "dup", cmd_ged_plain_wrapper, ged_exec_dup, NULL},
    {MGED_CMD_MAGIC, "E", cmd_E, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "e", cmd_draw, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "eac", cmd_ged_view_wrapper, ged_exec_eac, NULL},
    {MGED_CMD_MAGIC, "echo", cmd_ged_plain_wrapper, ged_exec_echo, NULL},
    {MGED_CMD_MAGIC, "edcodes", f_edcodes, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "edit", cmd_ged_plain_wrapper, ged_exec_edit, NULL},
    {MGED_CMD_MAGIC, "color", cmd_ged_plain_wrapper, ged_exec_color, NULL},
    {MGED_CMD_MAGIC, "edcolor", f_edcolor, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "edcomb", cmd_ged_plain_wrapper, ged_exec_edcomb, NULL},
    {MGED_CMD_MAGIC, "edgedir", f_edgedir, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "edmater", f_edmater, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "env", cmd_ged_plain_wrapper, ged_exec_env, NULL},
    {MGED_CMD_MAGIC, "erase", cmd_ged_erase_wrapper, ged_exec_erase, NULL},
    {MGED_CMD_MAGIC, "ev", cmd_ev, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "eqn", f_eqn, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "exit", f_quit, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "expand", cmd_ged_plain_wrapper, ged_exec_expand, NULL},
    {MGED_CMD_MAGIC, "extrude", f_extrude, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "eye_pt", cmd_ged_view_wrapper, ged_exec_eye_pt, NULL},
    {MGED_CMD_MAGIC, "exists", cmd_ged_plain_wrapper, ged_exec_exists, NULL},
    {MGED_CMD_MAGIC, "facedef", f_facedef, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "facetize", cmd_ged_plain_wrapper, ged_exec_facetize, NULL},
    {MGED_CMD_MAGIC, "facetize_old", cmd_ged_plain_wrapper, ged_exec_facetize_old, NULL},
    {MGED_CMD_MAGIC, "form", cmd_ged_plain_wrapper, ged_exec_form, NULL},
    {MGED_CMD_MAGIC, "fracture", cmd_ged_plain_wrapper, ged_exec_fracture, NULL},
    {MGED_CMD_MAGIC, "front", f_bv_front, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "g", cmd_ged_plain_wrapper, ged_exec_g, NULL},
    {MGED_CMD_MAGIC, "gdiff", cmd_ged_plain_wrapper, ged_exec_gdiff, NULL},
    {MGED_CMD_MAGIC, "garbage_collect", cmd_ged_plain_wrapper, ged_exec_garbage_collect, NULL},
    {MGED_CMD_MAGIC, "get", cmd_ged_plain_wrapper, ged_exec_get, NULL},
    {MGED_CMD_MAGIC, "get_type", cmd_ged_plain_wrapper, ged_exec_get_type, NULL},
    {MGED_CMD_MAGIC, "get_autoview", cmd_ged_plain_wrapper, ged_exec_get_autoview, NULL},
    {MGED_CMD_MAGIC, "get_comb", cmd_ged_plain_wrapper, ged_exec_get_comb, NULL},
    {MGED_CMD_MAGIC, "get_dbip", cmd_ged_plain_wrapper, ged_exec_get_dbip, NULL}, // TODO - this needs to go away
    {MGED_CMD_MAGIC, "get_dm_list", f_get_dm_list, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "get_more_default", cmd_get_more_default, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "get_sed", f_get_sedit, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "get_sed_menus", f_get_sedit_menus, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "get_solid_keypoint", f_get_solid_keypoint, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "graph", cmd_ged_plain_wrapper, ged_exec_graph, NULL},
    {MGED_CMD_MAGIC, "gqa", cmd_ged_gqa, ged_exec_gqa, NULL},
    {MGED_CMD_MAGIC, "grid2model_lu", cmd_ged_plain_wrapper, ged_exec_grid2model_lu, NULL},
    {MGED_CMD_MAGIC, "grid2view_lu", cmd_ged_plain_wrapper, ged_exec_grid2view_lu, NULL},
    {MGED_CMD_MAGIC, "has_embedded_fb", cmd_has_embedded_fb, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "heal", cmd_ged_plain_wrapper, ged_exec_heal, NULL},
    {MGED_CMD_MAGIC, "hide", cmd_ged_plain_wrapper, ged_exec_hide, NULL},
    {MGED_CMD_MAGIC, "hist", cmd_hist, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "history", f_history, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "i", cmd_ged_plain_wrapper, ged_exec_i, NULL},
    {MGED_CMD_MAGIC, "idents", cmd_ged_plain_wrapper, ged_exec_idents, NULL},
    {MGED_CMD_MAGIC, "ill", f_ill, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "in", cmd_ged_in, ged_exec_in, NULL},
    {MGED_CMD_MAGIC, "inside", cmd_ged_inside, ged_exec_inside, NULL},
    {MGED_CMD_MAGIC, "item", cmd_ged_plain_wrapper, ged_exec_item, NULL},
    {MGED_CMD_MAGIC, "joint", cmd_ged_plain_wrapper, ged_exec_joint, NULL},
    {MGED_CMD_MAGIC, "joint2", cmd_ged_plain_wrapper, ged_exec_joint2, NULL},
    {MGED_CMD_MAGIC, "journal", f_journal, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "keep", cmd_ged_plain_wrapper, ged_exec_keep, NULL},
    {MGED_CMD_MAGIC, "keypoint", f_keypoint, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "kill", cmd_ged_erase_wrapper, ged_exec_kill, NULL},
    {MGED_CMD_MAGIC, "killall", cmd_ged_erase_wrapper, ged_exec_killall, NULL},
    {MGED_CMD_MAGIC, "killrefs", cmd_ged_erase_wrapper, ged_exec_killrefs, NULL},
    {MGED_CMD_MAGIC, "killtree", cmd_ged_erase_wrapper, ged_exec_killtree, NULL},
    {MGED_CMD_MAGIC, "knob", f_knob, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "l", cmd_ged_info_wrapper, ged_exec_l, NULL},
    {MGED_CMD_MAGIC, "labelvert", cmd_ged_view_wrapper, ged_exec_labelvert, NULL},
    {MGED_CMD_MAGIC, "labelface", cmd_ged_view_wrapper, ged_exec_labelface, NULL},
    {MGED_CMD_MAGIC, "lc", cmd_ged_plain_wrapper, ged_exec_lc, NULL},
    {MGED_CMD_MAGIC, "left", f_bv_left, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "lint", cmd_ged_plain_wrapper, ged_exec_lint, NULL},
    {MGED_CMD_MAGIC, "listeval", cmd_ged_plain_wrapper, ged_exec_listeval, NULL},
    {MGED_CMD_MAGIC, "loadtk", cmd_tk, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "loadview", cmd_ged_view_wrapper, ged_exec_loadview, NULL},
    {MGED_CMD_MAGIC, "lod", cmd_ged_plain_wrapper, ged_exec_lod, NULL},
    {MGED_CMD_MAGIC, "lookat", cmd_ged_view_wrapper, ged_exec_lookat, NULL},
    {MGED_CMD_MAGIC, "ls", cmd_ged_plain_wrapper, ged_exec_ls, NULL},
    {MGED_CMD_MAGIC, "lt", cmd_ged_plain_wrapper, ged_exec_lt, NULL},
    {MGED_CMD_MAGIC, "M", f_mouse, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "m2v_point", cmd_ged_plain_wrapper, ged_exec_m2v_point, NULL},
    {MGED_CMD_MAGIC, "make", f_make, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "make_name", cmd_ged_plain_wrapper, ged_exec_make_name, NULL},
    {MGED_CMD_MAGIC, "make_pnts", cmd_ged_more_wrapper, ged_exec_make_pnts, NULL},
    {MGED_CMD_MAGIC, "match", cmd_ged_plain_wrapper, ged_exec_match, NULL},
    {MGED_CMD_MAGIC, "mater", cmd_ged_plain_wrapper, ged_exec_mater, NULL},
    {MGED_CMD_MAGIC, "material", cmd_ged_plain_wrapper, ged_exec_material, NULL},
    {MGED_CMD_MAGIC, "matpick", f_matpick, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "mat_ae", cmd_ged_plain_wrapper, ged_exec_mat_ae, NULL},
    {MGED_CMD_MAGIC, "mat_mul", cmd_ged_plain_wrapper, ged_exec_mat_mul, NULL},
    {MGED_CMD_MAGIC, "mat4x3pnt", cmd_ged_plain_wrapper, ged_exec_mat4x3pnt, NULL},
    {MGED_CMD_MAGIC, "mat_scale_about_pnt", cmd_ged_plain_wrapper, ged_exec_mat_scale_about_pnt, NULL},
    {MGED_CMD_MAGIC, "mged_update", f_update, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "mged_wait", f_wait, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "mirface", f_mirface, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "mirror", cmd_ged_plain_wrapper, ged_exec_mirror, NULL},
    {MGED_CMD_MAGIC, "mmenu_get", cmd_mmenu_get, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "mmenu_set", cmd_nop, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "model2grid_lu", cmd_ged_plain_wrapper, ged_exec_model2grid_lu, NULL},
    {MGED_CMD_MAGIC, "model2view", cmd_ged_plain_wrapper, ged_exec_model2view, NULL},
    {MGED_CMD_MAGIC, "model2view_lu", cmd_ged_plain_wrapper, ged_exec_model2view_lu, NULL},
    {MGED_CMD_MAGIC, "mrot", cmd_mrot, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "mv", cmd_ged_plain_wrapper, ged_exec_mv, NULL},
    {MGED_CMD_MAGIC, "mvall", cmd_ged_plain_wrapper, ged_exec_mvall, NULL},
    {MGED_CMD_MAGIC, "nirt", f_nirt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "nmg_collapse", cmd_nmg_collapse, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "nmg_fix_normals", cmd_ged_plain_wrapper, ged_exec_nmg_fix_normals, NULL},
    {MGED_CMD_MAGIC, "nmg_simplify", cmd_ged_plain_wrapper, ged_exec_nmg_simplify, NULL},
    {MGED_CMD_MAGIC, "nmg", cmd_ged_plain_wrapper, ged_exec_nmg, NULL},
    {MGED_CMD_MAGIC, "npush", cmd_ged_plain_wrapper, ged_exec_npush, NULL},
    {MGED_CMD_MAGIC, "o_rotate", f_be_o_rotate, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "o_scale", f_be_o_scale, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oed", cmd_oed, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oed_apply", f_oedit_apply, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oed_reset", f_oedit_reset, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oill", f_be_o_illuminate, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "opendb", f_opendb, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "orientation", cmd_ged_view_wrapper, ged_exec_orientation, NULL},
    {MGED_CMD_MAGIC, "orot", f_rot_obj, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oscale", f_sc_obj, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "output_hook", cmd_output_hook, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "overlay", cmd_overlay, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "ox", f_be_o_x, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oxscale", f_be_o_xscale, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oxy", f_be_o_xy, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oy", f_be_o_y, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "oyscale", f_be_o_yscale, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "ozscale", f_be_o_zscale, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "p", f_param, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "pathlist", cmd_ged_plain_wrapper, ged_exec_pathlist, NULL},
    {MGED_CMD_MAGIC, "paths", cmd_ged_plain_wrapper, ged_exec_paths, NULL},
    {MGED_CMD_MAGIC, "permute", f_permute, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "plot", cmd_ged_plain_wrapper, ged_exec_plot, NULL},
    {MGED_CMD_MAGIC, "png", cmd_ged_plain_wrapper, ged_exec_png, NULL},
    {MGED_CMD_MAGIC, "pnts", cmd_ged_plain_wrapper, ged_exec_pnts, NULL},
    {MGED_CMD_MAGIC, "prcolor", cmd_ged_plain_wrapper, ged_exec_prcolor, NULL},
    {MGED_CMD_MAGIC, "prefix", cmd_ged_plain_wrapper, ged_exec_prefix, NULL},
    {MGED_CMD_MAGIC, "press", f_press, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "preview", cmd_ged_dm_wrapper, ged_exec_preview, NULL},
    {MGED_CMD_MAGIC, "process", cmd_ged_plain_wrapper, ged_exec_process, NULL},
    {MGED_CMD_MAGIC, "postscript", f_postscript, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "ps", cmd_ps, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "pull", cmd_ged_plain_wrapper, ged_exec_pull, NULL},
    {MGED_CMD_MAGIC, "push", cmd_ged_plain_wrapper, ged_exec_push, NULL},
    {MGED_CMD_MAGIC, "put", cmd_ged_plain_wrapper, ged_exec_put, NULL},
    {MGED_CMD_MAGIC, "put_comb", cmd_ged_plain_wrapper, ged_exec_put_comb, NULL},
    {MGED_CMD_MAGIC, "put_sed", f_put_sedit, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "putmat", cmd_ged_plain_wrapper, ged_exec_putmat, NULL},
    {MGED_CMD_MAGIC, "q", f_quit, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "qorot", f_qorot, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "qray", cmd_ged_plain_wrapper, ged_exec_qray, NULL},
    {MGED_CMD_MAGIC, "query_ray", f_nirt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "quit", f_quit, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "qvrot", cmd_ged_view_wrapper, ged_exec_qvrot, NULL},
    {MGED_CMD_MAGIC, "r", cmd_ged_plain_wrapper, ged_exec_r, NULL},
    {MGED_CMD_MAGIC, "rate", f_bv_rate_toggle, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rcodes", cmd_ged_plain_wrapper, ged_exec_rcodes, NULL},
    {MGED_CMD_MAGIC, "rear", f_bv_rear, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "red", f_red, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "refresh", f_refresh, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "regdebug", f_regdebug, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "regdef", cmd_ged_plain_wrapper, ged_exec_regdef, NULL},
    {MGED_CMD_MAGIC, "regions", cmd_ged_plain_wrapper, ged_exec_regions, NULL},
    {MGED_CMD_MAGIC, "reject", f_be_reject, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "release", f_release, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "reset", f_bv_reset, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "restore", f_bv_vrestore, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rfarb", f_rfarb, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "right", f_bv_right, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rm", cmd_ged_plain_wrapper, ged_exec_rm, NULL},
    {MGED_CMD_MAGIC, "rmater", cmd_ged_plain_wrapper, ged_exec_rmater, NULL},
    {MGED_CMD_MAGIC, "rmats", f_rmats, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rot", cmd_rot, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rotobj", f_rot_obj, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rrt", cmd_rrt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rset", f_rset, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rt", cmd_rt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rt_gettrees", cmd_rt_gettrees, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rtabort", cmd_ged_plain_wrapper, ged_exec_rtabort, NULL},
    {MGED_CMD_MAGIC, "rtarea", cmd_rt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rtcheck", cmd_rt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rtedge", cmd_rt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "rtweight", cmd_rt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "save", f_bv_vsave, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "savekey", cmd_ged_plain_wrapper, ged_exec_savekey, NULL},
    {MGED_CMD_MAGIC, "saveview", cmd_ged_plain_wrapper, ged_exec_saveview, NULL},
    {MGED_CMD_MAGIC, "sca", cmd_sca, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "screengrab", cmd_ged_dm_wrapper, ged_exec_screengrab, NULL},
    {MGED_CMD_MAGIC, "search", cmd_search, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "sed", f_sed, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "sed_apply", f_sedit_apply, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "sed_reset", f_sedit_reset, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "sedit", f_be_s_edit, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "select", cmd_ged_plain_wrapper, ged_exec_select, NULL},
    {MGED_CMD_MAGIC, "set_more_default", cmd_set_more_default, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "setview", cmd_setview, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "shaded_mode", cmd_shaded_mode, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "shader", cmd_ged_plain_wrapper, ged_exec_shader, NULL},
    {MGED_CMD_MAGIC, "share", f_share, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "shells", cmd_ged_plain_wrapper, ged_exec_shells, NULL},
    {MGED_CMD_MAGIC, "showmats", cmd_ged_plain_wrapper, ged_exec_showmats, NULL},
    {MGED_CMD_MAGIC, "sill", f_be_s_illuminate, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "size", cmd_size, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "simulate", cmd_ged_simulate_wrapper, ged_exec_simulate, NULL},
    {MGED_CMD_MAGIC, "solid_report", cmd_ged_plain_wrapper, ged_exec_solid_report, NULL},
    {MGED_CMD_MAGIC, "solids", cmd_ged_plain_wrapper, ged_exec_solids, NULL},
    {MGED_CMD_MAGIC, "solids_on_ray", cmd_ged_plain_wrapper, ged_exec_solids_on_ray, NULL},
    {MGED_CMD_MAGIC, "srot", f_be_s_rotate, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "sscale", f_be_s_scale, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "stat", cmd_ged_plain_wrapper, ged_exec_stat, NULL},
    {MGED_CMD_MAGIC, "status", f_status, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "stuff_str", cmd_stuff_str, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "summary", cmd_ged_plain_wrapper, ged_exec_summary, NULL},
    {MGED_CMD_MAGIC, "sv", f_slewview, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "svb", f_svbase, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "sxy", f_be_s_trans, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "sync", cmd_ged_plain_wrapper, ged_exec_sync, NULL},
    {MGED_CMD_MAGIC, "t", cmd_ged_plain_wrapper, ged_exec_t, NULL},
    {MGED_CMD_MAGIC, "ted", f_tedit, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "tie", f_tie, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "tire", cmd_ged_plain_wrapper, ged_exec_tire, NULL},
    {MGED_CMD_MAGIC, "title", cmd_ged_plain_wrapper, ged_exec_title, NULL},
    {MGED_CMD_MAGIC, "tol", cmd_tol, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "top", f_bv_top, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "tops", cmd_ged_plain_wrapper, ged_exec_tops, NULL},
    {MGED_CMD_MAGIC, "tra", cmd_tra, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "track", f_amtrack, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "tracker", f_tracker, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "translate", f_tr_obj, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "tree", cmd_ged_plain_wrapper, ged_exec_tree, NULL},
    {MGED_CMD_MAGIC, "unhide", cmd_ged_plain_wrapper, ged_exec_unhide, NULL},
    {MGED_CMD_MAGIC, "units", cmd_units, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "v2m_point", cmd_ged_plain_wrapper, ged_exec_v2m_point, NULL},
    {MGED_CMD_MAGIC, "vars", f_set, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "vdraw", cmd_ged_plain_wrapper, ged_exec_vdraw, NULL},
    {MGED_CMD_MAGIC, "view", cmd_ged_view_wrapper, ged_exec_view, NULL},
    {MGED_CMD_MAGIC, "view_ring", f_view_ring, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "view2grid_lu", cmd_ged_plain_wrapper, ged_exec_view2grid_lu, NULL},
    {MGED_CMD_MAGIC, "view2model", cmd_ged_plain_wrapper, ged_exec_view2model, NULL},
    {MGED_CMD_MAGIC, "view2model_lu", cmd_ged_plain_wrapper, ged_exec_view2model_lu, NULL},
    {MGED_CMD_MAGIC, "view2model_vec", cmd_ged_plain_wrapper, ged_exec_view2model_vec, NULL},
    {MGED_CMD_MAGIC, "viewdir", cmd_ged_plain_wrapper, ged_exec_viewdir, NULL},
    {MGED_CMD_MAGIC, "viewsize", cmd_size, GED_FUNC_PTR_NULL, NULL}, /* alias "size" for saveview scripts */
    {MGED_CMD_MAGIC, "vnirt", f_vnirt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "voxelize", cmd_ged_plain_wrapper, ged_exec_voxelize, NULL},
    {MGED_CMD_MAGIC, "vquery_ray", f_vnirt, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "vrot", cmd_vrot, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "wcodes", cmd_ged_plain_wrapper, ged_exec_wcodes, NULL},
    {MGED_CMD_MAGIC, "whatid", cmd_ged_plain_wrapper, ged_exec_whatid, NULL},
    {MGED_CMD_MAGIC, "which_shader", cmd_ged_plain_wrapper, ged_exec_which_shader, NULL},
    {MGED_CMD_MAGIC, "whichair", cmd_ged_plain_wrapper, ged_exec_whichair, NULL},
    {MGED_CMD_MAGIC, "whichid", cmd_ged_plain_wrapper, ged_exec_whichid, NULL},
    {MGED_CMD_MAGIC, "who", cmd_ged_plain_wrapper, ged_exec_who, NULL},
    {MGED_CMD_MAGIC, "winset", f_winset, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "wmater", cmd_ged_plain_wrapper, ged_exec_wmater, NULL},
    {MGED_CMD_MAGIC, "x", cmd_ged_plain_wrapper, ged_exec_x, NULL},
    {MGED_CMD_MAGIC, "xpush", cmd_ged_plain_wrapper, ged_exec_xpush, NULL},
    {MGED_CMD_MAGIC, "Z", cmd_zap, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "zoom", cmd_zoom, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "zoomin", f_bv_zoomin, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, "zoomout", f_bv_zoomout, GED_FUNC_PTR_NULL, NULL},
    {MGED_CMD_MAGIC, NULL, NULL, GED_FUNC_PTR_NULL, NULL}
};


/**
 * Register all MGED commands.
 */
static void
cmd_setup(struct mged_state *s)
{
    struct cmdtab *ctp;
    struct bu_vls temp = BU_VLS_INIT_ZERO;

    // TODO - should be using libged cmd list to populate everything in this
    // table that is a plain wrapper - that way all ged commands are always
    // automatically there and we can delete the hardcoded logic.  Drawback
    // would be we would lose the compile time check that libged has what MGED
    // is expecting... a compromise might be to have mged validate such a list
    // at startup and exit if any expected commands aren't valid.
    //
    // In the case of a conflicting cmd name between the non-plain wrappers and
    // the above table contents the MGED tbl should win, but we should also
    // look into the reasons for the wrappers NOT being plain and try to find a
    // way (callbacks, etc.) to avoid the need for non-plain wrappers.

    for (ctp = mged_cmdtab; ctp->name != NULL; ctp++) {

	// Uncomment the following to have MGED report which entries in its
	// command table collide with libged command names
#if 0
	if (ctp->ged_func == GED_FUNC_PTR_NULL && ged_cmd_exists(ctp->name)
	    bu_log("%12s: matches an existing GED command name, but uses an MGED wrapper function.\n", ctp->name);
#endif

        ctp->s = s;
	bu_vls_strcpy(&temp, "_mged_");
	bu_vls_strcat(&temp, ctp->name);

	(void)Tcl_CreateCommand(s->interp, ctp->name, ctp->tcl_func,
				(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(s->interp, bu_vls_addr(&temp), ctp->tcl_func,
				(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }

    /* Init mged's Tcl interface to libwdb */
    Wdb_Init(s->interp);

    tkwin = NULL;

    bu_vls_free(&temp);
}


static void
mged_output_handler(struct ged *UNUSED(gp), char *line)
{
    if (line)
	bu_log("%s", line);
}

static void
mged_refresh_handler(void *clientdata)
{
    struct mged_state *s = (struct mged_state *)clientdata;
    MGED_CK_STATE(s);

    view_state->vs_flag = 1;
    refresh(s);
}


/*
 * Initialize mged, configure the path, set up the tcl interpreter.
 */
void
mged_setup(struct mged_state *s)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    const char *name = bu_dir(NULL, 0, BU_DIR_BIN, bu_getprogname(), BU_DIR_EXT, NULL);

    /* locate our run-time binary (must be called before Tcl_CreateInterp()) */
    if (name) {
	Tcl_FindExecutable(name);
    } else {
	Tcl_FindExecutable("mged");
    }

    if (s->interp != NULL)
	Tcl_DeleteInterp(s->interp);

    /* Create the interpreter */
    s->interp = Tcl_CreateInterp();
    s->interp = s->interp;

    /* Do basic Tcl initialization - note that Tk
     * is not initialized at this point. */
    if (tclcad_init(s->interp, 0, &tlog) == TCL_ERROR) {
	bu_log("tclcad_init error:\n%s\n", bu_vls_addr(&tlog));
    }
    bu_vls_free(&tlog);

    mged_global_db_ctx.s = s;

    s->gedp = ged_create();
    s->gedp->ged_output_handler = mged_output_handler;
    s->gedp->ged_refresh_clientdata = (void *)s;
    s->gedp->ged_refresh_handler = mged_refresh_handler;
    s->gedp->vlist_ctx = (void *)s;
    s->gedp->ged_create_vlist_scene_obj_callback = createDListSolid;
    s->gedp->ged_create_vlist_display_list_callback = createDListAll;
    s->gedp->ged_destroy_vlist_callback = freeDListsAll;
    s->gedp->ged_create_io_handler = &tclcad_create_io_handler;
    s->gedp->ged_delete_io_handler = &tclcad_delete_io_handler;

    ged_clbk_set(s->gedp, "opendb", BU_CLBK_PRE, &mged_pre_opendb_clbk, (void *)&mged_global_db_ctx);
    ged_clbk_set(s->gedp, "opendb", BU_CLBK_POST, &mged_post_opendb_clbk, (void *)&mged_global_db_ctx);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_PRE, &mged_pre_closedb_clbk, (void *)&mged_global_db_ctx);
    ged_clbk_set(s->gedp, "closedb", BU_CLBK_POST, &mged_post_closedb_clbk, (void *)&mged_global_db_ctx);

    // Register during-execution callback function for search command
    ged_clbk_set(s->gedp, "search", BU_CLBK_DURING, &mged_db_search_callback, (void *)s);

    struct tclcad_io_data *t_iod = tclcad_create_io_data();
    t_iod->io_mode = TCL_READABLE;
    t_iod->interp = s->interp;
    s->gedp->ged_io_data = t_iod;

    /* Set up the default state of the standard open/close db container */
    mged_global_db_ctx.argc = 0;
    mged_global_db_ctx.argv = NULL;
    mged_global_db_ctx.force_create = 0;
    mged_global_db_ctx.no_create = 0;
    mged_global_db_ctx.created_new_db = 0;
    mged_global_db_ctx.ret = 0;
    mged_global_db_ctx.ged_ret = 0;
    mged_global_db_ctx.interpreter = s->interp;
    mged_global_db_ctx.old_dbip = NULL;
    mged_global_db_ctx.post_open_cnt = 0;

    BU_ALLOC(view_state->vs_gvp, struct bview);
    bv_init(view_state->vs_gvp, NULL);
    BU_GET(view_state->vs_gvp->callbacks, struct bu_ptbl);
    bu_ptbl_init(view_state->vs_gvp->callbacks, 8, "bv callbacks");

    view_state->vs_gvp->gv_callback = mged_view_callback;
    view_state->vs_gvp->gv_clientData = (void *)view_state;
    MAT_DELTAS_GET_NEG(view_state->vs_gvp->orig_pos, view_state->vs_gvp->gv_center);

    view_state->vs_gvp->vset = &s->gedp->ged_views;

    bv_set_add_view(&s->gedp->ged_views, view_state->vs_gvp);
    bu_ptbl_ins(&s->gedp->ged_free_views, (long *)view_state->vs_gvp);
    s->gedp->ged_gvp = view_state->vs_gvp;

    /* register commands */
    cmd_setup(s);

    history_setup();
    mged_global_variable_setup(s);
    mged_variable_setup(s);

    /* Tcl needs to write nulls onto subscripted variable names */
    bu_vls_printf(&str, "%s(state)", MGED_DISPLAY_VAR);
    Tcl_SetVar(s->interp, bu_vls_addr(&str), state_str[s->edit_state.global_editing_state], TCL_GLOBAL_ONLY);

    /* Set defaults for view status variables */
    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "set %s(.topid_0.ur,ang) {ang=(0.00 0.00 0.00)};", MGED_DISPLAY_VAR);
    bu_vls_printf(&str, "set %s(.topid_0.ur,aet) {az=35.00  el=25.00  tw=0.00};", MGED_DISPLAY_VAR);
    bu_vls_printf(&str, "set %s(.topid_0.ur,size) sz=1000.000;", MGED_DISPLAY_VAR);
    bu_vls_printf(&str, "set %s(.topid_0.ur,center) {cent=(0.000 0.000 0.000)};", MGED_DISPLAY_VAR);
    bu_vls_printf(&str, "set %s(units) mm", MGED_DISPLAY_VAR);
    Tcl_Eval(s->interp, bu_vls_addr(&str));

    Tcl_ResetResult(s->interp);

    bu_vls_free(&str);
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
