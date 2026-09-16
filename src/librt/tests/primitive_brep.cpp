/*               P R I M I T I V E _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file primitive_brep.cpp
 *
 * Verify solid primitive-to-BRep conversions produce closed oriented
 * manifolds with genuinely shared topology.
 */

#include "common.h"

#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "wdb.h"


static int
check_brep(const char *name, struct rt_db_internal *intern, int expected_faces)
{
    struct bn_tol tol = BN_TOL_INIT_TOL;
    ON_Brep *brep = ON_Brep::New();
    intern->idb_meth->ft_brep(&brep, intern, &tol);

    ON_wString messages;
    ON_TextLog log(messages);
    bool oriented = false;
    bool boundary = true;
    const bool valid = brep && brep->IsValid(&log);
    const bool solid = brep && brep->IsSolid();
    const bool manifold = brep && brep->IsManifold(&oriented, &boundary);
    bool paired_edges = brep && brep->m_E.Count() > 0;
    if (brep) {
	for (int i = 0; i < brep->m_E.Count(); ++i)
	    paired_edges = paired_edges && brep->m_E[i].m_ti.Count() == 2;
    }

    int result = 0;
    if (!valid || !solid || !manifold || !oriented || boundary ||
	!paired_edges || brep->m_F.Count() != expected_faces) {
	ON_String text(messages);
	bu_log("ERROR: %s BRep is not a closed oriented manifold "
	    "(valid=%d solid=%d manifold=%d oriented=%d boundary=%d "
	    "faces=%d edges=%d paired_edges=%d):\n%s",
	    name, valid, solid, manifold, oriented, boundary,
	    brep ? brep->m_F.Count() : 0, brep ? brep->m_E.Count() : 0,
	    paired_edges, text.Array());
	result = 1;
    }

    delete brep;
    return result;
}


static int
check_ehy(const char *name, const point_t vertex, const vect_t height,
	const vect_t major_axis, double major_radius, double minor_radius,
	double asymptote_distance)
{
    struct rt_ehy_internal ehy;
    ehy.ehy_magic = RT_EHY_INTERNAL_MAGIC;
    VMOVE(ehy.ehy_V, vertex);
    VMOVE(ehy.ehy_H, height);
    VMOVE(ehy.ehy_Au, major_axis);
    ehy.ehy_r1 = major_radius;
    ehy.ehy_r2 = minor_radius;
    ehy.ehy_c = asymptote_distance;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_EHY;
    intern.idb_ptr = &ehy;
    intern.idb_meth = &OBJ[ID_EHY];

    return check_brep(name, &intern, 2);
}


static int
check_epa(const char *name, const point_t vertex, const vect_t height,
	const vect_t major_axis, double major_radius, double minor_radius)
{
    struct rt_epa_internal epa;
    epa.epa_magic = RT_EPA_INTERNAL_MAGIC;
    VMOVE(epa.epa_V, vertex);
    VMOVE(epa.epa_H, height);
    VMOVE(epa.epa_Au, major_axis);
    epa.epa_r1 = major_radius;
    epa.epa_r2 = minor_radius;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_EPA;
    intern.idb_ptr = &epa;
    intern.idb_meth = &OBJ[ID_EPA];
    return check_brep(name, &intern, 2);
}


static int
check_hyp(const char *name, const point_t vertex, const vect_t height,
	const vect_t major_axis, double minor_radius, double neck_ratio)
{
    struct rt_hyp_internal hyp;
    hyp.hyp_magic = RT_HYP_INTERNAL_MAGIC;
    VMOVE(hyp.hyp_Vi, vertex);
    VMOVE(hyp.hyp_Hi, height);
    VMOVE(hyp.hyp_A, major_axis);
    hyp.hyp_b = minor_radius;
    hyp.hyp_bnr = neck_ratio;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_HYP;
    intern.idb_ptr = &hyp;
    intern.idb_meth = &OBJ[ID_HYP];
    return check_brep(name, &intern, 3);
}


static int
check_part(const char *name, const point_t vertex, const vect_t height,
	double vertex_radius, double height_radius)
{
    struct rt_part_internal part;
    part.part_magic = RT_PART_INTERNAL_MAGIC;
    VMOVE(part.part_V, vertex);
    VMOVE(part.part_H, height);
    part.part_vrad = vertex_radius;
    part.part_hrad = height_radius;
    part.part_type = RT_PARTICLE_TYPE_CONE;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_PARTICLE;
    intern.idb_ptr = &part;
    intern.idb_meth = &OBJ[ID_PARTICLE];
    return check_brep(name, &intern, 3);
}


