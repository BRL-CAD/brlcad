/*                        V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @addtogroup ged_view
 *
 * Geometry EDiting Library Database View Related Functions.
 *
 */
/** @{ */
/** @file ged/view.h */

#ifndef GED_VIEW_H
#define GED_VIEW_H

#include "common.h"
#include "ged/defines.h"
#include "bg/polygon.h"
#include "ged/view/adc.h"
#include "ged/view/matrix.h"
#include "ged/view/select.h"
#include "ged/view/state.h"

__BEGIN_DECLS


/** Check if a drawable exists */
#define GED_CHECK_DRAWABLE(_gedp, _flags) \
    if (_gedp->ged_gdp == GED_DRAWABLE_NULL) { \
	int ged_check_drawable_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_drawable_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A drawable does not exist."); \
	} \
	return (_flags); \
    }

/** Check if a view exists */
#define GED_CHECK_VIEW(_gedp, _flags) \
    if (_gedp->ged_gvp == GED_VIEW_NULL) { \
	int ged_check_view_quiet = (_flags) & GED_QUIET; \
	if (!ged_check_view_quiet) { \
	    bu_vls_trunc((_gedp)->ged_result_str, 0); \
	    bu_vls_printf((_gedp)->ged_result_str, "A view does not exist."); \
	} \
	return (_flags); \
    }

struct ged_bv_data {
    struct db_full_path s_fullpath;
    void *u_data;
};
/* Check ged_bv data associated with a display list */
GED_EXPORT extern unsigned long long ged_dl_hash(struct display_list *dl);


/* defined in display_list.c */
GED_EXPORT void dl_set_iflag(struct bu_list *hdlp, int iflag);
GED_EXPORT extern void dl_color_soltab(struct bu_list *hdlp);
GED_EXPORT extern void dl_erasePathFromDisplay(struct ged *gedp, const char *path, int allow_split);
GED_EXPORT extern struct display_list *dl_addToDisplay(struct bu_list *hdlp, struct db_i *dbip, const char *name);

GED_EXPORT extern int invent_solid(struct ged *gedp, char *name, struct bu_list *vhead, long int rgb, int copy, fastf_t transparency, int dmode, int csoltab);

/**
 * Grid utility command.
 */
GED_EXPORT extern int ged_grid(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert grid coordinates to model coordinates.
 */
GED_EXPORT extern int ged_grid2model_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert grid coordinates to view coordinates.
 */
GED_EXPORT extern int ged_grid2view_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Overlay the specified 2D/3D UNIX plot file
 */
GED_EXPORT extern int ged_overlay(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a unix plot file of the currently displayed objects.
 */
GED_EXPORT extern int ged_plot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a png file of the view.
 */
GED_EXPORT extern int ged_png(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_screen_grab(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a postscript file of the view.
 */
GED_EXPORT extern int ged_ps(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns the solid table & vector list as a string
 */
GED_EXPORT extern int ged_solid_report(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save the view
 */
GED_EXPORT extern int ged_saveview(struct ged *gedp, int argc, const char *argv[]);


/**
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 */
GED_EXPORT extern int ged_tree(struct ged *gedp, int argc, const char *argv[]);


/**
 * Vector drawing utility.
 */
GED_EXPORT extern int ged_vdraw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set view attributes
 */
GED_EXPORT extern int ged_view_func(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the unix plot output mode
 */
GED_EXPORT extern int ged_set_uplotOutputMode(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern int ged_export_polygon(struct ged *gedp, bv_data_polygon_state *gdpsp, size_t polygon_i, const char *sname);
GED_EXPORT extern struct bg_polygon *ged_import_polygon(struct ged *gedp, const char *sname);
GED_EXPORT extern int ged_polygons_overlap(struct ged *gedp, struct bg_polygon *polyA, struct bg_polygon *polyB);
GED_EXPORT extern void ged_polygon_fill_segments(struct ged *gedp, struct bg_polygon *poly, vect2d_t vfilldir, fastf_t vfilldelta);

GED_EXPORT extern struct bview * ged_find_view(struct ged *gedp, const char *key);

__END_DECLS

#endif /* GED_VIEW_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
