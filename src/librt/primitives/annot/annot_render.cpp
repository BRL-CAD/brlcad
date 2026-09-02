/*                     A N N O T _ R E N D E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bu/ptbl.h"
#include "bv/vlist.h"
#include "raytrace.h"
#include "rt/primitives/annot.h"
#include "rt/search.h"
#include "RTree.h"

namespace {

constexpr fastf_t TEXT_STROKE_RATIO = 1.0 / 14.0;
constexpr fastf_t GEOMETRY_STROKE_RATIO = 0.01;
constexpr fastf_t COVERAGE_EPSILON = 1.0e-9;
constexpr fastf_t RELATIVE_EPSILON =
    64.0 * std::numeric_limits<fastf_t>::epsilon();

struct point2 {
    fastf_t x = 0.0;
    fastf_t y = 0.0;
};

struct render_segment {
    size_t index = 0;
    uint32_t role = RT_ANNOT_ROLE_UNSPECIFIED;
    uint32_t pattern = RT_ANNOT_LINE_CONTINUOUS;
    fastf_t width_scale = 1.0;
    bool text = false;
    bool fill = false;
    std::array<unsigned char, 4> color = {255, 255, 255, 255};
    std::vector<std::vector<point2>> paths;
    std::vector<std::array<point2, 3>> triangles;
};

using segment_tree = RTree<size_t, fastf_t, 2>;
using instance_tree = RTree<size_t, fastf_t, 3>;

struct render_instance {
    std::string path;
    bool screen_space = false;
    point_t anchor = VINIT_ZERO;
    vect_t u = VINIT_ZERO;
    vect_t v = VINIT_ZERO;
    fastf_t base_width = 0.0;
    point2 local_minimum;
    point2 local_maximum;
    bool have_local_bounds = false;
    std::vector<render_segment> segments;
    std::unique_ptr<segment_tree> segment_index;
};

struct coverage_layer {
    struct rt_annot_hit hit = {};
    fastf_t screen_depth = 0.0;
    size_t draw_order = 0;
};

static void
include_local_point(point2 &minimum, point2 &maximum, bool &have_bounds,
	const point2 &point)
{
    if (!have_bounds) {
	minimum = point;
	maximum = point;
	have_bounds = true;
	return;
    }
    minimum.x = std::min(minimum.x, point.x);
    minimum.y = std::min(minimum.y, point.y);
    maximum.x = std::max(maximum.x, point.x);
    maximum.y = std::max(maximum.y, point.y);
}

static bool
segment_bounds(const render_segment &segment, fastf_t base_width,
	point2 &minimum, point2 &maximum)
{
    bool have_bounds = false;

    for (const auto &path : segment.paths)
	for (const point2 &point : path)
	    include_local_point(minimum, maximum, have_bounds, point);
    for (const auto &triangle : segment.triangles)
	for (const point2 &point : triangle)
	    include_local_point(minimum, maximum, have_bounds, point);
    if (!have_bounds)
	return false;
    const fastf_t radius = segment.fill ? COVERAGE_EPSILON :
	0.5 * std::max(base_width * segment.width_scale,
	    (fastf_t)COVERAGE_EPSILON);
    minimum.x -= radius;
    minimum.y -= radius;
    maximum.x += radius;
    maximum.y += radius;
    return true;
}

static void
prepare_local_index(render_instance &instance)
{
    instance.segment_index = std::make_unique<segment_tree>();
    for (size_t i = 0; i < instance.segments.size(); ++i) {
	point2 minimum, maximum;
	if (!segment_bounds(instance.segments[i], instance.base_width,
		minimum, maximum))
	    continue;
	const fastf_t minimum_values[2] = {minimum.x, minimum.y};
	const fastf_t maximum_values[2] = {maximum.x, maximum.y};
	instance.segment_index->Insert(minimum_values, maximum_values, i);
	include_local_point(instance.local_minimum, instance.local_maximum,
	    instance.have_local_bounds, minimum);
	include_local_point(instance.local_minimum, instance.local_maximum,
	    instance.have_local_bounds, maximum);
    }
    if (!instance.have_local_bounds)
	include_local_point(instance.local_minimum, instance.local_maximum,
	    instance.have_local_bounds, {0.0, 0.0});
}

static fastf_t
distance_sq(const point2 &a, const point2 &b)
{
    const fastf_t dx = a.x - b.x;
    const fastf_t dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static bool
path_is_closed(const std::vector<point2> &path)
{
    return path.size() >= 3 &&
	distance_sq(path.front(), path.back()) <=
	COVERAGE_EPSILON * COVERAGE_EPSILON;
}

static int
polygon_winding(const point2 &p, const std::vector<point2> &polygon)
{
    int winding = 0;

    if (polygon.size() < 3)
	return 0;
    for (size_t i = 0; i < polygon.size(); ++i) {
	const point2 &a = polygon[i];
	const point2 &b = polygon[(i + 1) % polygon.size()];
	const fastf_t side = (b.x - a.x) * (p.y - a.y) -
	    (p.x - a.x) * (b.y - a.y);
	if (a.y <= p.y && b.y > p.y && side > COVERAGE_EPSILON)
	    ++winding;
	else if (a.y > p.y && b.y <= p.y && side < -COVERAGE_EPSILON)
	    --winding;
    }
    return winding;
}

static bool
point_in_triangle(const point2 &p, const std::array<point2, 3> &triangle)
{
    const point2 &a = triangle[0];
    const point2 &b = triangle[1];
    const point2 &c = triangle[2];
    const fastf_t area = (b.x - a.x) * (c.y - a.y) -
	(b.y - a.y) * (c.x - a.x);
    if (std::fabs(area) <= COVERAGE_EPSILON)
	return false;
    const fastf_t ab = (p.x - b.x) * (a.y - b.y) -
	(p.y - b.y) * (a.x - b.x);
    const fastf_t bc = (p.x - c.x) * (b.y - c.y) -
	(p.y - c.y) * (b.x - c.x);
    const fastf_t ca = (p.x - a.x) * (c.y - a.y) -
	(p.y - a.y) * (c.x - a.x);
    const bool has_negative = ab < -COVERAGE_EPSILON ||
	bc < -COVERAGE_EPSILON || ca < -COVERAGE_EPSILON;
    const bool has_positive = ab > COVERAGE_EPSILON ||
	bc > COVERAGE_EPSILON || ca > COVERAGE_EPSILON;
    return !(has_negative && has_positive);
}

static fastf_t
segment_distance_sq(const point2 &p, const point2 &a, const point2 &b,
	fastf_t *path_distance, fastf_t distance_before)
{
    const fastf_t dx = b.x - a.x;
    const fastf_t dy = b.y - a.y;
    const fastf_t length_sq = dx * dx + dy * dy;
    fastf_t t = 0.0;

    if (length_sq > COVERAGE_EPSILON)
	t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) /
	    length_sq, 0.0, 1.0);
    if (path_distance)
	*path_distance = distance_before + t * std::sqrt(length_sq);
    const point2 closest = {a.x + t * dx, a.y + t * dy};
    return distance_sq(p, closest);
}

static bool
pattern_on(uint32_t pattern, fastf_t distance, fastf_t width)
{
    const fastf_t unit = std::max(width, (fastf_t)COVERAGE_EPSILON);
    const auto interval_on = [distance, unit](const fastf_t *values,
	    size_t count) {
	fastf_t period = 0.0;
	for (size_t i = 0; i < count; ++i)
	    period += values[i] * unit;
	fastf_t cursor = std::fmod(std::max(distance, (fastf_t)0.0), period);
	for (size_t i = 0; i < count; ++i) {
	    const fastf_t interval = values[i] * unit;
	    if (cursor <= interval)
		return !(i & 1);
	    cursor -= interval;
	}
	return true;
    };

    switch (pattern) {
	case RT_ANNOT_LINE_DASHED: {
	    const fastf_t values[] = {8.0, 4.0};
	    return interval_on(values, 2);
	}
	case RT_ANNOT_LINE_DOTTED: {
	    const fastf_t values[] = {1.0, 3.0};
	    return interval_on(values, 2);
	}
	case RT_ANNOT_LINE_CENTER: {
	    const fastf_t values[] = {8.0, 3.0, 2.0, 3.0};
	    return interval_on(values, 4);
	}
	case RT_ANNOT_LINE_PHANTOM: {
	    const fastf_t values[] = {8.0, 3.0, 2.0, 3.0, 2.0, 3.0};
	    return interval_on(values, 6);
	}
	default:
	    return true;
    }
}

static bool
stroke_covers(const render_segment &segment, const point2 &point,
	fastf_t width, bool only_open_paths)
{
    const fastf_t radius_sq = 0.25 * width * width;

    for (const auto &path : segment.paths) {
	if (path.size() < 2)
	    continue;
	if (only_open_paths && path_is_closed(path))
	    continue;
	fastf_t distance_before = 0.0;
	for (size_t i = 1; i < path.size(); ++i) {
	    fastf_t path_distance = 0.0;
	    const fastf_t d_sq = segment_distance_sq(point, path[i - 1],
		path[i], &path_distance, distance_before);
	    if (d_sq <= radius_sq && pattern_on(segment.pattern,
		    path_distance, width))
		return true;
	    distance_before += std::sqrt(distance_sq(path[i - 1], path[i]));
	}
    }
    return false;
}

static bool
segment_covers(const render_segment &segment, const point2 &point,
	fastf_t base_width)
{
    if (segment.fill) {
	for (const auto &triangle : segment.triangles)
	    if (point_in_triangle(point, triangle))
		return true;
	return false;
    }

    const fastf_t width = std::max(base_width * segment.width_scale,
	(fastf_t)COVERAGE_EPSILON);
    if (!segment.text)
	return stroke_covers(segment, point, width, false);

    int winding = 0;
    for (const auto &path : segment.paths) {
	if (path_is_closed(path))
	    winding += polygon_winding(point, path);
    }
    return winding != 0 || stroke_covers(segment, point, width, true);
}

static bool
compatibility_outline(const struct rt_annot_internal *annot, size_t index)
{
    for (size_t i = 0; i < annot->ant.count; ++i) {
	if (!annot->ant.segments[i] ||
		*(uint32_t *)annot->ant.segments[i] != ANN_FSEG_MAGIC)
	    continue;
	const auto *fill = (const struct fill_seg *)annot->ant.segments[i];
	if (fill->legacy_start >= 0 && fill->legacy_count > 0) {
	    const size_t start = (size_t)fill->legacy_start;
	    const size_t end = start + (size_t)fill->legacy_count;
	    if (index >= start && index < end)
		return true;
	}
    }
    return false;
}

static render_segment
parse_segment(const struct rt_annot_internal *annot, size_t index,
	const struct bg_tess_tol *ttol,
	const std::array<unsigned char, 4> &inherited_color)
{
    render_segment result;
    struct bu_list vhead;
    struct bu_list vlfree;
    std::vector<point2> current_path;
    std::vector<point2> current_triangle;
    const uint32_t magic = *(uint32_t *)annot->ant.segments[index];

    result.index = index;
    result.text = magic == ANN_TSEG_MAGIC;
    result.fill = magic == ANN_FSEG_MAGIC;
    result.color = inherited_color;
    if (annot->styles) {
	const struct rt_annot_seg_style &style = annot->styles[index];
	result.role = style.role;
	result.pattern = style.line_pattern;
	if (style.flags & RT_ANNOT_STYLE_WIDTH)
	    result.width_scale = style.line_width;
	else if (result.text && (style.flags & RT_ANNOT_STYLE_BOLD))
	    result.width_scale = 2.0;
	if (style.flags & RT_ANNOT_STYLE_COLOR)
	    std::copy(style.color, style.color + 4, result.color.begin());
    }

    BU_LIST_INIT(&vhead);
    BU_LIST_INIT(&vlfree);
    if (rt_annot_segment_vlist(&vlfree, &vhead, ttol, annot, index)) {
	bv_vlist_cleanup(&vhead);
	bv_vlist_cleanup(&vlfree);
	return result;
    }

    struct bv_vlist *vp;
    for (BU_LIST_FOR(vp, bv_vlist, &vhead)) {
	for (size_t i = 0; i < vp->nused; ++i) {
	    const point2 point = {vp->pt[i][X], vp->pt[i][Y]};
	    switch (vp->cmd[i]) {
		case BV_VLIST_LINE_MOVE:
		    if (!current_path.empty())
			result.paths.push_back(std::move(current_path));
		    current_path.clear();
		    current_path.push_back(point);
		    break;
		case BV_VLIST_LINE_DRAW:
		    current_path.push_back(point);
		    break;
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_TRI_MOVE:
		    current_triangle.clear();
		    current_triangle.push_back(point);
		    break;
		case BV_VLIST_POLY_DRAW:
		case BV_VLIST_TRI_DRAW:
		    current_triangle.push_back(point);
		    break;
		case BV_VLIST_POLY_END:
		case BV_VLIST_TRI_END:
		    if (current_triangle.size() >= 3) {
			for (size_t j = 1; j + 1 < current_triangle.size(); ++j)
			    result.triangles.push_back({current_triangle[0],
				current_triangle[j], current_triangle[j + 1]});
		    }
		    current_triangle.clear();
		    break;
	    }
	}
    }
    if (!current_path.empty())
	result.paths.push_back(std::move(current_path));
    BV_FREE_VLIST(&vlfree, &vhead);
    bv_vlist_cleanup(&vlfree);
    return result;
}


static std::array<unsigned char, 4>
path_color(struct db_i *dbip, const char *path)
{
    std::array<unsigned char, 4> color = {255, 255, 255, 255};
    struct db_tree_state state = RT_DBTS_INIT_ZERO;
    struct db_full_path resolved_path;

    if (!dbip || !path)
	return color;
    db_init_db_tree_state(&state, dbip);
    db_full_path_init(&resolved_path);
    if (db_follow_path_for_state(&state, &resolved_path, path,
	    LOOKUP_QUIET) >= 0 && state.ts_mater.ma_color_valid) {
	for (size_t channel = 0; channel < 3; ++channel) {
	    const fastf_t component = std::clamp(
		(fastf_t)state.ts_mater.ma_color[channel],
		(fastf_t)0.0, (fastf_t)1.0);
	    color[channel] = (unsigned char)std::lround(component * 255.0);
	}
    }
    db_free_full_path(&resolved_path);
    db_free_db_tree_state(&state);
    return color;
}

static fastf_t
automatic_width(const struct rt_annot_internal *annot)
{
    fastf_t largest_text = 0.0;

    for (size_t i = 0; i < annot->ant.count; ++i) {
	if (annot->ant.segments[i] &&
		*(uint32_t *)annot->ant.segments[i] == ANN_TSEG_MAGIC) {
	    const auto *text = (const struct txt_seg *)annot->ant.segments[i];
	    largest_text = std::max(largest_text, text->txt_size);
	}
    }
    if (largest_text > 0.0)
	return largest_text * TEXT_STROKE_RATIO;

    if (annot->vert_count) {
	point2 minimum = {MAX_FASTF, MAX_FASTF};
	point2 maximum = {-MAX_FASTF, -MAX_FASTF};
	for (size_t i = 0; i < annot->vert_count; ++i) {
	    minimum.x = std::min(minimum.x, annot->verts[i][X]);
	    minimum.y = std::min(minimum.y, annot->verts[i][Y]);
	    maximum.x = std::max(maximum.x, annot->verts[i][X]);
	    maximum.y = std::max(maximum.y, annot->verts[i][Y]);
	}
	const fastf_t diagonal = std::sqrt(distance_sq(minimum, maximum));
	if (diagonal > 0.0)
	    return diagonal * GEOMETRY_STROKE_RATIO;
    }
    return 1.0;
}

static bool
local_bounds_contain(const render_instance &instance, const point2 &point)
{
    if (point.x < instance.local_minimum.x - COVERAGE_EPSILON ||
	    point.x > instance.local_maximum.x + COVERAGE_EPSILON ||
	    point.y < instance.local_minimum.y - COVERAGE_EPSILON ||
	    point.y > instance.local_maximum.y + COVERAGE_EPSILON)
	return false;
    return true;
}

static void
set_segment_hit(const render_instance &instance,
	const render_segment &segment, struct rt_annot_hit *hit)
{
    hit->path = instance.path.c_str();
    hit->segment = segment.index;
    hit->role = segment.role;
    std::copy(segment.color.begin(), segment.color.end(), hit->color);
}

struct local_hit_context {
    const render_instance *instance = nullptr;
    const point2 *point = nullptr;
    size_t matched = 0;
    bool have_match = false;
};

static bool
test_local_hit(const size_t &index, void *context)
{
    auto *state = static_cast<local_hit_context *>(context);
    const render_segment &segment = state->instance->segments[index];

    if (segment.color[3] && segment_covers(segment, *state->point,
	    state->instance->base_width) &&
	    (!state->have_match || index > state->matched)) {
	state->matched = index;
	state->have_match = true;
    }
    return true;
}

static bool
local_hit(const render_instance &instance, const point2 &point,
	struct rt_annot_hit *hit)
{
    local_hit_context state;

    if (!instance.segment_index || !local_bounds_contain(instance, point))
	return false;
    state.instance = &instance;
    state.point = &point;
    const fastf_t query[2] = {point.x, point.y};
    (void)instance.segment_index->Search(query, query, test_local_hit, &state);
    if (!state.have_match)
	return false;
    if (hit)
	set_segment_hit(instance, instance.segments[state.matched], hit);
    return true;
}

struct append_layers_context {
    const render_instance *instance = nullptr;
    const point2 *point = nullptr;
    const fastf_t *model_point = nullptr;
    fastf_t distance = 0.0;
    int screen_space = 0;
    fastf_t screen_depth = 0.0;
    std::vector<coverage_layer> *layers = nullptr;
};

static bool
append_local_layer(const size_t &index, void *context)
{
    auto *state = static_cast<append_layers_context *>(context);
    const render_segment &segment = state->instance->segments[index];

    if (!segment.color[3] || !segment_covers(segment, *state->point,
	    state->instance->base_width))
	return true;
    coverage_layer layer;
    set_segment_hit(*state->instance, segment, &layer.hit);
    VMOVE(layer.hit.point, state->model_point);
    layer.hit.distance = state->distance;
    layer.hit.screen_space = state->screen_space;
    layer.hit.visible = 1;
    layer.screen_depth = state->screen_depth;
    layer.draw_order = index;
    state->layers->push_back(layer);
    return true;
}

static void
append_local_layers(const render_instance &instance, const point2 &point,
	const point_t model_point, fastf_t distance, int screen_space,
	fastf_t screen_depth, std::vector<coverage_layer> &layers)
{
    append_layers_context state;

    if (!instance.segment_index || !local_bounds_contain(instance, point))
	return;
    state.instance = &instance;
    state.point = &point;
    state.model_point = model_point;
    state.distance = distance;
    state.screen_space = screen_space;
    state.screen_depth = screen_depth;
    state.layers = &layers;
    const fastf_t query[2] = {point.x, point.y};
    (void)instance.segment_index->Search(query, query, append_local_layer,
	&state);
}

static bool
model_sample(const render_instance &instance, const struct xray *ray,
	point2 *local, point_t model_point, fastf_t *ray_distance)
{
    vect_t normal;
    vect_t offset;
    vect_t from_anchor;
    fastf_t uu, uv, vv, du, dv, determinant, denominator, distance;
    fastf_t normal_magnitude, ray_magnitude;

    VCROSS(normal, instance.u, instance.v);
    denominator = VDOT(normal, ray->r_dir);
    normal_magnitude = MAGNITUDE(normal);
    ray_magnitude = MAGNITUDE(ray->r_dir);
    if (normal_magnitude <= 0.0 || ray_magnitude <= 0.0 ||
	    std::fabs(denominator) <= RELATIVE_EPSILON *
	    normal_magnitude * ray_magnitude)
	return false;
    VSUB2(offset, instance.anchor, ray->r_pt);
    distance = VDOT(offset, normal) / denominator;
    if (distance < 0.0)
	return false;
    VJOIN1(model_point, ray->r_pt, distance, ray->r_dir);
    VSUB2(from_anchor, model_point, instance.anchor);
    uu = VDOT(instance.u, instance.u);
    uv = VDOT(instance.u, instance.v);
    vv = VDOT(instance.v, instance.v);
    du = VDOT(from_anchor, instance.u);
    dv = VDOT(from_anchor, instance.v);
    determinant = uu * vv - uv * uv;
    if (determinant <= RELATIVE_EPSILON * uu * vv)
	return false;
    local->x = (du * vv - dv * uv) / determinant;
    local->y = (dv * uu - du * uv) / determinant;
    *ray_distance = distance;
    return true;
}

static bool
model_hit(const render_instance &instance, const struct xray *ray,
	fastf_t scene_distance, fastf_t model_tolerance,
	struct rt_annot_hit *hit)
{
    point2 local;
    point_t point;
    fastf_t distance;

    if (!model_sample(instance, ray, &local, point, &distance))
	return false;
    if (!local_hit(instance, local, hit))
	return false;
    if (hit) {
	VMOVE(hit->point, point);
	hit->distance = distance;
	hit->screen_space = 0;
	hit->visible = !std::isfinite(scene_distance) ||
	    distance <= scene_distance + model_tolerance;
    }
    return true;
}

static bool
screen_sample(const render_instance &instance, const struct rt_annot_view *view,
	fastf_t sample_x, fastf_t sample_y, point2 *local,
	fastf_t *anchor_depth)
{
    point_t projected;
    fastf_t view_x, view_y;

    if (!view || !view->width || !view->height)
	return false;
    MAT4X3PNT(projected, view->model2view, instance.anchor);
    view_x = projected[X];
    view_y = projected[Y];
    if (view->perspective > 0.0) {
	if (projected[Z] >= -COVERAGE_EPSILON)
	    return false;
	const fastf_t zoom = 1.0 /
	    std::tan(DEG2RAD * view->perspective * 0.5);
	const fastf_t factor = -zoom / projected[Z];
	view_x *= factor;
	view_y *= factor;
    }
    const fastf_t anchor_x = (view_x + 1.0) * (fastf_t)view->width * 0.5;
    const fastf_t anchor_y = view_y * (fastf_t)view->width * 0.5 +
	(fastf_t)view->height * 0.5;
    local->x = (sample_x - anchor_x) / RT_ANNOT_SCREEN_PIXELS_PER_MM;
    local->y = (sample_y - anchor_y) / RT_ANNOT_SCREEN_PIXELS_PER_MM;
    if (anchor_depth)
	*anchor_depth = projected[Z];
    return true;
}

struct render_scene {
    std::vector<render_instance> instances;
    instance_tree model_index;
    std::vector<size_t> screen_instances;
    fastf_t model_tolerance = BN_TOL_DIST;
};

static void
instance_bounds(const render_instance &instance, point_t minimum,
	point_t maximum)
{
    bool have_bounds = false;

    for (int x_index = 0; x_index < 2; ++x_index) {
	const fastf_t x = x_index ? instance.local_maximum.x :
	    instance.local_minimum.x;
	for (int y_index = 0; y_index < 2; ++y_index) {
	    const fastf_t y = y_index ? instance.local_maximum.y :
		instance.local_minimum.y;
	    point_t point;
	    VJOIN2(point, instance.anchor, x, instance.u, y, instance.v);
	    if (!have_bounds) {
		VMOVE(minimum, point);
		VMOVE(maximum, point);
		have_bounds = true;
	    } else {
		VMINMAX(minimum, maximum, point);
	    }
	}
    }
}

static void
prepare_scene_indices(render_scene &scene)
{
    const fastf_t padding = std::max(scene.model_tolerance,
	(fastf_t)COVERAGE_EPSILON);

    for (size_t i = 0; i < scene.instances.size(); ++i) {
	render_instance &instance = scene.instances[i];
	prepare_local_index(instance);
	if (instance.screen_space) {
	    /* Its world-space extent changes with the view; only its local
	     * segment coverage has view-independent bounds. */
	    scene.screen_instances.push_back(i);
	    continue;
	}
	point_t minimum, maximum;
	instance_bounds(instance, minimum, maximum);
	for (size_t axis = 0; axis < 3; ++axis) {
	    minimum[axis] -= padding;
	    maximum[axis] += padding;
	}
	scene.model_index.Insert(minimum, maximum, i);
    }
}

