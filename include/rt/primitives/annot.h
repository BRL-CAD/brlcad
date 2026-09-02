/*                        A N N O T . H
 * BRL-CAD
 *
 * Copyright (c) 2017-2026 United States Government as represented by
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
/** @addtogroup rt_annotation */
/** @{ */
/** @file rt/primitives/annot.h */

#ifndef RT_PRIMITIVES_ANNOT_H
#define RT_PRIMITIVES_ANNOT_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "rt/geom.h"
#include "rt/defines.h"

__BEGIN_DECLS

struct db_i;
struct xray;

#define RT_ANNOT_SCREEN_DPI 96.0
#define RT_ANNOT_MILLIMETERS_PER_INCH 25.4
#define RT_ANNOT_SCREEN_PIXELS_PER_MM \
    (RT_ANNOT_SCREEN_DPI / RT_ANNOT_MILLIMETERS_PER_INCH)

RT_EXPORT extern struct rt_annot_internal *rt_copy_annot(const struct rt_annot_internal *annot_ip);

/** Validate annotation topology, model-space placement, and optional segment
 * presentation data.  Returns zero when valid. */
RT_EXPORT extern int rt_annot_validate(const struct rt_annot_internal *annot_ip,
	struct bu_vls *messages);

/** Opaque, immutable collection of prepared annotation instances. */
struct rt_annot_scene;

/** Camera state needed to evaluate view-space annotations.  Pixel sample
 * coordinates use a lower-left origin. */
struct rt_annot_view {
    mat_t model2view;
    size_t width;
    size_t height;
    fastf_t perspective;
};

/** A zero-thickness annotation coverage event.  path is owned by the scene. */
struct rt_annot_hit {
    const char *path;
    size_t segment;
    uint32_t role;
    point_t point;
    fastf_t distance;
    unsigned char color[4];
    int screen_space;
    int visible;
};

/** Prepare annotations referenced by the supplied top-level database paths.
 * Boolean operations are intentionally ignored: annotations are
 * informational leaves, not solids. */
RT_EXPORT extern struct rt_annot_scene *rt_annot_scene_create(
	struct db_i *dbip, int path_count, const char * const *paths,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol);

RT_EXPORT extern void rt_annot_scene_destroy(struct rt_annot_scene *scene);
RT_EXPORT extern size_t rt_annot_scene_count(const struct rt_annot_scene *scene);
/** Return model-space bounds for prepared annotations.  Screen-space geometry
 * contributes its model-space anchor because its extent is view-dependent. */
RT_EXPORT extern int rt_annot_scene_bounds(const struct rt_annot_scene *scene,
	point_t minimum, point_t maximum);

/** Return the top visible coverage event for a primary sample. */
RT_EXPORT extern int rt_annot_scene_query(const struct rt_annot_scene *scene,
	const struct rt_annot_view *view, const struct xray *ray,
	fastf_t sample_x, fastf_t sample_y, fastf_t scene_distance,
	struct rt_annot_hit *hit);

/** Return all visible primary-sample coverage events in back-to-front order.
 * Model-space events precede screen-space events.  When hits is NULL or
 * capacity is zero, only the required result count is returned. */
RT_EXPORT extern size_t rt_annot_scene_query_layers(
	const struct rt_annot_scene *scene, const struct rt_annot_view *view,
	const struct xray *ray, fastf_t sample_x, fastf_t sample_y,
	fastf_t scene_distance, struct rt_annot_hit *hits, size_t capacity);

/** Return all model-space coverage events, sorted by ray distance.  When hits
 * is NULL or capacity is zero, only the required result count is returned. */
RT_EXPORT extern size_t rt_annot_scene_query_model(const struct rt_annot_scene *scene,
	const struct xray *ray, fastf_t scene_distance,
	struct rt_annot_hit *hits, size_t capacity);

/** Alpha-composite a coverage result over a linear RGB sample. */
RT_EXPORT extern void rt_annot_hit_blend(fastf_t color[3],
	const struct rt_annot_hit *hit);

/** Alpha-composite all visible primary-sample coverage over color.  If
 * front_hit is non-NULL, it receives the frontmost event. */
RT_EXPORT extern int rt_annot_scene_composite(const struct rt_annot_scene *scene,
	const struct rt_annot_view *view, const struct xray *ray,
	fastf_t sample_x, fastf_t sample_y, fastf_t scene_distance,
	fastf_t color[3], struct rt_annot_hit *front_hit);

/** Generate one annotation segment in annotation-local XY coordinates.  This
 * is shared by plotting and the coverage preparer to keep curve and font
 * expansion identical. */
RT_EXPORT extern int rt_annot_segment_vlist(struct bu_list *vlfree,
	struct bu_list *vhead, const struct bg_tess_tol *ttol,
	const struct rt_annot_internal *annot_ip, size_t segment);

__END_DECLS

/** @} */

#endif /* RT_PRIMITIVES_ANNOT_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
