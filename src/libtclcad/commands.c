/*                     C O M M A N D S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2021 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file libtclcad/commands.c
 *
 * A quasi-object-oriented database interface.
 *
 * A GED object contains the attributes and methods for controlling a
 * BRL-CAD geometry edit object.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "png.h"

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bio.h"

#include "bn.h"
#include "bu/cmd.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/units.h"
#include "vmath.h"
#include "rt/db4.h"
#include "rt/geom.h"
#include "wdb.h"
#include "raytrace.h"
#include "ged.h"
#include "tclcad.h"

#include "rt/solid.h"
#include "dm.h"
#include "dm/bview_util.h"

#include "icv/io.h"
#include "icv/ops.h"
#include "icv/crop.h"
#include "dm.h"

#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

/* For the moment call internal libged functions - a cleaner
 * solution will be needed eventually */
#include "../libged/ged_private.h"

/* Private headers */
#include "brlcad_version.h"
#include "./tclcad_private.h"
#include "./view/view.h"

HIDDEN int to_base2local(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bg(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bounds(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_configure(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_constrain_rmode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_constrain_tmode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_copy(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_data_move(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_data_move_func(struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage);
HIDDEN int to_data_move_object_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_data_move_object_mode_func(struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage);
HIDDEN int to_data_move_point_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_data_move_point_mode_func(struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage);
HIDDEN int to_data_pick(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int
to_data_pick_func(struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage);
HIDDEN int to_data_vZ(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_dplot(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_dlist_on(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bot_edge_split(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bot_face_split(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_fontsize(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_fit_png_image(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_init_view_bindings(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_delete_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_handle_expose(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_hide_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_idle_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_light(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_list_views(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_local2base(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_lod(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_make(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_mirror(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_edit_motion_delta_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_more_args_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_move_arb_edge_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_move_arb_face_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bot_move_pnt(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bot_move_pnts(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bot_move_pnt_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_bot_move_pnts_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_metaball_move_pnt_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_pipe_move_pnt_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_move_pnt_common(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_new_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_orotate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_oscale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_otranslate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_paint_rect_area(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
#ifdef HAVE_GL_GL_H
HIDDEN int to_pix(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_png(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
#endif
HIDDEN int to_rect_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_rotate_arb_face_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_rotate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_rt_end_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_rt_gettrees(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_protate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_pscale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_ptranslate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_data_scale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_scale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_screen2model(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_screen2view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_set_coord(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_snap_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_translate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_transparency(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_view_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_view_win_size(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_view2screen(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_vmake(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_vslew(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_zbuffer(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);
HIDDEN int to_zclip(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs);

/* Utility Functions */

HIDDEN void to_create_vlist_callback_solid(struct solid *gdlp);
HIDDEN void to_create_vlist_callback(struct display_list *gdlp);
HIDDEN void to_destroy_vlist_callback(unsigned int dlist, int range);
HIDDEN void to_rt_end_callback_internal(int aborted);

HIDDEN void to_output_handler(struct ged *gedp, char *line);

typedef int (*to_wrapper_func_ptr)(struct ged *, int, const char *[], ged_func_ptr, const char *, int);
#define TO_WRAPPER_FUNC_PTR_NULL (to_wrapper_func_ptr)0


struct to_cmdtab {
    const char *to_name;
    const char *to_usage;
    int to_maxargs;
    to_wrapper_func_ptr to_wrapper_func;
    ged_func_ptr to_func;
};


static struct to_cmdtab ged_cmds[] = {
    {"3ptarb",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_3ptarb},
    {"adc",	"args", 7, to_view_func, ged_adc},
    {"adjust",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_adjust},
    {"ae2dir",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ae2dir},
    {"aet",	"[[-i] az el [tw]]", 6, to_view_func_plus, ged_aet},
    {"analyze",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_analyze},
    {"annotate", (char *)0, TO_UNLIMITED, to_pass_through_func, ged_annotate},
    {"pipe_append_pnt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pipe_append_pnt},
    {"arb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_arb},
    {"arced",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_arced},
    {"arot",	"x y z angle", 6, to_view_func_plus, ged_arot},
    {"attr",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_attr},
    {"bb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bb},
    {"bev",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bev},
    {"bo",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bo},
    {"bot",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot},
    {"bot_condense",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_condense},
    {"bot_decimate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_decimate},
    {"bot_dump",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_dump},
    {"bot_face_fuse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_face_fuse},
    {"bot_face_sort",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_face_sort},
    {"bot_flip",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_flip},
    {"bot_fuse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_fuse},
    {"bot_merge",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_merge},
    {"bot_smooth",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_smooth},
    {"bot_split",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_split},
    {"bot_sync",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_sync},
    {"bot_vertex_fuse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_vertex_fuse},
    {"brep",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_brep},
    {"c",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_comb_std},
    {"cat",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_cat},
    {"center",	"[x y z]", 5, to_view_func_plus, ged_center},
    {"check",	(char *)0, TO_UNLIMITED, to_view_func, ged_check},
    {"clear",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_zap},
    {"clone",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_clone},
    {"coil",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_coil},
    {"color",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_color},
    {"comb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_comb},
    {"comb_color",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_comb_color},
    {"combmem",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_combmem},
    {"constraint", (char *)0, TO_UNLIMITED, to_pass_through_func, ged_constraint},
    {"copyeval",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_copyeval},
    {"copymat",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_copymat},
    {"cpi",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_cpi},
    {"d",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_erase},
    {"dbconcat",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_concat},
    {"dbfind",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_find},
    {"dbip",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dbip},
    {"dbot_dump",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dbot_dump},
    {"debug", 	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debug},
    {"debugbu", 	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debugbu},
    {"debugdir",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debugdir},
    {"debuglib",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debuglib},
    {"debugnmg",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debugnmg},
    {"decompose",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_decompose},
    {"delay",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_delay},
    {"dplot",	"dplot_logfile", 1, to_dplot, ged_dplot},
    {"metaball_delete_pnt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_metaball_delete_pnt},
    {"pipe_delete_pnt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pipe_delete_pnt},
    {"dir2ae",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dir2ae},
    {"draw",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_draw},
    {"dump",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dump},
    {"dup",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dup},
    {"E",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_E},
    {"e",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_draw},
    {"eac",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_eac},
    {"echo",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_echo},
    {"edarb",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_edarb},
    {"edcodes",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_edcodes},
    {"edcolor",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_edcolor},
    {"edcomb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_edcomb},
    {"edit",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_edit},
    {"edmater",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_edmater},
    {"env",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_env},
    {"erase",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_erase},
    {"ev",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_ev},
    {"expand",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_expand},
    {"eye",	"[x y z]", 5, to_view_func_plus, ged_eye},
    {"eye_pos",	"[x y z]", 5, to_view_func_plus, ged_eye_pos},
    {"eye_pt",	"[x y z]", 5, to_view_func_plus, ged_eye},
    {"exists",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_exists},
    {"facetize",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_facetize},
    {"voxelize",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_voxelize},
    {"fb2pix",  	"[-h -i -c] [-s squaresize] [-w width] [-n height] [file.pix]", TO_UNLIMITED, to_view_func, ged_fb2pix},
    {"fbclear",  	"[r g b]", TO_UNLIMITED, to_view_func, ged_fbclear},
    {"find_arb_edge",	"arb vx vy ptol", 5, to_view_func, ged_find_arb_edge_nearest_pnt},
    {"find_bot_edge",	"bot vx vy", 5, to_view_func, ged_find_bot_edge_nearest_pnt},
    {"find_bot_pnt",	"bot vx vy", 5, to_view_func, ged_find_bot_pnt_nearest_pnt},
    {"find_pipe_pnt",	"pipe x y z", 6, to_view_func, ged_find_pipe_pnt_nearest_pnt},
    {"form",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_form},
    {"fracture",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_fracture},
    {"g",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_group},
    {"gdiff",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_gdiff},
    {"get",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get},
    {"get_autoview",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_autoview},
    {"get_bot_edges",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_bot_edges},
    {"get_comb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_comb},
    {"get_eyemodel",	"vname", 2, to_view_func, ged_get_eyemodel},
    {"get_type",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_type},
    {"glob",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_glob},
    {"gqa",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_gqa},
    {"graph",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_graph},
    {"grid",	"args", 6, to_view_func, ged_grid},
    {"grid2model_lu",	"x y", 4, to_view_func_less, ged_grid2model_lu},
    {"grid2view_lu",	"x y", 4, to_view_func_less, ged_grid2view_lu},
    {"heal",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_heal},
    {"hide",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_hide},
    {"how",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_how},
    {"human",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_human},
    {"i",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_instance},
    {"idents",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tables},
    {"illum",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_illum},
    {"importFg4Section",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_importFg4Section},
    {"in",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_in},
    {"inside",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_inside},
    {"isize",	"vname", 2, to_view_func, ged_isize},
    {"item",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_item},
    {"joint",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_joint},
    {"joint2",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_joint2},
    {"keep",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_keep},
    {"keypoint",	"[x y z]", 5, to_view_func, ged_keypoint},
    {"kill",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_kill},
    {"killall",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_killall},
    {"killrefs",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_killrefs},
    {"killtree",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_killtree},
    {"l",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_list},
    {"lc",      "[-d|-s|-r] [-z] [-0|-1|-2|-3|-4|-5] [-f {FileName}] {GroupName}", TO_UNLIMITED, to_pass_through_func, ged_lc},
    {"listeval",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pathsum},
    {"loadview",	"filename", 3, to_view_func, ged_loadview},
    {"lod",	(char *)0, TO_UNLIMITED, to_lod, ged_lod},
    {"log",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_log},
    {"lookat",	"x y z", 5, to_view_func_plus, ged_lookat},
    {"ls",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ls},
    {"lt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_lt},
    {"m2v_point",	"x y z", 5, to_view_func, ged_m2v_point},
    {"make_name",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_make_name},
    {"make_pnts",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_make_pnts},
    {"mat4x3pnt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_mat4x3pnt},
    {"match",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_match},
    {"mater",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_mater},
    {"model2grid_lu",	"x y z", 5, to_view_func_less, ged_model2grid_lu},
    {"model2view",	"vname", 2, to_view_func, ged_model2view},
    {"model2view_lu",	"x y z", 5, to_view_func_less, ged_model2view_lu},
    {"move_arb_edge",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move_arb_edge},
    {"move_arb_face",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move_arb_face},
    {"metaball_move_pnt",	(char *)0, TO_UNLIMITED, to_move_pnt_common, ged_metaball_move_pnt},
    {"pipe_move_pnt",	(char *)0, TO_UNLIMITED, to_move_pnt_common, ged_pipe_move_pnt},
    {"mouse_add_metaball_pnt",	"obj mx my", TO_UNLIMITED, to_mouse_append_pnt_common, ged_metaball_add_pnt},
    {"mouse_append_pipe_pnt",	"obj mx my", TO_UNLIMITED, to_mouse_append_pnt_common, ged_pipe_append_pnt},
    {"mouse_move_metaball_pnt",	"obj i mx my", TO_UNLIMITED, to_mouse_move_pnt_common, ged_metaball_move_pnt},
    {"mouse_move_pipe_pnt",	"obj i mx my", TO_UNLIMITED, to_mouse_move_pnt_common, ged_pipe_move_pnt},
    {"mouse_prepend_pipe_pnt",	"obj mx my", TO_UNLIMITED, to_mouse_append_pnt_common, ged_pipe_prepend_pnt},
    {"mv",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move},
    {"mvall",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move_all},
    {"nirt",	"[args]", TO_UNLIMITED, to_view_func, ged_nirt},
    {"nmg_collapse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_nmg_collapse},
    {"nmg_fix_normals",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_nmg_fix_normals},
    {"nmg_simplify",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_nmg_simplify},
    {"npush",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_npush},
    {"ocenter",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ocenter},
    {"open",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_reopen},
    {"orient",	"quat", 6, to_view_func_plus, ged_orient},
    {"orientation",	"quat", 6, to_view_func_plus, ged_orient},
    {"orotate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_orotate},
    {"oscale",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_oscale},
    {"otranslate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_otranslate},
    {"overlay",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_overlay},
    {"pathlist",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pathlist},
    {"paths",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pathsum},
    {"perspective",	"[angle]", 3, to_view_func_plus, ged_perspective},
    {"pix2fb",  	"[options] [file.pix]", TO_UNLIMITED, to_view_func, ged_pix2fb},
    {"plot",	"[options] file.pl", 16, to_view_func, ged_plot},
    {"pmat",	"[mat]", 3, to_view_func, ged_pmat},
    {"pmodel2view",	"vname", 2, to_view_func, ged_pmodel2view},
    {"png2fb",  "[options] [file.png]", TO_UNLIMITED, to_view_func, ged_png2fb},
    {"pngwf",	"[options] file.png", 16, to_view_func, ged_png},
    {"prcolor",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_prcolor},
    {"prefix",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_prefix},
    {"pipe_prepend_pnt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pipe_prepend_pnt},
    {"preview",	"[options] script", TO_UNLIMITED, to_dm_func, ged_preview},
    {"protate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_protate},
    {"postscript", "[options] file.ps", 16, to_view_func, ged_ps},
    {"pscale",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pscale},
    {"pset",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pset},
    {"ptranslate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ptranslate},
    {"push",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_push},
    {"put",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_put},
    {"put_comb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_put_comb},
    {"putmat",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_putmat},
    {"qray",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_qray},
    {"quat",	"a b c d", 6, to_view_func_plus, ged_quat},
    {"qvrot",	"x y z angle", 6, to_view_func_plus, ged_qvrot},
    {"r",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_region},
    {"rcodes",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rcodes},
    {"rect",	"args", 6, to_view_func, ged_rect},
    {"red",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_red},
    {"regdef",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_regdef},
    {"regions",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tables},
    {"solid_report",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_solid_report},
    {"rfarb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rfarb},
    {"rm",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_remove},
    {"rmap",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rmap},
    {"rmat",	"[mat]", 3, to_view_func, ged_rmat},
    {"rmater",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rmater},
    {"rot",	"[-m|-v] x y z", 6, to_view_func_plus, ged_rot},
    {"rot_about",	"[e|k|m|v]", 3, to_view_func, ged_rotate_about},
    {"rot_point",	"x y z", 5, to_view_func, ged_rot_point},
    {"rotate_arb_face",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rotate_arb_face},
    {"rrt",	"[args]", TO_UNLIMITED, to_view_func, ged_rrt},
    {"rselect",		(char *)0, TO_UNLIMITED, to_view_func, ged_rselect},
    {"rt",	"[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rtabort",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rtabort},
    {"rtarea",	"[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rtcheck",	"[args]", TO_UNLIMITED, to_view_func, ged_rtcheck},
    {"rtedge",	"[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rtweight", "[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rtwizard", "[args]", TO_UNLIMITED, to_view_func, ged_rtwizard},
    {"savekey",	"filename", 3, to_view_func, ged_savekey},
    {"saveview", (char *)0, TO_UNLIMITED, to_view_func, ged_saveview},
    {"sca",	"sf", 3, to_view_func_plus, ged_scale},
    {"screengrab",	"imagename.ext", TO_UNLIMITED, to_dm_func, ged_screen_grab},
    {"search",		(char *)0, TO_UNLIMITED, to_pass_through_func, ged_search},
    {"select",		(char *)0, TO_UNLIMITED, to_view_func, ged_select},
    {"set_output_script",	"[script]", TO_UNLIMITED, to_pass_through_func, ged_set_output_script},
    {"set_transparency",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_set_transparency},
    {"set_uplotOutputMode",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_set_uplotOutputMode},
    {"setview",	"x y z", 5, to_view_func_plus, ged_setview},
    {"shaded_mode",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_shaded_mode},
    {"shader",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_shader},
    {"shells",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_shells},
    {"showmats",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_showmats},
    {"size",	"[size]", 3, to_view_func_plus, ged_size},
    {"slew",	"x y [z]", 5, to_view_func_plus, ged_slew},
    {"solids",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tables},
    {"solids_on_ray",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_solids_on_ray},
    {"summary",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_summary},
    {"sv",	"x y [z]", 5, to_view_func_plus, ged_slew},
    {"sync",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_sync},
    {"t",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ls},
    {"tire",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tire},
    {"title",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_title},
    {"tol",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tol},
    {"tops",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tops},
    {"tra",	"[-m|-v] x y z", 6, to_view_func_plus, ged_tra},
    {"track",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_track},
    {"tree",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tree},
    {"unhide",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_unhide},
    {"units",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_units},
    {"v2m_point",	"x y z", 5, to_view_func, ged_v2m_point},
    {"vdraw",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_vdraw},
    {"version",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_version},
    {"view",	"quat|ypr|aet|center|eye|size [args]", 7, to_view_func_plus, ged_view_func},
    {"view2grid_lu",	"x y z", 5, to_view_func_less, ged_view2grid_lu},
    {"view2model",	"", 2, to_view_func_less, ged_view2model},
    {"view2model_lu",	"x y z", 5, to_view_func_less, ged_view2model_lu},
    {"view2model_vec",	"x y z", 5, to_view_func_less, ged_view2model_vec},
    {"viewdir",	"[-i]", 3, to_view_func_less, ged_viewdir},
    {"vnirt",	"[args]", TO_UNLIMITED, to_view_func, ged_vnirt},
    {"wcodes",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_wcodes},
    {"whatid",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_whatid},
    {"which_shader",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_which_shader},
    {"whichair",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_which},
    {"whichid",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_which},
    {"who",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_who},
    {"wmater",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_wmater},
    {"x",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_solid_report},
    {"xpush",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_xpush},
    {"ypr",	"yaw pitch roll", 5, to_view_func_plus, ged_ypr},
    {"zap",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_zap},
    {"zoom",	"sf", 3, to_view_func_plus, ged_zoom},
    {(char *)0,	(char *)0, 0, TO_WRAPPER_FUNC_PTR_NULL, GED_FUNC_PTR_NULL}
};


static struct to_cmdtab to_cmds[] = {
    {"autoview",	"vname", TO_UNLIMITED, to_autoview, GED_FUNC_PTR_NULL},
    {"base2local",	(char *)0, TO_UNLIMITED, to_base2local, GED_FUNC_PTR_NULL},
    {"bg",	"[r g b]", TO_UNLIMITED, to_bg, GED_FUNC_PTR_NULL},
    {"blast",	(char *)0, TO_UNLIMITED, to_blast, GED_FUNC_PTR_NULL},
    {"bot_edge_split",	"bot face", TO_UNLIMITED, to_bot_edge_split, GED_FUNC_PTR_NULL},
    {"bot_face_split",	"bot face", TO_UNLIMITED, to_bot_face_split, GED_FUNC_PTR_NULL},
    {"bounds",	"[\"minX maxX minY maxY minZ maxZ\"]", TO_UNLIMITED, to_bounds, GED_FUNC_PTR_NULL},
    {"configure",	"vname", TO_UNLIMITED, to_configure, GED_FUNC_PTR_NULL},
    {"constrain_rmode",	"x|y|z x y", TO_UNLIMITED, to_constrain_rmode, GED_FUNC_PTR_NULL},
    {"constrain_tmode",	"x|y|z x y", TO_UNLIMITED, to_constrain_tmode, GED_FUNC_PTR_NULL},
    {"cp",	"[-f] from to", TO_UNLIMITED, to_copy, GED_FUNC_PTR_NULL},
    {"data_arrows",	"???", TO_UNLIMITED, to_data_arrows, GED_FUNC_PTR_NULL},
    {"data_axes",	"???", TO_UNLIMITED, to_data_axes, GED_FUNC_PTR_NULL},
    {"data_labels",	"???", TO_UNLIMITED, to_data_labels, GED_FUNC_PTR_NULL},
    {"data_lines",	"???", TO_UNLIMITED, to_data_lines, GED_FUNC_PTR_NULL},
    {"data_polygons",	"???", TO_UNLIMITED, to_data_polygons, GED_FUNC_PTR_NULL},
    {"data_move",	"???", TO_UNLIMITED, to_data_move, GED_FUNC_PTR_NULL},
    {"data_move_object_mode",	"x y", TO_UNLIMITED, to_data_move_object_mode, GED_FUNC_PTR_NULL},
    {"data_move_point_mode",	"x y", TO_UNLIMITED, to_data_move_point_mode, GED_FUNC_PTR_NULL},
    {"data_pick",	"???", TO_UNLIMITED, to_data_pick, GED_FUNC_PTR_NULL},
    {"data_scale_mode",	"x y", TO_UNLIMITED, to_data_scale_mode, GED_FUNC_PTR_NULL},
    {"data_vZ",	"[z]", TO_UNLIMITED, to_data_vZ, GED_FUNC_PTR_NULL},
    {"delete_view",	"vname", TO_UNLIMITED, to_delete_view, GED_FUNC_PTR_NULL},
    {"dlist_on",	"[0|1]", TO_UNLIMITED, to_dlist_on, GED_FUNC_PTR_NULL},
    {"faceplate",	"center_dot|prim_labels|view_params|view_scale color|draw [val(s)]", TO_UNLIMITED, to_faceplate, GED_FUNC_PTR_NULL},
    {"fit_png_image",	"image_file_in req_width req_height scale image_file_out", 6, to_fit_png_image, GED_FUNC_PTR_NULL},
    {"fontsize",	"[fontsize]", 3, to_fontsize, GED_FUNC_PTR_NULL},
    {"get_prev_mouse",	"vname", TO_UNLIMITED, to_get_prev_mouse, GED_FUNC_PTR_NULL},
    {"handle_expose",	"vname count", TO_UNLIMITED, to_handle_expose, GED_FUNC_PTR_NULL},
    {"hide_view",	"vname [0|1]", 3, to_hide_view, GED_FUNC_PTR_NULL},
    {"idle_mode",	"vname", TO_UNLIMITED, to_idle_mode, GED_FUNC_PTR_NULL},
    {"init_view_bindings",	"vname", TO_UNLIMITED, to_init_view_bindings, GED_FUNC_PTR_NULL},
    {"light",	"[0|1]", TO_UNLIMITED, to_light, GED_FUNC_PTR_NULL},
    {"list_views",	(char *)0, TO_UNLIMITED, to_list_views, GED_FUNC_PTR_NULL},
    {"listen",	"[port]", TO_UNLIMITED, to_listen, GED_FUNC_PTR_NULL},
    {"local2base",	(char *)0, TO_UNLIMITED, to_local2base, GED_FUNC_PTR_NULL},
    {"make",	(char *)0, TO_UNLIMITED, to_make, GED_FUNC_PTR_NULL},
    {"mirror",	(char *)0, TO_UNLIMITED, to_mirror, GED_FUNC_PTR_NULL},
    {"model_axes",	"???", TO_UNLIMITED, to_model_axes, GED_FUNC_PTR_NULL},
    {"edit_motion_delta_callback",	"vname [args]", TO_UNLIMITED, to_edit_motion_delta_callback, GED_FUNC_PTR_NULL},
    {"more_args_callback",	"set/get the \"more args\" callback", TO_UNLIMITED, to_more_args_callback, GED_FUNC_PTR_NULL},
    {"move_arb_edge_mode",	"obj edge x y", TO_UNLIMITED, to_move_arb_edge_mode, GED_FUNC_PTR_NULL},
    {"move_arb_face_mode",	"obj face x y", TO_UNLIMITED, to_move_arb_face_mode, GED_FUNC_PTR_NULL},
    {"bot_move_pnt",	(char *)0, TO_UNLIMITED, to_bot_move_pnt, GED_FUNC_PTR_NULL},
    {"bot_move_pnts",	(char *)0, TO_UNLIMITED, to_bot_move_pnts, GED_FUNC_PTR_NULL},
    {"bot_move_pnt_mode",	"obj i mx my", TO_UNLIMITED, to_bot_move_pnt_mode, GED_FUNC_PTR_NULL},
    {"bot_move_pnts_mode",	"mx my obj i1 [i2 ... iN]", TO_UNLIMITED, to_bot_move_pnts_mode, GED_FUNC_PTR_NULL},
    {"metaball_move_pnt_mode",	"obj pt_i mx my", TO_UNLIMITED, to_metaball_move_pnt_mode, GED_FUNC_PTR_NULL},
    {"pipe_pnt_mode",	"obj seg_i mx my", TO_UNLIMITED, to_pipe_move_pnt_mode, GED_FUNC_PTR_NULL},
    {"mouse_brep_selection_append", "obj mx my", 5, to_mouse_brep_selection_append, GED_FUNC_PTR_NULL},
    {"mouse_brep_selection_translate", "obj mx my", 5, to_mouse_brep_selection_translate, GED_FUNC_PTR_NULL},
    {"mouse_constrain_rot",	"coord mx my", TO_UNLIMITED, to_mouse_constrain_rot, GED_FUNC_PTR_NULL},
    {"mouse_constrain_trans",	"coord mx my", TO_UNLIMITED, to_mouse_constrain_trans, GED_FUNC_PTR_NULL},
    {"mouse_data_scale",	"mx my", TO_UNLIMITED, to_mouse_data_scale, GED_FUNC_PTR_NULL},
    {"mouse_find_arb_edge",	"obj mx my ptol", TO_UNLIMITED, to_mouse_find_arb_edge, GED_FUNC_PTR_NULL},
    {"mouse_find_bot_edge",	"obj mx my", TO_UNLIMITED, to_mouse_find_bot_edge, GED_FUNC_PTR_NULL},
    {"mouse_find_bot_pnt",	"obj mx my", TO_UNLIMITED, to_mouse_find_bot_pnt, GED_FUNC_PTR_NULL},
    {"mouse_find_metaball_pnt",	"obj mx my", TO_UNLIMITED, to_mouse_find_metaball_pnt, GED_FUNC_PTR_NULL},
    {"mouse_find_pipe_pnt",	"obj mx my", TO_UNLIMITED, to_mouse_find_pipe_pnt, GED_FUNC_PTR_NULL},
    {"mouse_joint_select", "obj mx my", 5, to_mouse_joint_select, GED_FUNC_PTR_NULL},
    {"mouse_joint_selection_translate", "obj mx my", 5, to_mouse_joint_selection_translate, GED_FUNC_PTR_NULL},
    {"mouse_move_arb_edge",	"obj edge mx my", TO_UNLIMITED, to_mouse_move_arb_edge, GED_FUNC_PTR_NULL},
    {"mouse_move_arb_face",	"obj face mx my", TO_UNLIMITED, to_mouse_move_arb_face, GED_FUNC_PTR_NULL},
    {"mouse_move_bot_pnt",	"[-r] obj i mx my", TO_UNLIMITED, to_mouse_move_bot_pnt, GED_FUNC_PTR_NULL},
    {"mouse_move_bot_pnts",	"mx my obj i1 [i2 ... iN]", TO_UNLIMITED, to_mouse_move_bot_pnts, GED_FUNC_PTR_NULL},
    {"mouse_orotate",	"obj mx my", TO_UNLIMITED, to_mouse_orotate, GED_FUNC_PTR_NULL},
    {"mouse_oscale",	"obj mx my", TO_UNLIMITED, to_mouse_oscale, GED_FUNC_PTR_NULL},
    {"mouse_otranslate",	"obj mx my", TO_UNLIMITED, to_mouse_otranslate, GED_FUNC_PTR_NULL},
    {"mouse_poly_circ",	"mx my", TO_UNLIMITED, to_mouse_poly_circ, GED_FUNC_PTR_NULL},
    {"mouse_poly_cont",	"mx my", TO_UNLIMITED, to_mouse_poly_cont, GED_FUNC_PTR_NULL},
    {"mouse_poly_ell",	"mx my", TO_UNLIMITED, to_mouse_poly_ell, GED_FUNC_PTR_NULL},
    {"mouse_poly_rect",	"mx my", TO_UNLIMITED, to_mouse_poly_rect, GED_FUNC_PTR_NULL},
    {"mouse_ray",	"mx my", TO_UNLIMITED, to_mouse_ray, GED_FUNC_PTR_NULL},
    {"mouse_rect",	"mx my", TO_UNLIMITED, to_mouse_rect, GED_FUNC_PTR_NULL},
    {"mouse_rot",	"mx my", TO_UNLIMITED, to_mouse_rot, GED_FUNC_PTR_NULL},
    {"mouse_rotate_arb_face",	"obj face v mx my", TO_UNLIMITED, to_mouse_rotate_arb_face, GED_FUNC_PTR_NULL},
    {"mouse_scale",	"mx my", TO_UNLIMITED, to_mouse_scale, GED_FUNC_PTR_NULL},
    {"mouse_protate",	"obj attribute mx my", TO_UNLIMITED, to_mouse_protate, GED_FUNC_PTR_NULL},
    {"mouse_pscale",	"obj attribute mx my", TO_UNLIMITED, to_mouse_pscale, GED_FUNC_PTR_NULL},
    {"mouse_ptranslate",	"obj attribute mx my", TO_UNLIMITED, to_mouse_ptranslate, GED_FUNC_PTR_NULL},
    {"mouse_trans",	"mx my", TO_UNLIMITED, to_mouse_trans, GED_FUNC_PTR_NULL},
    {"new_view",	"vname type [args]", TO_UNLIMITED, to_new_view, GED_FUNC_PTR_NULL},
    {"orotate_mode",	"obj x y", TO_UNLIMITED, to_orotate_mode, GED_FUNC_PTR_NULL},
    {"oscale_mode",	"obj x y", TO_UNLIMITED, to_oscale_mode, GED_FUNC_PTR_NULL},
    {"otranslate_mode",	"obj x y", TO_UNLIMITED, to_otranslate_mode, GED_FUNC_PTR_NULL},
    {"paint_rect_area",	"vname", TO_UNLIMITED, to_paint_rect_area, GED_FUNC_PTR_NULL},
#ifdef HAVE_GL_GL_H
    {"pix",	"file", TO_UNLIMITED, to_pix, GED_FUNC_PTR_NULL},
    {"png",	"file", TO_UNLIMITED, to_png, GED_FUNC_PTR_NULL},
#endif
    {"poly_circ_mode",	"x y", TO_UNLIMITED, to_poly_circ_mode, GED_FUNC_PTR_NULL},
    {"poly_cont_build",	"x y", TO_UNLIMITED, to_poly_cont_build, GED_FUNC_PTR_NULL},
    {"poly_cont_build_end",	"y", TO_UNLIMITED, to_poly_cont_build_end, GED_FUNC_PTR_NULL},
    {"poly_ell_mode",	"x y", TO_UNLIMITED, to_poly_ell_mode, GED_FUNC_PTR_NULL},
    {"poly_rect_mode",	"x y [s]", TO_UNLIMITED, to_poly_rect_mode, GED_FUNC_PTR_NULL},
    {"prim_label",	"[prim_1 prim_2 ... prim_N]", TO_UNLIMITED, to_prim_label, GED_FUNC_PTR_NULL},
    {"protate_mode",	"obj attribute x y", TO_UNLIMITED, to_protate_mode, GED_FUNC_PTR_NULL},
    {"pscale_mode",	"obj attribute x y", TO_UNLIMITED, to_pscale_mode, GED_FUNC_PTR_NULL},
    {"ptranslate_mode",	"obj attribute x y", TO_UNLIMITED, to_ptranslate_mode, GED_FUNC_PTR_NULL},
    {"rect_mode",	"x y", TO_UNLIMITED, to_rect_mode, GED_FUNC_PTR_NULL},
    {"redraw",	"obj", 2, to_redraw, GED_FUNC_PTR_NULL},
    {"refresh",	"vname", TO_UNLIMITED, to_refresh, GED_FUNC_PTR_NULL},
    {"refresh_all",	(char *)0, TO_UNLIMITED, to_refresh_all, GED_FUNC_PTR_NULL},
    {"refresh_on",	"[0|1]", TO_UNLIMITED, to_refresh_on, GED_FUNC_PTR_NULL},
    {"rotate_arb_face_mode",	"obj face v x y", TO_UNLIMITED, to_rotate_arb_face_mode, GED_FUNC_PTR_NULL},
    {"rotate_mode",	"x y", TO_UNLIMITED, to_rotate_mode, GED_FUNC_PTR_NULL},
    {"rt_end_callback",	"[args]", TO_UNLIMITED, to_rt_end_callback, GED_FUNC_PTR_NULL},
    {"rt_gettrees",	"[-i] [-u] pname object", TO_UNLIMITED, to_rt_gettrees, GED_FUNC_PTR_NULL},
    {"scale_mode",	"x y", TO_UNLIMITED, to_scale_mode, GED_FUNC_PTR_NULL},
    {"screen2model",	"x y", TO_UNLIMITED, to_screen2model, GED_FUNC_PTR_NULL},
    {"screen2view",	"x y", TO_UNLIMITED, to_screen2view, GED_FUNC_PTR_NULL},
    {"sdata_arrows",	"???", TO_UNLIMITED, to_data_arrows, GED_FUNC_PTR_NULL},
    {"sdata_axes",	"???", TO_UNLIMITED, to_data_axes, GED_FUNC_PTR_NULL},
    {"sdata_labels",	"???", TO_UNLIMITED, to_data_labels, GED_FUNC_PTR_NULL},
    {"sdata_lines",	"???", TO_UNLIMITED, to_data_lines, GED_FUNC_PTR_NULL},
    {"sdata_polygons",	"???", TO_UNLIMITED, to_data_polygons, GED_FUNC_PTR_NULL},
    {"set_coord",	"[m|v]", TO_UNLIMITED, to_set_coord, GED_FUNC_PTR_NULL},
    {"set_fb_mode",	"[mode]", TO_UNLIMITED, to_set_fb_mode, GED_FUNC_PTR_NULL},
    {"snap_view",	"vx vy", 4, to_snap_view, GED_FUNC_PTR_NULL},
    {"translate_mode",	"x y", TO_UNLIMITED, to_translate_mode, GED_FUNC_PTR_NULL},
    {"transparency",	"[val]", TO_UNLIMITED, to_transparency, GED_FUNC_PTR_NULL},
    {"view_axes",	"vname [args]", TO_UNLIMITED, to_view_axes, GED_FUNC_PTR_NULL},
    {"view_callback",	"vname [args]", TO_UNLIMITED, to_view_callback, GED_FUNC_PTR_NULL},
    {"view_win_size",	"[s] | [x y]", 4, to_view_win_size, GED_FUNC_PTR_NULL},
    {"view2screen",	"", 2, to_view2screen, GED_FUNC_PTR_NULL},
    {"vmake",	"pname ptype", TO_UNLIMITED, to_vmake, GED_FUNC_PTR_NULL},
    {"vslew",	"x y", TO_UNLIMITED, to_vslew, GED_FUNC_PTR_NULL},
    {"zbuffer",	"[0|1]", TO_UNLIMITED, to_zbuffer, GED_FUNC_PTR_NULL},
    {"zclip",	"[0|1]", TO_UNLIMITED, to_zclip, GED_FUNC_PTR_NULL},
    {(char *)0,	(char *)0, 0, TO_WRAPPER_FUNC_PTR_NULL, GED_FUNC_PTR_NULL}
};


/**
 * @brief create the Tcl command for to_open
 *
 */
int
Ged_Init(Tcl_Interp *interp)
{

    if (library_initialized(0))
	return TCL_OK;

    {
	const char *version_str = brlcad_version();
	tclcad_eval_noresult(interp, "set brlcad_version", 1, &version_str);
    }

    BU_LIST_INIT(&HeadTclcadObj.l);
    (void)Tcl_CreateCommand(interp, (const char *)"go_open", to_open_tcl,
	    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    (void)Tcl_CreateCommand(interp, (const char *)"dm_list", dm_list_tcl,
	    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    (void)library_initialized(1);

    return TCL_OK;
}


/**
 * @brief
 * Generic interface for database commands.
 *
 * @par Usage:
 * procname cmd ?args?
 *
 * @return result of ged command.
 */
HIDDEN int
to_cmd(ClientData clientData,
	Tcl_Interp *interp,
	int argc,
	char **argv)
{
    struct to_cmdtab *ctp;
    struct tclcad_obj *top = (struct tclcad_obj *)clientData;
    Tcl_DString ds;
    int ret = GED_ERROR;

    Tcl_DStringInit(&ds);

    if (argc < 2) {
	Tcl_DStringAppend(&ds, "subcommand not specified; must be one of: ", -1);
	for (ctp = ged_cmds; ctp->to_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->to_name, -1);
	}
	for (ctp = to_cmds; ctp->to_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->to_name, -1);
	}
	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

    current_top = top;

    for (ctp = to_cmds; ctp->to_name != (char *)0; ctp++) {
	if (BU_STR_EQUAL(ctp->to_name, argv[1])) {
	    struct ged *gedp = top->to_gedp;
	    ret = (*ctp->to_wrapper_func)(gedp, argc-1, (const char **)argv+1, ctp->to_func, ctp->to_usage, ctp->to_maxargs);
	    break;
	}
    }
    if (ctp->to_name == (char *)0) {
	for (ctp = ged_cmds; ctp->to_name != (char *)0; ctp++) {
	    if (BU_STR_EQUAL(ctp->to_name, argv[1])) {
		struct ged *gedp = top->to_gedp;
		ret = (*ctp->to_wrapper_func)(gedp, argc-1, (const char **)argv+1, ctp->to_func, ctp->to_usage, ctp->to_maxargs);
		break;
	    }
	}
    }

    /* Command not found. */
    if (ctp->to_name == (char *)0) {
	Tcl_DStringAppend(&ds, "unknown subcommand: ", -1);
	Tcl_DStringAppend(&ds, argv[1], -1);
	Tcl_DStringAppend(&ds, "; must be one of: ", -1);

	for (ctp = ged_cmds; ctp->to_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->to_name, -1);
	}

	for (ctp = to_cmds; ctp->to_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->to_name, -1);
	}

	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

    Tcl_DStringAppend(&ds, bu_vls_addr(top->to_gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret & GED_ERROR)
	return TCL_ERROR;

    return TCL_OK;
}


HIDDEN void
free_path_edit_params(struct bu_hash_tbl *t)
{
    struct bu_hash_entry *entry = bu_hash_next(t, NULL);
    while (entry) {
	struct path_edit_params *pp = (struct path_edit_params *)bu_hash_value(entry, NULL);
	BU_PUT(pp, struct path_edit_params);
	entry = bu_hash_next(t, entry);
    }
}


/**
 * @brief
 * Called by Tcl when the object is destroyed.
 */
void
to_deleteProc(ClientData clientData)
{
    struct tclcad_obj *top = (struct tclcad_obj *)clientData;
    BU_LIST_DEQUEUE(&top->l);

    if (current_top == top)
	current_top = TCLCAD_OBJ_NULL;

    if (top->to_gedp) {

	// Clean up the libtclcad view data.
	struct bview *gdvp = NULL;
	for (size_t i = 0; i < BU_PTBL_LEN(&top->to_gedp->ged_views); i++) {
	    gdvp = (struct bview *)BU_PTBL_GET(&top->to_gedp->ged_views, i);

	    // There is a top level command created in the Tcl interp that is the name
	    // of the dm.  Clear that command.
	    struct bu_vls *dm_tcl_cmd = dm_get_pathname((struct dm *)gdvp->dmp);
	    if (dm_tcl_cmd && bu_vls_strlen(dm_tcl_cmd))
		Tcl_DeleteCommand(top->to_interp, bu_vls_cstr(dm_tcl_cmd));

	    // Close the dm.  This is not done by libged because libged only manages the
	    // data bview knows about.  From bview's perspective, dmp is just a pointer
	    // to an opaque data structure it knows nothing about.
	    (void)dm_close((struct dm *)gdvp->dmp);

	    // Delete libtclcad specific parts of data - ged_free (called by
	    // ged_close) will handle freeing the primary bview list entries.
	    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	    if (tvd) {
		bu_vls_free(&tvd->gdv_edit_motion_delta_callback);
		bu_vls_free(&tvd->gdv_callback);
		BU_PUT(tvd, struct tclcad_view_data);
		gdvp->u_data = NULL;
	    }

	}

	// Clean up the other libtclcad data
	if (top->to_gedp->u_data) {
	    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)top->to_gedp->u_data;
	    bu_vls_free(&tgd->go_rt_end_callback);
	    bu_vls_free(&tgd->go_more_args_callback);
	    free_path_edit_params(tgd->go_edited_paths);
	    bu_hash_destroy(tgd->go_edited_paths);
	    BU_PUT(tgd, struct tclcad_ged_data);
	    top->to_gedp->u_data = NULL;
	}
	if (top->to_gedp->ged_io_data) {
	    struct tclcad_io_data *t_iod = (struct tclcad_io_data *)top->to_gedp->ged_io_data;
	    tclcad_destroy_io_data(t_iod);
	    top->to_gedp->ged_io_data = NULL;
	}

	// Got the libtclcad cleanup done, have libged do its up.
	ged_close(top->to_gedp);

	BU_PUT(top->to_gedp, struct ged);
    }

    bu_free((void *)top, "struct ged_obj");
}


/**
 * @brief
 * Create a command named "oname" in "interp" using "gedp" as its state.
 *
 */
int
to_create_cmd(Tcl_Interp *interp,
	struct tclcad_obj *top,	/* pointer to object */
	const char *oname)	/* object name */
{
    if (top == TCLCAD_OBJ_NULL) {
	Tcl_AppendResult(interp, "to_create_cmd ", oname, " failed", NULL);
	return TCL_ERROR;
    }

    /* Instantiate the newprocname, with clientData of top */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(interp, oname, (Tcl_CmdProc *)to_cmd,
	    (ClientData)top, to_deleteProc);

    /* Return new function name as result */
    Tcl_AppendResult(interp, oname, (char *)NULL);

    return TCL_OK;
}

/**
 * @brief
 * A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 * @par Implicit return -
 * Creates a new TCL proc which responds to get/put/etc. arguments
 * when invoked.  clientData of that proc will be ged_obj pointer for
 * this instance of the database.  Easily allows keeping track of
 * multiple databases.
 *
 * @return wdb pointer, for more traditional C-style interfacing.
 *
 * @par Example -
 * set top [go_open .inmem inmem $dbip]
 *@n	.inmem get box.s
 *@n	.inmem close
 *
 *@n go_open db file "bob.g"
 *@n db get white.r
 *@n db close
 */
int
to_open_tcl(ClientData UNUSED(clientData),
	Tcl_Interp *interp,
	int argc,
	const char **argv)
{
    struct tclcad_obj *top = NULL;
    struct ged *gedp = NULL;
    const char *dbname = NULL;

    if (argc == 1) {
	/* get list of database objects */
	for (BU_LIST_FOR(top, tclcad_obj, &HeadTclcadObj.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&top->to_gedp->go_name), " ", (char *)NULL);

	return TCL_OK;
    }

    if (argc < 3 || 4 < argc) {
	Tcl_AppendResult(interp, "\
		Usage: go_open\n\
		go_open newprocname file filename\n\
		go_open newprocname disk $dbip\n\
		go_open newprocname disk_append $dbip\n\
		go_open newprocname inmem $dbip\n\
		go_open newprocname inmem_append $dbip\n\
		go_open newprocname db filename\n\
		go_open newprocname filename\n",
		NULL);
	return TCL_ERROR;
    }

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand(interp, argv[1]);

    if (argc == 3 || BU_STR_EQUAL(argv[2], "db")) {
	if (argc == 3) {
	    dbname = argv[2];
	    gedp = ged_open("filename", dbname, 0);
	} else {
	    dbname = argv[3];
	    gedp = ged_open("db", dbname, 0);
	}
    } else {
	dbname = argv[3];
	gedp = ged_open(argv[2], dbname, 0);
    }

    if (gedp == GED_NULL) {
	Tcl_AppendResult(interp, "Unable to open geometry database: ", dbname, (char *)NULL);
	return TCL_ERROR;
    }
    gedp->ged_interp = (void *)interp;

    /* Set the Tcl specific I/O handlers for asynchronous subprocess I/O */
    struct tclcad_io_data *t_iod = tclcad_create_io_data();
    t_iod->io_mode  = TCL_READABLE;
    t_iod->interp = interp;
    gedp->ged_io_data = (void *)t_iod;
    gedp->ged_create_io_handler = &tclcad_create_io_handler;
    gedp->ged_delete_io_handler = &tclcad_delete_io_handler;

    /* initialize tclcad_obj */
    BU_ALLOC(top, struct tclcad_obj);
    top->to_interp = interp;

    BU_ASSERT(gedp != NULL);
    top->to_gedp = gedp;

    top->to_gedp->ged_output_handler = to_output_handler;
    top->to_gedp->ged_refresh_handler = to_refresh_handler;
    top->to_gedp->ged_create_vlist_solid_callback = to_create_vlist_callback_solid;
    top->to_gedp->ged_create_vlist_display_list_callback = to_create_vlist_callback;
    top->to_gedp->ged_destroy_vlist_callback = to_destroy_vlist_callback;

    BU_ASSERT(gedp->ged_gdp != NULL);
    top->to_gedp->ged_gdp->gd_rtCmdNotify = to_rt_end_callback_internal;

    // Initialize libtclcad GED data container
    struct tclcad_ged_data *tgd;
    BU_GET(tgd, struct tclcad_ged_data);
    bu_vls_init(&tgd->go_rt_end_callback);
    tgd->go_rt_end_callback_cnt = 0;
    bu_vls_init(&tgd->go_more_args_callback);
    tgd->go_more_args_callback_cnt = 0;
    tgd->go_edited_paths = bu_hash_create(0);
    tgd->gedp = top->to_gedp;
    tgd->go_refresh_on = 1;
    gedp->u_data = (void *)tgd;

    bu_vls_strcpy(&top->to_gedp->go_name, argv[1]);

    /* append to list of tclcad_obj */
    BU_LIST_APPEND(&HeadTclcadObj.l, &top->l);

    return to_create_cmd(interp, top, argv[1]);
}


/*************************** Local Command Functions ***************************/

HIDDEN int
to_base2local(struct ged *gedp,
	int UNUSED(argc),
	const char *UNUSED(argv[]),
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%lf", current_top->to_gedp->ged_wdbp->dbip->dbi_base2local);

    return GED_OK;
}


HIDDEN int
to_bg(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int r, g, b;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get background color */
    if (argc == 2) {
	unsigned char *dm_bg = dm_get_bg((struct dm *)gdvp->dmp);
	if (dm_bg) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
		    dm_bg[0],
		    dm_bg[1],
		    dm_bg[2]);
	}
	return GED_OK;
    }

    /* set background color */
    if (bu_sscanf(argv[2], "%d", &r) != 1 ||
	    bu_sscanf(argv[3], "%d", &g) != 1 ||
	    bu_sscanf(argv[4], "%d", &b) != 1)
	goto bad_color;

    /* validate color */
    if (r < 0 || 255 < r ||
	    g < 0 || 255 < g ||
	    b < 0 || 255 < b)
	goto bad_color;

    (void)dm_make_current((struct dm *)gdvp->dmp);
    (void)dm_set_bg((struct dm *)gdvp->dmp, (unsigned char)r, (unsigned char)g, (unsigned char)b);

    to_refresh_view(gdvp);

    return GED_OK;

bad_color:
    bu_vls_printf(gedp->ged_result_str, "%s: %s %s %s", argv[0], argv[2], argv[3], argv[4]);
    return GED_ERROR;
}


HIDDEN int
to_bounds(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    fastf_t bounds[6];

    /* must be double for scanf */
    double scan[6];

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get window bounds */
    if (argc == 2) {
	vect_t *cmin = dm_get_clipmin((struct dm *)gdvp->dmp);
	vect_t *cmax = dm_get_clipmax((struct dm *)gdvp->dmp);
	if (cmin && cmax) {
	    bu_vls_printf(gedp->ged_result_str, "%g %g %g %g %g %g",
		    (*cmin)[X], (*cmax)[X], (*cmin)[Y], (*cmax)[Y], (*cmin)[Z], (*cmax)[Z]);
	}
	return GED_OK;
    }

    /* set window bounds */
    if (bu_sscanf(argv[2], "%lf %lf %lf %lf %lf %lf",
		&scan[0], &scan[1],
		&scan[2], &scan[3],
		&scan[4], &scan[5]) != 6) {
	bu_vls_printf(gedp->ged_result_str, "%s: invalid bounds - %s", argv[0], argv[2]);
	return GED_ERROR;
    }
    /* convert double to fastf_t */
    VMOVE(bounds, scan);         /* first point */
    VMOVE(&bounds[3], &scan[3]); /* second point */

    /*
     * Since dm_bound doesn't appear to be used anywhere, I'm going to
     * use it for controlling the location of the zclipping plane in
     * dm-ogl.c. dm-X.c uses dm_clipmin and dm_clipmax.
     */
    if (dm_get_clipmax((struct dm *)gdvp->dmp) && (*dm_get_clipmax((struct dm *)gdvp->dmp))[2] <= GED_MAX)
	dm_set_bound((struct dm *)gdvp->dmp, 1.0);
    else
	dm_set_bound((struct dm *)gdvp->dmp, GED_MAX/((*dm_get_clipmax((struct dm *)gdvp->dmp))[2]));

    (void)dm_make_current((struct dm *)gdvp->dmp);
    (void)dm_set_win_bounds((struct dm *)gdvp->dmp, bounds);

    return GED_OK;
}


HIDDEN int
to_configure(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int status;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* configure the display manager window */
    status = dm_configure_win((struct dm *)gdvp->dmp, 0);

    /* configure the framebuffer window */
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    if (tvd->gdv_fbs.fbs_fbp != FB_NULL)
	(void)fb_configure_window(tvd->gdv_fbs.fbs_fbp, dm_get_width((struct dm *)gdvp->dmp), dm_get_height((struct dm *)gdvp->dmp));

    {
	char cdimX[32];
	char cdimY[32];
	const char *av[5];

	snprintf(cdimX, 32, "%d", dm_get_width((struct dm *)gdvp->dmp));
	snprintf(cdimY, 32, "%d", dm_get_height((struct dm *)gdvp->dmp));

	av[0] = "rect";
	av[1] = "cdim";
	av[2] = cdimX;
	av[3] = cdimY;
	av[4] = NULL;

	gedp->ged_gvp = gdvp;
	(void)ged_rect(gedp, 4, (const char **)av);
    }

    if (status == TCL_OK) {
	to_refresh_view(gdvp);
	return GED_OK;
    }

    return GED_ERROR;
}


HIDDEN int
to_constrain_rmode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if ((argv[2][0] != 'x' &&
		argv[2][0] != 'y' &&
		argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	    bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_CONSTRAINED_ROTATE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_constrain_rot %s %s %%x %%y}; break",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_constrain_tmode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if ((argv[2][0] != 'x' &&
		argv[2][0] != 'y' &&
		argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_OK;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	    bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_CONSTRAINED_TRANSLATE_MODE;

    if (dm_get_pathname((struct dm *)gdvp->dmp)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_constrain_trans %s %s %%x %%y}; break",
		bu_vls_addr(dm_get_pathname((struct dm *)gdvp->dmp)),
		bu_vls_addr(&current_top->to_gedp->go_name),
		bu_vls_addr(&gdvp->gv_name),
		argv[2]);
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_copy(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct ged *from_gedp = GED_NULL;
    struct ged *to_gedp = GED_NULL;
    int ret;
    const char *cp;
    struct tclcad_obj *top;
    struct bu_vls db_vls = BU_VLS_INIT_ZERO;
    struct bu_vls from_vls = BU_VLS_INIT_ZERO;
    struct bu_vls to_vls = BU_VLS_INIT_ZERO;
    int fflag;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 4) {
	if (argv[1][0] != '-' || argv[1][1] != 'f' ||  argv[1][2] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

	fflag = 1;

	/* Advance past the -f option */
	--argc;
	++argv;
    } else
	fflag = 0;

    cp = strchr(argv[1], ':');
    if (cp) {
	bu_vls_strncpy(&db_vls, argv[1], cp-argv[1]);
	bu_vls_strcpy(&from_vls, cp+1);

	for (BU_LIST_FOR(top, tclcad_obj, &HeadTclcadObj.l)) {
	    if (BU_STR_EQUAL(bu_vls_addr(&top->to_gedp->go_name), bu_vls_addr(&db_vls))) {
		from_gedp = top->to_gedp;
		break;
	    }
	}

	bu_vls_free(&db_vls);

	if (from_gedp == GED_NULL) {
	    bu_vls_free(&from_vls);
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	bu_vls_strcpy(&from_vls, argv[1]);
	from_gedp = gedp;
    }

    cp = strchr(argv[2], ':');
    if (cp) {
	bu_vls_trunc(&db_vls, 0);
	bu_vls_strncpy(&db_vls, argv[2], cp-argv[2]);
	bu_vls_strcpy(&to_vls, cp+1);

	for (BU_LIST_FOR(top, tclcad_obj, &HeadTclcadObj.l)) {
	    if (BU_STR_EQUAL(bu_vls_addr(&top->to_gedp->go_name), bu_vls_addr(&db_vls))) {
		to_gedp = top->to_gedp;
		break;
	    }
	}

	bu_vls_free(&db_vls);

	if (to_gedp == GED_NULL) {
	    bu_vls_free(&from_vls);
	    bu_vls_free(&to_vls);
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	bu_vls_strcpy(&to_vls, argv[2]);
	to_gedp = gedp;
    }

    if (from_gedp == to_gedp) {
	ret = ged_dbcopy(from_gedp, to_gedp,
		bu_vls_addr(&from_vls),
		bu_vls_addr(&to_vls),
		fflag);

	if (ret != GED_OK && from_gedp != gedp)
	    bu_vls_strcpy(gedp->ged_result_str, bu_vls_addr(from_gedp->ged_result_str));
    } else {
	ret = ged_dbcopy(from_gedp, to_gedp,
		bu_vls_addr(&from_vls),
		bu_vls_addr(&to_vls),
		fflag);

	if (ret != GED_OK) {
	    if (bu_vls_strlen(from_gedp->ged_result_str)) {
		if (from_gedp != gedp)
		    bu_vls_strcpy(gedp->ged_result_str, bu_vls_addr(from_gedp->ged_result_str));
	    } else if (to_gedp != gedp && bu_vls_strlen(to_gedp->ged_result_str))
		bu_vls_strcpy(gedp->ged_result_str, bu_vls_addr(to_gedp->ged_result_str));
	}
    }

    bu_vls_free(&from_vls);
    bu_vls_free(&to_vls);

    return ret;
}


int
go_data_move(Tcl_Interp *UNUSED(interp),
	struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_refresh_on = 0;
    }

    return to_data_move_func(gedp, gdvp, argc, argv, usage);
}


/*
 * Usage: data_move vname dtype dindex mx my
 */
HIDDEN int
to_data_move(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 5 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_arrows_func */
    argv[1] = argv[0];
    return to_data_move_func(gedp, gdvp, argc-1, argv+1, usage);
}


HIDDEN int
to_data_move_func(struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage)
{
    int mx, my;
    int width, height;
    int dindex;
    fastf_t cx, cy;
    fastf_t vx, vy;
    fastf_t sf;
    point_t mpoint, vpoint;

    if (bu_sscanf(argv[2], "%d", &dindex) != 1 || dindex < 0)
	goto bad;

    if (argc == 4) {
	if (bu_sscanf(argv[3], "%d %d", &mx, &my) != 2)
	    goto bad;
    } else {
	if (bu_sscanf(argv[3], "%d", &mx) != 1)
	    goto bad;

	if (bu_sscanf(argv[4], "%d", &my) != 1)
	    goto bad;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    cx = 0.5 * (fastf_t)width;
    height = dm_get_height((struct dm *)gdvp->dmp);
    cy = 0.5 * (fastf_t)height;
    sf = 2.0 / width;
    vx = (mx - cx) * sf;
    vy = (cy - my) * sf;

    if (BU_STR_EQUAL(argv[1], "data_polygons")) {
	size_t i, j, k;
	bview_data_polygon_state *gdpsp = &gdvp->gv_data_polygons;

	if (bu_sscanf(argv[2], "%zu %zu %zu", &i, &j, &k) != 3)
	    goto bad;

	/* Silently ignore */
	if (i >= gdpsp->gdps_polygons.num_polygons ||
		j >= gdpsp->gdps_polygons.polygon[i].num_contours ||
		k >= gdpsp->gdps_polygons.polygon[i].contour[j].num_points)
	    return GED_OK;

	/* This section is for moving more than a single point on a contour */
	if (gdvp->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    VMOVE(old_mpoint, gdpsp->gdps_polygons.polygon[i].contour[j].point[k]);

	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdpsp->gdps_polygons.polygon[i].contour[j].point[k]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    /* Move all polygons and all their respective contours. */
	    if (gdpsp->gdps_moveAll) {
		size_t p, c;
		for (p = 0; p < gdpsp->gdps_polygons.num_polygons; ++p) {
		    for (c = 0; c < gdpsp->gdps_polygons.polygon[p].num_contours; ++c) {
			for (k = 0; k < gdpsp->gdps_polygons.polygon[p].contour[c].num_points; ++k) {
			    VADD2(gdpsp->gdps_polygons.polygon[p].contour[c].point[k],
				    gdpsp->gdps_polygons.polygon[p].contour[c].point[k],
				    diff);
			}
		    }
		}
	    } else {
		/* Move only the contour. */
		for (k = 0; k < gdpsp->gdps_polygons.polygon[i].contour[j].num_points; ++k) {
		    VADD2(gdpsp->gdps_polygons.polygon[i].contour[j].point[k],
			    gdpsp->gdps_polygons.polygon[i].contour[j].point[k],
			    diff);
		}
	    }
	} else {
	    /* This section is for moving a single point on a contour */
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdpsp->gdps_polygons.polygon[i].contour[j].point[k]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(gdpsp->gdps_polygons.polygon[i].contour[j].point[k], gdvp->gv_view2model, vpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "data_arrows")) {
	struct bview_data_arrow_state *gdasp = &gdvp->gv_data_arrows;

	/* Silently ignore */
	if (dindex >= gdvp->gv_data_arrows.gdas_num_points)
	    return GED_OK;

	/* This section is for moving the entire arrow */
	if (gdvp->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdasp->gdas_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdasp->gdas_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdasp->gdas_points[dindexA], new_mpoint);
	    VADD2(gdasp->gdas_points[dindexB], gdasp->gdas_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdasp->gdas_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	    VMOVE(gdasp->gdas_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "sdata_arrows")) {
	struct bview_data_arrow_state *gdasp = &gdvp->gv_sdata_arrows;

	/* Silently ignore */
	if (dindex >= gdvp->gv_sdata_arrows.gdas_num_points)
	    return GED_OK;

	/* This section is for moving the entire arrow */
	if (gdvp->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdasp->gdas_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdasp->gdas_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdasp->gdas_points[dindexA], new_mpoint);
	    VADD2(gdasp->gdas_points[dindexB], gdasp->gdas_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdasp->gdas_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	    VMOVE(gdasp->gdas_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "data_axes")) {
	struct bview_data_axes_state *gdasp = &gdvp->gv_data_axes;

	/* Silently ignore */
	if (dindex >= gdvp->gv_data_axes.num_points)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gv_model2view, gdasp->points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	VMOVE(gdasp->points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "sdata_axes")) {
	struct bview_data_axes_state *gdasp = &gdvp->gv_sdata_axes;

	/* Silently ignore */
	if (dindex >= gdvp->gv_sdata_axes.num_points)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gv_model2view, gdasp->points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	VMOVE(gdasp->points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }


    if (BU_STR_EQUAL(argv[1], "data_labels")) {
	struct bview_data_label_state *gdlsp = &gdvp->gv_data_labels;

	/* Silently ignore */
	if (dindex >= gdvp->gv_data_labels.gdls_num_labels)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gv_model2view, gdlsp->gdls_points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	VMOVE(gdlsp->gdls_points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "sdata_labels")) {
	struct bview_data_label_state *gdlsp = &gdvp->gv_sdata_labels;

	/* Silently ignore */
	if (dindex >= gdvp->gv_sdata_labels.gdls_num_labels)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gv_model2view, gdlsp->gdls_points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	VMOVE(gdlsp->gdls_points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "data_lines")) {
	struct bview_data_line_state *gdlsp = &gdvp->gv_data_lines;

	/* Silently ignore */
	if (dindex >= gdvp->gv_data_lines.gdls_num_points)
	    return GED_OK;

	/* This section is for moving the entire line */
	if (gdvp->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdlsp->gdls_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdlsp->gdls_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdlsp->gdls_points[dindexA], new_mpoint);
	    VADD2(gdlsp->gdls_points[dindexB], gdlsp->gdls_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdlsp->gdls_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	    VMOVE(gdlsp->gdls_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "sdata_lines")) {
	struct bview_data_line_state *gdlsp = &gdvp->gv_sdata_lines;

	/* Silently ignore */
	if (dindex >= gdvp->gv_sdata_lines.gdls_num_points)
	    return GED_OK;

	/* This section is for moving the entire line */
	if (gdvp->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdlsp->gdls_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdlsp->gdls_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdlsp->gdls_points[dindexA], new_mpoint);
	    VADD2(gdlsp->gdls_points[dindexB], gdlsp->gdls_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdlsp->gdls_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gv_view2model, vpoint);
	    VMOVE(gdlsp->gdls_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


int
go_data_move_object_mode(Tcl_Interp *UNUSED(interp),
	struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_refresh_on = 0;
    }

    return to_data_move_object_mode_func(gedp, gdvp, argc, argv, usage);
}


HIDDEN int
to_data_move_object_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_move_point_mode_func */
    argv[1] = argv[0];
    return to_data_move_object_mode_func(gedp, gdvp, argc-1, argv+1, usage);
}


HIDDEN int
to_data_move_object_mode_func(struct ged *gedp,
	struct bview *gdvp,
	int UNUSED(argc),
	const char *argv[],
	const char *usage)
{
    int x, y;

    gedp->ged_gvp = gdvp;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	    bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* At the moment, only gv_mode is being used. */
    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_DATA_MOVE_OBJECT_MODE;

    return GED_OK;
}


int
go_data_move_point_mode(Tcl_Interp *UNUSED(interp),
	struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_refresh_on = 0;
    }

    return to_data_move_point_mode_func(gedp, gdvp, argc, argv, usage);
}


HIDDEN int
to_data_move_point_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_move_point_mode_func */
    argv[1] = argv[0];
    return to_data_move_point_mode_func(gedp, gdvp, argc-1, argv+1, usage);
}


HIDDEN int
to_data_move_point_mode_func(struct ged *gedp,
	struct bview *gdvp,
	int UNUSED(argc),
	const char *argv[],
	const char *usage)
{
    int x, y;

    gedp->ged_gvp = gdvp;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	    bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* At the moment, only gv_mode is being used. */
    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_DATA_MOVE_POINT_MODE;

    return GED_OK;
}


int
go_data_pick(struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2 || 3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_refresh_on = 0;
    }

    return to_data_pick_func(gedp, gdvp, argc, argv, usage);
}


HIDDEN int
to_data_pick(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_pick_func */
    argv[1] = argv[0];
    return to_data_pick_func(gedp, gdvp, argc-1, argv+1, usage);
}


HIDDEN int
to_data_pick_func(struct ged *gedp,
	struct bview *gdvp,
	int argc,
	const char *argv[],
	const char *usage)
{
    int mx, my, width, height;
    fastf_t cx, cy;
    fastf_t vx, vy;
    fastf_t sf;
    point_t dpoint, vpoint;
    int i;
    fastf_t top_z = -MAX_FASTF;
    point_t top_point = VINIT_ZERO;
    size_t top_i = 0;
    size_t top_j = 0;
    size_t top_k = 0;
    int found_top = 0;
    const char *top_data_str = NULL;
    const char *top_data_label = NULL;
    static fastf_t tol = 0.015;
    static const char *data_polygons_str = "data_polygons";
    static const char *data_labels_str = "data_labels";
    static const char *sdata_labels_str = "sdata_labels";
    static const char *data_lines_str = "data_lines";
    static const char *sdata_lines_str = "sdata_lines";
    static const char *data_arrows_str = "data_arrows";
    static const char *sdata_arrows_str = "sdata_arrows";
    static const char *data_axes_str = "data_axes";
    static const char *sdata_axes_str = "sdata_axes";

    if (argc == 2) {
	if (bu_sscanf(argv[1], "%d %d", &mx, &my) != 2)
	    goto bad;
    } else {
	if (bu_sscanf(argv[1], "%d", &mx) != 1)
	    goto bad;

	if (bu_sscanf(argv[2], "%d", &my) != 1)
	    goto bad;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    cx = 0.5 * (fastf_t)width;
    height = dm_get_height((struct dm *)gdvp->dmp);
    cy = 0.5 * (fastf_t)height;
    sf = 2.0 / width;
    vx = (mx - cx) * sf;
    vy = (cy - my) * sf;

    /* check for polygon points */
    if (gdvp->gv_data_polygons.gdps_draw &&
	    gdvp->gv_data_polygons.gdps_polygons.num_polygons) {
	size_t si, sj, sk;

	bview_data_polygon_state *gdpsp = &gdvp->gv_data_polygons;

	for (si = 0; si < gdpsp->gdps_polygons.num_polygons; ++si)
	    for (sj = 0; sj < gdpsp->gdps_polygons.polygon[si].num_contours; ++sj)
		for (sk = 0; sk < gdpsp->gdps_polygons.polygon[si].contour[sj].num_points; ++sk) {
		    fastf_t minX, maxX;
		    fastf_t minY, maxY;

		    MAT4X3PNT(vpoint, gdvp->gv_model2view, gdpsp->gdps_polygons.polygon[si].contour[sj].point[sk]);
		    minX = vpoint[X] - tol;
		    maxX = vpoint[X] + tol;
		    minY = vpoint[Y] - tol;
		    maxY = vpoint[Y] + tol;

		    if (minX < vx && vx < maxX &&
			    minY < vy && vy < maxY) {
			if (!found_top || top_z < vpoint[Z]) {
			    top_z = vpoint[Z];
			    top_data_str = data_polygons_str;
			    top_i = si;
			    top_j = sj;
			    top_k = sk;
			    VMOVE(top_point, gdpsp->gdps_polygons.polygon[si].contour[sj].point[sk]);
			    found_top = 1;
			}
		    }
		}
    }

    if (found_top) {
	bu_vls_printf(gedp->ged_result_str, "%s {%zu %zu %zu} {%lf %lf %lf}",
		top_data_str, top_i, top_j, top_k, V3ARGS(top_point));
	return GED_OK;
    }

    /* check for label points */
    if (gdvp->gv_data_labels.gdls_draw &&
	    gdvp->gv_data_labels.gdls_num_labels) {
	struct bview_data_label_state *gdlsp = &gdvp->gv_data_labels;

	for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X];
	    maxX = vpoint[X] + (2 * tol);
	    minY = vpoint[Y];
	    maxY = vpoint[Y] + (2 * tol);

	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (!found_top || top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_data_str = data_labels_str;
		    top_i = i;
		    top_data_label = gdlsp->gdls_labels[i];
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
	    }
	}
    }

    /* check for selected label points */
    if (gdvp->gv_sdata_labels.gdls_draw &&
	    gdvp->gv_sdata_labels.gdls_num_labels) {
	struct bview_data_label_state *gdlsp = &gdvp->gv_sdata_labels;

	for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X];
	    maxX = vpoint[X] + (2 * tol);
	    minY = vpoint[Y];
	    maxY = vpoint[Y] + (2 * tol);

	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (!found_top || top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_data_str = sdata_labels_str;
		    top_i = i;
		    top_data_label = gdlsp->gdls_labels[i];
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
	    }
	}
    }

    if (found_top) {
	bu_vls_printf(gedp->ged_result_str, "%s %zu {{%s} {%lf %lf %lf}}",
		top_data_str, top_i, top_data_label, V3ARGS(top_point));
	return GED_OK;
    }

    /* check for line points */
    if (gdvp->gv_data_lines.gdls_draw &&
	    gdvp->gv_data_lines.gdls_num_points) {
	struct bview_data_line_state *gdlsp = &gdvp->gv_data_lines;

	for (i = 0; i < gdlsp->gdls_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X] - tol;
	    maxX = vpoint[X] + tol;
	    minY = vpoint[Y] - tol;
	    maxY = vpoint[Y] + tol;
	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_data_str = data_lines_str;
		    top_i = i;
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
		bu_vls_printf(gedp->ged_result_str, "data_lines %d {%lf %lf %lf}", i, V3ARGS(dpoint));
		return GED_OK;
	    }
	}
    }

    /* check for selected line points */
    if (gdvp->gv_sdata_lines.gdls_draw &&
	    gdvp->gv_sdata_lines.gdls_num_points) {
	struct bview_data_line_state *gdlsp = &gdvp->gv_sdata_lines;

	for (i = 0; i < gdlsp->gdls_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X] - tol;
	    maxX = vpoint[X] + tol;
	    minY = vpoint[Y] - tol;
	    maxY = vpoint[Y] + tol;
	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (!found_top || top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_data_str = sdata_lines_str;
		    top_i = i;
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
	    }
	}
    }

    if (found_top) {
	bu_vls_printf(gedp->ged_result_str, "%s %zu {%lf %lf %lf}",
		top_data_str, top_i, V3ARGS(top_point));
	return GED_OK;
    }

    /* check for arrow points */
    if (gdvp->gv_data_arrows.gdas_draw &&
	    gdvp->gv_data_arrows.gdas_num_points) {
	struct bview_data_arrow_state *gdasp = &gdvp->gv_data_arrows;

	for (i = 0; i < gdasp->gdas_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->gdas_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X] - tol;
	    maxX = vpoint[X] + tol;
	    minY = vpoint[Y] - tol;
	    maxY = vpoint[Y] + tol;
	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (!found_top || top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_data_str = data_arrows_str;
		    top_i = i;
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
	    }
	}
    }

    /* check for selected arrow points */
    if (gdvp->gv_sdata_arrows.gdas_draw &&
	    gdvp->gv_sdata_arrows.gdas_num_points) {
	struct bview_data_arrow_state *gdasp = &gdvp->gv_sdata_arrows;

	for (i = 0; i < gdasp->gdas_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->gdas_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X] - tol;
	    maxX = vpoint[X] + tol;
	    minY = vpoint[Y] - tol;
	    maxY = vpoint[Y] + tol;
	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (!found_top || top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_data_str = sdata_arrows_str;
		    top_i = i;
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
	    }
	}
    }

    if (found_top) {
	bu_vls_printf(gedp->ged_result_str, "%s %zu {%lf %lf %lf}",
		top_data_str, top_i, V3ARGS(top_point));
	return GED_OK;
    }

    /* check for axes points */
    if (gdvp->gv_data_axes.draw &&
	    gdvp->gv_data_axes.num_points) {
	struct bview_data_axes_state *gdasp = &gdvp->gv_data_axes;

	for (i = 0; i < gdasp->num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X] - tol;
	    maxX = vpoint[X] + tol;
	    minY = vpoint[Y] - tol;
	    maxY = vpoint[Y] + tol;
	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (!found_top || top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_i = i;
		    top_data_str = data_axes_str;
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
	    }
	}
    }

    /* check for selected axes points */
    if (gdvp->gv_sdata_axes.draw &&
	    gdvp->gv_sdata_axes.num_points) {
	struct bview_data_axes_state *gdasp = &gdvp->gv_sdata_axes;

	for (i = 0; i < gdasp->num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->points[i]);
	    MAT4X3PNT(vpoint, gdvp->gv_model2view, dpoint);

	    minX = vpoint[X] - tol;
	    maxX = vpoint[X] + tol;
	    minY = vpoint[Y] - tol;
	    maxY = vpoint[Y] + tol;
	    if (minX < vx && vx < maxX &&
		    minY < vy && vy < maxY) {
		if (!found_top || top_z < vpoint[Z]) {
		    top_z = vpoint[Z];
		    top_i = i;
		    top_data_str = sdata_axes_str;
		    VMOVE(top_point, dpoint);
		    found_top = 1;
		}
	    }
	}
    }

    if (found_top)
	bu_vls_printf(gedp->ged_result_str, "%s %zu {%lf %lf %lf}",
		top_data_str, top_i, V3ARGS(top_point));

    return GED_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


HIDDEN int
to_data_vZ(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* must be double for scanf */
    double vZ;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Get the data vZ */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%lf", gdvp->gv_data_vZ);
	return GED_OK;
    }

    /* Set the data vZ */
    if (bu_sscanf(argv[2], "%lf", &vZ) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_data_vZ = vZ;

    return GED_OK;
}


HIDDEN void
to_init_default_bindings(struct bview *gdvp)
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    if (dm_get_pathname((struct dm *)gdvp->dmp)) {
	struct bu_vls *pathvls = dm_get_pathname((struct dm *)gdvp->dmp);
	if (pathvls) {
	    bu_vls_printf(&bindings, "bind %s <Configure> {%s configure %s; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Enter> {focus %s; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(pathvls));
	    bu_vls_printf(&bindings, "bind %s <Expose> {%s handle_expose %s %%c; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "catch {wm protocol %s WM_DELETE_WINDOW {%s delete_view %s; break}}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Mouse Bindings */
	    bu_vls_printf(&bindings, "bind %s <2> {%s vslew %s %%x %%y; focus %s; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name),
		    bu_vls_addr(pathvls));
	    bu_vls_printf(&bindings, "bind %s <1> {%s zoom %s 0.5; focus %s; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name),
		    bu_vls_addr(pathvls));
	    bu_vls_printf(&bindings, "bind %s <3> {%s zoom %s 2.0; focus %s;  break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name),
		    bu_vls_addr(pathvls));
	    bu_vls_printf(&bindings, "bind %s <4> {%s zoom %s 1.1; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <5> {%s zoom %s 0.9; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <MouseWheel> {if {%%D < 0} {%s zoom %s 0.9} else {%s zoom %s 1.1}; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Idle Mode */
	    bu_vls_printf(&bindings, "bind %s <ButtonRelease> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <KeyRelease-Control_L> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <KeyRelease-Control_R> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <KeyRelease-Shift_L> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <KeyRelease-Shift_R> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <KeyRelease-Alt_L> {%s idle_mode %s; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <KeyRelease-Alt_R> {%s idle_mode %s; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Rotate Mode */
	    bu_vls_printf(&bindings, "bind %s <Control-ButtonRelease-1> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-ButtonPress-1> {%s rotate_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-ButtonPress-2> {%s rotate_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-ButtonPress-3> {%s rotate_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Translate Mode */
	    bu_vls_printf(&bindings, "bind %s <Shift-ButtonRelease-1> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Shift-ButtonPress-1> {%s translate_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Shift-ButtonPress-2> {%s translate_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Shift-ButtonPress-3> {%s translate_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Scale Mode */
	    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonRelease-1> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonPress-1> {%s scale_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonPress-2> {%s scale_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonPress-3> {%s scale_mode %s %%x %%y}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Constrained Rotate Mode */
	    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonRelease-1> {%s idle_mode %s}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonPress-1> {%s constrain_rmode %s x %%x %%y; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonPress-2> {%s constrain_rmode %s y %%x %%y; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonPress-3> {%s constrain_rmode %s z %%x %%y; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Constrained Translate Mode */
	    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonRelease-1> {%s idle_mode %s; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonPress-1> {%s constrain_tmode %s x %%x %%y; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonPress-2> {%s constrain_tmode %s y %%x %%y; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonPress-3> {%s constrain_tmode %s z %%x %%y; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    /* Key Bindings */
	    bu_vls_printf(&bindings, "bind %s 3 {%s aet %s 35 25; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s 4 {%s aet %s 45 45; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s f {%s aet %s 0 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s F {%s aet %s 0 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s R {%s aet %s 180 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s r {%s aet %s 270 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s l {%s aet %s 90 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s L {%s aet %s 90 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s t {%s aet %s 270 90; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s T {%s aet %s 270 90; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s b {%s aet %s 270 -90; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s B {%s aet %s 270 -90; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s + {%s zoom %s 2.0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s = {%s zoom %s 2.0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s _ {%s zoom %s 0.5; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s - {%s zoom %s 0.5; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Key-Left> {%s rot %s -v 0 1 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Key-Right> {%s rot %s -v 0 -1 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Key-Up> {%s rot %s -v 1 0 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));
	    bu_vls_printf(&bindings, "bind %s <Key-Down> {%s rot %s -v -1 0 0; break}; ",
		    bu_vls_addr(pathvls),
		    bu_vls_addr(&current_top->to_gedp->go_name),
		    bu_vls_addr(&gdvp->gv_name));

	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
	}
    }
    bu_vls_free(&bindings);
}


HIDDEN int
to_dlist_on(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
    int on;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (2 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    /* Get dlist_on state */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d", tgd->go_dlist_on);
	return GED_OK;
    }

    /* Set dlist_on state */
    if (bu_sscanf(argv[1], "%d", &on) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    tgd->go_dlist_on = on;

    return GED_OK;
}

HIDDEN int
to_dplot(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ac;
    int ret;
    char *av[256];
    struct bu_vls callback_cmd = BU_VLS_INIT_ZERO;
    struct bu_vls temp = BU_VLS_INIT_ZERO;
    struct bu_vls result_copy = BU_VLS_INIT_ZERO;
    struct bview *gdvp;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
    const char *who_av[3] = {"who", "b", NULL};
    int first = 1;
    int aflag = 0;

    /* copy all args */
    ac = argc;
    for (int i = 0; i < ac; ++i)
	av[i] = bu_strdup((char *)argv[i]);
    av[ac] = (char *)0;

    /* check for displayed objects */
    ret = ged_who(gedp, 2, (const char **)who_av);
    if (ret == GED_OK && strlen(bu_vls_addr(gedp->ged_result_str)) == 0)
	aflag = 1;
    bu_vls_trunc(gedp->ged_result_str, 0);

    while ((ret = (*func)(gedp, ac, (const char **)av)) & GED_MORE) {
	int ac_more;
	const char **avmp;
	const char **av_more = NULL;

	/* save result string */
	bu_vls_substr(&result_copy, gedp->ged_result_str, 0, bu_vls_strlen(gedp->ged_result_str));
	bu_vls_trunc(gedp->ged_result_str, 0);

	ret = ged_who(gedp, 1, (const char **)who_av);
	if (ret == GED_OK && strlen(bu_vls_addr(gedp->ged_result_str)) == 0)
	    aflag = 1;

	bu_vls_trunc(gedp->ged_result_str, 0);

	for (size_t i = 0; i < BU_PTBL_LEN(&current_top->to_gedp->ged_views); i++) {
	    gdvp = (struct bview *)BU_PTBL_GET(&current_top->to_gedp->ged_views, i);
	    if (to_is_viewable(gdvp)) {
		gedp->ged_gvp->gv_x_samples = dm_get_width((struct dm *)gdvp->dmp);
		gedp->ged_gvp->gv_y_samples = dm_get_height((struct dm *)gdvp->dmp);
	    }
	}

	if (first && aflag) {
	    first = 0;
	    to_autoview_all_views(current_top);
	} else {
	    to_refresh_all_views(current_top);
	}
	/* restore result string */
	bu_vls_substr(gedp->ged_result_str, &result_copy, 0, bu_vls_strlen(&result_copy));
	bu_vls_free(&result_copy);

	if (0 < bu_vls_strlen(&tgd->go_more_args_callback)) {
	    bu_vls_trunc(&callback_cmd, 0);
	    bu_vls_printf(&callback_cmd, "%s [string range {%s} 0 end]",
		    bu_vls_addr(&tgd->go_more_args_callback),
		    bu_vls_addr(gedp->ged_result_str));

	    if (Tcl_Eval(current_top->to_interp, bu_vls_addr(&callback_cmd)) != TCL_OK) {
		bu_vls_trunc(gedp->ged_result_str, 0);
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		Tcl_ResetResult(current_top->to_interp);
		return GED_ERROR;
	    }

	    bu_vls_trunc(&temp, 0);
	    bu_vls_printf(&temp, "%s", Tcl_GetStringResult(current_top->to_interp));
	    Tcl_ResetResult(current_top->to_interp);
	} else {
	    bu_log("\r%s", bu_vls_addr(gedp->ged_result_str));
	    bu_vls_trunc(&temp, 0);
	    if (bu_vls_gets(&temp, stdin) < 0) {
		break;
	    }
	}

	if (Tcl_SplitList(current_top->to_interp, bu_vls_addr(&temp), &ac_more, &av_more) != TCL_OK) {
	    continue;
	}

	if (ac_more < 1) {
	    /* space has still been allocated */
	    Tcl_Free((char *)av_more);

	    continue;
	}

	/* skip first element if empty */
	avmp = av_more;
	if (*avmp[0] == '\0') {
	    --ac_more;
	    ++avmp;
	}

	/* ignore last element if empty */
	if (*avmp[ac_more-1] == '\0')
	    --ac_more;

	/* copy additional args */
	for (int i = 0; i < ac_more; ++i)
	    av[ac++] = bu_strdup(avmp[i]);
	av[ac+1] = (char *)0;

	Tcl_Free((char *)av_more);
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&current_top->to_gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&current_top->to_gedp->ged_views, i);
	if (to_is_viewable(gdvp)) {
	    gedp->ged_gvp->gv_x_samples = dm_get_width((struct dm *)gdvp->dmp);
	    gedp->ged_gvp->gv_y_samples = dm_get_height((struct dm *)gdvp->dmp);
	}
    }
    to_refresh_all_views(current_top);

    bu_vls_free(&callback_cmd);
    bu_vls_free(&temp);

    bu_vls_printf(gedp->ged_result_str, "BUILT_BY_MORE_ARGS");
    for (int i = 0; i < ac; ++i) {
	bu_vls_printf(gedp->ged_result_str, "%s ", av[i]);
	bu_free((void *)av[i], "to_more_args_func");
    }
    return GED_OK;
}

HIDDEN int
to_fontsize(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int fontsize;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2 || 3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get the font size */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_fontsize((struct dm *)gdvp->dmp));
	return GED_OK;
    }

    /* set background color */
    if (bu_sscanf(argv[2], "%d", &fontsize) != 1)
	goto bad_fontsize;

    if (DM_VALID_FONT_SIZE(fontsize) || fontsize == 0) {
	dm_set_fontsize((struct dm *)gdvp->dmp, fontsize);
	(void)dm_configure_win((struct dm *)gdvp->dmp, 1);
	to_refresh_view(gdvp);
	return GED_OK;
    }

bad_fontsize:
    bu_vls_printf(gedp->ged_result_str, "%s: %s", argv[0], argv[2]);
    return GED_ERROR;
}


HIDDEN int
to_fit_png_image(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    icv_image_t *img;
    size_t i_w, i_n;
    size_t o_w_requested, o_n_requested;
    size_t o_w_used, o_n_used;
    size_t x_offset, y_offset;
    fastf_t ar_w;
    fastf_t sf;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6 ||
	    bu_sscanf(argv[2], "%zu", &o_w_requested) != 1 ||
	    bu_sscanf(argv[3], "%zu", &o_n_requested) != 1 ||
	    bu_sscanf(argv[4], "%lf", &sf) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    img = icv_read(argv[1], BU_MIME_IMAGE_PNG, 0, 0);
    if (img == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s does not exist or is not a png file", argv[0], argv[1]);
	return GED_ERROR;
    }

    i_w = img->width;
    i_n = img->height;
    ar_w = o_w_requested / (fastf_t)i_w;

    o_w_used = (size_t)(i_w * ar_w * sf);
    o_n_used = (size_t)(i_n * ar_w * sf);

    if (icv_resize(img, ICV_RESIZE_BINTERP, o_w_used, o_n_used, 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: icv_resize failed", argv[0]);
	icv_destroy(img);
	return GED_ERROR;
    }

    if (NEAR_EQUAL(sf, 1.0, BN_TOL_DIST)) {
	fastf_t ar_n = o_n_requested / (fastf_t)i_n;

	if (ar_w > ar_n && !NEAR_EQUAL(ar_w, ar_n, BN_TOL_DIST)) {
	    /* need to crop final image height so that we keep the center of the image */
	    size_t x_orig = 0;
	    size_t y_orig = (o_n_used - o_n_requested) * 0.5;

	    if (icv_rect(img, x_orig, y_orig, o_w_requested, o_n_requested) < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: icv_rect failed", argv[0]);
		icv_destroy(img);
		return GED_ERROR;
	    }

	    x_offset = 0;
	    y_offset = 0;
	} else {
	    /* user needs to offset final image in the window */
	    x_offset = 0;
	    y_offset = (size_t)((o_n_requested - o_n_used) * 0.5);
	}
    } else {
	if (sf > 1.0) {
	    size_t x_orig = (o_w_used - o_w_requested) * 0.5;
	    size_t y_orig = (o_n_used - o_n_requested) * 0.5;

	    if (icv_rect(img, x_orig, y_orig, o_w_requested, o_n_requested) < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: icv_rect failed", argv[0]);
		icv_destroy(img);
		return GED_ERROR;
	    }

	    x_offset = 0;
	    y_offset = 0;
	} else {
	    /* user needs to offset final image in the window */
	    x_offset = (size_t)((o_w_requested - o_w_used) * 0.5);
	    y_offset = (size_t)((o_n_requested - o_n_used) * 0.5);
	}
    }

    bu_vls_printf(gedp->ged_result_str, "%zu %zu %zu %zu", img->width, img->height, x_offset, y_offset);

    /* icv_write should return < 0 for errors but doesn't */
    if (icv_write(img, argv[5], BU_MIME_IMAGE_PNG) == 0) {
	icv_destroy(img);
	return GED_ERROR;
    }

    icv_destroy(img);
    return GED_OK;
}


HIDDEN int
to_init_view_bindings(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    to_init_default_bindings(gdvp);

    return GED_OK;
}


HIDDEN int
to_delete_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    return GED_OK;
}


HIDDEN int
to_handle_expose(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int count;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3 ||
	    bu_sscanf(argv[2], "%d", &count) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s, argv[2] - %s", argv[0], usage, argv[2]);
	return GED_ERROR;
    }

    /* There are more expose events to come so ignore this one */
    if (count)
	return GED_OK;

    return to_handle_refresh(gedp, argv[1]);
}


HIDDEN int
to_hide_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int hide_view;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* return the hide view setting */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gv_hidden);
	return GED_OK;
    }

    if (bu_sscanf(argv[2], "%d", &hide_view) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad value - %s, should be 0 or 1", argv[1], argv[2]);
	return GED_ERROR;
    }

    return GED_OK;
}


struct redraw_edited_path_data {
    struct ged *gedp;
    struct bview *gdvp;
    int *need_refresh;
};


HIDDEN void
redraw_edited_paths(struct bu_hash_tbl *t, void *udata)
{
    const char *av[5] = {0};
    uint8_t *key;
    char *draw_path;
    struct redraw_edited_path_data *data;
    int ret, dmode = 0;
    struct bu_vls path_dmode = BU_VLS_INIT_ZERO;
    struct path_edit_params *params;
    struct bu_hash_entry *entry = bu_hash_next(t, NULL);

    while (entry) {

	bu_hash_key(entry, &key, NULL);
	draw_path = (char *)key;

	data = (struct redraw_edited_path_data *)udata;

	params = (struct path_edit_params *)bu_hash_value(entry, NULL);
	if (params->edit_mode == TCLCAD_OTRANSLATE_MODE) {
	    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;
	    struct bu_vls tran_x_vls = BU_VLS_INIT_ZERO;
	    struct bu_vls tran_y_vls = BU_VLS_INIT_ZERO;
	    struct bu_vls tran_z_vls = BU_VLS_INIT_ZERO;
	    mat_t dvec;

	    MAT_DELTAS_GET(dvec, params->edit_mat);
	    VSCALE(dvec, dvec, data->gedp->ged_wdbp->dbip->dbi_base2local);

	    bu_vls_printf(&tran_x_vls, "%lf", dvec[X]);
	    bu_vls_printf(&tran_y_vls, "%lf", dvec[Y]);
	    bu_vls_printf(&tran_z_vls, "%lf", dvec[Z]);
	    MAT_IDN(params->edit_mat);

	    struct tclcad_view_data *tvd = (struct tclcad_view_data *)data->gdvp->u_data;
	    bu_vls_printf(&tcl_cmd, "%s otranslate %s %s %s",
		    bu_vls_addr(&tvd->gdv_edit_motion_delta_callback),
		    bu_vls_addr(&tran_x_vls), bu_vls_addr(&tran_y_vls),
		    bu_vls_addr(&tran_z_vls));
	    tvd->gdv_edit_motion_delta_callback_cnt++;
	    if (tvd->gdv_edit_motion_delta_callback_cnt > 1) {
		bu_log("Warning - recursive gdv_edit_motion_delta_callback call\n");
	    }

	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&tcl_cmd));
	    tvd->gdv_edit_motion_delta_callback_cnt++;
	    bu_vls_free(&tcl_cmd);
	    bu_vls_free(&tran_x_vls);
	    bu_vls_free(&tran_y_vls);
	    bu_vls_free(&tran_z_vls);
	}

	av[0] = "how";
	av[1] = draw_path;
	av[2] = NULL;
	ret = ged_how(data->gedp, 2, av);
	if (ret == GED_OK) {
	    ret = bu_sscanf(bu_vls_cstr(data->gedp->ged_result_str), "%d", &dmode);
	}
	if (dmode == 5) {
	    bu_vls_printf(&path_dmode, "-h");
	} else {
	    bu_vls_printf(&path_dmode, "-m%d", dmode);
	}

	av[0] = "erase";
	ret = ged_erase(data->gedp, 2, av);

	if (ret == GED_OK) {
	    av[0] = "draw";
	    av[1] = "-R";
	    av[2] = bu_vls_cstr(&path_dmode);
	    av[3] = draw_path;
	    av[4] = NULL;
	    ged_draw(data->gedp, 4, av);
	}

	*data->need_refresh = 1;

	entry = bu_hash_next(t, entry);
    }
}


HIDDEN int
to_idle_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int mode, need_refresh = 0;
    struct redraw_edited_path_data data;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    mode = gdvp->gv_mode;

    if (gdvp->gv_adaptive_plot &&
	    gdvp->gv_redraw_on_zoom &&
	    mode == TCLCAD_SCALE_MODE)
    {
	const char *av[] = {"redraw", NULL};

	ged_redraw(gedp, 1, (const char **)av);

	need_refresh = 1;
    }

    if (mode != TCLCAD_POLY_CONTOUR_MODE ||
	    gdvp->gv_data_polygons.gdps_cflag == 0)
    {
	struct bu_vls bindings = BU_VLS_INIT_ZERO;

	struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
	if (pathname && bu_vls_strlen(pathname)) {
	    bu_vls_printf(&bindings, "bind %s <Motion> {}", bu_vls_cstr(pathname));
	    Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
	}
	bu_vls_free(&bindings);
    }

    if (gdvp->gv_grid.snap &&
	    (mode == TCLCAD_TRANSLATE_MODE ||
	     mode == TCLCAD_CONSTRAINED_TRANSLATE_MODE))
    {
	const char *av[3];

	gedp->ged_gvp = gdvp;
	av[0] = "grid";
	av[1] = "vsnap";
	av[2] = NULL;
	ged_grid(gedp, 2, (const char **)av);

	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    tvd->gdv_callback_cnt++;
	    if (tvd->gdv_callback_cnt > 1) {
		bu_log("Warning - recursive gvd_callback call\n");
	    }
	    tclcad_eval_noresult(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback), 0, NULL);
	    tvd->gdv_callback_cnt--;
	}

	need_refresh = 1;
    }

    /* redraw any edited paths, then clear them from our table */
    Tcl_Eval(current_top->to_interp, "SetWaitCursor $::ArcherCore::application");
    data.gedp = gedp;
    data.gdvp = gdvp;
    data.need_refresh = &need_refresh;

    redraw_edited_paths(tgd->go_edited_paths, &data);

    free_path_edit_params(tgd->go_edited_paths);
    bu_hash_destroy(tgd->go_edited_paths);
    tgd->go_edited_paths = bu_hash_create(0);
    Tcl_Eval(current_top->to_interp, "SetNormalCursor $::ArcherCore::application");

    if (need_refresh) {
	to_refresh_all_views(current_top);
    }

    gdvp->gv_mode = TCLCAD_IDLE_MODE;
    gdvp->gv_sdata_polygons.gdps_cflag = 0;

    return GED_OK;
}


HIDDEN int
to_light(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int light;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get light flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_light((struct dm *)gdvp->dmp));
	return GED_OK;
    }

    /* set light flag */
    if (bu_sscanf(argv[2], "%d", &light) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (light < 0)
	light = 0;

    (void)dm_make_current((struct dm *)gdvp->dmp);
    (void)dm_set_light((struct dm *)gdvp->dmp, light);
    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_list_views(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    struct bview *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&current_top->to_gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&current_top->to_gedp->ged_views, i);
	bu_vls_printf(gedp->ged_result_str, "%s ", bu_vls_addr(&gdvp->gv_name));
    }

    return GED_OK;
}


HIDDEN int
to_local2base(struct ged *gedp,
	int UNUSED(argc),
	const char *UNUSED(argv[]),
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "%lf", current_top->to_gedp->ged_wdbp->dbip->dbi_local2base);

    return GED_OK;
}


HIDDEN int
to_lod(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    struct bview *gdvp;

    for (size_t i = 0; i < BU_PTBL_LEN(&current_top->to_gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&current_top->to_gedp->ged_views, i);
	gedp->ged_gvp = gdvp;
	(*func)(gedp, argc, (const char **)argv);
    }

    return GED_OK;
}


HIDDEN int
to_make(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ret;
    const char *av[3];

    ret = ged_make(gedp, argc, argv);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[argc-2];
	av[2] = (char *)0;
	to_autoview_func(gedp, 2, (const char **)av, ged_draw, (char *)0, TO_UNLIMITED);
    }

    return ret;
}


HIDDEN int
to_mirror(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ret;
    const char *av[3];

    ret = ged_mirror(gedp, argc, argv);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[argc-1];
	av[2] = (char *)0;
	to_autoview_func(gedp, 2, (const char **)av, ged_draw, (char *)0, TO_UNLIMITED);
    }

    return ret;
}


HIDDEN int
to_edit_motion_delta_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int i;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get the callback string */
    if (argc == 2) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&tvd->gdv_edit_motion_delta_callback));

	return GED_OK;
    }

    /* set the callback string */
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    bu_vls_trunc(&tvd->gdv_edit_motion_delta_callback, 0);
    for (i = 2; i < argc; ++i)
	bu_vls_printf(&tvd->gdv_edit_motion_delta_callback, "%s ", argv[i]);

    return GED_OK;
}


HIDDEN int
to_more_args_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int i;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the callback string */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&tgd->go_more_args_callback));

	return GED_OK;
    }

    /* set the callback string */
    bu_vls_trunc(&tgd->go_more_args_callback, 0);
    for (i = 1; i < argc; ++i)
	bu_vls_printf(&tgd->go_more_args_callback, "%s ", argv[i]);

    return GED_OK;
}


HIDDEN int
to_view_cmd(ClientData UNUSED(clientData),
	Tcl_Interp *UNUSED(interp),
	int UNUSED(argc),
	char **UNUSED(argv))
{
    return TCL_OK;
}


HIDDEN int
to_move_arb_edge_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_MOVE_ARB_EDGE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_arb_edge %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_move_arb_face_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_MOVE_ARB_FACE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_arb_face %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_bot_move_pnt(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ret;

    if ((ret = ged_bot_move_pnt(gedp, argc, argv)) == GED_OK) {
	const char *av[3];
	int i;

	if (argc == 4)
	    i = 1;
	else
	    i = 2;

	av[0] = "draw";
	av[1] = (char *)argv[i];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);

	return GED_OK;
    }

    return ret;
}


HIDDEN int
to_bot_move_pnts(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ret;

    if ((ret = ged_bot_move_pnts(gedp, argc, argv)) == GED_OK) {
	const char *av[3];

	av[0] = "draw";
	av[1] = (char *)argv[1];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);

	return GED_OK;
    }

    return ret;
}


HIDDEN int
to_bot_move_pnt_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_MOVE_BOT_POINT_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_bot_move_pnt -r %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_bot_move_pnts_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int i;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	    bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_MOVE_BOT_POINTS_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_bot_move_pnts %s %%x %%y %s ",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[4]);
    }
    for (i = 5; i < argc; ++i)
	bu_vls_printf(&bindings, "%s ", argv[i]);
    bu_vls_printf(&bindings, "}");

    Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_metaball_move_pnt_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_MOVE_METABALL_POINT_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_metaball_move_pnt %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_pipe_move_pnt_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_MOVE_PIPE_POINT_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_pipe_move_pnt %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_move_pnt_common(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ret;

    if ((ret = (*func)(gedp, argc, argv)) == GED_OK) {
	const char *av[3];
	int i;

	if (argc == 4)
	    i = 1;
	else
	    i = 2;

	av[0] = "draw";
	av[1] = (char *)argv[i];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);

	return GED_OK;
    }

    return ret;
}


HIDDEN int
to_new_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bview *new_gdvp;
    HIDDEN const int name_index = 1;
    const char *type = NULL;
    struct bu_vls event_vls = BU_VLS_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[2], "nu"))
	type = argv[2];

    /* find display manager type */
    if (argv[2][0] == 'X' || argv[2][0] == 'x')
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "tk"))
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "ogl"))
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "osg"))
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "osgl"))
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "wgl"))
	type = argv[2];

    if (BU_STR_EQUAL(argv[2], "qt"))
	type = argv[2];

    if (!type) {
	bu_vls_printf(gedp->ged_result_str, "ERROR:  Requisite display manager is not available.\nBRL-CAD may need to be recompiled with support for:  %s\nRun 'fbhelp' for a list of available display managers.\n", argv[2]);
	return GED_ERROR;
    }

    BU_ALLOC(new_gdvp, struct bview);
    BU_GET(new_gdvp->callbacks, struct bu_ptbl);
    bu_ptbl_init(new_gdvp->callbacks, 8, "bview callbacks");

    {
	int i;
	int arg_start = 3;
	int newargs = 0;
	int ac;
	const char **av;

	ac = argc + newargs;
	av = (const char **)bu_malloc(sizeof(char *) * (ac+1), "to_new_view: av");
	av[0] = argv[0];

	/*
	 * Stuff name into argument list.
	 */
	av[1] = "-n";
	av[2] = argv[name_index];

	/* copy the rest */
	for (i = arg_start; i < argc; ++i)
	    av[i+newargs] = argv[i];
	av[i+newargs] = (const char *)NULL;

	new_gdvp->dmp = (void *)dm_open((void *)current_top->to_interp, type, ac, av);
	if ((struct dm *)new_gdvp->dmp == DM_NULL) {
	    bu_ptbl_free(new_gdvp->callbacks);
	    BU_PUT(new_gdvp->callbacks, struct bu_ptbl);
	    bu_free((void *)new_gdvp, "ged_view");
	    bu_free((void *)new_gdvp, "bview");
	    bu_free((void *)av, "to_new_view: av");

	    bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", argv[1]);
	    return GED_ERROR;
	}

	bu_free((void *)av, "to_new_view: av");

    }

    bu_vls_init(&new_gdvp->gv_name);

    struct tclcad_view_data *tvd;
    BU_GET(tvd, struct tclcad_view_data);
    bu_vls_init(&tvd->gdv_edit_motion_delta_callback);
    tvd->gdv_edit_motion_delta_callback_cnt = 0;
    bu_vls_init(&tvd->gdv_callback);
    tvd->gdv_callback_cnt = 0;
    tvd->gedp = current_top->to_gedp;
    new_gdvp->u_data = (void *)tvd;

    bu_vls_printf(&new_gdvp->gv_name, "%s", argv[name_index]);
    ged_view_init(new_gdvp);
    bu_ptbl_ins(&current_top->to_gedp->ged_views, (long *)new_gdvp);

    new_gdvp->gv_point_scale = 1.0;
    new_gdvp->gv_curve_scale = 1.0;

    tvd->gdv_fbs.fbs_listener.fbsl_fbsp = &tvd->gdv_fbs;
    tvd->gdv_fbs.fbs_listener.fbsl_fd = -1;
    tvd->gdv_fbs.fbs_listener.fbsl_port = -1;
    tvd->gdv_fbs.fbs_fbp = FB_NULL;
    tvd->gdv_fbs.fbs_callback = (void (*)(void *clientData))to_fbs_callback;
    tvd->gdv_fbs.fbs_clientData = new_gdvp;
    tvd->gdv_fbs.fbs_interp = current_top->to_interp;

    /* open the framebuffer */
    to_open_fbs(new_gdvp, current_top->to_interp);

    /* Set default bindings */
    to_init_default_bindings(new_gdvp);

    struct bu_vls *pathname = dm_get_pathname((struct dm *)new_gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&event_vls, "event generate %s <Configure>; %s autoview %s",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&new_gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&event_vls));
    }
    bu_vls_free(&event_vls);

    if (pathname && bu_vls_strlen(pathname)) {
	(void)Tcl_CreateCommand(current_top->to_interp,
		bu_vls_cstr(pathname),
		(Tcl_CmdProc *)to_view_cmd,
		(ClientData)new_gdvp,
		NULL);
    }

    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&new_gdvp->gv_name));
    return GED_OK;
}


HIDDEN int
to_orotate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	    bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_OROTATE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_orotate %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_oscale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	    bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_OSCALE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_oscale %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


/* to_model_delta_mode */
HIDDEN int
to_otranslate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	    bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_OTRANSLATE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_otranslate %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_paint_rect_area(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }
    if (argc < 2 || 7 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 0);

    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    (void)fb_refresh(tvd->gdv_fbs.fbs_fbp, gdvp->gv_rect.pos[X], gdvp->gv_rect.pos[Y],
	    gdvp->gv_rect.dim[X], gdvp->gv_rect.dim[Y]);

    (void)dm_set_depth_mask((struct dm *)gdvp->dmp, 1);

    return GED_OK;
}


#ifdef HAVE_GL_GL_H
HIDDEN int
to_pix(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    FILE *fp = NULL;
    unsigned char *scanline;
    unsigned char *pixels;
    static int bytes_per_pixel = 3;
    int i = 0;
    int width = 0;
    int height = 0;
    int make_ret = 0;
    int bytes_per_line;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (!BU_STR_EQUIV(dm_get_type((struct dm *)gdvp->dmp), "wgl") && !BU_STR_EQUIV(dm_get_type((struct dm *)gdvp->dmp), "ogl")) {
	bu_vls_printf(gedp->ged_result_str, "%s: not yet supported for this display manager (i.e. must be OpenGL based)", argv[0]);
	return GED_OK;
    }

    bytes_per_line = dm_get_width((struct dm *)gdvp->dmp) * bytes_per_pixel;

    if ((fp = fopen(argv[2], "wb")) == NULL) {
	bu_vls_printf(gedp->ged_result_str,
		"%s: cannot open \"%s\" for writing.",
		argv[0], argv[2]);
	return GED_ERROR;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    height = dm_get_height((struct dm *)gdvp->dmp);

    make_ret = dm_make_current((struct dm *)gdvp->dmp);
    if (!make_ret) {
	bu_vls_printf(gedp->ged_result_str, "%s: Couldn't make context current\n", argv[0]);
	fclose(fp);
	return GED_ERROR;
    }

    pixels = (unsigned char *)bu_calloc(width * height, bytes_per_pixel, "pixels");
    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    for (i = 0; i < height; ++i) {
	scanline = (unsigned char *)(pixels + (i*bytes_per_line));

	if (fwrite((char *)scanline, bytes_per_line, 1, fp) != 1) {
	    perror("fwrite");
	    break;
	}
    }

    bu_free(pixels, "pixels");
    fclose(fp);

    return GED_OK;
}


HIDDEN int
to_png(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    png_structp png_p;
    png_infop info_p;
    FILE *fp = NULL;
    unsigned char **rows = NULL;
    unsigned char *pixels;
    static int bytes_per_pixel = 3;
    static int bits_per_channel = 8;  /* bits per color channel */
    int i = 0;
    int width = 0;
    int height = 0;
    int make_ret = 0;
    int bytes_per_line;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (!BU_STR_EQUIV(dm_get_type((struct dm *)gdvp->dmp), "wgl") && !BU_STR_EQUIV(dm_get_type((struct dm *)gdvp->dmp), "ogl")) {
	bu_vls_printf(gedp->ged_result_str, "%s: not yet supported for this display manager (i.e. must be OpenGL based)", argv[0]);
	return GED_OK;
    }

    bytes_per_line = dm_get_width((struct dm *)gdvp->dmp) * bytes_per_pixel;

    if ((fp = fopen(argv[2], "wb")) == NULL) {
	bu_vls_printf(gedp->ged_result_str,
		"%s: cannot open \"%s\" for writing.",
		argv[0], argv[2]);
	return GED_ERROR;
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_vls_printf(gedp->ged_result_str, "%s: could not create PNG write structure.", argv[0]);
	fclose(fp);
	return GED_ERROR;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_vls_printf(gedp->ged_result_str, "%s: could not create PNG info structure.", argv[0]);
	fclose(fp);
	return GED_ERROR;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    height = dm_get_height((struct dm *)gdvp->dmp);
    make_ret = dm_make_current((struct dm *)gdvp->dmp);
    if (make_ret) {
	bu_vls_printf(gedp->ged_result_str, "%s: Couldn't make context current\n", argv[0]);
	fclose(fp);
	return GED_ERROR;
    }

    pixels = (unsigned char *)bu_calloc(width * height, bytes_per_pixel, "pixels");
    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    rows = (unsigned char **)bu_calloc(height, sizeof(unsigned char *), "rows");

    for (i = 0; i < height; ++i)
	rows[i] = (unsigned char *)(pixels + ((height-i-1)*bytes_per_line));

    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p, width, height, bits_per_channel,
	    PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
	    PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, 0.5);
    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);

    bu_free(rows, "rows");
    bu_free(pixels, "pixels");
    fclose(fp);

    return GED_OK;
}
#endif


HIDDEN int
to_rect_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int ac;
    int x, y;
    const char *av[5];
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct bu_vls x_vls = BU_VLS_INIT_ZERO;
    struct bu_vls y_vls = BU_VLS_INIT_ZERO;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    gedp->ged_gvp = gdvp;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	    bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = dm_get_height((struct dm *)gdvp->dmp) - y;
    gdvp->gv_mode = TCLCAD_RECTANGLE_MODE;

    ac = 4;
    av[0] = "rect";
    av[1] = "dim";
    av[2] = "0";
    av[3] = "0";
    av[4] = (char *)0;
    (void)ged_rect(gedp, ac, (const char **)av);

    bu_vls_printf(&x_vls, "%d", (int)gdvp->gv_prevMouseX);
    bu_vls_printf(&y_vls, "%d", (int)gdvp->gv_prevMouseY);
    av[1] = "pos";
    av[2] = bu_vls_addr(&x_vls);
    av[3] = bu_vls_addr(&y_vls);
    (void)ged_rect(gedp, ac, (const char **)av);
    bu_vls_free(&x_vls);
    bu_vls_free(&y_vls);

    ac = 3;
    av[0] = "rect";
    av[1] = "draw";
    av[2] = "1";
    av[3] = (char *)0;
    (void)ged_rect(gedp, ac, (const char **)av);

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_rect %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_rotate_arb_face_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 7) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[5], "%lf", &x) != 1 ||
	    bu_sscanf(argv[6], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_ROTATE_ARB_FACE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_rotate_arb_face %s %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3],
		argv[4]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_rotate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	    bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_ROTATE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_rot %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


/**
 * Called when the named proc created by rt_gettrees() is destroyed.
 */
HIDDEN void
to_deleteProc_rt(ClientData clientData)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;

    RT_AP_CHECK(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    rt_free_rti(rtip);
    ap->a_rt_i = (struct rt_i *)NULL;

    bu_free((void *)ap, "struct application");
}


HIDDEN int
to_rt_end_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int i;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the callback string */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&tgd->go_rt_end_callback));

	return GED_OK;
    }

    /* set the callback string */
    bu_vls_trunc(&tgd->go_rt_end_callback, 0);
    for (i = 1; i < argc; ++i)
	bu_vls_printf(&tgd->go_rt_end_callback, "%s ", argv[i]);

    return GED_OK;
}


/**
 * @brief
 * Given an instance of a database and the name of some treetops,
 * create a named "ray-tracing" object (proc) which will respond to
 * subsequent operations.  Returns new proc name as result.
 *
 * @par Example:
 *	.inmem rt_gettrees .rt all.g light.r
 */
int
to_rt_gettrees(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct application *ap;
    char *newprocname;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    newprocname = (char *)argv[1];

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand(current_top->to_interp, newprocname);

    /* Skip past newprocname when calling to_rt_gettrees_application */
    if ((ap=to_rt_gettrees_application(gedp, argc-2, argv+2)) == RT_APPLICATION_NULL) {
	return GED_ERROR;
    }

    /* Instantiate the proc, with clientData of wdb */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(current_top->to_interp, newprocname, tclcad_rt,
	    (ClientData)ap, to_deleteProc_rt);

    /* Return new function name as result */
    bu_vls_printf(gedp->ged_result_str, "%s", newprocname);

    return GED_OK;
}


HIDDEN int
to_protate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_PROTATE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_protate %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_pscale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_PSCALE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_pscale %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_ptranslate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	    bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_PTRANSLATE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_ptranslate %s %s %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name),
		argv[2],
		argv[3]);
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_data_scale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	    bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_DATA_SCALE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_data_scale %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_scale_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	    bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_SCALE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_scale %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_screen2model(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    point_t view;
    point_t model;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	    bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x((struct dm *)gdvp->dmp, x);
    y = screen_to_view_y((struct dm *)gdvp->dmp, y);
    VSET(view, x, y, 0.0);
    MAT4X3PNT(model, gdvp->gv_view2model, view);

    bu_vls_printf(gedp->ged_result_str, "%lf %lf %lf", V3ARGS(model));

    return GED_OK;
}


HIDDEN int
to_screen2view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    point_t view;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	    bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x((struct dm *)gdvp->dmp, x);
    y = screen_to_view_y((struct dm *)gdvp->dmp, y);
    VSET(view, x, y, 0.0);

    bu_vls_printf(gedp->ged_result_str, "%lf %lf %lf", V3ARGS(view));

    return GED_OK;
}


HIDDEN int
to_set_coord(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Get coord */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%c", gdvp->gv_coord);
	return GED_OK;
    }

    /* Set coord */
    if ((argv[2][0] != 'm' && argv[2][0] != 'v') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "set_coord: bad value - %s\n", argv[2]);
	return GED_ERROR;
    }

    gdvp->gv_coord = argv[2][0];

    return GED_OK;
}


HIDDEN int
to_snap_view(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    fastf_t fvx, fvy;

    /* must be double for scanf */
    double vx, vy;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &vx) != 1 ||
	    bu_sscanf(argv[3], "%lf", &vy) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }
    /* convert double to fastf_t */
    fvx = vx;
    fvy = vy;

    gedp->ged_gvp = gdvp;
    if (!gedp->ged_gvp->gv_snap_lines && !gedp->ged_gvp->gv_grid.snap) {
	bu_vls_printf(gedp->ged_result_str, "%lf %lf", fvx, fvy);
	return GED_OK;
    }

    int snapped = 0;
    if (gedp->ged_gvp->gv_snap_lines) {
	snapped = ged_snap_to_lines(gedp, &fvx, &fvy);
    }
    if (!snapped && gedp->ged_gvp->gv_grid.snap) {
	ged_snap_to_grid(gedp, &fvx, &fvy);
    }

    bu_vls_printf(gedp->ged_result_str, "%lf %lf", fvx, fvy);

    return GED_OK;
}


HIDDEN int
to_bot_edge_split(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ret;

    if ((ret = ged_bot_edge_split(gedp, argc, argv)) == GED_OK) {
	const char *av[3];
	struct bu_vls save_result;

	bu_vls_init(&save_result);

	av[0] = "draw";
	av[1] = (char *)argv[1];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);

	bu_vls_trunc(gedp->ged_result_str, 0);
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&save_result));
	bu_vls_free(&save_result);

	return GED_OK;
    }

    return ret;
}


HIDDEN int
to_bot_face_split(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *UNUSED(usage),
	int UNUSED(maxargs))
{
    int ret;

    if ((ret = ged_bot_face_split(gedp, argc, argv)) == GED_OK) {
	const char *av[3];
	struct bu_vls save_result;

	bu_vls_init(&save_result);

	av[0] = "draw";
	av[1] = (char *)argv[1];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);

	bu_vls_trunc(gedp->ged_result_str, 0);
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&save_result));
	bu_vls_free(&save_result);

	return GED_OK;
    }

    return ret;
}


HIDDEN int
to_translate_mode(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	    bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_TRANSLATE_MODE;

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_trans %s %%x %%y}",
		bu_vls_cstr(pathname),
		bu_vls_cstr(&current_top->to_gedp->go_name),
		bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_transparency(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int transparency;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get transparency flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_transparency((struct dm *)gdvp->dmp));
	return GED_OK;
    }

    /* set transparency flag */
    if (argc == 3) {
	if (bu_sscanf(argv[2], "%d", &transparency) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: invalid transparency value - %s", argv[0], argv[2]);
	    return GED_ERROR;
	}

	(void)dm_make_current((struct dm *)gdvp->dmp);
	(void)dm_set_transparency((struct dm *)gdvp->dmp, transparency);
	return GED_OK;
    }

    return GED_OK;
}


