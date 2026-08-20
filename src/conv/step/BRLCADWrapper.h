/*                 BRLCADWrapper.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/BRLCADWrapper.h
 *
 * Class definition for C++ wrapper to BRL-CAD database functions.
 *
 */

#ifndef CONV_STEP_BRLCADWRAPPER_H
#define CONV_STEP_BRLCADWRAPPER_H

#include "common.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

/* brlcad headers */
#include <bu/list.h>
#include <wdb.h>
#include <raytrace.h>

#include "STEPDocument.h"


class ON_Brep;

typedef std::map<std::string, struct bu_list *> MAP_OF_BU_LIST_HEADS;

namespace brlcad {
namespace step {

/** Presentation and semantic information for an annotation stroke. */
struct AnnotationLineStyle {
    uint32_t role = RT_ANNOT_ROLE_GEOMETRY;
    uint32_t line_pattern = RT_ANNOT_LINE_CONTINUOUS;
    bool has_width = false;
    double width = 1.0;
    bool has_color = false;
    std::array<unsigned char, 4> color = {{0, 0, 0, 255}};
    std::string symbol;
};

/** Text content for one persistent BRL-CAD annotation segment. */
struct AnnotationText {
    size_t reference_vertex = 0;
    std::string label;
    double size = 1.0;
    double rotation = 0.0;
    int relative_position = RT_TXT_POS_BL;
    std::string font;
    double x_scale = 1.0;
    double xy_scale = 0.0;
    double yx_scale = 0.0;
    double y_scale = 1.0;
    uint32_t role = RT_ANNOT_ROLE_TEXT;
    uint32_t style_flags = 0;
    std::string symbol;
    bool has_color = false;
    std::array<unsigned char, 4> color = {{0, 0, 0, 255}};
};

/** A filled annotation area: loop zero is the boundary and later loops are
 * holes.  Indices refer to the annotation vertex array. */
struct AnnotationFill {
    std::vector<std::vector<size_t> > loops;
    uint32_t role = RT_ANNOT_ROLE_GEOMETRY;
    std::string symbol;
    bool has_color = false;
    std::array<unsigned char, 4> color = {{0, 0, 0, 255}};
};

}
}

class BRLCADWrapper
{
private:
    struct CombinationProperties {
	bool is_region = false;
	int64_t step_id = 0;
	std::string original_name;
	bool has_style = false;
	brlcad::step::Style style;
	std::map<std::string, std::string> attributes;
    };

