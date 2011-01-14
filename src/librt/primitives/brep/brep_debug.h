/*                     B R E P _ D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @addtogroup g_ */
/** @{ */
/** @file brep_debug.h
 *
 * brep debugging utilities
 *
 */

#ifndef __BREP_DEBUG
#define __BREP_DEBUG

#include "common.h"

#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "vmath.h"

#include "brep.h"
#include "dvec.h"

#include "raytrace.h"
#include "rtgeom.h"

#include "../../opennurbs_ext.h"


#define fastf_t double

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

using namespace brlcad;

#include "plot3.h"

#define BLUEVIOLET 138, 43, 226
#define CADETBLUE 95, 159, 159
#define CORNFLOWERBLUE 66, 66, 111
#define LIGHTBLUE 173, 216, 230
#define DARKGREEN 0, 100, 0
#define KHAKI 189, 183, 107
#define FORESTGREEN 34, 139, 34
#define LIMEGREEN 124, 252, 0
#define PALEGREEN 152, 251, 152
#define DARKORANGE 255, 140, 0
#define DARKSALMON 233, 150, 122
#define LIGHTCORAL 240, 128, 128
#define PEACH 255, 218, 185
#define DEEPPINK 255, 20, 147
#define HOTPINK 255, 105, 180
#define INDIANRED 205, 92, 92
#define DARKVIOLET 148, 0, 211
#define MAROON 139, 28, 98
#define GOLDENROD 218, 165, 32
#define DARKGOLDENROD 184, 134, 11
#define LIGHTGOLDENROD 238, 221, 130
#define DARKYELLOW 155, 155, 52
#define LIGHTYELLOW 255, 255, 224
#define RED 255, 0, 0
#define GREEN 0, 255, 0
#define BLUE 0, 0, 255
#define YELLOW 255, 255, 0
#define MAGENTA 255, 0, 255
#define CYAN 0, 255, 255
#define BLACK 0, 0, 0
#define WHITE 255, 255, 255

extern FILE* brep_plot_file(const char *pname = NULL);

#define M_COLOR_PLOT(c) pl_color(brep_plot_file(), c)
#define COLOR_PLOT(r, g, b) pl_color(brep_plot_file(), (r), (g), (b))
#define M_PT_PLOT(p) {				\
	point_t pp, ppp;		        \
	vect_t grow;				\
	VSETALL(grow, 0.01);			\
	VADD2(pp, p, grow);			\
	VSUB2(ppp, p, grow);			\
	pdv_3box(brep_plot_file(), pp, ppp); 	\
    }
#define PT_PLOT(p) {				\
	point_t pp;				\
	VSCALE(pp, p, 1.001);			\
	pdv_3box(brep_plot_file(), p, pp);	\
    }
#define LINE_PLOT(p1, p2) pdv_3move(brep_plot_file(), p1); pdv_3line(brep_plot_file(), p1, p2)
#define BB_PLOT(p1, p2) pdv_3box(brep_plot_file(), p1, p2)


void plotsurfaceleafs(SurfaceTree* surf);
void plotleaf3d(BBNode* bb);
void plotleafuv(BBNode* bb);
void plottrim(ON_BrepFace &face, struct bn_vlblock *vbp);
void plottrim(const ON_Curve &curve, double from, double to);
void plottrim(ON_Curve &curve);

#endif
/** @} */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
