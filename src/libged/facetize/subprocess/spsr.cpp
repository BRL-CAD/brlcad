/*                      S P S R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY IMPLIED WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/facetize/subprocess/spsr.cpp
 *
 * Adaptive Screened Poisson reconstruction backed by exact source rays.
 *
 * A fixed, arbitrarily sized point cloud can miss thin partitions and spend
 * most samples on already well-resolved surfaces.  The initial sample set
 * therefore comes from deterministic Crofton rays whose area estimate has
 * stabilized; paired entry/exit hits retain normals and local thickness.
 * Each candidate mesh is then compared with the original object, and source
 * rays targeted at reported error locations supply the next solve.
 *
 * Sellan, Ren, Batty, and Stein, "Reach for the Arcs: Reconstructing Surfaces
 * from SDFs via Tangent Points", SIGGRAPH 2024, motivates preserving more
 * local tangency information than an unstructured point dump.  This is not
 * their SDF algorithm; it applies the same lesson to BRL-CAD's exact hits and
 * partitions.  The upstream Poisson solver remains unchanged.
 */

#include "common.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "vmath.h"
#include "bu/datetime.h"
#include "bg/spsr.h"
#include "bg/trimesh.h"
#include "rt/func.h"
#include "../../ged_private.h"
#include "../validation.h"
#include "./tessellate.h"

