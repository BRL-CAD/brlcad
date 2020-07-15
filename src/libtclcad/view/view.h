/*                          V I E W . H
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
/** @file view.h
 *
 * Brief description
 *
 */

#include "common.h"
#include "ged.h"
#include "tclcad.h"

/* Arrows */
extern void go_dm_draw_arrows(struct dm *dmp, struct bview_data_arrow_state *gdasp, fastf_t sf);

extern int to_data_arrows(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  ged_func_ptr func,
			  const char *usage,
			  int maxargs);
extern int to_data_arrows_func(Tcl_Interp *interp,
			       struct ged *gedp,
			       struct ged_dm_view *gdvp,
			       int argc,
			       const char *argv[]);

/* Axes */
extern int to_axes(struct ged *gedp,
		   struct ged_dm_view *gdvp,
		   struct bview_axes_state *gasp,
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
			     struct ged_dm_view *gdvp,
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


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
