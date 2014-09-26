/*                       T C L C A D _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2014 United States Government as represented by
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
/** @file libtclcad/tclcad_obj.c
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

#include <zlib.h>
#include <png.h>

#include "tcl.h"

#include "bn.h"
#include "bu/cmd.h"
#include "bu/units.h"
#include "vmath.h"
#include "db.h"
#include "rtgeom.h"
#include "wdb.h"
#include "mater.h"
#include "tclcad.h"

#include "solid.h"
#include "dm.h"
#include "dm/bview.h"
#include "obj.h"

#include "ged.h"

#include "fb.h"

#include "dm/dm_xvars.h"

#ifdef DM_X
#  ifdef WITH_TK
#    include "tk.h"
#endif
#  include <X11/Xutil.h>
#  include "dm/dm_xvars.h"
#endif /* DM_X */

#ifdef DM_TK
#  ifdef WITH_TK
#    include "tk.h"
#  endif
#  include "dm/dm_xvars.h"
#endif /* DM_TK */

#ifdef DM_OGL
#  include "fb/fb_ogl.h"
#endif /* DM_OGL */

#ifdef DM_OSG
#  include "dm/dm_xvars.h"
#endif /* DM_OSG */

#ifdef DM_OSGL
#  include "dm/dm_xvars.h"
#endif /* DM_OSGL */

#ifdef DM_WGL
#  include <tkwinport.h>
#  include "fb/fb_wgl.h"
#  include "dm/dm_xvars.h"
#endif /* DM_WGL */

#ifdef DM_QT
#  include "dm/dm_xvars.h"
#endif /* DM_QT */

/* Private headers */
#include "tclcad_private.h"

#include "brlcad_version.h"


#define TO_UNLIMITED -1

/*
 * Weird upper limit from clipper ---> sqrt(2^63 -1)/2
 * Probably should be sqrt(2^63 -1)
 */
#define CLIPPER_MAX 1518500249

HIDDEN int to_autoview(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr func,
		       const char *usage,
		       int maxargs);