namespace {

constexpr size_t SPSR_VALIDATION_RAYS = 16384;
constexpr size_t SPSR_MAX_HINTS_PER_PASS = 64;
constexpr size_t SPSR_MAX_REFINEMENT_SAMPLES = 20000;
constexpr fastf_t SPSR_TANGENT_DOT_LIMIT = 0.05;
constexpr fastf_t SPSR_PARTITION_ERROR_LIMIT = 0.01;
constexpr fastf_t SPSR_CHORD_ERROR_LIMIT = 0.01;
constexpr fastf_t SPSR_BEST_EFFORT_CHORD_ERROR_LIMIT = 0.02;
constexpr fastf_t SPSR_BEST_EFFORT_THIN_ERROR_LIMIT = 0.10;
constexpr fastf_t SPSR_ESTIMATE_ERROR_LIMIT = 0.05;
constexpr fastf_t SPSR_ENDPOINT_P95_SCALE = 1.0;
constexpr fastf_t SPSR_ENDPOINT_P99_SCALE = 2.0;
constexpr fastf_t SPSR_THIN_PARTITION_SCALE = 4.0;
constexpr fastf_t SPSR_HINT_STENCIL_SCALE = 0.5;
constexpr fastf_t SPSR_DEFAULT_FEATURE_FRACTION = 0.01;
constexpr fastf_t SPSR_DEFAULT_DECIMATION_SCALE = 1.5;
constexpr fastf_t SPSR_MIN_RELATIVE_IMPROVEMENT = 0.01;
constexpr size_t SPSR_STAGNANT_PASS_LIMIT = 2;
const char *SPSR_CANDIDATE_OBJECT = "facetize_spsr_candidate";

struct validation_limits {
    fastf_t chord_error;
    fastf_t thin_error;
};

constexpr validation_limits SPSR_STRICT_LIMITS = {
    SPSR_CHORD_ERROR_LIMIT, SPSR_PARTITION_ERROR_LIMIT
};

/* Open edges make thin source partitions particularly unstable.  The relaxed
 * limits still require close global area, volume, and endpoint agreement. */
constexpr validation_limits SPSR_BEST_EFFORT_LIMITS = {
    SPSR_BEST_EFFORT_CHORD_ERROR_LIMIT, SPSR_BEST_EFFORT_THIN_ERROR_LIMIT
};

using segment_list =
    std::vector<const struct rt_crofton_segment *>;
using segment_map = std::map<size_t, segment_list>;

struct spsr_context {
    struct rt_i *source = NULL;
    point_t bounds_min;
    point_t bounds_max;
    fastf_t feature_size = 0.0;
    size_t ray_offset = 0;
    uint64_t next_pair_id = 1;
    bool best_effort_source = false;
    bool best_effort_accepted = false;
    bool have_validation_score = false;
    bool validation_stagnated = false;
    fastf_t best_validation_score = 0.0;
    size_t stagnant_validation_passes = 0;
    std::vector<struct bg_3d_spsr_sample> refinement_samples;
};

static struct rt_bot_internal *
new_bot(int *faces, size_t face_count, point_t *vertices,
    size_t vertex_count)
{
    struct rt_bot_internal *bot;
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->num_faces = face_count;
    bot->num_vertices = vertex_count;
    bot->faces = faces;
    bot->vertices = reinterpret_cast<fastf_t *>(vertices);
    return bot;
}

static struct rt_bot_internal *
clone_bot(const struct rt_bot_internal *source)
{
    if (!source || !source->faces || !source->vertices)
	return NULL;

    int *faces = static_cast<int *>(bu_malloc(
	3 * source->num_faces * sizeof(int), "SPSR copied faces"));
    point_t *vertices = static_cast<point_t *>(bu_malloc(
	source->num_vertices * sizeof(point_t), "SPSR copied vertices"));
    memcpy(faces, source->faces, 3 * source->num_faces * sizeof(int));
    memcpy(vertices, source->vertices,
	source->num_vertices * sizeof(point_t));
    return new_bot(faces, source->num_faces, vertices,
	source->num_vertices);
}

static bool
mesh_is_valid(struct rt_bot_internal *bot)
{
    return bot && bot->num_vertices && bot->num_faces &&
	bot->num_vertices <= static_cast<size_t>(INT_MAX) &&
	bot->num_faces <= static_cast<size_t>(INT_MAX) &&
	!bg_trimesh_solid2(static_cast<int>(bot->num_vertices),
	    static_cast<int>(bot->num_faces), bot->vertices, bot->faces,
	    NULL) && bot_is_manifold(bot);
}

static bool
prepared_bounds(point_t minimum, point_t maximum, struct rt_i *rtip)
{
    VSETALL(minimum, MAX_FASTF);
    VSETALL(maximum, -MAX_FASTF);
    struct soltab *solid;
    RT_VISIT_ALL_SOLTABS_START(solid, rtip) {
	VMIN(minimum, solid->st_min);
	VMAX(maximum, solid->st_max);
    } RT_VISIT_ALL_SOLTABS_END;
    return minimum[X] < MAX_FASTF &&
	minimum[X] < maximum[X] && minimum[Y] < maximum[Y] &&
	minimum[Z] < maximum[Z];
}

static struct rt_i *
prepare_object(struct db_i *dbip, const char *object)
{
    struct rt_i *rtip = rt_i_create(dbip);
    if (!rtip)
	return NULL;

    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
    facetize_log_hooks_silence(&saved_hooks);
    int status = rt_gettree(rtip, object);
    if (!status)
	rt_prep_parallel(rtip, 1);
    facetize_log_hooks_restore(&saved_hooks);

    if (status || !rtip->stats.nsolids || !rtip->stats.nregions) {
	rt_i_destroy(rtip);
	return NULL;
    }
    return rtip;
}

static void
append_segment_samples(std::vector<struct bg_3d_spsr_sample> &samples,
    const struct rt_crofton_segment &segment, uint64_t &next_pair_id,
    size_t limit)
{
    if (limit && samples.size() + 2 > limit)
	return;

    struct bg_3d_spsr_sample entry = {};
    struct bg_3d_spsr_sample exit = {};
    VMOVE(entry.point, segment.in_point);
    VMOVE(entry.normal, segment.in_normal);
    entry.thickness = segment.thickness;
    entry.pair_id = next_pair_id;
    VMOVE(exit.point, segment.out_point);
    VMOVE(exit.normal, segment.out_normal);
    exit.thickness = segment.thickness;
    exit.pair_id = next_pair_id;
    next_pair_id++;
    samples.push_back(entry);
    samples.push_back(exit);
}

static fastf_t
average_thickness(const struct rt_crofton_result &result)
{
    if (!result.segment_count)
	return 0.0;

    double total = 0.0;
    for (size_t i = 0; i < result.segment_count; i++)
	total += result.segments[i].thickness;
    return total / static_cast<double>(result.segment_count);
}

static fastf_t
relative_error(double reference, double candidate)
{
    if (reference <= SMALL_FASTF)
	return candidate <= SMALL_FASTF ? 0.0 :
	    std::numeric_limits<fastf_t>::infinity();
    return std::fabs(candidate - reference) / reference;
}

static fastf_t
mismatch_fraction(size_t mismatches, size_t count, fastf_t empty_value)
{
    if (!count)
	return empty_value;
    return static_cast<fastf_t>(mismatches) / static_cast<fastf_t>(count);
}

static bool
validation_passes(const struct bg_3d_spsr_validation &validation,
    fastf_t feature_size, const validation_limits &limits)
{
    const fastf_t partition_error = mismatch_fraction(
	validation.partition_mismatch_rays, validation.ray_count, 1.0);
    const fastf_t thin_error = mismatch_fraction(
	validation.thin_partition_mismatches,
	validation.thin_partition_count, 0.0);

    return partition_error <= SPSR_PARTITION_ERROR_LIMIT &&
	validation.endpoint_error_p95 <=
	    SPSR_ENDPOINT_P95_SCALE * feature_size &&
	validation.endpoint_error_p99 <=
	    SPSR_ENDPOINT_P99_SCALE * feature_size &&
	validation.chord_error_fraction <= limits.chord_error &&
	thin_error <= limits.thin_error &&
	validation.surface_area_error_fraction <= SPSR_ESTIMATE_ERROR_LIMIT &&
	validation.volume_error_fraction <= SPSR_ESTIMATE_ERROR_LIMIT;
}

static fastf_t
validation_score(const struct bg_3d_spsr_validation &validation,
    fastf_t feature_size, const validation_limits &limits)
{
    const fastf_t partition_error = mismatch_fraction(
	validation.partition_mismatch_rays, validation.ray_count,
	std::numeric_limits<fastf_t>::infinity());
    const fastf_t thin_error = mismatch_fraction(
	validation.thin_partition_mismatches,
	validation.thin_partition_count, 0.0);

    return std::max({
	partition_error / SPSR_PARTITION_ERROR_LIMIT,
	validation.endpoint_error_p95 /
	    (SPSR_ENDPOINT_P95_SCALE * feature_size),
	validation.endpoint_error_p99 /
	    (SPSR_ENDPOINT_P99_SCALE * feature_size),
	validation.chord_error_fraction / limits.chord_error,
	thin_error / limits.thin_error,
	validation.surface_area_error_fraction / SPSR_ESTIMATE_ERROR_LIMIT,
	validation.volume_error_fraction / SPSR_ESTIMATE_ERROR_LIMIT
    });
}

static fastf_t
percentile(std::vector<fastf_t> &values, fastf_t fraction)
{
    if (values.empty())
	return 0.0;
    std::sort(values.begin(), values.end());
    size_t index = static_cast<size_t>(
	std::ceil(fraction * static_cast<fastf_t>(values.size())));
    index = index ? index - 1 : 0;
    return values[std::min(index, values.size() - 1)];
}

static void
map_segments(segment_map &mapped, const struct rt_crofton_result &result)
{
    for (size_t i = 0; i < result.segment_count; i++)
	mapped[result.segments[i].ray_id].push_back(&result.segments[i]);
}

static fastf_t
segment_position(const struct rt_crofton_segment *segment,
    const vect_t direction)
{
    point_t middle;
    VADD2SCALE(middle, segment->in_point, segment->out_point, 0.5);
    return VDOT(middle, direction);
}

static void
sort_segments(segment_list &segments, const vect_t direction)
{
    std::sort(segments.begin(), segments.end(),
	[&direction](const struct rt_crofton_segment *left,
	    const struct rt_crofton_segment *right) {
	    return segment_position(left, direction) <
		segment_position(right, direction);
	});
}

static fastf_t
interval_disagreement(const segment_list &source,
    const segment_list &candidate, const vect_t direction,
    double &source_length)
{
    std::vector<std::pair<fastf_t, fastf_t>> source_intervals;
    std::vector<std::pair<fastf_t, fastf_t>> candidate_intervals;
    double ray_source_length = 0.0;
    for (const auto *segment : source) {
	fastf_t entry = VDOT(segment->in_point, direction);
	fastf_t exit = VDOT(segment->out_point, direction);
	if (entry > exit)
	    std::swap(entry, exit);
	source_intervals.emplace_back(entry, exit);
	ray_source_length += exit - entry;
    }
    double candidate_length = 0.0;
    for (const auto *segment : candidate) {
	fastf_t entry = VDOT(segment->in_point, direction);
	fastf_t exit = VDOT(segment->out_point, direction);
	if (entry > exit)
	    std::swap(entry, exit);
	candidate_intervals.emplace_back(entry, exit);
	candidate_length += exit - entry;
    }

    double intersection = 0.0;
    for (const auto &source_interval : source_intervals) {
	for (const auto &candidate_interval : candidate_intervals) {
	    fastf_t entry = std::max(source_interval.first,
		candidate_interval.first);
	    fastf_t exit = std::min(source_interval.second,
		candidate_interval.second);
	    if (exit > entry)
		intersection += exit - entry;
	}
    }
    source_length += ray_source_length;
    return ray_source_length + candidate_length - 2.0 * intersection;
}

static struct rt_i *
prepare_candidate(struct db_i **candidate_db,
    const struct rt_bot_internal *bot)
{
    *candidate_db = db_create_inmem();
    if (!*candidate_db)
	return NULL;

    struct rt_bot_internal *copy = clone_bot(bot);
    struct rt_wdb *writer = wdb_dbopen(*candidate_db,
	RT_WDB_TYPE_DB_INMEM);
    if (!copy || !writer) {
	if (copy)
	    _tess_facetize_free_bot(copy);
	return NULL;
    }

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_minor_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    internal.idb_ptr = copy;
    if (wdb_put_internal(writer, SPSR_CANDIDATE_OBJECT, &internal,
	    1.0) < 0)
	return NULL;
    return prepare_object(*candidate_db, SPSR_CANDIDATE_OBJECT);
}

static bool
evaluate_candidate(spsr_context &context, struct rt_bot_internal *bot,
    struct bg_3d_spsr_validation &validation,
    std::vector<struct bg_3d_spsr_sample> *new_samples)
{
    validation = {};
    struct rt_crofton_params parameters = {};
    parameters.n_rays = SPSR_VALIDATION_RAYS;

    struct rt_crofton_result source_result = RT_CROFTON_RESULT_INIT;
    int source_status = rt_crofton_collect(&source_result, context.source,
	&parameters, context.ray_offset, context.bounds_min,
	context.bounds_max);
    if (source_status <= 0) {
	rt_crofton_result_free(&source_result);
	return false;
    }

    const bool valid_mesh = mesh_is_valid(bot);
    struct db_i *candidate_db = NULL;
    struct rt_i *candidate_rtip = valid_mesh ?
	prepare_candidate(&candidate_db, bot) : NULL;
    struct rt_crofton_result candidate_result = RT_CROFTON_RESULT_INIT;
    int candidate_status = candidate_rtip ?
	rt_crofton_collect(&candidate_result, candidate_rtip, &parameters,
	    context.ray_offset, context.bounds_min, context.bounds_max) : -1;
    if (candidate_rtip)
	rt_i_destroy(candidate_rtip);
    if (candidate_db)
	db_close(candidate_db);

    context.ray_offset += SPSR_VALIDATION_RAYS;
    validation.ray_count = source_result.ray_count;

    segment_map source_rays;
    segment_map candidate_rays;
    map_segments(source_rays, source_result);
    if (candidate_status >= 0)
	map_segments(candidate_rays, candidate_result);
    validation.source_hit_rays = source_rays.size();

    std::set<size_t> ray_ids;
    for (const auto &entry : source_rays)
	ray_ids.insert(entry.first);
    for (const auto &entry : candidate_rays)
	ray_ids.insert(entry.first);

    std::set<size_t> mismatch_rays;
    std::vector<fastf_t> endpoint_errors;
    double source_chord = 0.0;
    double chord_disagreement = 0.0;
    const fastf_t thin_limit =
	SPSR_THIN_PARTITION_SCALE * context.feature_size;

    for (size_t ray_id : ray_ids) {
	segment_list source = source_rays[ray_id];
	segment_list candidate = candidate_rays[ray_id];
	if (source.size() != candidate.size()) {
	    validation.partition_mismatch_rays++;
	    mismatch_rays.insert(ray_id);
	}

	vect_t direction;
	if (!source.empty())
	    VSUB2(direction, source.front()->out_point,
		source.front()->in_point);
	else if (!candidate.empty())
	    VSUB2(direction, candidate.front()->out_point,
		candidate.front()->in_point);
	else
	    continue;
	if (VNEAR_ZERO(direction, VUNITIZE_TOL))
	    continue;
	VUNITIZE(direction);
	sort_segments(source, direction);
	sort_segments(candidate, direction);
	chord_disagreement += interval_disagreement(source, candidate,
	    direction, source_chord);

	const size_t common = std::min(source.size(), candidate.size());
	for (size_t i = 0; i < source.size(); i++) {
	    const struct rt_crofton_segment *source_segment = source[i];
	    const bool thin = source_segment->thickness <= thin_limit;
	    if (thin)
		validation.thin_partition_count++;
	    if (i >= common) {
		if (thin)
		    validation.thin_partition_mismatches++;
		continue;
	    }

	    fastf_t entry_error = DIST_PNT_PNT(source_segment->in_point,
		candidate[i]->in_point);
	    fastf_t exit_error = DIST_PNT_PNT(source_segment->out_point,
		candidate[i]->out_point);
	    bool entry_nontangent =
		std::fabs(VDOT(source_segment->in_normal, direction)) >=
		SPSR_TANGENT_DOT_LIMIT;
	    bool exit_nontangent =
		std::fabs(VDOT(source_segment->out_normal, direction)) >=
		SPSR_TANGENT_DOT_LIMIT;
	    if (entry_nontangent)
		endpoint_errors.push_back(entry_error);
	    if (exit_nontangent)
		endpoint_errors.push_back(exit_error);
	    if (thin && (entry_error >
		    SPSR_ENDPOINT_P99_SCALE * context.feature_size ||
		    exit_error > SPSR_ENDPOINT_P99_SCALE *
		    context.feature_size))
		validation.thin_partition_mismatches++;
	}
    }

    validation.endpoint_error_p95 = percentile(endpoint_errors, 0.95);
    validation.endpoint_error_p99 = percentile(endpoint_errors, 0.99);
    validation.chord_error_fraction = source_chord > SMALL_FASTF ?
	chord_disagreement / source_chord :
	std::numeric_limits<fastf_t>::infinity();
    validation.surface_area_error_fraction = candidate_status >= 0 ?
	relative_error(source_result.surface_area,
	    candidate_result.surface_area) :
	std::numeric_limits<fastf_t>::infinity();
    validation.volume_error_fraction = candidate_status >= 0 ?
	relative_error(source_result.volume, candidate_result.volume) :
	std::numeric_limits<fastf_t>::infinity();

    const bool can_accept = valid_mesh && candidate_status >= 0;
    validation.passed = can_accept && validation_passes(validation,
	context.feature_size, SPSR_STRICT_LIMITS);
    if (!validation.passed && can_accept && context.best_effort_source &&
	validation_passes(validation, context.feature_size,
	    SPSR_BEST_EFFORT_LIMITS)) {
	validation.passed = 1;
	context.best_effort_accepted = true;
    }

    if (new_samples && !validation.passed) {
	for (size_t i = 0; i < source_result.segment_count; i++) {
	    const struct rt_crofton_segment &segment =
		source_result.segments[i];
	    if (mismatch_rays.find(segment.ray_id) != mismatch_rays.end() ||
		    segment.thickness <= thin_limit)
		append_segment_samples(*new_samples, segment,
		    context.next_pair_id, SPSR_MAX_REFINEMENT_SAMPLES);
	}
	if (new_samples->empty()) {
	    for (size_t i = 0; i < source_result.segment_count; i++)
		append_segment_samples(*new_samples,
		    source_result.segments[i], context.next_pair_id,
		    SPSR_MAX_REFINEMENT_SAMPLES);
	}
    }

    rt_crofton_result_free(&candidate_result);
    rt_crofton_result_free(&source_result);
    return true;
}

struct source_ray_data {
    spsr_context *context;
    std::vector<struct bg_3d_spsr_sample> *samples;
};

static int
source_ray_hit(struct application *application,
    struct partition *head, struct seg *UNUSED(segments))
{
    source_ray_data *data =
	static_cast<source_ray_data *>(application->a_uptr);
    for (struct partition *partition = head->pt_forw;
	 partition != head; partition = partition->pt_forw) {
	struct rt_crofton_segment segment = {};
	segment.thickness = partition->pt_outhit->hit_dist -
	    partition->pt_inhit->hit_dist;
	VJOIN1(segment.in_point, application->a_ray.r_pt,
	    partition->pt_inhit->hit_dist, application->a_ray.r_dir);
	VJOIN1(segment.out_point, application->a_ray.r_pt,
	    partition->pt_outhit->hit_dist, application->a_ray.r_dir);
	RT_HIT_NORMAL(segment.in_normal, partition->pt_inhit,
	    partition->pt_inseg->seg_stp, &application->a_ray,
	    partition->pt_inflip);
	RT_HIT_NORMAL(segment.out_normal, partition->pt_outhit,
	    partition->pt_outseg->seg_stp, &application->a_ray,
	    partition->pt_outflip);
	if (segment.thickness > 0.0)
	    append_segment_samples(*data->samples, segment,
		data->context->next_pair_id, SPSR_MAX_REFINEMENT_SAMPLES);
    }
    return 1;
}

static int
source_ray_miss(struct application *UNUSED(application))
{
    return 0;
}

static void
sample_hints(spsr_context &context,
    const struct bg_3d_spsr_refinement_request *request)
{
    if (!request->hint_count ||
	context.refinement_samples.size() >= SPSR_MAX_REFINEMENT_SAMPLES)
	return;

    struct resource resource = RT_RESOURCE_INIT_ZERO;
    rt_init_resource(&resource, 0, context.source);
    struct application application;
    RT_APPLICATION_INIT(&application);
    source_ray_data data = {&context, &context.refinement_samples};
    application.a_rt_i = context.source;
    application.a_resource = &resource;
    application.a_hit = source_ray_hit;
    application.a_miss = source_ray_miss;
    application.a_logoverlap = rt_silent_logoverlap;
    application.a_uptr = &data;

    fastf_t launch_distance =
	2.0 * DIST_PNT_PNT(context.bounds_min, context.bounds_max) +
	context.feature_size;
    size_t hint_count = std::min(request->hint_count,
	SPSR_MAX_HINTS_PER_PASS);
    for (size_t hint_index = 0; hint_index < hint_count; hint_index++) {
	vect_t direction;
	VMOVE(direction, request->hints[hint_index].normal);
	if (VNEAR_ZERO(direction, VUNITIZE_TOL))
	    continue;
	VUNITIZE(direction);

	vect_t reference;
	if (std::fabs(direction[Z]) < 0.9)
	    VSET(reference, 0.0, 0.0, 1.0);
	else
	    VSET(reference, 1.0, 0.0, 0.0);
	vect_t tangent_u;
	vect_t tangent_v;
	VCROSS(tangent_u, direction, reference);
	VUNITIZE(tangent_u);
	VCROSS(tangent_v, direction, tangent_u);
	VUNITIZE(tangent_v);

	for (int u = -1; u <= 1; u++) {
	    for (int v = -1; v <= 1; v++) {
		point_t target;
		VJOIN2(target, request->hints[hint_index].point,
		    u * SPSR_HINT_STENCIL_SCALE * context.feature_size,
		    tangent_u,
		    v * SPSR_HINT_STENCIL_SCALE * context.feature_size,
		    tangent_v);
		VJOIN1(application.a_ray.r_pt, target, -launch_distance,
		    direction);
		VMOVE(application.a_ray.r_dir, direction);
		rt_shootray(&application);
		if (context.refinement_samples.size() >=
			SPSR_MAX_REFINEMENT_SAMPLES)
		    break;
	    }
	    if (context.refinement_samples.size() >=
		    SPSR_MAX_REFINEMENT_SAMPLES)
		break;
	}
	if (context.refinement_samples.size() >=
		SPSR_MAX_REFINEMENT_SAMPLES)
	    break;
    }

    rt_clean_resource_basic(context.source, &resource);
    BU_PTBL_SET(&context.source->rti_resources, 0, NULL);
}

static int
refine_spsr(struct bg_3d_spsr_refinement_response *response,
    const struct bg_3d_spsr_refinement_request *request, void *client_data)
{
    if (!response || !request || !client_data)
	return BRLCAD_ERROR;

    spsr_context &context = *static_cast<spsr_context *>(client_data);
    context.refinement_samples.clear();
    struct rt_bot_internal *candidate = new_bot(
	const_cast<int *>(request->faces), request->face_count,
	const_cast<point_t *>(request->vertices), request->vertex_count);
    bool evaluated = evaluate_candidate(context, candidate,
	response->validation, &context.refinement_samples);
    candidate->faces = NULL;
    candidate->vertices = NULL;
    _tess_facetize_free_bot(candidate);
    if (!evaluated)
	return BRLCAD_ERROR;

    if (!response->validation.passed) {
	const validation_limits &limits = context.best_effort_source ?
	    SPSR_BEST_EFFORT_LIMITS : SPSR_STRICT_LIMITS;
	const fastf_t score = validation_score(response->validation,
	    context.feature_size, limits);
	if (!context.have_validation_score) {
	    context.best_validation_score = score;
	    context.have_validation_score = true;
	} else {
	    /* Independent ray batches have some noise.  Continue only for a
	     * measurable reduction in the worst normalized validation error;
	     * the time, point, and pass ceilings remain the hard safeguards. */
	    const bool improved = std::isfinite(score) &&
		(!std::isfinite(context.best_validation_score) ||
		 score < context.best_validation_score *
		    (1.0 - SPSR_MIN_RELATIVE_IMPROVEMENT));
	    if (improved) {
		context.best_validation_score = score;
		context.stagnant_validation_passes = 0;
	    } else {
		context.stagnant_validation_passes++;
	    }
	}

	if (context.stagnant_validation_passes >= SPSR_STAGNANT_PASS_LIMIT) {
	    context.validation_stagnated = true;
	    response->stop_refinement = 1;
	} else {
	    sample_hints(context, request);
	}
    }
    response->samples = context.refinement_samples.data();
    response->sample_count = context.refinement_samples.size();
    return BRLCAD_OK;
}

static bool
pnts_samples(std::vector<struct bg_3d_spsr_sample> &samples,
    struct rt_pnts_internal *pnts)
{
    if (!pnts || !pnts->point || !(pnts->type & RT_PNT_TYPE_NRM))
	return false;

