/*             B R E P _ A D J A C E N T _ S P A N . C P P
 * BRL-CAD
 *
 * Internal Bezier-span BREP ray regression.
 */

#include "common.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include "bu/app.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "brep/cobb.h"
#include "../librt_private.h"

#include "../primitives/brep/brep_local.h"

static const int ADJACENT_SPAN_SURFACE_TREE_DEPTH = 0;
static const double ADJACENT_SPAN_RADIUS = 10.0;
static const double ADJACENT_SPAN_RADIAL_OFFSET = 8.0;
static const double ADJACENT_SPAN_ORIGIN_DISTANCE = 20.0;
static const int ADJACENT_SPAN_HIERARCHY_SPAN_COUNT = 16;
static const double ADJACENT_SPAN_ENTRY_DISTANCE =
    ADJACENT_SPAN_ORIGIN_DISTANCE - std::sqrt(
	ADJACENT_SPAN_RADIUS * ADJACENT_SPAN_RADIUS -
	ADJACENT_SPAN_RADIAL_OFFSET * ADJACENT_SPAN_RADIAL_OFFSET);
static const double ADJACENT_SPAN_EXIT_DISTANCE =
    ADJACENT_SPAN_ORIGIN_DISTANCE + std::sqrt(
	ADJACENT_SPAN_RADIUS * ADJACENT_SPAN_RADIUS -
	ADJACENT_SPAN_RADIAL_OFFSET * ADJACENT_SPAN_RADIAL_OFFSET);


static struct soltab *
prepare_solid(struct rt_i *rtip, struct rt_db_internal *intern, int type)
{
    struct soltab *stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab),
	"adjacent span ray soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    struct directory *directory = (struct directory *)bu_calloc(1,
	sizeof(struct directory), "adjacent span ray directory");
    directory->d_magic = RT_DIR_MAGIC;
    directory->d_namep = (char *)"adjacent_span_ray.s";
    stp->st_dp = directory;
    stp->st_id = type;
    stp->st_meth = &OBJ[type];

    if (OBJ[type].ft_prep(stp, intern, rtip)) {
	if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	    stp->st_meth->ft_free(stp);
	bu_free((void *)stp->st_dp, "adjacent span ray directory");
	bu_free(stp, "adjacent span ray soltab");
	return NULL;
    }
    return stp;
}


static void
free_solid(struct soltab *stp)
{
    if (!stp)
	return;
    if (stp->st_meth && stp->st_meth->ft_free)
	stp->st_meth->ft_free(stp);
    bu_free((void *)stp->st_dp, "adjacent span ray directory");
    bu_free(stp, "adjacent span ray soltab");
}


static void
free_rtip(struct rt_i *rtip, struct resource *resource)
{
    if (!rtip || !resource)
	return;
    rt_clean_resource_basic(rtip, resource);
    BU_PTBL_SET(&rtip->rti_resources, 0, NULL);
    rt_i_destroy(rtip);
}


static ON_Brep *
make_adjacent_span_sphere()
{
    const ON_3dPoint center(0.0, 0.0, 0.0);
    ON_Brep *brep = ON_Brep_CobbSphereSewn(ADJACENT_SPAN_RADIUS, center);
    if (!brep)
	return NULL;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const int surface_index = brep->m_F[face_index].m_si;
	ON_NurbsSurface *surface = surface_index >= 0 &&
	    surface_index < brep->m_S.Count() ? ON_NurbsSurface::Cast(
	brep->m_S[surface_index]) : NULL;
	if (!surface) {
	    delete brep;
	    return NULL;
	}
	const ON_Interval domain = surface->Domain(1);
	const double span_width = 0.25 * (domain.Max() - domain.Min());
	if (!domain.IsIncreasing() || !(span_width > 0.0) ||
		!surface->InsertKnot(1, domain.Min() + span_width) ||
		!surface->InsertKnot(1, domain.Mid())) {
	    delete brep;
	    return NULL;
	}
    }
    return brep;
}