static void
init_test_sketch(struct rt_sketch_internal *sketch, point2d_t *vertices,
	struct line_seg *segments, void **segment_pointers, int *reverse)
{
    *sketch = {};
    sketch->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(sketch->V, 0.0, 0.0, 0.0);
    VSET(sketch->u_vec, 2.0, 0.0, 0.0);
    VSET(sketch->v_vec, 0.0, 0.0, 3.0);
    sketch->vert_count = 9;
    sketch->verts = vertices;

    const double coordinates[9][2] = {
	{0.0, 0.0}, {10.0, 0.0}, {10.0, 8.0}, {0.0, 8.0},
	{3.0, 2.0}, {7.0, 2.0}, {7.0, 5.0}, {3.0, 5.0},
	{0.0, 0.0}
    };
    for (size_t i = 0; i < 9; ++i) {
	vertices[i][0] = coordinates[i][0];
	vertices[i][1] = coordinates[i][1];
    }

    /* Deliberately unordered, with one reversed inner-loop segment and one
     * zero-length segment, to exercise loop recovery and degenerate input. */
    const int endpoints[9][2] = {
	{1, 2}, {0, 1}, {3, 0}, {2, 3},
	{4, 5}, {6, 5}, {6, 7}, {7, 4}, {0, 8}
    };
    for (size_t i = 0; i < 9; ++i) {
	segments[i].magic = CURVE_LSEG_MAGIC;
	segments[i].start = endpoints[i][0];
	segments[i].end = endpoints[i][1];
	segment_pointers[i] = &segments[i];
	reverse[i] = 0;
    }
    sketch->curve.count = 9;
    sketch->curve.reverse = reverse;
    sketch->curve.segment = segment_pointers;
}


static int
check_extrude_sketch(const char *name, struct rt_sketch_internal *sketch,
	int expected_faces)
{
    struct rt_extrude_internal extrude = {};
    extrude.magic = RT_EXTRUDE_INTERNAL_MAGIC;
    VSET(extrude.V, 2.0, -3.0, 5.0);
    VSET(extrude.h, 2.0, 1.0, 12.0);
    VSET(extrude.u_vec, 2.0, 0.0, 0.0);
    VSET(extrude.v_vec, 0.0, 3.0, 0.0);
    extrude.skt = sketch;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_EXTRUDE;
    intern.idb_ptr = &extrude;
    intern.idb_meth = &OBJ[ID_EXTRUDE];
    return check_brep(name, &intern, expected_faces);
}


static int
check_extrude(void)
{
    point2d_t vertices[9];
    struct line_seg segments[9];
    void *segment_pointers[9];
    int reverse[9];
    struct rt_sketch_internal sketch;
    init_test_sketch(&sketch, vertices, segments, segment_pointers, reverse);

    return check_extrude_sketch("extrude with inner loop", &sketch, 10);
}


static int
check_arc_extrude(void)
{
    point2d_t vertices[2] = {{-5.0, 0.0}, {5.0, 0.0}};
    struct carc_seg arcs[2] = {};
    void *segment_pointers[2] = {&arcs[0], &arcs[1]};
    int reverse[2] = {1, 0};
    for (size_t i = 0; i < 2; ++i) {
	arcs[i].magic = CURVE_CARC_MAGIC;
	arcs[i].start = 0;
	arcs[i].end = 1;
	arcs[i].radius = 5.0;
	arcs[i].center_is_left = 1;
    }
    arcs[0].orientation = 0;
    arcs[1].orientation = 1;

    struct rt_sketch_internal sketch = {};
    sketch.magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch.vert_count = 2;
    sketch.verts = vertices;
    sketch.curve.count = 2;
    sketch.curve.reverse = reverse;
    sketch.curve.segment = segment_pointers;
    return check_extrude_sketch("arc-segment EXTRUDE", &sketch, 4);
}


static int
check_full_circle_extrude(void)
{
    point2d_t vertices[2] = {{5.0, 0.0}, {0.0, 0.0}};
    struct carc_seg circle = {};
    circle.magic = CURVE_CARC_MAGIC;
    circle.start = 0;
    circle.end = 1;
    circle.radius = -1.0;
    void *segment_pointer = &circle;
    int reverse = 0;

    struct rt_sketch_internal sketch = {};
    sketch.magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch.vert_count = 2;
    sketch.verts = vertices;
    sketch.curve.count = 1;
    sketch.curve.reverse = &reverse;
    sketch.curve.segment = &segment_pointer;
    return check_extrude_sketch("full-circle EXTRUDE", &sketch, 3);
}


