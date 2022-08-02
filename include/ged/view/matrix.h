/*                       M A T R I X . H
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
/** @addtogroup ged_matrix
 *
 * Geometry EDiting Library Angle Matrix/Quat Functions.
 *
 */
/** @{ */
/** @file ged/view/matrix.h */

#ifndef GED_VIEW_MATRIX_H
#define GED_VIEW_MATRIX_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS

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

/**
 * Set/get the perspective angle.
 */
GED_EXPORT extern int ged_perspective(struct ged *gedp, int argc, const char *argv[]);



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
 * Get or set the view center.
 */
GED_EXPORT extern int ged_center(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert a direction vector to az/el.
 */
GED_EXPORT extern int ged_dir2ae(struct ged *gedp, int argc, const char *argv[]);

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
 * Returns the inverse view size.
 */
GED_EXPORT extern int ged_isize(struct ged *gedp, int argc, const char *argv[]);

/**
 * Load the view
 */
GED_EXPORT extern int ged_loadview(struct ged *gedp, int argc, const char *argv[]);


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
 * Set/get the perspective matrix.
 */
GED_EXPORT extern int ged_pmat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get the pmodel2view matrix.
 */
GED_EXPORT extern int ged_pmodel2view(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get/set the view orientation using a quaternion
 */
GED_EXPORT extern int ged_quat(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the view from a direction vector and twist.
 */
GED_EXPORT extern int ged_qvrot(struct ged *gedp, int argc, const char *argv[]);

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
 * Scale the view.
 */
GED_EXPORT extern int ged_scale_args(struct ged *gedp, int argc, const char *argv[], fastf_t *sf1, fastf_t *sf2, fastf_t *sf3);
GED_EXPORT extern int ged_scale(struct ged *gedp, int argc, const char *argv[]);

/**
 * Set the view orientation given the angles x, y and z in degrees.
 */
GED_EXPORT extern int ged_setview(struct ged *gedp, int argc, const char *argv[]);

/**
 * Get or set the view size.
 */
GED_EXPORT extern int ged_size(struct ged *gedp, int argc, const char *argv[]);


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
 * Get/set the view orientation using yaw, pitch and roll
 */
GED_EXPORT extern int ged_ypr(struct ged *gedp, int argc, const char *argv[]);

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
 * Slew the view
 */
GED_EXPORT extern int ged_slew(struct ged *gedp, int argc, const char *argv[]);


/**
 * Set/get the rotation matrix.
 */
GED_EXPORT extern int ged_rmat(struct ged *gedp, int argc, const char *argv[]);


/**
 * Translate the view.
 */
GED_EXPORT extern int ged_tra_args(struct ged *gedp, int argc, const char *argv[], char *coord, vect_t tvec);
GED_EXPORT extern int ged_tra(struct ged *gedp, int argc, const char *argv[]);

/**
 * Convert the specified view point to a model point.
 */
GED_EXPORT extern int ged_v2m_point(struct ged *gedp, int argc, const char *argv[]);



__END_DECLS

#endif /* GED_VIEW_MATRIX_H */

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