static std::set<size_t>
model_candidates(const render_scene &scene, const struct xray *ray)
{
    std::set<size_t> candidates;
    instance_tree::Ray index_ray;
    vect_t inverse_direction;

    bg_ray_invdir(&inverse_direction, ray->r_dir);
    VMOVE(index_ray.o, ray->r_pt);
    VMOVE(index_ray.d, ray->r_dir);
    VMOVE(index_ray.di, inverse_direction);
    (void)scene.model_index.Intersects(&index_ray, &candidates);
    return candidates;
}

static std::vector<coverage_layer>
coverage_layers(const render_scene &scene, const struct rt_annot_view *view,
	const struct xray *ray, fastf_t sample_x, fastf_t sample_y,
	fastf_t scene_distance)
{
    std::vector<coverage_layer> layers;

    for (const size_t index : model_candidates(scene, ray)) {
	const render_instance &instance = scene.instances[index];
	point2 local;
	point_t point;
	fastf_t distance;
	if (model_sample(instance, ray, &local, point, &distance) &&
		(!std::isfinite(scene_distance) ||
		distance <= scene_distance + scene.model_tolerance))
	    append_local_layers(instance, local, point, distance, 0, 0.0,
		layers);
    }
    for (const size_t index : scene.screen_instances) {
	const render_instance &instance = scene.instances[index];
	point2 local;
	fastf_t anchor_depth;
	if (screen_sample(instance, view, sample_x, sample_y, &local,
		&anchor_depth))
	    append_local_layers(instance, local, instance.anchor, 0.0, 1,
		anchor_depth, layers);
    }
    std::stable_sort(layers.begin(), layers.end(),
	[](const coverage_layer &a, const coverage_layer &b) {
	    if (a.hit.screen_space != b.hit.screen_space)
		return a.hit.screen_space < b.hit.screen_space;
	    const fastf_t a_depth = a.hit.screen_space ? a.screen_depth :
		a.hit.distance;
	    const fastf_t b_depth = b.hit.screen_space ? b.screen_depth :
		b.hit.distance;
	    if (a_depth < b_depth)
		return a.hit.screen_space != 0;
	    if (a_depth > b_depth)
		return a.hit.screen_space == 0;
	    const int path_order = bu_strcmp(a.hit.path, b.hit.path);
	    if (path_order)
		return path_order > 0;
	    return a.draw_order < b.draw_order;
	});
    return layers;
}

} // namespace

