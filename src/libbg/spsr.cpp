/*                         S P S R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2024 United States Government as represented by
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
 * Interface to Screened Poisson Surface Reconstruction
 *
 * TODO - it may be worth porting the Open3D ball pivot code to vmath.h
 * types and using straight nanoflann to provide another alternative to
 * the SPSR methodology:
 *
 * https://github.com/isl-org/Open3D/blob/master/cpp/open3d/geometry/SurfaceReconstructionBallPivoting.cpp
 *
 * Not sure if that would be easier than reviving the src/other/gct code or not
 * at this point, but the latter hasn't been built in a long time.
 *
 * Ball Pivot lacks the solidity properties of SPSR, but it may be a useful,
 * fast alternative in some situations now that the patent is expired and we
 * can look at using it.  In particular, I'm wondering if it might be used to
 * construct plate mode meshes for thin surface shapes that SPSR doesn't do
 * well with...
 *
 */
#include "common.h"
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/spsr.h"
#include "SPSR/Reconstructors.h"

template< typename Real , unsigned int Dim >
struct PtStream : public Reconstructor::InputSampleStream<Real, Dim>
{
    // Constructs a stream that contains the specified number of samples
    PtStream( const point_t *ipnts, const vect_t *inorms, int cnt) : _size(cnt) , _current(0), _input_pnts(ipnts), _input_nrmls(inorms) {}

    // Overrides the pure abstract method from InputSampleStream< Real , Dim >
    void reset( void ){ _current = 0; }

    // Overrides the pure abstract method from InputSampleStream< Real , Dim >
    bool base_read( Point< Real, Dim > &p , Point< Real, Dim > &n )
    {
	if( _current < _size )
	{
	    for( size_t d = 0 ; d < Dim; d++ )
	       	p[d] = _input_pnts[_current][d];
	    for( size_t d = 0 ; d < Dim; d++ )
	       	n[d] = _input_nrmls[_current][d];
	    _current++;
	    return true;
	}
	else return false;
    }

    protected:
    unsigned int _size , _current;
    const point_t *_input_pnts;
    const vect_t *_input_nrmls;
};

// A stream into which we can write polygons of the form std::vector< node_index_type >
template< typename Index >
struct PolygonStream : public Reconstructor::OutputFaceStream< 2 >
{
    // Construct a stream that adds polygons to the vector of polygons
    PolygonStream( std::vector< std::vector< Index > > &polygonStream ) : _polygons( polygonStream ) {}

    // Override the pure abstract method from OutputPolygonStream
    void base_write( const std::vector< node_index_type > &polygon )
    {
	std::vector< Index > poly( polygon.size() );
	for( unsigned int i=0 ; i<polygon.size() ; i++ ) poly[i] = (Index)polygon[i];
	_polygons.push_back( poly );
    }
    protected:
    std::vector< std::vector< Index > > &_polygons;
};

// A stream into which we can write the output vertices of the extracted mesh
template< typename Real , unsigned int Dim >
struct VertexStream : public Reconstructor::OutputVertexStream< Real , Dim >
{
    // Construct a stream that adds vertices into the coordinates
    VertexStream( std::vector< Real > &vCoordinates ) : _vCoordinates( vCoordinates ) {}

    // Override the pure abstract method from Reconstructor::OutputVertexStream< Real , Dim >
    void base_write( Point< Real , Dim > p , Point< Real , Dim > , Real ){ for( unsigned int d=0 ; d<Dim ; d++ ) _vCoordinates.push_back( p[d] ); }
    protected:
    std::vector< Real > &_vCoordinates;
};


extern "C" int
bg_3d_spsr(int **faces, int *num_faces, point_t **points, int *num_pnts,
	   const point_t *input_points_3d, const vect_t *input_normals_3d,
	   int num_input_pnts, struct bg_3d_spsr_opts *spsr_opts)
{
    static const unsigned int FEMSig = FEMDegreeAndBType< Reconstructor::Poisson::DefaultFEMDegree , Reconstructor::Poisson::DefaultFEMBoundary >::Signature;

    ThreadPool::Init(ThreadPool::THREAD_POOL , std::thread::hardware_concurrency());

    Reconstructor::Poisson::SolutionParameters< fastf_t > solverParams;
    solverParams.verbose = false;
    solverParams.depth = (spsr_opts) ? spsr_opts->depth : 8;
    solverParams.fullDepth = (spsr_opts) ? spsr_opts->full_depth : 11;
    solverParams.samplesPerNode = (spsr_opts) ? spsr_opts->samples_per_node : 1.5;
    solverParams.exactInterpolation = true;

    Reconstructor::LevelSetExtractionParameters extractionParams;
    extractionParams.forceManifold = true;
    extractionParams.linearFit = false; // Should be true if using SSD
    extractionParams.polygonMesh = false;
    extractionParams.verbose = false;

    PtStream<fastf_t, 3> vstream(input_points_3d, input_normals_3d, num_input_pnts);

    Reconstructor::Poisson::Implicit<fastf_t, 3, FEMSig> implicit(vstream, solverParams);

    // vectors for storing the polygons (specifically, triangles) and the coordinates of the vertices
    std::vector< std::vector< int > > polygons;
    std::vector< fastf_t > vCoordinates;

    // Streams backed by these vectors
    PolygonStream< int > pStream( polygons );
    VertexStream< fastf_t , 3 > vStream( vCoordinates );

    // Extract the iso-surface
    implicit.extractLevelSet( vStream , pStream , extractionParams );

    // Translate to libbg output types
    *num_faces = (int)(polygons.size());
    *num_pnts = (int)(vCoordinates.size()/3);
    bu_log("Point cnt: %d\n", *num_pnts);
    bu_log("Face cnt: %d\n", *num_faces);
    (*faces) = (int *)bu_calloc(*num_faces*3, sizeof(int), "faces array");
    (*points) = (point_t *)bu_calloc(*num_pnts, sizeof(point_t), "points array");
    for (int i = 0; i < *num_faces; i++) {
	(*faces)[3*i*0] = polygons[i][0];
	(*faces)[3*i*1] = polygons[i][1];
	(*faces)[3*i*2] = polygons[i][2];
    }
    for (int i = 0; i < *num_pnts; i++) {
	(*points)[i][X] = vCoordinates[3*i+0];
	(*points)[i][Y] = vCoordinates[3*i+1];
	(*points)[i][Z] = vCoordinates[3*i+2];
    }

    // All done - clean up the thread pool
    ThreadPool::Terminate();

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