static ON_Brep *
make_hierarchy_span_sphere()
{
    const ON_3dPoint center(0.0, 0.0, 0.0);
    ON_Brep *brep = ON_Brep_CobbSphereSewn(ADJACENT_SPAN_RADIUS, center);
    if (!brep)
	return NULL;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const int surface_index = brep->m_F[face_index].m_si;
	ON_NurbsSurface *surface = surface_index >= 0 &&
	    surface_index < brep->m_S.Count() ? ON_NurbsSurface::Cast(
	brep->m_S[surface_index]) : NULL;
	if (!surface) {
	    delete brep;
	    return NULL;
	}
	const ON_Interval domain = surface->Domain(1);
	const double span_width = (domain.Max() - domain.Min()) /
	    ADJACENT_SPAN_HIERARCHY_SPAN_COUNT;
	if (!domain.IsIncreasing() || !(span_width > 0.0)) {
	    delete brep;
	    return NULL;
	}
	for (int span = 1; span < ADJACENT_SPAN_HIERARCHY_SPAN_COUNT;
		++span) {
	    if (!surface->InsertKnot(1, domain.Min() + span * span_width)) {
		delete brep;
		return NULL;
	    }
	}
    }
    return brep;
}


static struct soltab *
prepare_brep_solid(struct rt_i *rtip, ON_Brep *brep)
{
    if (!rtip || !brep)
	return NULL;
    struct rt_brep_internal brep_internal = {};
    brep_internal.magic = RT_BREP_INTERNAL_MAGIC;
    brep_internal.brep = brep;
    struct rt_db_internal database_internal;
    RT_DB_INTERNAL_INIT(&database_internal);
    database_internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    database_internal.idb_type = ID_BREP;
    database_internal.idb_meth = &OBJ[ID_BREP];
    database_internal.idb_ptr = &brep_internal;
    return prepare_solid(rtip, &database_internal, ID_BREP);
}


static bool
bbox_contains(const ON_BoundingBox &outer, const ON_BoundingBox &inner)
{
    if (!outer.IsValid() || !inner.IsValid())
	return false;
    for (int axis = 0; axis < 3; ++axis) {
	if (outer.m_min[axis] > inner.m_min[axis] ||
		outer.m_max[axis] < inner.m_max[axis])
	    return false;
    }
    return true;
}


static bool
prepared_span_tree_node_valid(const brep_specific &specific,
    size_t node_index, size_t span_begin, size_t span_count)
{
    if (!span_count || node_index >= specific.prepared_span_nodes.size() ||
	span_begin > specific.surface_spans.size() ||
	span_count > specific.surface_spans.size() - span_begin)
	return false;
    const brep_prepared_span_node &node =
	specific.prepared_span_nodes[node_index];
    if (node.span_count != span_count || !node.cullable || !node.bbox.IsValid())
	return false;
    if (span_count == 1)
	return bbox_contains(node.bbox, specific.surface_spans[span_begin].bbox);
    const size_t left_count = span_count / 2;
    const size_t left_child = node_index + 1;
    const size_t right_child = left_child + 2 * left_count - 1;
    if (right_child >= specific.prepared_span_nodes.size() ||
	!prepared_span_tree_node_valid(specific, left_child, span_begin,
	    left_count) ||
	!prepared_span_tree_node_valid(specific, right_child,
	    span_begin + left_count, span_count - left_count))
	return false;
    return bbox_contains(node.bbox,
	specific.prepared_span_nodes[left_child].bbox) &&
	bbox_contains(node.bbox,
	specific.prepared_span_nodes[right_child].bbox);
}


static bool
prepared_span_tree_available(const struct soltab *solid)
{
    const brep_specific *specific = solid ?
	static_cast<const brep_specific *>(solid->st_specific) : NULL;
    if (!specific)
	return false;
    size_t multi_span_faces = 0;
    bool multi_level_tree = false;
    for (std::vector<brep_face_record>::const_iterator record_it =
	    specific->face_records.begin();
	    record_it != specific->face_records.end(); ++record_it) {
	const brep_face_record &record = *record_it;
	if (!record.supported || record.span_count < 2)
	    continue;
	multi_span_faces++;
	if (record.span_count >= 3)
	    multi_level_tree = true;
	if (!record.span_tree_available ||
		record.span_tree_root >= specific->prepared_span_nodes.size())
	    return false;
	if (!prepared_span_tree_node_valid(*specific, record.span_tree_root,
		record.span_begin, record.span_count))
	    return false;
    }
    return multi_span_faces > 0 && multi_level_tree;
}


