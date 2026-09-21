/*             B R E P _ S P A N _ J U N C T I O N . C P P
 * BRL-CAD
 *
 * Internal Bezier-span junction BREP ray regression.
 */

#include "common.h"

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "bu/app.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "brep/cobb.h"
#include "../librt_private.h"


static const int SPAN_JUNCTION_SURFACE_TREE_DEPTH = 0;
static const int SPAN_JUNCTION_DEFAULT_FACE_INDEX = 5;
static const double SPAN_JUNCTION_RADIUS = 10.0;
static const double SPAN_JUNCTION_RAY_CLEARANCE = 20.0;
static const double SPAN_JUNCTION_OBLIQUE_TANGENT_FRACTION = 0.25;
static const double SPAN_JUNCTION_BOUNDARY_TRANSVERSE_FRACTION = 0.25;
static const size_t SPAN_JUNCTION_STREAM_EVENTS = 2;
static const size_t SPAN_JUNCTION_JUNCTION_BOXES = 4;
static const size_t SPAN_JUNCTION_ADJACENT_BOXES = 2;
static const size_t SPAN_JUNCTION_SINGLETON_BOXES = 1;
static const double SPAN_JUNCTION_ORIGIN_RADIUS =
    SPAN_JUNCTION_RADIUS + SPAN_JUNCTION_RAY_CLEARANCE;
static const double SPAN_JUNCTION_ENTRY_DISTANCE =
    SPAN_JUNCTION_RAY_CLEARANCE;
static const double SPAN_JUNCTION_EXIT_DISTANCE =
    SPAN_JUNCTION_ORIGIN_RADIUS + SPAN_JUNCTION_RADIUS;


static struct soltab *
prepare_solid(struct rt_i *rtip, struct rt_db_internal *intern, int type)
{
    struct soltab *stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab),
	"span junction ray soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    struct directory *directory = (struct directory *)bu_calloc(1,
	sizeof(struct directory), "span junction ray directory");
    directory->d_magic = RT_DIR_MAGIC;
    directory->d_namep = (char *)"span_junction_ray.s";
    stp->st_dp = directory;
    stp->st_id = type;
    stp->st_meth = &OBJ[type];

    if (OBJ[type].ft_prep(stp, intern, rtip)) {
	if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	    stp->st_meth->ft_free(stp);
	bu_free((void *)stp->st_dp, "span junction ray directory");
	bu_free(stp, "span junction ray soltab");
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
    bu_free((void *)stp->st_dp, "span junction ray directory");
    bu_free(stp, "span junction ray soltab");
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
make_span_junction_sphere()
{
    const ON_3dPoint center(0.0, 0.0, 0.0);
    ON_Brep *brep = ON_Brep_CobbSphereSewn(SPAN_JUNCTION_RADIUS, center);
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
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval domain = surface->Domain(direction);
	    if (!domain.IsIncreasing() ||
		!surface->InsertKnot(direction, domain.Mid())) {
		delete brep;
		return NULL;
	    }
	}
    }
	return brep;
}


static ON_NurbsSurface *
span_junction_face_surface(ON_Brep *brep, int face_index)
{
    if (!brep || face_index < 0 || face_index >= brep->m_F.Count())
	return NULL;
    const int surface_index = brep->m_F[face_index].m_si;
    return surface_index >= 0 && surface_index < brep->m_S.Count() ?
	ON_NurbsSurface::Cast(brep->m_S[surface_index]) : NULL;
}


static bool
span_junction_radial(ON_Brep *brep, int face_index, ON_3dVector &radial)
{
    const ON_3dPoint center(0.0, 0.0, 0.0);
    ON_NurbsSurface *surface = span_junction_face_surface(brep, face_index);
    if (!surface)
	return false;
    const ON_3dPoint junction = surface->PointAt(surface->Domain(0).Mid(),
	surface->Domain(1).Mid());
    radial = junction - center;
    return junction.IsValid() && radial.Unitize();
}