struct rt_annot_scene {
    void *implementation = nullptr;
};

namespace {

static render_scene *
scene_impl(struct rt_annot_scene *scene)
{
    return scene ? static_cast<render_scene *>(scene->implementation) : nullptr;
}

static const render_scene *
scene_impl(const struct rt_annot_scene *scene)
{
    return scene ? static_cast<const render_scene *>(scene->implementation) : nullptr;
}

} // namespace

extern "C" struct rt_annot_scene *
rt_annot_scene_create(struct db_i *dbip, int path_count,
	const char * const *paths, const struct bg_tess_tol *ttol,
	const struct bn_tol *tol)
{
    struct bg_tess_tol default_ttol = BG_TESS_TOL_INIT_ZERO;
    struct bu_ptbl results = BU_PTBL_INIT_ZERO;
    std::vector<struct directory *> roots;
    auto *scene = new rt_annot_scene;
    render_scene *implementation = new render_scene;

    scene->implementation = implementation;

    if (tol && tol->magic == BN_TOL_MAGIC && std::isfinite(tol->dist) &&
	    tol->dist >= 0.0)
	implementation->model_tolerance = tol->dist;

    if (!dbip || path_count < 0 || (path_count && !paths))
	return scene;
    if (!ttol) {
	default_ttol.magic = BG_TESS_TOL_MAGIC;
	default_ttol.rel = 0.01;
	ttol = &default_ttol;
    }
    for (int i = 0; i < path_count; ++i) {
	struct directory *dp = db_lookup(dbip, paths[i], LOOKUP_QUIET);
	if (dp)
	    roots.push_back(dp);
    }
    if (roots.empty())
	return scene;

    const int found = db_search(&results, DB_SEARCH_QUIET, "-type annot",
	(int)roots.size(), roots.data(), dbip, NULL, NULL, NULL);
    if (found <= 0) {
	db_search_free(&results);
	return scene;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&results); ++i) {
	auto *path = (struct db_full_path *)BU_PTBL_GET(&results, i);
	struct rt_db_internal intern;
	mat_t matrix = MAT_INIT_IDN;
	char *path_string;
	int internal_type;
	if (!path || !path->fp_len ||
		!db_path_to_mat(dbip, path, matrix, (int)path->fp_len - 1))
	    continue;
	RT_DB_INTERNAL_INIT(&intern);
	internal_type = rt_db_get_internal(&intern, DB_FULL_PATH_CUR_DIR(path),
	    dbip, matrix);
	if (internal_type != ID_ANNOT) {
	    if (internal_type >= 0)
		rt_db_free_internal(&intern);
	    continue;
	}
	const auto *annot = (const struct rt_annot_internal *)intern.idb_ptr;
	if (rt_annot_validate(annot, NULL)) {
	    rt_db_free_internal(&intern);
	    continue;
	}
	render_instance instance;
	path_string = db_path_to_string(path);
	instance.path = path_string ? path_string : "";
	const std::array<unsigned char, 4> inherited_color =
	    path_color(dbip, instance.path.c_str());
	if (path_string)
	    bu_free(path_string, "annotation path");
	instance.screen_space = !(annot->flags & RT_ANNOT_MODEL_SPACE);
	VMOVE(instance.anchor, annot->V);
	VMOVE(instance.u, annot->u_vec);
	VMOVE(instance.v, annot->v_vec);
	instance.base_width = automatic_width(annot);
	if (instance.screen_space) {
	    instance.base_width = 1.0 / RT_ANNOT_SCREEN_PIXELS_PER_MM;
	}
	/* Background fills are deliberately evaluated before stored strokes. */
	for (int fill_pass = 1; fill_pass >= 0; --fill_pass) {
	    for (size_t segment = 0; segment < annot->ant.count; ++segment) {
		const bool is_fill = annot->ant.segments[segment] &&
		    *(uint32_t *)annot->ant.segments[segment] == ANN_FSEG_MAGIC;
		if ((int)is_fill != fill_pass ||
			(!is_fill && compatibility_outline(annot, segment)))
		    continue;
		instance.segments.push_back(parse_segment(annot, segment, ttol,
		    inherited_color));
	    }
	}
	implementation->instances.push_back(std::move(instance));
	rt_db_free_internal(&intern);
    }
    db_search_free(&results);
    std::sort(implementation->instances.begin(), implementation->instances.end(),
	[](const render_instance &a, const render_instance &b) {
	    return a.path < b.path;
	});
    implementation->instances.erase(std::unique(implementation->instances.begin(),
	implementation->instances.end(), [](const render_instance &a,
	    const render_instance &b) { return a.path == b.path; }),
	implementation->instances.end());
    prepare_scene_indices(*implementation);
    return scene;
}

