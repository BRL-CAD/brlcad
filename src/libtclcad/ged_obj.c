/*                       G E D _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2009 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file ged_obj.c
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
#include "bio.h"

#include <zlib.h>
#include <png.h>

#include "tcl.h"

#include "bu.h"
#include "bn.h"
#include "cmd.h"
#include "vmath.h"
#include "db.h"
#include "rtgeom.h"
#include "wdb.h"
#include "mater.h"
#include "tclcad.h"

#include "solid.h"
#include "dm.h"
#include "dg.h"

#ifdef DM_X
#  ifdef WITH_TK
#    include "tk.h"
#endif
#  include <X11/Xutil.h>
#  include "dm_xvars.h"
#  include "dm-X.h"
#endif /* DM_X */

#ifdef DM_TK
#  ifdef WITH_TK
#    include "tk.h"
#  endif
#  include "dm_xvars.h"
#  include "dm-tk.h"
#endif /* DM_TK */

#ifdef DM_OGL
#  include "dm_xvars.h"
#  include "dm-ogl.h"
#endif /* DM_OGL */

#ifdef DM_WGL
#  include <tkwinport.h>
#  include "dm_xvars.h"
#  include "dm-wgl.h"
#endif /* DM_WGL */


HIDDEN int go_open_tcl(ClientData clientData,
		       Tcl_Interp *interp,
		       int argc,
		       const char **argv);
HIDDEN int go_autoview(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr func,
		       const char *usage,
		       int maxargs);
HIDDEN int go_axes(struct ged *gedp,
		   struct ged_dm_view *gdvp,
		   struct ged_axes_state *gasp,
		   int argc,
		   const char *argv[],
		   const char *usage,
		   int dflag);
