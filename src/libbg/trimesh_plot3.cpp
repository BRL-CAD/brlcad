/*                 T R I M E S H _ P L O T 3 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#include "common.h"
#include "string.h" /* for memcpy */

#include <set>
#include <map>
#include <unordered_set>

#include "bu/bitv.h"
#include "bu/log.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bv/plot3.h"
#include "bg/trimesh.h"
#include "bg/plane.h"

#define BB_PLOT_2D(min, max) {         \
    fastf_t pt[4][3];                  \
    VSET(pt[0], max[X], min[Y], 0);    \
    VSET(pt[1], max[X], max[Y], 0);    \
    VSET(pt[2], min[X], max[Y], 0);    \
    VSET(pt[3], min[X], min[Y], 0);    \
    pdv_3move(plot_file, pt[0]); \
    pdv_3cont(plot_file, pt[1]); \
    pdv_3cont(plot_file, pt[2]); \
    pdv_3cont(plot_file, pt[3]); \
    pdv_3cont(plot_file, pt[0]); \
}

static void
plot_point2d_t(FILE *plot_file, const point2d_t *p, double r)
{
    point_t origin, bnp;
    VSET(origin, (*p)[X], (*p)[Y], 0);
    pdv_3move(plot_file, origin);

    VSET(bnp, origin[X]+r, origin[Y], 0);
    pdv_3cont(plot_file, bnp);
    pdv_3cont(plot_file, origin);
    VSET(bnp, origin[X]-r, origin[Y], 0);
    pdv_3cont(plot_file, bnp);
    pdv_3cont(plot_file, origin);
    VSET(bnp, origin[X], origin[Y]+r, 0);
    pdv_3cont(plot_file, bnp);
    pdv_3cont(plot_file, origin);
    VSET(bnp, origin[X], origin[Y]-r, 0);
    pdv_3cont(plot_file, bnp);

    pdv_3cont(plot_file, origin);
}

int
pntset_2d_plot3(const char *fname, std::unordered_set<int> &pinds, const point2d_t *p, struct bu_color *c)
{
    /* If inputs are insufficient, we can't plot */
    if (!fname || !p | !c)
	return BRLCAD_ERROR;

    /* Note - deliberately assuming we may be adding to an existing plot file */
    FILE *plot_file = fopen (fname, "a");
    if (!plot_file)
	return BRLCAD_ERROR;

    // Write color
    if (c)
	pl_color_buc(plot_file, c);

    // Get the bounding box
    point2d_t bmin = {INFINITY, INFINITY};
    point2d_t bmax = {-INFINITY, -INFINITY};
    std::unordered_set<int>::iterator p_it;
    for (p_it = pinds.begin(); p_it != pinds.end(); p_it++) {
	V2MINMAX(bmin, bmax, p[*p_it]);
    }
    fastf_t bdist = DIST_PNT2_PNT2(bmin, bmax);

    // Assemble the set of all points, and then remove any that are
    // actively used in the faces.  What's left are unused points
    if (pinds.size()) {
	for (p_it = pinds.begin(); p_it != pinds.end(); p_it++) {
	    plot_point2d_t(plot_file, &p[*p_it], bdist*0.02);
	}
    }

    fclose(plot_file);

    /* Success */
    return BRLCAD_OK;
}

int
pntset_2d_bbox_plot3(const char *fname, std::unordered_set<int> &pinds, const point2d_t *p, struct bu_color *c)
{
    /* If inputs are insufficient, we can't plot */
    if (!fname || !p)
	return BRLCAD_ERROR;

    /* Note - deliberately assuming we may be adding to an existing plot file */
    FILE *plot_file = fopen (fname, "a");
    if (!plot_file)
	return BRLCAD_ERROR;

    // Write color
    if (c)
	pl_color_buc(plot_file, c);

    // Get the bounding box
    point2d_t bmin = {INFINITY, INFINITY};
    point2d_t bmax = {-INFINITY, -INFINITY};
    std::unordered_set<int>::iterator p_it;
    for (p_it = pinds.begin(); p_it != pinds.end(); p_it++) {
	V2MINMAX(bmin, bmax, p[*p_it]);
    }
    BB_PLOT_2D(bmin, bmax);

    fclose(plot_file);

    /* Success */
    return BRLCAD_OK;
}

int
tri_edges_2d_plot3(const char *fname, const int *faces, size_t num_faces, const point2d_t *p, struct bu_color *c)
{
    /* If inputs are insufficient, we can't plot */
    if (!fname || !faces || num_faces == 0 || !p)
	return BRLCAD_ERROR;

    FILE *plot_file = fopen (fname, "a");
    if (!plot_file)
	return BRLCAD_ERROR;

    // Write color
    if (c)
	pl_color_buc(plot_file, c);


    for (size_t i = 0; i < num_faces; i++) {
	point_t tp[3];
	point_t tporig;
	point_t tc = VINIT_ZERO;
	for (int j = 0; j < 3; j++) {
	    int pind = faces[i*3+j];
	    VSET(tp[j], p[pind][X], p[pind][Y], 0);
	    tc[X] += tp[j][X];
	    tc[Y] += tp[j][Y];
	}
	tc[X] = tc[X]/3.0;
	tc[Y] = tc[Y]/3.0;

	for (size_t j = 0; j < 3; j++) {
	    if (j == 0) {
		VMOVE(tporig, tp[j]);
		pdv_3move(plot_file, tp[j]);
	    }
	    pdv_3cont(plot_file, tp[j]);
	}
	pdv_3cont(plot_file, tporig);
    }
    fclose(plot_file);

    /* Success */
    return BRLCAD_OK;
}

