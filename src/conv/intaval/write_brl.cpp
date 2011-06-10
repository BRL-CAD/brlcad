/*                  W R I T E _ B R L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file write_brl.cpp
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * write primitives into BRL-CAD database
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#include "nmg.h"
#include "rtgeom.h"
#include "write_brl.h"


double IntavalUnitInMm = 0.1;

size_t arb6_counter = 0;
size_t arb8_counter = 0;
size_t pipe_counter = 0;
size_t rcc_counter  = 0;
size_t sph_counter  = 0;
size_t rpp_counter  = 0;
size_t trc_counter  = 0;
size_t bot_counter  = 0;


void addTriangle
(
    int*    faces,
    size_t& num_faces,
    int     a,
    int     b,
    int     c
) {
    // is it a triangle?
    if ((a == b) || (b == c) || (c == a))
        return;

    // search for duplicate triangle
    for(size_t i = 0; i < num_faces; ++i) {
        if (faces[i * 3] == a) {
            if (faces[i * 3 + 1] == b) {
                if (faces[i * 3 + 2] == c)
                    return;
            }
            else if (faces[i * 3 + 1] == c) {
                if (faces[i * 3 + 2] == b)
                    return;
            }
        }
        else if (faces[i * 3] == b) {
            if (faces[i * 3 + 1] == a) {
                if (faces[i * 3 + 2] == c)
                    return;
            }
            else if (faces[i * 3 + 1] == c) {
                if (faces[i * 3 + 2] == a)
                    return;
            }
        }
        else if (faces[i * 3] == c) {
            if (faces[i * 3 + 1] == b) {
                if (faces[i * 3 + 2] == a)
                    return;
            }
            else if (faces[i * 3 + 1] == a) {
                if (faces[i * 3 + 2] == b)
                    return;
            }
        }
    }

    // add a new triangle
    faces[num_faces * 3]     = a;
    faces[num_faces * 3 + 1] = b;
    faces[num_faces * 3 + 2] = c;

    ++num_faces;
}


void smoothBot
(
    rt_wdb* wdbp,
    char*   name
) {
    directory* dp = db_lookup(wdbp->dbip, name, LOOKUP_QUIET);

    if (dp != RT_DIR_NULL) {
        rt_db_internal intern;

        if (rt_db_get_internal(&intern, dp, wdbp->dbip, 0, &rt_uniresource) == ID_BOT) {
            rt_bot_internal* bot = static_cast<rt_bot_internal*>(intern.idb_ptr);

            rt_bot_smooth(bot, name, wdbp->dbip, M_PI / 4.);
                rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource);
        }

        rt_db_free_internal(&intern);
    }
}


void writeTitle
(
    rt_wdb* wdbp,
    char*   string
) {
   mk_id(wdbp, string);
}


void writePipe
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char    name[NAMELEN + 1];
    fastf_t radius = form.radius1 * IntavalUnitInMm;
    bu_list pipePointList;

    mk_pipe_init(&pipePointList);

    for (size_t i = 1; i < form.npts; ++i) {
        point_t pipePoint;

        if (translate)
            VADD2(pipePoint, form.pt[i-1], form.tr_vec)
        else
            VMOVE(pipePoint, form.pt[i-1])

        VSCALE(pipePoint, pipePoint, IntavalUnitInMm)

        mk_add_pipe_pt(&pipePointList, pipePoint, radius, 2 * radius / 3., radius / 2.);
    }

    sprintf(name, "s%lu.pipe", (long unsigned int)++pipe_counter);
    if (mk_pipe(wdbp, name, &pipePointList) == 0) {
        addToRegion(form.compnr, name);

        if (form.s_compnr >= 1000)
            excludeFromRegion(form.s_compnr, name);
    }

    mk_pipe_free(&pipePointList);
}


void writeRectangularBox
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char    name[NAMELEN + 1];
    point_t min, max;

    if (translate) {
        VADD2(min, form.pt[0], form.tr_vec)
        VADD2(max, form.pt[1], form.tr_vec)
    }
    else {
        VMOVE(min, form.pt[0])
        VMOVE(max, form.pt[1])
    }

    VSCALE(min, min, IntavalUnitInMm)
    VSCALE(max, max, IntavalUnitInMm)

    sprintf(name, "s%lu.rpp", (long unsigned int)++rpp_counter);
    mk_rpp(wdbp, name, min, max);
    addToRegion(form.compnr, name);

    if (form.s_compnr >= 1000)
        excludeFromRegion(form.s_compnr, name);
}


void writeSolidBot
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char    name[NAMELEN + 1];
    fastf_t vertices[MAX_NPTS * 3];

    if (translate) {
        for(size_t i = 0; i < form.bot.num_vertices; ++i) {
            vertices[i * 3]     = (form.bot.vertices[i * 3] + form.tr_vec[0]) * IntavalUnitInMm;
            vertices[i * 3 + 1] = (form.bot.vertices[i * 3 + 1] + form.tr_vec[1]) * IntavalUnitInMm;
            vertices[i * 3 + 2] = (form.bot.vertices[i * 3 + 2] + form.tr_vec[2]) * IntavalUnitInMm;
        }
    }
    else {
        for(size_t i = 0; i < form.bot.num_vertices; ++i) {
            vertices[i * 3]     = form.bot.vertices[i * 3] * IntavalUnitInMm;
            vertices[i * 3 + 1] = form.bot.vertices[i * 3 + 1] * IntavalUnitInMm;
            vertices[i * 3 + 2] = form.bot.vertices[i * 3 + 2] * IntavalUnitInMm;
        }
    }

    sprintf(name, "s%lu.sbot", (long unsigned)++bot_counter);

    mk_bot(wdbp,
           name,
           RT_BOT_SOLID,
           RT_BOT_UNORIENTED,
           0,
           form.bot.num_vertices,
           form.bot.num_faces,
           vertices,
           form.bot.faces,
           0,
           0);

    smoothBot(wdbp, name);
    addToRegion(form.compnr, name);

    if (form.s_compnr >= 1000)
        excludeFromRegion(form.s_compnr, name);
}


void writeRingModeBox
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char name[NAMELEN + 1];

    // get the transformed outer points
    vect_t outer[MAX_NPTS];

    if (translate) {
        for(size_t i = 0; i < form.npts; ++i)
            VADD2(outer[i], form.pt[i], form.tr_vec)
    }
    else {
        for(size_t i = 0; i < form.npts; ++i)
            VMOVE(outer[i], form.pt[i])
    }

    for (size_t i1 = 0; i1 < form.npts; ++i1)
        VSCALE(outer[i1], outer[i1], IntavalUnitInMm)

    // compute inner points
    vect_t inner[MAX_NPTS];

    for(size_t i2 = 0; i2 < form.npts; ++i2) {
        vect_t a, b, c;
        VMOVE(c, outer[(i2 - 1) % form.npts])
        VMOVE(a, outer[i2])
        VMOVE(b, outer[(i2 + 1) % form.npts])

        vect_t b_v, c_v;
        VSUB2(b_v, b, a)
        VSUB2(c_v, c, a)

        vect_t n_v;
        VCROSS(n_v, b_v, c_v)

        // with on b_v
        vect_t width_b_v;
        VCROSS(width_b_v, b_v, n_v)

        if (VDOT(width_b_v, c_v) < 0)
            VREVERSE(width_b_v, width_b_v)

        VUNITIZE(width_b_v)
        VSCALE(width_b_v, width_b_v, form.width * IntavalUnitInMm)

        // with on c_v
        vect_t width_c_v;
        VCROSS(width_c_v, c_v, n_v)

        if (VDOT(width_c_v, b_v) < 0)
            VREVERSE(width_c_v, width_c_v)

        VUNITIZE(width_c_v)
        VSCALE(width_c_v, width_c_v, form.width * IntavalUnitInMm)

        // intersection
        VUNITIZE(b_v)
        VUNITIZE(c_v)

        vect_t cb_v;
        VSUB2(cb_v, b_v, c_v);
        fastf_t l_cb_v = MAGNITUDE(cb_v);

        if (!NEAR_ZERO(l_cb_v, VUNITIZE_TOL)) {
            vect_t width_cb_v;
            VSUB2(width_cb_v, width_b_v, width_c_v)

            vect_t s_b_v;
            VSCALE(s_b_v, b_v, MAGNITUDE(width_cb_v) / l_cb_v)

            vect_t res;
            VADD2(res, a, width_c_v)
            VADD2(res, res, s_b_v)

            VMOVE(inner[i2], res)
        }
        else
            VMOVE(inner[i2], outer[i2])
    }

    // bot parameters
    // vertices
    size_t num_vertices = 0;
    int outer_i[MAX_NPTS];
    int inner_i[MAX_NPTS];
    fastf_t vertices[MAX_NPTS * 3];

    for(size_t i3 = 0; i3 < form.npts; ++i3) {
        size_t i = 0;

        // outer
        // search for duplicate vertex
        for(; i < num_vertices; ++i) {
          if (NEAR_EQUAL(outer[i3][0], vertices[3 * i], VUNITIZE_TOL) &&
              NEAR_EQUAL(outer[i3][1], vertices[3 * i + 1], VUNITIZE_TOL) &&
              NEAR_EQUAL(outer[i3][2], vertices[3 * i + 2], VUNITIZE_TOL)) {
                outer_i[i3] = i;
                break;
            }
        }

        if (i == num_vertices) {
	    // add a new vertex
	    vertices[num_vertices * 3]     = outer[i3][0];
	    vertices[num_vertices * 3 + 1] = outer[i3][1];
	    vertices[num_vertices * 3 + 2] = outer[i3][2];

            outer_i[i3] = num_vertices;
	    ++num_vertices;
        }

        // inner
        // search for duplicate vertex
        for(i = 0; i < num_vertices; ++i) {
            if (NEAR_EQUAL(inner[i3][0], vertices[3 * i], VUNITIZE_TOL) &&
                NEAR_EQUAL(inner[i3][1], vertices[3 * i + 1], VUNITIZE_TOL) &&
                NEAR_EQUAL(inner[i3][2], vertices[3 * i + 2], VUNITIZE_TOL)) {
                inner_i[i3] = i;
                break;
            }
        }

        if (i == num_vertices) {
	    // add a new vertex
	    vertices[num_vertices * 3]     = inner[i3][0];
	    vertices[num_vertices * 3 + 1] = inner[i3][1];
	    vertices[num_vertices * 3 + 2] = inner[i3][2];

            inner_i[i3] = num_vertices;
	    ++num_vertices;
        }
    }

    // faces
    size_t num_faces = 0;
    int faces[MAX_TRIANGLES * 3];

    for(size_t i4 = 0; i4 < form.npts; ++i4) {
        size_t nextIndex = (i4 + 1) % form.npts;

        addTriangle(faces, num_faces, outer_i[i4], outer_i[nextIndex], inner_i[i4]);
        addTriangle(faces, num_faces, inner_i[i4], outer_i[nextIndex], inner_i[nextIndex]);
    }

    fastf_t thickness[MAX_TRIANGLES];

    for(size_t i5 = 0; i5 < num_faces; ++i5)
        thickness[i5] = form.thickness * IntavalUnitInMm;

    bu_bitv* faceMode = bu_bitv_new(num_faces);

    sprintf(name, "s%lu.pbot", (long unsigned)++bot_counter);

    mk_bot(wdbp,
           name,
           RT_BOT_PLATE,
           RT_BOT_UNORIENTED,
           0,
           num_vertices,
           num_faces,
           vertices,
           faces,
           thickness,
           faceMode);

    addToRegion(form.compnr, name);

    if (form.s_compnr >= 1000)
        excludeFromRegion(form.s_compnr, name);

    bu_bitv_free(faceMode);
}


void writePlateBot
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char name[NAMELEN + 1];

    fastf_t vertices[MAX_NPTS * 3];

    if (translate) {
        for(size_t i = 0; i < form.bot.num_vertices; ++i) {
            vertices[i * 3]     = (form.bot.vertices[i * 3] + form.tr_vec[0]) * IntavalUnitInMm;
            vertices[i * 3 + 1] = (form.bot.vertices[i * 3 + 1] + form.tr_vec[1]) * IntavalUnitInMm;
            vertices[i * 3 + 2] = (form.bot.vertices[i * 3 + 2] + form.tr_vec[2]) * IntavalUnitInMm;
        }
    }
    else {
        for(size_t i = 0; i<form.bot.num_vertices; ++i) {
            vertices[i * 3]     = form.bot.vertices[i * 3] * IntavalUnitInMm;
            vertices[i * 3 + 1] = form.bot.vertices[i * 3 + 1] * IntavalUnitInMm;
            vertices[i * 3 + 2] = form.bot.vertices[i * 3 + 2] * IntavalUnitInMm;
        }
    }

    fastf_t thickness[MAX_TRIANGLES];

    for(size_t i = 0; i < form.bot.num_faces; ++i)
        thickness[i] = form.thickness * IntavalUnitInMm;

    bu_bitv* faceMode = bu_bitv_new(form.bot.num_faces);

    sprintf(name, "s%lu.pbot", (long unsigned)++bot_counter);

    mk_bot(wdbp,
           name,
           RT_BOT_PLATE,
           RT_BOT_UNORIENTED,
           0,
           form.bot.num_vertices,
           form.bot.num_faces,
           vertices,
           form.bot.faces,
           thickness,
           faceMode);

    addToRegion(form.compnr, name);

    if (form.s_compnr >= 1000)
        excludeFromRegion(form.s_compnr, name);

    bu_bitv_free(faceMode);
}


void writeCone
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char   name[NAMELEN + 1];
    vect_t base, height;

    if (translate)
        VADD2(base, form.pt[0], form.tr_vec)
    else
        VMOVE(base, form.pt[0])

    VSUB2(height, form.pt[1], form.pt[0]);

    VSCALE(base, base, IntavalUnitInMm)
    VSCALE(height, height, IntavalUnitInMm)

    fastf_t radius1 = form.radius1 * IntavalUnitInMm;
    fastf_t radius2 = form.radius2 * IntavalUnitInMm;

    sprintf(name, "s%lu.trc", (long unsigned)++trc_counter);
    mk_trc_h(wdbp, name, base, height, radius1, radius2);
    addToRegion(form.compnr, name);

    if (form.s_compnr >= 1000)
        excludeFromRegion(form.s_compnr, name);
}


void writeCylinder
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char   name[NAMELEN + 1];
    vect_t base, height;

    if (translate)
        VADD2(base, form.pt[0], form.tr_vec)
    else
        VMOVE(base, form.pt[0])

    VSUB2(height, form.pt[1], form.pt[0]);

    VSCALE(base, base, IntavalUnitInMm)
    VSCALE(height, height, IntavalUnitInMm)

    fastf_t radius = form.radius1 * IntavalUnitInMm;

    sprintf(name, "s%lu.rcc", (long unsigned)++rcc_counter);
    mk_rcc(wdbp, name, base, height, radius);
    addToRegion(form.compnr, name);

    if (form.s_compnr >= 1000)
        excludeFromRegion(form.s_compnr, name);
}


void writeArb8
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
) {
    char    name[NAMELEN + 1];
    point_t shuffle[8];

    if (translate) {
        VADD2(shuffle[0], form.pt[0], form.tr_vec)
        VADD2(shuffle[1], form.pt[7], form.tr_vec)
        VADD2(shuffle[2], form.pt[3], form.tr_vec)
        VADD2(shuffle[3], form.pt[1], form.tr_vec)
        VADD2(shuffle[4], form.pt[6], form.tr_vec)
        VADD2(shuffle[5], form.pt[5], form.tr_vec)
        VADD2(shuffle[6], form.pt[4], form.tr_vec)
        VADD2(shuffle[7], form.pt[2], form.tr_vec)
    }
    else {
        VMOVE(shuffle[0], form.pt[0])
        VMOVE(shuffle[1], form.pt[7])
        VMOVE(shuffle[2], form.pt[3])
        VMOVE(shuffle[3], form.pt[1])
        VMOVE(shuffle[4], form.pt[6])
        VMOVE(shuffle[5], form.pt[5])
        VMOVE(shuffle[6], form.pt[4])
        VMOVE(shuffle[7], form.pt[2])
    }

    for (size_t i = 0; i < 8; ++i)
        VSCALE(shuffle[i], shuffle[i], IntavalUnitInMm)

    sprintf(name, "s%lu.arb8", (long unsigned)++arb8_counter);
    mk_arb8(wdbp, name, reinterpret_cast<fastf_t*>(shuffle));
    addToRegion(form.compnr, name);

    if (form.s_compnr >= 1000)
        excludeFromRegion(form.s_compnr, name);
}