static bool
disable_prepared_span_tree(struct soltab *solid)
{
    brep_specific *specific = solid ?
	static_cast<brep_specific *>(solid->st_specific) : NULL;
    if (!specific)
	return false;
    for (std::vector<brep_face_record>::iterator record_it =
	    specific->face_records.begin();
	    record_it != specific->face_records.end(); ++record_it) {
	record_it->span_tree_root = 0;
	record_it->span_tree_available = false;
    }
    specific->prepared_span_nodes.clear();
    return true;
}


static bool
disable_prepared_face_bvh(struct soltab *solid)
{
    brep_specific *specific = solid ?
	static_cast<brep_specific *>(solid->st_specific) : NULL;
    if (!specific)
	return false;
    specific->prepared_face_bvh_nodes.clear();
    specific->prepared_face_bvh_root = (size_t)-1;
    return true;
}


static bool
disable_prepared_face_tree(struct soltab *solid)
{
    brep_specific *specific = solid ?
	static_cast<brep_specific *>(solid->st_specific) : NULL;
    if (!specific || !disable_prepared_face_bvh(solid))
	return false;
    specific->prepared_face_nodes.clear();
    return true;
}


static bool
prepared_face_tree_node_valid(const brep_specific &specific,
    size_t node_index, size_t face_begin, size_t face_count)
{
    if (!face_count || node_index >= specific.prepared_face_nodes.size() ||
	face_begin > specific.face_records.size() ||
	face_count > specific.face_records.size() - face_begin)
	return false;
    const brep_prepared_face_node &node =
	specific.prepared_face_nodes[node_index];
    if (node.face_begin != face_begin)
	return false;
    if (face_count == 1) {
	const brep_face_record &record = specific.face_records[face_begin];
	const size_t supported = record.supported ? 1 : 0;
	const size_t reparameterized = record.supported &&
	    record.nurb_form_status == 2 ? 1 : 0;
	const size_t unsupported = record.supported ? 0 : 1;
	const bool cullable = record.supported && record.span_count &&
	    record.bbox.IsValid();
	return node.leaf && node.span_count ==
	    (record.supported ? record.span_count : 0) &&
	    node.supported_face_count == supported &&
	    node.reparameterized_face_count == reparameterized &&
	    node.unsupported_face_count == unsupported &&
	    node.cullable == cullable && (!record.supported ||
	    !record.bbox.IsValid() ||
	    bbox_contains(node.bbox, record.bbox));
    }
    const size_t left_count = face_count / 2;
    const size_t right_count = face_count - left_count;
    if (node.leaf || node.left_child >= specific.prepared_face_nodes.size() ||
	node.right_child >= specific.prepared_face_nodes.size() ||
	!prepared_face_tree_node_valid(specific, node.left_child, face_begin,
	    left_count) || !prepared_face_tree_node_valid(specific,
	    node.right_child, face_begin + left_count, right_count))
	return false;
    const brep_prepared_face_node &left =
	specific.prepared_face_nodes[node.left_child];
    const brep_prepared_face_node &right =
	specific.prepared_face_nodes[node.right_child];
    const bool left_complete = !left.supported_face_count || left.cullable;
    const bool right_complete = !right.supported_face_count || right.cullable;
    const bool cullable = node.span_count && left_complete && right_complete &&
	node.bbox.IsValid();
    return node.span_count == left.span_count + right.span_count &&
	node.supported_face_count == left.supported_face_count +
	right.supported_face_count && node.reparameterized_face_count ==
	left.reparameterized_face_count + right.reparameterized_face_count &&
	node.unsupported_face_count == left.unsupported_face_count +
	right.unsupported_face_count && node.cullable == cullable &&
	(!left.bbox.IsValid() || bbox_contains(node.bbox, left.bbox)) &&
	(!right.bbox.IsValid() || bbox_contains(node.bbox, right.bbox));
}


static bool
prepared_face_tree_available(const struct soltab *solid)
{
    const brep_specific *specific = solid ?
	static_cast<const brep_specific *>(solid->st_specific) : NULL;
    return specific && !specific->face_records.empty() &&
	!specific->prepared_face_nodes.empty() &&
	prepared_face_tree_node_valid(*specific, 0, 0,
	    specific->face_records.size());
}


