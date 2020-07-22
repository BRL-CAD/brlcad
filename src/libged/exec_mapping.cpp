/*                    E X E C _ M A P P I N G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file exec_wrapping.cpp
 *
 * Provide compile time wrappers that pass specific libged function
 * calls through to the plugin system.
 *
 */

#include "common.h"

#include "ged.h"

int ged_3ptarb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_E(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_adc(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_adjust(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_ae2dir(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_aet(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_analyze(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_annotate(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_arb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_arced(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_arot(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_attr(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_autoview(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bev(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_blast(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bo(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_condense(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_decimate(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_dump(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_face_fuse(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_face_sort(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_flip(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_fuse(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_merge(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_smooth(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_split(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_sync(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_bot_vertex_fuse(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_brep(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_cat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_cc(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_center(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_check(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_clone(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_coil(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_color(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_comb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_comb_color(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_comb_std(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_combmem(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_concat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_constraint(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_copy(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_copyeval(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_copymat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_cpi(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_dbip(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_dbot_dump(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_debug(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_debugbu(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_debugdir(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_debuglib(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_debugnmg(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_decompose(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_delay(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_dir2ae(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_draw(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_dsp(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_dump(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_dup(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_eac(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_echo(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_edarb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_edcodes(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_edcolor(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_edcomb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_edit(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_editit(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_edmater(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_env(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_erase(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_ev(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_exists(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_expand(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_eye(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_eye_pos(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_facetize(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_fb2pix(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_fbclear(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_find(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_find_arb_edge_nearest_pnt(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_form(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_fracture(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_gdiff(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_get(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_get_autoview(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_get_comb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_get_eyemodel(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_get_type(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_glob(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_gqa(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_graph(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_grid(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_grid2model_lu(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_grid2view_lu(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_group(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_heal(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_help(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_hide(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_how(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_human(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_illum(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_importFg4Section(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_in(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_inside(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_instance(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_isize(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_item(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_joint(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_joint2(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_keep(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_keypoint(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_kill(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_killall(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_killrefs(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_killtree(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_lc(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_lint(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_list(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_loadview(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_lod(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_log(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_lookat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_ls(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_lt(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_m2v_point(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_make(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_make_name(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_make_pnts(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_match(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_mater(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_mirror(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_model2grid_lu(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_model2view(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_model2view_lu(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_move(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_move_all(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_move_arb_edge(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_move_arb_face(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_mrot(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_nirt(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_nmg(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_nmg_collapse(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_nmg_fix_normals(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_nmg_simplify(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_ocenter(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_orient(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_orotate(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_oscale(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_otranslate(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_overlay(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pathlist(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pathsum(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_perspective(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pix2fb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_plot(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pmat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pmodel2view(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_png(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_png2fb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pnts(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_prcolor(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_prefix(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_preview(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_process(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_protate(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_ps(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pscale(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pset(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_ptranslate(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_pull(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_push(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_put(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_put_comb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_putmat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_qray(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_quat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_qvrot(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rcodes(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rect(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_red(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_redraw(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_regdef(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_region(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_remove(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_reopen(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rfarb(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rmap(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rmat(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rmater(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rot(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rot_point(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rrt(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rselect(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rt(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rtabort(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rtcheck(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_rtwizard(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_savekey(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_saveview(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_scale(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_screen_grab(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_search(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_select(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_set_output_script(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_set_transparency(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_set_uplotOutputMode(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_setview(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_shaded_mode(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_shader(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_shells(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_showmats(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_simulate(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_size(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_slew(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_solid_report(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_solids_on_ray(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_sphgroup(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_summary(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_sync(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_tables(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_tire(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_title(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_tol(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_tops(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_tra(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_track(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_tracker(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_tree(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_unhide(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_units(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_v2m_point(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_vdraw(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_version(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_view2grid_lu(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_view2model(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_view2model_lu(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_view2model_vec(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_view_func(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_viewdir(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_vnirt(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_voxelize(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_vrot(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_wcodes(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_whatid(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_which(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_which_shader(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_who(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_wmater(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_xpush(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_ypr(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_zap(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}
int ged_zoom(struct ged *gedp, int argc, const char *argv[])
{
    return ged_exec(gedp, argc, argv);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

