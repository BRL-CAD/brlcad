/*                  B R E P _ M I X E D _ P O L E . C P P
 * BRL-CAD
 *
 * Mixed collapsed-pole and regular-root BREP ray regression.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "bu/app.h"
#include "bu/malloc.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "../librt_private.h"


static const int MIXED_POLE_SURFACE_TREE_DEPTH = 2;
static const double MIXED_POLE_RADIUS = 10.0;
static const double MIXED_POLE_ENTRY_DISTANCE = 20.0;
static const double MIXED_POLE_EXIT_DISTANCE = 36.0;
static const double MIXED_POLE_STREAM_CENTER_DISTANCE = 60.0;
static const double MIXED_POLE_STREAM_ENTRY_DISTANCE =
    MIXED_POLE_STREAM_CENTER_DISTANCE - MIXED_POLE_RADIUS;
static const double MIXED_POLE_STREAM_EXIT_DISTANCE =
    MIXED_POLE_STREAM_CENTER_DISTANCE + MIXED_POLE_RADIUS;
static const double MIXED_POLE_REVERSE_STREAM_ORIGIN_DISTANCE =
    MIXED_POLE_STREAM_EXIT_DISTANCE + 2.0 * MIXED_POLE_RADIUS;
static const double MIXED_POLE_PREFIX_STREAM_FIRST_CENTER_DISTANCE =
    MIXED_POLE_STREAM_CENTER_DISTANCE;
static const double MIXED_POLE_PREFIX_STREAM_HALF_CHORD = 0.2;
static const double MIXED_POLE_PREFIX_STREAM_GAP = 0.05;
static const double MIXED_POLE_PREFIX_STREAM_RAY_OFFSET = std::sqrt(
    MIXED_POLE_RADIUS * MIXED_POLE_RADIUS -
    MIXED_POLE_PREFIX_STREAM_HALF_CHORD *
    MIXED_POLE_PREFIX_STREAM_HALF_CHORD);
static const double MIXED_POLE_PREFIX_STREAM_SECOND_CENTER_DISTANCE =
    MIXED_POLE_PREFIX_STREAM_FIRST_CENTER_DISTANCE +
    2.0 * MIXED_POLE_PREFIX_STREAM_HALF_CHORD +
    MIXED_POLE_PREFIX_STREAM_GAP;
static const double MIXED_POLE_PREFIX_STREAM_FIRST_ENTRY_DISTANCE =
    MIXED_POLE_PREFIX_STREAM_FIRST_CENTER_DISTANCE -
    MIXED_POLE_PREFIX_STREAM_HALF_CHORD;
static const double MIXED_POLE_PREFIX_STREAM_FIRST_EXIT_DISTANCE =
    MIXED_POLE_PREFIX_STREAM_FIRST_CENTER_DISTANCE +
    MIXED_POLE_PREFIX_STREAM_HALF_CHORD;
static const double MIXED_POLE_PREFIX_STREAM_SECOND_ENTRY_DISTANCE =
    MIXED_POLE_PREFIX_STREAM_FIRST_EXIT_DISTANCE +
    MIXED_POLE_PREFIX_STREAM_GAP;
static const double MIXED_POLE_PREFIX_STREAM_SECOND_EXIT_DISTANCE =
    MIXED_POLE_PREFIX_STREAM_SECOND_ENTRY_DISTANCE +
    2.0 * MIXED_POLE_PREFIX_STREAM_HALF_CHORD;
static const double MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE =
    MIXED_POLE_PREFIX_STREAM_SECOND_EXIT_DISTANCE +
    2.0 * MIXED_POLE_RADIUS;
static const double MIXED_POLE_ORTHOGONAL_RAY_COMPONENT = std::sqrt(0.5);
static const double MIXED_POLE_MULTI_POLE_DISTANCE = 60.0;
static const double MIXED_POLE_MULTI_TRANSLATION_DISTANCE =
    MIXED_POLE_MULTI_POLE_DISTANCE - MIXED_POLE_ENTRY_DISTANCE;
static const double MIXED_POLE_MULTI_EXIT_DISTANCE =
    MIXED_POLE_MULTI_POLE_DISTANCE + MIXED_POLE_EXIT_DISTANCE -
    MIXED_POLE_ENTRY_DISTANCE;
static const double MIXED_POLE_REVERSE_MULTI_ORIGIN_DISTANCE =
    MIXED_POLE_MULTI_EXIT_DISTANCE + 2.0 * MIXED_POLE_RADIUS;
static const double MIXED_POLE_AXIAL_ORIGIN_DISTANCE = 26.0;
static const double MIXED_POLE_AXIAL_POLE_ENTRY_DISTANCE =
    MIXED_POLE_AXIAL_ORIGIN_DISTANCE - MIXED_POLE_RADIUS;
static const double MIXED_POLE_AXIAL_POLE_EXIT_DISTANCE =
    MIXED_POLE_AXIAL_ORIGIN_DISTANCE + MIXED_POLE_RADIUS;
static const double MIXED_POLE_AXIAL_REGULAR_CENTER_DISTANCE = 40.0;
static const double MIXED_POLE_AXIAL_REGULAR_HALF_CHORD = 0.2;
static const double MIXED_POLE_AXIAL_REGULAR_GAP = 0.05;
static const double MIXED_POLE_AXIAL_REGULAR_RAY_OFFSET = std::sqrt(
    MIXED_POLE_RADIUS * MIXED_POLE_RADIUS -
    MIXED_POLE_AXIAL_REGULAR_HALF_CHORD *
    MIXED_POLE_AXIAL_REGULAR_HALF_CHORD);
static const double MIXED_POLE_AXIAL_REGULAR_SECOND_CENTER_DISTANCE =
    MIXED_POLE_AXIAL_REGULAR_CENTER_DISTANCE +
    2.0 * MIXED_POLE_AXIAL_REGULAR_HALF_CHORD +
    MIXED_POLE_AXIAL_REGULAR_GAP;
static const double MIXED_POLE_AXIAL_REGULAR_FIRST_ENTRY_DISTANCE =
    MIXED_POLE_AXIAL_ORIGIN_DISTANCE +
    MIXED_POLE_AXIAL_REGULAR_CENTER_DISTANCE -
    MIXED_POLE_AXIAL_REGULAR_HALF_CHORD;
static const double MIXED_POLE_AXIAL_REGULAR_FIRST_EXIT_DISTANCE =
    MIXED_POLE_AXIAL_ORIGIN_DISTANCE +
    MIXED_POLE_AXIAL_REGULAR_CENTER_DISTANCE +
    MIXED_POLE_AXIAL_REGULAR_HALF_CHORD;
static const double MIXED_POLE_AXIAL_REGULAR_SECOND_ENTRY_DISTANCE =
    MIXED_POLE_AXIAL_REGULAR_FIRST_EXIT_DISTANCE +
    MIXED_POLE_AXIAL_REGULAR_GAP;
static const double MIXED_POLE_AXIAL_REGULAR_SECOND_EXIT_DISTANCE =
    MIXED_POLE_AXIAL_REGULAR_SECOND_ENTRY_DISTANCE +
    2.0 * MIXED_POLE_AXIAL_REGULAR_HALF_CHORD;
static const double MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE =
    MIXED_POLE_AXIAL_REGULAR_SECOND_EXIT_DISTANCE +
    2.0 * MIXED_POLE_RADIUS;


static struct soltab *
prepare_solid(struct rt_i *rtip, struct rt_db_internal *intern, int type)
{
    struct soltab *stp = (struct soltab *)bu_calloc(1, sizeof(struct soltab),
	"mixed pole ray soltab");
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    struct directory *dp = (struct directory *)bu_calloc(1,
	sizeof(struct directory), "mixed pole ray directory");
    dp->d_magic = RT_DIR_MAGIC;
    dp->d_namep = (char *)"mixed_pole_ray.s";
    stp->st_dp = dp;
    stp->st_id = type;
    stp->st_meth = &OBJ[type];

    if (OBJ[type].ft_prep(stp, intern, rtip)) {
	if (stp->st_specific && stp->st_meth && stp->st_meth->ft_free)
	    stp->st_meth->ft_free(stp);
	bu_free((void *)stp->st_dp, "mixed pole ray directory");
	bu_free(stp, "mixed pole ray soltab");
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
    bu_free((void *)stp->st_dp, "mixed pole ray directory");
    bu_free(stp, "mixed pole ray soltab");
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
make_ellipsoid_brep(const point_t center, double radius,
    const struct bn_tol *tolerance)
{
    struct rt_ell_internal ellipsoid = {};
    ellipsoid.magic = RT_ELL_INTERNAL_MAGIC;
    VMOVE(ellipsoid.v, center);
    VSET(ellipsoid.a, radius, 0.0, 0.0);
    VSET(ellipsoid.b, 0.0, radius, 0.0);
    VSET(ellipsoid.c, 0.0, 0.0, radius);
    struct rt_db_internal ellipsoid_internal;
    RT_DB_INTERNAL_INIT(&ellipsoid_internal);
    ellipsoid_internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ellipsoid_internal.idb_type = ID_ELL;
    ellipsoid_internal.idb_meth = &OBJ[ID_ELL];
    ellipsoid_internal.idb_ptr = &ellipsoid;

    ON_Brep *brep = ON_Brep::New();
    if (!brep)
	return NULL;
    OBJ[ID_ELL].ft_brep(&brep, &ellipsoid_internal, tolerance);
    return brep;
}


static ON_Brep *
make_compound_brep(const ON_Brep *first, const ON_Brep *second)
{
    if (!first || !second)
	return NULL;
    ON_Brep *brep = ON_Brep::New();
    if (!brep)
	return NULL;
    brep->Append(*first);
    brep->Append(*second);
    return brep;
}


static ON_Brep *
make_compound_brep(const ON_Brep *first, const ON_Brep *second,
    const ON_Brep *third)
{
    ON_Brep *brep = make_compound_brep(first, second);
    if (!brep || !third) {
	delete brep;
	return NULL;
    }
    brep->Append(*third);
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


static bool
trace_source_boxes_owned(const struct rt_brep_shot_trace &trace)
{
    if (!trace.stored_surface_boxes)
	return false;
    for (size_t box_index = 0; box_index < trace.stored_surface_boxes;
	    ++box_index)
	if (trace.surface_boxes[box_index].disposition ==
	    RT_BREP_TRACE_BOX_UNRESOLVED)
	    return false;
    return true;
}


static bool
trace_has_expected_events(const struct rt_brep_shot_trace &trace,
    double singular_distance, int singular_direction, double regular_distance,
    int regular_direction, double distance_tolerance)
{
    if (trace.stored_physical_events != 2)
	return false;
    bool singular = false;
    bool regular = false;
    for (size_t event_index = 0;
	 event_index < trace.stored_physical_events; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	if (event.certificate == RT_BREP_TRACE_EVENT_SINGULAR_POLE) {
	    singular = event.source_kind ==
		RT_BREP_TRACE_EVENT_SOURCE_SINGULAR_POLE &&
		event.source_box_count > 0 &&
		event.direction == singular_direction &&
		fabs(event.dist - singular_distance) <=
		distance_tolerance;
	    continue;
	}
	if (event.certificate == RT_BREP_TRACE_EVENT_REGULAR_INTERIOR) {
	    regular = event.source_kind == RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT &&
		event.source_box_count > 0 &&
		event.direction == regular_direction &&
		fabs(event.dist - regular_distance) <=
		distance_tolerance;
	}
    }
    return singular && regular;
}


static bool
trace_is_prepared_mixed_result(const struct rt_brep_shot_trace &trace,
    int hits, double singular_distance, int singular_direction,
    double regular_distance, int regular_direction, double distance_tolerance)
{
    return hits == 2 && trace.prepared_production_selected == 1 &&
	trace.prepared_production_fallback == RT_BREP_PREPARED_FALLBACK_NONE &&
	trace.prepared_production_hits == 2 && trace.physical_event_complete == 1 &&
	trace.physical_event_material_segments == 1 &&
	!trace.physical_event_unresolved && !trace.physical_event_state_failures &&
	trace_source_boxes_owned(trace) && trace_has_expected_events(trace,
	singular_distance, singular_direction, regular_distance,
	regular_direction, distance_tolerance);
}


static bool
trace_has_expected_ordered_events(const struct rt_brep_shot_trace &trace,
    const double *expected_distance, const int *expected_certificate,
    const int *expected_direction, size_t expected_count,
    double distance_tolerance)
{
    if (!expected_distance || !expected_certificate || !expected_direction ||
	trace.stored_physical_events != expected_count)
	return false;
    for (size_t event_index = 0; event_index < expected_count;
	++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	const int expected_source = expected_certificate[event_index] ==
	    RT_BREP_TRACE_EVENT_SINGULAR_POLE ?
	    RT_BREP_TRACE_EVENT_SOURCE_SINGULAR_POLE :
	    RT_BREP_TRACE_EVENT_SOURCE_LOCAL_ROOT;
	if (event.certificate != expected_certificate[event_index] ||
		event.source_kind != expected_source || !event.source_box_count ||
		event.direction != expected_direction[event_index] ||
		fabs(event.dist - expected_distance[event_index]) >
		distance_tolerance)
	    return false;
    }
    return true;
}


static bool
trace_is_prepared_segment_result(const struct rt_brep_shot_trace &trace,
    int hits, bool events_valid, size_t segment_count)
{
    const int expected_hits = 2 * static_cast<int>(segment_count);
    return hits == expected_hits && trace.prepared_production_selected == 1 &&
	trace.prepared_production_fallback == RT_BREP_PREPARED_FALLBACK_NONE &&
	trace.prepared_production_hits == expected_hits &&
	trace.physical_event_complete == 1 &&
	trace.physical_event_material_segments == segment_count &&
	!trace.physical_event_unresolved && !trace.physical_event_state_failures &&
	trace_source_boxes_owned(trace) && events_valid;
}


static bool
trace_has_expected_stream_events(const struct rt_brep_shot_trace &trace,
    bool reverse, double distance_tolerance)
{
    const double forward_distance[4] = {
	MIXED_POLE_ENTRY_DISTANCE, MIXED_POLE_EXIT_DISTANCE,
	MIXED_POLE_STREAM_ENTRY_DISTANCE, MIXED_POLE_STREAM_EXIT_DISTANCE
    };
    const double reverse_distance[4] = {
	MIXED_POLE_REVERSE_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_STREAM_EXIT_DISTANCE,
	MIXED_POLE_REVERSE_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_STREAM_ENTRY_DISTANCE,
	MIXED_POLE_REVERSE_STREAM_ORIGIN_DISTANCE - MIXED_POLE_EXIT_DISTANCE,
	MIXED_POLE_REVERSE_STREAM_ORIGIN_DISTANCE - MIXED_POLE_ENTRY_DISTANCE
    };
    const int forward_certificate[4] = {
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR
    };
    const int reverse_certificate[4] = {
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE
    };
    const int expected_direction[4] = {
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING,
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING
    };
    return trace_has_expected_ordered_events(trace,
	reverse ? reverse_distance : forward_distance,
	reverse ? reverse_certificate : forward_certificate,
	expected_direction, 4, distance_tolerance);
}


static bool
trace_is_prepared_mixed_stream_result(const struct rt_brep_shot_trace &trace,
    int hits, bool reverse, double distance_tolerance)
{
    return trace_is_prepared_segment_result(trace, hits,
	trace_has_expected_stream_events(trace, reverse, distance_tolerance), 2);
}


static bool
trace_has_expected_prefix_stream_events(
    const struct rt_brep_shot_trace &trace, bool reverse,
    double distance_tolerance)
{
    const double forward_distance[6] = {
	MIXED_POLE_ENTRY_DISTANCE,
	MIXED_POLE_EXIT_DISTANCE,
	MIXED_POLE_PREFIX_STREAM_FIRST_ENTRY_DISTANCE,
	MIXED_POLE_PREFIX_STREAM_FIRST_EXIT_DISTANCE,
	MIXED_POLE_PREFIX_STREAM_SECOND_ENTRY_DISTANCE,
	MIXED_POLE_PREFIX_STREAM_SECOND_EXIT_DISTANCE
    };
    const double reverse_distance[6] = {
	MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_PREFIX_STREAM_SECOND_EXIT_DISTANCE,
	MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_PREFIX_STREAM_SECOND_ENTRY_DISTANCE,
	MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_PREFIX_STREAM_FIRST_EXIT_DISTANCE,
	MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_PREFIX_STREAM_FIRST_ENTRY_DISTANCE,
	MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_EXIT_DISTANCE,
	MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE -
	MIXED_POLE_ENTRY_DISTANCE
    };
    const int forward_certificate[6] = {
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR
    };
    const int reverse_certificate[6] = {
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE
    };
    const int expected_direction[6] = {
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING,
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING,
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING
    };
    return trace_has_expected_ordered_events(trace,
	reverse ? reverse_distance : forward_distance,
	reverse ? reverse_certificate : forward_certificate,
	expected_direction, 6, distance_tolerance);
}


static bool
trace_is_prepared_prefix_stream_result(const struct rt_brep_shot_trace &trace,
    int hits, bool reverse, double distance_tolerance)
{
    return trace.physical_event_singular_attempts == 1 &&
	trace.physical_event_singular_candidates == 1 &&
	trace.physical_event_singular_certified == 1 &&
	!trace.physical_event_singular_failures &&
	trace.physical_event_regular_stream_attempts == 1 &&
	trace.physical_event_regular_stream_certified == 1 &&
	trace.physical_event_regular_stream_components == 5 &&
	trace.physical_event_regular_stream_boxes ==
	trace.stored_surface_boxes &&
	trace.physical_event_regular_stream_roots == trace.stored_local_roots &&
	!trace.physical_event_regular_stream_failure_stage &&
	trace_is_prepared_segment_result(trace, hits,
	trace_has_expected_prefix_stream_events(trace, reverse, distance_tolerance),
	3);
}


static bool
trace_has_expected_multi_pole_events(const struct rt_brep_shot_trace &trace,
    bool reverse, double distance_tolerance)
{
    const double forward_distance[4] = {
	MIXED_POLE_ENTRY_DISTANCE, MIXED_POLE_EXIT_DISTANCE,
	MIXED_POLE_MULTI_POLE_DISTANCE, MIXED_POLE_MULTI_EXIT_DISTANCE
    };
    const double reverse_distance[4] = {
	MIXED_POLE_REVERSE_MULTI_ORIGIN_DISTANCE -
	MIXED_POLE_MULTI_EXIT_DISTANCE,
	MIXED_POLE_REVERSE_MULTI_ORIGIN_DISTANCE -
	MIXED_POLE_MULTI_POLE_DISTANCE,
	MIXED_POLE_REVERSE_MULTI_ORIGIN_DISTANCE - MIXED_POLE_EXIT_DISTANCE,
	MIXED_POLE_REVERSE_MULTI_ORIGIN_DISTANCE - MIXED_POLE_ENTRY_DISTANCE
    };
    const int forward_certificate[4] = {
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR
    };
    const int reverse_certificate[4] = {
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE
    };
    const int expected_direction[4] = {
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING,
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING
    };
    return trace_has_expected_ordered_events(trace,
	reverse ? reverse_distance : forward_distance,
	reverse ? reverse_certificate : forward_certificate,
	expected_direction, 4, distance_tolerance);
}


static bool
trace_is_prepared_multi_pole_result(const struct rt_brep_shot_trace &trace,
    int hits, bool reverse, double distance_tolerance)
{
    return trace_is_prepared_segment_result(trace, hits,
	trace_has_expected_multi_pole_events(trace, reverse, distance_tolerance),
	2);
}


static bool
trace_has_expected_axial_pole_events(const struct rt_brep_shot_trace &trace,
    bool reverse, double distance_tolerance)
{
    const double forward_distance[6] = {
	MIXED_POLE_AXIAL_POLE_ENTRY_DISTANCE,
	MIXED_POLE_AXIAL_POLE_EXIT_DISTANCE,
	MIXED_POLE_AXIAL_REGULAR_FIRST_ENTRY_DISTANCE,
	MIXED_POLE_AXIAL_REGULAR_FIRST_EXIT_DISTANCE,
	MIXED_POLE_AXIAL_REGULAR_SECOND_ENTRY_DISTANCE,
	MIXED_POLE_AXIAL_REGULAR_SECOND_EXIT_DISTANCE
    };
    const double reverse_distance[6] = {
	MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE -
	MIXED_POLE_AXIAL_REGULAR_SECOND_EXIT_DISTANCE,
	MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE -
	MIXED_POLE_AXIAL_REGULAR_SECOND_ENTRY_DISTANCE,
	MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE -
	MIXED_POLE_AXIAL_REGULAR_FIRST_EXIT_DISTANCE,
	MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE -
	MIXED_POLE_AXIAL_REGULAR_FIRST_ENTRY_DISTANCE,
	MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE -
	MIXED_POLE_AXIAL_POLE_EXIT_DISTANCE,
	MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE -
	MIXED_POLE_AXIAL_POLE_ENTRY_DISTANCE
    };
    const int forward_certificate[6] = {
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR
    };
    const int reverse_certificate[6] = {
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_REGULAR_INTERIOR,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE,
	RT_BREP_TRACE_EVENT_SINGULAR_POLE
    };
    const int expected_direction[6] = {
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING,
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING,
	RT_BREP_TRACE_ENTERING, RT_BREP_TRACE_LEAVING
    };
    return trace_has_expected_ordered_events(trace,
	reverse ? reverse_distance : forward_distance,
	reverse ? reverse_certificate : forward_certificate,
	expected_direction, 6, distance_tolerance);
}


static bool
trace_is_prepared_axial_pole_result(const struct rt_brep_shot_trace &trace,
    int hits, bool reverse, double distance_tolerance)
{
    return trace.physical_event_singular_attempts == 1 &&
	trace.physical_event_singular_candidates == 2 &&
	trace.physical_event_singular_certified == 1 &&
	!trace.physical_event_singular_failures &&
	trace.physical_event_regular_stream_attempts == 1 &&
	trace.physical_event_regular_stream_certified == 1 &&
	trace.physical_event_regular_stream_components == 4 &&
	trace.physical_event_regular_stream_boxes ==
	trace.stored_surface_boxes &&
	trace.physical_event_regular_stream_roots == trace.stored_local_roots &&
	!trace.physical_event_regular_stream_failure_stage &&
	trace_is_prepared_segment_result(trace, hits,
	trace_has_expected_axial_pole_events(trace, reverse, distance_tolerance),
	3);
}


static void
report_surface_spans(const ON_Brep *brep)
{
    if (!brep)
	return;
    size_t span_index = 0;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep->m_F[face_index];
	const ON_Surface *surface = face.SurfaceOf();
	ON_NurbsSurface nurbs;
	const int nurb_form_status = surface ? surface->GetNurbForm(nurbs) : 0;
	if (nurb_form_status < 1)
	    continue;
	std::printf("  face=%d NURBS form=%d order=%d/%d singular sides=%d/%d/%d/%d\n",
	    face_index, nurb_form_status, nurbs.m_order[0], nurbs.m_order[1], surface->IsSingular(0),
	    surface->IsSingular(1), surface->IsSingular(2),
	    surface->IsSingular(3));
	const ON_Interval domain[2] = {nurbs.Domain(0), nurbs.Domain(1)};
	for (int u_span = 0;
	     u_span <= nurbs.m_cv_count[0] - nurbs.m_order[0]; ++u_span) {
	    const double u_lower =
		nurbs.m_knot[0][u_span + nurbs.m_order[0] - 2];
	    const double u_upper =
		nurbs.m_knot[0][u_span + nurbs.m_order[0] - 1];
	    if (!(u_lower < u_upper))
		continue;
	    for (int v_span = 0;
		 v_span <= nurbs.m_cv_count[1] - nurbs.m_order[1]; ++v_span) {
		const double v_lower =
		    nurbs.m_knot[1][v_span + nurbs.m_order[1] - 2];
		const double v_upper =
		    nurbs.m_knot[1][v_span + nurbs.m_order[1] - 1];
		if (!(v_lower < v_upper))
		    continue;
		unsigned int singular_mask = 0;
		if (v_lower == domain[1].Min() && surface->IsSingular(0) &&
		    nurbs.IsSingular(0))
		    singular_mask |= 1u << 0;
		if (u_upper == domain[0].Max() && surface->IsSingular(1) &&
		    nurbs.IsSingular(1))
		    singular_mask |= 1u << 1;
		if (v_upper == domain[1].Max() && surface->IsSingular(2) &&
		    nurbs.IsSingular(2))
		    singular_mask |= 1u << 2;
		if (u_lower == domain[0].Min() && surface->IsSingular(3) &&
		    nurbs.IsSingular(3))
		    singular_mask |= 1u << 3;
		std::printf("  prepared span[%zu] face=%d uv=[%.17g,%.17g]x[%.17g,%.17g] mask=%u\n",
		    span_index++, face_index, u_lower, u_upper, v_lower,
		    v_upper, singular_mask);
	    }
	}
    }
}


static void
report_trace(const struct rt_brep_shot_trace &trace)
{
    size_t unresolved_boxes = 0;
    for (size_t box_index = 0; box_index < trace.stored_surface_boxes;
	 ++box_index)
	unresolved_boxes += trace.surface_boxes[box_index].disposition ==
	    RT_BREP_TRACE_BOX_UNRESOLVED ? 1 : 0;
    std::printf("prepared selected/fallback/hits=%zu/%d/%zu "
	"surface spans/candidate/excluded=%zu/%zu/%zu "
	"boxes/unresolved/roots/singular/events=%zu/%zu/%zu/%zu/%zu "
	"singular attempts/certified/failures=%zu/%zu/%zu workspace=%zu\n",
	trace.prepared_production_selected, trace.prepared_production_fallback,
	trace.prepared_production_hits, trace.prepared_surface_spans,
	trace.candidate_surface_spans, trace.excluded_surface_spans,
	trace.stored_surface_boxes,
	unresolved_boxes, trace.stored_local_roots,
	trace.stored_surface_singular_spans, trace.stored_physical_events,
	trace.physical_event_singular_attempts,
	trace.physical_event_singular_certified,
	trace.physical_event_singular_failures, trace.surface_workspace_exhausted);
    std::printf("  singular surface attempts/collapsed/line/deflated/resolved/signed/failures=%zu/%zu/%zu/%zu/%zu/%zu/%zu "
	"isolated/krawczyk/local-candidates/folds=%zu/%zu/%zu/%zu\n",
	trace.surface_singular_attempts, trace.surface_singular_collapsed_sides,
	trace.surface_singular_line_candidates,
	trace.surface_singular_deflated_exclusions,
	trace.surface_singular_resolved_spans,
	trace.surface_singular_determinant_signed,
	trace.surface_singular_span_failures, trace.surface_isolated_boxes,
	trace.surface_krawczyk_boxes, trace.local_root_candidates,
	trace.stored_surface_fold_roots);
    for (size_t root_index = 0; root_index < trace.stored_local_roots;
	 ++root_index) {
	const struct rt_brep_trace_local_root &root =
	    trace.local_roots[root_index];
	std::printf("  root[%zu] face/span=%d/%d uv=(%.17g,%.17g) t=%.17g "
	    "class/trim/direction=%d/%d/%d\n",
	    root_index, root.face_index, root.span_index, root.uv[0], root.uv[1], root.dist,
	    root.hit_class, root.trim_status, root.direction);
    }
    for (size_t singular_index = 0;
	 singular_index < trace.stored_surface_singular_spans; ++singular_index) {
	const struct rt_brep_trace_singular_span &singular =
	    trace.surface_singular_spans[singular_index];
	std::printf("  singular[%zu] face/span/side=%d/%d/%d uv=(%.17g,%.17g) "
	    "t=%.17g direction=%d\n",
	    singular_index, singular.face_index, singular.span_index,
	    singular.side, singular.uv[0], singular.uv[1], singular.dist,
	    singular.direction);
    }
    for (size_t box_index = 0; box_index < trace.stored_surface_boxes;
	 ++box_index) {
	const struct rt_brep_trace_surface_box &box =
	    trace.surface_boxes[box_index];
	bool first = true;
	for (size_t previous = 0; previous < box_index; ++previous)
	    if (trace.surface_boxes[previous].face_index == box.face_index &&
		trace.surface_boxes[previous].span_index == box.span_index)
		first = false;
	if (!first)
	    continue;
	size_t boxes = 0;
	size_t unresolved = 0;
	fastf_t u_minimum = box.uv_min[0];
	fastf_t u_maximum = box.uv_max[0];
	fastf_t v_minimum = box.uv_min[1];
	fastf_t v_maximum = box.uv_max[1];
	fastf_t t_minimum = box.t_min;
	fastf_t t_maximum = box.t_max;
	for (size_t group_index = box_index;
	     group_index < trace.stored_surface_boxes; ++group_index) {
	    const struct rt_brep_trace_surface_box &candidate =
		trace.surface_boxes[group_index];
	    if (candidate.face_index != box.face_index ||
		candidate.span_index != box.span_index)
		continue;
	    boxes++;
	    unresolved += candidate.disposition == RT_BREP_TRACE_BOX_UNRESOLVED ?
		1 : 0;
	    u_minimum = std::min(u_minimum, candidate.uv_min[0]);
	    u_maximum = std::max(u_maximum, candidate.uv_max[0]);
	    v_minimum = std::min(v_minimum, candidate.uv_min[1]);
	    v_maximum = std::max(v_maximum, candidate.uv_max[1]);
	    t_minimum = std::min(t_minimum, candidate.t_min);
	    t_maximum = std::max(t_maximum, candidate.t_max);
	}
	std::printf("  boxes face/span=%d/%d count/unresolved=%zu/%zu "
	    "uv=[%.17g,%.17g]x[%.17g,%.17g] t=[%.17g,%.17g]\n",
	    box.face_index, box.span_index, boxes, unresolved, u_minimum,
	    u_maximum, v_minimum, v_maximum, t_minimum, t_maximum);
    }
    for (size_t event_index = 0;
	 event_index < trace.stored_physical_events; ++event_index) {
	const struct rt_brep_trace_physical_event &event =
	    trace.physical_events[event_index];
	std::printf("  event[%zu] face/span=%d/%d t=%.17g certificate/source/direction=%d/%d/%d\n",
	    event_index, event.face_index, event.span_index, event.dist,
	    event.certificate, event.source_kind, event.direction);
    }
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
	MIXED_POLE_SURFACE_TREE_DEPTH)) {
	std::printf("FAIL: BREP surface tree setup\n");
	rt_i_destroy(rtip);
	return 1;
    }

    struct resource resource = {};
    rt_init_resource(&resource, 0, rtip);
    const point_t brep_center = {0.0, 0.0, 0.0};
    ON_Brep *brep = make_ellipsoid_brep(brep_center, MIXED_POLE_RADIUS,
	&rtip->rti_tol);
    if (!brep) {
	std::printf("FAIL: ellipsoid BREP conversion\n");
	free_rtip(rtip, &resource);
	return 1;
    }
    struct soltab *brep_solid = prepare_brep_solid(rtip, brep);
    if (!brep_solid) {
	std::printf("FAIL: ellipsoid BREP preparation\n");
	delete brep;
	free_rtip(rtip, &resource);
	return 1;
    }

    const point_t origin = {-8.48528137423857, -8.48528137423857, 26.0};
    const vect_t direction = {0.424264068711929, 0.424264068711929, -0.8};
    struct rt_brep_shot_trace trace = {};
    const int hits = shoot_trace(brep_solid, rtip, &resource, origin,
	direction, trace);
    const double distance_tolerance = std::max(1.0e-9,
	(double)rtip->rti_tol.dist);

    const bool forward_valid = trace_is_prepared_mixed_result(trace, hits,
	MIXED_POLE_ENTRY_DISTANCE, RT_BREP_TRACE_ENTERING,
	MIXED_POLE_EXIT_DISTANCE, RT_BREP_TRACE_LEAVING, distance_tolerance);
    if (!forward_valid) {
	std::printf("FAIL: forward mixed pole and regular BREP prepared trace\n");
	report_trace(trace);
	report_surface_spans(brep);
    }

    point_t reverse_origin;
    vect_t reverse_direction;
    VJOIN1(reverse_origin, origin, MIXED_POLE_ENTRY_DISTANCE +
	MIXED_POLE_EXIT_DISTANCE, direction);
    VREVERSE(reverse_direction, direction);
    struct rt_brep_shot_trace reverse_trace = {};
    const int reverse_hits = shoot_trace(brep_solid, rtip, &resource,
	reverse_origin, reverse_direction, reverse_trace);
    const bool reverse_valid = trace_is_prepared_mixed_result(reverse_trace,
	reverse_hits, MIXED_POLE_EXIT_DISTANCE, RT_BREP_TRACE_LEAVING,
	MIXED_POLE_ENTRY_DISTANCE, RT_BREP_TRACE_ENTERING, distance_tolerance);
    if (!reverse_valid) {
	std::printf("FAIL: reverse mixed pole and regular BREP prepared trace\n");
	report_trace(reverse_trace);
	report_surface_spans(brep);
    }

    point_t stream_center;
    VJOIN1(stream_center, origin, MIXED_POLE_STREAM_CENTER_DISTANCE,
	direction);
    ON_Brep *stream_sphere = make_ellipsoid_brep(stream_center,
	MIXED_POLE_RADIUS, &rtip->rti_tol);
    ON_Brep *stream_brep = make_compound_brep(brep, stream_sphere);
    bool stream_valid = stream_brep && stream_brep->IsSolid();
    delete stream_sphere;
    if (!stream_valid) {
	std::printf("FAIL: mixed pole regular stream BREP construction\n");
	delete stream_brep;
    } else {
	struct soltab *stream_solid = prepare_brep_solid(rtip, stream_brep);
	if (!stream_solid) {
	    std::printf("FAIL: mixed pole regular stream BREP preparation\n");
	    delete stream_brep;
	    stream_valid = false;
	} else {
	    struct rt_brep_shot_trace stream_trace = {};
	    const int stream_hits = shoot_trace(stream_solid, rtip, &resource,
		origin, direction, stream_trace);
	    const bool stream_forward_valid =
		trace_is_prepared_mixed_stream_result(stream_trace, stream_hits,
		false, distance_tolerance);
	    if (!stream_forward_valid) {
		std::printf("FAIL: forward mixed pole regular BREP prepared stream\n");
		report_trace(stream_trace);
		report_surface_spans(stream_brep);
	    }
	    point_t reverse_stream_origin;
	    VJOIN1(reverse_stream_origin, origin,
		MIXED_POLE_REVERSE_STREAM_ORIGIN_DISTANCE, direction);
	    struct rt_brep_shot_trace reverse_stream_trace = {};
	    const int reverse_stream_hits = shoot_trace(stream_solid, rtip,
		&resource, reverse_stream_origin, reverse_direction,
		reverse_stream_trace);
	    const bool stream_reverse_valid =
		trace_is_prepared_mixed_stream_result(reverse_stream_trace,
		reverse_stream_hits, true, distance_tolerance);
	    if (!stream_reverse_valid) {
		std::printf("FAIL: reverse mixed pole regular BREP prepared stream\n");
		report_trace(reverse_stream_trace);
		report_surface_spans(stream_brep);
	    }
	    stream_valid = stream_forward_valid && stream_reverse_valid;
	    free_solid(stream_solid);
	}
    }

    const vect_t prefix_stream_offset_direction = {
	MIXED_POLE_ORTHOGONAL_RAY_COMPONENT,
	-MIXED_POLE_ORTHOGONAL_RAY_COMPONENT, 0.0
    };
    point_t prefix_stream_first_axis_center;
    VJOIN1(prefix_stream_first_axis_center, origin,
	MIXED_POLE_PREFIX_STREAM_FIRST_CENTER_DISTANCE, direction);
    point_t prefix_stream_first_center;
    VJOIN1(prefix_stream_first_center, prefix_stream_first_axis_center,
	MIXED_POLE_PREFIX_STREAM_RAY_OFFSET, prefix_stream_offset_direction);
    point_t prefix_stream_second_axis_center;
    VJOIN1(prefix_stream_second_axis_center, origin,
	MIXED_POLE_PREFIX_STREAM_SECOND_CENTER_DISTANCE, direction);
    point_t prefix_stream_second_center;
    VJOIN1(prefix_stream_second_center, prefix_stream_second_axis_center,
	-MIXED_POLE_PREFIX_STREAM_RAY_OFFSET, prefix_stream_offset_direction);
    ON_Brep *prefix_stream_first_sphere = make_ellipsoid_brep(
	prefix_stream_first_center, MIXED_POLE_RADIUS, &rtip->rti_tol);
    ON_Brep *prefix_stream_second_sphere = make_ellipsoid_brep(
	prefix_stream_second_center, MIXED_POLE_RADIUS, &rtip->rti_tol);
    ON_Brep *prefix_stream_brep = make_compound_brep(brep,
	prefix_stream_first_sphere, prefix_stream_second_sphere);
    bool prefix_stream_valid = prefix_stream_brep &&
	prefix_stream_brep->IsSolid();
    delete prefix_stream_first_sphere;
    delete prefix_stream_second_sphere;
    if (!prefix_stream_valid) {
	std::printf("FAIL: mixed-ledger BREP construction\n");
	delete prefix_stream_brep;
    } else {
	struct soltab *prefix_stream_solid = prepare_brep_solid(rtip,
	    prefix_stream_brep);
	if (!prefix_stream_solid) {
	    std::printf("FAIL: mixed-ledger BREP preparation\n");
	    delete prefix_stream_brep;
	    prefix_stream_valid = false;
	} else {
	    struct rt_brep_shot_trace prefix_stream_trace = {};
	    const int prefix_stream_hits = shoot_trace(prefix_stream_solid, rtip,
		&resource, origin, direction, prefix_stream_trace);
	    const bool prefix_stream_forward_valid =
		trace_is_prepared_prefix_stream_result(prefix_stream_trace,
		prefix_stream_hits, false, distance_tolerance);
	    if (!prefix_stream_forward_valid) {
		std::printf("FAIL: forward mixed-ledger BREP prepared stream\n");
		report_trace(prefix_stream_trace);
		report_surface_spans(prefix_stream_brep);
	    }
	    point_t reverse_prefix_stream_origin;
	    VJOIN1(reverse_prefix_stream_origin, origin,
		MIXED_POLE_REVERSE_PREFIX_STREAM_ORIGIN_DISTANCE, direction);
	    struct rt_brep_shot_trace reverse_prefix_stream_trace = {};
	    const int reverse_prefix_stream_hits = shoot_trace(
		prefix_stream_solid, rtip, &resource, reverse_prefix_stream_origin,
		reverse_direction, reverse_prefix_stream_trace);
	    const bool prefix_stream_reverse_valid =
		trace_is_prepared_prefix_stream_result(reverse_prefix_stream_trace,
		reverse_prefix_stream_hits, true, distance_tolerance);
	    if (!prefix_stream_reverse_valid) {
		std::printf("FAIL: reverse mixed-ledger BREP prepared stream\n");
		report_trace(reverse_prefix_stream_trace);
		report_surface_spans(prefix_stream_brep);
	    }
	    prefix_stream_valid = prefix_stream_forward_valid &&
		prefix_stream_reverse_valid;
	    free_solid(prefix_stream_solid);
	}
    }

    point_t multi_pole_center;
    VJOIN1(multi_pole_center, brep_center,
	MIXED_POLE_MULTI_TRANSLATION_DISTANCE, direction);
    ON_Brep *multi_pole_sphere = make_ellipsoid_brep(multi_pole_center,
	MIXED_POLE_RADIUS, &rtip->rti_tol);
    ON_Brep *multi_pole_brep = make_compound_brep(brep, multi_pole_sphere);
    bool multi_pole_valid = multi_pole_brep && multi_pole_brep->IsSolid();
    delete multi_pole_sphere;
    if (!multi_pole_valid) {
	std::printf("FAIL: multi-pole BREP construction\n");
	delete multi_pole_brep;
    } else {
	struct soltab *multi_pole_solid = prepare_brep_solid(rtip,
	    multi_pole_brep);
	if (!multi_pole_solid) {
	    std::printf("FAIL: multi-pole BREP preparation\n");
	    delete multi_pole_brep;
	    multi_pole_valid = false;
	} else {
	    struct rt_brep_shot_trace multi_pole_trace = {};
	    const int multi_pole_hits = shoot_trace(multi_pole_solid, rtip,
		&resource, origin, direction, multi_pole_trace);
	    const bool multi_pole_forward_valid =
		trace_is_prepared_multi_pole_result(multi_pole_trace,
		multi_pole_hits, false, distance_tolerance);
	    if (!multi_pole_forward_valid) {
		std::printf("FAIL: forward multi-pole BREP prepared stream\n");
		report_trace(multi_pole_trace);
		report_surface_spans(multi_pole_brep);
	    }
	    point_t reverse_multi_pole_origin;
	    VJOIN1(reverse_multi_pole_origin, origin,
		MIXED_POLE_REVERSE_MULTI_ORIGIN_DISTANCE, direction);
	    struct rt_brep_shot_trace reverse_multi_pole_trace = {};
	    const int reverse_multi_pole_hits = shoot_trace(multi_pole_solid,
		rtip, &resource, reverse_multi_pole_origin, reverse_direction,
		reverse_multi_pole_trace);
	    const bool multi_pole_reverse_valid =
		trace_is_prepared_multi_pole_result(reverse_multi_pole_trace,
		reverse_multi_pole_hits, true, distance_tolerance);
	    if (!multi_pole_reverse_valid) {
		std::printf("FAIL: reverse multi-pole BREP prepared stream\n");
		report_trace(reverse_multi_pole_trace);
		report_surface_spans(multi_pole_brep);
	    }
	    multi_pole_valid = multi_pole_forward_valid &&
		multi_pole_reverse_valid;
	    free_solid(multi_pole_solid);
	}
    }

    const vect_t axial_direction = {0.0, 0.0, -1.0};
    const vect_t axial_offset_direction = {0.6, 0.8, 0.0};
    point_t axial_origin;
    VJOIN1(axial_origin, brep_center, -MIXED_POLE_AXIAL_ORIGIN_DISTANCE,
	axial_direction);
    point_t axial_regular_axis_center;
    VJOIN1(axial_regular_axis_center, brep_center,
	MIXED_POLE_AXIAL_REGULAR_CENTER_DISTANCE, axial_direction);
    point_t axial_regular_first_center;
    VJOIN1(axial_regular_first_center, axial_regular_axis_center,
	MIXED_POLE_AXIAL_REGULAR_RAY_OFFSET, axial_offset_direction);
    point_t axial_regular_second_axis_center;
    VJOIN1(axial_regular_second_axis_center, brep_center,
	MIXED_POLE_AXIAL_REGULAR_SECOND_CENTER_DISTANCE, axial_direction);
    point_t axial_regular_second_center;
    VJOIN1(axial_regular_second_center, axial_regular_second_axis_center,
	-MIXED_POLE_AXIAL_REGULAR_RAY_OFFSET, axial_offset_direction);
    ON_Brep *axial_regular_first_sphere = make_ellipsoid_brep(
	axial_regular_first_center, MIXED_POLE_RADIUS, &rtip->rti_tol);
    ON_Brep *axial_regular_second_sphere = make_ellipsoid_brep(
	axial_regular_second_center, MIXED_POLE_RADIUS, &rtip->rti_tol);
    ON_Brep *axial_pole_brep = make_compound_brep(brep,
	axial_regular_first_sphere, axial_regular_second_sphere);
    bool axial_pole_valid = axial_pole_brep && axial_pole_brep->IsSolid();
    delete axial_regular_first_sphere;
    delete axial_regular_second_sphere;
    if (!axial_pole_valid) {
	std::printf("FAIL: axial pole BREP construction\n");
	delete axial_pole_brep;
    } else {
	struct soltab *axial_pole_solid = prepare_brep_solid(rtip,
	    axial_pole_brep);
	if (!axial_pole_solid) {
	    std::printf("FAIL: axial pole BREP preparation\n");
	    delete axial_pole_brep;
	    axial_pole_valid = false;
	} else {
	    struct rt_brep_shot_trace axial_pole_trace = {};
	    const int axial_pole_hits = shoot_trace(axial_pole_solid, rtip,
		&resource, axial_origin, axial_direction, axial_pole_trace);
	    const bool axial_pole_forward_valid =
		trace_is_prepared_axial_pole_result(axial_pole_trace,
		axial_pole_hits, false, distance_tolerance);
	    if (!axial_pole_forward_valid) {
		std::printf("FAIL: forward axial pole BREP prepared stream\n");
		report_trace(axial_pole_trace);
		report_surface_spans(axial_pole_brep);
	    }
	    point_t reverse_axial_origin;
	    VJOIN1(reverse_axial_origin, axial_origin,
		MIXED_POLE_AXIAL_REVERSE_ORIGIN_DISTANCE, axial_direction);
	    vect_t reverse_axial_direction;
	    VREVERSE(reverse_axial_direction, axial_direction);
	    struct rt_brep_shot_trace reverse_axial_pole_trace = {};
	    const int reverse_axial_pole_hits = shoot_trace(axial_pole_solid,
		rtip, &resource, reverse_axial_origin, reverse_axial_direction,
		reverse_axial_pole_trace);
	    const bool axial_pole_reverse_valid =
		trace_is_prepared_axial_pole_result(reverse_axial_pole_trace,
		reverse_axial_pole_hits, true, distance_tolerance);
	    if (!axial_pole_reverse_valid) {
		std::printf("FAIL: reverse axial pole BREP prepared stream\n");
		report_trace(reverse_axial_pole_trace);
		report_surface_spans(axial_pole_brep);
	    }
	    axial_pole_valid = axial_pole_forward_valid &&
		axial_pole_reverse_valid;
	    free_solid(axial_pole_solid);
	}
    }

    free_solid(brep_solid);
    free_rtip(rtip, &resource);
    return forward_valid && reverse_valid && stream_valid &&
	prefix_stream_valid && multi_pole_valid && axial_pole_valid ? 0 : 1;
}
