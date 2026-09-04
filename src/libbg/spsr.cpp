/*                         S P S R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file spsr.cpp
 *
 * Screened Poisson surface reconstruction for fixed and adaptive oriented
 * point sets.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vmath.h"
#include "bu/malloc.h"
#include "bg/spsr.h"
#include "RTree.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wall"
#elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Weverything"
#endif
#include "SPSR/Reconstructors.h"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#elif defined(__clang__)
#  pragma clang diagnostic pop
#endif

using namespace PoissonRecon;

namespace {

using clock_type = std::chrono::steady_clock;
using real_type = fastf_t;

constexpr size_t SPSR_MAX_HINTS = 4096;
constexpr size_t SPSR_MIN_NEIGHBORS = 4;
constexpr double SPSR_NORMAL_ANGLE_DEGREES = 15.0;
constexpr double SPSR_DEDUP_FRACTION = 0.25;
constexpr int SPSR_MAX_DEPTH = 30;

struct spsr_mesh {
    std::vector<int> faces;
    std::vector<real_type> vertices;
};

template<typename Real, unsigned int Dim>
struct point_stream : public Reconstructor::InputOrientedSampleStream<Real, Dim>
{
    point_stream(const std::vector<struct bg_3d_spsr_sample> &samples)
        : current(0), input(samples)
    {
    }

    void reset(void)
    {
        current = 0;
    }

    bool read(Point<Real, Dim> &point, Point<Real, Dim> &normal)
    {
        if (current >= input.size())
            return false;

        for (size_t dimension = 0; dimension < Dim; dimension++) {
            point[dimension] = input[current].point[dimension];
            normal[dimension] = input[current].normal[dimension];
        }
        current++;
        return true;
    }

  private:
    size_t current;
    const std::vector<struct bg_3d_spsr_sample> &input;
};

template<typename Index>
struct polygon_stream : public Reconstructor::OutputFaceStream<2>
{
    polygon_stream(std::vector<std::vector<Index>> &output) : polygons(output)
    {
    }

    size_t size(void) const
    {
        return polygons.size();
    }

    size_t write(const std::vector<node_index_type> &polygon)
    {
        std::vector<Index> converted(polygon.size());
        for (size_t i = 0; i < polygon.size(); i++)
            converted[i] = static_cast<Index>(polygon[i]);
        polygons.push_back(std::move(converted));
        return polygons.size() - 1;
    }

  private:
    std::vector<std::vector<Index>> &polygons;
};

template<typename Real, unsigned int Dim>
struct vertex_stream : public Reconstructor::OutputLevelSetVertexStream<Real, Dim>
{
    vertex_stream(std::vector<Real> &output) : coordinates(output)
    {
    }

    size_t size(void) const
    {
        return coordinates.size() / Dim;
    }

    size_t write(const Point<Real, Dim> &point, const Point<Real, Dim> &,
        const Real &)
    {
        for (unsigned int dimension = 0; dimension < Dim; dimension++)
            coordinates.push_back(point[dimension]);
        return coordinates.size() / Dim - 1;
    }

  private:
    std::vector<Real> &coordinates;
};

static double
elapsed_seconds(const clock_type::time_point &start)
{
    return std::chrono::duration<double>(clock_type::now() - start).count();
}

static bool
finite_vector(const fastf_t *vector)
{
    return std::isfinite(vector[X]) && std::isfinite(vector[Y]) &&
        std::isfinite(vector[Z]);
}

static bool
valid_options(const struct bg_3d_spsr_opts &options)
{
    if (options.degree != BG_3D_SPSR_DEFAULT_DEGREE ||
        options.btype != BG_3D_SPSR_BOUNDARY_NEUMANN ||
        options.max_memory_GB != BG_3D_SPSR_DEFAULT_MAX_MEM ||
        options.threads != BG_3D_SPSR_DEFAULT_THREADS ||
        !NEAR_ZERO(options.confidence, SMALL_FASTF) ||
        !NEAR_ZERO(options.confidence_bias, SMALL_FASTF))
        return false;

    if (options.depth < 1 || options.depth > SPSR_MAX_DEPTH ||
        options.kerneldepth < 0 || options.iterations < 0 ||
        options.full_depth < 0 || options.base_depth < 0 ||
        options.baseVcycles < 0 || options.samples_per_node <= 0.0 ||
        options.scale < 1.0 || options.width < 0.0 ||
        options.cgsolver_accuracy <= 0.0 || options.point_weight < 0.0)
        return false;

    return options.nonManifold >= 0 && options.nonManifold <= 1 &&
        options.linearFit >= 0 && options.linearFit <= 1 &&
        options.exact >= 0 && options.exact <= 1;
}

static void
set_solver_options(
    Reconstructor::Poisson::SolutionParameters<real_type> &solver,
    Reconstructor::LevelSetExtractionParameters &extraction,
    const struct bg_3d_spsr_opts &options)
{
    solver.depth = static_cast<unsigned int>(options.depth);
    solver.kernelDepth = options.kerneldepth ?
        static_cast<unsigned int>(options.kerneldepth) :
        static_cast<unsigned int>(-1);
    solver.iters = options.iterations ?
        static_cast<unsigned int>(options.iterations) : 8u;
    solver.fullDepth = options.full_depth ?
        static_cast<unsigned int>(options.full_depth) :
        std::min(5u, solver.depth);
    solver.baseDepth = options.base_depth ?
        static_cast<unsigned int>(options.base_depth) :
        static_cast<unsigned int>(-1);
    solver.baseVCycles = options.baseVcycles ?
        static_cast<unsigned int>(options.baseVcycles) : 1u;
    solver.samplesPerNode = options.samples_per_node;
    solver.scale = options.scale;
    solver.width = options.width;
    solver.cgSolverAccuracy = options.cgsolver_accuracy;
    solver.pointWeight = options.point_weight;
    solver.exactInterpolation = options.exact != 0;
    solver.confidence = false;
    solver.verbose = false;
    solver.showResidual = false;

    extraction.forceManifold = options.nonManifold == 0;
    extraction.linearFit = options.linearFit != 0;
    extraction.polygonMesh = false;
    extraction.verbose = false;
}

static fastf_t
sample_extent(const std::vector<struct bg_3d_spsr_sample> &samples)
{
    point_t minimum;
    point_t maximum;
    VSETALL(minimum, INFINITY);
    VSETALL(maximum, -INFINITY);
    for (const auto &sample : samples)
        VMINMAX(minimum, maximum, sample.point);
    return DIST_PNT_PNT(minimum, maximum);
}

class sample_set
{
  public:
    sample_set(fastf_t tolerance, size_t limit) :
        distance_tolerance(tolerance), distance_tolerance_sq(tolerance * tolerance),
        point_limit(limit)
    {
    }

    bool append(const struct bg_3d_spsr_sample &input)
    {
        if (!finite_vector(input.point) || !finite_vector(input.normal))
            return false;

        struct bg_3d_spsr_sample sample = input;
        fastf_t normal_length = MAGNITUDE(sample.normal);
        if (!std::isfinite(normal_length) || normal_length <= VUNITIZE_TOL)
            return false;
        VSCALE(sample.normal, sample.normal, 1.0 / normal_length);

        double minimum[3];
        double maximum[3];
        for (size_t dimension = 0; dimension < 3; dimension++) {
            minimum[dimension] = sample.point[dimension] - distance_tolerance;
            maximum[dimension] = sample.point[dimension] + distance_tolerance;
        }

        bool duplicate = false;
        index.Search(minimum, maximum,
            [&sample, this, &duplicate](const size_t &candidate, void *) {
                if (DIST_PNT_PNT_SQ(sample.point, values[candidate].point) <=
                    distance_tolerance_sq) {
                    duplicate = true;
                    return false;
                }
                return true;
            }, NULL);
        if (duplicate || (point_limit && values.size() >= point_limit))
            return false;

        size_t sample_index = values.size();
        values.push_back(sample);
        double point[3] = {
            sample.point[X], sample.point[Y], sample.point[Z]
        };
        index.Insert(point, point, sample_index);
        return true;
    }

    const std::vector<struct bg_3d_spsr_sample> &samples(void) const
    {
        return values;
    }

    bool full(void) const
    {
        return point_limit && values.size() >= point_limit;
    }

  private:
    fastf_t distance_tolerance;
    fastf_t distance_tolerance_sq;
    size_t point_limit;
    std::vector<struct bg_3d_spsr_sample> values;
    RTree<size_t, double, 3> index;
};

static void
append_hint(std::vector<struct bg_3d_spsr_refinement_hint> &hints,
    const point_t point, const vect_t normal, fastf_t error,
    unsigned int reasons)
{
    struct bg_3d_spsr_refinement_hint hint = {};
    VMOVE(hint.point, point);
    VMOVE(hint.normal, normal);
    hint.error = error;
    hint.reasons = reasons;
    hints.push_back(hint);
}

template<typename Implicit>
static void
sample_hints(std::vector<struct bg_3d_spsr_refinement_hint> &hints,
    const std::vector<struct bg_3d_spsr_sample> &samples,
    const Implicit &implicit, fastf_t feature_size)
{
    const fastf_t normal_dot_limit =
        std::cos(SPSR_NORMAL_ANGLE_DEGREES * DEG2RAD);
    typename Implicit::Evaluator evaluator = implicit.evaluator();

    for (const auto &sample : samples) {
        Point<real_type, 3> point;
        for (size_t dimension = 0; dimension < 3; dimension++)
            point[dimension] = sample.point[dimension];

        try {
            Point<real_type, 3> gradient = evaluator.grad(point);
            real_type gradient_length = std::sqrt(
                gradient[0] * gradient[0] + gradient[1] * gradient[1] +
                gradient[2] * gradient[2]);
            unsigned int reasons = 0;
            fastf_t error = feature_size;
            vect_t normal;
            VSET(normal, gradient[0], gradient[1], gradient[2]);

            if (gradient_length <= VUNITIZE_TOL) {
                reasons = BG_3D_SPSR_REFINE_RESIDUAL |
                    BG_3D_SPSR_REFINE_NORMAL;
                VSETALL(normal, 0.0);
            } else {
                fastf_t residual = std::fabs(
                    evaluator(point) - implicit.isoValue) / gradient_length;
                fastf_t normal_dot = std::fabs(
                    VDOT(sample.normal, normal) / gradient_length);
                if (residual > feature_size) {
                    reasons |= BG_3D_SPSR_REFINE_RESIDUAL;
                    error = residual;
                }
                if (normal_dot < normal_dot_limit) {
                    reasons |= BG_3D_SPSR_REFINE_NORMAL;
                    error = std::max(error,
                        feature_size * (1.0 + normal_dot_limit - normal_dot));
                }
                VSCALE(normal, normal, 1.0 / gradient_length);
            }

            if (reasons)
                append_hint(hints, sample.point, normal, error, reasons);
        } catch (const std::exception &) {
            append_hint(hints, sample.point, sample.normal,
                2.0 * feature_size, BG_3D_SPSR_REFINE_RESIDUAL);
        }
    }
}

static void
density_hints(std::vector<struct bg_3d_spsr_refinement_hint> &hints,
    const std::vector<struct bg_3d_spsr_sample> &samples,
    fastf_t feature_size)
{
    RTree<size_t, double, 3> index;
    for (size_t i = 0; i < samples.size(); i++) {
        double point[3] = {
            samples[i].point[X], samples[i].point[Y], samples[i].point[Z]
        };
        index.Insert(point, point, i);
    }

    fastf_t search_distance_sq = feature_size * feature_size;
    for (size_t i = 0; i < samples.size(); i++) {
        double minimum[3];
        double maximum[3];
        for (size_t dimension = 0; dimension < 3; dimension++) {
            minimum[dimension] =
                samples[i].point[dimension] - feature_size;
            maximum[dimension] =
                samples[i].point[dimension] + feature_size;
        }

        size_t neighbor_count = 0;
        index.Search(minimum, maximum,
            [&samples, i, search_distance_sq, &neighbor_count](
                const size_t &candidate, void *) {
                if (DIST_PNT_PNT_SQ(samples[i].point,
                        samples[candidate].point) <= search_distance_sq)
                    neighbor_count++;
                return neighbor_count < SPSR_MIN_NEIGHBORS;
            }, NULL);

        if (neighbor_count < SPSR_MIN_NEIGHBORS)
            append_hint(hints, samples[i].point, samples[i].normal,
                feature_size, BG_3D_SPSR_REFINE_DENSITY);
    }
}

static void
surface_variation_hints(
    std::vector<struct bg_3d_spsr_refinement_hint> &hints,
    const spsr_mesh &mesh, fastf_t feature_size)
{
    size_t vertex_count = mesh.vertices.size() / 3;
    std::vector<std::array<fastf_t, 3>> first_normal(vertex_count);
    std::vector<fastf_t> variation(vertex_count, 0.0);
    std::vector<unsigned char> have_normal(vertex_count, 0);
    const fastf_t variation_limit =
        1.0 - std::cos(SPSR_NORMAL_ANGLE_DEGREES * DEG2RAD);

    for (size_t face = 0; face < mesh.faces.size() / 3; face++) {
        int ia = mesh.faces[3 * face];
        int ib = mesh.faces[3 * face + 1];
        int ic = mesh.faces[3 * face + 2];
        const fastf_t *a = &mesh.vertices[3 * static_cast<size_t>(ia)];
        const fastf_t *b = &mesh.vertices[3 * static_cast<size_t>(ib)];
        const fastf_t *c = &mesh.vertices[3 * static_cast<size_t>(ic)];
        vect_t ab;
        vect_t ac;
        vect_t normal;
        VSUB2(ab, b, a);
        VSUB2(ac, c, a);
        VCROSS(normal, ab, ac);
        fastf_t length = MAGNITUDE(normal);
        if (length <= VUNITIZE_TOL)
            continue;
        VSCALE(normal, normal, 1.0 / length);

        const int vertices[3] = {ia, ib, ic};
        for (size_t corner = 0; corner < 3; corner++) {
            size_t vertex = static_cast<size_t>(vertices[corner]);
            if (!have_normal[vertex]) {
                VMOVE(first_normal[vertex].data(), normal);
                have_normal[vertex] = 1;
            } else {
                variation[vertex] = std::max(variation[vertex],
                    1.0 - std::fabs(VDOT(first_normal[vertex].data(), normal)));
            }
        }
    }

    for (size_t vertex = 0; vertex < vertex_count; vertex++) {
        if (variation[vertex] <= variation_limit)
            continue;
        const fastf_t *point = &mesh.vertices[3 * vertex];
        append_hint(hints, point, first_normal[vertex].data(),
            feature_size * (1.0 + variation[vertex]),
            BG_3D_SPSR_REFINE_SURFACE_VARIATION);
    }
}

static void
limit_hints(std::vector<struct bg_3d_spsr_refinement_hint> &hints,
    fastf_t feature_size)
{
    std::sort(hints.begin(), hints.end(),
        [](const struct bg_3d_spsr_refinement_hint &left,
           const struct bg_3d_spsr_refinement_hint &right) {
            return left.error > right.error;
        });

    RTree<size_t, double, 3> locations;
    std::vector<struct bg_3d_spsr_refinement_hint> selected;
    selected.reserve(std::min(hints.size(), SPSR_MAX_HINTS));
    fastf_t separation = feature_size * SPSR_DEDUP_FRACTION;
    fastf_t separation_sq = separation * separation;

    for (const auto &hint : hints) {
        double minimum[3];
        double maximum[3];
        for (size_t dimension = 0; dimension < 3; dimension++) {
            minimum[dimension] = hint.point[dimension] - separation;
            maximum[dimension] = hint.point[dimension] + separation;
        }

        bool nearby = false;
        locations.Search(minimum, maximum,
            [&hint, &selected, separation_sq, &nearby](
                const size_t &candidate, void *) {
                if (DIST_PNT_PNT_SQ(hint.point,
                        selected[candidate].point) <= separation_sq) {
                    nearby = true;
                    return false;
                }
                return true;
            }, NULL);
        if (nearby)
            continue;

        size_t hint_index = selected.size();
        selected.push_back(hint);
        double point[3] = {hint.point[X], hint.point[Y], hint.point[Z]};
        locations.Insert(point, point, hint_index);
        if (selected.size() == SPSR_MAX_HINTS)
            break;
    }

    hints.swap(selected);
}

template<typename Implicit>
static bool
extract_mesh(spsr_mesh &mesh, const Implicit &implicit,
    const Reconstructor::LevelSetExtractionParameters &parameters)
{
    std::vector<std::vector<int>> polygons;
    polygon_stream<int> faces(polygons);
    vertex_stream<real_type, 3> vertices(mesh.vertices);
    implicit.extractLevelSet(vertices, faces, parameters);

    if (mesh.vertices.empty() || mesh.vertices.size() % 3 ||
        mesh.vertices.size() / 3 >
            static_cast<size_t>(std::numeric_limits<int>::max()) ||
        polygons.empty() ||
        polygons.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        return false;

    mesh.faces.reserve(polygons.size() * 3);
    for (const auto &polygon : polygons) {
        if (polygon.size() != 3)
            return false;
        for (int vertex : polygon) {
            if (vertex < 0 ||
                static_cast<size_t>(vertex) >= mesh.vertices.size() / 3)
                return false;
            mesh.faces.push_back(vertex);
        }
    }

    return true;
}

static bool
solve_once(spsr_mesh &mesh,
    std::vector<struct bg_3d_spsr_refinement_hint> &hints,
    const std::vector<struct bg_3d_spsr_sample> &samples,
    const struct bg_3d_spsr_adaptive_opts &options)
{
    static std::mutex solver_mutex;
    std::lock_guard<std::mutex> guard(solver_mutex);

    static const unsigned int fem_signature =
        FEMDegreeAndBType<Reconstructor::Poisson::DefaultFEMDegree,
            Reconstructor::Poisson::DefaultFEMBoundary>::Signature;
    using fem_signatures = IsotropicUIntPack<3, fem_signature>;
    using implicit_type =
        Reconstructor::Implicit<real_type, 3, fem_signatures>;
    using solver_type =
        Reconstructor::Poisson::Solver<real_type, 3, fem_signatures>;

    Reconstructor::Poisson::SolutionParameters<real_type> solver_parameters;
    Reconstructor::LevelSetExtractionParameters extraction_parameters;
    set_solver_options(solver_parameters, extraction_parameters,
        options.solver);

    ThreadPool::ParallelizationType = ThreadPool::ASYNC;
    point_stream<real_type, 3> points(samples);
    std::unique_ptr<implicit_type> implicit(
        solver_type::Solve(points, solver_parameters));
    if (!implicit || !extract_mesh(mesh, *implicit, extraction_parameters))
        return false;

    if (options.target_feature_size > 0.0) {
        sample_hints(hints, samples, *implicit,
            options.target_feature_size);
        density_hints(hints, samples, options.target_feature_size);
        surface_variation_hints(hints, mesh,
            options.target_feature_size);
        limit_hints(hints, options.target_feature_size);
    }

    return true;
}

static bool
copy_mesh(int **faces, int *face_count, point_t **vertices,
    int *vertex_count, const spsr_mesh &mesh)
{
    size_t faces_size = mesh.faces.size() * sizeof(int);
    size_t vertices_size = mesh.vertices.size() * sizeof(real_type);
    int *new_faces = static_cast<int *>(bu_malloc(faces_size,
        "SPSR face array"));
    point_t *new_vertices = static_cast<point_t *>(bu_malloc(vertices_size,
        "SPSR vertex array"));
    if (!new_faces || !new_vertices) {
        if (new_faces)
            bu_free(new_faces, "SPSR face array");
        if (new_vertices)
            bu_free(new_vertices, "SPSR vertex array");
        return false;
    }

    std::memcpy(new_faces, mesh.faces.data(), faces_size);
    std::memcpy(new_vertices, mesh.vertices.data(), vertices_size);
    *faces = new_faces;
    *face_count = static_cast<int>(mesh.faces.size() / 3);
    *vertices = new_vertices;
    *vertex_count = static_cast<int>(mesh.vertices.size() / 3);
    return true;
}

static void
initialize_outputs(int **faces, int *face_count, point_t **vertices,
    int *vertex_count, struct bg_3d_spsr_report *report)
{
    if (faces)
        *faces = NULL;
    if (face_count)
        *face_count = 0;
    if (vertices)
        *vertices = NULL;
    if (vertex_count)
        *vertex_count = 0;
    if (report)
        std::memset(report, 0, sizeof(*report));
}

} // namespace

extern "C" int
bg_3d_spsr_adaptive(int **faces, int *num_faces, point_t **vertices,
    int *num_vertices, const struct bg_3d_spsr_sample *input_samples,
    size_t sample_count, const struct bg_3d_spsr_adaptive_opts *input_options,
    bg_3d_spsr_refinement_func_t refine, void *client_data,
    struct bg_3d_spsr_report *report)
{
    initialize_outputs(faces, num_faces, vertices, num_vertices, report);
    if (!faces || !num_faces || !vertices || !num_vertices ||
        !input_samples || !sample_count)
        return BRLCAD_ERROR;

    struct bg_3d_spsr_adaptive_opts default_options =
        BG_3D_SPSR_ADAPTIVE_OPTS_DEFAULT;
    const struct bg_3d_spsr_adaptive_opts &options =
        input_options ? *input_options : default_options;
    if (!valid_options(options.solver) ||
        options.target_feature_size < 0.0 || options.max_time < 0.0 ||
        (options.max_refinement_passes &&
            (!refine || options.target_feature_size <= 0.0)) ||
        (options.max_points && options.max_points < sample_count))
        return BRLCAD_ERROR;

    fastf_t extent = 0.0;
    {
        std::vector<struct bg_3d_spsr_sample> initial(
            input_samples, input_samples + sample_count);
        extent = sample_extent(initial);
    }
    if (!std::isfinite(extent) || extent <= SMALL_FASTF)
        return BRLCAD_ERROR;

    fastf_t deduplication_distance =
        options.target_feature_size > 0.0 ?
        options.target_feature_size * SPSR_DEDUP_FRACTION :
        std::max(extent * 1.0e-12, static_cast<fastf_t>(SMALL_FASTF));
    sample_set accumulated(deduplication_distance, options.max_points);
    for (size_t i = 0; i < sample_count; i++)
        accumulated.append(input_samples[i]);
    if (accumulated.samples().empty())
        return BRLCAD_ERROR;

    const clock_type::time_point start = clock_type::now();
    if (report)
        report->initial_sample_count = accumulated.samples().size();

    bool point_limit_reached = accumulated.full();
    for (size_t pass = 0; ; pass++) {
        if (options.max_time > 0.0 &&
            elapsed_seconds(start) >= options.max_time) {
            if (report)
                report->termination = BG_3D_SPSR_TIME_LIMIT;
            break;
        }

        spsr_mesh mesh;
        std::vector<struct bg_3d_spsr_refinement_hint> hints;
        try {
            if (!solve_once(mesh, hints, accumulated.samples(), options)) {
                if (report)
                    report->termination = BG_3D_SPSR_SOLVER_ERROR;
                break;
            }
        } catch (const std::exception &) {
            if (report)
                report->termination = BG_3D_SPSR_SOLVER_ERROR;
            break;
        } catch (...) {
            if (report)
                report->termination = BG_3D_SPSR_SOLVER_ERROR;
            break;
        }

        if (report)
            report->solve_count++;

        if (!refine) {
            if (copy_mesh(faces, num_faces, vertices, num_vertices, mesh)) {
                if (report) {
                    report->termination = BG_3D_SPSR_COMPLETE;
                    report->validation.passed = 1;
                    report->final_sample_count = accumulated.samples().size();
                    report->elapsed_time = elapsed_seconds(start);
                }
                return BRLCAD_OK;
            }
            if (report)
                report->termination = BG_3D_SPSR_SOLVER_ERROR;
            break;
        }

        double remaining_time = options.max_time > 0.0 ?
            std::max(0.0, options.max_time - elapsed_seconds(start)) : 0.0;
        if (options.max_time > 0.0 && remaining_time <= 0.0) {
            if (report)
                report->termination = BG_3D_SPSR_TIME_LIMIT;
            break;
        }

        struct bg_3d_spsr_refinement_request request = {};
        request.pass = pass;
        request.target_feature_size = options.target_feature_size;
        request.remaining_time = remaining_time;
        request.vertices =
            reinterpret_cast<const point_t *>(mesh.vertices.data());
        request.vertex_count = mesh.vertices.size() / 3;
        request.faces = mesh.faces.data();
        request.face_count = mesh.faces.size() / 3;
        request.hints = hints.data();
        request.hint_count = hints.size();

        struct bg_3d_spsr_refinement_response response = {};
        if (refine(&response, &request, client_data) != BRLCAD_OK) {
            if (report)
                report->termination = BG_3D_SPSR_CALLBACK_STOP;
            break;
        }
        if (report)
            report->validation = response.validation;

        if (response.validation.passed) {
            if (copy_mesh(faces, num_faces, vertices, num_vertices, mesh)) {
                if (report) {
                    report->termination = BG_3D_SPSR_COMPLETE;
                    report->final_sample_count = accumulated.samples().size();
                    report->elapsed_time = elapsed_seconds(start);
                }
                return BRLCAD_OK;
            }
            if (report)
                report->termination = BG_3D_SPSR_SOLVER_ERROR;
            break;
        }
        if (response.stop_refinement) {
            if (report)
                report->termination = BG_3D_SPSR_CALLBACK_STOP;
            break;
        }

        if (pass == options.max_refinement_passes) {
            if (report)
                report->termination = point_limit_reached ?
                    BG_3D_SPSR_POINT_LIMIT : BG_3D_SPSR_CALLBACK_STOP;
            break;
        }
        if (response.sample_count && !response.samples) {
            if (report)
                report->termination = BG_3D_SPSR_CALLBACK_STOP;
            break;
        }

        if (report)
            report->requested_sample_count += response.sample_count;
        size_t accepted = 0;
        for (size_t i = 0; i < response.sample_count; i++) {
            if (accumulated.append(response.samples[i]))
                accepted++;
        }
        if (report)
            report->accepted_sample_count += accepted;
        point_limit_reached = accumulated.full();

        if (!accepted) {
            if (report)
                report->termination = point_limit_reached ?
                    BG_3D_SPSR_POINT_LIMIT : BG_3D_SPSR_NO_NEW_SAMPLES;
            break;
        }
    }

    if (report) {
        report->final_sample_count = accumulated.samples().size();
        report->elapsed_time = elapsed_seconds(start);
    }
    return BRLCAD_ERROR;
}

extern "C" int
bg_3d_spsr(int **faces, int *num_faces, point_t **vertices, int *num_vertices,
    const point_t *input_points_3d, const vect_t *input_normals_3d,
    int num_input_pnts, struct bg_3d_spsr_opts *spsr_opts)
{
    if (!input_points_3d || !input_normals_3d || num_input_pnts <= 0)
        return BRLCAD_ERROR;

    std::vector<struct bg_3d_spsr_sample> samples(
        static_cast<size_t>(num_input_pnts));
    for (size_t i = 0; i < samples.size(); i++) {
        VMOVE(samples[i].point, input_points_3d[i]);
        VMOVE(samples[i].normal, input_normals_3d[i]);
    }

    struct bg_3d_spsr_adaptive_opts options =
        BG_3D_SPSR_ADAPTIVE_OPTS_DEFAULT;
    options.max_refinement_passes = 0;
    if (spsr_opts)
        options.solver = *spsr_opts;

    return bg_3d_spsr_adaptive(faces, num_faces, vertices, num_vertices,
        samples.data(), samples.size(), &options, NULL, NULL, NULL);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
