/*                        V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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

/* defined in adc.c */
GED_EXPORT extern void ged_calc_adc_pos(struct bview *gvp);
GED_EXPORT extern void ged_calc_adc_a1(struct bview *gvp);
GED_EXPORT extern void ged_calc_adc_a2(struct bview *gvp);
GED_EXPORT extern void ged_calc_adc_dst(struct bview *gvp);

/* defined in display_list.c */
GED_EXPORT void dl_set_iflag(struct bu_list *hdlp, int iflag);
GED_EXPORT extern void dl_color_soltab(struct bu_list *hdlp);
GED_EXPORT extern void dl_erasePathFromDisplay(struct bu_list *hdlp,
	                                       struct db_i *dbip,
					       void (*callback)(unsigned int, int),
					       const char *path,
					       int allow_split,
					       struct solid *freesolid);
GED_EXPORT extern struct display_list *dl_addToDisplay(struct bu_list *hdlp, struct db_i *dbip, const char *name);
/* When finalized, this stuff belongs in a header file of its own */
struct polygon_header {
    uint32_t magic;             /* magic number */
    int ident;                  /* identification number */
    int interior;               /* >0 => interior loop, gives ident # of exterior loop */
    vect_t normal;                      /* surface normal */
    unsigned char color[3];     /* Color from containing region */
    int npts;                   /* number of points */
};
#define POLYGON_HEADER_MAGIC 0x8623bad2
GED_EXPORT extern void dl_polybinout(struct bu_list *hdlp, struct polygon_header *ph, FILE *fp);

GED_EXPORT extern int invent_solid(struct bu_list *hdlp, struct db_i *dbip, void (*callback_create)(struct solid *), void (*callback_free)(unsigned int, int), char *name, struct bu_list *vhead, long int rgb, int copy, fastf_t transparency, int dmode, struct solid *freesolid, int csoltab);

/* defined in ged.c */
GED_EXPORT extern void ged_view_init(struct bview *gvp);

/* defined in grid.c */
GED_EXPORT extern void ged_snap_to_grid(struct ged *gedp, fastf_t *vx, fastf_t *vy);


/* Defined in vutil.c */
GED_EXPORT extern void ged_persp_mat(fastf_t *m,
				     fastf_t fovy,
				     fastf_t aspect,
				     fastf_t near1,
				     fastf_t far1,
				     fastf_t backoff);
GED_EXPORT extern void ged_mike_persp_mat(fastf_t *pmat,
					  const fastf_t *eye);
GED_EXPORT extern void ged_deering_persp_mat(fastf_t *m,
					     const fastf_t *l,
					     const fastf_t *h,
					     const fastf_t *eye);
GED_EXPORT extern void ged_view_update(struct bview *gvp);


/**
 * Angle distance cursor.
 */