static bool
prepared_face_bvh_node_valid(const brep_specific &specific,
    size_t node_index, std::vector<bool> &seen_faces)
{
    if (node_index >= specific.prepared_face_bvh_nodes.size())
	return false;
    const brep_face_bvh_node &node =
	specific.prepared_face_bvh_nodes[node_index];
    if (!node.bbox.IsValid())
	return false;
    if (node.leaf) {
	if (node.index >= specific.face_records.size() ||
		seen_faces[node.index])
	    return false;
	const brep_face_record &record = specific.face_records[node.index];
	if (!record.supported || !record.span_count || !record.bbox.IsValid() ||
		!bbox_contains(node.bbox, record.bbox))
	    return false;
	seen_faces[node.index] = true;
	return true;
    }
    if (node.left_child <= node_index || node.right_child <= node_index ||
	node.left_child >= specific.prepared_face_bvh_nodes.size() ||
	node.right_child >= specific.prepared_face_bvh_nodes.size())
	return false;
    const brep_face_bvh_node &left =
	specific.prepared_face_bvh_nodes[node.left_child];
    const brep_face_bvh_node &right =
	specific.prepared_face_bvh_nodes[node.right_child];
    return bbox_contains(node.bbox, left.bbox) &&
	bbox_contains(node.bbox, right.bbox) &&
	prepared_face_bvh_node_valid(specific, node.left_child, seen_faces) &&
	prepared_face_bvh_node_valid(specific, node.right_child, seen_faces);
}


static bool
prepared_face_bvh_available(const struct soltab *solid)
{
    const brep_specific *specific = solid ?
	static_cast<const brep_specific *>(solid->st_specific) : NULL;
    if (!specific || specific->prepared_face_bvh_nodes.empty() ||
	specific->prepared_face_bvh_root >=
	specific->prepared_face_bvh_nodes.size())
	return false;
    std::vector<bool> seen_faces(specific->face_records.size(), false);
    if (!prepared_face_bvh_node_valid(*specific,
	specific->prepared_face_bvh_root, seen_faces))
	return false;
    for (size_t record_index = 0;
	record_index < specific->face_records.size(); ++record_index) {
	if (specific->face_records[record_index].supported !=
		seen_faces[record_index])
	    return false;
    }
    return true;
}


static int
shoot_trace(struct soltab *stp, struct rt_i *rtip, struct resource *resource,
    const point_t origin, const vect_t direction,
    struct rt_brep_shot_trace &trace)
{
    struct application application;
    struct seg seghead;
    struct xray ray;
    RT_APPLICATION_INIT(&application);
    application.a_rt_i = rtip;
    application.a_resource = resource;
    VMOVE(ray.r_pt, origin);
    VMOVE(ray.r_dir, direction);
    ray.magic = RT_RAY_MAGIC;
    BU_LIST_INIT(&seghead.l);
    const int hits = _rt_brep_shot_trace(stp, &ray, &application, &seghead,
	&trace);

    struct seg *segment;
    while (BU_LIST_WHILE(segment, seg, &seghead.l)) {
	BU_LIST_DEQUEUE(&segment->l);
	RT_FREE_SEG(segment, resource);
    }
    return hits;
}


static void
report_trace(const struct rt_brep_shot_trace &trace)
{
    std::printf("  selected/fallback/hits=%zu/%d/%zu boxes/roots/events="
	"%zu/%zu/%zu stream=%zu/%zu/%zu/%zu/%zu stage=%d\n",
	trace.prepared_production_selected, trace.prepared_production_fallback,
	trace.prepared_production_hits, trace.stored_surface_boxes,
	trace.stored_local_roots, trace.stored_physical_events,
	trace.physical_event_regular_stream_attempts,
	trace.physical_event_regular_stream_certified,
	trace.physical_event_regular_stream_components,
	trace.physical_event_regular_stream_boxes,
	trace.physical_event_regular_stream_roots,
	trace.physical_event_regular_stream_failure_stage);
    for (size_t root_index = 0; root_index < trace.stored_local_roots;
	 ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	std::printf("  root[%zu] face/span=%d/%d uv=(%.17g,%.17g) t=%.17g "
	    "normal/direction=%.17g/%d\n", root_index, root.face_index,
	    root.span_index, root.uv[0], root.uv[1], root.dist,
	    root.normal_dot, root.direction);
    }
    for (size_t box_index = 0; box_index < trace.stored_surface_boxes;
	 ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[box_index];
	std::printf("  box[%zu] face/span=%d/%d uv=[%.17g,%.17g]x"
	    "[%.17g,%.17g] t=[%.17g,%.17g] state/sign=%d/%d\n",
	    box_index, box.face_index, box.span_index, box.uv_min[0],
	    box.uv_max[0], box.uv_min[1], box.uv_max[1], box.t_min,
	    box.t_max, box.disposition, box.determinant_sign);
    }
    for (size_t event_index = 0;
	 event_index < trace.stored_physical_events; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	std::printf("  event[%zu] face/span=%d/%d t=[%.17g,%.17g] "
	    "distance=%.17g root/boxes=%zu/%zu certificate/direction=%d/%d\n",
	    event_index, event.face_index, event.span_index, event.t_min,
	    event.t_max, event.dist, event.source_root, event.source_box_count,
	    event.certificate, event.direction);
    }
}