    struct bu_list *head = &static_cast<struct pnt *>(pnts->point)->l;
    for (struct bu_list *node = head->forw; node != head;
	 node = node->forw) {
	const point_t *point = NULL;
	const vect_t *normal = NULL;
	switch (pnts->type) {
	    case RT_PNT_TYPE_NRM:
		point = &reinterpret_cast<struct pnt_normal *>(node)->v;
		normal = &reinterpret_cast<struct pnt_normal *>(node)->n;
		break;
	    case RT_PNT_TYPE_COL_NRM:
		point = &reinterpret_cast<struct pnt_color_normal *>(node)->v;
		normal = &reinterpret_cast<struct pnt_color_normal *>(node)->n;
		break;
	    case RT_PNT_TYPE_SCA_NRM:
		point = &reinterpret_cast<struct pnt_scale_normal *>(node)->v;
		normal = &reinterpret_cast<struct pnt_scale_normal *>(node)->n;
		break;
	    case RT_PNT_TYPE_COL_SCA_NRM:
		point =
		    &reinterpret_cast<struct pnt_color_scale_normal *>(node)->v;
		normal =
		    &reinterpret_cast<struct pnt_color_scale_normal *>(node)->n;
		break;
	    default:
		return false;
	}
	struct bg_3d_spsr_sample sample = {};
	VMOVE(sample.point, *point);
	VMOVE(sample.normal, *normal);
	samples.push_back(sample);
    }
    return !samples.empty();
}

static const char *
termination_name(int termination)
{
    switch (termination) {
	case BG_3D_SPSR_COMPLETE:
	    return "accepted";
	case BG_3D_SPSR_CALLBACK_STOP:
	    return "fidelity target not reached";
	case BG_3D_SPSR_POINT_LIMIT:
	    return "point limit reached";
	case BG_3D_SPSR_TIME_LIMIT:
	    return "time limit reached";
	case BG_3D_SPSR_NO_NEW_SAMPLES:
	    return "no new source samples";
	case BG_3D_SPSR_SOLVER_ERROR:
	    return "solver error";
	default:
	    return "unknown status";
    }
}

} // namespace