HIDDEN int
to_view_callback(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int i;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get the callback string */
    if (argc == 2) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&tvd->gdv_callback));

	return GED_OK;
    }

    /* set the callback string */
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    bu_vls_trunc(&tvd->gdv_callback, 0);
    for (i = 2; i < argc; ++i)
	bu_vls_printf(&tvd->gdv_callback, "%s ", argv[i]);

    return GED_OK;
}


HIDDEN int
to_view_win_size(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int width, height;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc > 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d %d", dm_get_width((struct dm *)gdvp->dmp), dm_get_height((struct dm *)gdvp->dmp));
	return GED_OK;
    }

    if (argc == 3) {
	if (bu_sscanf(argv[2], "%d", &width) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad size %s", argv[0], argv[2]);
	    return GED_ERROR;
	}

	height = width;
    } else {
	if (bu_sscanf(argv[2], "%d", &width) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad width %s", argv[0], argv[2]);
	    return GED_ERROR;
	}

	if (bu_sscanf(argv[3], "%d", &height) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad height %s", argv[0], argv[3]);
	    return GED_ERROR;
	}
    }

    dm_geometry_request((struct dm *)gdvp->dmp, width, height);

    return GED_OK;
}


HIDDEN int
to_view2screen(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int width, height;
    fastf_t x, y;
    fastf_t aspect;

    /* must be double for scanf */
    double view[ELEMENTS_PER_POINT];

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf %lf", &view[X], &view[Y]) != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    height = dm_get_height((struct dm *)gdvp->dmp);
    aspect = (fastf_t)width/(fastf_t)height;
    x = (view[X] + 1.0) * 0.5 * (fastf_t)width;
    y = (view[Y] * aspect - 1.0) * -0.5 * (fastf_t)height;

    bu_vls_printf(gedp->ged_result_str, "%d %d", (int)x, (int)y);

    return GED_OK;
}