int
tri_interior_2d_plot3(const char *fname, const int *faces, size_t num_faces, const point2d_t *p, struct bu_color *c)
{
    /* If inputs are insufficient, we can't plot */
    if (!fname || !faces || num_faces == 0 || !p)
	return BRLCAD_ERROR;

    FILE *plot_file = fopen (fname, "a");
    if (!plot_file)
	return BRLCAD_ERROR;

    // Write color
    if (c)
	pl_color_buc(plot_file, c);


    for (size_t i = 0; i < num_faces; i++) {
	point_t tp[3];
	point_t tc = VINIT_ZERO;
	for (int j = 0; j < 3; j++) {
	    int pind = faces[i*3+j];
	    VSET(tp[j], p[pind][X], p[pind][Y], 0);
	    tc[X] += tp[j][X];
	    tc[Y] += tp[j][Y];
	}
	tc[X] = tc[X]/3.0;
	tc[Y] = tc[Y]/3.0;

	// Interior
	for (size_t j = 0; j < 3; j++) {
	    pdv_3move(plot_file, tp[j]);
	    pdv_3cont(plot_file, tc);
	}

    }

    fclose(plot_file);

    /* Success */
    return BRLCAD_OK;
}

int
tri_normals_2d_plot3(const char *fname, const int *faces, size_t num_faces, const point2d_t *p, struct bu_color *c)
{
    /* If inputs are insufficient, we can't plot */
    if (!fname || !faces || num_faces == 0 || !p)
	return BRLCAD_ERROR;

    FILE *plot_file = fopen (fname, "a");
    if (!plot_file)
	return BRLCAD_ERROR;

    // Write color
    if (c)
	pl_color_buc(plot_file, c);

    for (size_t i = 0; i < num_faces; i++) {
	point_t tp[3];
	point_t tc = VINIT_ZERO;
	for (int j = 0; j < 3; j++) {
	    int pind = faces[i*3+j];
	    VSET(tp[j], p[pind][X], p[pind][Y], 0);
	    tc[X] += tp[j][X];
	    tc[Y] += tp[j][Y];
	}
	tc[X] = tc[X]/3.0;
	tc[Y] = tc[Y]/3.0;

	// The 3D normal indicates CW vs CCW
	point_t p1, p2, p3;
	VSET(p1, tp[0][X], tp[0][Y], 0);
	VSET(p2, tp[1][X], tp[1][Y], 0);
	VSET(p3, tp[2][X], tp[2][Y], 0);
	vect_t vnorm, v1, v2, vtip;
	VSUB2(v1, p2, p1);
	VSUB2(v2, p3, p1);
	VCROSS(vnorm, v1, v2);
	VUNITIZE(vnorm);
	VADD2(vtip, tc, vnorm);
	pdv_3move(plot_file, tc);
	pdv_3cont(plot_file, vtip);
    }

    fclose(plot_file);

    /* Success */
    return BRLCAD_OK;
}

int
bg_trimesh_2d_plot3(const char *fname, const int *faces, size_t num_faces, const point2d_t *p, size_t num_pnts)
{
    unsigned char rgb[3] = {255, 0, 0};
    struct bu_color c = BU_COLOR_INIT_ZERO;

    /* If inputs are insufficient, we can't plot */
    if (!fname || !faces || num_faces == 0 || !p || num_pnts == 0)
	return BRLCAD_ERROR;

    if (bu_file_exists(fname, NULL))
       return BRLCAD_ERROR;

    // Assemble the set of all points
    std::unordered_set<int> pnts;
    for (size_t i = 0; i < num_pnts; i++)
	pnts.insert(i);

    // Plot the bounding box of the points
    rgb[0] = 255;
    rgb[1] = 0;
    rgb[2] = 0;
    bu_color_from_rgb_chars(&c, rgb);
    pntset_2d_bbox_plot3(fname, pnts, p, &c);

    // Remove any that are actively used in the faces.  What's left are unused
    // points - plot them
    for (size_t i = 0; i < num_faces; i++) {
	pnts.erase(faces[i*3+0]);
	pnts.erase(faces[i*3+1]);
	pnts.erase(faces[i*3+2]);
    }
    if (pnts.size()) {
	rgb[0] = 255;
	rgb[1] = 0;
	rgb[2] = 0;
	bu_color_from_rgb_chars(&c, rgb);
	pntset_2d_plot3(fname, pnts, p, &c);
    }

    // Plot triangle interiors
    rgb[0] = 0;
    rgb[1] = 255;
    rgb[2] = 255;
    bu_color_from_rgb_chars(&c, rgb);
    tri_interior_2d_plot3(fname, faces, num_faces, p, &c);

    // Plot triangle edges
    rgb[0] = 255;
    rgb[1] = 255;
    rgb[2] = 0;
    bu_color_from_rgb_chars(&c, rgb);
    tri_edges_2d_plot3(fname, faces, num_faces, p, &c);

    // Plot triangle normals
    rgb[0] = 0;
    rgb[1] = 255;
    rgb[2] = 0;
    bu_color_from_rgb_chars(&c, rgb);
    tri_normals_2d_plot3(fname, faces, num_faces, p, &c);

    /* Success */
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

