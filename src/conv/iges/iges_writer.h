/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef CONV_IGES_WRITER_H
#define CONV_IGES_WRITER_H

#include "common.h"
#include "iges_document.h"
#include "iges_runtime.h"

#include <array>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

class ON_Curve;
class ON_Surface;

namespace brlcad {
namespace iges {

/** A checked sequence of IGES parameter values.  String encoding is separate
 * from numeric formatting so delimiters inside metadata remain literal. */
class ParameterWriter {
public:
    explicit ParameterWriter(int type);
    ParameterWriter &integer(int64_t value);
    ParameterWriter &real(double value);
    ParameterWriter &text(const std::string &value);
    ParameterWriter &empty();
    ParameterWriter &point(const fastf_t *value);
    ParameterWriter &reals(const double *values, size_t count);
    std::string finish() const;

private:
    std::ostringstream stream_;
};

struct EntitySpec {
    explicit EntitySpec(int entity_type) : type(entity_type) {}
    int type;
    int form = 0;
    int status = 0;
    int structure = 0;
    int transform = 0;
    int color = 0;
};

struct ExportOptions {
    enum class Mode { Csg, Faceted, Trimmed };
    Mode mode = Mode::Csg;
    bool flatten_brep = false;
    bool nurbs_facets = false;
    bool verbose = false;
    int processors = 1;
    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    struct bg_tess_tol tessellation = BG_TESS_TOL_INIT_TOL;
};

struct BrepEdge {
    int curve_de = 0;
    size_t start_vertex = 0;
    size_t end_vertex = 0;
};

struct BrepLoopUse {
    enum Kind { Edge = 0, Vertex = 1 };
    int kind = Edge;
    size_t index = 0;
    int orientation = 1;
    int isoparametric = 0;
    int parameter_curve_de = 0;
};

struct ExportedEntity {
    int directory = 0;
    bool brep = false;
    explicit operator bool() const { return directory != 0; }
};

/** One physical IGES file.  Sequence numbers, scratch sections, properties,
 * and root status belong to this writer, never to the input database. */
class Writer {
public:
    Writer(const ExportOptions &options, std::set<std::string> roots);
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    const ExportOptions &options() const { return options_; }
    int entity(const EntitySpec &specification, const ParameterWriter &parameters);
    int named_entity(EntitySpec specification, ParameterWriter &parameters,
	const std::string &name, const struct rt_comb_internal *properties = nullptr);
    int name_property(const std::string &name);
    int transform(const mat_t matrix);
    int instance(ExportedEntity definition, const mat_t matrix);
    int assembly(const std::string &name, const std::vector<int> &members);
    int group(const std::string &name, const std::vector<int> &members);
    int color(const unsigned char rgb[3]);
    void finish(FILE *output, const std::string &source, const std::string &destination);
    void print_statistics() const;
    void omission(const std::string &message);
    size_t omissions() const { return omissions_; }

    int nurbs_surface(int k1, int k2, int m1, int m2, int rational,
	int closed_u, int closed_v, int periodic_u, int periodic_v,
	const double *u_knots, const double *v_knots, const double *weights,
	const double *controls, double u0, double u1, double v0, double v1);
    int nurbs_curve(int k, int m, int rational, int planar, int closed, int periodic,
	const double *knots, const double *weights, const double *controls,
	double v0, double v1, double nx, double ny, double nz);
    int composite_curve(const int *members, int count);
    int curve_on_surface(int surface, int parameter_curve, int model_curve);
    int trimmed_surface(int surface, int outer, const int *inner, int count);
    int vertex_list(const double *vertices, size_t count);
    int edge_list(int vertices, const BrepEdge *edges, size_t count);
    int loop(int vertices, int edges, const BrepLoopUse *uses, size_t count);
    int face(int surface, const int *loops, size_t count, int has_outer);
    int shell(const int *faces, const int *orientations, size_t count);
    int solid(const char *name, int outer_shell, int outer_orientation,
	const int *void_shells, const int *void_orientations, size_t count);

private:
    int attribute_definition();
    int attributes(const struct rt_comb_internal &combination);
    ExportOptions options_;
    std::set<std::string> roots_;
    File directories_;
    File parameters_;
    int directory_sequence_ = 0;
    int parameter_sequence_ = 0;
    int attribute_definition_ = 0;
    size_t omissions_ = 0;
    std::map<int, size_t> type_counts_;
};

ExportedEntity export_brep(Writer &writer, struct rt_db_internal &internal,
    const std::string &name);
/** Only qualified primitive callbacks are part of the IGES dispatch. */
bool supported_primitive(int type);
ExportedEntity export_primitive(Writer &writer, struct rt_db_internal &internal,
    const std::string &name);
ExportedEntity export_nmg(Writer &writer, struct rt_db_internal &internal,
    const std::string &name);
ExportedEntity export_nmg_region(Writer &writer, struct nmgregion &region,
    const std::string &name);
int export_surface(Writer &writer, const ON_Surface &surface);
int export_curve(Writer &writer, const ON_Curve &curve, bool planar = false);

} // namespace iges
} // namespace brlcad
#endif

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