HIDDEN int
to_vmake(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    {
	int ret;
	const char *av[8];
	char center[512];
	char scale[128];

	sprintf(center, "%f %f %f",
		-gdvp->gv_center[MDX],
		-gdvp->gv_center[MDY],
		-gdvp->gv_center[MDZ]);
	sprintf(scale, "%f", gdvp->gv_scale * 2.0);

	av[0] = (char *)argv[0];
	av[1] = "-o";
	av[2] = center;
	av[3] = "-s";
	av[4] = scale;
	av[5] = (char *)argv[2];
	av[6] = (char *)argv[3];
	av[7] = (char *)0;

	ret = ged_make(gedp, 7, (const char **)av);

	if (ret == GED_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    to_autoview_func(gedp, 2, (const char **)av, ged_draw, (char *)0, TO_UNLIMITED);
	}

	return ret;
    }
}


HIDDEN int
to_vslew(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int ret, width, height;
    int ac;
    const char *av[3];
    fastf_t xpos2, ypos2;
    fastf_t sf;
    struct bu_vls slew_vec = BU_VLS_INIT_ZERO;

    /* must be double for scanf */
    double xpos1, ypos1;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &xpos1) != 1 ||
	    bu_sscanf(argv[3], "%lf", &ypos1) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    width = dm_get_width((struct dm *)gdvp->dmp);
    xpos2 = 0.5 * (fastf_t)width;
    height = dm_get_height((struct dm *)gdvp->dmp);
    ypos2 = 0.5 * (fastf_t)height;
    sf = 2.0 / width;

    bu_vls_printf(&slew_vec, "%lf %lf", (xpos1 - xpos2) * sf, (ypos2 - ypos1) * sf);

    gedp->ged_gvp = gdvp;
    ac = 2;
    av[0] = (char *)argv[0];
    av[1] = bu_vls_addr(&slew_vec);
    av[2] = (char *)0;

    ret = ged_slew(gedp, ac, (const char **)av);
    bu_vls_free(&slew_vec);

    if (ret == GED_OK) {
	if (gdvp->gv_grid.snap) {

	    gedp->ged_gvp = gdvp;
	    av[0] = "grid";
	    av[1] = "vsnap";
	    av[2] = NULL;
	    ged_grid(gedp, 2, (const char **)av);
	}

	if (gedp->ged_gvp->gv_snap_lines) {
	    ged_view_center_linesnap(gedp);
	}

	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	if (0 < bu_vls_strlen(&tvd->gdv_callback)) {
	    tvd->gdv_callback_cnt++;
	    if (tvd->gdv_callback_cnt > 1) {
		bu_log("Warning - recursive gvd_callback call\n");
	    }
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&tvd->gdv_callback));
	    tvd->gdv_callback_cnt--;
	}

	to_refresh_view(gdvp);
    }

    return ret;
}


