/*                       T I R E _ T R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/tire_tread.cpp
 *
 * Tread sketch/extrusion CSG emission for the tire command.
 */

#include "common.h"

#include <math.h>
#include <string>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

#include "tire_private.h"

namespace tire_private {

static int
emit_tread_extrusions(CsgTransaction &builder, const std::string &scope, int pattern_index, const TreadSketch &verts,
	    fastf_t patternwidth1, fastf_t patternwidth2,
	    fastf_t tirewidth, fastf_t zbase, fastf_t ztire)
{
    struct rt_sketch_internal skt;
    struct line_seg *lsg;
    point_t V;
    vect_t u_vec, v_vec, h;
    size_t i;

    skt.magic = RT_SKETCH_INTERNAL_MAGIC;

    VSET(V, 0, -tirewidth/2, zbase-.1*zbase);
    VSET(u_vec, 1, 0, 0);
    VSET(v_vec, 0, 1, 0);
    VMOVE(skt.V, V);
    VMOVE(skt.u_vec, u_vec);
    VMOVE(skt.v_vec, v_vec);

    skt.vert_count = verts.size();
    skt.verts = (point2d_t *)bu_calloc(skt.vert_count, sizeof(point2d_t), "verts");
    for (i = 0; i < skt.vert_count; i++) {
	V2SET(skt.verts[i],
	      verts[i][0] * patternwidth2 - patternwidth2 / 2.0,
	      verts[i][1] * tirewidth);
    }

    skt.curve.count = verts.size();
    skt.curve.reverse = (int *)bu_calloc(skt.curve.count, sizeof(int), "sketch: reverse");
    skt.curve.segment = (void **)bu_calloc(skt.curve.count, sizeof(void *), "segs");

    for (i = 0; i < verts.size()-1; i++) {
	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = i;
	lsg->end = i + 1;
	skt.curve.segment[i] = (void *)lsg;
    }

    BU_ALLOC(lsg, struct line_seg);
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = verts.size() - 1;
    lsg->end = 0;
    skt.curve.segment[verts.size() - 1] = (void *)lsg;

    const std::string sketch_name = indexed_object_name("sketch", scope, pattern_index, ".s");
    const int sketch_ret = builder.write_sketch(sketch_name.c_str(), &skt);

    bu_free(skt.verts, "verts");
    rt_curve_free(&skt.curve);

    if (sketch_ret != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(h, patternwidth1 / 2 - patternwidth2 / 2, 0, ztire - (zbase - .11 * zbase));
    if (builder.write_extrusion(indexed_object_name("extrude.a", scope, pattern_index, ".s").c_str(), sketch_name.c_str(), V, h, u_vec, v_vec, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(h, -patternwidth1 / 2 + patternwidth2 / 2, 0, ztire - (zbase - .11 * zbase));
    if (builder.write_extrusion(indexed_object_name("extrude.b", scope, pattern_index, ".s").c_str(), sketch_name.c_str(), V, h, u_vec, v_vec, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(h, 0, 0, ztire - (zbase - .11 * zbase));
    return builder.write_extrusion(indexed_object_name("extrude.c", scope, pattern_index, ".s").c_str(), sketch_name.c_str(), V, h, u_vec, v_vec, 0);
}

int
make_tread_pattern(CsgTransaction &builder, const std::string &suffix, fastf_t dwidth,
		 fastf_t z_base, fastf_t ztire, int number_of_patterns,
		 const TreadSpec &tread_spec)
{
    struct wmember treadpattern, tread;
    fastf_t patternwidth1, patternwidth2;
    mat_t y;
    int i;

    patternwidth1 = ztire * sin(M_PI / number_of_patterns);
    patternwidth2 = z_base * sin(M_PI / number_of_patterns);

    BU_LIST_INIT(&treadpattern.l);

    for (i = 0; i < static_cast<int>(tread_spec.sketches.size()); i++) {
	if (emit_tread_extrusions(builder, suffix + ".tread", i + 1, tread_spec.sketches[i],
		    2 * patternwidth1, 2 * patternwidth2, dwidth, z_base, ztire) != BRLCAD_OK)
	    return BRLCAD_ERROR;
	add_member(indexed_object_name("extrude.a", suffix + ".tread", i + 1, ".s"), &treadpattern.l, WMOP_UNION);
	add_member(indexed_object_name("extrude.b", suffix + ".tread", i + 1, ".s"), &treadpattern.l, WMOP_UNION);
	add_member(indexed_object_name("extrude.c", suffix + ".tread", i + 1, ".s"), &treadpattern.l, WMOP_UNION);
    }

    if (builder.write_lcomb(object_name("master", suffix + ".tread", ".c").c_str(), &treadpattern, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&tread.l);
    add_member(object_name("shape", suffix + ".tread", ".c"), &tread.l, WMOP_UNION);
    for (i = 1; i <= number_of_patterns; i++) {
	tire_y_rot_mat(&y, i * M_2PI / number_of_patterns);
	add_member(object_name("master", suffix + ".tread", ".c"), &tread.l, y, WMOP_SUBTRACT);
    }

    return builder.write_lcomb(object_name("", suffix + ".tread", ".c").c_str(), &tread, 0, NULL, NULL, NULL , 0);
}

} /* namespace tire_private */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
