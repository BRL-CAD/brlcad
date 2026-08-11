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
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file spsr.cpp
 *
 * Interface to Adaptive Multigrid Solvers for Screened Poisson Surface Reconstruction
 * Compatible with mkazhdan/PoissonRecon API (v18.x+).
 *
 */

#include "common.h"
#include <climits>
#include <cmath>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/spsr.h"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push /* start new diagnostic pragma */
#  pragma GCC diagnostic ignored "-Wall"
#elif defined(__clang__)
#  pragma clang diagnostic push /* start new diagnostic pragma */
#  pragma clang diagnostic ignored "-Weverything"
#endif
#include "SPSR/Reconstructors.h"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop /* end ignoring warnings */
#elif defined(__clang__)
#  pragma clang diagnostic pop /* end ignoring warnings */
#endif

using namespace PoissonRecon;

// Input stream for oriented samples: implements `read` for coordinates and normals
template<typename Real, unsigned int Dim>
struct PtStream : public Reconstructor::InputOrientedSampleStream<Real, Dim>
{
    PtStream(const point_t *ipnts, const vect_t *inorms, int cnt)
	: _size(cnt), _current(0), _input_pnts(ipnts), _input_nrmls(inorms) {}
    void reset(void) { _current = 0; }
    bool read(Point<Real, Dim> &p, Point<Real, Dim> &n)
    {
	if (_current < _size) {
	    for (size_t d = 0; d < Dim; d++)
		p[d] = _input_pnts[_current][d];
	    for (size_t d = 0; d < Dim; d++)
		n[d] = _input_nrmls[_current][d];
	    _current++;
	    return true;
	}
	return false;
    }
    protected:
    unsigned int _size, _current;
    const point_t *_input_pnts;
    const vect_t *_input_nrmls;
};

// Output face stream: must implement write() and size()
template <typename Index>
struct PolygonStream : public Reconstructor::OutputFaceStream<2>
{
    PolygonStream(std::vector<std::vector<Index>> &polygonStream) : _polygons(polygonStream) {}
    size_t size(void) const { return _polygons.size(); }
    size_t write(const std::vector<node_index_type> &polygon)
    {
	std::vector<Index> poly(polygon.size());
	for (size_t i = 0; i < polygon.size(); i++) poly[i] = (Index)polygon[i];
	_polygons.push_back(poly);
	return _polygons.size() - 1;
    }
    protected:
    std::vector<std::vector<Index>> &_polygons;
};

// Output vertex stream: must implement write() and size()
template <typename Real, unsigned int Dim>
struct VertexStream : public Reconstructor::OutputLevelSetVertexStream<Real, Dim>
{
    VertexStream(std::vector<Real> &vCoordinates) : _vCoordinates(vCoordinates) {}
    size_t size(void) const { return _vCoordinates.size() / Dim; }
    size_t write(const Point<Real, Dim> &p, const Point<Real, Dim> &, const Real &)
    {
	for (unsigned int d = 0; d < Dim; d++) _vCoordinates.push_back(p[d]);
	return _vCoordinates.size() / Dim - 1;
    }
    protected:
    std::vector<Real> &_vCoordinates;
};