    std::string filename;
    struct rt_wdb *outfp;
    struct db_i *dbip;
    MAP_OF_BU_LIST_HEADS heads;
    std::map<std::string, CombinationProperties> combination_properties;
    std::map<std::string, int64_t> allocated_names;
    uint64_t anonymous_name_counter;

public:
    int dry_run;
    BRLCADWrapper();
    virtual ~BRLCADWrapper();
    bool load(std::string &filename);
    bool OpenFile(std::string &filename);
    bool WriteHeader();
    bool WriteSphere(const std::string &name, const double *center, double radius,
	int64_t step_id = 0, const std::string &original_name = std::string());
    bool WriteEllipsoid(const std::string &name, const double *center,
	const double *a, const double *b, const double *c,
	int64_t step_id = 0, const std::string &original_name = std::string());
    bool WriteRcc(const std::string &name, const double *base, const double *height,
	double radius, int64_t step_id = 0, const std::string &original_name = std::string());
    bool WriteTgc(const std::string &name, const double *base, const double *height,
	const double *a, const double *b, const double *c, const double *d,
	int64_t step_id = 0, const std::string &original_name = std::string());
    bool WriteTorus(const std::string &name, const double *center, const double *normal,
	double major_radius, double minor_radius, int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WriteHalf(const std::string &name, const double *normal, double distance,
	int64_t step_id = 0, const std::string &original_name = std::string());
    bool WriteArb8(const std::string &name, const double *points, int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WritePoints(const std::string &name, size_t count, const double *points,
	double display_scale = 1.0, int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WriteDatum(const std::string &name, rt_datum_type type,
	const double *origin, const double *direction = NULL,
	const double *x_axis = NULL, const double *y_axis = NULL,
	double display_scale = 10.0, int64_t step_id = 0,
	const std::string &original_name = std::string(),
	const std::string &identifier = std::string(),
	const std::string &description = std::string(),
	rt_datum_role role = RT_DATUM_ROLE_REFERENCE);
    bool WriteDatumPlane(const std::string &name, const double *origin,
	const double *normal, double display_scale = 10.0, int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WriteAnnotation(const std::string &name, const double *origin,
	const double *u_axis, const double *v_axis,
	const std::vector<std::array<double, 2> > &vertices,
	const std::vector<std::pair<size_t, size_t> > &lines,
	int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WriteAnnotation(const std::string &name, const double *origin,
	const double *u_axis, const double *v_axis,
	const std::vector<std::array<double, 2> > &vertices,
	const std::vector<std::pair<size_t, size_t> > &lines,
	const std::vector<brlcad::step::AnnotationText> &texts,
	int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WriteAnnotation(const std::string &name, const double *origin,
	const double *u_axis, const double *v_axis,
	const std::vector<std::array<double, 2> > &vertices,
	const std::vector<std::pair<size_t, size_t> > &lines,
	const std::vector<brlcad::step::AnnotationText> &texts,
	const std::vector<brlcad::step::AnnotationFill> &fills,
	int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WriteAnnotation(const std::string &name, const double *origin,
	const double *u_axis, const double *v_axis,
	const std::vector<std::array<double, 2> > &vertices,
	const std::vector<std::pair<size_t, size_t> > &lines,
	const std::vector<brlcad::step::AnnotationLineStyle> &line_styles,
	const std::vector<brlcad::step::AnnotationText> &texts,
	const std::vector<brlcad::step::AnnotationFill> &fills,
	int64_t step_id = 0,
	const std::string &original_name = std::string());
    bool WriteBrep(std::string name, ON_Brep *brep, mat_t &mat, bool is_region = true,
	int64_t step_id = 0, const std::string &original_name = std::string(),
	const brlcad::step::Style *style = NULL);
    bool WriteBot(std::string name, size_t num_vertices, size_t num_faces,
	fastf_t *vertices, int *faces, mat_t &mat, int64_t step_id = 0,
	const std::string &original_name = std::string(),
	const brlcad::step::Style *style = NULL, int mode = RT_BOT_SOLID,
	int orientation = RT_BOT_CCW,
	const std::string &representation = "FACETED_BREP");
    bool WriteCombs();
    /** Schedule a combination even when it has no converted members.  Product
     * definitions use this to remain valid assembly targets when all of their
     * representation items are skipped. */
    bool EnsureCombination(const std::string &combname);
    bool AddMember(const std::string &combname, const std::string &member, mat_t mat,
	int operation = WMOP_UNION);
    bool SetCombinationProperties(const std::string &combname, bool is_region,
	int64_t step_id = 0, const std::string &original_name = std::string(),
	const brlcad::step::Style *style = NULL);
    bool SetCombinationAttribute(const std::string &combname,
	const std::string &key, const std::string &value);
    std::string ReplaceAccented( std::string &str );
    std::string CleanBRLCADName(std::string &name);
    std::string GetBRLCADName(std::string &name);
    std::string StableBRLCADName(const std::string &name, int64_t step_id);
    bool SetAttribute(const std::string &object, const std::string &key, const std::string &value);
    bool SetAttributes(const std::string &object,
	const std::map<std::string, std::string> &attributes);
    /** Retain an arbitrary-size flattened STEP graph in a versioned binary
     * object.  Small graphs are also mirrored on _GLOBAL for compatibility. */
    bool WriteSTEPMetadata(
	const std::map<std::string, std::string> &attributes);
    /** Copy one already-serialized object from another open BRL-CAD database
     * without importing and re-exporting its geometry. */
    bool CopyObjectFrom(BRLCADWrapper &source, const std::string &object);
    static void getRandomColor(unsigned char *rgb);
    struct db_i * GetDBIP();
    bool Close();
};


#endif /* CONV_STEP_BRLCADWRAPPER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