HIDDEN int to_axes(struct ged *gedp,
		   struct ged_dm_view *gdvp,
		   struct bview_axes_state *gasp,
		   int argc,
		   const char *argv[],
		   const char *usage);
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
HIDDEN int to_blast(struct ged *gedp,
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
HIDDEN int to_data_arrows(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int to_data_axes(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int to_data_labels(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int to_data_lines(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int to_data_polygons(struct ged *gedp,
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
HIDDEN int to_data_move_object_mode(struct ged *gedp,
				    int argc,
				    const char *argv[],
				    ged_func_ptr func,
				    const char *usage,
				    int maxargs);
HIDDEN int to_data_move_point_mode(struct ged *gedp,
				   int argc,
				   const char *argv[],
				   ged_func_ptr func,
				   const char *usage,
				   int maxargs);
HIDDEN int to_data_pick(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int to_data_vZ(struct ged *gedp,
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
HIDDEN int to_get_prev_mouse(struct ged *gedp,
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
HIDDEN int to_faceplate(struct ged *gedp,
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
HIDDEN int to_handle_refresh(struct ged *gedp,
			     const char *name);
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
HIDDEN int to_is_viewable(struct ged_dm_view *gdvp);
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
HIDDEN int to_listen(struct ged *gedp,
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
HIDDEN int to_model_axes(struct ged *gedp,
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
HIDDEN int to_mouse_append_pt_common(struct ged *gedp,
				     int argc,
				     const char *argv[],
				     ged_func_ptr func,
				     const char *usage,
				     int maxargs);
HIDDEN int to_mouse_brep_selection_append(struct ged *gedp,
					  int argc,
					  const char *argv[],
					  ged_func_ptr func,
					  const char *usage,
					  int maxargs);
HIDDEN int to_mouse_brep_selection_translate(struct ged *gedp,
					     int argc,
					     const char *argv[],
					     ged_func_ptr func,
					     const char *usage,
					     int maxargs);
HIDDEN int to_mouse_constrain_rot(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int to_mouse_constrain_trans(struct ged *gedp,
				    int argc,
				    const char *argv[],
				    ged_func_ptr func,
				    const char *usage,
				    int maxargs);
HIDDEN int to_mouse_find_arb_edge(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int to_mouse_find_bot_edge(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int to_mouse_find_botpt(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int to_mouse_find_metaballpt(struct ged *gedp,
				    int argc,
				    const char *argv[],
				    ged_func_ptr func,
				    const char *usage,
				    int maxargs);
HIDDEN int to_mouse_find_pipept(struct ged *gedp,
				int argc,
				const char *argv[],
				ged_func_ptr func,
				const char *usage,
				int maxargs);
HIDDEN int to_mouse_joint_select(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *usage,
				 int maxargs);
HIDDEN int to_mouse_joint_selection_translate(struct ged *gedp,
					      int argc,
					      const char *argv[],
					      ged_func_ptr func,
					      const char *usage,
					      int maxargs);
HIDDEN int to_mouse_move_arb_edge(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int to_mouse_move_arb_face(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int to_mouse_move_botpt(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int to_mouse_move_botpts(struct ged *gedp,
				int argc,
				const char *argv[],
				ged_func_ptr func,
				const char *usage,
				int maxargs);
HIDDEN int to_mouse_move_pt_common(struct ged *gedp,
				   int argc,
				   const char *argv[],
				   ged_func_ptr func,
				   const char *usage,
				   int maxargs);
HIDDEN int to_mouse_orotate(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int to_mouse_oscale(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int to_mouse_otranslate(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int to_mouse_poly_circ(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int to_mouse_poly_cont(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int to_mouse_poly_ell(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int to_mouse_poly_rect(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int to_mouse_ray(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int to_mouse_rect(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int to_mouse_rot(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int to_mouse_rotate_arb_face(struct ged *gedp,
				    int argc,
				    const char *argv[],
				    ged_func_ptr func,
				    const char *usage,
				    int maxargs);
HIDDEN int to_mouse_data_scale(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int to_mouse_scale(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int to_mouse_protate(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int to_mouse_pscale(struct ged *gedp,
			   int argc,
			   const char *argv[],
			   ged_func_ptr func,
			   const char *usage,
			   int maxargs);
HIDDEN int to_mouse_ptranslate(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int to_mouse_trans(struct ged *gedp,
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
HIDDEN int to_move_botpt(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int to_move_botpts(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int to_move_botpt_mode(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int to_move_botpts_mode(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int to_move_metaballpt_mode(struct ged *gedp,
				   int argc,
				   const char *argv[],
				   ged_func_ptr func,
				   const char *usage,
				   int maxargs);
HIDDEN int to_move_pipept_mode(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs);
HIDDEN int to_move_pt_common(struct ged *gedp,
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
#if defined(DM_OGL) || defined(DM_WGL)
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

HIDDEN int to_poly_circ_mode(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int to_poly_cont_build(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr func,
			      const char *usage,
			      int maxargs);
HIDDEN int to_poly_cont_build_end(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr func,
				  const char *usage,
				  int maxargs);
HIDDEN int to_poly_ell_mode(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int to_poly_rect_mode(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int to_prim_label(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
HIDDEN int to_redraw(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *usage,
		     int maxargs);
HIDDEN int to_rect_mode(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int to_refresh(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs);
HIDDEN int to_refresh_all(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
HIDDEN int to_refresh_on(struct ged *gedp,
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
HIDDEN int to_set_fb_mode(struct ged *gedp,
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
HIDDEN int to_view_axes(struct ged *gedp,
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

/* Wrapper Functions */

HIDDEN int to_autoview_func(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);
HIDDEN int to_more_args_func(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int to_pass_through_func(struct ged *gedp,
				int argc,
				const char *argv[],
				ged_func_ptr func,
				const char *usage,
				int maxargs);
HIDDEN int to_pass_through_and_refresh_func(struct ged *gedp,
					    int argc,
					    const char *argv[],
					    ged_func_ptr func,
					    const char *usage,
					    int maxargs);
HIDDEN int to_view_func(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
HIDDEN int to_view_func_common(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr func,
			       const char *usage,
			       int maxargs,
			       int cflag,
			       int rflag);
HIDDEN int to_view_func_less(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int to_view_func_plus(struct ged *gedp,
			     int argc,
			     const char *argv[],
			     ged_func_ptr func,
			     const char *usage,
			     int maxargs);
HIDDEN int to_dm_func(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs);

/* Utility Functions */
HIDDEN int to_close_fbs(struct ged_dm_view *gdvp);
HIDDEN void to_dm_get_display_image(struct ged *gedp, unsigned char **idata);
HIDDEN void to_fbs_callback();
HIDDEN int to_open_fbs(struct ged_dm_view *gdvp, Tcl_Interp *interp);

HIDDEN void to_create_vlist_callback(struct display_list *gdlp);
HIDDEN void to_free_vlist_callback(unsigned int dlist, int range);
HIDDEN void to_refresh_all_views(struct tclcad_obj *top);
HIDDEN void to_refresh_view(struct ged_dm_view *gdvp);
HIDDEN void to_refresh_handler(void *clientdata);
HIDDEN void to_autoview_view(struct ged_dm_view *gdvp, const char *scale);
HIDDEN void to_autoview_all_views(struct tclcad_obj *top);
HIDDEN void to_rt_end_callback_internal(int aborted);

HIDDEN void to_output_handler(struct ged *gedp, char *line);
HIDDEN int to_log_output_handler(void *client_data, void *vpstr);

HIDDEN int to_edit_redraw(struct ged *gedp, int argc, const char *argv[]);

typedef int (*to_wrapper_func_ptr)(struct ged *, int, const char *[], ged_func_ptr, const char *, int);
#define TO_WRAPPER_FUNC_PTR_NULL (to_wrapper_func_ptr)0

static struct tclcad_obj HeadTclcadObj;
static struct tclcad_obj *current_top = TCLCAD_OBJ_NULL;

struct path_edit_params {
    int edit_mode;
    mat_t edit_mat;
};


struct to_cmdtab {
    char *to_name;
    char *to_usage;
    int to_maxargs;
    to_wrapper_func_ptr to_wrapper_func;
    ged_func_ptr to_func;
};


static struct to_cmdtab to_cmds[] = {
    {"3ptarb",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_3ptarb},
    {"adc",	"args", 7, to_view_func, ged_adc},
    {"adjust",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_adjust},
    {"ae2dir",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ae2dir},
    {"aet",	"[[-i] az el [tw]]", 6, to_view_func_plus, ged_aet},
    {"analyze",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_analyze},
    {"annotate", (char *)0, TO_UNLIMITED, to_pass_through_func, ged_annotate},
    {"append_pipept",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_append_pipept},
    {"arb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_arb},
    {"arced",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_arced},
    {"arot",	"x y z angle", 6, to_view_func_plus, ged_arot},
    {"attr",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_attr},
    {"autoview",	"vname", TO_UNLIMITED, to_autoview, GED_FUNC_PTR_NULL},
    {"bb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bb},
    {"bev",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bev},
    {"base2local",	(char *)0, TO_UNLIMITED, to_base2local, GED_FUNC_PTR_NULL},
    {"bg",	"[r g b]", TO_UNLIMITED, to_bg, GED_FUNC_PTR_NULL},
    {"blast",	(char *)0, TO_UNLIMITED, to_blast, GED_FUNC_PTR_NULL},
    {"bo",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bo},
    {"bot",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot},
    {"bot_condense",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_condense},
    {"bot_decimate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_decimate},
    {"bot_dump",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_dump},
    {"bot_face_fuse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_face_fuse},
    {"bot_face_sort",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_face_sort},
    {"bot_edge_split",	"bot face", TO_UNLIMITED, to_bot_edge_split, GED_FUNC_PTR_NULL},
    {"bot_face_split",	"bot face", TO_UNLIMITED, to_bot_face_split, GED_FUNC_PTR_NULL},
    {"bot_flip",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_flip},
    {"bot_fuse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_fuse},
    {"bot_merge",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_merge},
    {"bot_smooth",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_smooth},
    {"bot_split",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_split},
    {"bot_sync",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_sync},
    {"bot_vertex_fuse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_bot_vertex_fuse},
    {"bounds",	"[\"minX maxX minY maxY minZ maxZ\"]", TO_UNLIMITED, to_bounds, GED_FUNC_PTR_NULL},
    {"brep",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_brep},
    {"c",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_comb_std},
    {"cat",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_cat},
    {"center",	"[x y z]", 5, to_view_func_plus, ged_center},
    {"clear",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_zap},
    {"clone",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_clone},
    {"coil",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_coil},
    {"color",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_color},
    {"comb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_comb},
    {"comb_color",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_comb_color},
    {"combmem",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_combmem},
    {"configure",	"vname", TO_UNLIMITED, to_configure, GED_FUNC_PTR_NULL},
    {"constrain_rmode",	"x|y|z x y", TO_UNLIMITED, to_constrain_rmode, GED_FUNC_PTR_NULL},
    {"constrain_tmode",	"x|y|z x y", TO_UNLIMITED, to_constrain_tmode, GED_FUNC_PTR_NULL},
    {"constraint", (char *)0, TO_UNLIMITED, to_pass_through_func, ged_constraint},
    {"copyeval",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_copyeval},
    {"copymat",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_copymat},
    {"cp",	"[-f] from to", TO_UNLIMITED, to_copy, GED_FUNC_PTR_NULL},
    {"cpi",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_cpi},
    {"d",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_erase},
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
    {"dbconcat",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_concat},
    {"dbfind",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_find},
    {"dbip",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dbip},
    {"dbot_dump",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dbot_dump},
    {"debugbu", 	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debugbu},
    {"debugdir",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debugdir},
    {"debuglib",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debuglib},
    {"debugmem",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debugmem},
    {"debugnmg",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_debugnmg},
    {"decompose",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_decompose},
    {"delay",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_delay},
    {"delete_metaballpt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_delete_metaballpt},
    {"delete_pipept",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_delete_pipept},
    {"delete_view",	"vname", TO_UNLIMITED, to_delete_view, GED_FUNC_PTR_NULL},
    {"dir2ae",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_dir2ae},
    {"dlist_on",	"[0|1]", TO_UNLIMITED, to_dlist_on, GED_FUNC_PTR_NULL},
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
    {"erase",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_erase},
    {"ev",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_ev},
    {"expand",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_expand},
    {"eye",	"[x y z]", 5, to_view_func_plus, ged_eye},
    {"eye_pos",	"[x y z]", 5, to_view_func_plus, ged_eye_pos},
    {"eye_pt",	"[x y z]", 5, to_view_func_plus, ged_eye},
    {"exists",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_exists},
    {"faceplate",	"center_dot|prim_labels|view_params|view_scale color|draw [val(s)]", TO_UNLIMITED, to_faceplate, GED_FUNC_PTR_NULL},
    {"facetize",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_facetize},
    {"voxelize",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_voxelize},
    {"fb2pix",  	"[-h -i -c] [-s squaresize] [-w width] [-n height] [file.pix]", TO_UNLIMITED, to_view_func, ged_fb2pix},
    {"fbclear",  	"[r g b]", TO_UNLIMITED, to_view_func, ged_fbclear},
    {"find_arb_edge",	"arb vx vy ptol", 5, to_view_func, ged_find_arb_edge_nearest_pt},
    {"find_bot_edge",	"bot vx vy", 5, to_view_func, ged_find_bot_edge_nearest_pt},
    {"find_botpt",	"bot vx vy", 5, to_view_func, ged_find_botpt_nearest_pt},
    {"find_pipept",	"pipe x y z", 6, to_view_func, ged_find_pipept_nearest_pt},
    {"fontsize",	"[fontsize]", 3, to_fontsize, GED_FUNC_PTR_NULL},
    {"form",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_form},
    {"fracture",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_fracture},
    {"g",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_group},
    {"gdiff",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_gdiff},
    {"get",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get},
    {"get_autoview",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_autoview},
    {"get_bot_edges",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_bot_edges},
    {"get_comb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_comb},
    {"get_eyemodel",	"vname", 2, to_view_func, ged_get_eyemodel},
    {"get_prev_mouse",	"vname", TO_UNLIMITED, to_get_prev_mouse, GED_FUNC_PTR_NULL},
    {"get_type",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_get_type},
    {"glob",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_glob},
    {"gqa",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_gqa},
    {"graph",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_graph},
    {"grid",	"args", 6, to_view_func, ged_grid},
    {"grid2model_lu",	"x y", 4, to_view_func_less, ged_grid2model_lu},
    {"grid2view_lu",	"x y", 4, to_view_func_less, ged_grid2view_lu},
    {"handle_expose",	"vname count", TO_UNLIMITED, to_handle_expose, GED_FUNC_PTR_NULL},
    {"hide",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_hide},
    {"hide_view",	"vname [0|1]", 3, to_hide_view, GED_FUNC_PTR_NULL},
    {"how",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_how},
    {"human",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_human},
    {"i",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_instance},
    {"idents",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tables},
    {"idle_mode",	"vname", TO_UNLIMITED, to_idle_mode, GED_FUNC_PTR_NULL},
    {"illum",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_illum},
    {"importFg4Section",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_importFg4Section},
    {"in",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_in},
    {"init_view_bindings",	"vname", TO_UNLIMITED, to_init_view_bindings, GED_FUNC_PTR_NULL},
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
    {"light",	"[0|1]", TO_UNLIMITED, to_light, GED_FUNC_PTR_NULL},
    {"list_views",	(char *)0, TO_UNLIMITED, to_list_views, GED_FUNC_PTR_NULL},
    {"listen",	"[port]", TO_UNLIMITED, to_listen, GED_FUNC_PTR_NULL},
    {"listeval",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pathsum},
    {"loadview",	"filename", 3, to_view_func, ged_loadview},
    {"local2base",	(char *)0, TO_UNLIMITED, to_local2base, GED_FUNC_PTR_NULL},
    {"lod",	(char *)0, TO_UNLIMITED, to_lod, ged_lod},
    {"log",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_log},
    {"lookat",	"x y z", 5, to_view_func_plus, ged_lookat},
    {"ls",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ls},
    {"lt",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_lt},
    {"m2v_point",	"x y z", 5, to_view_func, ged_m2v_point},
    {"make",	(char *)0, TO_UNLIMITED, to_make, GED_FUNC_PTR_NULL},
    {"make_name",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_make_name},
    {"make_pnts",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_make_pnts},
    {"match",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_match},
    {"mater",	(char *)0, TO_UNLIMITED, to_more_args_func, ged_mater},
    {"memprint",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_memprint},
    {"mirror",	(char *)0, TO_UNLIMITED, to_mirror, GED_FUNC_PTR_NULL},
    {"model2grid_lu",	"x y z", 5, to_view_func_less, ged_model2grid_lu},
    {"model2view",	"vname", 2, to_view_func, ged_model2view},
    {"model2view_lu",	"x y z", 5, to_view_func_less, ged_model2view_lu},
    {"model_axes",	"???", TO_UNLIMITED, to_model_axes, GED_FUNC_PTR_NULL},
    {"edit_motion_delta_callback",	"vname [args]", TO_UNLIMITED, to_edit_motion_delta_callback, GED_FUNC_PTR_NULL},
    {"more_args_callback",	"set/get the \"more args\" callback", TO_UNLIMITED, to_more_args_callback, GED_FUNC_PTR_NULL},
    {"move_arb_edge",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move_arb_edge},
    {"move_arb_edge_mode",	"obj edge x y", TO_UNLIMITED, to_move_arb_edge_mode, GED_FUNC_PTR_NULL},
    {"move_arb_face",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move_arb_face},
    {"move_arb_face_mode",	"obj face x y", TO_UNLIMITED, to_move_arb_face_mode, GED_FUNC_PTR_NULL},
    {"move_botpt",	(char *)0, TO_UNLIMITED, to_move_botpt, GED_FUNC_PTR_NULL},
    {"move_botpts",	(char *)0, TO_UNLIMITED, to_move_botpts, GED_FUNC_PTR_NULL},
    {"move_botpt_mode",	"obj i mx my", TO_UNLIMITED, to_move_botpt_mode, GED_FUNC_PTR_NULL},
    {"move_botpts_mode",	"mx my obj i1 [i2 ... iN]", TO_UNLIMITED, to_move_botpts_mode, GED_FUNC_PTR_NULL},
    {"move_metaballpt",	(char *)0, TO_UNLIMITED, to_move_pt_common, ged_move_metaballpt},
    {"move_metaballpt_mode",	"obj pt_i mx my", TO_UNLIMITED, to_move_metaballpt_mode, GED_FUNC_PTR_NULL},
    {"move_pipept",	(char *)0, TO_UNLIMITED, to_move_pt_common, ged_move_pipept},
    {"move_pipept_mode",	"obj seg_i mx my", TO_UNLIMITED, to_move_pipept_mode, GED_FUNC_PTR_NULL},
    {"mouse_add_metaballpt",	"obj mx my", TO_UNLIMITED, to_mouse_append_pt_common, ged_add_metaballpt},
    {"mouse_append_pipept",	"obj mx my", TO_UNLIMITED, to_mouse_append_pt_common, ged_append_pipept},
    {"mouse_brep_selection_append", "obj mx my", 5, to_mouse_brep_selection_append, GED_FUNC_PTR_NULL},
    {"mouse_brep_selection_translate", "obj mx my", 5, to_mouse_brep_selection_translate, GED_FUNC_PTR_NULL},
    {"mouse_constrain_rot",	"coord mx my", TO_UNLIMITED, to_mouse_constrain_rot, GED_FUNC_PTR_NULL},
    {"mouse_constrain_trans",	"coord mx my", TO_UNLIMITED, to_mouse_constrain_trans, GED_FUNC_PTR_NULL},
    {"mouse_data_scale",	"mx my", TO_UNLIMITED, to_mouse_data_scale, GED_FUNC_PTR_NULL},
    {"mouse_find_arb_edge",	"obj mx my ptol", TO_UNLIMITED, to_mouse_find_arb_edge, GED_FUNC_PTR_NULL},
    {"mouse_find_bot_edge",	"obj mx my", TO_UNLIMITED, to_mouse_find_bot_edge, GED_FUNC_PTR_NULL},
    {"mouse_find_botpt",	"obj mx my", TO_UNLIMITED, to_mouse_find_botpt, GED_FUNC_PTR_NULL},
    {"mouse_find_metaballpt",	"obj mx my", TO_UNLIMITED, to_mouse_find_metaballpt, GED_FUNC_PTR_NULL},
    {"mouse_find_pipept",	"obj mx my", TO_UNLIMITED, to_mouse_find_pipept, GED_FUNC_PTR_NULL},
    {"mouse_joint_select", "obj mx my", 5, to_mouse_joint_select, GED_FUNC_PTR_NULL},
    {"mouse_joint_selection_translate", "obj mx my", 5, to_mouse_joint_selection_translate, GED_FUNC_PTR_NULL},
    {"mouse_move_arb_edge",	"obj edge mx my", TO_UNLIMITED, to_mouse_move_arb_edge, GED_FUNC_PTR_NULL},
    {"mouse_move_arb_face",	"obj face mx my", TO_UNLIMITED, to_mouse_move_arb_face, GED_FUNC_PTR_NULL},
    {"mouse_move_botpt",	"[-r] obj i mx my", TO_UNLIMITED, to_mouse_move_botpt, GED_FUNC_PTR_NULL},
    {"mouse_move_botpts",	"mx my obj i1 [i2 ... iN]", TO_UNLIMITED, to_mouse_move_botpts, GED_FUNC_PTR_NULL},
    {"mouse_move_metaballpt",	"obj i mx my", TO_UNLIMITED, to_mouse_move_pt_common, ged_move_metaballpt},
    {"mouse_move_pipept",	"obj i mx my", TO_UNLIMITED, to_mouse_move_pt_common, ged_move_pipept},
    {"mouse_orotate",	"obj mx my", TO_UNLIMITED, to_mouse_orotate, GED_FUNC_PTR_NULL},
    {"mouse_oscale",	"obj mx my", TO_UNLIMITED, to_mouse_oscale, GED_FUNC_PTR_NULL},
    {"mouse_otranslate",	"obj mx my", TO_UNLIMITED, to_mouse_otranslate, GED_FUNC_PTR_NULL},
    {"mouse_poly_circ",	"mx my", TO_UNLIMITED, to_mouse_poly_circ, GED_FUNC_PTR_NULL},
    {"mouse_poly_cont",	"mx my", TO_UNLIMITED, to_mouse_poly_cont, GED_FUNC_PTR_NULL},
    {"mouse_poly_ell",	"mx my", TO_UNLIMITED, to_mouse_poly_ell, GED_FUNC_PTR_NULL},
    {"mouse_poly_rect",	"mx my", TO_UNLIMITED, to_mouse_poly_rect, GED_FUNC_PTR_NULL},
    {"mouse_prepend_pipept",	"obj mx my", TO_UNLIMITED, to_mouse_append_pt_common, ged_prepend_pipept},
    {"mouse_ray",	"mx my", TO_UNLIMITED, to_mouse_ray, GED_FUNC_PTR_NULL},
    {"mouse_rect",	"mx my", TO_UNLIMITED, to_mouse_rect, GED_FUNC_PTR_NULL},
    {"mouse_rot",	"mx my", TO_UNLIMITED, to_mouse_rot, GED_FUNC_PTR_NULL},
    {"mouse_rotate_arb_face",	"obj face v mx my", TO_UNLIMITED, to_mouse_rotate_arb_face, GED_FUNC_PTR_NULL},
    {"mouse_scale",	"mx my", TO_UNLIMITED, to_mouse_scale, GED_FUNC_PTR_NULL},
    {"mouse_protate",	"obj attribute mx my", TO_UNLIMITED, to_mouse_protate, GED_FUNC_PTR_NULL},
    {"mouse_pscale",	"obj attribute mx my", TO_UNLIMITED, to_mouse_pscale, GED_FUNC_PTR_NULL},
    {"mouse_ptranslate",	"obj attribute mx my", TO_UNLIMITED, to_mouse_ptranslate, GED_FUNC_PTR_NULL},
    {"mouse_trans",	"mx my", TO_UNLIMITED, to_mouse_trans, GED_FUNC_PTR_NULL},
    {"mv",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move},
    {"mvall",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_move_all},
    {"new_view",	"vname type [args]", TO_UNLIMITED, to_new_view, GED_FUNC_PTR_NULL},
    {"nirt",	"[args]", TO_UNLIMITED, to_view_func, ged_nirt},
    {"nmg_collapse",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_nmg_collapse},
    {"nmg_fix_normals",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_nmg_fix_normals},
    {"nmg_simplify",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_nmg_simplify},
    {"ocenter",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ocenter},
    {"open",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_reopen},
    {"orient",	"quat", 6, to_view_func_plus, ged_orient},
    {"orientation",	"quat", 6, to_view_func_plus, ged_orient},
    {"orotate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_orotate},
    {"orotate_mode",	"obj x y", TO_UNLIMITED, to_orotate_mode, GED_FUNC_PTR_NULL},
    {"oscale",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_oscale},
    {"oscale_mode",	"obj x y", TO_UNLIMITED, to_oscale_mode, GED_FUNC_PTR_NULL},
    {"otranslate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_otranslate},
    {"otranslate_mode",	"obj x y", TO_UNLIMITED, to_otranslate_mode, GED_FUNC_PTR_NULL},
    {"overlay",	(char *)0, TO_UNLIMITED, to_autoview_func, ged_overlay},
    {"paint_rect_area",	"vname", TO_UNLIMITED, to_paint_rect_area, GED_FUNC_PTR_NULL},
    {"pathlist",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pathlist},
    {"paths",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pathsum},
    {"perspective",	"[angle]", 3, to_view_func_plus, ged_perspective},
    {"pix2fb",  	"[options] [file.pix]", TO_UNLIMITED, to_view_func, ged_pix2fb},
    {"plot",	"[options] file.pl", 16, to_view_func, ged_plot},
    {"pmat",	"[mat]", 3, to_view_func, ged_pmat},
    {"pmodel2view",	"vname", 2, to_view_func, ged_pmodel2view},
#if defined(DM_OGL) || defined(DM_WGL)
    {"pix",	"file", TO_UNLIMITED, to_pix, GED_FUNC_PTR_NULL},
    {"png",	"file", TO_UNLIMITED, to_png, GED_FUNC_PTR_NULL},
#endif
    {"pngwf",	"[options] file.png", 16, to_view_func, ged_png},
    {"poly_circ_mode",	"x y", TO_UNLIMITED, to_poly_circ_mode, GED_FUNC_PTR_NULL},
    {"poly_cont_build",	"x y", TO_UNLIMITED, to_poly_cont_build, GED_FUNC_PTR_NULL},
    {"poly_cont_build_end",	"y", TO_UNLIMITED, to_poly_cont_build_end, GED_FUNC_PTR_NULL},
    {"poly_ell_mode",	"x y", TO_UNLIMITED, to_poly_ell_mode, GED_FUNC_PTR_NULL},
    {"poly_rect_mode",	"x y [s]", TO_UNLIMITED, to_poly_rect_mode, GED_FUNC_PTR_NULL},
    {"polybinout",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_polybinout},
    {"pov",	"center quat scale eye_pos perspective", 7, to_view_func_plus, ged_pmat},
    {"prcolor",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_prcolor},
    {"prefix",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_prefix},
    {"prepend_pipept",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_prepend_pipept},
    {"preview",	"[options] script", TO_UNLIMITED, to_dm_func, ged_preview},
    {"prim_label",	"[prim_1 prim_2 ... prim_N]", TO_UNLIMITED, to_prim_label, GED_FUNC_PTR_NULL},
    {"protate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_protate},
    {"protate_mode",	"obj attribute x y", TO_UNLIMITED, to_protate_mode, GED_FUNC_PTR_NULL},
    {"ps",	"[options] file.ps", 16, to_view_func, ged_ps},
    {"pscale",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pscale},
    {"pscale_mode",	"obj attribute x y", TO_UNLIMITED, to_pscale_mode, GED_FUNC_PTR_NULL},
    {"pset",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_pset},
    {"ptranslate",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_ptranslate},
    {"ptranslate_mode",	"obj attribute x y", TO_UNLIMITED, to_ptranslate_mode, GED_FUNC_PTR_NULL},
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
    {"rect_mode",	"x y", TO_UNLIMITED, to_rect_mode, GED_FUNC_PTR_NULL},
    {"red",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_red},
    {"redraw",	"obj", 2, to_redraw, GED_FUNC_PTR_NULL},
    {"refresh",	"vname", TO_UNLIMITED, to_refresh, GED_FUNC_PTR_NULL},
    {"refresh_all",	(char *)0, TO_UNLIMITED, to_refresh_all, GED_FUNC_PTR_NULL},
    {"refresh_on",	"[0|1]", TO_UNLIMITED, to_refresh_on, GED_FUNC_PTR_NULL},
    {"regdef",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_regdef},
    {"regions",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tables},
    {"report",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_report},
    {"rfarb",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rfarb},
    {"rm",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_remove},
    {"rmap",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rmap},
    {"rmat",	"[mat]", 3, to_view_func, ged_rmat},
    {"rmater",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rmater},
    {"rot",	"[-m|-v] x y z", 6, to_view_func_plus, ged_rot},
    {"rot_about",	"[e|k|m|v]", 3, to_view_func, ged_rotate_about},
    {"rot_point",	"x y z", 5, to_view_func, ged_rot_point},
    {"rotate_arb_face",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rotate_arb_face},
    {"rotate_arb_face_mode",	"obj face v x y", TO_UNLIMITED, to_rotate_arb_face_mode, GED_FUNC_PTR_NULL},
    {"rotate_mode",	"x y", TO_UNLIMITED, to_rotate_mode, GED_FUNC_PTR_NULL},
    {"rrt",	"[args]", TO_UNLIMITED, to_view_func, ged_rrt},
    {"rselect",		(char *)0, TO_UNLIMITED, to_view_func, ged_rselect},
    {"rt",	"[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rt_end_callback",	"[args]", TO_UNLIMITED, to_rt_end_callback, GED_FUNC_PTR_NULL},
    {"rt_gettrees",	"[-i] [-u] pname object", TO_UNLIMITED, to_rt_gettrees, GED_FUNC_PTR_NULL},
    {"rtabort",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_rtabort},
    {"rtarea",	"[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rtcheck",	"[args]", TO_UNLIMITED, to_view_func, ged_rtcheck},
    {"rtedge",	"[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rtweight", "[args]", TO_UNLIMITED, to_view_func, ged_rt},
    {"rtwizard", "[args]", TO_UNLIMITED, to_view_func, ged_rtwizard},
    {"savekey",	"filename", 3, to_view_func, ged_savekey},
    {"saveview",	"filename", 3, to_view_func, ged_saveview},
    {"sca",	"sf", 3, to_view_func_plus, ged_scale},
    {"scale_mode",	"x y", TO_UNLIMITED, to_scale_mode, GED_FUNC_PTR_NULL},
    {"screen2model",	"x y", TO_UNLIMITED, to_screen2model, GED_FUNC_PTR_NULL},
    {"screen2view",	"x y", TO_UNLIMITED, to_screen2view, GED_FUNC_PTR_NULL},
    {"screengrab",	"imagename.ext", TO_UNLIMITED, to_dm_func, ged_screen_grab},
    {"sdata_arrows",	"???", TO_UNLIMITED, to_data_arrows, GED_FUNC_PTR_NULL},
    {"sdata_axes",	"???", TO_UNLIMITED, to_data_axes, GED_FUNC_PTR_NULL},
    {"sdata_labels",	"???", TO_UNLIMITED, to_data_labels, GED_FUNC_PTR_NULL},
    {"sdata_lines",	"???", TO_UNLIMITED, to_data_lines, GED_FUNC_PTR_NULL},
    {"sdata_polygons",	"???", TO_UNLIMITED, to_data_polygons, GED_FUNC_PTR_NULL},
    {"search",		(char *)0, TO_UNLIMITED, to_pass_through_func, ged_search},
    {"select",		(char *)0, TO_UNLIMITED, to_view_func, ged_select},
    {"set_coord",	"[m|v]", TO_UNLIMITED, to_set_coord, GED_FUNC_PTR_NULL},
    {"set_fb_mode",	"[mode]", TO_UNLIMITED, to_set_fb_mode, GED_FUNC_PTR_NULL},
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
    {"snap_view",	"vx vy", 4, to_snap_view, GED_FUNC_PTR_NULL},
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
    {"translate_mode",	"x y", TO_UNLIMITED, to_translate_mode, GED_FUNC_PTR_NULL},
    {"transparency",	"[val]", TO_UNLIMITED, to_transparency, GED_FUNC_PTR_NULL},
    {"tree",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_tree},
    {"unhide",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_unhide},
    {"units",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_units},
    {"v2m_point",	"x y z", 5, to_view_func, ged_v2m_point},
    {"vdraw",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_vdraw},
    {"version",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_version},
    {"view",	"quat|ypr|aet|center|eye|size [args]", 7, to_view_func_plus, ged_view_func},
    {"view_axes",	"vname [args]", TO_UNLIMITED, to_view_axes, GED_FUNC_PTR_NULL},
    {"view_callback",	"vname [args]", TO_UNLIMITED, to_view_callback, GED_FUNC_PTR_NULL},
    {"view_win_size",	"[s] | [x y]", 4, to_view_win_size, GED_FUNC_PTR_NULL},
    {"view2grid_lu",	"x y z", 5, to_view_func_less, ged_view2grid_lu},
    {"view2model",	"", 2, to_view_func_less, ged_view2model},
    {"view2model_lu",	"x y z", 5, to_view_func_less, ged_view2model_lu},
    {"view2model_vec",	"x y z", 5, to_view_func_less, ged_view2model_vec},
    {"view2screen",	"", 2, to_view2screen, GED_FUNC_PTR_NULL},
    {"viewdir",	"[-i]", 3, to_view_func_less, ged_viewdir},
    {"vmake",	"pname ptype", TO_UNLIMITED, to_vmake, GED_FUNC_PTR_NULL},
    {"vnirt",	"[args]", TO_UNLIMITED, to_view_func, ged_vnirt},
    {"vslew",	"x y", TO_UNLIMITED, to_vslew, GED_FUNC_PTR_NULL},
    {"wcodes",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_wcodes},
    {"whatid",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_whatid},
    {"which_shader",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_which_shader},
    {"whichair",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_which},
    {"whichid",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_which},
    {"who",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_who},
    {"wmater",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_wmater},
    {"x",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_report},
    {"xpush",	(char *)0, TO_UNLIMITED, to_pass_through_func, ged_xpush},
    {"ypr",	"yaw pitch roll", 5, to_view_func_plus, ged_ypr},
    {"zap",	(char *)0, TO_UNLIMITED, to_pass_through_and_refresh_func, ged_zap},
    {"zbuffer",	"[0|1]", TO_UNLIMITED, to_zbuffer, GED_FUNC_PTR_NULL},
    {"zclip",	"[0|1]", TO_UNLIMITED, to_zclip, GED_FUNC_PTR_NULL},
    {"zoom",	"sf", 3, to_view_func_plus, ged_zoom},
    {(char *)0,	(char *)0, 0, TO_WRAPPER_FUNC_PTR_NULL, GED_FUNC_PTR_NULL}
};


static fastf_t
screen_to_view_x(dm *dmp, fastf_t x)
{
    int width = dm_get_width(dmp);
    return x / (fastf_t)width * 2.0 - 1.0;
}


static fastf_t
screen_to_view_y(dm *dmp, fastf_t y)
{
    int height = dm_get_height(dmp);
    return (y / (fastf_t)height * -2.0 + 1.0) / dm_get_aspect(dmp);
}



/**
 * @brief
 * A TCL interface to dm_list_types()).
 *
 * @return a list of available dm types.
 */
int
dm_list_tcl(ClientData UNUSED(clientData),
	    Tcl_Interp *interp,
	    int UNUSED(argc),
	    const char **UNUSED(argv))
{
    struct bu_vls *list = dm_list_types(',');
    Tcl_SetResult(interp, bu_vls_addr(list), TCL_VOLATILE);
    bu_vls_free(list);
    BU_PUT(list, struct bu_vls);
    return TCL_OK;
}

/**
 * @brief create the Tcl command for to_open
 *
 */
int
Go_Init(Tcl_Interp *interp)
{
    if (library_initialized(0))
	return TCL_OK;

    {
	const char *version_str = brlcad_version();
	tclcad_eval_noresult(interp, "set brlcad_version", 1, &version_str);
    }

    /*XXX Use of brlcad_interp is temporary */
    brlcad_interp = interp;

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
    register struct to_cmdtab *ctp;
    struct tclcad_obj *top = (struct tclcad_obj *)clientData;
    Tcl_DString ds;
    int ret = GED_ERROR;

    Tcl_DStringInit(&ds);

    if (argc < 2) {
	Tcl_DStringAppend(&ds, "subcommand not specified; must be one of: ", -1);
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
	if (ctp->to_name[0] == argv[1][0] &&
	    BU_STR_EQUAL(ctp->to_name, argv[1])) {
	    struct ged *gedp = top->to_gop->go_gedp;
	    bu_log_add_hook(to_log_output_handler, (void *)gedp);
	    ret = (*ctp->to_wrapper_func)(gedp, argc-1, (const char **)argv+1, ctp->to_func, ctp->to_usage, ctp->to_maxargs);
	    bu_log_delete_hook(to_log_output_handler, (void *)gedp);
	    break;
	}
    }

    /* Command not found. */
    if (ctp->to_name == (char *)0) {
	Tcl_DStringAppend(&ds, "unknown subcommand: ", -1);
	Tcl_DStringAppend(&ds, argv[1], -1);
	Tcl_DStringAppend(&ds, "; must be one of: ", -1);

	for (ctp = to_cmds; ctp->to_name != (char *)NULL; ctp++) {
	    Tcl_DStringAppend(&ds, " ", -1);
	    Tcl_DStringAppend(&ds, ctp->to_name, -1);
	}
	Tcl_DStringAppend(&ds, "\n", -1);
	Tcl_DStringResult(interp, &ds);

	return TCL_ERROR;
    }

    Tcl_DStringAppend(&ds, bu_vls_addr(top->to_gop->go_gedp->ged_result_str), -1);
    Tcl_DStringResult(interp, &ds);

    if (ret & GED_ERROR)
	return TCL_ERROR;

    return TCL_OK;
}


HIDDEN int
free_path_edit_params_entry(struct bu_hash_entry *entry, void *UNUSED(udata))
{
    struct path_edit_params *pp = (struct path_edit_params *)bu_get_hash_value(entry);
    BU_PUT(pp, struct path_edit_params);
    return 0;
}


/**
 * @brief
 * Called by Tcl when the object is destroyed.
 */
void
to_deleteProc(ClientData clientData)
{
    struct tclcad_obj *top = (struct tclcad_obj *)clientData;
    struct ged_dm_view *gdvp;

    if (current_top == top)
	current_top = TCLCAD_OBJ_NULL;

    BU_LIST_DEQUEUE(&top->l);
    bu_vls_free(&top->to_gop->go_name);
    ged_close(top->to_gop->go_gedp);
    if (top->to_gop->go_gedp)
	BU_PUT(top->to_gop->go_gedp, struct ged);

    bu_hash_tbl_traverse(top->to_gop->go_edited_paths, free_path_edit_params_entry, NULL);
    bu_hash_tbl_free(top->to_gop->go_edited_paths);

    while (BU_LIST_WHILE(gdvp, ged_dm_view, &top->to_gop->go_head_views.l)) {
	BU_LIST_DEQUEUE(&(gdvp->l));
	bu_vls_free(&gdvp->gdv_name);
	bu_vls_free(&gdvp->gdv_callback);
	bu_vls_free(&gdvp->gdv_edit_motion_delta_callback);
	(void)dm_close(gdvp->gdv_dmp);
	bu_free((void *)gdvp->gdv_view, "ged_view");

	to_close_fbs(gdvp);

	bu_free((void *)gdvp, "ged_dm_view");
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
	    Tcl_AppendResult(interp, bu_vls_addr(&top->to_gop->go_name), " ", (char *)NULL);

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

    /* initialize tclcad_obj */
    BU_ALLOC(top, struct tclcad_obj);
    top->to_interp = interp;

    /* initialize ged_obj */
    BU_ALLOC(top->to_gop, struct ged_obj);

    BU_ASSERT_PTR(gedp, !=, NULL);
    top->to_gop->go_gedp = gedp;

    top->to_gop->go_gedp->ged_output_handler = to_output_handler;
    top->to_gop->go_gedp->ged_refresh_handler = to_refresh_handler;
    top->to_gop->go_gedp->ged_create_vlist_callback = to_create_vlist_callback;
    top->to_gop->go_gedp->ged_free_vlist_callback = to_free_vlist_callback;

    BU_ASSERT_PTR(gedp->ged_gdp, !=, NULL);
    top->to_gop->go_gedp->ged_gdp->gd_rtCmdNotify = to_rt_end_callback_internal;

    bu_vls_init(&top->to_gop->go_name);
    bu_vls_strcpy(&top->to_gop->go_name, argv[1]);
    bu_vls_init(&top->to_gop->go_more_args_callback);
    bu_vls_init(&top->to_gop->go_rt_end_callback);
    BU_LIST_INIT(&top->to_gop->go_observers.l);
    top->to_gop->go_refresh_on = 1;
    top->to_gop->go_edited_paths = bu_hash_tbl_create(0);

    BU_LIST_INIT(&top->to_gop->go_head_views.l);

    /* append to list of tclcad_obj */
    BU_LIST_APPEND(&HeadTclcadObj.l, &top->l);

    return to_create_cmd(interp, top, argv[1]);
}


/*************************** Local Command Functions ***************************/

HIDDEN int
to_autoview(struct ged *gedp,
	    int argc,
	    const char *argv[],
	    ged_func_ptr UNUSED(func),
	    const char *usage,
	    int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s [scale]", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argc > 2)
	to_autoview_view(gdvp, argv[2]);
    else
	to_autoview_view(gdvp, NULL);

    return GED_OK;
}


HIDDEN int
to_axes(struct ged *gedp,
	struct ged_dm_view *gdvp,
	struct bview_axes_state *gasp,
	int argc,
	const char *argv[],
	const char *usage)
{

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->draw);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->draw = 1;
	    else
		gasp->draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_size")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%lf", gasp->axes_size);
	    return GED_OK;
	}

	if (argc == 4) {
	    double size; /* must be double for scanf */

	    if (bu_sscanf(argv[3], "%lf", &size) != 1)
		goto bad;

	    gasp->axes_size = size;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_pos")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%lf %lf %lf",
			  V3ARGS(gasp->axes_pos));
	    return GED_OK;
	}

	if (argc == 6) {
	    double x, y, z; /* must be double for scanf */

	    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
		bu_sscanf(argv[4], "%lf", &y) != 1 ||
		bu_sscanf(argv[5], "%lf", &z) != 1)
		goto bad;

	    VSET(gasp->axes_pos, x, y, z);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->axes_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->axes_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "label_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->label_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->label_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "line_width")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->line_width);
	    return GED_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    gasp->line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "pos_only")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->pos_only);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->pos_only = 1;
	    else
		gasp->pos_only = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->tick_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->tick_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_enable")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_enabled);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->tick_enabled = 1;
	    else
		gasp->tick_enabled = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_interval")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%f", gasp->tick_interval);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_interval;

	    if (bu_sscanf(argv[3], "%d", &tick_interval) != 1)
		goto bad;

	    gasp->tick_interval = tick_interval;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_length")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_length);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_length;

	    if (bu_sscanf(argv[3], "%d", &tick_length) != 1)
		goto bad;

	    gasp->tick_length = tick_length;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_major_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->tick_major_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->tick_major_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_major_length")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_major_length);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_major_length;

	    if (bu_sscanf(argv[3], "%d", &tick_major_length) != 1)
		goto bad;

	    gasp->tick_major_length = tick_major_length;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "ticks_per_major")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->ticks_per_major);
	    return GED_OK;
	}

	if (argc == 4) {
	    int ticks_per_major;

	    if (bu_sscanf(argv[3], "%d", &ticks_per_major) != 1)
		goto bad;

	    gasp->ticks_per_major = ticks_per_major;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_threshold")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_threshold);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tick_threshold;

	    if (bu_sscanf(argv[3], "%d", &tick_threshold) != 1)
		goto bad;

	    if (tick_threshold < 1)
		tick_threshold = 1;

	    gasp->tick_threshold = tick_threshold;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "triple_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->triple_color);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->triple_color = 1;
	    else
		gasp->triple_color = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


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

    bu_vls_printf(gedp->ged_result_str, "%lf", current_top->to_gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get background color */
    if (argc == 2) {
	unsigned char *dm_bg = dm_get_bg(gdvp->gdv_dmp);
	bu_vls_printf(gedp->ged_result_str, "%d %d %d",
		      dm_bg[0],
		      dm_bg[1],
		      dm_bg[2]);
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

    (void)dm_make_current(gdvp->gdv_dmp);
    (void)dm_set_bg(gdvp->gdv_dmp, (unsigned char)r, (unsigned char)g, (unsigned char)b);

    to_refresh_view(gdvp);

    return GED_OK;

bad_color:
    bu_vls_printf(gedp->ged_result_str, "%s: %s %s %s", argv[0], argv[2], argv[3], argv[4]);
    return GED_ERROR;
}


HIDDEN int
to_blast(struct ged *gedp,
	 int argc,
	 const char *argv[],
	 ged_func_ptr UNUSED(func),
	 const char *UNUSED(usage),
	 int UNUSED(maxargs))
{
    int ret;

    ret = ged_blast(gedp, argc, argv);

    if (ret != GED_OK)
	return ret;

    to_autoview_all_views(current_top);

    return ret;
}


HIDDEN int
to_bounds(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  ged_func_ptr UNUSED(func),
	  const char *usage,
	  int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get window bounds */
    if (argc == 2) {
	vect_t *cmin = dm_get_clipmin(gdvp->gdv_dmp);
	vect_t *cmax = dm_get_clipmax(gdvp->gdv_dmp);
	bu_vls_printf(gedp->ged_result_str, "%g %g %g %g %g %g",
	       	(*cmin)[X], (*cmax)[X], (*cmin)[Y], (*cmax)[Y], (*cmin)[Z], (*cmax)[Z]);
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
    if ((*dm_get_clipmax(gdvp->gdv_dmp))[2] <= GED_MAX)
	dm_set_bound(gdvp->gdv_dmp, 1.0);
    else
	dm_set_bound(gdvp->gdv_dmp, GED_MAX/((*dm_get_clipmax(gdvp->gdv_dmp))[2]));

    (void)dm_make_current(gdvp->gdv_dmp);
    (void)dm_set_win_bounds(gdvp->gdv_dmp, bounds);

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
    struct ged_dm_view *gdvp;
    int status;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* configure the display manager window */
    status = dm_configure_win(gdvp->gdv_dmp, 0);

    /* configure the framebuffer window */
    if (gdvp->gdv_fbs.fbs_fbp != FB_NULL)
	(void)fb_configure_window(gdvp->gdv_fbs.fbs_fbp, dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));

    {
	char cdimX[32];
	char cdimY[32];
	char *av[5];

	snprintf(cdimX, 32, "%d", dm_get_width(gdvp->gdv_dmp));
	snprintf(cdimY, 32, "%d", dm_get_height(gdvp->gdv_dmp));

	av[0] = "rect";
	av[1] = "cdim";
	av[2] = cdimX;
	av[3] = cdimY;
	av[4] = NULL;

	gedp->ged_gvp = gdvp->gdv_view;
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
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

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_CONSTRAINED_ROTATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_constrain_rot %s %s %%x %%y}; break",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
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

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_CONSTRAINED_TRANSLATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_constrain_trans %s %s %%x %%y}; break",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    char *cp;
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
	    if (BU_STR_EQUAL(bu_vls_addr(&top->to_gop->go_name), bu_vls_addr(&db_vls))) {
		from_gedp = top->to_gop->go_gedp;
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
	    if (BU_STR_EQUAL(bu_vls_addr(&top->to_gop->go_name), bu_vls_addr(&db_vls))) {
		to_gedp = top->to_gop->go_gedp;
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


HIDDEN int
to_data_arrows(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
    struct bview_data_arrow_state *gdasp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdasp = &gdvp->gdv_view->gv_sdata_arrows;
    else
	gdasp = &gdvp->gdv_view->gv_data_arrows;

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_draw);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gdasp->gdas_draw = 1;
	    else
		gdasp->gdas_draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdasp->gdas_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gdasp->gdas_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "line_width")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_line_width);
	    return GED_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    gdasp->gdas_line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "points")) {
	register int i;

	if (argc == 3) {
	    for (i = 0; i < gdasp->gdas_num_points; ++i) {
		bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ",
			      V3ARGS(gdasp->gdas_points[i]));
	    }
	    return GED_OK;
	}

	if (argc == 4) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(current_top->to_interp, argv[3], &ac, &av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		return GED_ERROR;
	    }

	    if (ac % 2) {
		bu_vls_printf(gedp->ged_result_str, "%s: must be an even number of points", argv[0]);
		return GED_ERROR;
	    }

	    if (gdasp->gdas_num_points) {
		bu_free((void *)gdasp->gdas_points, "data points");
		gdasp->gdas_points = (point_t *)0;
		gdasp->gdas_num_points = 0;
	    }

	    /* Clear out data points */
	    if (ac < 1) {
		to_refresh_view(gdvp);
		Tcl_Free((char *)av);
		return GED_OK;
	    }

	    gdasp->gdas_num_points = ac;
	    gdasp->gdas_points = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	    for (i = 0; i < ac; ++i) {
		double scan[ELEMENTS_PER_VECT];

		if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {

		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);

		    bu_free((void *)gdasp->gdas_points, "data points");
		    gdasp->gdas_points = (point_t *)0;
		    gdasp->gdas_num_points = 0;

		    to_refresh_view(gdvp);
		    Tcl_Free((char *)av);
		    return GED_ERROR;
		}
		/* convert double to fastf_t */
		VMOVE(gdasp->gdas_points[i], scan);
	    }

	    to_refresh_view(gdvp);
	    Tcl_Free((char *)av);
	    return GED_OK;
	}
    }

    if (BU_STR_EQUAL(argv[2], "tip_length")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_tip_length);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tip_length;

	    if (bu_sscanf(argv[3], "%d", &tip_length) != 1)
		goto bad;

	    gdasp->gdas_tip_length = tip_length;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tip_width")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->gdas_tip_width);
	    return GED_OK;
	}

	if (argc == 4) {
	    int tip_width;

	    if (bu_sscanf(argv[3], "%d", &tip_width) != 1)
		goto bad;

	    gdasp->gdas_tip_width = tip_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


HIDDEN int
to_data_axes(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
    struct bview_data_axes_state *gdasp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdasp = &gdvp->gdv_view->gv_sdata_axes;
    else
	gdasp = &gdvp->gdv_view->gv_data_axes;

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->draw);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (0 <= i && i <= 2)
		gdasp->draw = i;
	    else
		gdasp->draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdasp->color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gdasp->color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "line_width")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdasp->line_width);
	    return GED_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    gdasp->line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "size")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%lf", gdasp->size);
	    return GED_OK;
	}

	if (argc == 4) {
	    double size; /* must be double for scanf */

	    if (bu_sscanf(argv[3], "%lf", &size) != 1)
		goto bad;

	    gdasp->size = size;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "points")) {
	register int i;

	if (argc == 3) {
	    for (i = 0; i < gdasp->num_points; ++i) {
		bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ",
			      V3ARGS(gdasp->points[i]));
	    }
	    return GED_OK;
	}

	if (argc == 4) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(current_top->to_interp, argv[3], &ac, &av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		return GED_ERROR;
	    }

	    if (gdasp->num_points) {
		bu_free((void *)gdasp->points, "data points");
		gdasp->points = (point_t *)0;
		gdasp->num_points = 0;
	    }

	    /* Clear out data points */
	    if (ac < 1) {
		to_refresh_view(gdvp);
		Tcl_Free((char *)av);
		return GED_OK;
	    }

	    gdasp->num_points = ac;
	    gdasp->points = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	    for (i = 0; i < ac; ++i) {
		double scan[3];

		if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);

		    bu_free((void *)gdasp->points, "data points");
		    gdasp->points = (point_t *)0;
		    gdasp->num_points = 0;

		    to_refresh_view(gdvp);
		    Tcl_Free((char *)av);
		    return GED_ERROR;
		}
		/* convert double to fastf_t */
		VMOVE(gdasp->points[i], scan);
	    }

	    to_refresh_view(gdvp);
	    Tcl_Free((char *)av);
	    return GED_OK;
	}
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


HIDDEN int
to_data_labels(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
    struct bview_data_label_state *gdlsp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdlsp = &gdvp->gdv_view->gv_sdata_labels;
    else
	gdlsp = &gdvp->gdv_view->gv_data_labels;

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdlsp->gdls_draw);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gdlsp->gdls_draw = 1;
	    else
		gdlsp->gdls_draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdlsp->gdls_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gdlsp->gdls_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "labels")) {
	register int i;

	/* { {{label this} {0 0 0}} {{label that} {100 100 100}} }*/

	if (argc == 3) {
	    for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
		bu_vls_printf(gedp->ged_result_str, "{{%s}", gdlsp->gdls_labels[i]);
		bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf}} ",
			      V3ARGS(gdlsp->gdls_points[i]));
	    }
	    return GED_OK;
	}

	if (argc == 4) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(current_top->to_interp, argv[3], &ac, &av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		return GED_ERROR;
	    }

	    if (gdlsp->gdls_num_labels) {
		for (i = 0; i < gdlsp->gdls_num_labels; ++i)
		    bu_free((void *)gdlsp->gdls_labels[i], "data label");

		bu_free((void *)gdlsp->gdls_labels, "data labels");
		bu_free((void *)gdlsp->gdls_points, "data points");
		gdlsp->gdls_labels = (char **)0;
		gdlsp->gdls_points = (point_t *)0;
		gdlsp->gdls_num_labels = 0;
	    }

	    /* Clear out data points */
	    if (ac < 1) {
		Tcl_Free((char *)av);
		to_refresh_view(gdvp);
		return GED_OK;
	    }

	    gdlsp->gdls_num_labels = ac;
	    gdlsp->gdls_labels = (char **)bu_calloc(ac, sizeof(char *), "data labels");
	    gdlsp->gdls_points = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	    for (i = 0; i < ac; ++i) {
		int sub_ac;
		const char **sub_av;
		double scan[ELEMENTS_PER_VECT];

		if (Tcl_SplitList(current_top->to_interp, av[i], &sub_ac, &sub_av) != TCL_OK) {
		    /*XXX Need a macro for the following lines. Do something similar for the rest. */
		    bu_free((void *)gdlsp->gdls_labels, "data labels");
		    bu_free((void *)gdlsp->gdls_points, "data points");
		    gdlsp->gdls_labels = (char **)0;
		    gdlsp->gdls_points = (point_t *)0;
		    gdlsp->gdls_num_labels = 0;

		    bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		    Tcl_Free((char *)av);
		    to_refresh_view(gdvp);
		    return GED_ERROR;
		}

		if (sub_ac != 2) {
		    /*XXX Need a macro for the following lines. Do something similar for the rest. */
		    bu_free((void *)gdlsp->gdls_labels, "data labels");
		    bu_free((void *)gdlsp->gdls_points, "data points");
		    gdlsp->gdls_labels = (char **)0;
		    gdlsp->gdls_points = (point_t *)0;
		    gdlsp->gdls_num_labels = 0;

		    bu_vls_printf(gedp->ged_result_str, "Each list element must contain a label and a point (i.e. {{some label} {0 0 0}})");
		    Tcl_Free((char *)sub_av);
		    Tcl_Free((char *)av);
		    to_refresh_view(gdvp);
		    return GED_ERROR;
		}

		if (bu_sscanf(sub_av[1], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", sub_av[1]);

		    /*XXX Need a macro for the following lines. Do something similar for the rest. */
		    bu_free((void *)gdlsp->gdls_labels, "data labels");
		    bu_free((void *)gdlsp->gdls_points, "data points");
		    gdlsp->gdls_labels = (char **)0;
		    gdlsp->gdls_points = (point_t *)0;
		    gdlsp->gdls_num_labels = 0;

		    Tcl_Free((char *)sub_av);
		    Tcl_Free((char *)av);
		    to_refresh_view(gdvp);
		    return GED_ERROR;
		}
		/* convert double to fastf_t */
		VMOVE(gdlsp->gdls_points[i], scan);

		gdlsp->gdls_labels[i] = bu_strdup(sub_av[0]);
		Tcl_Free((char *)sub_av);
	    }

	    Tcl_Free((char *)av);
	    to_refresh_view(gdvp);
	    return GED_OK;
	}
    }

    if (BU_STR_EQUAL(argv[2], "size")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdlsp->gdls_size);
	    return GED_OK;
	}

	if (argc == 4) {
	    int size;

	    if (bu_sscanf(argv[3], "%d", &size) != 1)
		goto bad;

	    gdlsp->gdls_size = size;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }


bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


HIDDEN int
to_data_lines(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
    struct bview_data_line_state *gdlsp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdlsp = &gdvp->gdv_view->gv_sdata_lines;
    else
	gdlsp = &gdvp->gdv_view->gv_data_lines;

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdlsp->gdls_draw);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gdlsp->gdls_draw = 1;
	    else
		gdlsp->gdls_draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdlsp->gdls_color));
	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gdlsp->gdls_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "line_width")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdlsp->gdls_line_width);
	    return GED_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    gdlsp->gdls_line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "points")) {
	register int i;

	if (argc == 3) {
	    for (i = 0; i < gdlsp->gdls_num_points; ++i) {
		bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ",
			      V3ARGS(gdlsp->gdls_points[i]));
	    }
	    return GED_OK;
	}

	if (argc == 4) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(current_top->to_interp, argv[3], &ac, &av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		return GED_ERROR;
	    }

	    if (ac % 2) {
		bu_vls_printf(gedp->ged_result_str, "%s: must be an even number of points", argv[0]);
		return GED_ERROR;
	    }

	    if (gdlsp->gdls_num_points) {
		bu_free((void *)gdlsp->gdls_points, "data points");
		gdlsp->gdls_points = (point_t *)0;
		gdlsp->gdls_num_points = 0;
	    }

	    /* Clear out data points */
	    if (ac < 1) {
		to_refresh_view(gdvp);
		Tcl_Free((char *)av);
		return GED_OK;
	    }

	    gdlsp->gdls_num_points = ac;
	    gdlsp->gdls_points = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	    for (i = 0; i < ac; ++i) {
		double scan[3];

		if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);

		    bu_free((void *)gdlsp->gdls_points, "data points");
		    gdlsp->gdls_points = (point_t *)0;
		    gdlsp->gdls_num_points = 0;

		    to_refresh_view(gdvp);
		    Tcl_Free((char *)av);
		    return GED_ERROR;
		}
		/* convert double to fastf_t */
		VMOVE(gdlsp->gdls_points[i], scan);
	    }

	    to_refresh_view(gdvp);
	    Tcl_Free((char *)av);
	    return GED_OK;
	}
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


/* These functions should be macros */
HIDDEN void
to_polygon_free(bview_polygon *gpp)
{
    register size_t j;

    if (gpp->gp_num_contours == 0)
	return;

    for (j = 0; j < gpp->gp_num_contours; ++j)
	bu_free((void *)gpp->gp_contour[j].gpc_point, "gp_contour points");

    bu_free((void *)gpp->gp_contour, "gp_contour");
    bu_free((void *)gpp->gp_hole, "gp_hole");
    gpp->gp_num_contours = 0;
}


HIDDEN void
to_polygons_free(bview_polygons *gpp)
{
    register size_t i;

    if (gpp->gp_num_polygons == 0)
	return;

    for (i = 0; i < gpp->gp_num_polygons; ++i) {
	to_polygon_free(&gpp->gp_polygon[i]);
    }

    bu_free((void *)gpp->gp_polygon, "data polygons");
    gpp->gp_polygon = (bview_polygon *)0;
    gpp->gp_num_polygons = 0;
}


HIDDEN int
to_extract_contours_av(struct ged *gedp, struct ged_dm_view *gdvp, bview_polygon *gpp, size_t contour_ac, const char **contour_av, int mode, int vflag)
{
    register size_t j = 0, k = 0;

    gpp->gp_num_contours = contour_ac;
    gpp->gp_hole = NULL;
    gpp->gp_contour = NULL;

    if (contour_ac == 0)
	return GED_OK;

    gpp->gp_hole = (int *)bu_calloc(contour_ac, sizeof(int), "gp_hole");
    gpp->gp_contour = (bview_poly_contour *)bu_calloc(contour_ac, sizeof(bview_poly_contour), "gp_contour");

    for (j = 0; j < contour_ac; ++j) {
	int ac;
	size_t point_ac;
	const char **point_av;
	int hole;

	/* Split contour j into points */
	if (Tcl_SplitList(current_top->to_interp, contour_av[j], &ac, &point_av) != TCL_OK) {
	    bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
	    return GED_ERROR;
	}
	point_ac = ac;

	/* point_ac includes a hole flag */
	if (mode != TCLCAD_POLY_CONTOUR_MODE && point_ac < 4) {
	    bu_vls_printf(gedp->ged_result_str, "There must be at least 3 points per contour");
	    Tcl_Free((char *)point_av);
	    return GED_ERROR;
	}

	gpp->gp_contour[j].gpc_num_points = point_ac - 1;
	gpp->gp_contour[j].gpc_point = (point_t *)bu_calloc(point_ac, sizeof(point_t), "gpc_point");

	if (bu_sscanf(point_av[0], "%d", &hole) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "contour %zu, point %zu: bad hole flag - %s\n",
			  j, k, point_av[k]);
	    Tcl_Free((char *)point_av);
	    return GED_ERROR;
	}
	gpp->gp_hole[j] = hole;

	for (k = 1; k < point_ac; ++k) {
	    double pt[ELEMENTS_PER_POINT]; /* must be double for scanf */

	    if (bu_sscanf(point_av[k], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
		bu_vls_printf(gedp->ged_result_str, "contour %zu, point %zu: bad data point - %s\n",
			      j, k, point_av[k]);
		Tcl_Free((char *)point_av);
		return GED_ERROR;
	    }

	    if (vflag) {
		MAT4X3PNT(gpp->gp_contour[j].gpc_point[k-1], gdvp->gdv_view->gv_view2model, pt);
	    } else {
		VMOVE(gpp->gp_contour[j].gpc_point[k-1], pt);
	    }

	}

	Tcl_Free((char *)point_av);
    }

    return GED_OK;
}


HIDDEN int
to_extract_polygons_av(struct ged *gedp, struct ged_dm_view *gdvp, bview_polygons *gpp, size_t polygon_ac, const char **polygon_av, int mode, int vflag)
{
    register size_t i;
    int ac;

    gpp->gp_num_polygons = polygon_ac;
    gpp->gp_polygon = (bview_polygon *)bu_calloc(polygon_ac, sizeof(bview_polygon), "data polygons");

    for (i = 0; i < polygon_ac; ++i) {
	size_t contour_ac;
	const char **contour_av;

	/* Split polygon i into contours */
	if (Tcl_SplitList(current_top->to_interp, polygon_av[i], &ac, &contour_av) != TCL_OK) {
	    bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
	    return GED_ERROR;
	}
	contour_ac = ac;

	if (to_extract_contours_av(gedp, gdvp, &gpp->gp_polygon[i], contour_ac, contour_av, mode, vflag) != GED_OK) {
	    Tcl_Free((char *)contour_av);
	    return GED_ERROR;
	}

	if (contour_ac)
	    Tcl_Free((char *)contour_av);
    }

    return GED_OK;
}


HIDDEN int
to_data_polygons(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 7 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gdv_view->gv_scale;
    gdpsp->gdps_data_vZ = gdvp->gdv_view->gv_data_vZ;
    VMOVE(gdpsp->gdps_origin, gdvp->gdv_view->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gdv_view->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gdv_view->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gdv_view->gv_view2model);

    if (BU_STR_EQUAL(argv[2], "target_poly")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%zu", gdpsp->gdps_target_polygon_i);
	    return GED_OK;
	}

	if (argc == 4) {
	    size_t i;

	    if (bu_sscanf(argv[3], "%zu", &i) != 1 || i > gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;

	    gdpsp->gdps_target_polygon_i = i;

	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "clip_type")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_clip_type);
	    return GED_OK;
	}

	if (argc == 4) {
	    int op;

	    if (bu_sscanf(argv[3], "%d", &op) != 1 || op > gctIntersection)
		goto bad;

	    gdpsp->gdps_clip_type = (ClipType)op;

	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_draw);
	    return GED_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gdpsp->gdps_draw = 1;
	    else
		gdpsp->gdps_draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: poly_color i [r g b]
     *
     * Set/get the color of polygon i.
     */
    if (BU_STR_EQUAL(argv[2], "poly_color")) {
	size_t i;

	if (argc == 4) {
	    /* Get the color for polygon i */
	    if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;

	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdpsp->gdps_polygons.gp_polygon[i].gp_color));

	    return GED_OK;
	}

	if (argc == 7) {
	    int r, g, b;

	    if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;

	    /* set background color */
	    if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		bu_sscanf(argv[5], "%d", &g) != 1 ||
		bu_sscanf(argv[6], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    /* Set the color for polygon i */
	    VSET(gdpsp->gdps_polygons.gp_polygon[i].gp_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: color [r g b]
     *
     * Set the color of all polygons, or get the default polygon color.
     */
    if (BU_STR_EQUAL(argv[2], "color")) {
	size_t i;

	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdpsp->gdps_color));

	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    /* Set the color for all polygons */
	    for (i = 0; i < gdpsp->gdps_polygons.gp_num_polygons; ++i) {
		VSET(gdpsp->gdps_polygons.gp_polygon[i].gp_color, r, g, b);
	    }

	    /* Set the default polygon color */
	    VSET(gdpsp->gdps_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: poly_line_width i [w]
     *
     * Set/get the line width of polygon i.
     */
    if (BU_STR_EQUAL(argv[2], "poly_line_width")) {
	size_t i;

	if (argc == 4) {
	    /* Get the line width for polygon i */
	    if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;

	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_polygons.gp_polygon[i].gp_line_width);

	    return GED_OK;
	}

	if (argc == 5) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;

	    if (bu_sscanf(argv[4], "%d", &line_width) != 1)
		goto bad;

	    if (line_width < 0)
		line_width = 0;

	    gdpsp->gdps_polygons.gp_polygon[i].gp_line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: line_width [w]
     *
     * Set the line width of all polygons, or get the default polygon line width.
     */
    if (BU_STR_EQUAL(argv[2], "line_width")) {
	size_t i;

	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_line_width);
	    return GED_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    if (line_width < 0)
		line_width = 0;

	    /* Set the line width for all polygons */
	    for (i = 0; i < gdpsp->gdps_polygons.gp_num_polygons; ++i) {
		gdpsp->gdps_polygons.gp_polygon[i].gp_line_width = line_width;
	    }

	    /* Set the default line width */
	    gdpsp->gdps_line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: poly_line_style i [w]
     *
     * Set/get the line style of polygon i.
     */
    if (BU_STR_EQUAL(argv[2], "poly_line_style")) {
	size_t i;

	if (argc == 4) {
	    /* Get the line style for polygon i */
	    if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;

	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_polygons.gp_polygon[i].gp_line_style);

	    return GED_OK;
	}

	if (argc == 5) {
	    int line_style;

	    if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;

	    if (bu_sscanf(argv[4], "%d", &line_style) != 1)
		goto bad;


	    if (line_style <= 0)
		gdpsp->gdps_polygons.gp_polygon[i].gp_line_style = 0;
	    else
		gdpsp->gdps_polygons.gp_polygon[i].gp_line_style = 1;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: line_style [w]
     *
     * Set the line style of all polygons, or get the default polygon line style.
     */
    if (BU_STR_EQUAL(argv[2], "line_style")) {
	size_t i;

	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_line_style);
	    return GED_OK;
	}

	if (argc == 4) {
	    int line_style;

	    if (bu_sscanf(argv[3], "%d", &line_style) != 1)
		goto bad;

	    if (line_style <= 0)
		line_style = 0;
	    else
		line_style = 1;

	    /* Set the line width for all polygons */
	    for (i = 0; i < gdpsp->gdps_polygons.gp_num_polygons; ++i) {
		gdpsp->gdps_polygons.gp_polygon[i].gp_line_style = line_style;
	    }

	    /* Set the default line style */
	    gdpsp->gdps_line_style = line_style;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: append_poly plist
     *
     * Append the polygon specified by plist.
     */
    if (BU_STR_EQUAL(argv[2], "append_poly")) {
	if (argc != 4)
	    goto bad;

	if (argc == 4) {
	    register size_t i;
	    int ac;
	    size_t contour_ac;
	    const char **contour_av;

	    /* Split the polygon in argv[3] into contours */
	    if (Tcl_SplitList(current_top->to_interp, argv[3], &ac, &contour_av) != TCL_OK ||
		ac < 1) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		return GED_ERROR;
	    }
	    contour_ac = ac;

	    i = gdpsp->gdps_polygons.gp_num_polygons;
	    ++gdpsp->gdps_polygons.gp_num_polygons;
	    gdpsp->gdps_polygons.gp_polygon = (bview_polygon *)bu_realloc(gdpsp->gdps_polygons.gp_polygon,
									gdpsp->gdps_polygons.gp_num_polygons * sizeof(bview_polygon),
									"realloc gp_polygon");

	    if (to_extract_contours_av(gedp, gdvp, &gdpsp->gdps_polygons.gp_polygon[i],
				       contour_ac, contour_av, gdvp->gdv_view->gv_mode, 0) != GED_OK) {
		Tcl_Free((char *)contour_av);
		return GED_ERROR;
	    }

	    VMOVE(gdpsp->gdps_polygons.gp_polygon[i].gp_color, gdpsp->gdps_color);
	    gdpsp->gdps_polygons.gp_polygon[i].gp_line_style = gdpsp->gdps_line_style;
	    gdpsp->gdps_polygons.gp_polygon[i].gp_line_width = gdpsp->gdps_line_width;

	    Tcl_Free((char *)contour_av);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}
    }


    /* Usage: clip [i j [op]]
     *
     * Clip polygon i using polygon j and op.
     */
    if (BU_STR_EQUAL(argv[2], "clip")) {
	size_t i, j;
	int op;
	bview_polygon *gpp;

	if (argc > 6)
	    goto bad;

	if (argc > 3) {
	    if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;
	} else
	    i = gdpsp->gdps_target_polygon_i;

	if (argc > 4) {
	    if (bu_sscanf(argv[4], "%zu", &j) != 1 ||
		j >= gdpsp->gdps_polygons.gp_num_polygons)
		goto bad;
	} else
	    j = gdpsp->gdps_polygons.gp_num_polygons - 1; /* Default - use last polygon as the clip polygon */

	if (argc != 6)
	    op = gdpsp->gdps_clip_type;
	else if (bu_sscanf(argv[5], "%d", &op) != 1 || op > gctIntersection)
	    goto bad;

	gpp = ged_clip_polygon((ClipType)op,
			       &gdpsp->gdps_polygons.gp_polygon[i],
			       &gdpsp->gdps_polygons.gp_polygon[j],
			       CLIPPER_MAX,
			       gdpsp->gdps_model2view,
			       gdpsp->gdps_view2model);

	/* Free the target polygon */
	to_polygon_free(&gdpsp->gdps_polygons.gp_polygon[i]);

	/* When using defaults, the clip polygon is assumed to be temporary and is removed after clipping */
	if (argc == 3) {
	    /* Free the clip polygon */
	    to_polygon_free(&gdpsp->gdps_polygons.gp_polygon[j]);

	    /* No longer need space for the clip polygon */
	    --gdpsp->gdps_polygons.gp_num_polygons;
	    gdpsp->gdps_polygons.gp_polygon = (bview_polygon *)bu_realloc(gdpsp->gdps_polygons.gp_polygon,
									gdpsp->gdps_polygons.gp_num_polygons * sizeof(bview_polygon),
									"realloc gp_polygon");
	}

	/* Replace the target polygon with the newly clipped polygon. */
	/* Not doing a struct copy to avoid overwriting the color, line width and line style. */
	gdpsp->gdps_polygons.gp_polygon[i].gp_num_contours = gpp->gp_num_contours;
	gdpsp->gdps_polygons.gp_polygon[i].gp_hole = gpp->gp_hole;
	gdpsp->gdps_polygons.gp_polygon[i].gp_contour = gpp->gp_contour;

	/* Free the clipped polygon container */
	bu_free((void *)gpp, "clip gpp");

	to_refresh_view(gdvp);
	return GED_OK;
    }

    /* Usage: export i sketch_name
     *
     * Export polygon i to sketch_name
     */
    if (BU_STR_EQUAL(argv[2], "export")) {
	size_t i;
	int ret;

	if (argc != 5)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	if ((ret = ged_export_polygon(gedp, gdpsp, i, argv[4])) != GED_OK)
	    bu_vls_printf(gedp->ged_result_str, "%s: failed to export polygon %zu to %s", argv[0], i, argv[4]);

	return ret;
    }

    /* Usage: import sketch_name
     *
     * Import sketch_name and append
     */
    if (BU_STR_EQUAL(argv[2], "import")) {
	bview_polygon *gpp;
	size_t i;

	if (argc != 4)
	    goto bad;

	if ((gpp = ged_import_polygon(gedp, argv[3])) == (bview_polygon *)0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: failed to import sketch %s", argv[0], argv[3]);
	    return GED_ERROR;
	}

	i = gdpsp->gdps_polygons.gp_num_polygons;
	++gdpsp->gdps_polygons.gp_num_polygons;
	gdpsp->gdps_polygons.gp_polygon = (bview_polygon *)bu_realloc(gdpsp->gdps_polygons.gp_polygon,
								    gdpsp->gdps_polygons.gp_num_polygons * sizeof(bview_polygon),
								    "realloc gp_polygon");

	gdpsp->gdps_polygons.gp_polygon[i] = *gpp;  /* struct copy */
	VMOVE(gdpsp->gdps_polygons.gp_polygon[i].gp_color, gdpsp->gdps_color);
	gdpsp->gdps_polygons.gp_polygon[i].gp_line_style = gdpsp->gdps_line_style;
	gdpsp->gdps_polygons.gp_polygon[i].gp_line_width = gdpsp->gdps_line_width;

	to_refresh_view(gdvp);
	return GED_OK;
    }

    /* Usage: area i
     *
     * Find area of polygon i
     */
    if (BU_STR_EQUAL(argv[2], "area")) {
	size_t i;
	double area;

	if (argc != 4)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	area = ged_find_polygon_area(&gdpsp->gdps_polygons.gp_polygon[i], CLIPPER_MAX,
				     gdpsp->gdps_model2view, gdpsp->gdps_scale);
	bu_vls_printf(gedp->ged_result_str, "%lf", area);

	return GED_OK;
    }

    /* Usage: polygons_overlap i j
     *
     * Do polygons i and j overlap?
     */
    if (BU_STR_EQUAL(argv[2], "polygons_overlap")) {
	size_t i, j;
	int ret;

	if (argc != 5)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	if (bu_sscanf(argv[4], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	ret = ged_polygons_overlap(gedp, &gdpsp->gdps_polygons.gp_polygon[i], &gdpsp->gdps_polygons.gp_polygon[j]);
	bu_vls_printf(gedp->ged_result_str, "%d", ret);

	return GED_OK;
    }

    /* Usage: [v]polygons [poly_list]
     *
     * Set/get the polygon list. If vpolygons is specified then
     * the polygon list is in view coordinates.
     */
    if (BU_STR_EQUAL(argv[2], "polygons") || BU_STR_EQUAL(argv[2], "vpolygons")) {
	register size_t i, j, k;
	int ac;
	int vflag;

	if (BU_STR_EQUAL(argv[2], "polygons"))
	    vflag = 0;
	else
	    vflag = 1;

	if (argc == 3) {
	    for (i = 0; i < gdpsp->gdps_polygons.gp_num_polygons; ++i) {
		bu_vls_printf(gedp->ged_result_str, " {");

		for (j = 0; j < gdpsp->gdps_polygons.gp_polygon[i].gp_num_contours; ++j) {
		    bu_vls_printf(gedp->ged_result_str, " {%d ", gdpsp->gdps_polygons.gp_polygon[i].gp_hole[j]);

		    for (k = 0; k < gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points; ++k) {
			point_t pt;

			if (vflag) {
			    MAT4X3PNT(pt, gdvp->gdv_view->gv_model2view, gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k]);
			} else {
			    VMOVE(pt, gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k]);
			}

			bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ", V3ARGS(pt));
		    }

		    bu_vls_printf(gedp->ged_result_str, "} ");
		}

		bu_vls_printf(gedp->ged_result_str, "} ");
	    }

	    return GED_OK;
	}

	if (argc == 4) {
	    size_t polygon_ac;
	    const char **polygon_av;

	    /* Split into polygons */
	    if (Tcl_SplitList(current_top->to_interp, argv[3], &ac, &polygon_av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		return GED_ERROR;
	    }
	    polygon_ac = ac;

	    to_polygons_free(&gdpsp->gdps_polygons);
	    gdpsp->gdps_target_polygon_i = 0;

	    if (polygon_ac < 1) {
		to_refresh_view(gdvp);
		return GED_OK;
	    }

	    if (to_extract_polygons_av(gedp, gdvp, &gdpsp->gdps_polygons, polygon_ac, polygon_av, gdvp->gdv_view->gv_mode, vflag) != GED_OK) {
		Tcl_Free((char *)polygon_av);
		return GED_ERROR;
	    }

	    Tcl_Free((char *)polygon_av);
	    to_refresh_view(gdvp);
	    return GED_OK;
	}
    }

    /* Usage: replace_poly i plist
     *
     * Replace polygon i with plist.
     */
    if (BU_STR_EQUAL(argv[2], "replace_poly")) {
	size_t i;
	int ac;
	size_t contour_ac;
	const char **contour_av;
	bview_polygon gp;

	if (argc != 5)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	/* Split the polygon in argv[4] into contours */
	if (Tcl_SplitList(current_top->to_interp, argv[4], &ac, &contour_av) != TCL_OK ||
	    ac < 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
	    return GED_ERROR;
	}
	contour_ac = ac;

	if (to_extract_contours_av(gedp, gdvp, &gp, contour_ac, contour_av, gdvp->gdv_view->gv_mode, 0) != GED_OK) {
	    Tcl_Free((char *)contour_av);
	    return GED_ERROR;
	}

	to_polygon_free(&gdpsp->gdps_polygons.gp_polygon[i]);

	/* Not doing a struct copy to avoid overwriting the color, line width and line style. */
	gdpsp->gdps_polygons.gp_polygon[i].gp_num_contours = gp.gp_num_contours;
	gdpsp->gdps_polygons.gp_polygon[i].gp_hole = gp.gp_hole;
	gdpsp->gdps_polygons.gp_polygon[i].gp_contour = gp.gp_contour;

	to_refresh_view(gdvp);
	return GED_OK;
    }

    /* Usage: append_point i j pt
     *
     * Append pt to polygon_i/contour_j
     */
    if (BU_STR_EQUAL(argv[2], "append_point")) {
	size_t i, j, k;
	double pt[ELEMENTS_PER_POINT]; /* must be double for scan */

	if (argc != 6)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	if (bu_sscanf(argv[4], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.gp_polygon[i].gp_num_contours)
	    goto bad;

	if (bu_sscanf(argv[5], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3)
	    goto bad;

	k = gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points;
	++gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points;
	gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point = (point_t *)bu_realloc(gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point,
											   gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points * sizeof(point_t),
											   "realloc gpc_point");
	VMOVE(gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k], pt);
	to_refresh_view(gdvp);
	return GED_OK;
    }

    /* Usage: get_point i j k
     *
     * Get polygon_i/contour_j/point_k
     */
    if (BU_STR_EQUAL(argv[2], "get_point")) {
	size_t i, j, k;

	if (argc != 6)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	if (bu_sscanf(argv[4], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.gp_polygon[i].gp_num_contours)
	    goto bad;

	if (bu_sscanf(argv[5], "%zu", &k) != 1 ||
	    k >= gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points)
	    goto bad;

	bu_vls_printf(gedp->ged_result_str, "%lf %lf %lf", V3ARGS(gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k]));
	return GED_OK;
    }

    /* Usage: replace_point i j k pt
     *
     * Replace polygon_i/contour_j/point_k with pt
     */
    if (BU_STR_EQUAL(argv[2], "replace_point")) {
	size_t i, j, k;
	double pt[ELEMENTS_PER_POINT];

	if (argc != 7)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.gp_num_polygons)
	    goto bad;

	if (bu_sscanf(argv[4], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.gp_polygon[i].gp_num_contours)
	    goto bad;

	if (bu_sscanf(argv[5], "%zu", &k) != 1 ||
	    k >= gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points)
	    goto bad;

	if (bu_sscanf(argv[6], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3)
	    goto bad;

	VMOVE(gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k], pt);
	to_refresh_view(gdvp);
	return GED_OK;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
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
    int mx, my;
    int width, height;
    int dindex;
    fastf_t cx, cy;
    fastf_t vx, vy;
    fastf_t sf;
    point_t mpoint, vpoint;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%d", &dindex) != 1 || dindex < 0)
	goto bad;

    if (argc == 5) {
	if (bu_sscanf(argv[4], "%d %d", &mx, &my) != 2)
	    goto bad;
    } else {
	if (bu_sscanf(argv[4], "%d", &mx) != 1)
	    goto bad;

	if (bu_sscanf(argv[5], "%d", &my) != 1)
	    goto bad;
    }

    width = dm_get_width(gdvp->gdv_dmp);
    cx = 0.5 * (fastf_t)width;
    height = dm_get_height(gdvp->gdv_dmp);
    cy = 0.5 * (fastf_t)height;
    sf = 2.0 / width;
    vx = (mx - cx) * sf;
    vy = (cy - my) * sf;

    if (BU_STR_EQUAL(argv[2], "data_polygons")) {
	size_t i, j, k;
	bview_data_polygon_state *gdpsp = &gdvp->gdv_view->gv_data_polygons;

	if (bu_sscanf(argv[3], "%zu %zu %zu", &i, &j, &k) != 3)
	    goto bad;

	/* Silently ignore */
	if (i >= gdpsp->gdps_polygons.gp_num_polygons ||
	    j >= gdpsp->gdps_polygons.gp_polygon[i].gp_num_contours ||
	    k >= gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points)
	    return GED_OK;

	/* This section is for moving the entire contour */
	if (gdvp->gdv_view->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    VMOVE(old_mpoint, gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k]);

	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    for (k = 0; k < gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_num_points; ++k) {
		VADD2(gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k],
		      gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k],
		      diff);
	    }
	} else {
	    /* This section is for moving a single point in the contour */
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(gdpsp->gdps_polygons.gp_polygon[i].gp_contour[j].gpc_point[k], gdvp->gdv_view->gv_view2model, vpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "data_arrows")) {
	struct bview_data_arrow_state *gdasp = &gdvp->gdv_view->gv_data_arrows;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_data_arrows.gdas_num_points)
	    return GED_OK;

	/* This section is for moving the entire arrow */
	if (gdvp->gdv_view->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdasp->gdas_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdasp->gdas_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdasp->gdas_points[dindexA], new_mpoint);
	    VADD2(gdasp->gdas_points[dindexB], gdasp->gdas_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdasp->gdas_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VMOVE(gdasp->gdas_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "sdata_arrows")) {
	struct bview_data_arrow_state *gdasp = &gdvp->gdv_view->gv_sdata_arrows;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_sdata_arrows.gdas_num_points)
	    return GED_OK;

	/* This section is for moving the entire arrow */
	if (gdvp->gdv_view->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdasp->gdas_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdasp->gdas_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdasp->gdas_points[dindexA], new_mpoint);
	    VADD2(gdasp->gdas_points[dindexB], gdasp->gdas_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdasp->gdas_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VMOVE(gdasp->gdas_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "data_axes")) {
	struct bview_data_axes_state *gdasp = &gdvp->gdv_view->gv_data_axes;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_data_axes.num_points)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdasp->points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	VMOVE(gdasp->points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "sdata_axes")) {
	struct bview_data_axes_state *gdasp = &gdvp->gdv_view->gv_sdata_axes;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_sdata_axes.num_points)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdasp->points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	VMOVE(gdasp->points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }


    if (BU_STR_EQUAL(argv[2], "data_labels")) {
	struct bview_data_label_state *gdlsp = &gdvp->gdv_view->gv_data_labels;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_data_labels.gdls_num_labels)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdlsp->gdls_points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	VMOVE(gdlsp->gdls_points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "sdata_labels")) {
	struct bview_data_label_state *gdlsp = &gdvp->gdv_view->gv_sdata_labels;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_sdata_labels.gdls_num_labels)
	    return GED_OK;

	MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdlsp->gdls_points[dindex]);
	vpoint[X] = vx;
	vpoint[Y] = vy;
	MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	VMOVE(gdlsp->gdls_points[dindex], mpoint);

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "data_lines")) {
	struct bview_data_line_state *gdlsp = &gdvp->gdv_view->gv_data_lines;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_data_lines.gdls_num_points)
	    return GED_OK;

	/* This section is for moving the entire line */
	if (gdvp->gdv_view->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdlsp->gdls_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdlsp->gdls_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdlsp->gdls_points[dindexA], new_mpoint);
	    VADD2(gdlsp->gdls_points[dindexB], gdlsp->gdls_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdlsp->gdls_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VMOVE(gdlsp->gdls_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "sdata_lines")) {
	struct bview_data_line_state *gdlsp = &gdvp->gdv_view->gv_sdata_lines;

	/* Silently ignore */
	if (dindex >= gdvp->gdv_view->gv_sdata_lines.gdls_num_points)
	    return GED_OK;

	/* This section is for moving the entire line */
	if (gdvp->gdv_view->gv_mode == TCLCAD_DATA_MOVE_OBJECT_MODE) {
	    int dindexA, dindexB;
	    point_t old_mpoint, new_mpoint;
	    vect_t diff;

	    dindexA = dindex;
	    if (dindex%2)
		dindexB = dindex - 1;
	    else
		dindexB = dindex + 1;

	    VMOVE(old_mpoint, gdlsp->gdls_points[dindexA]);

	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdlsp->gdls_points[dindexA]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(new_mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VSUB2(diff, new_mpoint, old_mpoint);

	    VMOVE(gdlsp->gdls_points[dindexA], new_mpoint);
	    VADD2(gdlsp->gdls_points[dindexB], gdlsp->gdls_points[dindexB], diff);
	} else {
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdlsp->gdls_points[dindex]);
	    vpoint[X] = vx;
	    vpoint[Y] = vy;
	    MAT4X3PNT(mpoint, gdvp->gdv_view->gv_view2model, vpoint);
	    VMOVE(gdlsp->gdls_points[dindex], mpoint);
	}

	to_refresh_view(gdvp);
	return GED_OK;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


/*
 * Usage: data_scale vname dtype sf
 */
HIDDEN int
to_data_scale(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    register int i;
    struct ged_dm_view *gdvp;
    fastf_t sf;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    usage = "vname dtype sf";
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &sf) != 1 || sf < 0) {
	bu_vls_printf(gedp->ged_result_str, "Invalid scale factor - %s", argv[2]);
	return GED_ERROR;
    }

    /* scale data arrows */
    {
	struct bview_data_arrow_state *gdasp = &gdvp->gdv_view->gv_data_arrows;
	point_t vcenter = {0, 0, 0};

	/* Scale the length of each arrow */
	for (i = 0; i < gdasp->gdas_num_points; i += 2) {
	    vect_t diff;
	    point_t vpoint;

	    MAT4X3PNT(vpoint, gedp->ged_gvp->gv_model2view, gdasp->gdas_points[i]);
	    vcenter[Z] = vpoint[Z];
	    VSUB2(diff, vpoint, vcenter);
	    VSCALE(diff, diff, sf);
	    VADD2(vpoint, vcenter, diff);
	    MAT4X3PNT(gdasp->gdas_points[i], gedp->ged_gvp->gv_view2model, vpoint);
	}
    }

    /* scale data labels */
    {
	struct bview_data_label_state *gdlsp = &gdvp->gdv_view->gv_data_labels;
	point_t vcenter = {0, 0, 0};
	point_t vpoint;

	/* Scale the location of each label WRT the view center */
	for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	    vect_t diff;

	    MAT4X3PNT(vpoint, gedp->ged_gvp->gv_model2view, gdlsp->gdls_points[i]);
	    vcenter[Z] = vpoint[Z];
	    VSUB2(diff, vpoint, vcenter);
	    VSCALE(diff, diff, sf);
	    VADD2(vpoint, vcenter, diff);
	    MAT4X3PNT(gdlsp->gdls_points[i], gedp->ged_gvp->gv_view2model, vpoint);
	}
    }


    to_refresh_view(gdvp);
    return GED_OK;
}


HIDDEN int
to_data_move_object_mode(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr UNUSED(func),
			 const char *usage,
			 int UNUSED(maxargs))
{
    int x, y;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* At the moment, only gv_mode is being used. */
    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_DATA_MOVE_OBJECT_MODE;

    return GED_OK;
}


HIDDEN int
to_data_move_point_mode(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr UNUSED(func),
			const char *usage,
			int UNUSED(maxargs))
{
    int x, y;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* At the moment, only gv_mode is being used. */
    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_DATA_MOVE_POINT_MODE;

    return GED_OK;
}


HIDDEN int
to_data_pick(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    int mx, my, width, height;
    fastf_t cx, cy;
    fastf_t vx, vy;
    fastf_t sf;
    point_t dpoint, vpoint;
    register int i;
    struct ged_dm_view *gdvp;
    fastf_t top_z = -MAX_FASTF;
    point_t top_point = VINIT_ZERO;
    size_t top_i = 0;
    size_t top_j = 0;
    size_t top_k = 0;
    int found_top = 0;
    char *top_data_str = NULL;
    char *top_data_label = NULL;
    static fastf_t tol = 0.015;
    static char *data_polygons_str = "data_polygons";
    static char *data_labels_str = "data_labels";
    static char *sdata_labels_str = "sdata_labels";
    static char *data_lines_str = "data_lines";
    static char *sdata_lines_str = "sdata_lines";
    static char *data_arrows_str = "data_arrows";
    static char *sdata_arrows_str = "sdata_arrows";
    static char *data_axes_str = "data_axes";
    static char *sdata_axes_str = "sdata_axes";

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argc == 3) {
	if (bu_sscanf(argv[2], "%d %d", &mx, &my) != 2)
	    goto bad;
    } else {
	if (bu_sscanf(argv[2], "%d", &mx) != 1)
	    goto bad;

	if (bu_sscanf(argv[3], "%d", &my) != 1)
	    goto bad;
    }

    width = dm_get_width(gdvp->gdv_dmp);
    cx = 0.5 * (fastf_t)width;
    height = dm_get_height(gdvp->gdv_dmp);
    cy = 0.5 * (fastf_t)height;
    sf = 2.0 / width;
    vx = (mx - cx) * sf;
    vy = (cy - my) * sf;

    /* check for polygon points */
    if (gdvp->gdv_view->gv_data_polygons.gdps_draw &&
	gdvp->gdv_view->gv_data_polygons.gdps_polygons.gp_num_polygons) {
	register size_t si, sj, sk;

	bview_data_polygon_state *gdpsp = &gdvp->gdv_view->gv_data_polygons;

	for (si = 0; si < gdpsp->gdps_polygons.gp_num_polygons; ++si)
	    for (sj = 0; sj < gdpsp->gdps_polygons.gp_polygon[si].gp_num_contours; ++sj)
		for (sk = 0; sk < gdpsp->gdps_polygons.gp_polygon[si].gp_contour[sj].gpc_num_points; ++sk) {
		    fastf_t minX, maxX;
		    fastf_t minY, maxY;

		    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, gdpsp->gdps_polygons.gp_polygon[si].gp_contour[sj].gpc_point[sk]);
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
			    VMOVE(top_point, gdpsp->gdps_polygons.gp_polygon[si].gp_contour[sj].gpc_point[sk]);
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
    if (gdvp->gdv_view->gv_data_labels.gdls_draw &&
	gdvp->gdv_view->gv_data_labels.gdls_num_labels) {
	struct bview_data_label_state *gdlsp = &gdvp->gdv_view->gv_data_labels;

	for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    if (gdvp->gdv_view->gv_sdata_labels.gdls_draw &&
	gdvp->gdv_view->gv_sdata_labels.gdls_num_labels) {
	struct bview_data_label_state *gdlsp = &gdvp->gdv_view->gv_sdata_labels;

	for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    if (gdvp->gdv_view->gv_data_lines.gdls_draw &&
	gdvp->gdv_view->gv_data_lines.gdls_num_points) {
	struct bview_data_line_state *gdlsp = &gdvp->gdv_view->gv_data_lines;

	for (i = 0; i < gdlsp->gdls_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    if (gdvp->gdv_view->gv_sdata_lines.gdls_draw &&
	gdvp->gdv_view->gv_sdata_lines.gdls_num_points) {
	struct bview_data_line_state *gdlsp = &gdvp->gdv_view->gv_sdata_lines;

	for (i = 0; i < gdlsp->gdls_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdlsp->gdls_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    if (gdvp->gdv_view->gv_data_arrows.gdas_draw &&
	gdvp->gdv_view->gv_data_arrows.gdas_num_points) {
	struct bview_data_arrow_state *gdasp = &gdvp->gdv_view->gv_data_arrows;

	for (i = 0; i < gdasp->gdas_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->gdas_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    if (gdvp->gdv_view->gv_sdata_arrows.gdas_draw &&
	gdvp->gdv_view->gv_sdata_arrows.gdas_num_points) {
	struct bview_data_arrow_state *gdasp = &gdvp->gdv_view->gv_sdata_arrows;

	for (i = 0; i < gdasp->gdas_num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->gdas_points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    if (gdvp->gdv_view->gv_data_axes.draw &&
	gdvp->gdv_view->gv_data_axes.num_points) {
	struct bview_data_axes_state *gdasp = &gdvp->gdv_view->gv_data_axes;

	for (i = 0; i < gdasp->num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    if (gdvp->gdv_view->gv_sdata_axes.draw &&
	gdvp->gdv_view->gv_sdata_axes.num_points) {
	struct bview_data_axes_state *gdasp = &gdvp->gdv_view->gv_sdata_axes;

	for (i = 0; i < gdasp->num_points; ++i) {
	    fastf_t minX, maxX;
	    fastf_t minY, maxY;

	    VMOVE(dpoint, gdasp->points[i]);
	    MAT4X3PNT(vpoint, gdvp->gdv_view->gv_model2view, dpoint);

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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Get the data vZ */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%lf", gdvp->gdv_view->gv_data_vZ);
	return GED_OK;
    }

    /* Set the data vZ */
    if (bu_sscanf(argv[2], "%lf", &vZ) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_data_vZ = vZ;

    return GED_OK;
}


HIDDEN void
to_deleteViewProc(ClientData clientData)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientData;

    BU_LIST_DEQUEUE(&(gdvp->l));
    bu_vls_free(&gdvp->gdv_name);
    bu_vls_free(&gdvp->gdv_callback);
    bu_vls_free(&gdvp->gdv_edit_motion_delta_callback);
    (void)dm_close(gdvp->gdv_dmp);
    bu_free((void *)gdvp->gdv_view, "ged_view");
    bu_free((void *)gdvp, "ged_dm_view");
}


HIDDEN void
to_init_default_bindings(struct ged_dm_view *gdvp)
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;

    bu_vls_printf(&bindings, "bind %s <Configure> {%s configure %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Enter> {focus %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)));
    bu_vls_printf(&bindings, "bind %s <Expose> {%s handle_expose %s %%c; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "catch {wm protocol %s WM_DELETE_WINDOW {%s delete_view %s; break}}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));

    /* Mouse Bindings */
    bu_vls_printf(&bindings, "bind %s <2> {%s vslew %s %%x %%y; focus %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)));
    bu_vls_printf(&bindings, "bind %s <1> {%s zoom %s 0.5; focus %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)));
    bu_vls_printf(&bindings, "bind %s <3> {%s zoom %s 2.0; focus %s;  break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)));
#ifdef DM_X
    bu_vls_printf(&bindings, "bind %s <4> {%s zoom %s 1.1; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <5> {%s zoom %s 0.9; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
#endif
#ifdef DM_WGL
    bu_vls_printf(&bindings, "bind %s <MouseWheel> {if {%%D < 0} {%s zoom %s 0.9} else {%s zoom %s 1.1}; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
#endif

    /* Idle Mode */
    bu_vls_printf(&bindings, "bind %s <ButtonRelease> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <KeyRelease-Control_L> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <KeyRelease-Control_R> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <KeyRelease-Shift_L> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <KeyRelease-Shift_R> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <KeyRelease-Alt_L> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <KeyRelease-Alt_R> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));

    /* Rotate Mode */
    bu_vls_printf(&bindings, "bind %s <Control-ButtonRelease-1> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-ButtonPress-1> {%s rotate_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-ButtonPress-2> {%s rotate_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-ButtonPress-3> {%s rotate_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));

    /* Translate Mode */
    bu_vls_printf(&bindings, "bind %s <Shift-ButtonRelease-1> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Shift-ButtonPress-1> {%s translate_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Shift-ButtonPress-2> {%s translate_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Shift-ButtonPress-3> {%s translate_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));

    /* Scale Mode */
    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonRelease-1> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonPress-1> {%s scale_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonPress-2> {%s scale_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-Shift-ButtonPress-3> {%s scale_mode %s %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));

    /* Constrained Rotate Mode */
    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonRelease-1> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonPress-1> {%s constrain_rmode %s x %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonPress-2> {%s constrain_rmode %s y %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Control-Lock-ButtonPress-3> {%s constrain_rmode %s z %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));

    /* Constrained Translate Mode */
    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonRelease-1> {%s idle_mode %s; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonPress-1> {%s constrain_tmode %s x %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonPress-2> {%s constrain_tmode %s y %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Shift-Lock-ButtonPress-3> {%s constrain_tmode %s z %%x %%y; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));

    /* Key Bindings */
    bu_vls_printf(&bindings, "bind %s 3 {%s aet %s 35 25; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s 4 {%s aet %s 45 45; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s f {%s aet %s 0 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s F {%s aet %s 0 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s R {%s aet %s 180 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s r {%s aet %s 270 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s l {%s aet %s 90 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s L {%s aet %s 90 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s t {%s aet %s 270 90; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s T {%s aet %s 270 90; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s b {%s aet %s 270 -90; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s B {%s aet %s 270 -90; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s + {%s zoom %s 2.0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s = {%s zoom %s 2.0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s _ {%s zoom %s 0.5; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s - {%s zoom %s 0.5; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Key-Left> {%s rot %s -v 0 1 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Key-Right> {%s rot %s -v 0 -1 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Key-Up> {%s rot %s -v 1 0 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    bu_vls_printf(&bindings, "bind %s <Key-Down> {%s rot %s -v -1 0 0; break}; ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));


    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    int on;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (2 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    /* Get dlist_on state */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d", current_top->to_gop->go_dlist_on);
	return GED_OK;
    }

    /* Set dlist_on state */
    if (bu_sscanf(argv[1], "%d", &on) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    current_top->to_gop->go_dlist_on = on;

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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get the font size */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_fontsize(gdvp->gdv_dmp));
	return GED_OK;
    }

    /* set background color */
    if (bu_sscanf(argv[2], "%d", &fontsize) != 1)
	goto bad_fontsize;

    if (DM_VALID_FONT_SIZE(fontsize) || fontsize == 0) {
	dm_set_fontsize(gdvp->gdv_dmp, fontsize);
	(void)dm_configure_win(gdvp->gdv_dmp, 1);
	to_refresh_view(gdvp);
	return GED_OK;
    }

bad_fontsize:
    bu_vls_printf(gedp->ged_result_str, "%s: %s", argv[0], argv[2]);
    return GED_ERROR;
}


HIDDEN int
to_get_prev_mouse(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d %d", (int)gdvp->gdv_view->gv_prevMouseX, (int)gdvp->gdv_view->gv_prevMouseY);
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    to_deleteViewProc(gdvp);

    return GED_OK;
}


HIDDEN int
to_faceplate(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    int i;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 7 < argc)
	goto bad;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[2], "center_dot")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_center_dot.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_center_dot.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_center_dot.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_center_dot.gos_line_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_center_dot.gos_line_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "prim_labels")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_prim_labels.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_prim_labels.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_prim_labels.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_prim_labels.gos_text_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_prim_labels.gos_text_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "view_params")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_view_params.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_view_params.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_view_params.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_view_params.gos_text_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_view_params.gos_text_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "view_scale")) {
	if (BU_STR_EQUAL(argv[3], "draw")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_view_scale.gos_draw);
		return GED_OK;
	    } else if (argc == 5) {
		if (bu_sscanf(argv[4], "%d", &i) != 1)
		    goto bad;

		if (i)
		    gdvp->gdv_view->gv_view_scale.gos_draw = 1;
		else
		    gdvp->gdv_view->gv_view_scale.gos_draw = 0;

		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	if (BU_STR_EQUAL(argv[3], "color")) {
	    if (argc == 4) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d", V3ARGS(gdvp->gdv_view->gv_view_scale.gos_line_color));
		return GED_OK;
	    } else if (argc == 7) {
		int r, g, b;

		if (bu_sscanf(argv[4], "%d", &r) != 1 ||
		    bu_sscanf(argv[5], "%d", &g) != 1 ||
		    bu_sscanf(argv[6], "%d", &b) != 1)
		    goto bad;

		VSET(gdvp->gdv_view->gv_view_scale.gos_line_color, r, g, b);
		to_refresh_view(gdvp);
		return GED_OK;
	    }
	}

	goto bad;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
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
to_handle_refresh(struct ged *gedp,
		  const char *name)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), name))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", name);
	return GED_ERROR;
    }

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_hide_view(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* return the hide view setting */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_hide_view);
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
    struct ged_dm_view *gdvp;
    int *need_refresh;
};


HIDDEN int
redraw_edited_path(struct bu_hash_entry *entry, void *udata)
{
    const char *av[5] = {0};
    char *draw_path = (char *)bu_get_hash_key(entry);
    struct redraw_edited_path_data *data;
    int ret, dmode = 0;
    struct bu_vls path_dmode = BU_VLS_INIT_ZERO;
    struct path_edit_params *params;

    data = (struct redraw_edited_path_data *)udata;

    params = (struct path_edit_params *)bu_get_hash_value(entry);
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

	bu_vls_printf(&tcl_cmd, "%s otranslate %s %s %s",
		      bu_vls_addr(&data->gdvp->gdv_edit_motion_delta_callback),
		      bu_vls_addr(&tran_x_vls), bu_vls_addr(&tran_y_vls),
		      bu_vls_addr(&tran_z_vls));
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&tcl_cmd));

	bu_vls_free(&tcl_cmd);
	bu_vls_free(&tran_x_vls);
	bu_vls_free(&tran_y_vls);
	bu_vls_free(&tran_z_vls);
    }

    Tcl_Eval(current_top->to_interp, "SetWaitCursor $::ArcherCore::application");

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
    Tcl_Eval(current_top->to_interp, "SetNormalCursor $::ArcherCore::application");

    *data->need_refresh = 1;

    return 0;
}


HIDDEN int
to_idle_mode(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;
    int mode, need_refresh = 0;
    struct redraw_edited_path_data data;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    mode = gdvp->gdv_view->gv_mode;

    if (gdvp->gdv_view->gv_adaptive_plot &&
	gdvp->gdv_view->gv_redraw_on_zoom &&
	mode == TCLCAD_SCALE_MODE)
    {
	char *av[] = {"redraw", NULL};

	ged_redraw(gedp, 1, (const char **)av);

	need_refresh = 1;
    }

    if (mode != TCLCAD_POLY_CONTOUR_MODE ||
	gdvp->gdv_view->gv_data_polygons.gdps_cflag == 0)
    {
	struct bu_vls bindings = BU_VLS_INIT_ZERO;

	bu_vls_printf(&bindings, "bind %s <Motion> {}",
		      bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)));
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
	bu_vls_free(&bindings);
    }

    if (gdvp->gdv_view->gv_grid.snap &&
	(mode == TCLCAD_TRANSLATE_MODE ||
	 mode == TCLCAD_CONSTRAINED_TRANSLATE_MODE))
    {
	char *av[3];

	gedp->ged_gvp = gdvp->gdv_view;
	av[0] = "grid";
	av[1] = "vsnap";
	av[2] = NULL;
	ged_grid(gedp, 2, (const char **)av);

	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    tclcad_eval_noresult(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback), 0, NULL);
	}

	need_refresh = 1;
    }

    /* redraw any edited paths, then clear them from our table */
    data.gedp = gedp;
    data.gdvp = gdvp;
    data.need_refresh = &need_refresh;
    bu_hash_tbl_traverse(current_top->to_gop->go_edited_paths, redraw_edited_path, &data);

    bu_hash_tbl_traverse(current_top->to_gop->go_edited_paths, free_path_edit_params_entry, NULL);
    bu_hash_tbl_free(current_top->to_gop->go_edited_paths);
    current_top->to_gop->go_edited_paths = bu_hash_tbl_create(0);

    if (need_refresh) {
	to_refresh_all_views(current_top);
    }

    gdvp->gdv_view->gv_mode = TCLCAD_IDLE_MODE;
    gdvp->gdv_view->gv_sdata_polygons.gdps_cflag = 0;

    return GED_OK;
}


HIDDEN int
to_is_viewable(struct ged_dm_view *gdvp)
{
    Tcl_Obj *our_result;
    Tcl_Obj *saved_result;
    int result_int;
    const char *pathname = bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp));

    /* stash any existing result so we can inspect our own */
    saved_result = Tcl_GetObjResult(current_top->to_interp);
    Tcl_IncrRefCount(saved_result);

    if (tclcad_eval(current_top->to_interp, "winfo viewable", 1, &pathname) != TCL_OK) {
	return 0;
    }

    our_result = Tcl_GetObjResult(current_top->to_interp);
    Tcl_GetIntFromObj(current_top->to_interp, our_result, &result_int);

    /* restore previous result */
    Tcl_SetObjResult(current_top->to_interp, saved_result);
    Tcl_DecrRefCount(saved_result);

    if (!result_int) {
	return 0;
    }

    return 1;
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get light flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_light_flag(gdvp->gdv_dmp));
	return GED_OK;
    }

    /* set light flag */
    if (bu_sscanf(argv[2], "%d", &light) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (light < 0)
	light = 0;

    (void)dm_make_current(gdvp->gdv_dmp);
    (void)dm_set_light(gdvp->gdv_dmp, light);
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
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l))
	bu_vls_printf(gedp->ged_result_str, "%s ", bu_vls_addr(&gdvp->gdv_name));

    return GED_OK;
}


HIDDEN int
to_listen(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  ged_func_ptr UNUSED(func),
	  const char *usage,
	  int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (gdvp->gdv_fbs.fbs_fbp == FB_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s listen: framebuffer not open!\n", argv[0]);
	return GED_ERROR;
    }

    /* return the port number */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_listener.fbsl_port);
	return GED_OK;
    }

    if (argc == 3) {
	int port;

	if (bu_sscanf(argv[2], "%d", &port) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "listen: bad value - %s\n", argv[2]);
	    return GED_ERROR;
	}

	if (port >= 0)
	    fbs_open(&gdvp->gdv_fbs, port);
	else {
	    fbs_close(&gdvp->gdv_fbs);
	}
	bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_listener.fbsl_port);
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
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

    bu_vls_printf(gedp->ged_result_str, "%lf", current_top->to_gop->go_gedp->ged_wdbp->dbip->dbi_local2base);

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
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	gedp->ged_gvp = gdvp->gdv_view;
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
    char *av[3];

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
    char *av[3];

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
to_model_axes(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    return to_axes(gedp, gdvp, &gdvp->gdv_view->gv_model_axes, argc, argv, usage);
}


HIDDEN int
to_edit_motion_delta_callback(struct ged *gedp,
			      int argc,
			      const char *argv[],
			      ged_func_ptr UNUSED(func),
			      const char *usage,
			      int UNUSED(maxargs))
{
    register int i;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get the callback string */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gdvp->gdv_edit_motion_delta_callback));

	return GED_OK;
    }

    /* set the callback string */
    bu_vls_trunc(&gdvp->gdv_edit_motion_delta_callback, 0);
    for (i = 2; i < argc; ++i)
	bu_vls_printf(&gdvp->gdv_edit_motion_delta_callback, "%s ", argv[i]);

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
    register int i;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the callback string */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&current_top->to_gop->go_more_args_callback));

	return GED_OK;
    }

    /* set the callback string */
    bu_vls_trunc(&current_top->to_gop->go_more_args_callback, 0);
    for (i = 1; i < argc; ++i)
	bu_vls_printf(&current_top->to_gop->go_more_args_callback, "%s ", argv[i]);

    return GED_OK;
}


HIDDEN int
to_mouse_append_pt_common(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int UNUSED(maxargs))
{
    int ret;
    char *av[4];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(view, x, y, 0.0);

    gedp->ged_gvp = gdvp->gdv_view;
    if (gedp->ged_gvp->gv_grid.snap)
	ged_snap_to_grid(gedp, &view[X], &view[Y]);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    ret = (*func)(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_brep_selection_append(struct ged *gedp,
			       int argc,
			       const char *argv[],
			       ged_func_ptr UNUSED(func),
			       const char *usage,
			       int maxargs)
{
    const char *cmd_argv[11] = {"brep", NULL, "selection", "append", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    struct ged_dm_view *gdvp;
    char *brep_name;
    char *end;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct bu_vls start[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    struct bu_vls dir[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    point_t screen_pt, view_pt, model_pt;
    vect_t view_dir, model_dir;
    mat_t invRot;

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    /* parse args */
    brep_name = (char *)bu_calloc(strlen(argv[2]), sizeof(char), "to_mouse_brep_selection_append brep_name");
    bu_basename(brep_name, argv[2]);

    screen_pt[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %d\n", screen_pt[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    screen_pt[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %d\n", screen_pt[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* stash point coordinates for future drag handling */
    gdvp->gdv_view->gv_prevMouseX = screen_pt[X];
    gdvp->gdv_view->gv_prevMouseY = screen_pt[Y];

    /* convert screen point to model-space start point and direction */
    view_pt[X] = screen_to_view_x(gdvp->gdv_dmp, screen_pt[X]);
    view_pt[Y] = screen_to_view_y(gdvp->gdv_dmp, screen_pt[Y]);
    view_pt[Z] = 1.0;

    MAT4X3PNT(model_pt, gdvp->gdv_view->gv_view2model, view_pt);

    VSET(view_dir, 0.0, 0.0, -1.0);
    bn_mat_inv(invRot, gedp->ged_gvp->gv_rotation);
    MAT4X3PNT(model_dir, invRot, view_dir);

    /* brep brep_name selection append selection_name startx starty startz dirx diry dirz */
    bu_vls_printf(&start[X], "%f", model_pt[X]);
    bu_vls_printf(&start[Y], "%f", model_pt[Y]);
    bu_vls_printf(&start[Z], "%f", model_pt[Z]);

    cmd_argv[1] = brep_name;
    cmd_argv[5] = bu_vls_addr(&start[X]);
    cmd_argv[6] = bu_vls_addr(&start[Y]);
    cmd_argv[7] = bu_vls_addr(&start[Z]);

    bu_vls_printf(&dir[X], "%f", model_dir[X]);
    bu_vls_printf(&dir[Y], "%f", model_dir[Y]);
    bu_vls_printf(&dir[Z], "%f", model_dir[Z]);

    cmd_argv[8] = bu_vls_addr(&dir[X]);
    cmd_argv[9] = bu_vls_addr(&dir[Y]);
    cmd_argv[10] = bu_vls_addr(&dir[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    ret = ged_brep(gedp, cmd_argc, cmd_argv);

    bu_vls_free(&start[X]);
    bu_vls_free(&start[Y]);
    bu_vls_free(&start[Z]);
    bu_vls_free(&dir[X]);
    bu_vls_free(&dir[Y]);
    bu_vls_free(&dir[Z]);

    if (ret != GED_OK) {
	return GED_ERROR;
    }

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_brep_selection_translate %s %s %%x %%y; "
		  "%s brep %s plot SCV}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  brep_name,
		  bu_vls_addr(&current_top->to_gop->go_name),
		  brep_name);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    bu_free((void *)brep_name, "brep_name");

    return GED_OK;
}


HIDDEN int
to_mouse_brep_selection_translate(struct ged *gedp,
				  int argc,
				  const char *argv[],
				  ged_func_ptr UNUSED(func),
				  const char *usage,
				  int maxargs)
{
    const char *cmd_argv[8] = {"brep", NULL, "selection", "translate", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    struct ged_dm_view *gdvp;
    char *brep_name;
    char *end;
    point_t screen_end, view_start, view_end, model_start, model_end;
    vect_t model_delta;
    struct bu_vls delta[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    brep_name = (char *)bu_calloc(strlen(argv[2]), sizeof(char), "to_mouse_brep_selection_translate brep_name");
    bu_basename(brep_name, argv[2]);

    screen_end[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %d\n", screen_end[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    screen_end[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %d\n", screen_end[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* convert screen-space delta to model-space delta */
    view_start[X] = screen_to_view_x(gdvp->gdv_dmp, gdvp->gdv_view->gv_prevMouseX);
    view_start[Y] = screen_to_view_y(gdvp->gdv_dmp, gdvp->gdv_view->gv_prevMouseY);
    view_start[Z] = 1;
    MAT4X3PNT(model_start, gdvp->gdv_view->gv_view2model, view_start);

    view_end[X] = screen_to_view_x(gdvp->gdv_dmp, screen_end[X]);
    view_end[Y] = screen_to_view_y(gdvp->gdv_dmp, screen_end[Y]);
    view_end[Z] = 1;
    MAT4X3PNT(model_end, gdvp->gdv_view->gv_view2model, view_end);

    VSUB2(model_delta, model_end, model_start);

    bu_vls_printf(&delta[X], "%f", model_delta[X]);
    bu_vls_printf(&delta[Y], "%f", model_delta[Y]);
    bu_vls_printf(&delta[Z], "%f", model_delta[Z]);

    cmd_argv[1] = brep_name;
    cmd_argv[5] = bu_vls_addr(&delta[X]);
    cmd_argv[6] = bu_vls_addr(&delta[Y]);
    cmd_argv[7] = bu_vls_addr(&delta[Z]);

    ret = ged_brep(gedp, cmd_argc, cmd_argv);

    bu_free((void *)brep_name, "brep_name");
    bu_vls_free(&delta[X]);
    bu_vls_free(&delta[Y]);
    bu_vls_free(&delta[Z]);

    if (ret != GED_OK) {
	return GED_ERROR;
    }

    /* need to tell front-end that we've modified the db */
    tclcad_eval_noresult(current_top->to_interp, "$::ArcherCore::application setSave", 0, NULL);

    gdvp->gdv_view->gv_prevMouseX = screen_end[X];
    gdvp->gdv_view->gv_prevMouseY = screen_end[Y];

    cmd_argc = 2;
    cmd_argv[0] = "draw";
    cmd_argv[1] = argv[2];
    cmd_argv[2] = NULL;
    ret = to_edit_redraw(gedp, cmd_argc, cmd_argv);

    return ret;
}


HIDDEN int
to_mouse_constrain_rot(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    int ret;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    fastf_t sf;
    struct bu_vls rot_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    switch (argv[2][0]) {
	case 'x':
	    bu_vls_printf(&rot_vls, "%lf 0 0", -sf);
	    break;
	case 'y':
	    bu_vls_printf(&rot_vls, "0 %lf 0", -sf);
	    break;
	case 'z':
	    bu_vls_printf(&rot_vls, "0 0 %lf", -sf);
    }

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "rot";
    av[1] = "-m";
    av[2] = bu_vls_addr(&rot_vls);
    av[3] = (char *)0;

    ret = ged_rot(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == GED_OK) {
	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    tclcad_eval_noresult(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback), 0, NULL);
	}

	to_refresh_view(gdvp);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_constrain_trans(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr UNUSED(func),
			 const char *usage,
			 int UNUSED(maxargs))
{
    int width;
    int ret;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls tran_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if ((argv[2][0] != 'x' && argv[2][0] != 'y' && argv[2][0] != 'z') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;

    if (fabs(dx) > fabs(dy))
	sf = dx;
    else
	sf = dy;

    switch (argv[2][0]) {
	case 'x':
	    bu_vls_printf(&tran_vls, "%lf 0 0", -sf);
	    break;
	case 'y':
	    bu_vls_printf(&tran_vls, "0 %lf 0", -sf);
	    break;
	case 'z':
	    bu_vls_printf(&tran_vls, "0 0 %lf", -sf);
    }

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "tra";
    av[1] = "-m";
    av[2] = bu_vls_addr(&tran_vls);
    av[3] = (char *)0;

    ret = ged_tra(gedp, ac, (const char **)av);
    bu_vls_free(&tran_vls);

    if (ret == GED_OK) {
	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    tclcad_eval_noresult(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback), 0, NULL);
	}

	to_refresh_view(gdvp);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_find_arb_edge(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    char *av[6];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(view, x, y, 0.0);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "find_arb_edge_nearest_pt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)argv[5];
    av[4] = (char *)0;

    (void)ged_find_arb_edge_nearest_pt(gedp, 4, (const char **)av);
    bu_vls_free(&pt_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_find_bot_edge(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    char *av[6];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(view, x, y, 0.0);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "find_bot_edge_nearest_pt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_find_bot_edge_nearest_pt(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_find_botpt(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    char *av[6];
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(view, x, y, 0.0);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", view[X], view[Y], view[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "find_botpt_nearest_pt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_find_botpt_nearest_pt(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_find_metaballpt(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr UNUSED(func),
			 const char *usage,
			 int UNUSED(maxargs))
{
    char *av[6];
    point_t model;
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(view, x, y, 0.0);
    MAT4X3PNT(model, gdvp->gdv_view->gv_view2model, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "find_metaballpt_nearest_pt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_find_metaballpt_nearest_pt(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_find_pipept(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr UNUSED(func),
		     const char *usage,
		     int UNUSED(maxargs))
{
    char *av[6];
    point_t model;
    point_t view;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(view, x, y, 0.0);
    MAT4X3PNT(model, gdvp->gdv_view->gv_view2model, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "find_pipept_nearest_pt";
    av[1] = (char *)argv[2];
    av[2] = bu_vls_addr(&pt_vls);
    av[3] = (char *)0;

    (void)ged_find_pipept_nearest_pt(gedp, 3, (const char **)av);
    bu_vls_free(&pt_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_joint_select(
    struct ged *gedp,
    int argc,
    const char *argv[],
    ged_func_ptr UNUSED(func),
    const char *usage,
    int maxargs)
{
    const char *cmd_argv[11] = {"joint2", NULL, "selection", "replace", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    struct ged_dm_view *gdvp;
    char *joint_name;
    char *end;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct bu_vls start[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    struct bu_vls dir[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};
    point_t screen_pt, view_pt, model_pt;
    vect_t view_dir, model_dir;
    mat_t invRot;

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    /* parse args */
    joint_name = (char *)bu_calloc(strlen(argv[2]), sizeof(char), "joint_name");
    bu_basename(joint_name, argv[2]);

    screen_pt[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %d\n", screen_pt[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    screen_pt[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %d\n", screen_pt[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* stash point coordinates for future drag handling */
    gdvp->gdv_view->gv_prevMouseX = screen_pt[X];
    gdvp->gdv_view->gv_prevMouseY = screen_pt[Y];

    /* convert screen point to model-space start point and direction */
    view_pt[X] = screen_to_view_x(gdvp->gdv_dmp, screen_pt[X]);
    view_pt[Y] = screen_to_view_y(gdvp->gdv_dmp, screen_pt[Y]);
    view_pt[Z] = 1.0;

    MAT4X3PNT(model_pt, gdvp->gdv_view->gv_view2model, view_pt);

    VSET(view_dir, 0.0, 0.0, -1.0);
    bn_mat_inv(invRot, gedp->ged_gvp->gv_rotation);
    MAT4X3PNT(model_dir, invRot, view_dir);

    /* joint2 joint_name selection append selection_name startx starty startz dirx diry dirz */
    bu_vls_printf(&start[X], "%f", model_pt[X]);
    bu_vls_printf(&start[Y], "%f", model_pt[Y]);
    bu_vls_printf(&start[Z], "%f", model_pt[Z]);

    cmd_argv[1] = joint_name;
    cmd_argv[5] = bu_vls_addr(&start[X]);
    cmd_argv[6] = bu_vls_addr(&start[Y]);
    cmd_argv[7] = bu_vls_addr(&start[Z]);

    bu_vls_printf(&dir[X], "%f", model_dir[X]);
    bu_vls_printf(&dir[Y], "%f", model_dir[Y]);
    bu_vls_printf(&dir[Z], "%f", model_dir[Z]);

    cmd_argv[8] = bu_vls_addr(&dir[X]);
    cmd_argv[9] = bu_vls_addr(&dir[Y]);
    cmd_argv[10] = bu_vls_addr(&dir[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    ret = ged_joint2(gedp, cmd_argc, cmd_argv);

    bu_vls_free(&start[X]);
    bu_vls_free(&start[Y]);
    bu_vls_free(&start[Z]);
    bu_vls_free(&dir[X]);
    bu_vls_free(&dir[Y]);
    bu_vls_free(&dir[Z]);

    if (ret != GED_OK) {
	return GED_ERROR;
    }

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_joint_selection_translate %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  joint_name);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    bu_free((void *)joint_name, "joint_name");

    return GED_OK;
}


HIDDEN int
to_mouse_joint_selection_translate(
    struct ged *gedp,
    int argc,
    const char *argv[],
    ged_func_ptr UNUSED(func),
    const char *usage,
    int maxargs)
{
    const char *cmd_argv[8] = {"joint2", NULL, "selection", "translate", "active"};
    int ret, cmd_argc = (int)(sizeof(cmd_argv) / sizeof(const char *));
    struct ged_dm_view *gdvp;
    char *joint_name;
    char *end;
    point_t screen_end, view_start, view_end, model_start, model_end;
    vect_t model_delta;
    struct bu_vls delta[] = {BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO};

    if (argc != maxargs) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    joint_name = (char *)bu_calloc(strlen(argv[2]), sizeof(char), "joint_name");
    bu_basename(joint_name, argv[2]);

    screen_end[X] = strtol(argv[3], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad x value %d\n", screen_end[X]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    screen_end[Y] = strtol(argv[4], &end, 10);
    if (*end != '\0') {
	bu_vls_printf(gedp->ged_result_str, "ERROR: bad y value: %d\n", screen_end[Y]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* convert screen-space delta to model-space delta */
    view_start[X] = screen_to_view_x(gdvp->gdv_dmp, gdvp->gdv_view->gv_prevMouseX);
    view_start[Y] = screen_to_view_y(gdvp->gdv_dmp, gdvp->gdv_view->gv_prevMouseY);
    view_start[Z] = 1;
    MAT4X3PNT(model_start, gdvp->gdv_view->gv_view2model, view_start);

    view_end[X] = screen_to_view_x(gdvp->gdv_dmp, screen_end[X]);
    view_end[Y] = screen_to_view_y(gdvp->gdv_dmp, screen_end[Y]);
    view_end[Z] = 1;
    MAT4X3PNT(model_end, gdvp->gdv_view->gv_view2model, view_end);

    VSUB2(model_delta, model_end, model_start);

    bu_vls_printf(&delta[X], "%f", model_delta[X]);
    bu_vls_printf(&delta[Y], "%f", model_delta[Y]);
    bu_vls_printf(&delta[Z], "%f", model_delta[Z]);

    cmd_argv[1] = joint_name;
    cmd_argv[5] = bu_vls_addr(&delta[X]);
    cmd_argv[6] = bu_vls_addr(&delta[Y]);
    cmd_argv[7] = bu_vls_addr(&delta[Z]);

    ret = ged_joint2(gedp, cmd_argc, cmd_argv);

    if (ret != GED_OK) {
	bu_free((void *)joint_name, "joint_name");
	bu_vls_free(&delta[X]);
	bu_vls_free(&delta[Y]);
	bu_vls_free(&delta[Z]);
	return GED_ERROR;
    }

    /* need to tell front-end that we've modified the db */
    Tcl_Eval(current_top->to_interp, "$::ArcherCore::application setSave");

    gdvp->gdv_view->gv_prevMouseX = screen_end[X];
    gdvp->gdv_view->gv_prevMouseY = screen_end[Y];

    cmd_argc = 3;
    cmd_argv[0] = "get";
    cmd_argv[1] = joint_name;
    cmd_argv[2] = "RP1";
    cmd_argv[3] = NULL;
    ret = ged_get(gedp, cmd_argc, cmd_argv);

    if (ret == GED_OK) {
	char *path_name = bu_strdup(bu_vls_cstr(gedp->ged_result_str));
	int dmode = 0;
	struct bu_vls path_dmode = BU_VLS_INIT_ZERO;

	/* get current display mode of path */
	cmd_argc = 2;
	cmd_argv[0] = "how";
	cmd_argv[1] = path_name;
	cmd_argv[2] = NULL;
	ret = ged_how(gedp, cmd_argc, cmd_argv);

	if (ret == GED_OK) {
	    ret = bu_sscanf(bu_vls_cstr(gedp->ged_result_str), "%d", &dmode);
	}
	if (dmode == 4) {
	    bu_vls_printf(&path_dmode, "-h");
	} else {
	    bu_vls_printf(&path_dmode, "-m%d", dmode);
	}

	/* erase path to split it from visible vlists */
	cmd_argc = 2;
	cmd_argv[0] = "erase";
	cmd_argv[1] = path_name;
	cmd_argv[2] = NULL;
	ret = ged_erase(gedp, cmd_argc, cmd_argv);

	if (ret == GED_OK) {
	    /* redraw path with its previous display mode */
	    cmd_argc = 4;
	    cmd_argv[0] = "draw";
	    cmd_argv[1] = "-R";
	    cmd_argv[2] = bu_vls_cstr(&path_dmode);
	    cmd_argv[3] = path_name;
	    cmd_argv[4] = NULL;
	    ret = ged_draw(gedp, cmd_argc, cmd_argv);

	    to_refresh_all_views(current_top);
	}
	bu_vls_free(&path_dmode);
	bu_free(path_name, "path_name");
    }

    bu_free((void *)joint_name, "joint_name");
    bu_vls_free(&delta[X]);
    bu_vls_free(&delta[Y]);
    bu_vls_free(&delta[Z]);

    return ret;
}


HIDDEN int
to_mouse_move_arb_edge(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    int width;
    int ret;
    char *av[6];
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_move_arb_edge expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

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
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_move_arb_face(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    int width;
    int ret;
    char *av[6];
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_move_arb_face expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

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
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_move_botpt(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    int width;
    int ret;
    int rflag;
    char *av[6];
    const char *cmd;
    fastf_t dx, dy, dz;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t v2m_mat;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_HELP;
    }

    if (argc == 7) {
	if (argv[1][0] != '-' || argv[1][1] != 'r' || argv[1][2] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}

	rflag = 1;
	--argc;
	++argv;
    } else
	rflag = 0;

    if (argc != 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "%s: View not found - %s", cmd, argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;

    if (rflag) {
	dx = x - gdvp->gdv_view->gv_prevMouseX;
	dy = gdvp->gdv_view->gv_prevMouseY - y;
	dz = 0.0;

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

	bn_mat_inv(v2m_mat, gdvp->gdv_view->gv_rotation);

	dx *= inv_width * gdvp->gdv_view->gv_size;
	dy *= inv_width * gdvp->gdv_view->gv_size;
    } else {
	struct rt_db_internal intern;
	struct rt_bot_internal *botip;
	mat_t mat;
	size_t vertex_i;
	char *last;

	if ((last = strrchr(argv[2], '/')) == NULL)
	    last = (char *)argv[2];
	else
	    ++last;

	if (last[0] == '\0') {
	    bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", cmd, argv[2]);
	    return GED_ERROR;
	}

	if (bu_sscanf(argv[3], "%zu", &vertex_i) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad bot vertex index - %s", cmd, argv[3]);
	    return GED_ERROR;
	}

	if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[2], gedp->ged_wdbp, mat) == GED_ERROR) {
	    bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, argv[2]);
	    return GED_ERROR;
	}

	if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	    intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	    bu_vls_printf(gedp->ged_result_str, "Object is not a BOT");
	    rt_db_free_internal(&intern);

	    return GED_ERROR;
	}

	botip = (struct rt_bot_internal *)intern.idb_ptr;

	if (vertex_i >= botip->num_vertices) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad bot vertex index - %s", cmd, argv[3]);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	MAT4X3PNT(view, gdvp->gdv_view->gv_model2view, &botip->vertices[vertex_i*3]);
	MAT_COPY(v2m_mat, gdvp->gdv_view->gv_view2model);

	dx = screen_to_view_x(gdvp->gdv_dmp, x);
	dy = screen_to_view_y(gdvp->gdv_dmp, y);
	dz = view[Z];

	rt_db_free_internal(&intern);
    }

    VSET(view, dx, dy, dz);
    MAT4X3PNT(model, v2m_mat, view);

    /* ged_move_botpt expects things to be in local units */
    VSCALE(model, model, gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "move_botpt";

    if (rflag) {
	av[1] = "-r";
	av[2] = (char *)argv[2];
	av[3] = (char *)argv[3];
	av[4] = bu_vls_addr(&pt_vls);
	av[5] = (char *)0;

	ret = ged_move_botpt(gedp, 5, (const char **)av);
    } else {
	av[1] = (char *)argv[2];
	av[2] = (char *)argv[3];
	av[3] = bu_vls_addr(&pt_vls);
	av[4] = (char *)0;

	ret = ged_move_botpt(gedp, 4, (const char **)av);
    }

    bu_vls_free(&pt_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_move_botpts(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr UNUSED(func),
		     const char *usage,
		     int UNUSED(maxargs))
{
    int ret, width;
    const char *cmd;
    fastf_t dx, dy, dz;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t v2m_mat;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

    /* must be double for scanf */
    double x, y;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_HELP;
    }

    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "%s: View not found - %s", cmd, argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = gdvp->gdv_view->gv_prevMouseY - y;
    dz = 0.0;

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

    bn_mat_inv(v2m_mat, gdvp->gdv_view->gv_rotation);

    dx *= inv_width * gdvp->gdv_view->gv_size;
    dy *= inv_width * gdvp->gdv_view->gv_size;

    VSET(view, dx, dy, dz);
    MAT4X3PNT(model, v2m_mat, view);

    /* ged_move_botpts expects things to be in local units */
    VSCALE(model, model, gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;

    {
	register int i, j;
	int ac = argc - 2;
	char **av = (char **)bu_calloc(ac, sizeof(char *), "to_mouse_move_botpts: av[]");
	av[0] = "move_botpts";

	av[1] = (char *)argv[4];
	av[2] = bu_vls_addr(&pt_vls);
	av[ac-1] = (char *)0;

	for (i=3, j=5; i < ac; ++i, ++j)
	    av[i] = (char *)argv[j];

	ret = ged_move_botpts(gedp, ac, (const char **)av);
	bu_vls_free(&pt_vls);

	if (ret == GED_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[4];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}

	bu_free((void *)av, "to_mouse_move_botpts: av[]");
    }

    return GED_OK;
}


HIDDEN int
to_mouse_move_pt_common(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int UNUSED(maxargs))
{
    int ret, width;
    char *av[6];
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_move_pipept expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&pt_vls, "%lf %lf %lf", model[X], model[Y], model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;
    av[0] = (char *)argv[0];
    av[1] = "-r";
    av[2] = (char *)argv[2];
    av[3] = (char *)argv[3];
    av[4] = bu_vls_addr(&pt_vls);
    av[5] = (char *)0;

    ret = (*func)(gedp, 5, (const char **)av);
    bu_vls_free(&pt_vls);

    if (ret == GED_OK) {
	av[0] = "draw";
	av[1] = (char *)argv[2];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_orotate(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls rot_x_vls = BU_VLS_INIT_ZERO;
    struct bu_vls rot_y_vls = BU_VLS_INIT_ZERO;
    struct bu_vls rot_z_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    bu_vls_printf(&rot_x_vls, "%lf", model[X]);
    bu_vls_printf(&rot_y_vls, "%lf", model[Y]);
    bu_vls_printf(&rot_z_vls, "%lf", model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;

    if (0 < bu_vls_strlen(&gdvp->gdv_edit_motion_delta_callback)) {
	const char *command = bu_vls_addr(&gdvp->gdv_edit_motion_delta_callback);
	const char *args[4];
	args[0] = "orotate";
	args[1] = bu_vls_addr(&rot_x_vls);
	args[2] = bu_vls_addr(&rot_y_vls);
	args[3] = bu_vls_addr(&rot_z_vls);
	tclcad_eval(current_top->to_interp, command, sizeof(args) / sizeof(args[0]), args);
    } else {
	char *av[6];

	av[0] = "orotate";
	av[1] = (char *)argv[2];
	av[2] = bu_vls_addr(&rot_x_vls);
	av[3] = bu_vls_addr(&rot_y_vls);
	av[4] = bu_vls_addr(&rot_z_vls);
	av[5] = (char *)0;

	if (ged_orotate(gedp, 5, (const char **)av) == GED_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}
    }

    bu_vls_free(&rot_x_vls);
    bu_vls_free(&rot_y_vls);
    bu_vls_free(&rot_z_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_oscale(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr UNUSED(func),
		const char *usage,
		int UNUSED(maxargs))
{
    int width;
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls sf_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gdv_view->gv_sscale;
    dy *= inv_width * gdvp->gdv_view->gv_sscale;

    if (fabs(dx) < fabs(dy))
	sf = 1.0 + dy;
    else
	sf = 1.0 + dx;

    bu_vls_printf(&sf_vls, "%lf", sf);

    gedp->ged_gvp = gdvp->gdv_view;

    if (0 < bu_vls_strlen(&gdvp->gdv_edit_motion_delta_callback)) {
	struct bu_vls tcl_cmd;

	bu_vls_init(&tcl_cmd);
	bu_vls_printf(&tcl_cmd, "%s oscale %s", bu_vls_addr(&gdvp->gdv_edit_motion_delta_callback), bu_vls_addr(&sf_vls));
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&tcl_cmd));
	bu_vls_free(&tcl_cmd);
    } else {
	char *av[6];

	av[0] = "oscale";
	av[1] = (char *)argv[2];
	av[2] = bu_vls_addr(&sf_vls);
	av[3] = (char *)0;

	if (ged_oscale(gedp, 3, (const char **)av) == GED_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}
    }

    bu_vls_free(&sf_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_otranslate(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    int width;
    fastf_t dx, dy;
    fastf_t inv_width;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls tran_x_vls = BU_VLS_INIT_ZERO;
    struct bu_vls tran_y_vls = BU_VLS_INIT_ZERO;
    struct bu_vls tran_z_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_otranslate expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

    bu_vls_printf(&tran_x_vls, "%lf", model[X]);
    bu_vls_printf(&tran_y_vls, "%lf", model[Y]);
    bu_vls_printf(&tran_z_vls, "%lf", model[Z]);

    gedp->ged_gvp = gdvp->gdv_view;

    if (0 < bu_vls_strlen(&gdvp->gdv_edit_motion_delta_callback)) {
	const char *path_string = argv[2];
	struct path_edit_params *params;
	int is_entry_new;
	struct bu_hash_entry *entry;
	mat_t dmat, prev_mat;
	vect_t dvec;

	entry = bu_hash_tbl_add(current_top->to_gop->go_edited_paths,
				(unsigned char *)path_string,
				sizeof(char) * strlen(path_string) + 1,
				&is_entry_new);

	if (is_entry_new) {
	    BU_GET(params, struct path_edit_params);
	    MAT_IDN(params->edit_mat);
	    params->edit_mode = gdvp->gdv_view->gv_mode;

	    bu_set_hash_value(entry, (unsigned char *)params);
	} else {
	    params = (struct path_edit_params *)bu_get_hash_value(entry);
	}

	MAT_IDN(dmat);
	VSCALE(dvec, model, gedp->ged_wdbp->dbip->dbi_local2base);
	MAT_DELTAS_VEC(dmat, dvec);

	MAT_COPY(prev_mat, params->edit_mat);
	bn_mat_mul(params->edit_mat, prev_mat, dmat);


	to_refresh_view(gdvp);
    } else {
	char *av[6];

	av[0] = "otranslate";
	av[1] = (char *)argv[2];
	av[2] = bu_vls_addr(&tran_x_vls);
	av[3] = bu_vls_addr(&tran_y_vls);
	av[4] = bu_vls_addr(&tran_z_vls);
	av[5] = (char *)0;

	if (ged_otranslate(gedp, 5, (const char **)av) == GED_OK) {
	    av[0] = "draw";
	    av[1] = (char *)argv[2];
	    av[2] = (char *)0;
	    to_edit_redraw(gedp, 2, (const char **)av);
	}
    }

    bu_vls_free(&tran_x_vls);
    bu_vls_free(&tran_y_vls);
    bu_vls_free(&tran_z_vls);

    return GED_OK;
}


HIDDEN int
to_mouse_poly_circ(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    int ac;
    char *av[7];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls i_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);

    bu_vls_printf(&plist, "{0 ");

    {
	vect_t vdiff;
	fastf_t r, arc;
	fastf_t curr_fx, curr_fy;
	register int nsegs, n;

	VSET(v_pt, fx, fy, gdvp->gdv_view->gv_data_vZ);
	VSUB2(vdiff, v_pt, gdpsp->gdps_prev_point);
	r = MAGNITUDE(vdiff);

	/* use a variable number of segments based on the size of the
	 * circle being created so small circles have few segments and
	 * large ones are nice and smooth.  select a chord length that
	 * results in segments approximately 4 pixels in length.
	 *
	 * circumference / 4 = PI * diameter / 4
	 *
	 */
	nsegs = M_PI_2 * r * gdvp->gdv_view->gv_scale;

	if (nsegs < 32)
	    nsegs = 32;

	arc = 360.0 / nsegs;
	for (n = 0; n < nsegs; ++n) {
	    fastf_t ang = n * arc;

	    curr_fx = cos(ang*DEG2RAD) * r + gdpsp->gdps_prev_point[X];
	    curr_fy = sin(ang*DEG2RAD) * r + gdpsp->gdps_prev_point[Y];
	    VSET(v_pt, curr_fx, curr_fy, gdvp->gdv_view->gv_data_vZ);
	    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
	    bu_vls_printf(&plist, " {%lf %lf %lf}", V3ARGS(m_pt));
	}
    }

    bu_vls_printf(&plist, " }");
    bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 5;
    av[0] = "data_polygons";
    av[1] = bu_vls_addr(&gdvp->gdv_name);
    av[2] = "replace_poly";
    av[3] = bu_vls_addr(&i_vls);
    av[4] = bu_vls_addr(&plist);
    av[5] = (char *)0;

    (void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
    bu_vls_free(&plist);
    bu_vls_free(&i_vls);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_mouse_poly_cont(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    int ac;
    char *av[8];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(v_pt, fx, fy, gdvp->gdv_view->gv_data_vZ);

    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
    gedp->ged_gvp = gdvp->gdv_view;

    {
	struct bu_vls i_vls = BU_VLS_INIT_ZERO;
	struct bu_vls k_vls = BU_VLS_INIT_ZERO;
	struct bu_vls plist = BU_VLS_INIT_ZERO;

	bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);
	bu_vls_printf(&k_vls, "%zu", gdpsp->gdps_curr_point_i);
	bu_vls_printf(&plist, "%lf %lf %lf", V3ARGS(m_pt));

	ac = 7;
	av[0] = "data_polygons";
	av[1] = bu_vls_addr(&gdvp->gdv_name);
	av[2] = "replace_point";
	av[3] = bu_vls_addr(&i_vls);
	av[4] = "0";
	av[5] = bu_vls_addr(&k_vls);
	av[6] = bu_vls_addr(&plist);
	av[7] = (char *)0;

	(void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
	bu_vls_free(&i_vls);
	bu_vls_free(&k_vls);
	bu_vls_free(&plist);
    }

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_mouse_poly_ell(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    int ac;
    char *av[7];
    int x, y;
    fastf_t fx, fy;
    point_t m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls i_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);

    bu_vls_printf(&plist, "{0 ");

    {
	fastf_t a, b, arc;
	point_t ellout;
	point_t A, B;
	register int nsegs, n;

	a = fx - gdpsp->gdps_prev_point[X];
	b = fy - gdpsp->gdps_prev_point[Y];

	/*
	 * For angle alpha, compute surface point as
	 *
	 * V + cos(alpha) * A + sin(alpha) * B
	 *
	 * note that sin(alpha) is cos(90-alpha).
	 */

	VSET(A, a, 0, gdvp->gdv_view->gv_data_vZ);
	VSET(B, 0, b, gdvp->gdv_view->gv_data_vZ);

	/* use a variable number of segments based on the size of the
	 * circle being created so small circles have few segments and
	 * large ones are nice and smooth.  select a chord length that
	 * results in segments approximately 4 pixels in length.
	 *
	 * circumference / 4 = PI * diameter / 4
	 *
	 */
	nsegs = M_PI_2 * FMAX(a, b) * gdvp->gdv_view->gv_scale;

	if (nsegs < 32)
	    nsegs = 32;

	arc = 360.0 / nsegs;
	for (n = 0; n < nsegs; ++n) {
	    fastf_t cosa = cos(n * arc * DEG2RAD);
	    fastf_t sina = sin(n * arc * DEG2RAD);

	    VJOIN2(ellout, gdpsp->gdps_prev_point, cosa, A, sina, B);
	    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, ellout);
	    bu_vls_printf(&plist, " {%lf %lf %lf}", V3ARGS(m_pt));
	}
    }

    bu_vls_printf(&plist, " }");
    bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 5;
    av[0] = "data_polygons";
    av[1] = bu_vls_addr(&gdvp->gdv_name);
    av[2] = "replace_poly";
    av[3] = bu_vls_addr(&i_vls);
    av[4] = bu_vls_addr(&plist);
    av[5] = (char *)0;

    (void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
    bu_vls_free(&plist);
    bu_vls_free(&i_vls);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_mouse_poly_rect(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    int ac;
    char *av[7];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls i_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);

    if (gdvp->gdv_view->gv_mode == TCLCAD_POLY_SQUARE_MODE) {
	fastf_t dx, dy;

	dx = fx - gdpsp->gdps_prev_point[X];
	dy = fy - gdpsp->gdps_prev_point[Y];

	if (fabs(dx) > fabs(dy)) {
	    if (dy < 0.0)
		fy = gdpsp->gdps_prev_point[Y] - fabs(dx);
	    else
		fy = gdpsp->gdps_prev_point[Y] + fabs(dx);
	} else {
	    if (dx < 0.0)
		fx = gdpsp->gdps_prev_point[X] - fabs(dy);
	    else
		fx = gdpsp->gdps_prev_point[X] + fabs(dy);
	}
    }

    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, gdpsp->gdps_prev_point);
    bu_vls_printf(&plist, "{0 {%lf %lf %lf} ",  V3ARGS(m_pt));

    VSET(v_pt, gdpsp->gdps_prev_point[X], fy, gdvp->gdv_view->gv_data_vZ);
    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
    bu_vls_printf(&plist, "{%lf %lf %lf} ",  V3ARGS(m_pt));

    VSET(v_pt, fx, fy, gdvp->gdv_view->gv_data_vZ);
    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
    bu_vls_printf(&plist, "{%lf %lf %lf} ",  V3ARGS(m_pt));
    VSET(v_pt, fx, gdpsp->gdps_prev_point[Y], gdvp->gdv_view->gv_data_vZ);
    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
    bu_vls_printf(&plist, "{%lf %lf %lf} }",  V3ARGS(m_pt));

    bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 5;
    av[0] = "data_polygons";
    av[1] = bu_vls_addr(&gdvp->gdv_name);
    av[2] = "replace_poly";
    av[3] = bu_vls_addr(&i_vls);
    av[4] = bu_vls_addr(&plist);
    av[5] = (char *)0;

    (void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
    bu_vls_free(&plist);
    bu_vls_free(&i_vls);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_mouse_ray(struct ged *UNUSED(gedp),
	     int UNUSED(argc),
	     const char *UNUSED(argv[]),
	     ged_func_ptr UNUSED(func),
	     const char *UNUSED(usage),
	     int UNUSED(maxargs))
{
    return GED_OK;
}


HIDDEN int
to_mouse_rect(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    int ret;
    int ac;
    char *av[5];
    int x, y;
    int dx, dy;
    struct bu_vls dx_vls = BU_VLS_INIT_ZERO;
    struct bu_vls dy_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    dx = x - gdvp->gdv_view->gv_prevMouseX;
    dy = dm_get_height(gdvp->gdv_dmp) - y - gdvp->gdv_view->gv_prevMouseY;

    bu_vls_printf(&dx_vls, "%d", dx);
    bu_vls_printf(&dy_vls, "%d", dy);
    gedp->ged_gvp = gdvp->gdv_view;
    ac = 4;
    av[0] = "rect";
    av[1] = "dim";
    av[2] = bu_vls_addr(&dx_vls);
    av[3] = bu_vls_addr(&dy_vls);
    av[4] = (char *)0;

    ret = ged_rect(gedp, ac, (const char **)av);
    bu_vls_free(&dx_vls);
    bu_vls_free(&dy_vls);

    if (ret == GED_OK)
	to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_mouse_rot(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    int ret;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    struct bu_vls rot_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    bu_vls_printf(&rot_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "rot";
    av[1] = "-v";
    av[2] = bu_vls_addr(&rot_vls);
    av[3] = (char *)0;

    ret = ged_rot(gedp, ac, (const char **)av);
    bu_vls_free(&rot_vls);

    if (ret == GED_OK) {
	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback));
	}

	to_refresh_view(gdvp);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_rotate_arb_face(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr UNUSED(func),
			 const char *usage,
			 int UNUSED(maxargs))
{
    int ret;
    char *av[6];
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls pt_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[5], "%lf", &x) != 1 ||
	bu_sscanf(argv[6], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


#define TO_COMMON_MOUSE_SCALE(_gdvp, _zoom_vls, _argc, _argv, _usage) { \
    int _width; \
    fastf_t _dx, _dy; \
    fastf_t _inv_width; \
    fastf_t _sf; \
 \
    /* must be double for scanf */ \
    double _x, _y; \
 \
    /* initialize result */ \
    bu_vls_trunc(gedp->ged_result_str, 0); \
 \
    /* must be wanting help */ \
    if ((_argc) == 1) { \
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", (_argv)[0], (_usage)); \
	return GED_HELP; \
    } \
 \
    if ((_argc) != 4) { \
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", (_argv)[0], (_usage)); \
	return GED_ERROR; \
    } \
 \
    for (BU_LIST_FOR((_gdvp), ged_dm_view, &current_top->to_gop->go_head_views.l)) { \
	if (BU_STR_EQUAL(bu_vls_addr(&(_gdvp)->gdv_name), (_argv)[1])) \
	    break; \
    } \
 \
    if (BU_LIST_IS_HEAD(&(_gdvp)->l, &current_top->to_gop->go_head_views.l)) { \
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", (_argv)[1]); \
	return GED_ERROR; \
    } \
 \
    if (bu_sscanf((_argv)[2], "%lf", &_x) != 1 || \
	bu_sscanf((_argv)[3], "%lf", &_y) != 1) { \
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", (_argv)[0], (_usage)); \
	return GED_ERROR; \
    } \
 \
    _dx = _x - (_gdvp)->gdv_view->gv_prevMouseX; \
    _dy = (_gdvp)->gdv_view->gv_prevMouseY - _y; \
 \
    (_gdvp)->gdv_view->gv_prevMouseX = _x; \
    (_gdvp)->gdv_view->gv_prevMouseY = _y; \
 \
    if (_dx < (_gdvp)->gdv_view->gv_minMouseDelta) \
	_dx = (_gdvp)->gdv_view->gv_minMouseDelta; \
    else if ((_gdvp)->gdv_view->gv_maxMouseDelta < _dx) \
	_dx = (_gdvp)->gdv_view->gv_maxMouseDelta; \
 \
    if (_dy < (_gdvp)->gdv_view->gv_minMouseDelta) \
	_dy = (_gdvp)->gdv_view->gv_minMouseDelta; \
    else if ((_gdvp)->gdv_view->gv_maxMouseDelta < _dy) \
	_dy = (_gdvp)->gdv_view->gv_maxMouseDelta; \
 \
    _width = dm_get_width((_gdvp)->gdv_dmp); \
    _inv_width = 1.0 / (fastf_t)_width; \
    _dx *= _inv_width * (_gdvp)->gdv_view->gv_sscale; \
    _dy *= _inv_width * (_gdvp)->gdv_view->gv_sscale; \
 \
    if (fabs(_dx) > fabs(_dy)) \
	_sf = 1.0 + _dx; \
    else \
	_sf = 1.0 + _dy; \
 \
    bu_vls_printf(&(_zoom_vls), "%lf", _sf);	\
}


HIDDEN int
to_mouse_data_scale(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    int ret;
    char *av[4];
    struct bu_vls scale_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

    TO_COMMON_MOUSE_SCALE(gdvp, scale_vls, argc, argv, usage);
    gedp->ged_gvp = gdvp->gdv_view;

    av[0] = "to_data_scale";
    av[1] = (char *)argv[1];
    av[2] = bu_vls_addr(&scale_vls);
    av[3] = (char *)0;

    ret = to_data_scale(gedp, 3, (const char **)av, (ged_func_ptr)NULL, NULL, 4);

    bu_vls_free(&scale_vls);

    return ret;
}


HIDDEN int
to_mouse_scale(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    int ret;
    char *av[3];
    struct bu_vls zoom_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

    TO_COMMON_MOUSE_SCALE(gdvp, zoom_vls, argc, argv, usage);
    gedp->ged_gvp = gdvp->gdv_view;

    av[0] = "zoom";
    av[1] = bu_vls_addr(&zoom_vls);
    av[2] = (char *)0;
    ret = ged_zoom(gedp, 2, (const char **)av);
    bu_vls_free(&zoom_vls);

    if (ret == GED_OK) {
	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback));
	}

	to_refresh_view(gdvp);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_protate(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    int ret;
    char *av[6];
    fastf_t dx, dy;
    point_t model;
    point_t view;
    mat_t inv_rot;
    struct bu_vls mrot_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_pscale(struct ged *gedp,
		int argc,
		const char *argv[],
		ged_func_ptr UNUSED(func),
		const char *usage,
		int UNUSED(maxargs))
{
    int ret, width;
    char *av[6];
    fastf_t dx, dy;
    fastf_t sf;
    fastf_t inv_width;
    struct bu_vls sf_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gdv_view->gv_sscale;
    dy *= inv_width * gdvp->gdv_view->gv_sscale;

    if (fabs(dx) < fabs(dy))
	sf = 1.0 + dy;
    else
	sf = 1.0 + dx;

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
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_ptranslate(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    int ret, width;
    char *av[6];
    fastf_t dx, dy;
    point_t model;
    point_t view;
    fastf_t inv_width;
    mat_t inv_rot;
    struct bu_vls tvec_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    /* ged_ptranslate expects things to be in local units */
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_base2local;
    VSET(view, dx, dy, 0.0);
    bn_mat_inv(inv_rot, gdvp->gdv_view->gv_rotation);
    MAT4X3PNT(model, inv_rot, view);

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
	to_edit_redraw(gedp, 2, (const char **)av);
    }

    return GED_OK;
}


HIDDEN int
to_mouse_trans(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    int ret, width;
    int ac;
    char *av[4];
    fastf_t dx, dy;
    fastf_t inv_width;
    struct bu_vls trans_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
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

    width = dm_get_width(gdvp->gdv_dmp);
    inv_width = 1.0 / (fastf_t)width;
    dx *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;
    dy *= inv_width * gdvp->gdv_view->gv_size * gedp->ged_wdbp->dbip->dbi_local2base;

    bu_vls_printf(&trans_vls, "%lf %lf 0", dx, dy);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 3;
    av[0] = "tra";
    av[1] = "-v";
    av[2] = bu_vls_addr(&trans_vls);
    av[3] = (char *)0;

    ret = ged_tra(gedp, ac, (const char **)av);
    bu_vls_free(&trans_vls);

    if (ret == GED_OK) {
	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback));
	}

	to_refresh_view(gdvp);
    }

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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_MOVE_ARB_EDGE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_arb_edge %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_MOVE_ARB_FACE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_arb_face %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_move_botpt(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *UNUSED(usage),
	      int UNUSED(maxargs))
{
    int ret;

    if ((ret = ged_move_botpt(gedp, argc, argv)) == GED_OK) {
	char *av[3];
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
to_move_botpts(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *UNUSED(usage),
	       int UNUSED(maxargs))
{
    int ret;

    if ((ret = ged_move_botpts(gedp, argc, argv)) == GED_OK) {
	char *av[3];

	av[0] = "draw";
	av[1] = (char *)argv[1];
	av[2] = (char *)0;
	to_edit_redraw(gedp, 2, (const char **)av);

	return GED_OK;
    }

    return ret;
}


HIDDEN int
to_move_botpt_mode(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "%s: View not found - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_MOVE_BOT_POINT_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_botpt -r %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_move_botpts_mode(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    register int i;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "%s: View not found - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_MOVE_BOT_POINTS_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_botpts %s %%x %%y %s ",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[4]);

    for (i = 5; i < argc; ++i)
	bu_vls_printf(&bindings, "%s ", argv[i]);
    bu_vls_printf(&bindings, "}");

    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_move_metaballpt_mode(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr UNUSED(func),
			const char *usage,
			int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_MOVE_METABALL_POINT_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_metaballpt %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_move_pipept_mode(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr UNUSED(func),
		    const char *usage,
		    int UNUSED(maxargs))
{
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_MOVE_PIPE_POINT_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_move_pipept %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    return GED_OK;
}


HIDDEN int
to_move_pt_common(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *UNUSED(usage),
		  int UNUSED(maxargs))
{
    int ret;

    if ((ret = (*func)(gedp, argc, argv)) == GED_OK) {
	char *av[3];
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
    struct ged_dm_view *new_gdvp;
    HIDDEN const int name_index = 1;
    int type = DM_TYPE_BAD;
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
	type = DM_TYPE_NULL;

    /* find display manager type */
#ifdef DM_X
    if (argv[2][0] == 'X' || argv[2][0] == 'x')
	type = DM_TYPE_X;
#endif /* DM_X */

#ifdef DM_TK
    if (BU_STR_EQUAL(argv[2], "tk"))
	type = DM_TYPE_TK;
#endif /* DM_TK */

#ifdef DM_OGL
    if (BU_STR_EQUAL(argv[2], "ogl"))
	type = DM_TYPE_OGL;
#endif /* DM_OGL */

#ifdef DM_OSG
    if (BU_STR_EQUAL(argv[2], "osg"))
	type = DM_TYPE_OSG;
#endif /* DM_OSG */

#ifdef DM_OSGL
    if (BU_STR_EQUAL(argv[2], "osgl"))
	type = DM_TYPE_OSGL;
#endif /* DM_OSGL */

#ifdef DM_WGL
    if (BU_STR_EQUAL(argv[2], "wgl"))
	type = DM_TYPE_WGL;
#endif /* DM_WGL */

#ifdef DM_QT
    if (BU_STR_EQUAL(argv[2], "qt"))
	type = DM_TYPE_QT;
#endif /* DM_QT */

    if (type == DM_TYPE_BAD) {
	bu_vls_printf(gedp->ged_result_str, "ERROR:  Requisite display manager is not available.\nBRL-CAD may need to be recompiled with support for:  %s\nRun 'fbhelp' for a list of available display managers.\n", argv[2]);
	return GED_ERROR;
    }

    BU_ALLOC(new_gdvp, struct ged_dm_view);
    BU_ALLOC(new_gdvp->gdv_view, struct bview);

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

	new_gdvp->gdv_dmp = dm_open(current_top->to_interp, type, ac, av);
	if (new_gdvp->gdv_dmp == DM_NULL) {
	    bu_free((void *)new_gdvp->gdv_view, "ged_view");
	    bu_free((void *)new_gdvp, "ged_dm_view");
	    bu_free((void *)av, "to_new_view: av");

	    bu_vls_printf(gedp->ged_result_str, "Failed to create %s\n", argv[1]);
	    return GED_ERROR;
	}

	bu_free((void *)av, "to_new_view: av");

    }

    new_gdvp->gdv_gop = current_top->to_gop;
    bu_vls_init(&new_gdvp->gdv_name);
    bu_vls_init(&new_gdvp->gdv_callback);
    bu_vls_init(&new_gdvp->gdv_edit_motion_delta_callback);
    bu_vls_printf(&new_gdvp->gdv_name, argv[name_index]);
    ged_view_init(new_gdvp->gdv_view);
    BU_LIST_INSERT(&current_top->to_gop->go_head_views.l, &new_gdvp->l);

    new_gdvp->gdv_view->gv_point_scale = 1.0;
    new_gdvp->gdv_view->gv_curve_scale = 1.0;

    new_gdvp->gdv_fbs.fbs_listener.fbsl_fbsp = &new_gdvp->gdv_fbs;
    new_gdvp->gdv_fbs.fbs_listener.fbsl_fd = -1;
    new_gdvp->gdv_fbs.fbs_listener.fbsl_port = -1;
    new_gdvp->gdv_fbs.fbs_fbp = FB_NULL;
    new_gdvp->gdv_fbs.fbs_callback = (void (*)(void *clientData))to_fbs_callback;
    new_gdvp->gdv_fbs.fbs_clientData = new_gdvp;
    new_gdvp->gdv_fbs.fbs_interp = current_top->to_interp;

    /* open the framebuffer */
    to_open_fbs(new_gdvp, current_top->to_interp);

    /* Set default bindings */
    to_init_default_bindings(new_gdvp);

    bu_vls_printf(&event_vls, "event generate %s <Configure>; %s autoview %s",
		  bu_vls_addr(dm_get_pathname(new_gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&new_gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&event_vls));
    bu_vls_free(&event_vls);

    (void)Tcl_CreateCommand(current_top->to_interp,
			    bu_vls_addr(dm_get_pathname(new_gdvp->gdv_dmp)),
			    (Tcl_CmdProc *)to_view_cmd,
			    (ClientData)new_gdvp,
			    to_deleteViewProc);

    bu_vls_printf(gedp->ged_result_str, bu_vls_addr(&new_gdvp->gdv_name));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_OROTATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_orotate %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_OSCALE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_oscale %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
	bu_sscanf(argv[4], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_OTRANSLATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_otranslate %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    (void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

    (void)fb_refresh(gdvp->gdv_fbs.fbs_fbp, gdvp->gdv_view->gv_rect.pos[X], gdvp->gdv_view->gv_rect.pos[Y],
		     gdvp->gdv_view->gv_rect.dim[X], gdvp->gdv_view->gv_rect.dim[Y]);

    (void)dm_set_depth_mask(gdvp->gdv_dmp, 1);

    return GED_OK;
}


#if defined(DM_OGL) || defined(DM_WGL)
HIDDEN int
to_pix(struct ged *gedp,
       int argc,
       const char *argv[],
       ged_func_ptr UNUSED(func),
       const char *usage,
       int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp = NULL;
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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (dm_get_type(gdvp->gdv_dmp) != DM_TYPE_WGL && dm_get_type(gdvp->gdv_dmp) != DM_TYPE_OGL) {
	bu_vls_printf(gedp->ged_result_str, "%s: not yet supported for this display manager (i.e. must be OpenGl based)", argv[0]);
	return GED_OK;
    }

    bytes_per_line = dm_get_width(gdvp->gdv_dmp) * bytes_per_pixel;

    if ((fp = fopen(argv[2], "wb")) == NULL) {
	bu_vls_printf(gedp->ged_result_str,
		      "%s: cannot open \"%s\" for writing.",
		      argv[0], argv[2]);
	return GED_ERROR;
    }

    width = dm_get_width(gdvp->gdv_dmp);
    height = dm_get_height(gdvp->gdv_dmp);

    make_ret = dm_make_current(gdvp->gdv_dmp);
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
    struct ged_dm_view *gdvp = NULL;
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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (dm_get_type(gdvp->gdv_dmp) != DM_TYPE_WGL && dm_get_type(gdvp->gdv_dmp) != DM_TYPE_OGL) {
	bu_vls_printf(gedp->ged_result_str, "%s: not yet supported for this display manager (i.e. must be OpenGl based)", argv[0]);
	return GED_OK;
    }

    bytes_per_line = dm_get_width(gdvp->gdv_dmp) * bytes_per_pixel;

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

    width = dm_get_width(gdvp->gdv_dmp);
    height = dm_get_height(gdvp->gdv_dmp);
    make_ret = dm_make_current(gdvp->gdv_dmp);
    if (!make_ret) {
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
    png_set_compression_level(png_p, Z_BEST_COMPRESSION);
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
to_poly_circ_mode(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    int ac;
    char *av[5];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gdv_view->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gdv_view->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gdv_view->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gdv_view->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gdv_view->gv_view2model);

    gedp->ged_gvp = gdvp->gdv_view;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_POLY_CIRCLE_MODE;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(v_pt, fx, fy, gdvp->gdv_view->gv_data_vZ);
    if (gedp->ged_gvp->gv_grid.snap)
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);

    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
    VMOVE(gdpsp->gdps_prev_point, v_pt);

    bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} }",
		  V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt));
    ac = 4;
    av[0] = "data_polygons";
    av[1] = bu_vls_addr(&gdvp->gdv_name);
    av[2] = "append_poly";
    av[3] = bu_vls_addr(&plist);
    av[4] = (char *)0;

    if (gdpsp->gdps_polygons.gp_num_polygons == gdpsp->gdps_target_polygon_i)
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
    else
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.gp_num_polygons;

    (void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
    bu_vls_free(&plist);

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_circ %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_poly_cont_build(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    int ac;
    char *av[8];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gdv_view->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gdv_view->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gdv_view->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gdv_view->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gdv_view->gv_view2model);

    gedp->ged_gvp = gdvp->gdv_view;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_POLY_CONTOUR_MODE;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(v_pt, fx, fy, gdvp->gdv_view->gv_data_vZ);
    if (gedp->ged_gvp->gv_grid.snap)
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);

    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);

    av[0] = "data_polygons";
    av[1] = bu_vls_addr(&gdvp->gdv_name);
    if (gdpsp->gdps_cflag == 0) {
	struct bu_vls plist = BU_VLS_INIT_ZERO;
	struct bu_vls bindings = BU_VLS_INIT_ZERO;

	if (gdpsp->gdps_polygons.gp_num_polygons == gdpsp->gdps_target_polygon_i)
	    gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
	else
	    gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.gp_num_polygons;

	gdpsp->gdps_cflag = 1;
	gdpsp->gdps_curr_point_i = 1;

	bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} }", V3ARGS(m_pt), V3ARGS(m_pt));
	ac = 4;
	av[2] = "append_poly";
	av[3] = bu_vls_addr(&plist);
	av[4] = (char *)0;

	(void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
	bu_vls_free(&plist);

	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_cont %s %%x %%y}",
		      bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		      bu_vls_addr(&current_top->to_gop->go_name),
		      bu_vls_addr(&gdvp->gdv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
	bu_vls_free(&bindings);
    } else {
	struct bu_vls i_vls = BU_VLS_INIT_ZERO;
	struct bu_vls k_vls = BU_VLS_INIT_ZERO;
	struct bu_vls plist = BU_VLS_INIT_ZERO;

	bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);
	bu_vls_printf(&k_vls, "%zu", gdpsp->gdps_curr_point_i);
	bu_vls_printf(&plist, "%lf %lf %lf", V3ARGS(m_pt));

	ac = 7;
	av[2] = "replace_point";
	av[3] = bu_vls_addr(&i_vls);
	av[4] = "0";
	av[5] = bu_vls_addr(&k_vls);
	av[6] = bu_vls_addr(&plist);
	av[7] = (char *)0;
	(void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);

	ac = 6;
	av[2] = "append_point";
	av[5] = bu_vls_addr(&plist);
	av[6] = (char *)0;
	(void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
	bu_vls_free(&i_vls);
	bu_vls_free(&k_vls);
	bu_vls_free(&plist);

	++gdpsp->gdps_curr_point_i;
    }

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_poly_cont_build_end(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdvp->gdv_view->gv_sdata_polygons.gdps_cflag = 0;
    else
	gdvp->gdv_view->gv_data_polygons.gdps_cflag = 0;

    gedp->ged_gvp = gdvp->gdv_view;
    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_poly_ell_mode(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    int ac;
    char *av[5];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gdv_view->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gdv_view->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gdv_view->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gdv_view->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gdv_view->gv_view2model);

    gedp->ged_gvp = gdvp->gdv_view;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_POLY_ELLIPSE_MODE;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(v_pt, fx, fy, gdvp->gdv_view->gv_data_vZ);
    if (gedp->ged_gvp->gv_grid.snap)
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);

    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
    VMOVE(gdpsp->gdps_prev_point, v_pt);

    bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} }",
		  V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt));
    ac = 4;
    av[0] = "data_polygons";
    av[1] = bu_vls_addr(&gdvp->gdv_name);
    av[2] = "append_poly";
    av[3] = bu_vls_addr(&plist);
    av[4] = (char *)0;

    if (gdpsp->gdps_polygons.gp_num_polygons == gdpsp->gdps_target_polygon_i)
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
    else
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.gp_num_polygons;

    (void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
    bu_vls_free(&plist);

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_ell %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_poly_rect_mode(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    int ac;
    char *av[5];
    int x, y;
    int sflag;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;
    bview_data_polygon_state *gdpsp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gdv_view->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gdv_view->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gdv_view->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gdv_view->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gdv_view->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gdv_view->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gdv_view->gv_view2model);

    gedp->ged_gvp = gdvp->gdv_view;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 4)
	sflag = 0;
    else {
	if (bu_sscanf(argv[4], "%d", &sflag) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    }
    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;

    if (sflag)
	gdvp->gdv_view->gv_mode = TCLCAD_POLY_SQUARE_MODE;
    else
	gdvp->gdv_view->gv_mode = TCLCAD_POLY_RECTANGLE_MODE;

    fx = screen_to_view_x(gdvp->gdv_dmp, x);
    fy = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(v_pt, fx, fy, gdvp->gdv_view->gv_data_vZ);
    if (gedp->ged_gvp->gv_grid.snap)
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);

    MAT4X3PNT(m_pt, gdvp->gdv_view->gv_view2model, v_pt);
    VMOVE(gdpsp->gdps_prev_point, v_pt);

    bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} }",
		  V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt));
    ac = 4;
    av[0] = "data_polygons";
    av[1] = bu_vls_addr(&gdvp->gdv_name);
    av[2] = "append_poly";
    av[3] = bu_vls_addr(&plist);
    av[4] = (char *)0;

    if (gdpsp->gdps_polygons.gp_num_polygons == gdpsp->gdps_target_polygon_i)
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
    else
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.gp_num_polygons;

    (void)to_data_polygons(gedp, ac, (const char **)av, (ged_func_ptr)0, "", 0);
    bu_vls_free(&plist);

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_rect %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_prim_label(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *UNUSED(usage),
	      int UNUSED(maxargs))
{
    register int i;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Free the previous list of primitives scheduled for labeling */
    if (current_top->to_gop->go_prim_label_list_size) {
	for (i = 0; i < current_top->to_gop->go_prim_label_list_size; ++i)
	    bu_vls_free(&current_top->to_gop->go_prim_label_list[i]);
	bu_free((void *)current_top->to_gop->go_prim_label_list, "prim_label");
	current_top->to_gop->go_prim_label_list = (struct bu_vls *)0;
    }

    /* Set the list of primitives scheduled for labeling */
    current_top->to_gop->go_prim_label_list_size = argc - 1;
    if (current_top->to_gop->go_prim_label_list_size < 1)
	return GED_OK;

    current_top->to_gop->go_prim_label_list = (struct bu_vls *)bu_calloc(current_top->to_gop->go_prim_label_list_size,
									 sizeof(struct bu_vls), "prim_label");
    for (i = 0; i < current_top->to_gop->go_prim_label_list_size; ++i) {
	bu_vls_init(&current_top->to_gop->go_prim_label_list[i]);
	bu_vls_printf(&current_top->to_gop->go_prim_label_list[i], "%s", argv[i+1]);
    }

    return GED_OK;
}


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
    char *av[5];
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    struct bu_vls x_vls = BU_VLS_INIT_ZERO;
    struct bu_vls y_vls = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    gedp->ged_gvp = gdvp->gdv_view;

    if (bu_sscanf(argv[2], "%d", &x) != 1 ||
	bu_sscanf(argv[3], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = dm_get_height(gdvp->gdv_dmp) - y;
    gdvp->gdv_view->gv_mode = TCLCAD_RECTANGLE_MODE;

    ac = 4;
    av[0] = "rect";
    av[1] = "dim";
    av[2] = "0";
    av[3] = "0";
    av[4] = (char *)0;
    (void)ged_rect(gedp, ac, (const char **)av);

    bu_vls_printf(&x_vls, "%d", (int)gdvp->gdv_view->gv_prevMouseX);
    bu_vls_printf(&y_vls, "%d", (int)gdvp->gdv_view->gv_prevMouseY);
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

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_rect %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return GED_OK;
}


HIDDEN int
to_redraw(struct ged *gedp,
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

    return to_edit_redraw(gedp, argc, argv);
}


HIDDEN int
to_refresh(struct ged *gedp,
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

    return to_handle_refresh(gedp, argv[1]);
}


HIDDEN int
to_refresh_all(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *UNUSED(usage),
	       int UNUSED(maxargs))
{
    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    to_refresh_all_views(current_top);

    return GED_OK;
}


HIDDEN int
to_refresh_on(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *UNUSED(usage),
	      int UNUSED(maxargs))
{
    int on;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (2 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    /* Get refresh_on state */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d", current_top->to_gop->go_refresh_on);
	return GED_OK;
    }

    /* Set refresh_on state */
    if (bu_sscanf(argv[1], "%d", &on) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    current_top->to_gop->go_refresh_on = on;

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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[5], "%lf", &x) != 1 ||
	bu_sscanf(argv[6], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_ROTATE_ARB_FACE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_rotate_arb_face %s %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3],
		  argv[4]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_ROTATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_rot %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    register int i;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the callback string */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&current_top->to_gop->go_rt_end_callback));

	return GED_OK;
    }

    /* set the callback string */
    bu_vls_trunc(&current_top->to_gop->go_rt_end_callback, 0);
    for (i = 1; i < argc; ++i)
	bu_vls_printf(&current_top->to_gop->go_rt_end_callback, "%s ", argv[i]);

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
    (void)Tcl_CreateCommand(current_top->to_interp, newprocname, rt_tcl_rt,
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_PROTATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_protate %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_PSCALE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_pscale %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[4], "%lf", &x) != 1 ||
	bu_sscanf(argv[5], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_PTRANSLATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_ptranslate %s %s %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name),
		  argv[2],
		  argv[3]);
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_DATA_SCALE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_data_scale %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_SCALE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_scale %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
    VSET(view, x, y, 0.0);
    MAT4X3PNT(model, gdvp->gdv_view->gv_view2model, view);

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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    x = screen_to_view_x(gdvp->gdv_dmp, x);
    y = screen_to_view_y(gdvp->gdv_dmp, y);
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Get coord */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%c", gdvp->gdv_view->gv_coord);
	return GED_OK;
    }

    /* Set coord */
    if ((argv[2][0] != 'm' && argv[2][0] != 'v') || argv[2][1] != '\0') {
	bu_vls_printf(gedp->ged_result_str, "set_coord: bad value - %s\n", argv[2]);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_coord = argv[2][0];

    return GED_OK;
}


HIDDEN int
to_set_fb_mode(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    int mode;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Get fb mode */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_fbs.fbs_mode);
	return GED_OK;
    }

    /* Set fb mode */
    if (bu_sscanf(argv[2], "%d", &mode) != 1) {
	bu_vls_printf(gedp->ged_result_str, "set_fb_mode: bad value - %s\n", argv[2]);
	return GED_ERROR;
    }

    if (mode < 0)
	mode = 0;
    else if (TCLCAD_OBJ_FB_MODE_OVERLAY < mode)
	mode = TCLCAD_OBJ_FB_MODE_OVERLAY;

    gdvp->gdv_fbs.fbs_mode = mode;
    to_refresh_view(gdvp);

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
    struct ged_dm_view *gdvp;
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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
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

    gedp->ged_gvp = gdvp->gdv_view;
    if (!gedp->ged_gvp->gv_grid.snap) {
	bu_vls_printf(gedp->ged_result_str, "%lf %lf", fvx, fvy);
	return GED_OK;
    }

    ged_snap_to_grid(gedp, &fvx, &fvy);
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
	char *av[3];
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
	char *av[3];
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &x) != 1 ||
	bu_sscanf(argv[3], "%lf", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gdv_view->gv_prevMouseX = x;
    gdvp->gdv_view->gv_prevMouseY = y;
    gdvp->gdv_view->gv_mode = TCLCAD_TRANSLATE_MODE;

    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_trans %s %%x %%y}",
		  bu_vls_addr(dm_get_pathname(gdvp->gdv_dmp)),
		  bu_vls_addr(&current_top->to_gop->go_name),
		  bu_vls_addr(&gdvp->gdv_name));
    Tcl_Eval(current_top->to_interp, bu_vls_addr(&bindings));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get transparency flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_transparency(gdvp->gdv_dmp));
	return GED_OK;
    }

    /* set transparency flag */
    if (argc == 3) {
	if (bu_sscanf(argv[2], "%d", &transparency) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: invalid transparency value - %s", argv[0], argv[2]);
	    return GED_ERROR;
	}

	(void)dm_make_current(gdvp->gdv_dmp);
	(void)dm_set_transparency(gdvp->gdv_dmp, transparency);
	return GED_OK;
    }

    return GED_OK;
}


HIDDEN int
to_view_axes(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    return to_axes(gedp, gdvp, &gdvp->gdv_view->gv_view_axes, argc, argv, usage);
}


HIDDEN int
to_view_callback(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    register int i;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get the callback string */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&gdvp->gdv_callback));

	return GED_OK;
    }

    /* set the callback string */
    bu_vls_trunc(&gdvp->gdv_callback, 0);
    for (i = 2; i < argc; ++i)
	bu_vls_printf(&gdvp->gdv_callback, "%s ", argv[i]);

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
    struct ged_dm_view *gdvp;
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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d %d", dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));
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

#if defined(DM_X) || defined(DM_TK) || defined(DM_OGL) || defined(DM_OSG) || defined(DM_OSGL) || defined(DM_WGL) || defined(DM_QT)
#   if (defined HAVE_TK)
    Tk_GeometryRequest(((struct dm_xvars *)(dm_get_public_vars(gdvp->gdv_dmp)))->xtkwin,
		       width, height);
#   endif
#endif

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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf %lf", &view[X], &view[Y]) != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    width = dm_get_width(gdvp->gdv_dmp);
    height = dm_get_height(gdvp->gdv_dmp);
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
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
    char *av[3];
    fastf_t xpos2, ypos2;
    fastf_t sf;
    struct bu_vls slew_vec = BU_VLS_INIT_ZERO;
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf", &xpos1) != 1 ||
	bu_sscanf(argv[3], "%lf", &ypos1) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    width = dm_get_width(gdvp->gdv_dmp);
    xpos2 = 0.5 * (fastf_t)width;
    height = dm_get_height(gdvp->gdv_dmp);
    ypos2 = 0.5 * (fastf_t)height;
    sf = 2.0 / width;

    bu_vls_printf(&slew_vec, "%lf %lf", (xpos1 - xpos2) * sf, (ypos2 - ypos1) * sf);

    gedp->ged_gvp = gdvp->gdv_view;
    ac = 2;
    av[0] = (char *)argv[0];
    av[1] = bu_vls_addr(&slew_vec);
    av[2] = (char *)0;

    ret = ged_slew(gedp, ac, (const char **)av);
    bu_vls_free(&slew_vec);

    if (ret == GED_OK) {
	if (gdvp->gdv_view->gv_grid.snap) {

	    gedp->ged_gvp = gdvp->gdv_view;
	    av[0] = "grid";
	    av[1] = "vsnap";
	    av[2] = NULL;
	    ged_grid(gedp, 2, (const char **)av);
	}

	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback));
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get zbuffer flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", dm_get_zbuffer(gdvp->gdv_dmp));
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

    (void)dm_make_current(gdvp->gdv_dmp);
    (void)dm_set_zbuffer(gdvp->gdv_dmp, zbuffer);
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
    struct ged_dm_view *gdvp;

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

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* get zclip flag */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", gdvp->gdv_view->gv_zclip);
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

    gdvp->gdv_view->gv_zclip = zclip;
    dm_set_zclip(gdvp->gdv_dmp, zclip);
    to_refresh_view(gdvp);

    return GED_OK;
}


/*************************** Wrapper Functions ***************************/
HIDDEN int
to_autoview_func(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr func,
		 const char *UNUSED(usage),
		 int UNUSED(maxargs))
{
    size_t i;
    int ret;
    char *av[2];
    int aflag = 0;
    int rflag = 0;
    struct ged_dm_view *gdvp;

    av[0] = "who";
    av[1] = (char *)0;
    ret = ged_who(gedp, 1, (const char **)av);

    for (i = 1; i < (size_t)argc; ++i) {
	if (argv[i][0] != '-') {
	    break;
	}

	if (argv[i][1] == 'R' && argv[i][2] == '\0') {
	    rflag = 1;
	    break;
	}
    }

    if (!rflag && ret == GED_OK && strlen(bu_vls_addr(gedp->ged_result_str)) == 0)
	aflag = 1;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (to_is_viewable(gdvp)) {
	    gedp->ged_gvp->gv_x_samples = dm_get_width(gdvp->gdv_dmp);
	    gedp->ged_gvp->gv_y_samples = dm_get_height(gdvp->gdv_dmp);
	}
    }

    ret = (*func)(gedp, argc, (const char **)argv);

    if (ret == GED_OK) {
	if (aflag)
	    to_autoview_all_views(current_top);
	else
	    to_refresh_all_views(current_top);
    }

    return ret;
}


HIDDEN int
to_edit_redraw(struct ged *gedp,
	       int argc,
	       const char *argv[])
{
    size_t i;
    register struct display_list *gdlp;
    register struct display_list *next_gdlp;
    struct db_full_path subpath;
    int ret = GED_OK;

    if (argc != 2)
	return GED_ERROR;

    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	gdlp->dl_wflag = 0;
	gdlp = BU_LIST_PNEXT(display_list, gdlp);
    }

    if (db_string_to_path(&subpath, gedp->ged_wdbp->dbip, argv[1]) == 0) {
	for (i = 0; i < subpath.fp_len; ++i) {
	    gdlp = BU_LIST_NEXT(display_list, gedp->ged_gdp->gd_headDisplay);
	    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
		register struct solid *curr_sp;

		next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

		if (gdlp->dl_wflag) {
		    gdlp = next_gdlp;
		    continue;
		}

		FOR_ALL_SOLIDS(curr_sp, &gdlp->dl_headSolid) {
		    if (db_full_path_search(&curr_sp->s_fullpath, subpath.fp_names[i])) {
			struct display_list *last_gdlp;
			struct solid *sp = BU_LIST_NEXT(solid, &gdlp->dl_headSolid);
			struct bu_vls mflag = BU_VLS_INIT_ZERO;
			struct bu_vls xflag = BU_VLS_INIT_ZERO;
			char *av[5] = {0};
			int arg = 0;

			av[arg++] = (char *)argv[0];
			if (sp->s_hiddenLine) {
			    av[arg++] = "-h";
			} else {
			    bu_vls_printf(&mflag, "-m%d", sp->s_dmode);
			    bu_vls_printf(&xflag, "-x%f", sp->s_transparency);
			    av[arg++] = bu_vls_addr(&mflag);
			    av[arg++] = bu_vls_addr(&xflag);
			}
			av[arg] = bu_vls_strdup(&gdlp->dl_path);

			ret = ged_draw(gedp, arg + 1, (const char **)av);

			bu_free(av[arg], "to_edit_redraw");
			bu_vls_free(&mflag);
			bu_vls_free(&xflag);

			/* The function call above causes gdlp to be
			 * removed from the display list. A new one is
			 * then created and appended to the end.  Here
			 * we put it back where it belongs (i.e. as
			 * specified by the user).  This also prevents
			 * an infinite loop where the last and the
			 * second to last list items play leap frog
			 * with the end of list.
			 */
			last_gdlp = BU_LIST_PREV(display_list, gedp->ged_gdp->gd_headDisplay);
			BU_LIST_DEQUEUE(&last_gdlp->l);
			BU_LIST_INSERT(&next_gdlp->l, &last_gdlp->l);
			last_gdlp->dl_wflag = 1;

			goto end;
		    }
		}

	    end:
		gdlp = next_gdlp;
	    }
	}

	db_free_full_path(&subpath);
    }

    to_refresh_all_views(current_top);

    return ret;
}


HIDDEN int
to_more_args_func(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *UNUSED(usage),
		  int UNUSED(maxargs))
{
    register int i;
    int ac;
    int ret;
    char *av[256];
    struct bu_vls callback_cmd = BU_VLS_INIT_ZERO;
    struct bu_vls temp = BU_VLS_INIT_ZERO;

    /* copy all args */
    ac = argc;
    for (i = 0; i < ac; ++i)
	av[i] = bu_strdup((char *)argv[i]);
    av[ac] = (char *)0;

    while ((ret = (*func)(gedp, ac, (const char **)av)) & GED_MORE) {
	int ac_more;
	const char **avmp;
	const char **av_more = NULL;

	if (0 < bu_vls_strlen(&current_top->to_gop->go_more_args_callback)) {
	    bu_vls_trunc(&callback_cmd, 0);
	    bu_vls_printf(&callback_cmd, "%s [string range {%s} 0 end]",
			  bu_vls_addr(&current_top->to_gop->go_more_args_callback),
			  bu_vls_addr(gedp->ged_result_str));

	    if (Tcl_Eval(current_top->to_interp, bu_vls_addr(&callback_cmd)) != TCL_OK) {
		bu_vls_trunc(gedp->ged_result_str, 0);
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(current_top->to_interp));
		Tcl_ResetResult(current_top->to_interp);
		return GED_ERROR;
	    }

	    bu_vls_trunc(&temp, 0);
	    bu_vls_printf(&temp, Tcl_GetStringResult(current_top->to_interp));
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
	for (i = 0; i < ac_more; ++i)
	    av[ac++] = bu_strdup(avmp[i]);
	av[ac+1] = (char *)0;

	Tcl_Free((char *)av_more);
    }

    bu_vls_free(&callback_cmd);
    bu_vls_free(&temp);

    bu_vls_printf(gedp->ged_result_str, "BUILT_BY_MORE_ARGS");
    for (i = 0; i < ac; ++i) {
	bu_vls_printf(gedp->ged_result_str, "%s ", av[i]);
	bu_free((void *)av[i], "to_more_args_func");
    }

    return ret;
}


HIDDEN int
to_pass_through_func(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *UNUSED(usage),
		     int UNUSED(maxargs))
{
    return (*func)(gedp, argc, argv);
}


HIDDEN int
to_pass_through_and_refresh_func(struct ged *gedp,
				 int argc,
				 const char *argv[],
				 ged_func_ptr func,
				 const char *UNUSED(usage),
				 int UNUSED(maxargs))
{
    int ret;

    ret = (*func)(gedp, argc, argv);

    if (ret == GED_OK)
	to_refresh_all_views(current_top);

    return ret;
}


HIDDEN int
to_view_func(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr func,
	     const char *usage,
	     int maxargs)
{
    return to_view_func_common(gedp, argc, argv, func, usage, maxargs, 0, 1);
}


HIDDEN int
to_view_func_common(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs,
		    int cflag,
		    int rflag)
{
    register int i;
    int ret;
    int ac;
    char **av;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    av = (char **)bu_calloc(argc+1, sizeof(char *), "alloc av copy");

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (maxargs != TO_UNLIMITED && maxargs < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Copy argv into av while skipping argv[1] (i.e. the view name) */
    gedp->ged_gvp = gdvp->gdv_view;
    gedp->ged_fbsp = &gdvp->gdv_fbs;
    gedp->ged_refresh_clientdata = (void *)gdvp;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;
    ret = (*func)(gedp, ac, (const char **)av);

    bu_free(av, "free av copy");

    /* Keep the view's perspective in sync with its corresponding display manager */
    dm_set_perspective(gdvp->gdv_dmp, gdvp->gdv_view->gv_perspective);

    if (gdvp->gdv_view->gv_adaptive_plot &&
	gdvp->gdv_view->gv_redraw_on_zoom)
    {
	char *gr_av[] = {"redraw", NULL};

	ged_redraw(gedp, 1, (const char **)gr_av);

	gdvp->gdv_view->gv_x_samples = dm_get_width(gdvp->gdv_dmp);
	gdvp->gdv_view->gv_y_samples = dm_get_height(gdvp->gdv_dmp);
    }

    if (ret == GED_OK) {
	if (cflag && 0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    struct bu_vls save_result = BU_VLS_INIT_ZERO;

	    bu_vls_printf(&save_result, "%s", bu_vls_addr(gedp->ged_result_str));
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback));
	    bu_vls_trunc(gedp->ged_result_str, 0);
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&save_result));
	    bu_vls_free(&save_result);
	}

	if (rflag)
	    to_refresh_view(gdvp);
    }

    return ret;
}


HIDDEN int
to_view_func_less(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *usage,
		  int maxargs)
{
    return to_view_func_common(gedp, argc, argv, func, usage, maxargs, 1, 0);
}


HIDDEN int
to_view_func_plus(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr func,
		  const char *usage,
		  int maxargs)
{
    return to_view_func_common(gedp, argc, argv, func, usage, maxargs, 1, 1);
}


HIDDEN int
to_dm_func(struct ged *gedp,
	   int argc,
	   const char *argv[],
	   ged_func_ptr func,
	   const char *usage,
	   int maxargs)
{
    register int i;
    int ret;
    int ac;
    char **av;
    struct ged_dm_view *gdvp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);
    av = (char **)bu_calloc(argc+1, sizeof(char *), "alloc av copy");

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (maxargs != TO_UNLIMITED && maxargs < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (BU_STR_EQUAL(bu_vls_addr(&gdvp->gdv_name), argv[1]))
	    break;
    }

    if (BU_LIST_IS_HEAD(&gdvp->l, &current_top->to_gop->go_head_views.l)) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* Copy argv into av while skipping argv[1] (i.e. the view name) */
    gedp->ged_gvp = gdvp->gdv_view;
    gedp->ged_dmp = (void *)gdvp->gdv_dmp;
    gedp->ged_dm_width = dm_get_width(gdvp->gdv_dmp);
    gedp->ged_dm_height = dm_get_height(gdvp->gdv_dmp);
    gedp->ged_dm_get_display_image = to_dm_get_display_image;
    gedp->ged_refresh_clientdata = (void *)gdvp;
    av[0] = (char *)argv[0];
    ac = argc-1;
    for (i = 2; i < argc; ++i)
	av[i-1] = (char *)argv[i];
    av[i-1] = (char *)0;
    ret = (*func)(gedp, ac, (const char **)av);

    bu_free(av, "free av copy");

    /* Keep the view's perspective in sync with its corresponding display manager */
    dm_set_perspective(gdvp->gdv_dmp, gdvp->gdv_view->gv_perspective);

    return ret;
}


/*************************** Local Utility Functions ***************************/


HIDDEN void
to_fbs_callback(void *clientData)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientData;

    to_refresh_view(gdvp);
}


HIDDEN int
to_close_fbs(struct ged_dm_view *gdvp)
{
    if (gdvp->gdv_fbs.fbs_fbp == FB_NULL)
	return TCL_OK;

    fb_flush(gdvp->gdv_fbs.fbs_fbp);
    fb_close_existing(gdvp->gdv_fbs.fbs_fbp);
    gdvp->gdv_fbs.fbs_fbp = FB_NULL;

    return TCL_OK;
}


HIDDEN void to_dm_get_display_image(struct ged *gedp, unsigned char **idata)
{
    if (gedp->ged_dmp) {
	(void)dm_get_display_image(((dm *)gedp->ged_dmp), idata);
    }
}


/*
 * Open/activate the display managers framebuffer.
 */
HIDDEN int
to_open_fbs(struct ged_dm_view *gdvp, Tcl_Interp *interp)
{
    /* already open */
    if (gdvp->gdv_fbs.fbs_fbp != FB_NULL)
	return TCL_OK;

    gdvp->gdv_fbs.fbs_fbp = dm_get_fb(gdvp->gdv_dmp);

    if (gdvp->gdv_fbs.fbs_fbp == FB_NULL) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "openfb: failed to allocate framebuffer memory\n", (char *)NULL);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
    }

    return TCL_OK;
}

HIDDEN void
to_create_vlist_callback_solid(struct solid *sp)
{
    struct ged_dm_view *gdvp;
    register int first = 1;

	for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (current_top->to_gop->go_dlist_on && to_is_viewable(gdvp)) {

	    (void)dm_make_current(gdvp->gdv_dmp);

	    if (first) {
		sp->s_dlist = dm_gen_dlists(gdvp->gdv_dmp, 1);
		first = 0;
	    }

	    (void)dm_begin_dlist(gdvp->gdv_dmp, sp->s_dlist);

	    if (sp->s_iflag == UP)
		(void)dm_set_fg(gdvp->gdv_dmp, 255, 255, 255, 0, sp->s_transparency);
	    else
		(void)dm_set_fg(gdvp->gdv_dmp,
			       (unsigned char)sp->s_color[0],
			       (unsigned char)sp->s_color[1],
			       (unsigned char)sp->s_color[2], 0, sp->s_transparency);

	    if (sp->s_hiddenLine) {
		(void)dm_draw_vlist_hidden_line(gdvp->gdv_dmp, (struct bn_vlist *)&sp->s_vlist);
	    } else {
		(void)dm_draw_vlist(gdvp->gdv_dmp, (struct bn_vlist *)&sp->s_vlist);
	    }

	    (void)dm_end_dlist(gdvp->gdv_dmp);
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
to_free_vlist_callback(unsigned int dlist, int range)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &current_top->to_gop->go_head_views.l)) {
	if (current_top->to_gop->go_dlist_on && to_is_viewable(gdvp)) {
	    (void)dm_make_current(gdvp->gdv_dmp);
	    (void)dm_free_dlists(gdvp->gdv_dmp, dlist, range);
	}
    }
}


HIDDEN void
to_refresh_all_views(struct tclcad_obj *top)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &top->to_gop->go_head_views.l)) {
	to_refresh_view(gdvp);
    }
}


HIDDEN void
to_refresh_view(struct ged_dm_view *gdvp)
{
    if (!current_top->to_gop->go_refresh_on)
	return;

    if (to_is_viewable(gdvp))
	go_refresh(current_top->to_gop, gdvp);
}


HIDDEN void
to_refresh_handler(void *clientdata)
{
    struct ged_dm_view *gdvp = (struct ged_dm_view *)clientdata;

    /* Possibly do more here */

    to_refresh_view(gdvp);
}


HIDDEN void
to_autoview_view(struct ged_dm_view *gdvp, const char *scale)
{
    int ret;
    const char *av[3];

    gdvp->gdv_gop->go_gedp->ged_gvp = gdvp->gdv_view;
    av[0] = "autoview";
    av[1] = scale;
    av[2] = NULL;

    if (scale)
	ret = ged_autoview(gdvp->gdv_gop->go_gedp, 2, (const char **)av);
    else
	ret = ged_autoview(gdvp->gdv_gop->go_gedp, 1, (const char **)av);

    if (ret == GED_OK) {
	if (0 < bu_vls_strlen(&gdvp->gdv_callback)) {
	    Tcl_Eval(current_top->to_interp, bu_vls_addr(&gdvp->gdv_callback));
	}

	to_refresh_view(gdvp);
    }
}


HIDDEN void
to_autoview_all_views(struct tclcad_obj *top)
{
    struct ged_dm_view *gdvp;

    for (BU_LIST_FOR(gdvp, ged_dm_view, &top->to_gop->go_head_views.l)) {
	to_autoview_view(gdvp, NULL);
    }
}


HIDDEN void
to_rt_end_callback_internal(int aborted)
{
    if (0 < bu_vls_strlen(&current_top->to_gop->go_rt_end_callback)) {
	struct bu_vls callback_cmd = BU_VLS_INIT_ZERO;

	bu_vls_printf(&callback_cmd, "%s %d",
		      bu_vls_addr(&current_top->to_gop->go_rt_end_callback),
		      aborted);
	Tcl_Eval(current_top->to_interp, bu_vls_addr(&callback_cmd));
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

HIDDEN int
to_log_output_handler(void *client_data, void *vpstr)
{
    struct ged *gedp = (struct ged *)client_data;
    char *str = (char *)vpstr;

    to_output_handler(gedp, str);

    return strlen(str);
}


HIDDEN void go_dm_draw_arrows(dm *dmp, struct bview_data_arrow_state *gdasp, fastf_t sf);
HIDDEN void go_dm_draw_labels(dm *dmp, struct bview_data_label_state *gdlsp, matp_t m2vmat);
HIDDEN void go_dm_draw_lines(dm *dmp, struct bview_data_line_state *gdlsp);
HIDDEN void go_dm_draw_polys(dm *dmp, bview_data_polygon_state *gdpsp, int mode);

HIDDEN void go_draw(struct ged_dm_view *gdvp);
HIDDEN int go_draw_dlist(struct ged_dm_view *gdvp);
HIDDEN void go_draw_faceplate(struct ged_obj *gop, struct ged_dm_view *gdvp);
HIDDEN void go_draw_solid(struct ged_dm_view *gdvp, struct solid *sp);


HIDDEN void
go_dm_draw_arrows(dm *dmp, struct bview_data_arrow_state *gdasp, fastf_t sf)
{
    register int i;
    int saveLineWidth;
    int saveLineStyle;

    if (gdasp->gdas_num_points < 1)
	return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    /* set color */
    (void)dm_set_fg(dmp,
			 gdasp->gdas_color[0],
			 gdasp->gdas_color[1],
			 gdasp->gdas_color[2], 1, 1.0);

    /* set linewidth */
    (void)dm_set_line_attr(dmp, gdasp->gdas_line_width, 0);  /* solid lines */

    (void)dm_draw_lines_3d(dmp,
			   gdasp->gdas_num_points,
			   gdasp->gdas_points, 0);

    for (i = 0; i < gdasp->gdas_num_points; i += 2) {
	point_t points[16];
	point_t A, B;
	point_t BmA;
	point_t offset;
	point_t perp1, perp2;
	point_t a_base;
	point_t a_pt1, a_pt2, a_pt3, a_pt4;

	VMOVE(A, gdasp->gdas_points[i]);
	VMOVE(B, gdasp->gdas_points[i+1]);
	VSUB2(BmA, B, A);

	VUNITIZE(BmA);
	VSCALE(offset, BmA, -gdasp->gdas_tip_length * sf);

	bn_vec_perp(perp1, BmA);
	VUNITIZE(perp1);

	VCROSS(perp2, BmA, perp1);
	VUNITIZE(perp2);

	VSCALE(perp1, perp1, gdasp->gdas_tip_width * sf);
	VSCALE(perp2, perp2, gdasp->gdas_tip_width * sf);

	VADD2(a_base, B, offset);
	VADD2(a_pt1, a_base, perp1);
	VADD2(a_pt2, a_base, perp2);
	VSUB2(a_pt3, a_base, perp1);
	VSUB2(a_pt4, a_base, perp2);

	VMOVE(points[0], B);
	VMOVE(points[1], a_pt1);
	VMOVE(points[2], B);
	VMOVE(points[3], a_pt2);
	VMOVE(points[4], B);
	VMOVE(points[5], a_pt3);
	VMOVE(points[6], B);
	VMOVE(points[7], a_pt4);
	VMOVE(points[8], a_pt1);
	VMOVE(points[9], a_pt2);
	VMOVE(points[10], a_pt2);
	VMOVE(points[11], a_pt3);
	VMOVE(points[12], a_pt3);
	VMOVE(points[13], a_pt4);
	VMOVE(points[14], a_pt4);
	VMOVE(points[15], a_pt1);

	(void)dm_draw_lines_3d(dmp, 16, points, 0);
    }

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}


HIDDEN void
go_dm_draw_labels(dm *dmp, struct bview_data_label_state *gdlsp, matp_t m2vmat)
{
    register int i;

    /* set color */
    (void)dm_set_fg(dmp,
			 gdlsp->gdls_color[0],
			 gdlsp->gdls_color[1],
			 gdlsp->gdls_color[2], 1, 1.0);

    for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
	point_t vpoint;

	MAT4X3PNT(vpoint, m2vmat,
		  gdlsp->gdls_points[i]);
	(void)dm_draw_string_2d(dmp, gdlsp->gdls_labels[i],
				vpoint[X], vpoint[Y], 0, 1);
    }
}


HIDDEN void
go_dm_draw_lines(dm *dmp, struct bview_data_line_state *gdlsp)
{
    int saveLineWidth;
    int saveLineStyle;

    if (gdlsp->gdls_num_points < 1)
	return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    /* set color */
    (void)dm_set_fg(dmp,
			 gdlsp->gdls_color[0],
			 gdlsp->gdls_color[1],
			 gdlsp->gdls_color[2], 1, 1.0);

    /* set linewidth */
    (void)dm_set_line_attr(dmp, gdlsp->gdls_line_width, 0);  /* solid lines */

    (void)dm_draw_lines_3d(dmp,
			   gdlsp->gdls_num_points,
			   gdlsp->gdls_points, 0);

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}


#define GO_DM_DRAW_POLY(_dmp, _gdpsp, _i, _last_poly, _mode) {	\
	size_t _j; \
	\
	/* set color */ \
	(void)dm_set_fg((_dmp), \
			     (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_color[0], \
			     (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_color[1], \
			     (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_color[2], \
			     1, 1.0);					\
	\
	/* set the linewidth and linestyle for polygon i */ \
	(void)dm_set_line_attr((_dmp), \
			       (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_line_width, \
			       (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_line_style); \
	\
	for (_j = 0; _j < (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_num_contours; ++_j) { \
	    size_t _last = (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_num_points-1; \
	    \
	    (void)dm_draw_lines_3d((_dmp),				\
				   (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_num_points, \
				   (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_point, 1); \
	    \
	    if (_mode != TCLCAD_POLY_CONTOUR_MODE || _i != _last_poly || (_gdpsp)->gdps_cflag == 0) { \
		(void)dm_draw_line_3d((_dmp),				\
				      (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_point[_last], \
				      (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_point[0]); \
	    } \
	}}


HIDDEN void
go_dm_draw_polys(dm *dmp, bview_data_polygon_state *gdpsp, int mode)
{
    register size_t i, last_poly;
    int saveLineWidth;
    int saveLineStyle;

    if (gdpsp->gdps_polygons.gp_num_polygons < 1)
	return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    last_poly = gdpsp->gdps_polygons.gp_num_polygons - 1;
    for (i = 0; i < gdpsp->gdps_polygons.gp_num_polygons; ++i) {
	if (i == gdpsp->gdps_target_polygon_i)
	    continue;

	GO_DM_DRAW_POLY(dmp, gdpsp, i, last_poly, mode);
    }

    /* draw the target poly last */
    GO_DM_DRAW_POLY(dmp, gdpsp, gdpsp->gdps_target_polygon_i, last_poly, mode);

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}


HIDDEN void
go_draw(struct ged_dm_view *gdvp)
{
    (void)dm_loadmatrix(gdvp->gdv_dmp, gdvp->gdv_view->gv_model2view, 0);

    if (SMALL_FASTF < gdvp->gdv_view->gv_perspective)
	(void)dm_loadpmatrix(gdvp->gdv_dmp, gdvp->gdv_view->gv_pmat);
    else
	(void)dm_loadpmatrix(gdvp->gdv_dmp, (fastf_t *)NULL);

    go_draw_dlist(gdvp);
}


/* Draw all display lists */
HIDDEN int
go_draw_dlist(struct ged_dm_view *gdvp)
{
    register struct display_list *gdlp;
    register struct display_list *next_gdlp;
    struct solid *sp;
    int line_style = -1;
    dm *dmp = gdvp->gdv_dmp;
    struct bu_list *hdlp = gdvp->gdv_gop->go_gedp->ged_gdp->gd_headDisplay;

    if (dm_get_transparency(dmp)) {
	/* First, draw opaque stuff */
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		if (sp->s_transparency < 1.0)
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    (void)dm_set_line_attr(dmp, dm_get_linewidth(dmp), line_style);
		}

		go_draw_solid(gdvp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* disable write to depth buffer */
	(void)dm_set_depth_mask(dmp, 0);

	/* Second, draw transparent stuff */
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		/* already drawn above */
		if (ZERO(sp->s_transparency - 1.0))
		    continue;

		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    (void)dm_set_line_attr(dmp, dm_get_linewidth(dmp), line_style);
		}

		go_draw_solid(gdvp, sp);
	    }

	    gdlp = next_gdlp;
	}

	/* re-enable write to depth buffer */
	(void)dm_set_depth_mask(dmp, 1);
    } else {
	gdlp = BU_LIST_NEXT(display_list, hdlp);
	while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	    next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	    FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
		if (line_style != sp->s_soldash) {
		    line_style = sp->s_soldash;
		    (void)dm_set_line_attr(dmp, dm_get_linewidth(dmp), line_style);
		}

		go_draw_solid(gdvp, sp);
	    }

	    gdlp = next_gdlp;
	}
    }

    return GED_OK;
}


HIDDEN void
go_draw_faceplate(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    /* Center dot */
    if (gdvp->gdv_view->gv_center_dot.gos_draw) {
	(void)dm_set_fg(gdvp->gdv_dmp,
			     gdvp->gdv_view->gv_center_dot.gos_line_color[0],
			     gdvp->gdv_view->gv_center_dot.gos_line_color[1],
			     gdvp->gdv_view->gv_center_dot.gos_line_color[2],
			     1, 1.0);
	(void)dm_draw_point_2d(gdvp->gdv_dmp, 0.0, 0.0);
    }

    /* Model axes */
    if (gdvp->gdv_view->gv_model_axes.draw) {
	point_t map;
	point_t save_map;

	VMOVE(save_map, gdvp->gdv_view->gv_model_axes.axes_pos);
	VSCALE(map, gdvp->gdv_view->gv_model_axes.axes_pos, gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	MAT4X3PNT(gdvp->gdv_view->gv_model_axes.axes_pos, gdvp->gdv_view->gv_model2view, map);

	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_model_axes);

	VMOVE(gdvp->gdv_view->gv_model_axes.axes_pos, save_map);
    }

    /* View axes */
    if (gdvp->gdv_view->gv_view_axes.draw) {
	int width, height;
	fastf_t inv_aspect;
	fastf_t save_ypos;

	save_ypos = gdvp->gdv_view->gv_view_axes.axes_pos[Y];
	width = dm_get_width(gdvp->gdv_dmp);
	height = dm_get_height(gdvp->gdv_dmp);
	inv_aspect = (fastf_t)height / (fastf_t)width;
	gdvp->gdv_view->gv_view_axes.axes_pos[Y] = save_ypos * inv_aspect;
	dm_draw_axes(gdvp->gdv_dmp,
		     gdvp->gdv_view->gv_size,
		     gdvp->gdv_view->gv_rotation,
		     &gdvp->gdv_view->gv_view_axes);

	gdvp->gdv_view->gv_view_axes.axes_pos[Y] = save_ypos;
    }


    /* View scale */
    if (gdvp->gdv_view->gv_view_scale.gos_draw)
	dm_draw_scale(gdvp->gdv_dmp,
		      gdvp->gdv_view->gv_size*gop->go_gedp->ged_wdbp->dbip->dbi_base2local,
		      gdvp->gdv_view->gv_view_scale.gos_line_color,
		      gdvp->gdv_view->gv_view_params.gos_text_color);

    /* View parameters */
    if (gdvp->gdv_view->gv_view_params.gos_draw) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	point_t center;
	char *ustr;

	MAT_DELTAS_GET_NEG(center, gdvp->gdv_view->gv_center);
	VSCALE(center, center, gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

	ustr = (char *)bu_units_string(gop->go_gedp->ged_wdbp->dbip->dbi_local2base);
	bu_vls_printf(&vls, "units:%s  size:%.2f  center:(%.2f, %.2f, %.2f) az:%.2f  el:%.2f  tw::%.2f",
		      ustr,
		      gdvp->gdv_view->gv_size * gop->go_gedp->ged_wdbp->dbip->dbi_base2local,
		      V3ARGS(center),
		      V3ARGS(gdvp->gdv_view->gv_aet));
	(void)dm_set_fg(gdvp->gdv_dmp,
			     gdvp->gdv_view->gv_view_params.gos_text_color[0],
			     gdvp->gdv_view->gv_view_params.gos_text_color[1],
			     gdvp->gdv_view->gv_view_params.gos_text_color[2],
			     1, 1.0);
	(void)dm_draw_string_2d(gdvp->gdv_dmp, bu_vls_addr(&vls), -0.98, -0.965, 10, 0);
	bu_vls_free(&vls);
    }

    /* Draw the angle distance cursor */
    if (gdvp->gdv_view->gv_adc.draw)
	dm_draw_adc(gdvp->gdv_dmp, &(gdvp->gdv_view->gv_adc), gdvp->gdv_view->gv_view2model, gdvp->gdv_view->gv_model2view);

    /* Draw grid */
    if (gdvp->gdv_view->gv_grid.draw)
	dm_draw_grid(gdvp->gdv_dmp, &gdvp->gdv_view->gv_grid, gdvp->gdv_view->gv_scale, gdvp->gdv_view->gv_model2view, gdvp->gdv_gop->go_gedp->ged_wdbp->dbip->dbi_base2local);

    /* Draw rect */
    if (gdvp->gdv_view->gv_rect.draw && gdvp->gdv_view->gv_rect.line_width)
	dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect);
}


struct path_match_data {
    struct db_full_path *s_fpath;
    struct db_i *dbip;
};


HIDDEN int
key_matches_path(struct bu_hash_entry *entry, void *udata)
{
    struct path_match_data *data = (struct path_match_data *)udata;
    struct db_full_path entry_fpath;
    char *path_string;
    int ret;

    path_string = (char *)bu_get_hash_key(entry);
    if (db_string_to_path(&entry_fpath, data->dbip, path_string) < 0) {
	return 0;
    }

    ret = 0;
    if (db_full_path_match_top(&entry_fpath, data->s_fpath)) {
	ret = 1;
    }

    db_free_full_path(&entry_fpath);
    return ret;
}


HIDDEN void
go_draw_solid(struct ged_dm_view *gdvp, struct solid *sp)
{
    struct ged_obj *gop = gdvp->gdv_gop;
    dm *dmp = gdvp->gdv_dmp;
    struct bu_hash_entry *entry;
    struct path_edit_params *params = NULL;
    mat_t save_mat, edit_model2view;
    struct path_match_data data;

    data.s_fpath = &sp->s_fullpath;
    data.dbip = gop->go_gedp->ged_wdbp->dbip;
    entry = bu_hash_tbl_traverse(gop->go_edited_paths, key_matches_path, &data);

    if (entry != NULL) {
	params = (struct path_edit_params *)bu_get_hash_value(entry);
    }
    if (params) {
	MAT_COPY(save_mat, gdvp->gdv_view->gv_model2view);
	bn_mat_mul(edit_model2view, gdvp->gdv_view->gv_model2view, params->edit_mat);
	dm_loadmatrix(dmp, edit_model2view, 0);
    }

    if (gop->go_dlist_on) {
	dm_draw_dlist(dmp, sp->s_dlist);
    } else {
	if (sp->s_iflag == UP)
	    (void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_transparency);
	else
	    (void)dm_set_fg(dmp,
				 (unsigned char)sp->s_color[0],
				 (unsigned char)sp->s_color[1],
				 (unsigned char)sp->s_color[2], 0, sp->s_transparency);

	if (sp->s_hiddenLine) {
	    (void)dm_draw_vlist_hidden_line(dmp, (struct bn_vlist *)&sp->s_vlist);
	} else {
	    (void)dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist);
	}
    }
    if (params) {
	dm_loadmatrix(dmp, save_mat, 0);
    }
}


HIDDEN void
go_draw_other(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    int width = dm_get_width(gdvp->gdv_dmp);
    fastf_t sf = (fastf_t)(gdvp->gdv_view->gv_size) / (fastf_t)width;

    if (gdvp->gdv_view->gv_data_arrows.gdas_draw)
	go_dm_draw_arrows(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_arrows, sf);

    if (gdvp->gdv_view->gv_sdata_arrows.gdas_draw)
	go_dm_draw_arrows(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_arrows, sf);

    if (gdvp->gdv_view->gv_data_axes.draw)
	dm_draw_data_axes(gdvp->gdv_dmp,
			  sf,
			  &gdvp->gdv_view->gv_data_axes);

    if (gdvp->gdv_view->gv_sdata_axes.draw)
	dm_draw_data_axes(gdvp->gdv_dmp,
			  sf,
			  &gdvp->gdv_view->gv_sdata_axes);

    if (gdvp->gdv_view->gv_data_lines.gdls_draw)
	go_dm_draw_lines(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_lines);

    if (gdvp->gdv_view->gv_sdata_lines.gdls_draw)
	go_dm_draw_lines(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_lines);

    if (gdvp->gdv_view->gv_data_polygons.gdps_draw)
	go_dm_draw_polys(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_polygons, gdvp->gdv_view->gv_mode);

    if (gdvp->gdv_view->gv_sdata_polygons.gdps_draw)
	go_dm_draw_polys(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_polygons, gdvp->gdv_view->gv_mode);

    /* Restore to non-rotated, full brightness */
    (void)dm_normal(gdvp->gdv_dmp);
    go_draw_faceplate(gop, gdvp);

    if (gdvp->gdv_view->gv_data_labels.gdls_draw)
	go_dm_draw_labels(gdvp->gdv_dmp, &gdvp->gdv_view->gv_data_labels, gdvp->gdv_view->gv_model2view);

    if (gdvp->gdv_view->gv_sdata_labels.gdls_draw)
	go_dm_draw_labels(gdvp->gdv_dmp, &gdvp->gdv_view->gv_sdata_labels, gdvp->gdv_view->gv_model2view);

    /* Draw labels */
    if (gdvp->gdv_view->gv_prim_labels.gos_draw) {
	register int i;

	for (i = 0; i < gop->go_prim_label_list_size; ++i) {
	    dm_draw_labels(gdvp->gdv_dmp,
			   gop->go_gedp->ged_wdbp,
			   bu_vls_addr(&gop->go_prim_label_list[i]),
			   gdvp->gdv_view->gv_model2view,
			   gdvp->gdv_view->gv_prim_labels.gos_text_color,
			   NULL, NULL);
	}
    }
}


void
go_refresh(struct ged_obj *gop, struct ged_dm_view *gdvp)
{
    int restore_zbuffer = 0;

    /* Turn off the zbuffer if the framebuffer is active AND the zbuffer is on. */
    if (gdvp->gdv_fbs.fbs_mode != TCLCAD_OBJ_FB_MODE_OFF && dm_get_zbuffer(gdvp->gdv_dmp)) {
	(void)dm_set_zbuffer(gdvp->gdv_dmp, 0);
	restore_zbuffer = 1;
    }

    (void)dm_draw_begin(gdvp->gdv_dmp);
    go_refresh_draw(gop, gdvp, restore_zbuffer);
    (void)dm_draw_end(gdvp->gdv_dmp);
}


void
go_refresh_draw(struct ged_obj *gop, struct ged_dm_view *gdvp, int restore_zbuffer)
{
    if (gdvp->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_OVERLAY) {
	if (gdvp->gdv_view->gv_rect.draw) {
	    go_draw(gdvp);

	    go_draw_other(gop, gdvp);

	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.pos[X], gdvp->gdv_view->gv_rect.pos[Y],
		       gdvp->gdv_view->gv_rect.dim[X], gdvp->gdv_view->gv_rect.dim[Y]);

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 1);

	    if (gdvp->gdv_view->gv_rect.line_width)
		dm_draw_rect(gdvp->gdv_dmp, &gdvp->gdv_view->gv_rect);
	} else {
	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 1);
	}

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer(gdvp->gdv_dmp, 1);
	}

	return;
    } else if (gdvp->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_INTERLAY) {
	go_draw(gdvp);

	/* disable write to depth buffer */
	(void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	if (gdvp->gdv_view->gv_rect.draw) {
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp,
		       gdvp->gdv_view->gv_rect.pos[X], gdvp->gdv_view->gv_rect.pos[Y],
		       gdvp->gdv_view->gv_rect.dim[X], gdvp->gdv_view->gv_rect.dim[Y]);
	} else
	    fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
		       dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));

	/* enable write to depth buffer */
	(void)dm_set_depth_mask(gdvp->gdv_dmp, 1);

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer(gdvp->gdv_dmp, 1);
	}
    } else {
	if (gdvp->gdv_fbs.fbs_mode == TCLCAD_OBJ_FB_MODE_UNDERLAY) {
	    /* disable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 0);

	    if (gdvp->gdv_view->gv_rect.draw) {
		fb_refresh(gdvp->gdv_fbs.fbs_fbp,
			   gdvp->gdv_view->gv_rect.pos[X], gdvp->gdv_view->gv_rect.pos[Y],
			   gdvp->gdv_view->gv_rect.dim[X], gdvp->gdv_view->gv_rect.dim[Y]);
	    } else
		fb_refresh(gdvp->gdv_fbs.fbs_fbp, 0, 0,
			   dm_get_width(gdvp->gdv_dmp), dm_get_height(gdvp->gdv_dmp));

	    /* enable write to depth buffer */
	    (void)dm_set_depth_mask(gdvp->gdv_dmp, 1);
	}

	if (restore_zbuffer) {
	    (void)dm_set_zbuffer(gdvp->gdv_dmp, 1);
	}

	go_draw(gdvp);
    }

    go_draw_other(gop, gdvp);
}


struct application *
to_rt_gettrees_application(struct ged *gedp,
			   int argc,
			   const char *argv[])
{
    struct rt_i *rtip;
    struct application *ap;
    struct resource resp;

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
    BU_ASSERT_PTR(BU_PTBL_GET(&rtip->rti_resources, 0), !=, NULL);

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
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