extern "C" int
bg_3d_spsr(int **faces, int *num_faces, point_t **points, int *num_pnts,
	const point_t *input_points_3d, const vect_t *input_normals_3d,
	int num_input_pnts, struct bg_3d_spsr_opts *spsr_opts)
{
    if (!faces || !num_faces || !points || !num_pnts)
	return -1;
    *faces = NULL;
    *num_faces = 0;
    *points = NULL;
    *num_pnts = 0;
    if (!input_points_3d || !input_normals_3d || num_input_pnts < 1)
	return -1;
    if (spsr_opts && (spsr_opts->depth < 1 || spsr_opts->depth > 20 ||
	    spsr_opts->full_depth < 0 ||
	    spsr_opts->full_depth > spsr_opts->depth ||
	    !(spsr_opts->samples_per_node > 0.0) ||
	    !std::isfinite(spsr_opts->samples_per_node) ||
	    !(spsr_opts->scale > 0.0) ||
	    !std::isfinite(spsr_opts->scale) ||
	    !(spsr_opts->point_weight >= 0.0) ||
	    !std::isfinite(spsr_opts->point_weight)))
	return -1;
    for (int i = 0; i < num_input_pnts; ++i) {
	double normal_squared = 0.0;
	for (int axis = 0; axis < 3; ++axis) {
	    if (!std::isfinite(input_points_3d[i][axis]) ||
		    !std::isfinite(input_normals_3d[i][axis]))
		return -1;
	    normal_squared += input_normals_3d[i][axis] *
		input_normals_3d[i][axis];
	}
	if (!(normal_squared > 0.0) || !std::isfinite(normal_squared))
	    return -1;
    }

    using Real = fastf_t;
    static const unsigned int FEMSig = FEMDegreeAndBType<Reconstructor::Poisson::DefaultFEMDegree, Reconstructor::Poisson::DefaultFEMBoundary>::Signature;
    using FEMSigs = IsotropicUIntPack<3, FEMSig>;

    /* The upstream worker configuration and async future storage are global.
     * Serialize reconstructions so per-call thread modes cannot race. */
    static std::mutex reconstruction_mutex;
    std::lock_guard<std::mutex> reconstruction_lock(reconstruction_mutex);

    // A one-thread request can be honored without changing the upstream
    // thread pool's private worker count.  Other requests use its configured
    // hardware concurrency.
    const PoissonRecon::ThreadPool::ParallelType previous_parallelization =
	PoissonRecon::ThreadPool::ParallelizationType;
    struct thread_mode_guard {
	PoissonRecon::ThreadPool::ParallelType previous;
	~thread_mode_guard()
	{
	    PoissonRecon::ThreadPool::ParallelizationType = previous;
	}
    } restore_thread_mode = {previous_parallelization};
    PoissonRecon::ThreadPool::ParallelizationType =
	(spsr_opts && spsr_opts->threads == 1) ?
	PoissonRecon::ThreadPool::NONE : PoissonRecon::ThreadPool::ASYNC;

    // Solver and extraction parameters
    Reconstructor::Poisson::SolutionParameters<Real> solverParams;
    solverParams.verbose = false;
    solverParams.depth = (spsr_opts) ? spsr_opts->depth : 8;
    solverParams.fullDepth = (spsr_opts) ? spsr_opts->full_depth : 11;
    solverParams.samplesPerNode =
	(spsr_opts) ? spsr_opts->samples_per_node : 1.5;
    solverParams.scale = (spsr_opts) ? spsr_opts->scale : 1.1;
    solverParams.pointWeight = (spsr_opts) ? spsr_opts->point_weight : 8.0;
    solverParams.exactInterpolation =
	spsr_opts ? spsr_opts->exact != 0 : true;

    Reconstructor::LevelSetExtractionParameters extractionParams;
    extractionParams.forceManifold =
	spsr_opts ? spsr_opts->nonManifold == 0 : true;
    extractionParams.linearFit = spsr_opts ? spsr_opts->linearFit != 0 : false;
    extractionParams.polygonMesh = false;
    extractionParams.verbose = false;

    PtStream<Real, 3> vstream(input_points_3d, input_normals_3d, num_input_pnts);

    using Implicit = Reconstructor::Implicit<Real, 3, FEMSigs>;
    using Solver = Reconstructor::Poisson::Solver<Real, 3, FEMSigs>;
    std::vector<std::vector<int>> polygons;
    std::vector<Real> vCoordinates;
    PolygonStream<int> pStream(polygons);
    VertexStream<Real, 3> vStream(vCoordinates);
    std::unique_ptr<Implicit> implicit;
    try {
	implicit.reset(Solver::Solve(vstream, solverParams));
	if (!implicit) {
	    bu_log("PoissonRecon: Solver::Solve failed\n");
	    return -1;
	}
	implicit->extractLevelSet(vStream, pStream, extractionParams);
    } catch (const std::exception &error) {
	bu_log("PoissonRecon failed: %s\n", error.what());
	return -1;
    } catch (...) {
	bu_log("PoissonRecon failed with an unknown exception\n");
	return -1;
    }

    if (vCoordinates.size() % 3 || polygons.empty() ||
	    vCoordinates.size() < 9 || polygons.size() > INT_MAX ||
	    vCoordinates.size() / 3 > INT_MAX)
	return -1;
    const size_t point_count = vCoordinates.size() / 3;
    for (size_t i = 0; i < point_count * 3; ++i) {
	if (!std::isfinite(vCoordinates[i]))
	    return -1;
    }
    for (const std::vector<int> &polygon : polygons) {
	if (polygon.size() != 3)
	    return -1;
	for (int vertex : polygon) {
	    if (vertex < 0 || (size_t)vertex >= point_count)
		return -1;
	}
    }

    *num_faces = (int)polygons.size();
    *num_pnts = (int)point_count;

    // Allocate output arrays
    (*faces) = (int *)bu_calloc((size_t)*num_faces * 3, sizeof(int),
	"faces array");
    (*points) = (point_t *)bu_calloc((size_t)*num_pnts, sizeof(point_t),
	"points array");

    // Copy the validated triangulated output.
    for (int i = 0; i < *num_faces; i++) {
	(*faces)[3*i+0] = polygons[i][0];
	(*faces)[3*i+1] = polygons[i][1];
	(*faces)[3*i+2] = polygons[i][2];
    }
    // Copy points
    for (int i = 0; i < *num_pnts; i++) {
	(*points)[i][X] = vCoordinates[3*i+0];
	(*points)[i][Y] = vCoordinates[3*i+1];
	(*points)[i][Z] = vCoordinates[3*i+2];
    }
    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
