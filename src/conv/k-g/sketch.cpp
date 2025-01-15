/*                 S K E T C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
 /** @file sketch.cpp
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate sketch implementation
  */

#include "sketch.h"


Sketch::Sketch(void)
{
    BU_GET(m_sketch, rt_sketch_internal);
    m_sketch->magic = RT_SKETCH_INTERNAL_MAGIC;
    m_sketch->curve.count = 0;
    m_sketch->vert_count = 0;
}


void Sketch::setName
(
    const char* value
) {
    if (value != nullptr)
	m_name = value;
    else
	m_name = "";
}


rt_sketch_internal* Sketch::creatSketch
(
    std::string                sectionType,
    const point_t&             node1,
    const point_t&             node2,
    const point_t&             node3,
    const std::vector<double>& D
) {
    line_seg* lsg;

    point_t V;
    VMOVE(V, node1);

    vect_t r, t, s;
    vect_t n2n3;

    VSUB2(r, node2, node1);
    VSUB2(n2n3, node3, node2);

    VCROSS(t, r, n2n3);
    VCROSS(s, r, t);

    VUNITIZE(t);
    VUNITIZE(s);

    if (sectionType == "SECTION_01") {
	const size_t vert_count = 12;
	const size_t seg_count  = 12;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = +D[0] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5 + D[1];

	verts[2][0] = +D[3] * 0.5;
	verts[2][1] = -D[2] * 0.5 + D[1];

	verts[3][0] = +D[3] * 0.5;
	verts[3][1] = +D[2] * 0.5 - D[1];

	verts[4][0] = +D[0] * 0.5;
	verts[4][1] = +D[2] * 0.5 - D[1];

	verts[5][0] = +D[0] * 0.5;
	verts[5][1] = +D[2] * 0.5;

	verts[6][0] = -D[0] * 0.5;
	verts[6][1] = +D[2] * 0.5;

	verts[7][0] = -D[0] * 0.5;
	verts[7][1] = +D[2] * 0.5 - D[1];

	verts[8][0] = -D[3] * 0.5;
	verts[8][1] = +D[2] * 0.5 - D[1];

	verts[9][0] = -D[3] * 0.5;
	verts[9][1] = -D[2] * 0.5 + D[1];

	verts[10][0] = -D[0] * 0.5;
	verts[10][1] = -D[2] * 0.5 + D[1];

	verts[11][0] = -D[0] * 0.5;
	verts[11][1] = -D[2] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_02") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[0] * 0.5;
	verts[2][1] = -D[2] * 0.5 + D[1];

	verts[3][0] = -D[0] / 2 + D[3];
	verts[3][1] = -D[2] * 0.5 + D[1];

	verts[4][0] = -D[0] * 0.5 + D[3];
	verts[4][1] = +D[2] * 0.5 - D[1];

	verts[5][0] = +D[0] * 0.5;
	verts[5][1] = +D[2] * 0.5 - D[1];

	verts[6][0] = +D[0] * 0.5;
	verts[6][1] = +D[2] * 0.5;

	verts[7][0] = -D[0] * 0.5;
	verts[7][1] = +D[2] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_03") {
	const size_t vert_count = 6;
	const size_t seg_count  = 6;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = D[0] * 0.5;
	verts[2][1] = -D[2] * 0.5 + D[1];

	verts[3][0] = -D[0] / 2 + D[3];
	verts[3][1] = -D[2] * 0.5 + D[1];

	verts[4][0] = -D[0] * 0.5 + D[3];
	verts[4][1] = +D[2] * 0.5;

	verts[5][0] = -D[0] * 0.5;
	verts[5][1] = +D[2] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_04") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[3] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[3] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[3] * 0.5;
	verts[2][1] = +D[2] * 0.5 - D[1];

	verts[3][0] = +D[0] * 0.5;
	verts[3][1] = +D[2] * 0.5 - D[1];

	verts[4][0] = +D[0] * 0.5;
	verts[4][1] = +D[2] * 0.5;

	verts[5][0] = -D[0] * 0.5;
	verts[5][1] = +D[2] * 0.5;

	verts[6][0] = -D[0] * 0.5;
	verts[6][1] = +D[2] * 0.5 - D[1];

	verts[7][0] = -D[3] * 0.5;
	verts[7][1] = +D[2] * 0.5 - D[1];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count-1] = lsg;
    }
    else if (sectionType == "SECTION_05") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[0] * 0.5;
	verts[2][1] = +D[2] * 0.5;

	verts[3][0] = -D[0] * 0.5;
	verts[3][1] = +D[2] * 0.5;

	verts[4][0] = -D[0] * 0.5 + D[3];
	verts[4][1] = -D[2] * 0.5 + D[1];

	verts[5][0] = +D[0] * 0.5 - D[3];
	verts[5][1] = -D[2] * 0.5 + D[1];

	verts[6][0] = +D[0] * 0.5 - D[3];
	verts[6][1] = +D[2] * 0.5 - D[1];

	verts[7][0] = -D[0] * 0.5 + D[3];
	verts[7][1] = +D[2] * 0.5 - D[1];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < 4; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 3;
	lsg->end = 0;
	crv->segment[3] = lsg;

	for (size_t i_c2 = 4; i_c2 < 8; ++i_c2) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c2;
	    lsg->end = i_c2 + 1;
	    crv->segment[i_c2] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 4;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_06") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[3] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[0] * 0.5;
	verts[2][1] = -D[2] * 0.5 + D[1];

	verts[3][0] = +D[3] * 0.5;
	verts[3][1] = -D[2] * 0.5 + D[1];

	verts[4][0] = +D[3] * 0.5;
	verts[4][1] = +D[2] * 0.5;

	verts[5][0] = -D[0] * 0.5;
	verts[5][1] = +D[2] * 0.5;

	verts[6][0] = -D[0] * 0.5;
	verts[6][1] = +D[2] * 0.5 - D[1];

	verts[7][0] = -D[3] * 0.5;
	verts[7][1] = +D[2] * 0.5 - D[1];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_07") {
	const size_t vert_count = 4;
	const size_t seg_count  = 4;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[0] * 0.5 - D[1] * 0.5;
	verts[2][1] = +D[2] * 0.5;

	verts[3][0] = -D[0] * 0.5 + D[1] * 0.5;
	verts[3][1] = +D[2] * 0.5;


	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_10") {
	const size_t vert_count = 12;
	const size_t seg_count  = 12;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[1] * 0.5;
	verts[0][1] = -D[0] * 0.5;

	verts[1][0] = +D[1] * 0.5;
	verts[1][1] = -D[0] * 0.5;

	verts[2][0] = +D[1] * 0.5;
	verts[2][1] = -D[0] * 0.5 + D[4];

	verts[3][0] = +D[3] * 0.5;
	verts[3][1] = -D[0] * 0.5 + D[4];

	verts[4][0] = +D[3] * 0.5;
	verts[4][1] = +D[0] * 0.5 - D[5];

	verts[5][0] = +D[2] * 0.5;
	verts[5][1] = +D[0] * 0.5 - D[5];

	verts[6][0] = +D[2] * 0.5;
	verts[6][1] = +D[0] * 0.5;

	verts[7][0] = -D[2] * 0.5;
	verts[7][1] = +D[0] * 0.5;

	verts[8][0] = -D[2] * 0.5;
	verts[8][1] = +D[0] * 0.5 - D[5];

	verts[9][0] = -D[3] * 0.5;
	verts[9][1] = +D[0] * 0.5 - D[5];

	verts[10][0] = -D[3] * 0.5;
	verts[10][1] = -D[0] * 0.5 + D[4];

	verts[11][0] = -D[1] * 0.5;
	verts[11][1] = -D[0] * 0.5 + D[4];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_11") {
	const size_t vert_count = 4;
	const size_t seg_count  = 4;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[1] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[1] * 0.5;

	verts[2][0] = +D[0] * 0.5;
	verts[2][1] = +D[1] * 0.5;

	verts[3][0] = -D[0] * 0.5;
	verts[3][1] = +D[1] * 0.5;


	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_12") {
	const size_t vert_count = 12;
	const size_t seg_count  = 12;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[1] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[1] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[1] * 0.5;
	verts[2][1] = +D[2] * 0.25 - D[3] * 0.5;

	verts[3][0] = +D[1] * 0.5 + D[0] * 0.5;
	verts[3][1] = +D[2] * 0.25 - D[3] * 0.5;

	verts[4][0] = +D[1] * 0.5 + D[0] * 0.5;
	verts[4][1] = +D[2] * 0.25 - D[3] * 0.5 + D[3];

	verts[5][0] = +D[1] * 0.5;
	verts[5][1] = +D[2] * 0.25 - D[3] * 0.5 + D[3];

	verts[6][0] = +D[1] * 0.5;
	verts[6][1] = +D[2] * 0.5;

	verts[7][0] = -D[1] * 0.5;
	verts[7][1] = +D[2] * 0.5;

	verts[8][0] = -D[1] * 0.5;
	verts[8][1] = +D[2] * 0.25 - D[3] * 0.5 + D[3];

	verts[9][0] = -D[1] * 0.5 - D[0] * 0.5;
	verts[9][1] = +D[2] * 0.25 - D[3] * 0.5 + D[3];

	verts[10][0] = -D[1] * 0.5 - D[0] * 0.5;
	verts[10][1] = +D[2] * 0.25 - D[3] * 0.5;

	verts[11][0] = -D[1] * 0.5;
	verts[11][1] = +D[2] * 0.25 - D[3] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_13") {
	const size_t vert_count = 12;
	const size_t seg_count  = 12;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5 - D[1] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = -D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = -D[0] * 0.5;
	verts[2][1] = -D[3] * 0.5;

	verts[3][0] = +D[0] * 0.5;
	verts[3][1] = -D[3] * 0.5;

	verts[4][0] = +D[0] * 0.5;
	verts[4][1] = -D[2] * 0.5;

	verts[5][0] = +D[0] * 0.5 + D[1] * 0.5;
	verts[5][1] = -D[2] * 0.5;

	verts[6][0] = +D[0] * 0.5 + D[1] / 2;
	verts[6][1] = +D[2] * 0.5;

	verts[7][0] = +D[0] * 0.5;
	verts[7][1] = +D[2] * 0.5;

	verts[8][0] = +D[0] * 0.5;
	verts[8][1] = +D[3] * 0.5;

	verts[9][0] = -D[0] * 0.5;
	verts[9][1] = +D[3] * 0.5;

	verts[10][0] = -D[0] * 0.5;
	verts[10][1] = +D[2] * 0.5;

	verts[11][0] = -D[0] * 0.5 - D[1] * 0.5;
	verts[11][1] = D[2] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_14") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[1] * 0.5 + D[2] * 0.5;
	verts[0][1] = -D[3] * 0.5;

	verts[1][0] = +D[1] * 0.5 + D[2] * 0.5 - D[2];
	verts[1][1] = -D[3] * 0.5;

	verts[2][0] = +D[1] * 0.5 + D[2] * 0.5 - D[2];
	verts[2][1] = -D[0] * 0.5;

	verts[3][0] = +D[1] * 0.5 + D[2] * 0.5;
	verts[3][1] = -D[0] * 0.5;

	verts[4][0] = +D[1] * 0.5 + D[2] * 0.5;
	verts[4][1] = +D[0] * 0.5;

	verts[5][0] = +D[1] * 0.5 + D[2] * 0.5 - D[2];
	verts[5][1] = +D[0] * 0.5;

	verts[6][0] = +D[1] * 0.5 + D[2] * 0.5 - D[2];
	verts[6][1] = +D[3] * 0.5;

	verts[7][0] = -D[1] * 0.5 + D[2] * 0.5;
	verts[7][1] = +D[3] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_15") {
	const size_t vert_count = 12;
	const size_t seg_count  = 12;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[1] * 0.5 - D[0] * 0.5;
	verts[0][1] = -D[3] * 0.5;

	verts[1][0] = +D[1] * 0.5 + D[0] * 0.5;
	verts[1][1] = -D[3] * 0.5;

	verts[2][0] = +D[1] * 0.5 + D[0] * 0.5;
	verts[2][1] = -D[2] * 0.5;

	verts[3][0] = +D[1] * 0.5;
	verts[3][1] = -D[2] * 0.5;

	verts[4][0] = +D[1] * 0.5;
	verts[4][1] = +D[2] * 0.5;

	verts[5][0] = +D[1] * 0.5 + D[0] * 0.5;
	verts[5][1] = +D[2] * 0.5;

	verts[6][0] = +D[1] * 0.5 + D[0] * 0.5;
	verts[6][1] = +D[3] * 0.5;

	verts[7][0] = -D[1] * 0.5 - D[0] * 0.5;
	verts[7][1] = +D[3] * 0.5;

	verts[8][0] = -D[1] * 0.5 - D[0] * 0.5;
	verts[8][1] = +D[2] * 0.5;

	verts[9][0] = -D[1] * 0.5;
	verts[9][1] = +D[2] * 0.5;

	verts[10][0] = -D[1] * 0.5;
	verts[10][1] = -D[2] * 0.5;

	verts[11][0] = -D[1] * 0.5 - D[0] * 0.5;
	verts[11][1] = -D[2] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_16") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[1] * 0.5;
	verts[0][1] = -D[3] * 0.5;

	verts[1][0] = +D[1] * 0.5 + D[0];
	verts[1][1] = -D[3] * 0.5;

	verts[2][0] = +D[1] * 0.5 + D[0];
	verts[2][1] = -D[2] * 0.5;

	verts[3][0] = +D[1] * 0.5;
	verts[3][1] = -D[2] * 0.5;

	verts[4][0] = +D[1] * 0.5;
	verts[4][1] = +D[2] * 0.5;

	verts[5][0] = +D[1] * 0.5 + D[0];
	verts[5][1] = +D[0] * 0.5;

	verts[6][0] = +D[1] * 0.5 + D[0];
	verts[6][1] = +D[3] * 0.5;

	verts[7][0] = -D[1] * 0.5;
	verts[7][1] = +D[3] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_17") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[3] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[3] * 0.5;
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[3] * 0.5;
	verts[2][1] = +D[2] * 0.5;

	verts[3][0] = +D[3] * 0.5 - D[0];
	verts[3][1] = +D[2] * 0.5;

	verts[4][0] = +D[3] * 0.5 - D[0];
	verts[4][1] = -D[2] * 0.5 + D[1];

	verts[5][0] = -D[3] * 0.5 + D[0];
	verts[5][1] = -D[2] * 0.5 + D[1];

	verts[6][0] = -D[3] * 0.5 + D[0];
	verts[6][1] = +D[2] * 0.5;

	verts[7][0] = -D[3] * 0.5;
	verts[7][1] = +D[2] * 0.5;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_18") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[1] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[1] * 0.5;

	verts[2][0] = +D[0] * 0.5;
	verts[2][1] = -D[1] * 0.5 + D[2];

	verts[3][0] = +D[3] * 0.5;
	verts[3][1] = -D[1] * 0.5 + D[2];

	verts[4][0] = +D[3] * 0.5;
	verts[4][1] = +D[1] * 0.5;

	verts[5][0] = -D[3] * 0.5;
	verts[5][1] = +D[1] * 0.5;

	verts[6][0] = -D[3] * 0.5;
	verts[6][1] = -D[1] * 0.5 + D[2];

	verts[7][0] = -D[0] * 0.5;
	verts[7][1] = -D[1] * 0.5 + D[2];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_19") {
	const size_t vert_count = 8;
	const size_t seg_count  = 8;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[1] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[1] * 0.5;

	verts[2][0] = +D[0] * 0.5;
	verts[2][1] = +D[1] * 0.5;

	verts[3][0] = -D[0] * 0.5;
	verts[3][1] = +D[1] * 0.5;

	verts[4][0] = -D[0] * 0.5 + D[5];
	verts[4][1] = -D[1] * 0.5 + D[3];

	verts[5][0] = +D[0] * 0.5 - D[4];
	verts[5][1] = -D[1] * 0.5 + D[3];

	verts[6][0] = +D[0] * 0.5 - D[4];
	verts[6][1] = +D[1] * 0.5 - D[2];

	verts[7][0] = -D[0] * 0.5 + D[5];
	verts[7][1] = +D[1] * 0.5 - D[2];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < 4; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 3;
	lsg->end = 0;
	crv->segment[3] = lsg;

	for (size_t i_c2 = 4; i_c2 < 8; ++i_c2) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c2;
	    lsg->end = i_c2 + 1;
	    crv->segment[i_c2] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (vert_count - 1);
	lsg->end = 4;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_20") {
	const size_t vert_count = 6;
	const size_t seg_count  = 6;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[1] * 0.5 + D[0];
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = +D[1] * 0.5 - D[0];
	verts[1][1] = -D[2] * 0.5;

	verts[2][0] = +D[1] * 0.5;
	verts[2][1] = +0.0;

	verts[3][0] = +D[1] * 0.5 - D[0];
	verts[3][1] = +D[2] * 0.5;

	verts[4][0] = -D[1] * 0.5 + D[0];
	verts[4][1] = +D[2] * 0.5;

	verts[5][0] = -D[1] * 0.5;
	verts[5][1] = 0.0;

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 5;
	lsg->end = 0;
	crv->segment[5] = lsg;
    }
    else if (sectionType == "SECTION_21") {
	const size_t vert_count = 12;
	const size_t seg_count  = 12;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[2] * 0.5 - D[3];
	verts[0][1] = -D[0] * 0.5;

	verts[1][0] = -D[2] * 0.5 + D[1];
	verts[1][1] = -D[0] * 0.5;

	verts[2][0] = -D[2] * 0.5 + D[1];
	verts[2][1] = +D[0] * 0.5 - D[1];

	verts[3][0] = +D[2] * 0.5 - D[1];
	verts[3][1] = +D[0] * 0.5 - D[1];

	verts[4][0] = +D[2] * 0.5 - D[1];
	verts[4][1] = -D[0] * 0.5;

	verts[5][0] = +D[2] * 0.5 + D[3];
	verts[5][1] = -D[0] * 0.5;

	verts[6][0] = +D[2] * 0.5 + D[3];
	verts[6][1] = -D[0] * 0.5 + D[1];

	verts[7][0] = +D[2] * 0.5;
	verts[7][1] = -D[0] * 0.5 + D[1];

	verts[8][0] = +D[2] * 0.5;
	verts[8][1] = +D[0] * 0.5;

	verts[9][0] = -D[2] * 0.5;
	verts[9][1] = +D[0] * 0.5;

	verts[10][0] = -D[2] * 0.5;
	verts[10][1] = -D[0] * 0.5 + D[1];

	verts[11][0] = -D[2] * 0.5 - D[3];
	verts[11][1] = -D[0] * 0.5 + D[1];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < seg_count; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 0;
	crv->segment[seg_count - 1] = lsg;
    }
    else if (sectionType == "SECTION_22") {
	const size_t vert_count = 16;
	const size_t seg_count  = 16;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");

	verts[0][0] = -D[0] * 0.5;
	verts[0][1] = -D[1] * 0.5;

	verts[1][0] = +D[0] * 0.5;
	verts[1][1] = -D[1] * 0.5;

	verts[2][0] = +D[0] * 0.5;
	verts[2][1] = -D[1] * 0.5 + D[4];

	verts[3][0] = -D[0] * 0.5;
	verts[3][1] = -D[1] * 0.5 + D[4];

	verts[4][0] = -D[2] * 0.5 - D[5];
	verts[4][1] = -D[1] * 0.5 + D[4];

	verts[5][0] = -D[2] * 0.5 + D[3];
	verts[5][1] = -D[1] * 0.5 + D[4];

	verts[6][0] = -D[2] * 0.5 + D[3];
	verts[6][1] = +D[1] * 0.5 - D[3];

	verts[7][0] = +D[2] * 0.5 - D[3];
	verts[7][1] = +D[1] * 0.5 - D[3];

	verts[8][0] = +D[2] * 0.5 - D[3];
	verts[8][1] = -D[1] * 0.5 + D[4];

	verts[9][0] = +D[2] * 0.5 + D[5];
	verts[9][1] = -D[1] * 0.5 + D[4];

	verts[10][0] = +D[2] * 0.5 + D[5];
	verts[10][1] = -D[1] * 0.5 + D[4] + D[3];

	verts[11][0] = +D[2] * 0.5;
	verts[11][1] = -D[1] * 0.5 + D[4] + D[3];

	verts[12][0] = +D[2] * 0.5;
	verts[12][1] = +D[1] * 0.5;

	verts[13][0] = -D[2] * 0.5;
	verts[13][1] = +D[1] * 0.5;

	verts[14][0] = -D[2] * 0.5;
	verts[14][1] = -D[1] * 0.5 + D[4] + D[3];

	verts[15][0] = -D[2] * 0.5 - D[5];
	verts[15][1] = -D[1] * 0.5 + D[4] + D[3];

	m_sketch->vert_count = vert_count;
	m_sketch->verts = verts;

	rt_curve* crv = &m_sketch->curve;
	crv->count = seg_count;
	crv->segment = (void**)bu_calloc(crv->count, sizeof(void*), "segments");
	crv->reverse = (int*)bu_calloc(crv->count, sizeof(int), "reverse");

	for (size_t i_c = 0; i_c < 4; ++i_c) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c;
	    lsg->end = i_c + 1;
	    crv->segment[i_c] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = 3;
	lsg->end = 0;
	crv->segment[3] = lsg;

	for (size_t i_c1 = 4; i_c1 < seg_count; ++i_c1) {
	    BU_ALLOC(lsg, struct line_seg);
	    lsg->magic = CURVE_LSEG_MAGIC;
	    lsg->start = i_c1;
	    lsg->end = i_c1 + 1;
	    crv->segment[i_c1] = lsg;
	}

	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = vert_count - 1;
	lsg->end = 4;
	crv->segment[seg_count - 1] = lsg;
    }

    return m_sketch;
}


rt_sketch_internal* Sketch::getSketch(void) const
{
    return m_sketch;
}


std::string Sketch::write
(
    rt_wdb* wdbp
) {
    std::string ret;

    rt_sketch_internal* sketch_wdb;
    BU_GET(sketch_wdb, rt_sketch_internal);
    sketch_wdb->magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch_wdb = m_sketch;

    if (sketch_wdb->vert_count > 0) {
	wdb_export(wdbp, m_name.c_str(), sketch_wdb, ID_SKETCH, 1);
	ret = m_name;
    }

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