static int
check_rational_nurb_extrude(void)
{
    const double root_half = 0.7071067811865475244;
    point2d_t vertices[4] = {
	{10.0, 0.0}, {10.0 * root_half, 10.0 * root_half},
	{0.0, 10.0}, {0.0, 0.0}
    };
    int control_points[3] = {0, 1, 2};
    fastf_t knots[6] = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    fastf_t weights[3] = {1.0, root_half, 1.0};
    struct nurb_seg nurb = {};
    nurb.magic = CURVE_NURB_MAGIC;
    nurb.order = 3;
    nurb.pt_type = RT_NURB_MAKE_PT_TYPE(3, 2, 1);
    nurb.k.k_size = 6;
    nurb.k.knots = knots;
    nurb.c_size = 3;
    nurb.ctl_points = control_points;
    nurb.weights = weights;
    struct line_seg lines[2] = {};
    lines[0].magic = CURVE_LSEG_MAGIC;
    lines[0].start = 2;
    lines[0].end = 3;
    lines[1].magic = CURVE_LSEG_MAGIC;
    lines[1].start = 3;
    lines[1].end = 0;
    void *segment_pointers[3] = {&nurb, &lines[0], &lines[1]};
    int reverse[3] = {0, 0, 0};

    struct rt_sketch_internal sketch = {};
    sketch.magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch.vert_count = 4;
    sketch.verts = vertices;
    sketch.curve.count = 3;
    sketch.curve.reverse = reverse;
    sketch.curve.segment = segment_pointers;
    return check_extrude_sketch("rational NURB EXTRUDE", &sketch, 5);
}


static int
check_revolve(const char *name, double angle, int expected_faces)
{
    point2d_t vertices[9];
    struct line_seg segments[9];
    void *segment_pointers[9];
    int reverse[9];
    struct rt_sketch_internal sketch;
    init_test_sketch(&sketch, vertices, segments, segment_pointers, reverse);

    struct rt_revolve_internal revolve = {};
    revolve.magic = RT_REVOLVE_INTERNAL_MAGIC;
    VSET(revolve.v3d, 3.0, -2.0, 1.0);
    VSET(revolve.axis3d, 1.0, 0.0, 0.0);
    VSET(revolve.r, 0.0, 0.0, 20.0);
    revolve.ang = angle;
    BU_VLS_INIT(&revolve.sketch_name);
    revolve.skt = &sketch;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_REVOLVE;
    intern.idb_ptr = &revolve;
    intern.idb_meth = &OBJ[ID_REVOLVE];
    const int result = check_brep(name, &intern, expected_faces);
    bu_vls_free(&revolve.sketch_name);
    return result;
}


static int
check_ars(void)
{
    const size_t point_count = 9;
    fastf_t curve_data[3][point_count * 3] = {};
    fastf_t *curves[3] = {curve_data[0], curve_data[1], curve_data[2]};
    const double ring[point_count][2] = {
	{10.0, 0.0}, {7.071067811865, 7.071067811865},
	{0.0, 10.0}, {-7.071067811865, 7.071067811865},
	{-10.0, 0.0}, {-7.071067811865, -7.071067811865},
	{0.0, -10.0}, {7.071067811865, -7.071067811865},
	{10.0, 0.0}
    };
    for (size_t i = 0; i < point_count; ++i) {
	curve_data[1][3 * i] = ring[i][0];
	curve_data[1][3 * i + 1] = ring[i][1];
	curve_data[1][3 * i + 2] = 5.0;
	curve_data[2][3 * i + 2] = 10.0;
    }

    struct rt_ars_internal ars = {};
    ars.magic = RT_ARS_INTERNAL_MAGIC;
    ars.ncurves = 3;
    ars.pts_per_curve = point_count;
    ars.curves = curves;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_ARS;
    intern.idb_ptr = &ars;
    intern.idb_meth = &OBJ[ID_ARS];
    return check_brep("closed ARS bicone", &intern, 16);
}


static int
check_bot(void)
{
    fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	10.0, 0.0, 0.0,
	0.0, 10.0, 0.0,
	0.0, 0.0, 10.0
    };
    int faces[] = {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
    struct rt_bot_internal bot = {};
    bot.magic = RT_BOT_INTERNAL_MAGIC;
    bot.mode = RT_BOT_SOLID;
    bot.orientation = RT_BOT_CCW;
    bot.num_faces = 4;
    bot.faces = faces;
    bot.num_vertices = 4;
    bot.vertices = vertices;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_BOT;
    intern.idb_ptr = &bot;
    intern.idb_meth = &OBJ[ID_BOT];
    return check_brep("solid BOT tetrahedron", &intern, 4);
}


