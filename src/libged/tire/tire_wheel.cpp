/*                       T I R E _ W H E E L . C P P
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
/** @file libged/tire_wheel.cpp
 *
 * Wheel and hub CSG emission for the tire command.
 */

#include "common.h"

#include <string>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

#include "tire_private.h"

#define D2R(x) (x * DEG2RAD)

namespace tire_private {

static int
emit_wheel_center(CsgTransaction &builder, const std::string &suffix,
		fastf_t fixing_start_middle, fastf_t fixing_width,
		fastf_t rim_thickness, fastf_t bead_height, fastf_t zhub,
		fastf_t dyhub, fastf_t spigot_diam, int bolts,
		fastf_t bolt_circ_diam, fastf_t bolt_diam)
{
    vect_t height, a, b, c;
    point_t origin, vertex;
    mat_t y;
    int i;
    struct wmember bolthole, boltholes, hubhole, hubholes, innerhub;

    VSET(origin, 0, fixing_start_middle + fixing_width / 4, 0);
    VSET(a, (zhub - 2 * bead_height), 0, 0);
    VSET(b, 0, -dyhub / 3, 0);
    VSET(c, 0, 0, (zhub - 2 * bead_height));
    if (builder.write_ell(object_name("hub.inner", suffix + ".wheel", ".s").c_str(), origin, a, b, c) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(origin, 0, fixing_start_middle + fixing_width / 4 - rim_thickness * 1.5, 0);
    if (builder.write_ell(object_name("hub.inner-cut-1", suffix + ".wheel", ".s").c_str(), origin, a, b, c) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(origin, 0, 0, 0);
    VSET(height, 0, dyhub / 2, 0);
    if (builder.write_rcc(object_name("hub.inner-cut-2", suffix + ".wheel", ".s").c_str(), origin, height, spigot_diam / 2) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, 0, (zhub - (bolt_circ_diam / 2 + bolt_diam / 2)) / 1.25);
    VSET(height, 0, dyhub / 2, 0);
    if (builder.write_rcc(object_name("hub.hole-source", suffix + ".wheel", ".s").c_str(), vertex, height,
	   (zhub - (bolt_circ_diam / 2 + bolt_diam / 2)) / 6.5) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&hubhole.l);
    BU_LIST_INIT(&hubholes.l);
    for (i = 1; i <= 10; i++) {
	tire_y_rot_mat(&y, D2R(i * 360 / 10));
	add_member(object_name("hub.hole-source", suffix + ".wheel", ".s"), &hubhole.l, y, WMOP_UNION);
	const std::string hub_hole_name = indexed_object_name("hub-hole", suffix + ".wheel", i, ".c");
	if (builder.write_lcomb(hub_hole_name.c_str(), &hubhole, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	    return BRLCAD_ERROR;
	add_member(hub_hole_name, &hubholes.l, WMOP_UNION);
	(void)BU_LIST_POP_T(&hubhole.l, struct wmember);
    }
    if (builder.write_lcomb(object_name("hub-holes", suffix + ".wheel", ".c").c_str(), &hubholes, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, 0, bolt_circ_diam / 2);
    VSET(height, 0, dyhub / 2, 0);
    if (builder.write_rcc(object_name("bolt-hole-source", suffix + ".wheel", ".s").c_str(), vertex, height, bolt_diam / 2) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&bolthole.l);
    BU_LIST_INIT(&boltholes.l);
    for (i = 1; i <= bolts; i++) {
	tire_y_rot_mat(&y, D2R(i * 360 / bolts));
	add_member(object_name("bolt-hole-source", suffix + ".wheel", ".s"), &bolthole.l, y, WMOP_UNION);
	const std::string bolt_hole_name = indexed_object_name("bolt-hole", suffix + ".wheel", i, ".c");
	if (builder.write_lcomb(bolt_hole_name.c_str(), &bolthole, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	    return BRLCAD_ERROR;
	add_member(bolt_hole_name, &boltholes.l, WMOP_UNION);
	(void)BU_LIST_POP_T(&bolthole.l, struct wmember);
    }
    if (builder.write_lcomb(object_name("bolt-holes", suffix + ".wheel", ".c").c_str(), &boltholes, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&innerhub.l);
    add_member(object_name("hub.inner", suffix + ".wheel", ".s"), &innerhub.l, WMOP_UNION);
    add_member(object_name("hub.inner-cut-1", suffix + ".wheel", ".s"), &innerhub.l, WMOP_SUBTRACT);
    add_member(object_name("hub.inner-cut-2", suffix + ".wheel", ".s"), &innerhub.l, WMOP_SUBTRACT);
    add_member(object_name("hub-holes", suffix + ".wheel", ".c"), &innerhub.l, WMOP_SUBTRACT);
    add_member(object_name("bolt-holes", suffix + ".wheel", ".c"), &innerhub.l, WMOP_SUBTRACT);
    return builder.write_lcomb(object_name("hub", suffix + ".wheel", ".c").c_str(), &innerhub, 0, NULL, NULL, NULL, 0);
}

int
make_wheel_rims(CsgTransaction &builder, const std::string &suffix, fastf_t dyhub,
	      fastf_t zhub, int bolts, fastf_t bolt_diam,
	      fastf_t bolt_circ_diam, fastf_t spigot_diam,
	      fastf_t fixing_offset, fastf_t bead_height,
	      fastf_t bead_width, fastf_t rim_thickness)
{
    struct wmember wheelrimsolid, wheelrimcut;
    struct wmember wheelrim, wheel;
    fastf_t inner_width, left_width, right_width, fixing_width;
    fastf_t inner_width_left_start, inner_width_right_start;
    fastf_t fixing_width_left_trans;
    fastf_t fixing_width_right_trans;
    fastf_t fixing_width_middle;
    fastf_t fixing_start_left, fixing_start_right, fixing_start_middle;
    unsigned char rgb[3];
    vect_t normal, height;
    point_t vertex;

    VSET(rgb, 217, 217, 217);

    VSET(vertex, 0, -dyhub / 2, 0);
    VSET(height, 0, bead_width, 0);
    if (builder.write_rcc(object_name("bead.left", suffix + ".wheel", ".s").c_str(), vertex, height, zhub) != BRLCAD_OK ||
	builder.write_rcc(object_name("bead.left-cut", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - rim_thickness) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, dyhub / 2, 0);
    VSET(height, 0, -bead_width, 0);
    if (builder.write_rcc(object_name("bead.right", suffix + ".wheel", ".s").c_str(), vertex, height, zhub) != BRLCAD_OK ||
	builder.write_rcc(object_name("bead.right-cut", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - rim_thickness) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, -dyhub / 2 + bead_width, 0);
    VSET(normal, 0, 1, 0);
    if (builder.write_cone(object_name("bead.left-transition", suffix + ".wheel", ".s").c_str(), vertex, normal, bead_width, zhub, zhub-bead_height) != BRLCAD_OK ||
	builder.write_cone(object_name("bead.left-transition-cut", suffix + ".wheel", ".s").c_str(), vertex, normal, bead_width, zhub-rim_thickness, zhub-bead_height-rim_thickness) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, dyhub / 2 - bead_width, 0);
    VSET(normal, 0, -1, 0);
    if (builder.write_cone(object_name("bead.right-transition", suffix + ".wheel", ".s").c_str(), vertex, normal, bead_width, zhub, zhub-bead_height) != BRLCAD_OK ||
	builder.write_cone(object_name("bead.right-transition-cut", suffix + ".wheel", ".s").c_str(), vertex, normal, bead_width, zhub-rim_thickness, zhub-bead_height-rim_thickness) != BRLCAD_OK)
	return BRLCAD_ERROR;

    inner_width = dyhub - 4 * bead_width;
    inner_width_left_start = -dyhub / 2 + 2 * bead_width;
    inner_width_right_start = dyhub / 2 - 2 * bead_width;
    fixing_width = .25 * inner_width;
    fixing_width_left_trans = .25 * fixing_width;
    fixing_width_right_trans = .25 * fixing_width;
    fixing_width_middle = .5 * fixing_width;
    fixing_start_middle = fixing_offset - fixing_width_middle / 2;
    fixing_start_left = fixing_start_middle - fixing_width_left_trans;
    fixing_start_right = fixing_offset + fixing_width_middle;
    right_width = inner_width_right_start - fixing_start_left - fixing_width;
    left_width = inner_width_left_start - fixing_start_left;

    VSET(vertex, 0, inner_width_left_start, 0);
    VSET(height, 0, -left_width, 0);
    if (builder.write_rcc(object_name("rim-inner.left", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - bead_height) != BRLCAD_OK ||
	builder.write_rcc(object_name("rim-inner.left-cut", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - rim_thickness - bead_height) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, inner_width_right_start, 0);
    VSET(height, 0, -right_width, 0);
    if (builder.write_rcc(object_name("rim-inner.right", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - bead_height) != BRLCAD_OK ||
	builder.write_rcc(object_name("rim-inner.right-cut", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - rim_thickness - bead_height) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, fixing_start_left, 0);
    VSET(normal, 0, 1, 0);
    if (builder.write_cone(object_name("fixing.left-transition", suffix + ".wheel", ".s").c_str(), vertex, normal, fixing_width_left_trans,
	    zhub - bead_height, zhub - 2 * bead_height) != BRLCAD_OK ||
	builder.write_cone(object_name("fixing.left-transition-cut", suffix + ".wheel", ".s").c_str(), vertex, normal, fixing_width_left_trans,
	    zhub - bead_height - rim_thickness,
	    zhub - 2 * bead_height - rim_thickness) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, fixing_start_right, 0);
    VSET(normal, 0, -1, 0);
    if (builder.write_cone(object_name("fixing.right-transition", suffix + ".wheel", ".s").c_str(), vertex, normal, fixing_width_right_trans,
	    zhub - bead_height, zhub - 2 * bead_height) != BRLCAD_OK ||
	builder.write_cone(object_name("fixing.right-transition-cut", suffix + ".wheel", ".s").c_str(), vertex, normal, fixing_width_right_trans,
	    zhub - bead_height - rim_thickness,
	    zhub - 2 * bead_height - rim_thickness) != BRLCAD_OK)
	return BRLCAD_ERROR;

    VSET(vertex, 0, fixing_start_middle, 0);
    VSET(height, 0, fixing_width_middle, 0);
    if (builder.write_rcc(object_name("fixing.inner", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - 2 * bead_height) != BRLCAD_OK ||
	builder.write_rcc(object_name("fixing.inner-cut", suffix + ".wheel", ".s").c_str(), vertex, height, zhub - rim_thickness - 2 * bead_height) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&wheelrimsolid.l);
    add_member(object_name("bead.left", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("bead.right", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("bead.left-transition", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("bead.right-transition", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("rim-inner.left", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("rim-inner.right", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("fixing.left-transition", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("fixing.right-transition", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    add_member(object_name("fixing.inner", suffix + ".wheel", ".s"), &wheelrimsolid.l, WMOP_UNION);
    if (builder.write_lcomb(object_name("rim-solid", suffix + ".wheel", ".c").c_str(), &wheelrimsolid, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&wheelrimcut.l);
    add_member(object_name("bead.left-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("bead.right-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("bead.left-transition-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("bead.right-transition-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("rim-inner.left-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("rim-inner.right-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("fixing.left-transition-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("fixing.right-transition-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    add_member(object_name("fixing.inner-cut", suffix + ".wheel", ".s"), &wheelrimcut.l, WMOP_UNION);
    if (builder.write_lcomb(object_name("rim-cut", suffix + ".wheel", ".c").c_str(), &wheelrimcut, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&wheelrim.l);
    add_member(object_name("rim-solid", suffix + ".wheel", ".c"), &wheelrim.l, WMOP_UNION);
    add_member(object_name("rim-cut", suffix + ".wheel", ".c"), &wheelrim.l, WMOP_SUBTRACT);
    if (builder.write_lcomb(object_name("rim", suffix + ".wheel", ".c").c_str(), &wheelrim, 0, NULL, NULL, NULL, 0) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (emit_wheel_center(builder, suffix, fixing_start_middle, fixing_width,
		    rim_thickness, bead_height, zhub, dyhub, spigot_diam,
		    bolts, bolt_circ_diam, bolt_diam) != BRLCAD_OK)
	return BRLCAD_ERROR;

    BU_LIST_INIT(&wheel.l);
    add_member(object_name("hub", suffix + ".wheel", ".c"), &wheel.l, WMOP_UNION);
    add_member(object_name("rim", suffix + ".wheel", ".c"), &wheel.l, WMOP_UNION);
    return builder.write_lcomb(object_name("", suffix + ".wheel", ".r").c_str(), &wheel, 1, "plastic", "di=.8 sp=.2", rgb, 0);
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