GED_EXPORT extern int ged_adc(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert az/el to a direction vector.
 */
GED_EXPORT extern int ged_ae2dir(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get or set the azimuth, elevation and twist.
 */
GED_EXPORT extern int ged_aet(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate angle degrees about the specified axis
 */
GED_EXPORT extern int ged_arot_args(struct ged *gedp, int argc, const char *argv[], mat_t rmat);
GED_EXPORT extern int ged_arot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Auto-adjust the view so that all displayed geometry is in view
 */
GED_EXPORT extern int ged_autoview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase all currently displayed geometry and draw the specified object(s)
 */
GED_EXPORT extern int ged_blast(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get or set the view center.
 */
GED_EXPORT extern int ged_center(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert a direction vector to az/el.
 */
GED_EXPORT extern int ged_dir2ae(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare object(s) for display
 */
GED_EXPORT extern int ged_draw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare object(s) for display
 */
GED_EXPORT extern int ged_E(struct ged *gedp, int argc, const char *argv[]);

/**
 * Prepare all regions with the given air code(s) for display
 */
GED_EXPORT extern int ged_eac(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase objects from the display.
 */
GED_EXPORT extern int ged_erase(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the eye point
 */
GED_EXPORT extern int ged_eye(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the eye position
 */
GED_EXPORT extern int ged_eye_pos(struct ged *gedp, int argc, const char *argv[]);


/**
 * Get view size and center such that all displayed solids would be in view
 */
GED_EXPORT extern int ged_get_autoview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the viewsize, orientation and eye point.
 */
GED_EXPORT extern int ged_get_eyemodel(struct ged *gedp, int argc, const char *argv[]);


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
 * Returns how an object is being displayed
 */
GED_EXPORT extern int ged_how(struct ged *gedp, int argc, const char *argv[]);

/**
 * Illuminate/highlight database object.
 */
GED_EXPORT extern int ged_illum(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns the inverse view size.
 */
GED_EXPORT extern int ged_isize(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the keypoint
 */
GED_EXPORT extern int ged_keypoint(struct ged *gedp, int argc, const char *argv[]);

/**
 * Load the view
 */
GED_EXPORT extern int ged_loadview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Configure Level of Detail drawing.
 */
GED_EXPORT extern int ged_lod(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the look-at point
 */
GED_EXPORT extern int ged_lookat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert the specified model point to a view point.
 */
GED_EXPORT extern int ged_m2v_point(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert model coordinates to grid coordinates.
 */
GED_EXPORT extern int ged_model2grid_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the model to view matrix
 */
GED_EXPORT extern int ged_model2view(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert model coordinates to view coordinates.
 */
GED_EXPORT extern int ged_model2view_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the view. Note - x, y and z are rotations in model coordinates.
 */
GED_EXPORT extern int ged_mrot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the view orientation using a quaternion.
 */
GED_EXPORT extern int ged_orient(struct ged *gedp, int argc, const char *argv[]);

/**
 * Overlay the specified 2D/3D UNIX plot file
 */
GED_EXPORT extern int ged_overlay(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the perspective angle.
 */
GED_EXPORT extern int ged_perspective(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a unix plot file of the currently displayed objects.
 */
GED_EXPORT extern int ged_plot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the perspective matrix.
 */
GED_EXPORT extern int ged_pmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the pmodel2view matrix.
 */
GED_EXPORT extern int ged_pmodel2view(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a png file of the view.
 */
GED_EXPORT extern int ged_png(struct ged *gedp, int argc, const char *argv[]);
GED_EXPORT extern int ged_screen_grab(struct ged *gedp, int argc, const char *argv[]);

/**
 * Write out polygons (binary) of the currently displayed geometry.
 */
GED_EXPORT extern int ged_polybinout(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set point of view
 */
GED_EXPORT extern int ged_pov(struct ged *gedp, int argc, const char *argv[]);

/**
 * Create a postscript file of the view.
 */
GED_EXPORT extern int ged_ps(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the view orientation using a quaternion
 */
GED_EXPORT extern int ged_quat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the view from a direction vector and twist.
 */
GED_EXPORT extern int ged_qvrot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rubber band rectangle utility.
 */
GED_EXPORT extern int ged_rect(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns the solid table & vector list as a string
 *
 * @todo - there's no way this function should be limited to the
 * documented purpose above...
 */
GED_EXPORT extern int ged_report(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the view.
 */
GED_EXPORT extern int ged_rot_args(struct ged *gedp, int argc, const char *argv[], char *coord, mat_t rmat);
GED_EXPORT extern int ged_rot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the rotate_about point.
 */
GED_EXPORT extern int ged_rotate_about(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns a list of items within the previously defined rectangle.
 */
GED_EXPORT extern int ged_rselect(struct ged *gedp, int argc, const char *argv[]);

/**
 * Save the view
 */
GED_EXPORT extern int ged_saveview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Scale the view.
 */
GED_EXPORT extern int ged_scale_args(struct ged *gedp, int argc, const char *argv[], fastf_t *sf1, fastf_t *sf2, fastf_t *sf3);
GED_EXPORT extern int ged_scale(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns a list of items within the specified rectangle or circle.
 */
GED_EXPORT extern int ged_select(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return ged selections for specified object. Created if it doesn't
 * exist.
 */
GED_EXPORT struct rt_object_selections *ged_get_object_selections(struct ged *gedp,
								  const char *object_name);

/**
 * Return ged selections of specified kind for specified object.
 * Created if it doesn't exist.
 */
GED_EXPORT struct rt_selection_set *ged_get_selection_set(struct ged *gedp,
							  const char *object_name,
							  const char *selection_name);


/**
 * Set the view orientation given the angles x, y and z in degrees.
 */
GED_EXPORT extern int ged_setview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set/get the shaded mode.
 */
GED_EXPORT extern int ged_shaded_mode(struct ged *gedp, int argc, const char *argv[]);


/**
 * Get or set the view size.
 */
GED_EXPORT extern int ged_size(struct ged *gedp, int argc, const char *argv[]);


/**
 * Translate the view.
 */
GED_EXPORT extern int ged_tra_args(struct ged *gedp, int argc, const char *argv[], char *coord, vect_t tvec);
GED_EXPORT extern int ged_tra(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 */
GED_EXPORT extern int ged_tree(struct ged *gedp, int argc, const char *argv[]);

/**
 * Recalculate plots for displayed objects.
 */
GED_EXPORT extern int ged_redraw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert the specified view point to a model point.
 */
GED_EXPORT extern int ged_v2m_point(struct ged *gedp, int argc, const char *argv[]);

/**
 * Vector drawing utility.
 */
GED_EXPORT extern int ged_vdraw(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set view attributes
 */
GED_EXPORT extern int ged_view_func(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert view coordinates to grid coordinates.
 */
GED_EXPORT extern int ged_view2grid_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the view2model matrix.
 */
GED_EXPORT extern int ged_view2model(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert view coordinates to model coordinates.
 */
GED_EXPORT extern int ged_view2model_lu(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert a view vector to a model vector.
 */
GED_EXPORT extern int ged_view2model_vec(struct ged *gedp, int argc, const char *argv[]);

/**
 * Rotate the view. Note - x, y and z are rotations in view coordinates.
 */
GED_EXPORT extern int ged_vrot(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return the view direction.
 */
GED_EXPORT extern int ged_viewdir(struct ged *gedp, int argc, const char *argv[]);

/**
 * List the objects currently prepped for drawing
 */
GED_EXPORT extern int ged_who(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the view orientation using yaw, pitch and roll
 */
GED_EXPORT extern int ged_ypr(struct ged *gedp, int argc, const char *argv[]);

/**
 * Erase all currently displayed geometry
 */
GED_EXPORT extern int ged_zap(struct ged *gedp, int argc, const char *argv[]);

/**
 * Zoom the view in or out.
 */
GED_EXPORT extern int ged_zoom(struct ged *gedp, int argc, const char *argv[]);


/* defined in clip.c */
GED_EXPORT extern int ged_clip(fastf_t *xp1,
			       fastf_t *yp1,
			       fastf_t *xp2,
			       fastf_t *yp2);
GED_EXPORT extern int ged_vclip(vect_t a,
				vect_t b,
				fastf_t *min,
				fastf_t *max);

/**
 * Set/get the rotation matrix.
 */
GED_EXPORT extern int ged_rmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the unix plot output mode
 */
GED_EXPORT extern int ged_set_uplotOutputMode(struct ged *gedp, int argc, const char *argv[]);

/**
 * Slew the view
 */
GED_EXPORT extern int ged_slew(struct ged *gedp, int argc, const char *argv[]);

GED_EXPORT extern bview_polygon *ged_clip_polygon(ClipType op, bview_polygon *subj, bview_polygon *clip, fastf_t sf, matp_t model2view, matp_t view2model);
GED_EXPORT extern bview_polygon *ged_clip_polygons(ClipType op, bview_polygons *subj, bview_polygons *clip, fastf_t sf, matp_t model2view, matp_t view2model);
GED_EXPORT extern int ged_export_polygon(struct ged *gedp, bview_data_polygon_state *gdpsp, size_t polygon_i, const char *sname);
GED_EXPORT extern bview_polygon *ged_import_polygon(struct ged *gedp, const char *sname);
GED_EXPORT extern fastf_t ged_find_polygon_area(bview_polygon *gpoly, fastf_t sf, matp_t model2view, fastf_t size);
GED_EXPORT extern int ged_polygons_overlap(struct ged *gedp, bview_polygon *polyA, bview_polygon *polyB);
GED_EXPORT extern void ged_polygon_fill_segments(struct ged *gedp, bview_polygon *poly, vect2d_t vfilldir, fastf_t vfilldelta);

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