static int
check_pipe_case(const char *name, const double (*coordinates)[3],
	size_t point_count, int expected_faces)
{
    struct rt_pipe_internal pipe = {};
    pipe.pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    BU_LIST_INIT(&pipe.pipe_segs_head);
    pipe.pipe_count = static_cast<int>(point_count);
    std::vector<struct wdb_pipe_pnt> points(point_count);
    for (size_t i = 0; i < point_count; ++i) {
	BU_LIST_INIT(&points[i].l);
	VSET(points[i].pp_coord, coordinates[i][0], coordinates[i][1],
	    coordinates[i][2]);
	points[i].pp_id = 4.0;
	points[i].pp_od = 8.0;
	points[i].pp_bendradius = 5.0;
	BU_LIST_INSERT(&pipe.pipe_segs_head, &points[i].l);
    }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_PIPE;
    intern.idb_ptr = &pipe;
    intern.idb_meth = &OBJ[ID_PIPE];
    const int result = check_brep(name, &intern, expected_faces);
    size_t retained_points = 0;
    struct wdb_pipe_pnt *point;
    for (BU_LIST_FOR(point, wdb_pipe_pnt, &pipe.pipe_segs_head))
	++retained_points;
    if (retained_points != point_count) {
	bu_log("ERROR: PIPE BRep conversion mutated its source point list\n");
	return 1;
    }
    return result;
}


static int
check_pipe(void)
{
    const double right_bend[4][3] = {
	{0.0, 0.0, 0.0}, {0.0, 0.0, 30.0},
	{0.0, 0.0, 30.0}, {20.0, 0.0, 30.0}
    };
    const double left_bend[3][3] = {
	{0.0, 0.0, 0.0}, {0.0, 0.0, 30.0},
	{-20.0, 0.0, 30.0}
    };
    const double compound_bend[4][3] = {
	{0.0, 0.0, 0.0}, {0.0, 0.0, 30.0},
	{20.0, 0.0, 30.0}, {20.0, 20.0, 30.0}
    };
    int result = check_pipe_case("bent hollow PIPE with duplicate point",
	right_bend, 4, 8);
    result |= check_pipe_case("oppositely oriented hollow PIPE bend",
	left_bend, 3, 8);
    result |= check_pipe_case("compound hollow PIPE bends", compound_bend,
	4, 12);
    return result;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    int result = 0;
    point_t vertex;
    vect_t height;
    vect_t major_axis;

    VSET(vertex, 10.0, 20.0, 30.0);
    VSET(height, 0.0, 0.0, 40.0);
    VSET(major_axis, 1.0, 0.0, 0.0);
    result |= check_ehy("elliptical", vertex, height, major_axis,
	8.0, 3.0, 5.0);

    VSET(vertex, 0.0, 0.0, -500.0);
    VSET(height, 0.0, 0.0, 1500.0);
    VSET(major_axis, 0.0, 1.0, 0.0);
    result |= check_ehy("circular", vertex, height, major_axis,
	1000.0, 1000.0, 1.0);

    VSET(vertex, -4.0, 7.0, 11.0);
    VSET(height, 0.0, 30.0, 40.0);
    VSET(major_axis, 1.0, 0.0, 0.0);
    result |= check_ehy("oblique", vertex, height, major_axis,
	12.0, 4.0, 250.0);

    VSET(vertex, 3.0, -7.0, 2.0);
    VSET(height, 0.0, 24.0, 32.0);
    VSET(major_axis, 1.0, 0.0, 0.0);
    result |= check_epa("oblique EPA", vertex, height, major_axis,
	9.0, 2.5);

    VSET(vertex, -10.0, 5.0, 4.0);
    VSET(height, 0.0, 30.0, 40.0);
    VSET(major_axis, 7.0, 0.0, 0.0);
    result |= check_hyp("oblique HYP", vertex, height, major_axis,
	3.0, 0.4);

    VSET(vertex, 2.0, 3.0, 5.0);
    VSET(height, 0.0, 30.0, 40.0);
    result |= check_part("conical PART", vertex, height, 12.0, 5.0);

    VSET(height, 20.0, 0.0, 0.0);
    result |= check_part("cylindrical PART", vertex, height, 4.0, 4.0);

    result |= check_pipe();
    result |= check_extrude();
    result |= check_arc_extrude();
    result |= check_full_circle_extrude();
    result |= check_rational_nurb_extrude();
    result |= check_revolve("partial REVOLVE with inner loop", 1.75, 4);
    result |= check_revolve("full REVOLVE with inner loop", 2.0 * ON_PI, 2);
    result |= check_ars();
    result |= check_bot();

    return result;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
