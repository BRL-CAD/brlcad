/*                          V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file view.h
 *
 * Brief description
 *
 */

#include "common.h"
#include "vmath.h"
#include "ged.h"
#include "tclcad.h"

/* Arrows */
extern int to_data_arrows(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
extern int to_data_arrows_func(Tcl_Interp *interp,
			       struct ged *gedp,
			       struct bview *gdvp,
			       int argc,
			       const char *argv[]);

/* Autoview */
extern int to_autoview(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr func,
		       const char *usage,
		       int maxargs);
extern void to_autoview_view(struct bview *gdvp, const char *scale);
extern void to_autoview_all_views(struct tclcad_obj *top);
extern int to_autoview_func(struct ged *gedp,
			    int argc,
			    const char *argv[],
			    ged_func_ptr func,
			    const char *usage,
			    int maxargs);


/* Axes */
extern int to_axes(struct ged *gedp,
		   struct bview *gdvp,
		   struct bv_axes *gasp,
		   int argc,
		   const char *argv[],
		   const char *usage);
extern int to_data_axes(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);
extern int to_data_axes_func(Tcl_Interp *interp,
			     struct ged *gedp,
			     struct bview *gdvp,
			     int argc,
			     const char *argv[]);
extern int to_model_axes(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
extern int to_view_axes(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);

/* Draw */
extern void go_draw(struct bview *gdvp);
extern int to_edit_redraw(struct ged *gedp, int argc, const char *argv[]);
extern int to_redraw(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     ged_func_ptr func,
		     const char *usage,
		     int maxargs);
extern int to_blast(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    ged_func_ptr func,
		    const char *usage,
		    int maxargs);

/* Faceplate */
extern int to_faceplate(struct ged *gedp,
			int argc,
			const char *argv[],
			ged_func_ptr func,
			const char *usage,
			int maxargs);


/* Labels */
extern int to_data_labels(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
extern int to_data_labels_func(Tcl_Interp *interp,
			       struct ged *gedp,
			       struct bview *gdvp,
			       int argc,
			       const char *argv[]);
extern int to_prim_label(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);

/* Lines */
int to_data_lines(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);

/* Refresh */
extern void to_refresh_handler(void *clientdata);
extern int to_handle_refresh(struct ged *gedp,
			     const char *name);
extern int to_refresh(struct ged *gedp,
		      int argc,
		      const char *argv[],
		      ged_func_ptr func,
		      const char *usage,
		      int maxargs);
extern int to_refresh_all(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
extern int to_refresh_on(struct ged *gedp,
			 int argc,
			 const char *argv[],
			 ged_func_ptr func,
			 const char *usage,
			 int maxargs);
extern void to_refresh_all_views(struct tclcad_obj *top);
extern void to_refresh_view(struct bview *gdvp);

/* Util */
extern int to_is_viewable(struct bview *gdvp);

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