static bool
trace_has_expected_events(const struct rt_brep_shot_trace &trace, int hits,
    double distance_tolerance)
{
    if (hits != 2 || trace.prepared_production_selected != 1 ||
	trace.prepared_production_fallback != RT_BREP_PREPARED_FALLBACK_NONE ||
	trace.prepared_production_hits != 2 || trace.physical_event_complete != 1 ||
	trace.physical_event_regular_stream_attempts != 1 ||
	trace.physical_event_regular_stream_certified != 1 ||
	trace.physical_event_regular_stream_components != 2 ||
	trace.physical_event_regular_stream_boxes !=
	trace.stored_surface_boxes ||
	trace.physical_event_regular_stream_roots !=
	trace.stored_local_roots ||
	trace.physical_event_regular_stream_failure_stage ||
	trace.stored_local_roots != 4 || trace.stored_physical_events != 2 ||
	trace.physical_event_regular != 2 || trace.physical_event_unresolved ||
	trace.physical_event_direction_mismatches ||
	trace.physical_event_state_failures ||
	trace.physical_event_material_segments != 1 ||
	trace.physical_event_subminimum_contacts ||
	trace.physical_event_tolerance_ambiguous)
	return false;

    const double expected_distance[2] = {
	ADJACENT_SPAN_ENTRY_DISTANCE, ADJACENT_SPAN_EXIT_DISTANCE
    };
    const int expected_direction[2] = {
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING
    };
    size_t source_boxes = 0;
    for (size_t event_index = 0; event_index < 2; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	if (event.certificate != RT_BREP_TRACE_EVENT_REGULAR_INTERIOR ||
		event.source_kind != RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT ||
		event.source_root >= trace.stored_local_roots ||
		event.source_box >= trace.stored_surface_boxes ||
		event.source_box_count < 2 || !event.determinant_sign ||
		event.direction != expected_direction[event_index] ||
		fabs(event.dist - expected_distance[event_index]) >
		distance_tolerance || event.dist < event.t_min ||
		event.dist > event.t_max)
	    return false;
	source_boxes += event.source_box_count;
    }
    if (source_boxes != trace.stored_surface_boxes)
	return false;
    for (size_t box_index = 0; box_index < trace.stored_surface_boxes;
	 ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[box_index];
	if (box.disposition != RT_BREP_TRACE_BOX_RESOLVED_REGULAR_STREAM ||
		!box.determinant_sign)
	    return false;
    }
    return true;
}