HIDDEN int
to_zbuffer(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int zbuffer;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get zbuffer flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_zbuffer((struct dm *)gdvp->dmp));
	return GED_OK;
    }

    /* set zbuffer flag */
    if (bu_sscanf(argv[2], "%d", &zbuffer) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (zbuffer < 0)
	zbuffer = 0;
    else if (1 < zbuffer)
	zbuffer = 1;

    (void)dm_make_current((struct dm *)gdvp->dmp);
    (void)dm_set_zbuffer((struct dm *)gdvp->dmp, zbuffer);
    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_zclip(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr UNUSED(func),
	const char *usage,
	int UNUSED(maxargs))
{
    int zclip;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    struct bview *gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get zclip flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gv_zclip);
	return GED_OK;
    }

    /* set zclip flag */
    if (bu_sscanf(argv[2], "%d", &zclip) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (zclip < 0)
	zclip = 0;
    else if (1 < zclip)
	zclip = 1;

    gdvp->gv_zclip = zclip;
    dm_set_zclip((struct dm *)gdvp->dmp, zclip);
    to_refresh_view(gdvp);

    return GED_OK;
}


/*************************** Local Utility Functions ***************************/

HIDDEN void
to_create_vlist_callback_solid(struct solid *sp)
{
    struct bview *gdvp;
    int first = 1;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;


    for (size_t i = 0; i < BU_PTBL_LEN(&current_top->to_gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&current_top->to_gedp->ged_views, i);
	if (tgd->go_dlist_on && to_is_viewable(gdvp)) {

	    (void)dm_make_current((struct dm *)gdvp->dmp);

	    if (first) {
		sp->s_dlist = dm_gen_dlists((struct dm *)gdvp->dmp, 1);
		first = 0;
	    }

	    (void)dm_begin_dlist((struct dm *)gdvp->dmp, sp->s_dlist);

	    if (sp->s_iflag == UP)
		(void)dm_set_fg((struct dm *)gdvp->dmp, 255, 255, 255, 0, sp->s_transparency);
	    else
		(void)dm_set_fg((struct dm *)gdvp->dmp,
			(unsigned char)sp->s_color[0],
			(unsigned char)sp->s_color[1],
			(unsigned char)sp->s_color[2], 0, sp->s_transparency);

	    if (sp->s_hiddenLine) {
		(void)dm_draw_vlist_hidden_line((struct dm *)gdvp->dmp, (struct bn_vlist *)&sp->s_vlist);
	    } else {
		(void)dm_draw_vlist((struct dm *)gdvp->dmp, (struct bn_vlist *)&sp->s_vlist);
	    }

	    (void)dm_end_dlist((struct dm *)gdvp->dmp);
	}
    }
}