extern "C" void
rt_annot_scene_destroy(struct rt_annot_scene *scene)
{
    delete scene_impl(scene);
    delete scene;
}

extern "C" size_t
rt_annot_scene_count(const struct rt_annot_scene *scene)
{
    const render_scene *implementation = scene_impl(scene);

    return implementation ? implementation->instances.size() : 0;
}

extern "C" int
rt_annot_scene_bounds(const struct rt_annot_scene *scene, point_t minimum,
	point_t maximum)
{
    bool have_bounds = false;
    const render_scene *implementation = scene_impl(scene);

    if (!implementation || !minimum || !maximum)
	return 0;
    for (const render_instance &instance : implementation->instances) {
	if (instance.screen_space) {
	    if (!have_bounds) {
		VMOVE(minimum, instance.anchor);
		VMOVE(maximum, instance.anchor);
		have_bounds = true;
	    } else {
		VMINMAX(minimum, maximum, instance.anchor);
	    }
	    continue;
	}
	point_t instance_minimum, instance_maximum;
	instance_bounds(instance, instance_minimum, instance_maximum);
	if (!have_bounds) {
	    VMOVE(minimum, instance_minimum);
	    VMOVE(maximum, instance_maximum);
	    have_bounds = true;
	} else {
	    VMINMAX(minimum, maximum, instance_minimum);
	    VMINMAX(minimum, maximum, instance_maximum);
	}
    }
    return have_bounds ? 1 : 0;
}