static bool
trace_has_hierarchy_pruning(const struct soltab *solid,
    const struct rt_brep_shot_trace &trace, int hits,
    double distance_tolerance)
{
    const brep_specific *specific = solid ?
	static_cast<const brep_specific *>(solid->st_specific) : NULL;
    if (!specific || !trace_has_expected_events(trace, hits,
	distance_tolerance))
	return false;
    size_t largest_face_span_count = 0;
    for (std::vector<brep_face_record>::const_iterator record_it =
	    specific->face_records.begin();
	    record_it != specific->face_records.end(); ++record_it) {
	const brep_face_record &record = *record_it;
	if (record.supported && record.span_count > largest_face_span_count)
	    largest_face_span_count = record.span_count;
    }
    const size_t prepared_span_count = specific->surface_spans.size();
    return largest_face_span_count >= ADJACENT_SPAN_HIERARCHY_SPAN_COUNT &&
	trace.prepared_surface_spans == prepared_span_count &&
	trace.candidate_surface_spans + trace.excluded_surface_spans ==
	prepared_span_count && trace.excluded_surface_spans >
	3 * prepared_span_count / 4 && trace.candidate_surface_spans <
	prepared_span_count / 4 && trace.surface_coefficient_expansion_avoided >=
	trace.excluded_surface_spans;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    struct rt_i *rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    if (!rtip) {
	std::printf("FAIL: in-memory raytrace setup\n");
	return 1;
    }
    rtip->rti_tol.magic = BN_TOL_MAGIC;
    rtip->rti_tol.dist = 0.0005;
    rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
    rtip->rti_tol.perp = 1.0e-6;
    rtip->rti_tol.para = 1.0 - rtip->rti_tol.perp;
    if (!_rt_brep_set_surface_tree_depth(rtip,
	ADJACENT_SPAN_SURFACE_TREE_DEPTH)) {
	std::printf("FAIL: BREP surface tree setup\n");
	rt_i_destroy(rtip);
	return 1;
    }

    struct resource resource = {};
    rt_init_resource(&resource, 0, rtip);
    ON_Brep *brep = make_adjacent_span_sphere();
    if (!brep || !brep->IsSolid()) {
	std::printf("FAIL: adjacent-span sphere construction\n");
	delete brep;
	free_rtip(rtip, &resource);
	return 1;
    }
    struct soltab *solid = prepare_brep_solid(rtip, brep);
    if (!solid) {
	std::printf("FAIL: adjacent-span sphere preparation\n");
	delete brep;
	free_rtip(rtip, &resource);
	return 1;
    }

    const bool span_tree_valid = prepared_span_tree_available(solid) &&
	prepared_face_tree_available(solid);
    if (!span_tree_valid)
	std::printf("FAIL: adjacent-span hierarchy preparation\n");

    const point_t forward_origin = {
	ADJACENT_SPAN_RADIAL_OFFSET, 0.0,
	-ADJACENT_SPAN_ORIGIN_DISTANCE
    };
    const vect_t forward_direction = {0.0, 0.0, 1.0};
    const point_t reverse_origin = {
	ADJACENT_SPAN_RADIAL_OFFSET, 0.0,
	ADJACENT_SPAN_ORIGIN_DISTANCE
    };
    const vect_t reverse_direction = {0.0, 0.0, -1.0};
    const double distance_tolerance = std::max(1.0e-9,
	(double)rtip->rti_tol.dist);

    struct rt_brep_shot_trace forward_trace = {};
    const int forward_hits = shoot_trace(solid, rtip, &resource,
	forward_origin, forward_direction, forward_trace);
    const bool forward_valid = trace_has_expected_events(forward_trace,
	forward_hits, distance_tolerance);
    if (!forward_valid) {
	std::printf("FAIL: forward adjacent-span stream\n");
	report_trace(forward_trace);
    }

    struct rt_brep_shot_trace reverse_trace = {};
    const int reverse_hits = shoot_trace(solid, rtip, &resource,
	reverse_origin, reverse_direction, reverse_trace);
    const bool reverse_valid = trace_has_expected_events(reverse_trace,
	reverse_hits, distance_tolerance);
    if (!reverse_valid) {
	std::printf("FAIL: reverse adjacent-span stream\n");
	report_trace(reverse_trace);
    }

    bool hierarchy_valid = false;
    ON_Brep *hierarchy_brep = make_hierarchy_span_sphere();
    if (hierarchy_brep && hierarchy_brep->IsSolid()) {
	struct soltab *hierarchy_solid = prepare_brep_solid(rtip, hierarchy_brep);
	if (hierarchy_solid) {
	    const bool hierarchy_tree_valid =
		prepared_span_tree_available(hierarchy_solid) &&
		prepared_face_tree_available(hierarchy_solid) &&
		prepared_face_bvh_available(hierarchy_solid);
	    struct rt_brep_shot_trace hierarchy_forward_trace = {};
	    const int hierarchy_forward_hits = shoot_trace(hierarchy_solid, rtip,
		&resource, forward_origin, forward_direction,
		hierarchy_forward_trace);
	    struct rt_brep_shot_trace hierarchy_reverse_trace = {};
	    const int hierarchy_reverse_hits = shoot_trace(hierarchy_solid, rtip,
		&resource, reverse_origin, reverse_direction,
		hierarchy_reverse_trace);
	    const bool hierarchy_pruned = hierarchy_tree_valid &&
		trace_has_hierarchy_pruning(hierarchy_solid,
		    hierarchy_forward_trace, hierarchy_forward_hits,
		    distance_tolerance) && trace_has_hierarchy_pruning(hierarchy_solid,
		    hierarchy_reverse_trace, hierarchy_reverse_hits,
		    distance_tolerance);
	    bool spatial_fallback_valid = false;
	    if (disable_prepared_face_bvh(hierarchy_solid)) {
		const bool spatial_fallback_tree_valid =
		    prepared_span_tree_available(hierarchy_solid) &&
		    prepared_face_tree_available(hierarchy_solid) &&
		    !prepared_face_bvh_available(hierarchy_solid);
		struct rt_brep_shot_trace fallback_forward_trace = {};
		const int fallback_forward_hits = shoot_trace(hierarchy_solid,
		    rtip, &resource, forward_origin, forward_direction,
		    fallback_forward_trace);
		struct rt_brep_shot_trace fallback_reverse_trace = {};
		const int fallback_reverse_hits = shoot_trace(hierarchy_solid,
		    rtip, &resource, reverse_origin, reverse_direction,
		    fallback_reverse_trace);
		spatial_fallback_valid = spatial_fallback_tree_valid &&
		    trace_has_hierarchy_pruning(hierarchy_solid, fallback_forward_trace,
		    fallback_forward_hits, distance_tolerance) &&
		    trace_has_hierarchy_pruning(hierarchy_solid, fallback_reverse_trace,
		    fallback_reverse_hits, distance_tolerance);
	    }
	    bool face_fallback_valid = false;
	    if (disable_prepared_face_tree(hierarchy_solid)) {
		const bool face_fallback_tree_valid =
		    prepared_span_tree_available(hierarchy_solid) &&
		    !prepared_face_tree_available(hierarchy_solid) &&
		    !prepared_face_bvh_available(hierarchy_solid);
		struct rt_brep_shot_trace fallback_forward_trace = {};
		const int fallback_forward_hits = shoot_trace(hierarchy_solid,
		    rtip, &resource, forward_origin, forward_direction,
		    fallback_forward_trace);
		struct rt_brep_shot_trace fallback_reverse_trace = {};
		const int fallback_reverse_hits = shoot_trace(hierarchy_solid,
		    rtip, &resource, reverse_origin, reverse_direction,
		    fallback_reverse_trace);
		face_fallback_valid = face_fallback_tree_valid &&
		    trace_has_hierarchy_pruning(hierarchy_solid, fallback_forward_trace,
		    fallback_forward_hits, distance_tolerance) &&
		    trace_has_hierarchy_pruning(hierarchy_solid, fallback_reverse_trace,
		    fallback_reverse_hits, distance_tolerance);
	    }
	    bool span_fallback_valid = false;
	    if (disable_prepared_span_tree(hierarchy_solid)) {
		struct rt_brep_shot_trace fallback_forward_trace = {};
		const int fallback_forward_hits = shoot_trace(hierarchy_solid,
		    rtip, &resource, forward_origin, forward_direction,
		    fallback_forward_trace);
		struct rt_brep_shot_trace fallback_reverse_trace = {};
		const int fallback_reverse_hits = shoot_trace(hierarchy_solid,
		    rtip, &resource, reverse_origin, reverse_direction,
		    fallback_reverse_trace);
		span_fallback_valid = trace_has_expected_events(fallback_forward_trace,
		    fallback_forward_hits, distance_tolerance) &&
		    trace_has_expected_events(fallback_reverse_trace,
		    fallback_reverse_hits, distance_tolerance);
	    }
	    hierarchy_valid = hierarchy_pruned && spatial_fallback_valid &&
		face_fallback_valid && span_fallback_valid;
	    free_solid(hierarchy_solid);
	} else {
	    delete hierarchy_brep;
	}
    } else {
	delete hierarchy_brep;
    }

    free_solid(solid);
    free_rtip(rtip, &resource);
    return span_tree_valid && forward_valid && reverse_valid &&
	hierarchy_valid ? 0 : 1;
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