HIDDEN int go_base2local(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int go_bg(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr func,
		 const char *usage,
		 int maxargs);
HIDDEN int go_blast(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs);
HIDDEN int go_bounds(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *usage,
		     int maxargs);
HIDDEN int go_configure(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_constrain_rmode(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int go_constrain_tmode(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int go_copy(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr func,
		   const char *usage,
		   int maxargs);
HIDDEN int go_data_axes(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_init_view_bindings(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *usage,
				 int maxargs);
HIDDEN int go_delete_view(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_faceplate(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_idle_mode(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_light(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs);
HIDDEN int go_list_views(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int go_listen(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *usage,
		     int maxargs);
HIDDEN int go_local2base(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int go_make(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr func,
		   const char *usage,
		   int maxargs);
HIDDEN int go_mirror(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *usage,
		     int maxargs);
HIDDEN int go_model_axes(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int go_more_args_callback(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *usage,
				 int maxargs);
HIDDEN int go_mouse_constrain_rot(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int go_mouse_constrain_trans(struct ged *gedp,
				    int argc,
				    const char *argv[],
				    ged_func_ptr func,
				    const char *usage,
				    int maxargs);
HIDDEN int go_mouse_move_arb_edge(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int go_mouse_move_arb_face(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int go_mouse_orotate(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int go_mouse_oscale(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int go_mouse_otranslate(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int go_mouse_translate(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int go_mouse_ray(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_mouse_rot(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_mouse_rotate_arb_face(struct ged *gedp,
				    int argc,
				    const char *argv[],
				    ged_func_ptr func,
				    const char *usage,
				    int maxargs);
HIDDEN int go_mouse_scale(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_mouse_protate(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int go_mouse_pscale(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int go_mouse_ptranslate(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int go_mouse_trans(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_move_arb_edge_mode(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *usage,
				 int maxargs);
HIDDEN int go_move_arb_face_mode(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *usage,
				 int maxargs);
HIDDEN int go_new_view(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr func,
		       const char *usage,
		       int maxargs);
HIDDEN int go_orotate_mode(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int go_oscale_mode(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_otranslate_mode(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int go_paint_rect_area(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
#if defined(DM_OGL) || defined(DM_WGL)
HIDDEN int go_png(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *usage,
		  int maxargs);
#endif

HIDDEN int go_prim_label(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int go_refresh(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs);
HIDDEN int go_refresh_all(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_refresh_on(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int go_rotate_arb_face_mode(struct ged *gedp,
				   int argc,
				   const char *argv[],
				   ged_func_ptr func,
				   const char *usage,
				   int maxargs);
HIDDEN int go_rotate_mode(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_rt_gettrees(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_protate_mode(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int go_pscale_mode(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_ptranslate_mode(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int go_scale_mode(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int go_screen2model(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int go_screen2view(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_set_coord(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_set_fb_mode(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int go_translate_mode(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int go_transparency(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int go_view_axes(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int go_view_win_size(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int go_vmake(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs);
HIDDEN int go_vslew(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs);
HIDDEN int go_zbuffer(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs);
HIDDEN int go_zclip(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs);

/* Wrapper Functions */
HIDDEN int go_autoview_func(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int go_edit_redraw_func(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int go_more_args_func(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int go_pass_through_func(struct ged *gedp,
				int argc,
				const char *argv[],
				ged_func_ptr func,
				const char *usage,
				int maxargs);
HIDDEN int go_pass_through_and_refresh_func(struct ged *gedp,
					    int argc,
					    const char *argv[],
					    ged_func_ptr func,
					    const char *usage,
					    int maxargs);
HIDDEN int go_view_func(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);

/* Utility Functions */
HIDDEN void go_drawSolid(struct dm *dmp, struct solid *sp);
HIDDEN int go_drawDList(struct dm *dmp, struct bu_list *hsp);

HIDDEN int go_close_fbs(struct ged_dm_view *gdvp);
HIDDEN void go_fbs_callback();
HIDDEN int go_open_fbs(struct ged_dm_view *gdvp, Tcl_Interp *interp);

HIDDEN void go_refresh_view(struct ged_dm_view *gdvp);
HIDDEN void go_refresh_handler(void *clientdata);
HIDDEN void go_refresh_all_views(struct ged_obj *gop);
HIDDEN void go_autoview_view(struct ged_dm_view *gdvp);
HIDDEN void go_autoview_all_views(struct ged_obj *gop);

HIDDEN void go_output_handler(struct ged *gedp, char *line);

typedef int (*go_wrapper_func_ptr)(struct ged *, int, const char *[], ged_func_ptr, const char *, int);
#define GO_WRAPPER_FUNC_PTR_NULL (go_wrapper_func_ptr)0

#define GO_MAX_RT_ARGS 64

static struct ged_obj HeadGedObj;
static struct ged_obj *go_current_gop = GED_OBJ_NULL;

#define GO_MAX_RT_ARGS 64

struct go_cmdtab {
    char *go_name;
    char *go_usage;
    int go_maxargs;
    go_wrapper_func_ptr go_wrapper_func;
    ged_func_ptr go_func;
};

static struct go_cmdtab go_cmds[] = {
    {"3ptarb",	(char *)0, MAXARGS, go_more_args_func, ged_3ptarb},
    {"adc",	"args", 7, go_view_func, ged_adc},
    {"adjust",	(char *)0, MAXARGS, go_pass_through_func, ged_adjust},
    {"ae2dir",	(char *)0, MAXARGS, go_pass_through_func, ged_ae2dir},
    {"aet",	"[[-i] az el [tw]]", 6, go_view_func, ged_aet},
    {"analyze",	(char *)0, MAXARGS, go_pass_through_func, ged_analyze},
    {"arb",	(char *)0, MAXARGS, go_pass_through_func, ged_arb},
    {"arced",	(char *)0, MAXARGS, go_pass_through_func, ged_arced},
    {"arot",	"x y z angle", 6, go_view_func, ged_arot},
    {"attr",	(char *)0, MAXARGS, go_pass_through_func, ged_attr},
    {"autoview",	"vname", MAXARGS, go_autoview, GED_FUNC_PTR_NULL},
    {"bb",	(char *)0, MAXARGS, go_pass_through_func, ged_bb},
    {"bev",	(char *)0, MAXARGS, go_pass_through_func, ged_bev},
    {"base2local",	(char *)0, MAXARGS, go_base2local, GED_FUNC_PTR_NULL},
    {"bg",	"[r g b]", MAXARGS, go_bg, GED_FUNC_PTR_NULL},
    {"blast",	(char *)0, MAXARGS, go_blast, GED_FUNC_PTR_NULL},
    {"bo",	(char *)0, MAXARGS, go_pass_through_func, ged_bo},
    {"bot_condense",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_condense},
    {"bot_decimate",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_decimate},
    {"bot_dump",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_dump},
    {"bot_face_fuse",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_face_fuse},
    {"bot_face_sort",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_face_sort},
    {"bot_flip",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_flip},
    {"bot_merge",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_merge},
    {"bot_smooth",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_smooth},
    {"bot_split",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_split},
    {"bot_sync",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_sync},
    {"bot_vertex_fuse",	(char *)0, MAXARGS, go_pass_through_func, ged_bot_vertex_fuse},
    {"bounds",	"[\"minX maxX minY maxY minZ maxZ\"]", MAXARGS, go_bounds, GED_FUNC_PTR_NULL},
    {"c",	(char *)0, MAXARGS, go_pass_through_func, ged_comb_std},
    {"cat",	(char *)0, MAXARGS, go_pass_through_func, ged_cat},
    {"center",	"[x y z]", 5, go_view_func, ged_center},
    {"clear",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_zap},
    {"clone",	(char *)0, MAXARGS, go_pass_through_func, ged_clone},
    {"color",	(char *)0, MAXARGS, go_pass_through_func, ged_color},
    {"comb",	(char *)0, MAXARGS, go_pass_through_func, ged_comb},
    {"comb_color",	(char *)0, MAXARGS, go_pass_through_func, ged_comb_color},
    {"configure",	"vname", MAXARGS, go_configure, GED_FUNC_PTR_NULL},
    {"constrain_rmode",	"x|y|z x y", MAXARGS, go_constrain_rmode, GED_FUNC_PTR_NULL},
    {"constrain_tmode",	"x|y|z x y", MAXARGS, go_constrain_tmode, GED_FUNC_PTR_NULL},
    {"copyeval",	(char *)0, MAXARGS, go_pass_through_func, ged_copyeval},
    {"copymat",	(char *)0, MAXARGS, go_pass_through_func, ged_copymat},
    {"cp",	"[-f] from to", MAXARGS, go_copy, GED_FUNC_PTR_NULL},
    {"cpi",	(char *)0, MAXARGS, go_pass_through_func, ged_cpi},
    {"d",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_erase},
    {"dall",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_erase_all},
    {"data_axes",	"???", MAXARGS, go_data_axes, GED_FUNC_PTR_NULL},
    {"dbconcat",	(char *)0, MAXARGS, go_pass_through_func, ged_concat},
    {"dbfind",	(char *)0, MAXARGS, go_pass_through_func, ged_find},
    {"dbip",	(char *)0, MAXARGS, go_pass_through_func, ged_dbip},
    {"decompose",	(char *)0, MAXARGS, go_pass_through_func, ged_decompose},
    {"delay",	(char *)0, MAXARGS, go_pass_through_func, ged_delay},
    {"delete_view",	"vname", MAXARGS, go_delete_view, GED_FUNC_PTR_NULL},
    {"dir2ae",	(char *)0, MAXARGS, go_pass_through_func, ged_dir2ae},
    {"draw",	(char *)0, MAXARGS, go_autoview_func, ged_draw},
    {"dump",	(char *)0, MAXARGS, go_pass_through_func, ged_dump},
    {"dup",	(char *)0, MAXARGS, go_pass_through_func, ged_dup},
    {"E",	(char *)0, MAXARGS, go_autoview_func, ged_E},
    {"e",	(char *)0, MAXARGS, go_autoview_func, ged_draw},
    {"eac",	(char *)0, MAXARGS, go_autoview_func, ged_eac},
    {"echo",	(char *)0, MAXARGS, go_pass_through_func, ged_echo},
    {"edcodes",	(char *)0, MAXARGS, go_pass_through_func, ged_edcodes},
    {"edcolor",	(char *)0, MAXARGS, go_pass_through_func, ged_edcolor},
    {"edcomb",	(char *)0, MAXARGS, go_pass_through_func, ged_edcomb},
    {"edmater",	(char *)0, MAXARGS, go_pass_through_func, ged_edmater},
    {"erase",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_erase},
    {"erase_all",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_erase_all},
    {"ev",	(char *)0, MAXARGS, go_autoview_func, ged_ev},
    {"expand",	(char *)0, MAXARGS, go_pass_through_func, ged_expand},
    {"eye",	"[x y z]", 5, go_view_func, ged_eye},
    {"eye_pos",	"[x y z]", 5, go_view_func, ged_eye_pos},
    {"faceplate",	"center_dot|prim_labels|view_params|view_scale color|draw [val(s)]", MAXARGS, go_faceplate, GED_FUNC_PTR_NULL},
    {"facetize",	(char *)0, MAXARGS, go_pass_through_func, ged_facetize},
    {"form",	(char *)0, MAXARGS, go_pass_through_func, ged_form},
    {"fracture",	(char *)0, MAXARGS, go_pass_through_func, ged_fracture},
    {"g",	(char *)0, MAXARGS, go_pass_through_func, ged_group},
    {"get",	(char *)0, MAXARGS, go_pass_through_func, ged_get},
    {"get_autoview",	(char *)0, MAXARGS, go_pass_through_func, ged_get_autoview},
    {"get_comb",	(char *)0, MAXARGS, go_pass_through_func, ged_get_comb},
    {"get_eyemodel",	"vname", 2, go_view_func, ged_get_eyemodel},
    {"get_type",	(char *)0, MAXARGS, go_pass_through_func, ged_get_type},
    {"glob",	(char *)0, MAXARGS, go_pass_through_func, ged_glob},
    {"gqa",	(char *)0, MAXARGS, go_pass_through_func, ged_gqa},
    {"grid",	"args", 6, go_view_func, ged_grid},
    {"hide",	(char *)0, MAXARGS, go_pass_through_func, ged_hide},
    {"how",	(char *)0, MAXARGS, go_pass_through_func, ged_how},
    {"human",	(char *)0, MAXARGS, go_pass_through_func, ged_human},
    {"i",	(char *)0, MAXARGS, go_pass_through_func, ged_instance},
    {"idents",	(char *)0, MAXARGS, go_pass_through_func, ged_tables},
    {"idle_mode",	"vname", MAXARGS, go_idle_mode, GED_FUNC_PTR_NULL},
    {"illum",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_illum},
    {"importFg4Section",	(char *)0, MAXARGS, go_pass_through_func, ged_importFg4Section},
    {"in",	(char *)0, MAXARGS, go_more_args_func, ged_in},
    {"init_view_bindings",	"vname", MAXARGS, go_init_view_bindings, GED_FUNC_PTR_NULL},
    {"inside",	(char *)0, MAXARGS, go_more_args_func, ged_inside},
    {"isize",	"vname", 2, go_view_func, ged_isize},
    {"item",	(char *)0, MAXARGS, go_pass_through_func, ged_item},
    {"keep",	(char *)0, MAXARGS, go_pass_through_func, ged_keep},
    {"keypoint",	"[x y z]", 5, go_view_func, ged_keypoint},
    {"kill",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_kill},
    {"killall",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_killall},
    {"killrefs",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_killrefs},
    {"killtree",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_killtree},
    {"l",	(char *)0, MAXARGS, go_pass_through_func, ged_list},
    {"light",	"[0|1]", MAXARGS, go_light, GED_FUNC_PTR_NULL},
    {"list_views",	(char *)0, MAXARGS, go_list_views, GED_FUNC_PTR_NULL},
    {"listen",	"[port]", MAXARGS, go_listen, GED_FUNC_PTR_NULL},
    {"listeval",	(char *)0, MAXARGS, go_pass_through_func, ged_pathsum},
    {"loadview",	"filename", 3, go_view_func, ged_loadview},
    {"local2base",	(char *)0, MAXARGS, go_local2base, GED_FUNC_PTR_NULL},
    {"log",	(char *)0, MAXARGS, go_pass_through_func, ged_log},
    {"lookat",	"x y z", 5, go_view_func, ged_lookat},
    {"ls",	(char *)0, MAXARGS, go_pass_through_func, ged_ls},
    {"lt",	(char *)0, MAXARGS, go_pass_through_func, ged_lt},
    {"m2v_point",	"x y z", 5, go_view_func, ged_m2v_point},
    {"make",	(char *)0, MAXARGS, go_make, GED_FUNC_PTR_NULL},
    {"make_bb",	(char *)0, MAXARGS, go_pass_through_func, ged_make_bb},
    {"make_name",	(char *)0, MAXARGS, go_pass_through_func, ged_make_name},
    {"make_pnts",	(char *)0, MAXARGS, go_more_args_func, ged_make_pnts},
    {"match",	(char *)0, MAXARGS, go_pass_through_func, ged_match},
    {"mater",	(char *)0, MAXARGS, go_pass_through_func, ged_mater},
    {"mirror",	(char *)0, MAXARGS, go_mirror, GED_FUNC_PTR_NULL},
    {"model2view",	"vname", 2, go_view_func, ged_model2view},
    {"model_axes",	"???", MAXARGS, go_model_axes, GED_FUNC_PTR_NULL},
    {"more_args_callback",	"set/get the \"more args\" callback", MAXARGS, go_more_args_callback, GED_FUNC_PTR_NULL},
    {"move_arb_edge",	(char *)0, MAXARGS, go_pass_through_func, ged_move_arb_edge},
    {"move_arb_edge_mode",	"obj edge x y", MAXARGS, go_move_arb_edge_mode, GED_FUNC_PTR_NULL},
    {"move_arb_face",	(char *)0, MAXARGS, go_pass_through_func, ged_move_arb_face},
    {"move_arb_face_mode",	"obj face x y", MAXARGS, go_move_arb_face_mode, GED_FUNC_PTR_NULL},
    {"mouse_constrain_rot",	"coord x y", MAXARGS, go_mouse_constrain_rot, GED_FUNC_PTR_NULL},
    {"mouse_constrain_trans",	"coord x y", MAXARGS, go_mouse_constrain_trans, GED_FUNC_PTR_NULL},
    {"mouse_move_arb_edge",	"obj edge x y", MAXARGS, go_mouse_move_arb_edge, GED_FUNC_PTR_NULL},
    {"mouse_move_arb_face",	"obj face x y", MAXARGS, go_mouse_move_arb_face, GED_FUNC_PTR_NULL},
    {"mouse_orotate",	"obj x y", MAXARGS, go_mouse_orotate, GED_FUNC_PTR_NULL},
    {"mouse_oscale",	"obj x y", MAXARGS, go_mouse_oscale, GED_FUNC_PTR_NULL},
    {"mouse_otranslate",	"obj x y", MAXARGS, go_mouse_otranslate, GED_FUNC_PTR_NULL},
    {"mouse_ray",	"x y", MAXARGS, go_mouse_ray, GED_FUNC_PTR_NULL},
    {"mouse_rot",	"x y", MAXARGS, go_mouse_rot, GED_FUNC_PTR_NULL},
    {"mouse_rotate_arb_face",	"obj face v x y", MAXARGS, go_mouse_rotate_arb_face, GED_FUNC_PTR_NULL},
    {"mouse_scale",	"x y", MAXARGS, go_mouse_scale, GED_FUNC_PTR_NULL},
    {"mouse_protate",	"obj attribute x y", MAXARGS, go_mouse_protate, GED_FUNC_PTR_NULL},
    {"mouse_pscale",	"obj attribute x y", MAXARGS, go_mouse_pscale, GED_FUNC_PTR_NULL},
    {"mouse_ptranslate",	"obj attribute x y", MAXARGS, go_mouse_ptranslate, GED_FUNC_PTR_NULL},
    {"mouse_trans",	"x y", MAXARGS, go_mouse_trans, GED_FUNC_PTR_NULL},
    {"mv",	(char *)0, MAXARGS, go_pass_through_func, ged_move},
    {"mvall",	(char *)0, MAXARGS, go_pass_through_func, ged_move_all},
    {"new_view",	"type [args]", MAXARGS, go_new_view, GED_FUNC_PTR_NULL},
    {"nirt",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_nirt},
    {"nmg_collapse",	(char *)0, MAXARGS, go_pass_through_func, ged_nmg_collapse},
    {"nmg_simplify",	(char *)0, MAXARGS, go_pass_through_func, ged_nmg_simplify},
    {"ocenter",	(char *)0, MAXARGS, go_pass_through_func, ged_ocenter},
    {"open",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_reopen},
    {"orient",	"quat", 6, go_view_func, ged_orient},
    {"orotate",	(char *)0, MAXARGS, go_pass_through_func, ged_orotate},
    {"orotate_mode",	"obj x y", MAXARGS, go_orotate_mode, GED_FUNC_PTR_NULL},
    {"oscale",	(char *)0, MAXARGS, go_pass_through_func, ged_oscale},
    {"oscale_mode",	"obj x y", MAXARGS, go_oscale_mode, GED_FUNC_PTR_NULL},
    {"otranslate",	(char *)0, MAXARGS, go_pass_through_func, ged_otranslate},
    {"otranslate_mode",	"obj x y", MAXARGS, go_otranslate_mode, GED_FUNC_PTR_NULL},
    {"overlay",	(char *)0, MAXARGS, go_autoview_func, ged_overlay},
    {"paint_rect_area",	"vname", MAXARGS, go_paint_rect_area, GED_FUNC_PTR_NULL},
    {"pathlist",	(char *)0, MAXARGS, go_pass_through_func, ged_pathlist},
    {"paths",	(char *)0, MAXARGS, go_pass_through_func, ged_pathsum},
    {"perspective",	"[angle]", 3, go_view_func, ged_perspective},
    {"plot",	"[options] file.pl", 16, go_view_func, ged_plot},
    {"pmat",	"[mat]", 3, go_view_func, ged_pmat},
    {"pmodel2view",	"vname", 2, go_view_func, ged_pmodel2view},
#if defined(DM_OGL) || defined(DM_WGL)
    {"png",	"file", MAXARGS, go_png, GED_FUNC_PTR_NULL},
#endif
    {"pngwf",	"[options] file.png", 16, go_view_func, ged_png},
    {"pov",	"center quat scale eye_pos perspective", 7, go_view_func, ged_pmat},
    {"prcolor",	(char *)0, MAXARGS, go_pass_through_func, ged_prcolor},
    {"prefix",	(char *)0, MAXARGS, go_pass_through_func, ged_prefix},
    {"preview",	"[options] script", MAXARGS, go_view_func, ged_preview},
    {"prim_label",	"[prim_1 prim_2 ... prim_N]", MAXARGS, go_prim_label, GED_FUNC_PTR_NULL},
    {"ps",	"[options] file.ps", 16, go_view_func, ged_ps},
    {"protate",	(char *)0, MAXARGS, go_pass_through_func, ged_protate},
    {"protate_mode",	"obj attribute x y", MAXARGS, go_protate_mode, GED_FUNC_PTR_NULL},
    {"pscale",	(char *)0, MAXARGS, go_pass_through_func, ged_pscale},
    {"pscale_mode",	"obj attribute x y", MAXARGS, go_pscale_mode, GED_FUNC_PTR_NULL},
    {"ptranslate",	(char *)0, MAXARGS, go_pass_through_func, ged_ptranslate},
    {"ptranslate_mode",	"obj attribute x y", MAXARGS, go_ptranslate_mode, GED_FUNC_PTR_NULL},
    {"push",	(char *)0, MAXARGS, go_pass_through_func, ged_push},
    {"put",	(char *)0, MAXARGS, go_pass_through_func, ged_put},
    {"put_comb",	(char *)0, MAXARGS, go_pass_through_func, ged_put_comb},
    {"putmat",	(char *)0, MAXARGS, go_pass_through_func, ged_putmat},
    {"qray",	(char *)0, MAXARGS, go_pass_through_func, ged_qray},
    {"quat",	"a b c d", 6, go_view_func, ged_quat},
    {"qvrot",	"x y z angle", 6, go_view_func, ged_qvrot},
    {"r",	(char *)0, MAXARGS, go_pass_through_func, ged_region},
    {"rcodes",	(char *)0, MAXARGS, go_pass_through_func, ged_rcodes},
    {"rect",	"args", 6, go_view_func, ged_rect},
    {"red",	(char *)0, MAXARGS, go_pass_through_func, ged_red},
    {"refresh",	"vname", MAXARGS, go_refresh, GED_FUNC_PTR_NULL},
    {"refresh_all",	(char *)0, MAXARGS, go_refresh_all, GED_FUNC_PTR_NULL},
    {"refresh_on",	"[0|1]", MAXARGS, go_refresh_on, GED_FUNC_PTR_NULL},
    {"regdef",	(char *)0, MAXARGS, go_pass_through_func, ged_regdef},
    {"regions",	(char *)0, MAXARGS, go_pass_through_func, ged_tables},
    {"report",	(char *)0, MAXARGS, go_pass_through_func, ged_report},
    {"rfarb",	(char *)0, MAXARGS, go_pass_through_func, ged_rfarb},
    {"rm",	(char *)0, MAXARGS, go_pass_through_func, ged_remove},
    {"rmap",	(char *)0, MAXARGS, go_pass_through_func, ged_rmap},
    {"rmat",	"[mat]", 3, go_view_func, ged_rmat},
    {"rmater",	(char *)0, MAXARGS, go_pass_through_func, ged_rmater},
    {"rot",	"[-m|-v] x y z", 6, go_view_func, ged_rot},
    {"rot_about",	"[e|k|m|v]", 3, go_view_func, ged_rotate_about},
    {"rot_point",	"x y z", 5, go_view_func, ged_rot_point},
    {"rotate_arb_face",	(char *)0, MAXARGS, go_pass_through_func, ged_rotate_arb_face},
    {"rotate_arb_face_mode",	"obj face v x y", MAXARGS, go_rotate_arb_face_mode, GED_FUNC_PTR_NULL},
    {"rotate_mode",	"x y", MAXARGS, go_rotate_mode, GED_FUNC_PTR_NULL},
    {"rrt",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_rrt},
    {"rt",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_rt},
    {"rt_gettrees",	"[-i] [-u] pname object", MAXARGS, go_rt_gettrees, GED_FUNC_PTR_NULL},
    {"rtabort",	(char *)0, GO_MAX_RT_ARGS, go_pass_through_func, ged_rtabort},
    {"rtarea",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_rt},
    {"rtcheck",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_rtcheck},
    {"rtedge",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_rt},
    {"rtweight",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_rt},
    {"savekey",	"filename", 3, go_view_func, ged_savekey},
    {"saveview",	"filename", 3, go_view_func, ged_saveview},
    {"sca",	"sf", 3, go_view_func, ged_scale},
    {"scale_mode",	"x y", MAXARGS, go_scale_mode, GED_FUNC_PTR_NULL},
    {"screen2model",	"x y", MAXARGS, go_screen2model, GED_FUNC_PTR_NULL},
    {"screen2view",	"x y", MAXARGS, go_screen2view, GED_FUNC_PTR_NULL},
    {"search",		(char *)0, MAXARGS, go_pass_through_func, ged_search},
    {"set_coord",	"[m|v]", MAXARGS, go_set_coord, GED_FUNC_PTR_NULL},
    {"set_fb_mode",	"[mode]", MAXARGS, go_set_fb_mode, GED_FUNC_PTR_NULL},
    {"set_output_script",	"[script]", MAXARGS, go_pass_through_func, ged_set_output_script},
    {"set_transparency",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_set_transparency},
    {"set_uplotOutputMode",	(char *)0, MAXARGS, go_pass_through_func, ged_set_uplotOutputMode},
    {"setview",	"x y z", 5, go_view_func, ged_setview},
    {"shaded_mode",	(char *)0, MAXARGS, go_pass_through_func, ged_shaded_mode},
    {"shader",	(char *)0, MAXARGS, go_pass_through_func, ged_shader},
    {"shells",	(char *)0, MAXARGS, go_pass_through_func, ged_shells},
    {"showmats",	(char *)0, MAXARGS, go_pass_through_func, ged_showmats},
    {"size",	"[size]", 3, go_view_func, ged_size},
    {"slew",	"x y [z]", 5, go_view_func, ged_slew},
    {"solids",	(char *)0, MAXARGS, go_pass_through_func, ged_tables},
    {"solids_on_ray",	(char *)0, MAXARGS, go_pass_through_func, ged_solids_on_ray},
    {"summary",	(char *)0, MAXARGS, go_pass_through_func, ged_summary},
    {"sync",	(char *)0, MAXARGS, go_pass_through_func, ged_sync},
    {"tire",	(char *)0, MAXARGS, go_pass_through_func, ged_tire},
    {"title",	(char *)0, MAXARGS, go_pass_through_func, ged_title},
    {"tol",	(char *)0, MAXARGS, go_pass_through_func, ged_tol},
    {"tops",	(char *)0, MAXARGS, go_pass_through_func, ged_tops},
    {"tra",	"[-m|-v] x y z", 6, go_view_func, ged_tra},
    {"track",	(char *)0, MAXARGS, go_pass_through_func, ged_track},
    {"translate_mode",	"x y", MAXARGS, go_translate_mode, GED_FUNC_PTR_NULL},
    {"transparency",	"[val]", MAXARGS, go_transparency, GED_FUNC_PTR_NULL},
    {"tree",	(char *)0, MAXARGS, go_pass_through_func, ged_tree},
    {"unhide",	(char *)0, MAXARGS, go_pass_through_func, ged_unhide},
    {"units",	(char *)0, MAXARGS, go_pass_through_func, ged_units},
    {"v2m_point",	"x y z", 5, go_view_func, ged_v2m_point},
    {"vdraw",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_vdraw},
    {"version",	(char *)0, MAXARGS, go_pass_through_func, ged_version},
    {"view",	"quat|ypr|aet|center|eye|size [args]", 3, go_view_func, ged_view},
    {"view_axes",	"???", MAXARGS, go_view_axes, GED_FUNC_PTR_NULL},
    {"view_win_size",	"[s] | [x y]", 4, go_view_win_size, GED_FUNC_PTR_NULL},
    {"view2model",	"vname", 2, go_view_func, ged_view2model},
    {"viewdir",	"[-i]", 3, go_view_func, ged_viewdir},
    {"vmake",	"pname ptype", MAXARGS, go_vmake, GED_FUNC_PTR_NULL},
    {"vnirt",	"[args]", GO_MAX_RT_ARGS, go_view_func, ged_vnirt},
    {"vslew",	"x y", MAXARGS, go_vslew, GED_FUNC_PTR_NULL},
    {"wcodes",	(char *)0, MAXARGS, go_pass_through_func, ged_wcodes},
    {"whatid",	(char *)0, MAXARGS, go_pass_through_func, ged_whatid},
    {"which_shader",	(char *)0, MAXARGS, go_pass_through_func, ged_which_shader},
    {"whichair",	(char *)0, MAXARGS, go_pass_through_func, ged_which},
    {"whichid",	(char *)0, MAXARGS, go_pass_through_func, ged_which},
    {"who",	(char *)0, MAXARGS, go_pass_through_func, ged_who},
    {"wmater",	(char *)0, MAXARGS, go_pass_through_func, ged_wmater},
    {"xpush",	(char *)0, MAXARGS, go_pass_through_func, ged_xpush},
    {"ypr",	"yaw pitch roll", 5, go_view_func, ged_ypr},
    {"zap",	(char *)0, MAXARGS, go_pass_through_and_refresh_func, ged_zap},
    {"zbuffer",	"[0|1]", MAXARGS, go_zbuffer, GED_FUNC_PTR_NULL},
    {"zclip",	"[0|1]", MAXARGS, go_zclip, GED_FUNC_PTR_NULL},
    {"zoom",	"sf", 3, go_view_func, ged_zoom},
    {(char *)0,	(char *)0, 0, GO_WRAPPER_FUNC_PTR_NULL, GED_FUNC_PTR_NULL}
};


/**
 * @brief create the Tcl command for go_open
 *
 */
int
Go_Init(Tcl_Interp *interp)
{
    /*XXX Use of brlcad_interp is temporary */
    brlcad_interp = interp;

    BU_LIST_INIT(&HeadGedObj.l);
    (void)Tcl_CreateCommand(interp, (const char *)"go_open", go_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

#if 1
    /*XXX Temporary */
    /* initialize database objects */
    Wdb_Init(interp);

    /* initialize drawable geometry objects */
    Dgo_Init(interp);

    /* initialize view objects */
    Vo_Init(interp);
#endif

    bu_semaphore_reinit(GED_SEM_LAST);

    return TCL_OK;
}

/**
 * G O _ C M D
 *
 * @brief
 * Generic interface for database commands.
 *
 * @par Usage:
 * procname cmd ?args?
 *
 * @return result of ged command.
 */
HIDDEN int
go_cmd(ClientData clientData,
       Tcl_Interp *interp,
       int argc,
       char **argv)
{
    register struct go_cmdtab *ctp;
    struct ged_obj *gop = (struct ged_obj *)clientData;
    Tcl_DString ds;
    int ret;

    Tcl_DStringInit(&ds);

    if (argc < 2) {
	Tcl_DStringAppend(&ds, "subcommand not specfied; must be one of: ", -1);
	for (ctp = go_cmds; ctp->go_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->go_name, -1);
	}
	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

    go_current_gop = gop;

    for (ctp = go_cmds; ctp->go_name != (char *)0; ctp++) {
	if (ctp->go_name[0] == argv[1][0] &&
	    !strcmp(ctp->go_name, argv[1])) {
	    ret = (*ctp->go_wrapper_func)(gop->go_gedp, argc-1, (const char **)argv+1, ctp->go_func, ctp->go_usage, ctp->go_maxargs);
	    break;
	}
    }

    /* Command not found. */
    if (ctp->go_name == (char *)0) {
	Tcl_DStringAppend(&ds, "unknown subcommand: ", -1);
	Tcl_DStringAppend(&ds, argv[1], -1);
	Tcl_DStringAppend(&ds, "; must be one of: ", -1);

	for (ctp = go_cmds; ctp->go_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->go_name, -1);
	}
	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

    Tcl_DStringAppend(&ds, bu_vls_addr(&gop->go_gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret & GED_ERROR)
	return TCL_ERROR;

    return TCL_OK;
}


/**
 * @brief
 * Called by Tcl when the object is destroyed.
 */
void
go_deleteProc(ClientData clientData)
{
    struct ged_obj *gop = (struct ged_obj *)clientData;
    struct ged_dm_view *gdvp;

    if (go_current_gop == gop)
	go_current_gop = GED_OBJ_NULL;

#if 0
    GED_CHECK_OBJ(gop);
#endif
    BU_LIST_DEQUEUE(&gop->l);
    bu_vls_free(&gop->go_name);
    ged_close(gop->go_gedp);
#if 1
    while (BU_LIST_WHILE(gdvp, ged_dm_view, &gop->go_head_views.l)) {
	BU_LIST_DEQUEUE(&(gdvp->l));
	bu_vls_free(&gdvp->gdv_name);
	DM_CLOSE(gdvp->gdv_dmp);
	bu_free((genptr_t)gdvp->gdv_view, "ged_view");

	go_close_fbs(gdvp);

	bu_free((genptr_t)gdvp, "ged_dm_view");
    }
#else
    for (i = 0; i < GED_OBJ_NUM_VIEWS; ++i)
	bu_free((genptr_t)gop->go_views[i], "struct ged_view");
    if (gop->go_dmp != DM_NULL)
	DM_CLOSE(gop->go_dmp);
#endif
    bu_free((genptr_t)gop, "struct ged_obj");
}

/**
 * @brief
 * Create a command named "oname" in "interp" using "gedp" as its state.
 *
 */
int
go_create_cmd(Tcl_Interp *interp,
	      struct ged_obj *gop,	/* pointer to object */
	      const char *oname)	/* object name */
{
    if (gop == GED_OBJ_NULL) {
	Tcl_AppendResult(interp, "go_create_cmd ", oname, " failed", NULL);
	return TCL_ERROR;
    }

    /* Instantiate the newprocname, with clientData of gop */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(interp, oname, (Tcl_CmdProc *)go_cmd,
			    (ClientData)gop, go_deleteProc);

    /* Return new function name as result */
    Tcl_AppendResult(interp, oname, (char *)NULL);

    return TCL_OK;
}


/**
 * G E D _ O P E N _ T C L
 *
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
 * set gop [go_open .inmem inmem $dbip]
 *@n	.inmem get box.s
 *@n	.inmem close
 *
 *@n go_open db file "bob.g"
 *@n db get white.r
 *@n db close
 */
HIDDEN int
go_open_tcl(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    const char **argv)
{
    struct ged_obj *gop;
    struct ged *gedp;

    if (argc == 1) {
	/* get list of database objects */
	for (BU_LIST_FOR(gop, ged_obj, &HeadGedObj.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&gop->go_name), " ", (char *)NULL);

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

    if (argc == 3 || strcmp(argv[2], "db") == 0) {
	if (argc == 3)
	    gedp = ged_open("filename", argv[2], 0); 
	else
	    gedp = ged_open("db", argv[3], 0); 
    } else
	gedp = ged_open(argv[2], argv[3], 0); 

    /* initialize ged_obj */
    BU_GETSTRUCT(gop, ged_obj);
    gop->go_gedp = gedp;
    gop->go_gedp->ged_output_handler = go_output_handler;
    gop->go_gedp->ged_refresh_handler = go_refresh_handler;
    bu_vls_init(&gop->go_name);
    bu_vls_strcpy(&gop->go_name, argv[1]);
    bu_vls_init(&gop->go_more_args_callback);
    BU_LIST_INIT(&gop->go_observers.l);
    gop->go_interp = interp;
    gop->go_refresh_on = 1;

    BU_LIST_INIT(&gop->go_head_views.l);

    /* append to list of ged_obj */
    BU_LIST_APPEND(&HeadGedObj.l, &gop->l);

    return go_create_cmd(interp, gop, argv[1]);
}


/*************************** Local Command Functions ***************************/
HIDDEN int
go_autoview(struct ged *gedp,
	    int argc,
	    const char *argv[],
	    ged_func_ptr func,
	    const char *usage,
	    int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    go_autoview_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_axes(struct ged *gedp,
	struct ged_dm_view *gdvp,
	struct ged_axes_state *gasp,
	int argc,
	const char *argv[],
	const char *usage,
	int dflag)
{

    if (strcmp(argv[2], "draw") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_draw);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->gas_draw = 1;
	    else
		gasp->gas_draw = 0;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "axes_size") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%lf", gasp->gas_axes_size);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    fastf_t size;

	    if (sscanf(argv[3], "%lf", &size) != 1)
		goto bad;

	    gasp->gas_axes_size = size;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "axes_pos") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%lf %lf %lf",
			  V3ARGS(gasp->gas_axes_pos));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    fastf_t x, y, z;

	    if (sscanf(argv[3], "%lf", &x) != 1 ||
		sscanf(argv[4], "%lf", &y) != 1 ||
		sscanf(argv[5], "%lf", &z) != 1)
		goto bad;

	    VSET(gasp->gas_axes_pos, x, y, z);

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "axes_color") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->gas_axes_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (sscanf(argv[3], "%d", &r) != 1 ||
		sscanf(argv[4], "%d", &g) != 1 ||
		sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->gas_axes_color, r, g, b);

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "label_color") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->gas_label_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (sscanf(argv[3], "%d", &r) != 1 ||
		sscanf(argv[4], "%d", &g) != 1 ||
		sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->gas_label_color, r, g, b);

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "line_width") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_line_width);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    gasp->gas_line_width = line_width;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "pos_only") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_pos_only);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->gas_pos_only = 1;
	    else
		gasp->gas_pos_only = 0;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "tick_color") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->gas_tick_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (sscanf(argv[3], "%d", &r) != 1 ||
		sscanf(argv[4], "%d", &g) != 1 ||
		sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->gas_tick_color, r, g, b);

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "tick_enabled") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_tick_enabled);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->gas_tick_enabled = 1;
	    else
		gasp->gas_tick_enabled = 0;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "tick_interval") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_tick_interval);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_interval;

	    if (sscanf(argv[3], "%d", &tick_interval) != 1)
		goto bad;

	    gasp->gas_tick_interval = tick_interval;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "tick_length") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_tick_length);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_length;

	    if (sscanf(argv[3], "%d", &tick_length) != 1)
		goto bad;

	    gasp->gas_tick_length = tick_length;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "tick_major_color") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->gas_tick_major_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (sscanf(argv[3], "%d", &r) != 1 ||
		sscanf(argv[4], "%d", &g) != 1 ||
		sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->gas_tick_major_color, r, g, b);

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "tick_major_length") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_tick_major_length);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_major_length;

	    if (sscanf(argv[3], "%d", &tick_major_length) != 1)
		goto bad;

	    gasp->gas_tick_major_length = tick_major_length;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "ticks_per_major") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_ticks_per_major);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int ticks_per_major;

	    if (sscanf(argv[3], "%d", &ticks_per_major) != 1)
		goto bad;

	    gasp->gas_ticks_per_major = ticks_per_major;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "tick_threshold") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_tick_threshold);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_threshold;

	    if (sscanf(argv[3], "%d", &tick_threshold) != 1)
		goto bad;

	    if (tick_threshold < 1)
		tick_threshold = 1;

	    gasp->gas_tick_threshold = tick_threshold;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (strcmp(argv[2], "triple_color") == 0) {
	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gasp->gas_triple_color);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->gas_triple_color = 1;
	    else
		gasp->gas_triple_color = 0;

	    go_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (dflag && strcmp(argv[2], "points") == 0) {
	register int i;

	if (argc == 3) {
	    bu_vls_printf(&gedp->ged_result_str, "{");
	    for (i = 0; i < gasp->gas_num_data_points; ++i) {
		bu_vls_printf(&gedp->ged_result_str, " {%lf %lf %lf} ",
			      V3ARGS(gasp->gas_data_points[i]));
	    }
	    bu_vls_printf(&gedp->ged_result_str, "}");
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(go_current_gop->go_interp, argv[3], &ac, &av) != TCL_OK) {
		bu_vls_printf(&gedp->ged_result_str, "%s", Tcl_GetStringResult(go_current_gop->go_interp));
		return BRLCAD_ERROR;
	    }

	    if (gasp->gas_num_data_points) {
		bu_free((genptr_t)gasp->gas_data_points, "data points");
		gasp->gas_data_points = (point_t *)0;
		gasp->gas_num_data_points = 0;
	    }

	    /* Clear out data points */
	    if (ac < 1) {
		go_refresh_view(gdvp);
		Tcl_Free((char *)av);
		return BRLCAD_OK;
	    }

	    gasp->gas_num_data_points = ac;
	    gasp->gas_data_points = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	    for (i = 0; i < ac; ++i) {
		if (sscanf(av[i], "%lf %lf %lf",
			   &gasp->gas_data_points[i][X],
			   &gasp->gas_data_points[i][Y],
			   &gasp->gas_data_points[i][Z]) != 3) {

		    bu_free((genptr_t)gasp->gas_data_points, "data points");
		    gasp->gas_data_points = (point_t *)0;
		    gasp->gas_num_data_points = 0;
		    bu_vls_printf(&gedp->ged_result_str, "bad data point - {%lf %lf %lf}\n",
				  V3ARGS(gasp->gas_data_points[i]));

		    go_refresh_view(gdvp);
		    Tcl_Free((char *)av);
		    return BRLCAD_ERROR;
		}
	    }

	    go_refresh_view(gdvp);
	    Tcl_Free((char *)av);
	    return BRLCAD_OK;
	}
    }

 bad:
    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

HIDDEN int
go_base2local(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr func,
	      const char *usage,
	      int maxargs)
{
    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    bu_vls_printf(&gedp->ged_result_str, "%lf", go_current_gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

    return BRLCAD_OK;
}

HIDDEN int
go_bg(struct ged *gedp,
      int argc,
      const char *argv[],
      ged_func_ptr func,
      const char *usage,
      int maxargs)
{
    int r, g, b;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* get background color */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
		      gdvp->gdv_dmp->dm_bg[0],
		      gdvp->gdv_dmp->dm_bg[1],
		      gdvp->gdv_dmp->dm_bg[2]);
	return BRLCAD_OK;
    }

    /* set background color */
    if (sscanf(argv[2], "%d", &r) != 1 ||
	sscanf(argv[3], "%d", &g) != 1 ||
	sscanf(argv[4], "%d", &b) != 1)
	goto bad_color;

    /* validate color */
    if (r < 0 || 255 < r ||
	g < 0 || 255 < g ||
	b < 0 || 255 < b)
	goto bad_color;

    DM_SET_BGCOLOR(gdvp->gdv_dmp,
		   (unsigned char)r,
		   (unsigned char)g,
		   (unsigned char)b);

    go_refresh_view(gdvp);

    return BRLCAD_OK;

 bad_color:
    bu_vls_printf(&gedp->ged_result_str, "%s: %s %s %s", argv[0], argv[2], argv[3], argv[4]);
    return BRLCAD_ERROR;
}

HIDDEN int
go_blast(struct ged *gedp,
	 int argc,
	 const char *argv[],
	 ged_func_ptr func,
	 const char *usage,
	 int maxargs)
{
    int ret;

    ret = ged_blast(gedp, argc, argv);

    if (ret != GED_OK)
	return ret;

    go_autoview_all_views(go_current_gop);

    return ret;
}

HIDDEN int
go_bounds(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  ged_func_ptr func,
	  const char *usage,
	  int maxargs)
{
    vect_t clipmin;
    vect_t clipmax;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* get window bounds */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%g %g %g %g %g %g",
		      gdvp->gdv_dmp->dm_clipmin[X],
		      gdvp->gdv_dmp->dm_clipmax[X],
		      gdvp->gdv_dmp->dm_clipmin[Y],
		      gdvp->gdv_dmp->dm_clipmax[Y],
		      gdvp->gdv_dmp->dm_clipmin[Z],
		      gdvp->gdv_dmp->dm_clipmax[Z]);
	return BRLCAD_OK;
    }

    /* set window bounds */
    if (sscanf(argv[2], "%lf %lf %lf %lf %lf %lf",
	       &clipmin[X], &clipmax[X],
	       &clipmin[Y], &clipmax[Y],
	       &clipmin[Z], &clipmax[Z]) != 6) {
	bu_vls_printf(&gedp->ged_result_str, "%s: invalid bounds - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    VMOVE(gdvp->gdv_dmp->dm_clipmin, clipmin);
    VMOVE(gdvp->gdv_dmp->dm_clipmax, clipmax);

    /*
     * Since dm_bound doesn't appear to be used anywhere, I'm going to
     * use it for controlling the location of the zclipping plane in
     * dm-ogl.c. dm-X.c uses dm_clipmin and dm_clipmax.
     */
    if (gdvp->gdv_dmp->dm_clipmax[2] <= GED_MAX)
	gdvp->gdv_dmp->dm_bound = 1.0;
    else
	gdvp->gdv_dmp->dm_bound = GED_MAX / gdvp->gdv_dmp->dm_clipmax[2];

    return BRLCAD_OK;
}

HIDDEN int
go_configure(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    struct ged_dm_view *gdvp;
    int status;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* configure the display manager window */
    status = DM_CONFIGURE_WIN(gdvp->gdv_dmp);

    /* configure the framebuffer window */
    if (gdvp->gdv_fbs.fbs_fbp != FBIO_NULL)
	fb_configureWindow(gdvp->gdv_fbs.fbs_fbp,
			   gdvp->gdv_dmp->dm_width,
			   gdvp->gdv_dmp->dm_height);

    {
	char cdimX[32];
	char cdimY[32];
	char *av[5];

	snprintf(cdimX, 32, "%d", gdvp->gdv_dmp->dm_width);
	snprintf(cdimY, 32, "%d", gdvp->gdv_dmp->dm_height);

	av[0] = "rect";
	av[1] = "cdim";
	av[2] = cdimX;
	av[3] = cdimY;
	av[4] = '\0';

	gedp->ged_gvp = gdvp->gdv_view;
	(void)ged_rect(gedp, 4, (const char **)av);
    }

    if (status == TCL_OK) {
	go_refresh_view(gdvp);
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

HIDDEN int
go_constrain_rmode(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr func,
		   const char *usage,
		   int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' &&
	 argv[2][0] != 'y' &&
	 argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_CONSTRAINED_ROTATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_constrain_rot %V %s %%x %%y}; break",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_constrain_tmode(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr func,
		   const char *usage,
		   int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' &&
	 argv[2][0] != 'y' &&
	 argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_CONSTRAINED_TRANSLATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_constrain_trans %V %s %%x %%y}; break",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_copy(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs)
{
    struct ged *from_gedp = GED_NULL;
    struct ged *to_gedp = GED_NULL;
    int ret;
    char *cp;
    struct ged_obj *gop;
    struct bu_vls db_vls;
    struct bu_vls from_vls;
    struct bu_vls to_vls;
    int fflag;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 4) {
	if (argv[1][0] != '-' || argv[1][1] != 'f' ||  argv[1][2] != '\0') {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}

	fflag = 1;

	/* Advance past the -f option */
	--argc;
	++argv;
    } else
	fflag = 0;

    bu_vls_init(&db_vls);
    bu_vls_init(&from_vls);
    bu_vls_init(&to_vls);

    if ((cp = strchr(argv[1], ':'))) {
	bu_vls_strncpy(&db_vls, argv[1], cp-argv[1]);
	bu_vls_strcpy(&from_vls, cp+1);

	for (BU_LIST_FOR(gop, ged_obj, &HeadGedObj.l)) {
	    if (!strcmp(bu_vls_addr(&gop->go_name), bu_vls_addr(&db_vls))) {
		from_gedp = gop->go_gedp;
		break;
	    }
	}

	bu_vls_free(&db_vls);

	if (from_gedp == GED_NULL) {
	    bu_vls_free(&from_vls);
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    } else {
	bu_vls_strcpy(&from_vls, argv[1]);
	from_gedp = gedp;
    }

    if ((cp = strchr(argv[2], ':'))) {
	bu_vls_trunc(&db_vls, 0);
	bu_vls_strncpy(&db_vls, argv[2], cp-argv[2]);
	bu_vls_strcpy(&to_vls, cp+1);

	for (BU_LIST_FOR(gop, ged_obj, &HeadGedObj.l)) {
	    if (!strcmp(bu_vls_addr(&gop->go_name), bu_vls_addr(&db_vls))) {
		to_gedp = gop->go_gedp;
		break;
	    }
	}

	bu_vls_free(&db_vls);

	if (to_gedp == GED_NULL) {
	    bu_vls_free(&from_vls);
	    bu_vls_free(&to_vls);
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
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
	    bu_vls_strcpy(&gedp->ged_result_str, bu_vls_addr(&from_gedp->ged_result_str));
    } else {
	ret = ged_dbcopy(from_gedp, to_gedp,
			 bu_vls_addr(&from_vls),
			 bu_vls_addr(&to_vls),
			 fflag);

	if (ret != GED_OK) {
	    if (bu_vls_strlen((const struct bu_vls *)&from_gedp->ged_result_str)) {
		if (from_gedp != gedp)
		    bu_vls_strcpy(&gedp->ged_result_str, bu_vls_addr(&from_gedp->ged_result_str));
	    } else if (to_gedp != gedp && bu_vls_strlen((const struct bu_vls *)&to_gedp))
		bu_vls_strcpy(&gedp->ged_result_str, bu_vls_addr(&to_gedp->ged_result_str));
	}
    }

    bu_vls_free(&from_vls);
    bu_vls_free(&to_vls);

    return ret;
}

HIDDEN int
go_data_axes(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    return go_axes(gedp, gdvp, &gdvp->gdv_view->gv_data_axes, argc, argv, usage, 1);
}

HIDDEN void
go_deleteViewProc(ClientData clientData)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientData;

    BU_LIST_DEQUEUE(&(gdvp->l));
    bu_vls_free(&gdvp->gdv_name);
    DM_CLOSE(gdvp->gdv_dmp);
    bu_free((genptr_t)gdvp->gdv_view, "ged_view");
    bu_free((genptr_t)gdvp, "ged_dm_view");
}

HIDDEN void
go_init_default_bindings(struct ged_dm_view *gdvp)
{
    struct bu_vls bindings;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Configure> {%V configure %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Expose> {%V refresh %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "catch {wm protocol %V WM_DELETE_WINDOW {%V delete_view %V; break}}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Mouse Bindings */
    bu_vls_printf(&bindings, "bind %V <2> {%V vslew %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <1> {%V zoom %V 0.5; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <3> {%V zoom %V 2.0; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Idle Mode */
    bu_vls_printf(&bindings, "bind %V <ButtonRelease> {%V idle_mode %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <KeyRelease-Control_L> {%V idle_mode %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <KeyRelease-Control_R> {%V idle_mode %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <KeyRelease-Shift_L> {%V idle_mode %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <KeyRelease-Shift_R> {%V idle_mode %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <KeyRelease-Alt_L> {%V idle_mode %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <KeyRelease-Alt_R> {%V idle_mode %V; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Rotate Mode */
    bu_vls_printf(&bindings, "bind %V <Control-ButtonPress-1> {%V rotate_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Control-ButtonPress-2> {%V rotate_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Control-ButtonPress-3> {%V rotate_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Translate Mode */
    bu_vls_printf(&bindings, "bind %V <Shift-ButtonPress-1> {%V translate_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Shift-ButtonPress-2> {%V translate_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Shift-ButtonPress-3> {%V translate_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Scale Mode */
    bu_vls_printf(&bindings, "bind %V <Control-Shift-ButtonPress-1> {%V scale_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Control-Shift-ButtonPress-2> {%V scale_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Control-Shift-ButtonPress-3> {%V scale_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Alt-Control-Shift-ButtonPress-1> {%V scale_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Alt-Control-Shift-ButtonPress-2> {%V scale_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Alt-Control-Shift-ButtonPress-3> {%V scale_mode %V %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Constrained Rotate Mode */
    bu_vls_printf(&bindings, "bind %V <Alt-Control-ButtonPress-1> {%V constrain_rmode %V x %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Alt-Control-ButtonPress-2> {%V constrain_rmode %V y %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Alt-Control-ButtonPress-3> {%V constrain_rmode %V z %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Constrained Translate Mode */
    bu_vls_printf(&bindings, "bind %V <Alt-Shift-ButtonPress-1> {%V constrain_tmode %V x %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Alt-Shift-ButtonPress-2> {%V constrain_tmode %V y %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V <Alt-Shift-ButtonPress-3> {%V constrain_tmode %V z %%x %%y; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    /* Key Bindings */
    bu_vls_printf(&bindings, "bind %V 3 {%V aet %V 35 25; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V 4 {%V aet %V 45 45; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V f {%V aet %V 0 0; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V R {%V aet %V 180 0; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V r {%V aet %V 270 0; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V l {%V aet %V 90 0; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V t {%V aet %V 0 90; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    bu_vls_printf(&bindings, "bind %V b {%V aet %V 0 270; break}; ",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);

    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);
}

HIDDEN int
go_init_view_bindings(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    go_init_default_bindings(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_delete_view(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    go_deleteViewProc(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_faceplate(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    int i;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 7 < argc)
	goto bad;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (strcmp(argv[2], "center_dot") == 0) {
	if (strcmp(argv[3], "draw") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_view->gv_center_dot.gos_draw);
		return BRLCAD_OK;
	    } else if (argc == 5) {
		if (sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_center_dot.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_center_dot.gos_draw = 0;

		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	if (strcmp(argv[3], "color") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_center_dot.gos_line_color));
		return BRLCAD_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (sscanf(argv[4], "%d", &r) != 1 ||
		    sscanf(argv[5], "%d", &g) != 1 ||
		    sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_center_dot.gos_line_color, r, g, b);
		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	goto bad;
    }

    if (strcmp(argv[2], "prim_labels") == 0) {
	if (strcmp(argv[3], "draw") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_view->gv_prim_labels.gos_draw);
		return BRLCAD_OK;
	    } else if (argc == 5) {
		if (sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_prim_labels.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_prim_labels.gos_draw = 0;

		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	if (strcmp(argv[3], "color") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_prim_labels.gos_line_color));
		return BRLCAD_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (sscanf(argv[4], "%d", &r) != 1 ||
		    sscanf(argv[5], "%d", &g) != 1 ||
		    sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_prim_labels.gos_line_color, r, g, b);
		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	goto bad;
    }

    if (strcmp(argv[2], "view_params") == 0) {
	if (strcmp(argv[3], "draw") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_view->gv_view_params.gos_draw);
		return BRLCAD_OK;
	    } else if (argc == 5) {
		if (sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_view_params.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_view_params.gos_draw = 0;

		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	if (strcmp(argv[3], "color") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_view_params.gos_text_color));
		return BRLCAD_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (sscanf(argv[4], "%d", &r) != 1 ||
		    sscanf(argv[5], "%d", &g) != 1 ||
		    sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_view_params.gos_text_color, r, g, b);
		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	goto bad;
    }

    if (strcmp(argv[2], "view_scale") == 0) {
	if (strcmp(argv[3], "draw") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_view->gv_view_scale.gos_draw);
		return BRLCAD_OK;
	    } else if (argc == 5) {
		if (sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_view_scale.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_view_scale.gos_draw = 0;

		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	if (strcmp(argv[3], "color") == 0) {
	    if (argc == 4) {
		bu_vls_printf(&gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_view_scale.gos_line_color));
		return BRLCAD_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (sscanf(argv[4], "%d", &r) != 1 ||
		    sscanf(argv[5], "%d", &g) != 1 ||
		    sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_view_scale.gos_line_color, r, g, b);
		go_refresh_view(gdvp);
		return BRLCAD_OK;
	    }
	}

	goto bad;
    }

 bad:
    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

HIDDEN int
go_idle_mode(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {}",
		  &gdvp->gdv_dmp->dm_pathName);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    if (gdvp->gdv_view->gv_grid.ggs_snap &&
	(gdvp->gdv_view->gv_mode == GED_TRANSLATE_MODE ||
	 gdvp->gdv_view->gv_mode == GED_CONSTRAINED_TRANSLATE_MODE)) {
	char *av[3];

	gedp->ged_gvp = gdvp->gdv_view;
	av[0] = "grid";
	av[1] = "vsnap";
	av[2] = '\0';
	ged_grid(gedp, 2, (const char **)av);
	go_refresh_view(gdvp);
    }

    gdvp->gdv_view->gv_mode = GED_IDLE_MODE;

    return BRLCAD_OK;
}

HIDDEN int
go_light(struct ged *gedp,
	 int argc,
	 const char *argv[],
	 ged_func_ptr func,
	 const char *usage,
	 int maxargs)
{
    int light;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* get light flag */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_dmp->dm_light);
	return BRLCAD_OK;
    }

    /* set light flag */
    if (sscanf(argv[2], "%d", &light) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (light < 0)
	light = 0;
    else if (1 < light)
	light = 1;

    DM_SET_LIGHT(gdvp->gdv_dmp, light);
    go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_list_views(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr func,
	      const char *usage,
	      int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l))
	bu_vls_printf(&gedp->ged_result_str, "%V ", &gdvp->gdv_name);

    return BRLCAD_OK;
}

HIDDEN int
go_listen(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  ged_func_ptr func,
	  const char *usage,
	  int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (gdvp->gdv_fbs.fbs_fbp == FBIO_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s listen: framebuffer not open!\n", argv[0]);
	return BRLCAD_ERROR;
    }

    /* return the port number */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_listener.fbsl_port);
	return BRLCAD_OK;
    }

    if (argc == 3) {
	int port;

	if (sscanf(argv[2], "%d", &port) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "listen: bad value - %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (port >= 0)
	    fbs_open(&gdvp->gdv_fbs, port);
	else {
	    fbs_close(&gdvp->gdv_fbs);
	}
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_listener.fbsl_port);
	return BRLCAD_OK;
    }

    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

HIDDEN int
go_local2base(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr func,
	      const char *usage,
	      int maxargs)
{
    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    bu_vls_printf(&gedp->ged_result_str, "%lf", go_current_gop->go_gedp->ged_wdbp->dbip->dbi_local2base);

    return BRLCAD_OK;
}

HIDDEN int
go_make(struct ged *gedp,
	int argc,
	const char *argv[],
	ged_func_ptr func,
	const char *usage,
	int maxargs)
{
    int ret;
    char *av[3];

    ret = ged_make(gedp, argc, argv);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[argc-2];
	av[2] = (char *)0;
	go_autoview_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return ret;
}

HIDDEN int
go_mirror(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  ged_func_ptr func,
	  const char *usage,
	  int maxargs)
{
    int ret;
    char *av[3];

    ret = ged_mirror(gedp, argc, argv);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[argc-1];
	av[2] = (char *)0;
	go_autoview_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return ret;
}

HIDDEN int
go_model_axes(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr func,
	      const char *usage,
	      int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    return go_axes(gedp, gdvp, &gdvp->gdv_view->gv_model_axes, argc, argv, usage, 0);
}

HIDDEN int
go_more_args_callback(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs)
{
    register int i;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* get the callback string */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "%s", bu_vls_addr(&go_current_gop->go_more_args_callback));
	
	return BRLCAD_OK;
    }

    /* set the callback string */
    bu_vls_trunc(&go_current_gop->go_more_args_callback, 0);
    for (i = 1; i < argc; ++i)
	bu_vls_printf(&go_current_gop->go_more_args_callback, "%s ", argv[i]);

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_constrain_rot(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr func,
		       const char *usage,
		       int maxargs)
{
    int ret;
    int ac;
    char *av[3];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    struct bu_vls rot_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    dx *= gdvp->gdv_view->gv_rscale;
    dy *= gdvp->gdv_view->gv_rscale;

    if (fabs(dx) > fabs(dy))
	sf = dx;
    else
	sf = dy;

    bu_vls_init(&rot_vls);
    switch (argv[2][0]) {
	case 'x':
	    bu_vls_printf(&rot_vls, "%lf 0 0", sf);
	case 'y':
	    bu_vls_printf(&rot_vls, "0 %lf 0", sf);
	case 'z':
	    bu_vls_printf(&rot_vls, "0 0 %lf", sf);
    }

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = "rot";
    av[1] = bu_vls_addr(&rot_vls);
    av[2] = (char *)0;

    ret = ged_rot(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == GED_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_constrain_trans(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs)
{
    int ret;
    int ac;
    char *av[3];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls tran_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;

    if (fabs(dx) > fabs(dy))
	sf = dx;
    else
	sf = dy;

    bu_vls_init(&tran_vls);
    switch (argv[2][0]) {
	case 'x':
	    bu_vls_printf(&tran_vls, "%lf 0 0", sf);
	case 'y':
	    bu_vls_printf(&tran_vls, "0 %lf 0", sf);
	case 'z':
	    bu_vls_printf(&tran_vls, "0 0 %lf", sf);
    }

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = "tra";
    av[1] = bu_vls_addr(&tran_vls);
    av[2] = (char *)0;

    ret = ged_tra(gedp, ac, (const char **)av);
    bu_vls_free(&tran_vls);

    if (ret == GED_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_move_arb_edge(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr func,
		       const char *usage,
		       int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    /* ged_move_arb_edge expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_init(&pt_vls);
    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "move_arb_edge";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = ged_move_arb_edge(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_move_arb_face(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr func,
		       const char *usage,
		       int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    /* ged_move_arb_face expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_init(&pt_vls);
    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "move_arb_face";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = ged_move_arb_face(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_orotate(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr func,
		 const char *usage,
		 int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls rot_x_vls;
    struct bu_vls rot_y_vls;
    struct bu_vls rot_z_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = y - gdvp->gdv_view->gv_prevMouseY;
    dy = x - gdvp->gdv_view->gv_prevMouseX;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    dx *= gdvp->gdv_view->gv_rscale;
    dy *= gdvp->gdv_view->gv_rscale;

    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_init(&rot_x_vls);
    bu_vls_init(&rot_y_vls);
    bu_vls_init(&rot_z_vls);
    bu_vls_printf(&rot_x_vls, "%lf", model[X]);
    bu_vls_printf(&rot_y_vls, "%lf", model[Y]);
    bu_vls_printf(&rot_z_vls, "%lf", model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "orotate";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&rot_x_vls);
    av[3] = bu_vls_addr(&rot_y_vls);
    av[4] = bu_vls_addr(&rot_z_vls);
    av[5] = (char *)0;

    ret = ged_orotate(gedp, 5, (const char **)av);
    bu_vls_free(&rot_x_vls);
    bu_vls_free(&rot_y_vls);
    bu_vls_free(&rot_z_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_oscale(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr func,
		const char *usage,
		int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls sf_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_sscale;
    dy *= inv_width * gdvp->gdv_view->gv_sscale;

    if (fabs(dx) < fabs(dy))
	sf = 1.0 + dy;
    else
	sf = 1.0 + dx;

    bu_vls_init(&sf_vls);
    bu_vls_printf(&sf_vls, "%lf", sf);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "oscale";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&sf_vls);
    av[3] = (char *)0;

    ret = ged_oscale(gedp, 3, (const char **)av);
    bu_vls_free(&sf_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_otranslate(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls tran_x_vls;
    struct bu_vls tran_y_vls;
    struct bu_vls tran_z_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    /* ged_otranslate expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_init(&tran_x_vls);
    bu_vls_init(&tran_y_vls);
    bu_vls_init(&tran_z_vls);
    bu_vls_printf(&tran_x_vls, "%lf", model[X]);
    bu_vls_printf(&tran_y_vls, "%lf", model[Y]);
    bu_vls_printf(&tran_z_vls, "%lf", model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "otranslate";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&tran_x_vls);
    av[3] = bu_vls_addr(&tran_y_vls);
    av[4] = bu_vls_addr(&tran_z_vls);
    av[5] = (char *)0;

    ret = ged_otranslate(gedp, 5, (const char **)av);
    bu_vls_free(&tran_x_vls);
    bu_vls_free(&tran_y_vls);
    bu_vls_free(&tran_z_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_ray(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
#if 0
    int ret;
    int ac;
    char *av[4];
    fastf_t x, y;
    fastf_t inv_width;
    point_t start;
    point_t target;
    point_t view;
    struct bu_vls mouse_vls;
    struct ged_dm_view *gdvp;


    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    x *= inv_width * 2.0 - 1.0;
    y *= inv_width * -2.0 + 1.0;
    VSET(view, x, y, -1.0);
    MAT4X3PNT(start, gdvp->ged_view->gv_view2model, view);
    VSET(view, x, y, 0.0);
    MAT4X3PNT(target, gdvp->ged_view->gv_view2model, view);

#if 0
    {
	char *av[];

	av[0] = "rt_gettrees";
	av[1] = argv[1];
	av[2] = "-i";
	av[3] = "-u";

	...

	    av[n] = (char *)0;
    }
#endif

    if (ret == GED_OK)
	go_refresh_view(gdvp);

#endif
    return BRLCAD_OK;
}

HIDDEN int
go_mouse_rot(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    int ret;
    int ac;
    char *av[4];
    fastf_t x, y;
    fastf_t dx, dy;
    struct bu_vls rot_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gdv_view->gv_prevMouseY - y;
    dy = gdvp->gdv_view->gv_prevMouseX - x;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    dx *= gdvp->gdv_view->gv_rscale;
    dy *= gdvp->gdv_view->gv_rscale;

    bu_vls_init(&rot_vls);
    bu_vls_printf(&rot_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "rot";
    av[1] = "-v";
    av[2] = bu_vls_addr(&rot_vls);
    av[3] = (char *)0;

    ret = ged_rot(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == GED_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_rotate_arb_face(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 7) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[5], "%lf", &x) != 1 ||
	sscanf(argv[6], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = y - gdvp->gdv_view->gv_prevMouseY;
    dy = x - gdvp->gdv_view->gv_prevMouseX;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    dx *= gdvp->gdv_view->gv_rscale;
    dy *= gdvp->gdv_view->gv_rscale;

    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_init(&pt_vls);
    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "rotate_arb_face";
    av[1] = (char *)argv[2];
    av[2] = (char *)argv[3];
    av[3] = (char *)argv[4];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = ged_rotate_arb_face(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_scale(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    int ret;
    char *av[3];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls zoom_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_sscale;
    dy *= inv_width * gdvp->gdv_view->gv_sscale;

    if (fabs(dx) > fabs(dy))
	sf = 1.0 + dx;
    else
	sf = 1.0 + dy;

    bu_vls_init(&zoom_vls);
    bu_vls_printf(&zoom_vls, "%lf", sf);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "zoom";
    av[1] = bu_vls_addr(&zoom_vls);
    av[2] = (char *)0;

    ret = ged_zoom(gedp, 2, (const char **)av);
    bu_vls_free(&zoom_vls);

    if (ret == GED_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_protate(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr func,
		 const char *usage,
		 int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls mrot_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = y - gdvp->gdv_view->gv_prevMouseY;
    dy = x - gdvp->gdv_view->gv_prevMouseX;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    dx *= gdvp->gdv_view->gv_rscale;
    dy *= gdvp->gdv_view->gv_rscale;

    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_init(&mrot_vls);
    bu_vls_printf(&mrot_vls, "%lf %lf %lf", V3ARGS(model));

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "protate";
    av[1] = (char *)argv[2];
    av[2] = (char *)argv[3];
    av[3] = bu_vls_addr(&mrot_vls);
    av[4] = (char *)0;

    ret = ged_protate(gedp, 4, (const char **)av);
    bu_vls_free(&mrot_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_pscale(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr func,
		const char *usage,
		int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls sf_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_sscale;
    dy *= inv_width * gdvp->gdv_view->gv_sscale;

    if (fabs(dx) < fabs(dy))
	sf = 1.0 + dy;
    else
	sf = 1.0 + dx;

    bu_vls_init(&sf_vls);
    bu_vls_printf(&sf_vls, "%lf", sf);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "pscale";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&sf_vls);
    av[5] = (char *)0;

    ret = ged_pscale(gedp, 5, (const char **)av);
    bu_vls_free(&sf_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_ptranslate(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs)
{
    int ret;
    char *av[6];
    fastf_t x, y;
    fastf_t dx, dy;
    point_t model;
    point_t view;
    fastf_t inv_width;
    mat_t inv_rot;
    struct bu_vls tvec_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    /* ged_ptranslate expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_init(&tvec_vls);
    bu_vls_printf(&tvec_vls, "%lf %lf %lf", V3ARGS(model));

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "ptranslate";
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&tvec_vls);
    av[5] = (char *)0;

    ret = ged_ptranslate(gedp, 5, (const char **)av);
    bu_vls_free(&tvec_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	go_edit_redraw_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_mouse_trans(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    int ret;
    int ac;
    char *av[4];
    fastf_t x, y;
    fastf_t dx, dy;
    fastf_t inv_width;
    struct bu_vls trans_vls;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dx = gdvp->gdv_view->gv_prevMouseX - x;
    dy = y - gdvp->gdv_view->gv_prevMouseY;

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (dx < gdvp->gdv_view->gv_minMouseDelta)
	dx = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dx)
	dx = gdvp->gdv_view->gv_maxMouseDelta;

    if (dy < gdvp->gdv_view->gv_minMouseDelta)
	dy = gdvp->gdv_view->gv_minMouseDelta;
    else if (gdvp->gdv_view->gv_maxMouseDelta < dy)
	dy = gdvp->gdv_view->gv_maxMouseDelta;

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;

    bu_vls_init(&trans_vls);
    bu_vls_printf(&trans_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "tra";
    av[1] = "-v";
    av[2] = bu_vls_addr(&trans_vls);
    av[3] = (char *)0;

    ret = ged_tra(gedp, ac, (const char **)av);
    bu_vls_free(&trans_vls);

    if (ret == GED_OK)
	go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_view_cmd(ClientData clientData,
	    Tcl_Interp *interp,
	    int argc,
	    char **argv)
{
    return TCL_OK;
}

HIDDEN int
go_move_arb_edge_mode(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_MOVE_ARB_EDGE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_move_arb_edge %V %s %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2],
		  argv[3]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_move_arb_face_mode(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_MOVE_ARB_FACE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_move_arb_face %V %s %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2],
		  argv[3]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_new_view(struct ged *gedp,
	    int argc,
	    const char *argv[],
	    ged_func_ptr func,
	    const char *usage,
	    int maxargs)
{
    struct ged_dm_view *new_gdvp = BU_LIST_LAST(ged_dm_view, &go_current_gop->go_head_views.l);
    HIDDEN const int name_index = 1;
    int type = DM_TYPE_BAD;
    struct bu_vls event_vls;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* find display manager type */
#ifdef DM_X
    if (argv[2][0] == 'X' || argv[2][0] == 'x')
	type = DM_TYPE_X;
#endif /* DM_X */

#ifdef DM_TK
    if (!strcmp(argv[2], "tk"))
	type = DM_TYPE_TK;
#endif /* DM_TK */

#ifdef DM_OGL
    if (!strcmp(argv[2], "ogl"))
	type = DM_TYPE_OGL;
#endif /* DM_OGL */

#ifdef DM_WGL
    if (!strcmp(argv[2], "wgl"))
	type = DM_TYPE_WGL;
#endif /* DM_WGL */

    if (type == DM_TYPE_BAD) {
	bu_vls_printf(&gedp->ged_result_str, "Unsupported display manager type - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    BU_GETSTRUCT(new_gdvp, ged_dm_view);
    BU_GETSTRUCT(new_gdvp->gdv_view, ged_view);

    {
	int i;
	int arg_start = 3;
	int newargs = 0;
	int ac;
	char **av;

	ac = argc + newargs;
	av = (char **)bu_malloc(sizeof(char *) * (ac+1), "go_new_view: av");
	av[0] = (char *)argv[0];

	/*
	 * Stuff name into argument list.
	 */
	av[1] = "-n";
	av[2] = (char *)argv[name_index];

	/* copy the rest */
	for (i = arg_start; i < argc; ++i)
	    av[i+newargs] = (char *)argv[i];
	av[i+newargs] = (char *)NULL;

	if ((new_gdvp->gdv_dmp = dm_open(go_current_gop->go_interp, type, ac, av)) == DM_NULL) {
	    bu_free((genptr_t)new_gdvp->gdv_view, "ged_view");
	    bu_free((genptr_t)new_gdvp, "ged_dm_view");
	    bu_free((genptr_t)av, "go_new_view: av");

	    bu_vls_printf(&gedp->ged_result_str, "Failed to create %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	bu_free((genptr_t)av, "go_new_view: av");

    }

    new_gdvp->gdv_gop = go_current_gop;
    bu_vls_init(&new_gdvp->gdv_name);
    bu_vls_printf(&new_gdvp->gdv_name, argv[name_index]);
    ged_view_init(new_gdvp->gdv_view);
    BU_LIST_INSERT(&go_current_gop->go_head_views.l, &new_gdvp->l);

    new_gdvp->gdv_fbs.fbs_listener.fbsl_fbsp = &new_gdvp->gdv_fbs;
    new_gdvp->gdv_fbs.fbs_listener.fbsl_fd = -1;
    new_gdvp->gdv_fbs.fbs_listener.fbsl_port = -1;
    new_gdvp->gdv_fbs.fbs_fbp = FBIO_NULL;
    new_gdvp->gdv_fbs.fbs_callback = go_fbs_callback;
    new_gdvp->gdv_fbs.fbs_clientData = new_gdvp;
    new_gdvp->gdv_fbs.fbs_interp = go_current_gop->go_interp;

    /* open the framebuffer */
    go_open_fbs(new_gdvp, go_current_gop->go_interp);

    /* Set default bindings */
    go_init_default_bindings(new_gdvp);

    bu_vls_init(&event_vls);
    bu_vls_printf(&event_vls, "event generate %V <Configure>; %V autoview %V",
		  &new_gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &new_gdvp->gdv_name);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&event_vls));
    bu_vls_free(&event_vls);

    (void)Tcl_CreateCommand(go_current_gop->go_interp,
			    bu_vls_addr(&new_gdvp->gdv_dmp->dm_pathName),
			    (Tcl_CmdProc *)go_view_cmd,
			    (ClientData)new_gdvp,
			    go_deleteViewProc);

    bu_vls_printf(&gedp->ged_result_str, bu_vls_addr(&new_gdvp->gdv_name));
    return BRLCAD_OK;
}

HIDDEN int
go_orotate_mode(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr func,
		const char *usage,
		int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_OROTATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_orotate %V %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_oscale_mode(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_OSCALE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_oscale %V %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_otranslate_mode(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr func,
		   const char *usage,
		   int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &x) != 1 ||
	sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_OTRANSLATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_otranslate %V %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_paint_rect_area(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr func,
		   const char *usage,
		   int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }
    if (argc < 2 || 7 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    (void)fb_refresh(gdvp->gdv_fbs.fbs_fbp, gdvp->gdv_view->gv_rect.grs_pos[X], gdvp->gdv_view->gv_rect.grs_pos[Y],
		     gdvp->gdv_view->gv_rect.grs_dim[X], gdvp->gdv_view->gv_rect.grs_dim[Y]);

    return BRLCAD_OK;
}


#if defined(DM_OGL) || defined(DM_WGL)
HIDDEN int
go_png(struct ged *gedp,
       int argc,
       const char *argv[],
       ged_func_ptr func,
       const char *usage,
       int maxargs)
{
    struct ged_dm_view *gdvp;
    FILE *fp;
    png_structp png_p;
    png_infop info_p;
    unsigned char **rows;
    unsigned char *idata;
    unsigned char *irow;
    int bytes_per_pixel;
    int bits_per_channel = 8;  /* bits per color channel */
    int i, j, k;
    unsigned char *dbyte0, *dbyte1, *dbyte2, *dbyte3;
    int width;
    int height;
    int found_valid_dm = 0;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if ((fp = fopen(argv[2], "wb")) == NULL) {
	bu_vls_printf(&gedp->ged_result_str,
		      "%s: cannot open \"%s\" for writing.",
		      argv[0], argv[2]);
	return GED_ERROR;
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_vls_printf(&gedp->ged_result_str, "%s: could not create PNG write structure.", argv[0]);
	fclose(fp);
	return GED_ERROR;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_vls_printf(&gedp->ged_result_str, "%s: could not create PNG info structure.", argv[0]);
	fclose(fp);
	return GED_ERROR;
    }

    if (gdvp->gdv_dmp->dm_type == DM_TYPE_WGL || gdvp->gdv_dmp->dm_type == DM_TYPE_OGL) {
	int make_ret = 0;
	found_valid_dm = 1;
	width = gdvp->gdv_dmp->dm_width;
	height = gdvp->gdv_dmp->dm_height;
	bytes_per_pixel = sizeof(GLuint);

#if defined(DM_WGL)
	make_ret = wglMakeCurrent(((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->hdc,
				  ((struct wgl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->glxc);
#else
#  if defined(DM_OGL)
	make_ret = glXMakeCurrent(((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
				  ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
				  ((struct ogl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->glxc);
#  endif
#endif
	if (!make_ret) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: Couldn't make context current\n", argv[0]);
	    fclose(fp);
	    return GED_ERROR;
	}

	{
	    unsigned int pixel;
	    unsigned int red_mask = 0xff000000;
	    unsigned int green_mask = 0x00ff0000;
	    unsigned int blue_mask = 0x0000ff00;
#if defined(DM_WGL)
	    unsigned int alpha_mask = 0x000000ff;
#endif
	    int big_endian, swap_bytes;
	    int bytes_per_line = gdvp->gdv_dmp->dm_width * bytes_per_pixel;
	    GLuint *pixels = bu_calloc(width * height, bytes_per_pixel, "pixels");

	    if ((bu_byteorder() == BU_BIG_ENDIAN))
		big_endian = 1;
	    else
		big_endian = 0;

#if defined(DM_WGL)
	    /* WTF */
	    swap_bytes = !big_endian;
#else
	    swap_bytes = big_endian;
#endif

	    glReadBuffer(GL_FRONT);
#if defined(DM_WGL)
	    /* XXX GL_UNSIGNED_INT_8_8_8_8 is currently not
	     * available on windows.  Need to update when it
	     * becomes available.
	     */
	    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#else
	    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixels);
#endif

	    rows = (unsigned char **)bu_calloc(height, sizeof(unsigned char *), "rows");
	    idata = (unsigned char *)bu_calloc(height * width, bytes_per_pixel, "png data");

	    for (i = 0, j = 0; i < height; ++i, ++j) {
		/* irow points to the current scanline in pixels */
		irow = (unsigned char *)(((unsigned char *)pixels) + ((height-i-1)*bytes_per_line));

		/* rows[j] points to the current scanline in pixels */
		rows[j] = (unsigned char *)(idata + ((height-i-1)*bytes_per_line));

		/* for each pixel in current scanline of ximage_p */
		for (k = 0; k < bytes_per_line; k += bytes_per_pixel) {
		    pixel = *((unsigned int *)(irow + k));

		    dbyte0 = rows[j] + k;
		    dbyte1 = dbyte0 + 1;
		    dbyte2 = dbyte0 + 2;
		    dbyte3 = dbyte0 + 3;

		    *dbyte0 = (pixel & red_mask) >> 24;
		    *dbyte1 = (pixel & green_mask) >> 16;
		    *dbyte2 = (pixel & blue_mask) >> 8;
#if defined(DM_WGL)
		    *dbyte3 = pixel & alpha_mask;
#else
		    *dbyte3 = 255;
#endif

#if defined(DM_WGL)
		    if (swap_bytes) {
			unsigned char tmp_byte;

			/* swap byte1 and byte2 */
			tmp_byte = *dbyte1;
			*dbyte1 = *dbyte2;
			*dbyte2 = tmp_byte;

			/* swap byte0 and byte3 */
			tmp_byte = *dbyte0;
			*dbyte0 = *dbyte3;
			*dbyte3 = tmp_byte;
		    }
#endif
		}
	    }

	    bu_free(pixels, "pixels");
	}
    }

    if (!found_valid_dm) {
	bu_vls_printf(&gedp->ged_result_str, "%s: not yet supported for this display manager.", argv[0]);
	fclose(fp);
	return GED_ERROR;
    }


    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, Z_BEST_COMPRESSION);
    png_set_IHDR(png_p, info_p, width, height, bits_per_channel,
		 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, 0.5);
    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);

    bu_free(rows, "rows");
    bu_free(idata, "image data");
    fclose(fp);

    return GED_OK;
}
#endif

HIDDEN int
go_prim_label(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr func,
	      const char *usage,
	      int maxargs)
{
    register int i;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* Free the previous list of primitives scheduled for labeling */
    if (go_current_gop->go_prim_label_list_size) {
	for (i = 0; i < go_current_gop->go_prim_label_list_size; ++i)
	    bu_vls_free(&go_current_gop->go_prim_label_list[i]);
	bu_free((void *)go_current_gop->go_prim_label_list, "prim_label");
	go_current_gop->go_prim_label_list = (struct bu_vls *)0;
    }

    /* Set the list of primitives scheduled for labeling */
    go_current_gop->go_prim_label_list_size = argc - 1;
    if (go_current_gop->go_prim_label_list_size < 1)
	return BRLCAD_OK;

    go_current_gop->go_prim_label_list = bu_calloc(go_current_gop->go_prim_label_list_size,
						   sizeof(struct bu_vls), "prim_label");
    for (i = 0; i < go_current_gop->go_prim_label_list_size; ++i) {
	bu_vls_init(&go_current_gop->go_prim_label_list[i]);
	bu_vls_printf(&go_current_gop->go_prim_label_list[i], "%s", argv[i+1]);
    }

    return BRLCAD_OK;
}

HIDDEN int
go_refresh(struct ged *gedp,
	   int argc,
	   const char *argv[],
	   ged_func_ptr func,
	   const char *usage,
	   int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_refresh_all(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    if (argc != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    go_refresh_all_views(go_current_gop);

    return BRLCAD_OK;
}

HIDDEN int
go_refresh_on(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr func,
	      const char *usage,
	      int maxargs)
{
    int on;

    if (2 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    /* Get refresh_on state */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "%d", go_current_gop->go_refresh_on);
	return BRLCAD_OK;
    }

    /* Set refresh_on state */
    if (sscanf(argv[1], "%d", &on) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    go_current_gop->go_refresh_on = on;

    return BRLCAD_OK;
}

HIDDEN int
go_rotate_arb_face_mode(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 7) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[5], "%lf", &x) != 1 ||
	sscanf(argv[6], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_ROTATE_ARB_FACE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_rotate_arb_face %V %s %s %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2],
		  argv[3],
		  argv[4]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_rotate_mode(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_ROTATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_rot %V %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

/**
 * GO _ D E L E T E P R O C _ R T
 *
 *@brief
 * Called when the named proc created by rt_gettrees() is destroyed.
 */
HIDDEN void
go_deleteProc_rt(ClientData clientData)
{
    struct application *ap = (struct application *)clientData;
    struct rt_i *rtip;

    RT_AP_CHECK(ap);
    rtip = ap->a_rt_i;
    RT_CK_RTI(rtip);

    rt_free_rti(rtip);
    ap->a_rt_i = (struct rt_i *)NULL;

    bu_free((genptr_t)ap, "struct application");
}

/**
 * G O _ R T _ G E T T R E E S
 *
 * @brief
 * Given an instance of a database and the name of some treetops,
 * create a named "ray-tracing" object (proc) which will respond to
 * subsequent operations.  Returns new proc name as result.
 *
 * @par Example:
 *	.inmem rt_gettrees .rt all.g light.r
 */
int
go_rt_gettrees(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    struct rt_i *rtip;
    struct application *ap;
    struct resource *resp;
    char *newprocname;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    rtip = rt_new_rti(gedp->ged_wdbp->dbip);
    newprocname = (char *)argv[1];

    /* Delete previous proc (if any) to release all that memory, first */
    (void)Tcl_DeleteCommand(go_current_gop->go_interp, newprocname);

    while (2 < argc && argv[2][0] == '-') {
	if (strcmp(argv[2], "-i") == 0) {
	    rtip->rti_dont_instance = 1;
	    argc--;
	    argv++;
	    continue;
	}
	if (strcmp(argv[2], "-u") == 0) {
	    rtip->useair = 1;
	    argc--;
	    argv++;
	    continue;
	}
	break;
    }

    if (rt_gettrees(rtip, argc-2, (const char **)&argv[2], 1) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "rt_gettrees() returned error");
	rt_free_rti(rtip);
	return TCL_ERROR;
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
    BU_GETSTRUCT(resp, resource);
    rt_init_resource(resp, 0, rtip);
    BU_ASSERT_PTR(BU_PTBL_GET(&rtip->rti_resources, 0), !=, NULL);

    ap = (struct application *)bu_malloc(sizeof(struct application), "go_rt_gettrees: ap");
    RT_APPLICATION_INIT(ap);
    ap->a_magic = RT_AP_MAGIC;
    ap->a_resource = resp;
    ap->a_rt_i = rtip;
    ap->a_purpose = "Conquest!";

    rt_ck(rtip);

    /* Instantiate the proc, with clientData of wdb */
    /* Beware, returns a "token", not TCL_OK. */
    (void)Tcl_CreateCommand(go_current_gop->go_interp, newprocname, rt_tcl_rt,
			    (ClientData)ap, go_deleteProc_rt);

    /* Return new function name as result */
    bu_vls_printf(&gedp->ged_result_str, "%s", newprocname);

    return TCL_OK;
}

HIDDEN int
go_protate_mode(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr func,
		const char *usage,
		int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_PROTATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_protate %V %s %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2],
		  argv[3]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_pscale_mode(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_PSCALE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_pscale %V %s %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2],
		  argv[3]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_ptranslate_mode(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr func,
		   const char *usage,
		   int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &x) != 1 ||
	sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_PTRANSLATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_ptranslate %V %s %s %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name,
		  argv[2],
		  argv[3]);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_scale_mode(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr func,
	      const char *usage,
	      int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_SCALE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_scale %V %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_screen2model(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr func,
		const char *usage,
		int maxargs)
{
    fastf_t x, y;
    fastf_t inv_width;
    fastf_t inv_height;
    fastf_t inv_aspect;
    point_t view;
    point_t model;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    inv_height = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_height;
    inv_aspect = (fastf_t)gdvp->gdv_dmp->dm_height / (fastf_t)gdvp->gdv_dmp->dm_width;
    x = x * inv_width * 2.0 - 1.0;
    y = (y * inv_height * -2.0 + 1.0) * inv_aspect;
    VSET(view, x, y, 0.0);
    MAT4X3PNT(model, gdvp->gdv_view->gv_view2model, view);

    bu_vls_printf(&gedp->ged_result_str, "%lf %lf %lf", V3ARGS(model));

    return BRLCAD_OK;
}

HIDDEN int
go_screen2view(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    fastf_t x, y;
    fastf_t inv_width;
    fastf_t inv_height;
    fastf_t inv_aspect;
    point_t view;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    inv_width = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_width;
    inv_height = 1.0 / (fastf_t)gdvp->gdv_dmp->dm_height;
    inv_aspect = (fastf_t)gdvp->gdv_dmp->dm_height / (fastf_t)gdvp->gdv_dmp->dm_width;
    x = x * inv_width * 2.0 - 1.0;
    y = (y * inv_height * -2.0 + 1.0) * inv_aspect;
    VSET(view, x, y, 0.0);

    bu_vls_printf(&gedp->ged_result_str, "%lf %lf %lf", V3ARGS(view));

    return BRLCAD_OK;
}

HIDDEN int
go_set_coord(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Get coord */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%c", gdvp->gdv_view->gv_coord);
	return BRLCAD_OK;
    }

    /* Set coord */
    if ((argv[2][0] != 'm' && argv[2][0] != 'v') || argv[2][1] != '\0') {
	bu_vls_printf(&gedp->ged_result_str, "set_coord: bad value - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_coord = argv[2][0];

    return BRLCAD_OK;
}

HIDDEN int
go_set_fb_mode(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr func,
	       const char *usage,
	       int maxargs)
{
    int mode;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Get fb mode */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_mode);
	return BRLCAD_OK;
    }

    /* Set fb mode */
    if (sscanf(argv[2], "%d", &mode) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "set_fb_mode: bad value - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    if (mode < 0)
	mode = 0;
    else if (GED_OBJ_FB_MODE_OVERLAY < mode)
	mode = GED_OBJ_FB_MODE_OVERLAY;

    gdvp->gdv_fbs.fbs_mode = mode;
    go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_translate_mode(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *usage,
		  int maxargs)
{
    fastf_t x, y;
    struct bu_vls bindings;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &x) != 1 ||
	sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = GED_TRANSLATE_MODE;

    bu_vls_init(&bindings);
    bu_vls_printf(&bindings, "bind %V <Motion> {%V mouse_trans %V %%x %%y}",
		  &gdvp->gdv_dmp->dm_pathName,
		  &go_current_gop->go_name,
		  &gdvp->gdv_name);
    Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return BRLCAD_OK;
}

HIDDEN int
go_transparency(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr func,
		const char *usage,
		int maxargs)
{
    int transparency;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* get transparency flag */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_dmp->dm_transparency);
	return BRLCAD_OK;
    }

    /* set transparency flag */
    if (argc == 3) {
	if (sscanf(argv[2], "%d", &transparency) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: invalid transparency value - %s", argv[2]);
	    return BRLCAD_ERROR;
	}

	DM_SET_TRANSPARENCY(gdvp->gdv_dmp, transparency);
	return BRLCAD_OK;
    }

    return BRLCAD_OK;
}

HIDDEN int
go_view_axes(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    return go_axes(gedp, gdvp, &gdvp->gdv_view->gv_view_axes, argc, argv, usage, 0);
}

HIDDEN int
go_view_win_size(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr func,
		 const char *usage,
		 int maxargs)
{
    struct ged_dm_view *gdvp;
    int width, height;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc > 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d %d", gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);
	return BRLCAD_OK;
    }

    if (argc == 3) {
	if (sscanf(argv[2], "%d", &width) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: bad size %s", argv[0], argv[2]);
	    return BRLCAD_ERROR;
	}

	height = width;
    } else {
	if (sscanf(argv[2], "%d", &width) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: bad width %s", argv[0], argv[2]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[3], "%d", &height) != 1) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: bad height %s", argv[0], argv[3]);
	    return BRLCAD_ERROR;
	}
    }

#if defined(DM_X) || defined(DM_TK) || defined(DM_OGL) || defined(DM_WGL)
    Tk_GeometryRequest(((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->xtkwin,
		       width, height);
#endif

    return BRLCAD_OK;
}

HIDDEN int
go_vmake(struct ged *gedp,
	 int argc,
	 const char *argv[],
	 ged_func_ptr func,
	 const char *usage,
	 int maxargs)
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    {
	int ret;
	char *av[8];
	char center[512];
	char scale[128];

	sprintf(center, "%f %f %f",
		-gdvp->gdv_view->gv_center[MDX],
		-gdvp->gdv_view->gv_center[MDY],
		-gdvp->gdv_view->gv_center[MDZ]);
	sprintf(scale, "%f", gdvp->gdv_view->gv_scale * 2.0);

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
	    go_autoview_func(gedp, 2, (const char **)av, ged_draw, (char *)0, MAXARGS);
	}

	return ret;
    }
}

HIDDEN int
go_vslew(struct ged *gedp,
	 int argc,
	 const char *argv[],
	 ged_func_ptr func,
	 const char *usage,
	 int maxargs)
{
    int ret;
    int ac;
    char *av[3];
    fastf_t xpos1, ypos1;
    fastf_t xpos2, ypos2;
    fastf_t sf;
    struct bu_vls slew_vec;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &xpos1) != 1 ||
	sscanf(argv[3], "%lf", &ypos1) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    xpos2 = 0.5 * gdvp->gdv_dmp->dm_width;
    ypos2 = 0.5 * gdvp->gdv_dmp->dm_height;
    sf = 2.0 / gdvp->gdv_dmp->dm_width;

    bu_vls_init(&slew_vec);
    bu_vls_printf(&slew_vec, "%lf %lf", (xpos1 - xpos2) * sf, (ypos2 - ypos1) * sf);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = (char *)argv[0];
    av[1] = bu_vls_addr(&slew_vec);
    av[2] = (char *)0;

    ret = ged_slew(gedp, ac, (const char **)av);
    bu_vls_free(&slew_vec);

    if (ret == GED_OK) {
	if (gdvp->gdv_view->gv_grid.ggs_snap) {

	    gedp->ged_gvp = gdvp->gdv_view;
	    av[0] = "grid";
	    av[1] = "vsnap";
	    av[2] = '\0';
	    ged_grid(gedp, 2, (const char **)av);
	}
	go_refresh_view(gdvp);
    }

    return ret;
}

HIDDEN int
go_zbuffer(struct ged *gedp,
	   int argc,
	   const char *argv[],
	   ged_func_ptr func,
	   const char *usage,
	   int maxargs)
{
    int zbuffer;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* get zbuffer flag */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_dmp->dm_zbuffer);
	return BRLCAD_OK;
    }

    /* set zbuffer flag */
    if (sscanf(argv[2], "%d", &zbuffer) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (zbuffer < 0)
	zbuffer = 0;
    else if (1 < zbuffer)
	zbuffer = 1;

    DM_SET_ZBUFFER(gdvp->gdv_dmp, zbuffer);
    go_refresh_view(gdvp);

    return BRLCAD_OK;
}

HIDDEN int
go_zclip(struct ged *gedp,
	 int argc,
	 const char *argv[],
	 ged_func_ptr func,
	 const char *usage,
	 int maxargs)
{
    int zclip;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* get zclip flag */
    if (argc == 2) {
	bu_vls_printf(&gedp->ged_result_str, "%d", gdvp->gdv_view->gv_zclip);
	return BRLCAD_OK;
    }

    /* set zclip flag */
    if (sscanf(argv[2], "%d", &zclip) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (zclip < 0)
	zclip = 0;
    else if (1 < zclip)
	zclip = 1;

    gdvp->gdv_view->gv_zclip = zclip;
    gdvp->gdv_dmp->dm_zclip = zclip;
    go_refresh_view(gdvp);

    return BRLCAD_OK;
}


/*************************** Wrapper Functions ***************************/
HIDDEN int
go_autoview_func(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr func,
		 const char *usage,
		 int maxargs)
{
    int ret;
    char *av[2];
    int aflag = 0;

    av[0] = "who";
    av[1] = (char *)0;
    ret = ged_who(gedp, 1, (const char **)av);

    if (ret == GED_OK && strlen(bu_vls_addr(&gedp->ged_result_str)) == 0)
	aflag = 1;

    ret = (*func)(gedp, argc, (const char **)argv);

    if (ret == GED_OK) {
	if (aflag)
	    go_autoview_all_views(go_current_gop);
	else
	    go_refresh_all_views(go_current_gop);
    }

    return ret;
}

HIDDEN int
go_edit_redraw_func(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs)
{
    register int i;
    register struct ged_display_list *gdlp;
    register struct ged_display_list *next_gdlp;
    struct db_full_path fullpath, subpath;
    int ret = GED_OK;

    if (argc != 2)
	return GED_ERROR;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	gdlp->gdl_wflag = 0;
	gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);
    }

    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, argv[1]) == 0) {
	for (i = 0; i < subpath.fp_len; ++i) {
	    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
	    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
		next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

		if (gdlp->gdl_wflag) {
		    gdlp = next_gdlp;
		    continue;
		}

		if (db_string_to_path(&fullpath, gedp->ged_wdbp->dbip, bu_vls_addr(&gdlp->gdl_path)) == 0) {
		    if (db_full_path_search(&fullpath, subpath.fp_names[i])) {
			register struct ged_display_list *last_gdlp;
			register struct solid *sp;
			char mflag[8];
			char xflag[8];
			char *av[5];

			av[0] = (char *)argv[0];
			av[1] = mflag;
			av[2] = xflag;
			av[3] = bu_vls_strdup(&gdlp->gdl_path);
			av[4] = (char *)0;

			sp = BU_LIST_NEXT(solid, &gdlp->gdl_headSolid);
			snprintf(mflag, 7, "-m%d", sp->s_dmode);
			snprintf(xflag, 7, "-x%.2g", sp->s_transparency);

			ret = (*func)(gedp, 4, (const char **)av);
			bu_free((genptr_t)av[3], "go_edit_redraw_func");

			/* The function call above causes gdlp to be
			 * removed from the display list. A new one is
			 * then created and appended to the end.  Here
			 * we put it back where it belongs (i.e. as
			 * specified by the user).  This also prevents
			 * an infinite loop where the last and the
			 * second to last list items play leap frog
			 * with the end of list.
			 */
			last_gdlp = BU_LIST_PREV(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
			BU_LIST_DEQUEUE(&last_gdlp->l);
			BU_LIST_INSERT(&next_gdlp->l, &last_gdlp->l);
			last_gdlp->gdl_wflag = 1;
		    }

		    db_free_full_path(&fullpath);
		}

		gdlp = next_gdlp;
	    }
	}

	db_free_full_path(&subpath);
    }

    go_refresh_all_views(go_current_gop);

    return ret;
}

HIDDEN int
go_more_args_func(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *usage,
		  int maxargs)
{
    register int i;
    int ac;
    int ret;
    char *av[256];
    struct bu_vls callback_cmd;
    struct bu_vls temp;

    bu_vls_init(&callback_cmd);
    bu_vls_init(&temp);

    /* copy all args */
    ac = argc;
    for (i = 0; i < ac; ++i)
	av[i] = bu_strdup((char *)argv[i]);
    av[ac] = (char *)0;

    while ((ret = (*func)(gedp, ac, (const char **)av)) & GED_MORE) {
	int ac_more;
	const char **avmp;
	const char **av_more = NULL;

	if (0 < bu_vls_strlen(&go_current_gop->go_more_args_callback)) {
	    bu_vls_trunc(&callback_cmd, 0);
	    bu_vls_printf(&callback_cmd, "%s \"%s\"",
			  bu_vls_addr(&go_current_gop->go_more_args_callback),
			  bu_vls_addr(&gedp->ged_result_str));

	    if (Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&callback_cmd)) != TCL_OK) {
		bu_vls_trunc(&gedp->ged_result_str, 0);
		bu_vls_printf(&gedp->ged_result_str, "%s", Tcl_GetStringResult(go_current_gop->go_interp));
		Tcl_ResetResult(go_current_gop->go_interp);
		return BRLCAD_ERROR;
	    }

	    bu_vls_trunc(&temp, 0);
	    bu_vls_printf(&temp, Tcl_GetStringResult(go_current_gop->go_interp));
	    Tcl_ResetResult(go_current_gop->go_interp);
	} else {
	    bu_log("\r%s", bu_vls_addr(&gedp->ged_result_str));
	    bu_vls_trunc(&temp, 0);
	    if (bu_vls_gets(&temp, stdin) < 0) {
		break;
	    }
	}
	
	if (Tcl_SplitList(go_current_gop->go_interp, bu_vls_addr(&temp), &ac_more, &av_more) != TCL_OK) {
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
	for (i = 0; i < ac_more; ++i)
	    av[ac++] = bu_strdup(avmp[i]);
	av[ac+1] = (char *)0;

	Tcl_Free((char *)av_more);
    }

    bu_vls_free(&callback_cmd);
    bu_vls_free(&temp);

    for (i = 0; i < ac; ++i)
	bu_free((void *)av[i], "go_more_args_func");

    return ret;
}

HIDDEN int
go_pass_through_func(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *usage,
		     int maxargs)
{
    return (*func)(gedp, argc, argv);
}

HIDDEN int
go_pass_through_and_refresh_func(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *usage,
				 int maxargs)
{
    int ret;

    ret = (*func)(gedp, argc, argv);

    if (ret == GED_OK)
	go_refresh_all_views(go_current_gop);

    return ret;
}

HIDDEN int
go_view_func(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    register int i;
    int ret;
    int ac;
    char *av[MAXARGS];
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (MAXARGS < maxargs || maxargs < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &go_current_gop->go_head_views.l)) {
	if (!strcmp(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &go_current_gop->go_head_views.l)) {
	bu_vls_printf(&gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Copy argv into av while skipping argv[1] (i.e. the view name) */
    gedp->ged_gvp = gdvp->gdv_view;
    gedp->ged_refresh_clientdata = (void *)gdvp;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;
    ret = (*func)(gedp, ac, (const char **)av);

    /* Keep the view's perspective in sync with its corresponding display manager */
    gdvp->gdv_dmp->dm_perspective = gdvp->gdv_view->gv_perspective;

    if (ret == GED_OK)
	go_refresh_view(gdvp);

    return ret;
}


/*************************** Local Utility Functions ***************************/
HIDDEN void
go_drawSolid(struct dm *dmp, struct solid *sp)
{
    if (sp->s_iflag == UP)
	DM_SET_FGCOLOR(dmp, 255, 255, 255, 0, sp->s_transparency);
    else
	DM_SET_FGCOLOR(dmp,
		       (unsigned char)sp->s_color[0],
		       (unsigned char)sp->s_color[1],
		       (unsigned char)sp->s_color[2], 0, sp->s_transparency);

    DM_DRAW_VLIST(dmp, (struct bn_vlist *)&sp->s_vlist);
}

/* Draw all display lists */
HIDDEN int
go_drawDList(struct dm *dmp, struct bu_list *hdlp)
{
    register struct ged_display_list *gdlp;
    register struct ged_display_list *next_gdlp;
    struct solid *sp;
    int line_style = -1;

    if (dmp->dm_transparency) {
	/* First, draw opaque stuff */
	gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		if (sp->s_transparency < 1.0)
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, line_style);
		}

		go_drawSolid(dmp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* disable write to depth buffer */
	DM_SET_DEPTH_MASK(dmp, 0);

	/* Second, draw transparent stuff */
	gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		/* already drawn above */
		if (NEAR_ZERO(sp->s_transparency - 1.0, SMALL_FASTF))
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, line_style);
		}

		go_drawSolid(dmp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* re-enable write to depth buffer */
	DM_SET_DEPTH_MASK(dmp, 1);
    } else {
	gdlp = BU_LIST_NEXT(ged_display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    DM_SET_LINE_ATTR(dmp, dmp->dm_lineWidth, line_style);
		}

		go_drawSolid(dmp, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    return BRLCAD_OK;
}

HIDDEN void
go_fbs_callback(genptr_t clientData)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientData;

    go_refresh_view(gdvp);
}

HIDDEN int
go_close_fbs(struct ged_dm_view *gdvp)
{
    if (gdvp->gdv_fbs.fbs_fbp == FBIO_NULL)
	return TCL_OK;

    fb_flush(gdvp->gdv_fbs.fbs_fbp);

    switch (gdvp->gdv_dmp->dm_type) {
#ifdef DM_X
	case DM_TYPE_X:
	    X24_close_existing(gdvp->gdv_fbs.fbs_fbp);
	    break;
#endif
#ifdef DM_TK
/* XXX TJM: not ready yet
   case DM_TYPE_TK:
   tk_close_existing(gdvp->gdv_fbs.fbs_fbp);
   break;
*/
#endif
#ifdef DM_OGL
	case DM_TYPE_OGL:
	    ogl_close_existing(gdvp->gdv_fbs.fbs_fbp);
	    break;
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    wgl_close_existing(gdvp->gdv_fbs.fbs_fbp);
	    break;
#endif
    }

    /* free framebuffer memory */
    if (gdvp->gdv_fbs.fbs_fbp->if_pbase != PIXEL_NULL)
	free((void *)gdvp->gdv_fbs.fbs_fbp->if_pbase);
    free((void *)gdvp->gdv_fbs.fbs_fbp->if_name);
    free((void *)gdvp->gdv_fbs.fbs_fbp);
    gdvp->gdv_fbs.fbs_fbp = FBIO_NULL;

    return TCL_OK;
}

/*
 * Open/activate the display managers framebuffer.
 */
HIDDEN int
go_open_fbs(struct ged_dm_view *gdvp, Tcl_Interp *interp)
{

    /* already open */
    if (gdvp->gdv_fbs.fbs_fbp != FBIO_NULL)
	return TCL_OK;

    /* don't use bu_calloc so we can fail slightly more gradefully */
    if ((gdvp->gdv_fbs.fbs_fbp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "openfb: failed to allocate framebuffer memory\n",
			       (char *)NULL);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
    }

    switch (gdvp->gdv_dmp->dm_type) {
#ifdef DM_X
	case DM_TYPE_X:
	    *gdvp->gdv_fbs.fbs_fbp = X24_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/X")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/X", strlen("/dev/X")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _X24_open_existing(gdvp->gdv_fbs.fbs_fbp,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			       ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->pix,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			       gdvp->gdv_dmp->dm_width,
			       gdvp->gdv_dmp->dm_height,
			       ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->gc);
	    break;
#endif
#ifdef DM_TK
#if 0
/* XXX TJM implement _tk_open_existing */
	case DM_TYPE_TK:
	    *gdvp->gdv_fbs.fbs_fbp = tk_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/tk")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/tk", strlen("/dev/tk")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _tk_open_existing(gdvp->gdv_fbs.fbs_fbp,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			      ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->pix,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			      ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			      gdvp->gdv_dmp->dm_width,
			      gdvp->gdv_dmp->dm_height,
			      ((struct x_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->gc);
	    break;
#endif
#endif

#ifdef DM_OGL
	case DM_TYPE_OGL:
	    *gdvp->gdv_fbs.fbs_fbp = ogl_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/ogl")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/ogl", strlen("/dev/ogl")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _ogl_open_existing(gdvp->gdv_fbs.fbs_fbp,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			       gdvp->gdv_dmp->dm_width,
			       gdvp->gdv_dmp->dm_height,
			       ((struct ogl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->glxc,
			       ((struct ogl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->mvars.doublebuffer,
			       0);
	    break;
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    *gdvp->gdv_fbs.fbs_fbp = wgl_interface; /* struct copy */

	    gdvp->gdv_fbs.fbs_fbp->if_name = bu_malloc((unsigned)strlen("/dev/wgl")+1, "if_name");
	    bu_strlcpy(gdvp->gdv_fbs.fbs_fbp->if_name, "/dev/wgl", strlen("/dev/wgl")+1);

	    /* Mark OK by filling in magic number */
	    gdvp->gdv_fbs.fbs_fbp->if_magic = FB_MAGIC;

	    _wgl_open_existing(gdvp->gdv_fbs.fbs_fbp,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->dpy,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->win,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->cmap,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->vip,
			       ((struct dm_xvars *)gdvp->gdv_dmp->dm_vars.pub_vars)->hdc,
			       gdvp->gdv_dmp->dm_width,
			       gdvp->gdv_dmp->dm_height,
			       ((struct wgl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->glxc,
			       ((struct wgl_vars *)gdvp->gdv_dmp->dm_vars.priv_vars)->mvars.doublebuffer,
			       0);
	    break;
#endif
    }

    return TCL_OK;
}

HIDDEN void
go_draw(struct ged_dm_view *gdvp)
{
    mat_t new;
    matp_t mat;
    mat_t perspective_mat;

    mat = gdvp->gdv_view->gv_model2view;

    if (0 < gdvp->gdv_view->gv_perspective) {
#if 1
	point_t l, h;

	VSET(l, -1.0, -1.0, -1.0);
	VSET(h, 1.0, 1.0, 200.0);

	if (NEAR_ZERO(gdvp->gdv_view->gv_eye_pos[Z] - 1.0, SMALL_FASTF)) {
	    /* This way works, with reasonable Z-clipping */
	    ged_persp_mat(perspective_mat, gdvp->gdv_view->gv_perspective,
			  (fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f);
	} else {
	    /* This way does not have reasonable Z-clipping,
	     * but includes shear, for GDurf's testing.
	     */
	    ged_deering_persp_mat(perspective_mat, l, h, gdvp->gdv_view->gv_eye_pos);
	}
#else
	/*
	 * There are two strategies that could be used:
	 *
	 * 1) Assume a standard head location w.r.t. the screen, and
	 * fix the perspective angle.
	 *
	 * 2) Based upon the perspective angle, compute where the head
	 * should be to achieve that field of view.
	 *
	 * Try strategy #2 for now.
	 */
	fastf_t to_eye_scr;	/* screen space dist to eye */
	point_t l, h, eye;

	/* Determine where eye should be */
	to_eye_scr = 1 / tan(gdvp->gdv_view->gv_perspective * bn_degtorad * 0.5);

	VSET(l, -1.0, -1.0, -1.0);
	VSET(h, 1.0, 1.0, 200.0);
	VSET(eye, 0.0, 0.0, to_eye_scr);

	/* Non-stereo case */
	ged_mike_persp_mat(perspective_mat, gdvp->gdv_view->gv_eye_pos);
#endif

	bn_mat_mul(new, perspective_mat, mat);
	mat = new;
    }

    DM_LOADMATRIX(gdvp->gdv_dmp, mat, 0);
    go_drawDList(gdvp->gdv_dmp, &gdvp->gdv_gop->go_gedp->ged_gdp->gd_headDisplay);
}

HIDDEN void
go_draw_faceplate(struct ged_dm_view *gdvp)
{
    /* Center dot */
    if (gdvp->gdv_view->gv_center_dot.gos_draw) {
	DM_SET_FGCOLOR(gdvp->gdv_dmp,
		       gdvp->gdv_view->gv_center_dot.gos_line_color[0],
		       gdvp->gdv_view->gv_center_dot.gos_line_color[1],
		       gdvp->gdv_view->gv_center_dot.gos_line_color[2],
		       1, 1.0);
	DM_DRAW_POINT_2D(gdvp->gdv_dmp, 0.0, 0.0);
    }

    /* Model axes */
    if (gdvp->gdv_view->gv_model_axes.gas_draw) {
	point_t map;
	point_t save_map;

	VMOVE(save_map, gdvp->gdv_view->gv_model_axes.gas_axes_pos);
	VSCALE(map, gdvp->gdv_view->gv_model_axes.gas_axes_pos, go_current_gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	MAT4X3PNT(gdvp->gdv_view->gv_model_axes.gas_axes_pos, gdvp->gdv_view->gv_model2view, map);

	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_model_axes);

	VMOVE(gdvp->gdv_view->gv_model_axes.gas_axes_pos, save_map);
    }

    /* View axes */
    if (gdvp->gdv_view->gv_view_axes.gas_draw) {
	fastf_t inv_aspect;
	fastf_t save_ypos;

	save_ypos = gdvp->gdv_view->gv_view_axes.gas_axes_pos[Y];
	inv_aspect = (fastf_t)gdvp->gdv_dmp->dm_height / (fastf_t)gdvp->gdv_dmp->dm_width;
	gdvp->gdv_view->gv_view_axes.gas_axes_pos[Y] = save_ypos * inv_aspect;
	
	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_view_axes);

	gdvp->gdv_view->gv_view_axes.gas_axes_pos[Y] = save_ypos;
    }

    /* View scale */
    if (gdvp->gdv_view->gv_view_scale.gos_draw)
	dm_draw_scale(gdvp->gdv_dmp,
		      gdvp->gdv_view->gv_size,
		      gdvp->gdv_view->gv_view_scale.gos_line_color,
		      gdvp->gdv_view->gv_view_scale.gos_text_color);

    /* View parameters */
    if (gdvp->gdv_view->gv_view_params.gos_draw) {
	struct bu_vls vls;
	point_t center;
	char *ustr;

	MAT_DELTAS_GET_NEG(center, gdvp->gdv_view->gv_center);
	VSCALE(center, center, go_current_gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

	bu_vls_init(&vls);
	ustr = (char *)bu_units_string(go_current_gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	bu_vls_printf(&vls, "units:%s  size:%.2f  center:(%.2f, %.2f, %.2f)  az:%.2f  el:%.2f  tw::%.2f",
		      ustr,
		      gdvp->gdv_view->gv_size * go_current_gop->go_gedp->ged_wdbp->dbip->dbi_base2local,
		      V3ARGS(center),
		      V3ARGS(gdvp->gdv_view->gv_aet));
	DM_SET_FGCOLOR(gdvp->gdv_dmp,
		       gdvp->gdv_view->gv_view_params.gos_text_color[0],
		       gdvp->gdv_view->gv_view_params.gos_text_color[1],
		       gdvp->gdv_view->gv_view_params.gos_text_color[2],
		       1, 1.0);
	DM_DRAW_STRING_2D(gdvp->gdv_dmp, bu_vls_addr(&vls), -0.98, -0.965, 10, 0);
	bu_vls_free(&vls);
    }

    /* Draw the angle distance cursor */
    if (gdvp->gdv_view->gv_adc.gas_draw)
	dm_draw_adc(gdvp->gdv_dmp, gdvp->gdv_view);

    /* Draw grid */
    if (gdvp->gdv_view->gv_grid.ggs_draw)
	dm_draw_grid(gdvp->gdv_dmp, &gdvp->gdv_view->gv_grid, gdvp->gdv_view, gdvp->gdv_gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

    /* Draw rect */
    if (gdvp->gdv_view->gv_rect.grs_draw)
	dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect, gdvp->gdv_view);
}

HIDDEN void
go_refresh_view(struct ged_dm_view *gdvp)
{
    int restore_zbuffer = 0;

    if (!go_current_gop->go_refresh_on)
	return;

    /* Turn off the zbuffer if the framebuffer is active AND the zbuffer is on. */
    if (gdvp->gdv_fbs.fbs_mode != GED_OBJ_FB_MODE_OFF &&
	gdvp->gdv_dmp->dm_zbuffer) {
	DM_SET_ZBUFFER(gdvp->gdv_dmp, 0);
	restore_zbuffer = 1;
    }

    /*XXX Need to check if window is even visible before drawing */

    DM_DRAW_BEGIN(gdvp->gdv_dmp);

    if (gdvp->gdv_fbs.fbs_mode == GED_OBJ_FB_MODE_OVERLAY) {
	if (gdvp->gdv_view->gv_rect.grs_draw) {
	    go_draw(gdvp);

	    /* Restore to non-rotated, full brightness */
	    DM_NORMAL(gdvp->gdv_dmp);

	    go_draw_faceplate(gdvp);

	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.grs_pos[X], gdvp->gdv_view->gv_rect.grs_pos[Y],
		       gdvp->gdv_view->gv_rect.grs_dim[X], gdvp->gdv_view->gv_rect.grs_dim[Y]);
	    dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect, gdvp->gdv_view);
	} else
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);

	DM_DRAW_END(gdvp->gdv_dmp);
	return;
    } else if (gdvp->gdv_fbs.fbs_mode == GED_OBJ_FB_MODE_INTERLAY) {
	go_draw(gdvp);

	if (gdvp->gdv_view->gv_rect.grs_draw) {
	    go_draw(gdvp);
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.grs_pos[X], gdvp->gdv_view->gv_rect.grs_pos[Y],
		       gdvp->gdv_view->gv_rect.grs_dim[X], gdvp->gdv_view->gv_rect.grs_dim[Y]);
	} else
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);
    } else {
	if (gdvp->gdv_fbs.fbs_mode == GED_OBJ_FB_MODE_UNDERLAY) {
	    if (gdvp->gdv_view->gv_rect.grs_draw) {
		fb_refresh(gdvp->gdv_fbs.fbs_fbp,
			   gdvp->gdv_view->gv_rect.grs_pos[X], gdvp->gdv_view->gv_rect.grs_pos[Y],
			   gdvp->gdv_view->gv_rect.grs_dim[X], gdvp->gdv_view->gv_rect.grs_dim[Y]);
	    } else
		fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
			   gdvp->gdv_dmp->dm_width, gdvp->gdv_dmp->dm_height);
	}

	go_draw(gdvp);
    }

    if (gdvp->gdv_view->gv_data_axes.gas_draw && gdvp->gdv_view->gv_data_axes.gas_num_data_points) {
	dm_draw_data_axes(gdvp->gdv_dmp,
			  gdvp->gdv_view->gv_size,
			  bn_mat_identity,
			  &gdvp->gdv_view->gv_data_axes);
    }

    /* Restore to non-rotated, full brightness */
    DM_NORMAL(gdvp->gdv_dmp);
    go_draw_faceplate(gdvp);

    /* Draw labels */
    if (gdvp->gdv_view->gv_prim_labels.gos_draw) {
	register int i;

	for (i = 0; i < go_current_gop->go_prim_label_list_size; ++i) {
	    dm_draw_labels(gdvp->gdv_dmp,
			   go_current_gop->go_gedp->ged_wdbp,
			   bu_vls_addr(&go_current_gop->go_prim_label_list[i]),
			   gdvp->gdv_view->gv_model2view,
			   gdvp->gdv_view->gv_prim_labels.gos_text_color,
			   NULL, NULL);
	}
    }

    DM_DRAW_END(gdvp->gdv_dmp);

    if (restore_zbuffer) {
	DM_SET_ZBUFFER(gdvp->gdv_dmp, 1);
    }
}

HIDDEN void
go_refresh_handler(void *clientdata)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientdata;

    go_refresh_view(gdvp);
}

HIDDEN void
go_refresh_all_views(struct ged_obj *gop)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &gop->go_head_views.l)) {
	go_refresh_view(gdvp);
    }
}

HIDDEN void
go_autoview_view(struct ged_dm_view *gdvp)
{
    int ret;
    char *av[2];

    gdvp->gdv_gop->go_gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "autoview";
    av[1] = (char *)0;
    ret = ged_autoview(gdvp->gdv_gop->go_gedp, 1, (const char **)av);

    if (ret == GED_OK)
	go_refresh_view(gdvp);
}

HIDDEN void
go_autoview_all_views(struct ged_obj *gop)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &gop->go_head_views.l)) {
	go_autoview_view(gdvp);
    }
}

HIDDEN void
go_output_handler(struct ged *gedp, char *line)
{
    if (gedp->ged_output_script != (char *)0) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%s \"%s\"", gedp->ged_output_script, line);
	Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    } else {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "puts \"%s\"", line);
	Tcl_Eval(go_current_gop->go_interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
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