HIDDEN void
to_create_vlist_callback(struct display_list *gdlp)
{
    struct solid *sp;
    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	to_create_vlist_callback_solid(sp);
    }
}


HIDDEN void
to_destroy_vlist_callback(unsigned int dlist, int range)
{
    struct bview *gdvp;
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;

    for (size_t i = 0; i < BU_PTBL_LEN(&current_top->to_gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&current_top->to_gedp->ged_views, i);
	if (tgd->go_dlist_on && to_is_viewable(gdvp)) {
	    (void)dm_make_current((struct dm *)gdvp->dmp);
	    (void)dm_free_dlists((struct dm *)gdvp->dmp, dlist, range);
	}
    }
}


HIDDEN void
to_rt_end_callback_internal(int aborted)
{
    struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
    if (0 < bu_vls_strlen(&tgd->go_rt_end_callback)) {
	struct bu_vls callback_cmd = BU_VLS_INIT_ZERO;
	tgd->go_rt_end_callback_cnt++;
	if (tgd->go_rt_end_callback_cnt > 1) {
	    bu_log("Warning - recursive go_rt_end_callback call\n");
	}
	bu_vls_printf(&callback_cmd, "%s %d",
		bu_vls_addr(&tgd->go_rt_end_callback), aborted);
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&callback_cmd));
	tgd->go_rt_end_callback_cnt--;
    }
}


