/*                   G E D _ B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file ged_brep.h
 *
 * Private header for libged brep cmd.
 *
 */

#ifndef LIBGED_BREP_GED_PRIVATE_H
#define LIBGED_BREP_GED_PRIVATE_H

#include "common.h"

#include <set>
#include <string>
#include <time.h>

#include "bu/opt.h"
#include "bn/plot3.h"
#include "bu/color.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "ged.h"

__BEGIN_DECLS

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
#define PURERED 255, 0, 0
#define GREEN 0, 255, 0
#define BLUE 0, 0, 255
#define YELLOW 255, 255, 0
#define MAGENTA 255, 0, 255
#define CYAN 0, 255, 255
#define BLACK 0, 0, 0
#define WHITE 255, 255, 255

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_brep_info {
    struct ged *gedp = NULL;
    struct rt_db_internal intern;
    struct directory *dp = NULL;
    struct bv_vlblock *vbp = NULL;
    struct bu_color *color = NULL;
    int verbosity;
    int plotres;
    std::string solid_name;
    const struct bu_cmdtab *cmds = NULL;
    struct bu_opt_desc *gopts = NULL;
};

int
_brep_indices(std::set<int> &elements, struct bu_vls *vls, int argc, const char **argv);

GED_EXPORT extern int _ged_brep_to_csg(struct ged *gedp, const char *obj_name, int verify);

GED_EXPORT extern int brep_info(struct bu_vls *vls, const ON_Brep *brep, int argc, const char **argv);
GED_EXPORT extern int brep_pick(struct _ged_brep_info *gb, int argc, const char **argv);
GED_EXPORT extern int brep_plot(struct _ged_brep_info *gb, int argc, const char **argv);
GED_EXPORT extern int brep_tikz(struct _ged_brep_info *gb, const char *outfile);
GED_EXPORT extern int brep_valid(struct bu_vls *vls, struct rt_db_internal *intern, int argc, const char **argv);

GED_EXPORT extern int brep_conversion(struct rt_db_internal* in, struct rt_db_internal* out, const struct db_i *dbip);
GED_EXPORT extern int brep_conversion_comb(struct rt_db_internal *old_internal, const char *name, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm);

GED_EXPORT extern int brep_intersect_point_point(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
GED_EXPORT extern int brep_intersect_point_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
GED_EXPORT extern int brep_intersect_point_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
GED_EXPORT extern int brep_intersect_curve_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
GED_EXPORT extern int brep_intersect_curve_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
GED_EXPORT extern int brep_intersect_surface_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j, struct bv_vlblock *vbp);

GED_EXPORT extern int ged_dplot_core(struct ged *gedp, int argc, const char *argv[]);

using namespace brlcad;
void
plotface(const ON_BrepFace &face, struct bv_vlblock *vbp, int plotres, bool dim3d, const int red, const int green, const int blue);
void
plotsurface(const ON_Surface &surf, struct bv_vlblock *vbp, int isocurveres, int gridres, const int red, const int green, const int blue);
void
plotcurve(const ON_Curve &curve, struct bv_vlblock *vbp, int plotres, const int red, const int green, const int blue);
void
plotcurveonsurface(const ON_Curve *curve, const ON_Surface *surface, struct bv_vlblock *vbp, int plotres, const int red, const int green, const int blue);
void
plotpoint(const ON_3dPoint &point, struct bv_vlblock *vbp, const int red, const int green, const int blue);

__END_DECLS

#endif /* LIBGED_BREP_GED_PRIVATE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