extern "C" size_t
rt_annot_scene_query_model(const struct rt_annot_scene *scene,
	const struct xray *ray, fastf_t scene_distance,
	struct rt_annot_hit *hits, size_t capacity)
{
    std::vector<struct rt_annot_hit> found;
    const render_scene *implementation = scene_impl(scene);

    if (!implementation || !ray)
	return 0;
    for (const size_t index : model_candidates(*implementation, ray)) {
	const render_instance &instance = implementation->instances[index];
	struct rt_annot_hit hit = {};
	if (model_hit(instance, ray,
		scene_distance, implementation->model_tolerance, &hit))
	    found.push_back(hit);
    }
    std::sort(found.begin(), found.end(), [](const struct rt_annot_hit &a,
	const struct rt_annot_hit &b) {
	if (a.distance < b.distance)
	    return true;
	if (a.distance > b.distance)
	    return false;
	return bu_strcmp(a.path, b.path) < 0;
    });
    if (hits && capacity) {
	const size_t count = std::min(capacity, found.size());
	std::copy_n(found.begin(), count, hits);
    }
    return found.size();
}

extern "C" int
rt_annot_scene_query(const struct rt_annot_scene *scene,
	const struct rt_annot_view *view, const struct xray *ray,
	fastf_t sample_x, fastf_t sample_y, fastf_t scene_distance,
	struct rt_annot_hit *hit)
{
    const render_scene *implementation = scene_impl(scene);