HIDDEN void
to_output_handler(struct ged *gedp, char *line)
{
    const char *script;

    if (gedp->ged_output_script != (char *)0)
	script = gedp->ged_output_script;
    else
	script = "puts";

    tclcad_eval_noresult(current_top->to_interp, script, 1, (const char **)&line);
}


int
go_run_tclscript(Tcl_Interp *interp,
	const char *tclscript,
	struct bu_vls *result_str)
{
    int ret;

    /* initialize result */
    bu_vls_trunc(result_str, 0);

    ret = Tcl_Eval(interp, tclscript);
    bu_vls_printf(result_str, "%s", Tcl_GetStringResult(interp));
    Tcl_ResetResult(interp);

    return ret;
}


struct application *
to_rt_gettrees_application(struct ged *gedp,
	int argc,
	const char *argv[])
{
    struct rt_i *rtip;
    struct application *ap;
    static struct resource resp = RT_RESOURCE_INIT_ZERO;

    if (argc < 1) {
	return RT_APPLICATION_NULL;
    }

    rtip = rt_new_rti(gedp->ged_wdbp->dbip);

    while (0 < argc && argv[0][0] == '-') {
	if (BU_STR_EQUAL(argv[0], "-i")) {
	    rtip->rti_dont_instance = 1;
	    argc--;
	    argv++;
	    continue;
	}
	if (BU_STR_EQUAL(argv[0], "-u")) {
	    rtip->useair = 1;
	    argc--;
	    argv++;
	    continue;
	}
	break;
    }

    if (rt_gettrees(rtip, argc, (const char **)&argv[0], 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_gettrees() returned error");
	rt_free_rti(rtip);
	return RT_APPLICATION_NULL;
    }

    /* Establish defaults for this rt_i */
    rtip->rti_hasty_prep = 1;	/* Tcl isn't going to fire many rays */

    /*
     * In case of multiple instances of the library, make sure that
     * each instance has a separate resource structure, because the
     * bit vector lengths depend on # of solids.  And the "overwrite"
     * sequence in Tcl is to create the new proc before running the
     * Tcl_CmdDeleteProc on the old one, which in this case would
     * trash rt_uniresource.  Once on the rti_resources list,
     * rt_clean() will clean 'em up.
     */
    rt_init_resource(&resp, 0, rtip);
    BU_ASSERT(BU_PTBL_GET(&rtip->rti_resources, 0) != NULL);

    BU_ALLOC(ap, struct application);
    RT_APPLICATION_INIT(ap);
    ap->a_magic = RT_AP_MAGIC;
    ap->a_resource = &resp;
    ap->a_rt_i = rtip;
    ap->a_purpose = "Conquest!";

    rt_ck(rtip);

    return ap;
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
