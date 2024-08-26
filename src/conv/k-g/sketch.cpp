/*                 S K E T C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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

void Sketch::setName(const char* value)
{
    name = value;
}

rt_sketch_internal* Sketch::creatSketch
(
    std::string sketchName,
    std::string sectionType,
    const point_t& node1,
    const point_t& node2,
    const point_t& node3,
    std::vector<double> D
) {
    if (sectionType == "SECTION_01") {
	unsigned long vert_count = 12;
	unsigned long seg_count  = 12;
	line_seg* lsg;

	point_t V;
	VMOVE(V, node1);
	vect_t r, t, s;
	vect_t n2n3;
	r[X] = node2[X] - node1[X];
	r[Y] = node2[Y] - node1[Y];
	r[Z] = node2[Z] - node1[Z];
	
	n2n3[X] = node3[X] - node2[X];
	n2n3[Y] = node3[Y] - node2[Y];
	n2n3[Z] = node3[Z] - node2[Z];

	t[X] = r[Y] * n2n3[Z] - r[Z] * n2n3[Y];
	t[Y] = r[Z] * n2n3[X] - r[X] * n2n3[Z];
	t[Z] = r[X] * n2n3[Y] - r[Y] * n2n3[X];

	s[X] = r[Y] * t[Z] - r[Z] * t[Y];
	s[Y] = r[Z] * t[X] - r[X] * t[Z];
	s[Z] = r[X] * t[Y] - r[Y] * t[X];

	double normt = sqrt(t[X] * t[X] + t[Y] * t[Y] + t[Z] * t[Z]);
	double norms = sqrt(s[X] * s[X] + s[Y] * s[Y] + s[Z] * s[Z]);

	t[X] = t[X] / normt;
	t[Y] = t[Y] / normt;
	t[Z] = t[Z] / normt;

	s[X] = s[X] / norms;
	s[Y] = s[Y] / norms;
	s[Z] = s[Z] / norms;

	VMOVE(m_sketch->V, V);
	VMOVE(m_sketch->u_vec, t);
	VMOVE(m_sketch->v_vec, s);

	point2d_t* verts = (point2d_t*)bu_calloc(vert_count, sizeof(point2d_t), "verts");
	verts[0][0] = D[0] * 0.5;
	verts[0][1] = -D[2] * 0.5;

	verts[1][0] = D[0] * 0.5;
	verts[1][1] = -D[2] * 0.5 + D[1];

	verts[2][0] = D[3] * 0.5;
	verts[2][1] = -D[2] * 0.5 + D[1];

	verts[3][0] = D[3] * 0.5;
	verts[3][1] = D[2] * 0.5 - D[1];

	verts[4][0] = D[0] * 0.5;
	verts[4][1] = D[2] * 0.5 - D[1];

	verts[5][0] = D[0] * 0.5;
	verts[5][1] = D[2] * 0.5;

	verts[6][0] = -D[0] * 0.5;
	verts[6][1] = D[2] * 0.5;

	verts[7][0] = -D[0] * 0.5;
	verts[7][1] = D[2] * 0.5 - D[1];

	verts[8][0] = -D[3] * 0.5;
	verts[8][1] = D[2] * 0.5 - D[1];

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
	lsg->start = 11;
	lsg->end = 0;
	crv->segment[11] = lsg;
	
	return m_sketch;
    }
}

const char* Sketch::getName(void) const
{
    return nullptr;
}

rt_sketch_internal* Sketch::getSketch(void) const
{
    return nullptr;
}

std::string Sketch::write(rt_wdb* wdbp)
{
    std::string ret;

    rt_sketch_internal* sketch_wdb;
    BU_GET(sketch_wdb, rt_sketch_internal);
    sketch_wdb->magic = RT_SKETCH_INTERNAL_MAGIC;
    sketch_wdb = m_sketch;

    if (sketch_wdb->vert_count > 0) {
	wdb_export(wdbp, name.c_str(), sketch_wdb, ID_SKETCH, 1);
	ret = name;
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