    if (!implementation || !ray || !hit)
	return 0;
    const std::vector<coverage_layer> layers = coverage_layers(*implementation,
	view, ray, sample_x, sample_y, scene_distance);
    if (layers.empty())
	return 0;
    *hit = layers.back().hit;
    return 1;
}

extern "C" size_t
rt_annot_scene_query_layers(const struct rt_annot_scene *scene,
	const struct rt_annot_view *view, const struct xray *ray,
	fastf_t sample_x, fastf_t sample_y, fastf_t scene_distance,
	struct rt_annot_hit *hits, size_t capacity)
{
    const render_scene *implementation = scene_impl(scene);

    if (!implementation || !ray)
	return 0;
    const std::vector<coverage_layer> layers = coverage_layers(*implementation,
	view, ray, sample_x, sample_y, scene_distance);
    if (hits && capacity) {
	const size_t count = std::min(capacity, layers.size());
	for (size_t i = 0; i < count; ++i)
	    hits[i] = layers[i].hit;
    }
    return layers.size();
}

extern "C" void
rt_annot_hit_blend(fastf_t color[3], const struct rt_annot_hit *hit)
{
    if (!color || !hit)
	return;
    const fastf_t alpha = (fastf_t)hit->color[3] / 255.0;
    for (size_t i = 0; i < 3; ++i)
	color[i] = alpha * ((fastf_t)hit->color[i] / 255.0) +
	    (1.0 - alpha) * color[i];
}

extern "C" int
rt_annot_scene_composite(const struct rt_annot_scene *scene,
	const struct rt_annot_view *view, const struct xray *ray,
	fastf_t sample_x, fastf_t sample_y, fastf_t scene_distance,
	fastf_t color[3], struct rt_annot_hit *front_hit)
{
    const render_scene *implementation = scene_impl(scene);

    if (!implementation || !ray || !color)
	return 0;
    const std::vector<coverage_layer> layers = coverage_layers(*implementation,
	view, ray, sample_x, sample_y, scene_distance);
    for (const coverage_layer &layer : layers)
	rt_annot_hit_blend(color, &layer.hit);
    if (front_hit && !layers.empty())
	*front_hit = layers.back().hit;
    return layers.empty() ? 0 : 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