static bool
span_junction_boundary_radial(ON_Brep *brep, int face_index,
    double transverse_fraction, ON_3dVector &radial)
{
    const ON_3dPoint center(0.0, 0.0, 0.0);
	if (!(transverse_fraction > 0.0) || !(transverse_fraction < 1.0))
	return false;
    ON_NurbsSurface *surface = span_junction_face_surface(brep, face_index);
    if (!surface)
	return false;
    const ON_Interval u_domain = surface->Domain(0);
    const ON_Interval v_domain = surface->Domain(1);
    if (!u_domain.IsIncreasing() || !v_domain.IsIncreasing())
	return false;
    const ON_3dPoint boundary = surface->PointAt(u_domain.Mid(),
	v_domain.ParameterAt(transverse_fraction));
    radial = boundary - center;
    return boundary.IsValid() && radial.Unitize();
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
	trace.stored_local_roots != 8 || trace.stored_physical_events != 2 ||
	trace.physical_event_regular != 2 || trace.physical_event_unresolved ||
	trace.physical_event_direction_mismatches ||
	trace.physical_event_state_failures ||
	trace.physical_event_material_segments != 1 ||
	trace.physical_event_subminimum_contacts ||
	trace.physical_event_tolerance_ambiguous)
	return false;

    const double expected_distance[2] = {
	SPAN_JUNCTION_ENTRY_DISTANCE, SPAN_JUNCTION_EXIT_DISTANCE
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
		event.source_box_count < 4 || !event.determinant_sign ||
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
trace_has_span_grid_events(const struct rt_brep_shot_trace &trace, int hits,
    size_t forward_box_count, size_t reverse_box_count, bool reverse)
{
    if (!forward_box_count || !reverse_box_count)
	return false;
    const size_t local_root_count = forward_box_count + reverse_box_count;
    if (hits != 2 || trace.prepared_production_selected != 1 ||
	trace.prepared_production_fallback != RT_BREP_PREPARED_FALLBACK_NONE ||
	trace.prepared_production_hits != 2 || trace.physical_event_complete != 1 ||
	trace.physical_event_regular_stream_attempts != 1 ||
	trace.physical_event_regular_stream_certified != 1 ||
	trace.physical_event_regular_stream_components !=
	SPAN_JUNCTION_STREAM_EVENTS ||
	trace.physical_event_regular_stream_boxes !=
	trace.stored_surface_boxes ||
	trace.physical_event_regular_stream_roots !=
	trace.stored_local_roots ||
	trace.physical_event_regular_stream_failure_stage ||
	trace.stored_surface_boxes != local_root_count ||
	trace.stored_local_roots != local_root_count ||
	trace.stored_physical_events != SPAN_JUNCTION_STREAM_EVENTS ||
	trace.physical_event_regular != SPAN_JUNCTION_STREAM_EVENTS ||
	trace.physical_event_unresolved ||
	trace.physical_event_direction_mismatches ||
	trace.physical_event_state_failures ||
	trace.physical_event_material_segments != 1 ||
	trace.physical_event_subminimum_contacts ||
	trace.physical_event_tolerance_ambiguous)
	return false;

    const size_t expected_boxes[SPAN_JUNCTION_STREAM_EVENTS] = {
	reverse ? reverse_box_count : forward_box_count,
	reverse ? forward_box_count : reverse_box_count
    };
    const int expected_direction[SPAN_JUNCTION_STREAM_EVENTS] = {
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING
    };
    size_t source_boxes = 0;
    for (size_t event_index = 0;
	event_index < SPAN_JUNCTION_STREAM_EVENTS; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	if (event.certificate != RT_BREP_TRACE_EVENT_REGULAR_INTERIOR ||
		event.source_kind != RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT ||
		event.source_root >= trace.stored_local_roots ||
		event.source_box >= trace.stored_surface_boxes ||
		event.source_box_count != expected_boxes[event_index] ||
		!event.determinant_sign ||
		event.direction != expected_direction[event_index] ||
		event.dist < event.t_min || event.dist > event.t_max)
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
trace_oblique_span_grid(struct soltab *solid, struct rt_i *rtip,
    struct resource *resource, const ON_3dVector &radial,
    ON_3dVector tangent, size_t forward_box_count,
    size_t reverse_box_count)
{
    const double projection = tangent.x * radial.x + tangent.y * radial.y +
	tangent.z * radial.z;
    tangent.x -= projection * radial.x;
    tangent.y -= projection * radial.y;
    tangent.z -= projection * radial.z;
    if (!tangent.Unitize())
	return false;
    ON_3dVector forward_direction(
	-radial.x + SPAN_JUNCTION_OBLIQUE_TANGENT_FRACTION * tangent.x,
	-radial.y + SPAN_JUNCTION_OBLIQUE_TANGENT_FRACTION * tangent.y,
	-radial.z + SPAN_JUNCTION_OBLIQUE_TANGENT_FRACTION * tangent.z);
    if (!forward_direction.Unitize())
	return false;
    const double radial_dot = radial.x * forward_direction.x +
	radial.y * forward_direction.y + radial.z * forward_direction.z;
    const double exit_distance = SPAN_JUNCTION_RAY_CLEARANCE -
	2.0 * SPAN_JUNCTION_RADIUS * radial_dot;
    if (!std::isfinite(exit_distance) ||
	exit_distance <= SPAN_JUNCTION_RAY_CLEARANCE)
	return false;

    point_t forward_origin;
    vect_t forward_ray;
    VSET(forward_origin, SPAN_JUNCTION_RADIUS * radial.x -
	SPAN_JUNCTION_RAY_CLEARANCE * forward_direction.x,
	SPAN_JUNCTION_RADIUS * radial.y -
	SPAN_JUNCTION_RAY_CLEARANCE * forward_direction.y,
	SPAN_JUNCTION_RADIUS * radial.z -
	SPAN_JUNCTION_RAY_CLEARANCE * forward_direction.z);
    VSET(forward_ray, forward_direction.x, forward_direction.y,
	forward_direction.z);
    struct rt_brep_shot_trace forward_trace = {};
    const int forward_hits = shoot_trace(solid, rtip, resource,
	forward_origin, forward_ray, forward_trace);
    const bool forward_valid = trace_has_span_grid_events(forward_trace,
	forward_hits, forward_box_count, reverse_box_count, false);
    if (!forward_valid)
	report_trace(forward_trace);

    point_t reverse_origin;
    vect_t reverse_ray;
    VSET(reverse_origin, SPAN_JUNCTION_RADIUS * radial.x +
	exit_distance * forward_direction.x,
	SPAN_JUNCTION_RADIUS * radial.y + exit_distance * forward_direction.y,
	SPAN_JUNCTION_RADIUS * radial.z + exit_distance * forward_direction.z);
    VSET(reverse_ray, -forward_direction.x, -forward_direction.y,
	-forward_direction.z);
    struct rt_brep_shot_trace reverse_trace = {};
    const int reverse_hits = shoot_trace(solid, rtip, resource,
	reverse_origin, reverse_ray, reverse_trace);
    const bool reverse_valid = trace_has_span_grid_events(reverse_trace,
	reverse_hits, forward_box_count, reverse_box_count, true);
    if (!reverse_valid)
	report_trace(reverse_trace);
    return forward_valid && reverse_valid;
}


static ON_3dVector
span_junction_tangent_axis(const ON_3dVector &radial)
{
    ON_3dVector tangent(1.0, 0.0, 0.0);
    if (fabs(radial.y) < fabs(radial.x) &&
	fabs(radial.y) <= fabs(radial.z))
	tangent = ON_3dVector(0.0, 1.0, 0.0);
    else if (fabs(radial.z) < fabs(radial.x) &&
	fabs(radial.z) < fabs(radial.y))
	tangent = ON_3dVector(0.0, 0.0, 1.0);
    return tangent;
}


static bool
trace_mixed_span_junction(struct soltab *solid, struct rt_i *rtip,
    struct resource *resource, const ON_3dVector &radial)
{
    return trace_oblique_span_grid(solid, rtip, resource, radial,
	span_junction_tangent_axis(radial), SPAN_JUNCTION_JUNCTION_BOXES,
	SPAN_JUNCTION_ADJACENT_BOXES);
}


static bool
trace_singleton_span_junction(struct soltab *solid, struct rt_i *rtip,
    struct resource *resource, const ON_3dVector &radial)
{
    const ON_3dVector tangent(1.0, 1.0, 1.0);
    return trace_oblique_span_grid(solid, rtip, resource, radial, tangent,
	SPAN_JUNCTION_JUNCTION_BOXES,
	SPAN_JUNCTION_SINGLETON_BOXES);
}


static bool
trace_adjacent_singleton_span(struct soltab *solid, struct rt_i *rtip,
    struct resource *resource, ON_Brep *brep, int face_index)
{
    ON_3dVector boundary_radial;
    if (!span_junction_boundary_radial(brep, face_index,
	SPAN_JUNCTION_BOUNDARY_TRANSVERSE_FRACTION, boundary_radial))
	return false;
    return trace_oblique_span_grid(solid, rtip, resource,
	boundary_radial, span_junction_tangent_axis(boundary_radial),
	SPAN_JUNCTION_ADJACENT_BOXES, SPAN_JUNCTION_SINGLETON_BOXES);
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    int face_index = SPAN_JUNCTION_DEFAULT_FACE_INDEX;
    if (argc == 2) {
	char *end = NULL;
	errno = 0;
	const long parsed_index = strtol(argv[1], &end, 10);
	if (errno || !end || end == argv[1] || *end ||
		parsed_index < 0 || parsed_index > INT_MAX)
	    return 1;
	face_index = (int)parsed_index;
    } else if (argc != 1) {
	return 1;
    }
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
	SPAN_JUNCTION_SURFACE_TREE_DEPTH)) {
	std::printf("FAIL: BREP surface tree setup\n");
	rt_i_destroy(rtip);
	return 1;
    }

    struct resource resource = {};
    rt_init_resource(&resource, 0, rtip);
    ON_Brep *brep = make_span_junction_sphere();
    if (!brep || !brep->IsSolid()) {
	std::printf("FAIL: span-junction sphere construction\n");
	delete brep;
	free_rtip(rtip, &resource);
	return 1;
    }
    struct soltab *solid = prepare_brep_solid(rtip, brep);
    if (!solid) {
	std::printf("FAIL: span-junction sphere preparation\n");
	delete brep;
	free_rtip(rtip, &resource);
	return 1;
    }

    ON_3dVector radial;
    if (!span_junction_radial(brep, face_index, radial)) {
	free_solid(solid);
	free_rtip(rtip, &resource);
	return 1;
    }

    point_t forward_origin;
    vect_t forward_direction;
    point_t reverse_origin;
    vect_t reverse_direction;
    VSET(forward_origin, SPAN_JUNCTION_ORIGIN_RADIUS * radial.x,
	SPAN_JUNCTION_ORIGIN_RADIUS * radial.y,
	SPAN_JUNCTION_ORIGIN_RADIUS * radial.z);
    VSET(forward_direction, -radial.x, -radial.y, -radial.z);
    VSET(reverse_origin, -SPAN_JUNCTION_ORIGIN_RADIUS * radial.x,
	-SPAN_JUNCTION_ORIGIN_RADIUS * radial.y,
	-SPAN_JUNCTION_ORIGIN_RADIUS * radial.z);
    VSET(reverse_direction, radial.x, radial.y, radial.z);
    const double distance_tolerance = std::max(1.0e-9,
	(double)rtip->rti_tol.dist);

    struct rt_brep_shot_trace forward_trace = {};
    const int forward_hits = shoot_trace(solid, rtip, &resource,
	forward_origin, forward_direction, forward_trace);
    const bool forward_valid = trace_has_expected_events(forward_trace,
	forward_hits, distance_tolerance);
    if (!forward_valid) {
	std::printf("FAIL: forward span-junction stream\n");
	report_trace(forward_trace);
    }

    struct rt_brep_shot_trace reverse_trace = {};
    const int reverse_hits = shoot_trace(solid, rtip, &resource,
	reverse_origin, reverse_direction, reverse_trace);
    const bool reverse_valid = trace_has_expected_events(reverse_trace,
	reverse_hits, distance_tolerance);
    if (!reverse_valid) {
	std::printf("FAIL: reverse span-junction stream\n");
	report_trace(reverse_trace);
    }

    const bool mixed_valid = face_index != SPAN_JUNCTION_DEFAULT_FACE_INDEX ||
	trace_mixed_span_junction(solid, rtip, &resource, radial);
    const bool singleton_valid =
	face_index != SPAN_JUNCTION_DEFAULT_FACE_INDEX ||
	trace_singleton_span_junction(solid, rtip, &resource, radial);
    const bool adjacent_singleton_valid =
	face_index != SPAN_JUNCTION_DEFAULT_FACE_INDEX ||
	trace_adjacent_singleton_span(solid, rtip, &resource, brep, face_index);

    free_solid(solid);
    free_rtip(rtip, &resource);
    return forward_valid && reverse_valid && mixed_valid && singleton_valid &&
	adjacent_singleton_valid ? 0 : 1;
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