int
spsr_mesh(struct rt_bot_internal **output, struct db_i *dbip,
    const char *object, struct rt_pnts_internal *pnts,
    bool best_effort_source, tess_opts *settings)
{
    if (!output || !dbip || !settings || (!object && !pnts))
	return BRLCAD_ERROR;
    *output = NULL;

    const int64_t start = bu_gettime();
    spsr_context context;
    context.best_effort_source = best_effort_source;
    std::vector<struct bg_3d_spsr_sample> samples;
    struct rt_crofton_result initial = RT_CROFTON_RESULT_INIT;

    if (pnts) {
	if (!pnts_samples(samples, pnts)) {
	    bu_log("SPSR: point input does not contain usable normals\n");
	    return BRLCAD_ERROR;
	}
	point_t minimum;
	point_t maximum;
	VSETALL(minimum, INFINITY);
	VSETALL(maximum, -INFINITY);
	for (const auto &sample : samples)
	    VMINMAX(minimum, maximum, sample.point);
	context.feature_size = settings->spsr_options.feature_size > 0.0 ?
	    settings->spsr_options.feature_size :
	    SPSR_DEFAULT_FEATURE_FRACTION *
		DIST_PNT_PNT(minimum, maximum);
    } else {
	context.source = prepare_object(dbip, object);
	if (!context.source ||
	    !prepared_bounds(context.bounds_min, context.bounds_max,
		context.source)) {
	    if (context.source)
		rt_i_destroy(context.source);
	    bu_log("SPSR: could not prepare source object %s\n", object);
	    return BRLCAD_ERROR;
	}

	struct rt_crofton_params parameters = {};
	if (settings->spsr_options.max_pnts > 0)
	    parameters.n_rays =
		static_cast<size_t>(settings->spsr_options.max_pnts) / 2;
	double sampling_time = settings->spsr_options.max_sample_time;
	if (settings->spsr_options.max_time > 0 &&
	    (sampling_time <= 0.0 ||
	    sampling_time > settings->spsr_options.max_time))
	    sampling_time = settings->spsr_options.max_time;
	parameters.time_ms = sampling_time > 0.0 ?
	    1000.0 * sampling_time : 0.0;

	int sampling_status = rt_crofton_collect(&initial, context.source,
	    &parameters, 0, context.bounds_min, context.bounds_max);
	if (sampling_status <= 0 || !initial.segment_count) {
	    rt_crofton_result_free(&initial);
	    rt_i_destroy(context.source);
	    bu_log("SPSR: source sampling failed for %s\n", object);
	    return BRLCAD_ERROR;
	}
	context.ray_offset = initial.ray_count;

	size_t point_limit = settings->spsr_options.max_pnts > 0 ?
	    static_cast<size_t>(settings->spsr_options.max_pnts) : 0;
	for (size_t i = 0; i < initial.segment_count; i++)
	    append_segment_samples(samples, initial.segments[i],
		context.next_pair_id, point_limit);

	vect_t dimensions;
	VSUB2(dimensions, context.bounds_max, context.bounds_min);
	fastf_t minimum_dimension =
	    std::min(dimensions[X], std::min(dimensions[Y], dimensions[Z]));
	fastf_t thickness = average_thickness(initial);
	fastf_t characteristic = thickness > SMALL_FASTF ?
	    std::min(minimum_dimension, thickness) : minimum_dimension;
	context.feature_size = settings->spsr_options.feature_size > 0.0 ?
	    settings->spsr_options.feature_size :
	    characteristic * settings->spsr_options.feature_scale;
	rt_crofton_result_free(&initial);
    }

    if (!std::isfinite(context.feature_size) ||
	context.feature_size <= SMALL_FASTF || samples.empty()) {
	if (context.source)
	    rt_i_destroy(context.source);
	bu_log("SPSR: could not determine a positive target feature size\n");
	return BRLCAD_ERROR;
    }

    struct bg_3d_spsr_adaptive_opts options =
	BG_3D_SPSR_ADAPTIVE_OPTS_DEFAULT;
    options.solver = settings->spsr_options.s_opts;
    options.target_feature_size = context.feature_size;
    options.max_points = settings->spsr_options.max_pnts > 0 ?
	static_cast<size_t>(settings->spsr_options.max_pnts) : 0;
    options.max_refinement_passes = pnts ? 0 :
	static_cast<size_t>(settings->spsr_options.refinement_passes);
    if (settings->spsr_options.max_time > 0) {
	double elapsed = static_cast<double>(bu_gettime() - start) / 1.0e6;
	options.max_time = settings->spsr_options.max_time - elapsed;
	if (options.max_time <= 0.0) {
	    bu_log("SPSR: time limit reached while sampling %s\n",
		object ? object : "point input");
	    if (context.source)
		rt_i_destroy(context.source);
	    return BRLCAD_ERROR;
	}
    }

    int *faces = NULL;
    int face_count = 0;
    point_t *vertices = NULL;
    int vertex_count = 0;
    struct bg_3d_spsr_report report = {};
    int status = bg_3d_spsr_adaptive(&faces, &face_count, &vertices,
	&vertex_count, samples.data(), samples.size(), &options,
	context.source ? refine_spsr : NULL,
	context.source ? &context : NULL, &report);
    if (status != BRLCAD_OK) {
	bu_log("SPSR: %s for %s after %zu solve(s) and %zu sample(s); "
	    "partition %.2f%%, endpoints p95/p99 %g/%g, chord %.2f%%, "
	    "area %.2f%%, volume %.2f%%, thin %zu/%zu\n",
	    context.validation_stagnated ?
		"validation stopped improving" : termination_name(report.termination),
	    object ? object : "point input",
	    report.solve_count, report.final_sample_count,
	    report.validation.ray_count ?
		100.0 * report.validation.partition_mismatch_rays /
		    report.validation.ray_count : 100.0,
	    report.validation.endpoint_error_p95,
	    report.validation.endpoint_error_p99,
	    100.0 * report.validation.chord_error_fraction,
	    100.0 * report.validation.surface_area_error_fraction,
	    100.0 * report.validation.volume_error_fraction,
	    report.validation.thin_partition_mismatches,
	    report.validation.thin_partition_count);
	if (context.source)
	    rt_i_destroy(context.source);
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot = new_bot(faces,
	static_cast<size_t>(face_count), vertices,
	static_cast<size_t>(vertex_count));
    if (!mesh_is_valid(bot)) {
	bu_log("SPSR: reconstruction for %s is not a closed manifold\n",
	    object ? object : "point input");
	_tess_facetize_free_bot(bot);
	if (context.source)
	    rt_i_destroy(context.source);
	return BRLCAD_ERROR;
    }

    const size_t reconstructed_faces = bot->num_faces;
    fastf_t decimation_size =
	settings->spsr_options.d_feature_size > 0.0 ?
	settings->spsr_options.d_feature_size :
	SPSR_DEFAULT_DECIMATION_SCALE * context.feature_size;
    bool decimated = false;
    struct rt_bot_internal *trial_input = clone_bot(bot);
    if (trial_input) {
	struct rt_bot_internal *trial =
	    _tess_facetize_decimate(trial_input, decimation_size);
	if (trial != trial_input && mesh_is_valid(trial)) {
	    bool accept_trial = true;
	    if (context.source) {
		struct bg_3d_spsr_validation decimated_validation = {};
		accept_trial = evaluate_candidate(context, trial,
		    decimated_validation, NULL) &&
		    decimated_validation.passed;
	    }
	    if (accept_trial) {
		_tess_facetize_free_bot(bot);
		bot = trial;
		decimated = true;
	    } else {
		_tess_facetize_free_bot(trial);
	    }
	} else {
	    _tess_facetize_free_bot(trial);
	}
    }

    if (context.best_effort_accepted)
	bu_log("SPSR: accepted best-effort fidelity for %s because the source "
	    "BoT is not a closed manifold\n", object);

    bu_log("SPSR: %s %s after %zu solve(s): %zu sample(s), "
	"%zu face(s)%s, target feature size %g\n",
	object ? object : "point input", termination_name(report.termination),
	report.solve_count, report.final_sample_count, bot->num_faces,
	decimated && bot->num_faces != reconstructed_faces ?
	    " after validated decimation" : "", context.feature_size);

    if (context.source)
	rt_i_destroy(context.source);
    *output = bot;
    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
