/*
Copyright (c) 2006, Michael Kazhdan and Matthew Bolitho
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of
conditions and the following disclaimer. Redistributions in binary form must reproduce
the above copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the distribution.

Neither the name of the Johns Hopkins University nor the names of its contributors
may be used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "SPR.h"
#include <iostream>
#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#endif // _WIN32
#include "MarchingCubes.h"
#include "Octree.h"
#include "SparseMatrix.h"
#include "PPolynomial.h"
#include "Ply.h"
#include "MemoryUsage.h"
#ifdef _OPENMP
#include "omp.h"
#endif // _OPENMP
void DumpOutput( const char* format , ... );
#include "MultiGridOctreeData.h"
void DumpOutput2( std::vector< char* >& comments , const char* format , ... );

//DEFAULT - LOW
#define SPR_OPTIONS_LOW_INIT { NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 6, 1, 8, -1, 5, 0, 8, 1, 1, 1.0, 2.0, 0.001, 15.0, NULL }
//GOOD - MEDIUM
#define SPR_OPTIONS_MED_INIT { NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 6, 1, 8, -1, 5, 0, 8, 1, 1, 0.75, 2.0, 0.001, 17.0, NULL }
//BEST (lots of fidelity) - HIGH
#define SPR_OPTIONS_HIGH_INIT { NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 6, 1, 8, -1, 5, 0, 8, 1, 1, 0.25, 2.0, 0.001, 20.0, NULL }

void DumpOutput( const char* format , ... ) {};
void DumpOutput2( std::vector< char* >& comments , const char* format , ... ) {};

template< class Real, class Vertex > int
spr_surface_build_cpp_with_data(XForm4x4< Real>& xForm,
				OrientedPointStreamWithData< float, Point3D< unsigned char > >* pointStream,
				const struct spr_options &opts,
				CoredFileMeshData< Vertex >* mesh)
{
    double maxMemoryUsage;
    double t;
    double tt=Time();
    std::vector< char *>* comments = (std::vector< char *>*) opts.comments;
    
    // Probably unnecessary but here to be consistent with original code
    Reset< Real >();
    Octree< Real > tree;
    typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >* pointInfo = new typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >();    
    typename Octree< Real >::template SparseNodeData< Point3D< Real > >* normalInfo = new typename Octree< Real >::template SparseNodeData< Point3D< Real > >();
    std::vector< Real >* kernelDensityWeights = new std::vector< Real >();
    std::vector< Real >* centerWeights = new std::vector< Real >();

    t=Time() , tree.maxMemoryUsage=0;
    tree.threads = opts.thread_cnt;

    OctNode< TreeNodeData >::SetAllocator( MEMORY_ALLOCATOR_BLOCK_SIZE );
    
    typedef typename Octree< Real >::template ProjectiveData< Point3D< Real > > ProjectiveColor;
    typename Octree< Real >::template SparseNodeData< ProjectiveColor > colorData;
    int pointCount = tree.template SetTree< float >(pointStream , opts.mindepth, opts.depth, opts.fulldepth, opts.kerneldepth,
						    opts.samples_per_node, opts.scale, opts.confidenceset, opts.normalweightsset,
						    opts.pointweight, opts.adaptiveexponent, *kernelDensityWeights, *pointInfo,
						    *normalInfo , *centerWeights, colorData, xForm, opts.boundarytype, opts.complete);
    for( const OctNode< TreeNodeData >* n = tree.tree.nextNode() ; n!=NULL ; n=tree.tree.nextNode( n ) )
    {
	int idx = colorData.index( n );
	if( idx>=0 ) colorData.data[idx] *= (Real)pow( opts.color , n->depth() );
    }
    
    DumpOutput2( *comments , "#             Tree set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Input Points: %d\n" , pointCount );
    DumpOutput( "Leaves/Nodes: %d/%d\n" , tree.tree.leaves() , tree.tree.nodes() );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    
    maxMemoryUsage = tree.maxMemoryUsage;
    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) constraints = tree.SetLaplacianConstraints( *normalInfo );
    delete normalInfo;
    DumpOutput2( *comments , "#      Constraints set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage())/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );

    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) solution = tree.SolveSystem(*pointInfo, constraints, opts.showresidual, opts.iters, opts.maxsolvedepth,
						opts.cgdepth, float(opts.cssolveraccuracy));
    delete pointInfo;
    FreePointer(constraints);
    
    DumpOutput2( *comments , "# Linear system solved in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    
    t=Time();
    Real isoValue = tree.GetIsoValue( solution , *centerWeights );
    centerWeights->clear();
    delete centerWeights;
    centerWeights = NULL;
    
    DumpOutput( "Got average in: %f\n" , Time()-t );
    DumpOutput( "Iso-Value: %e\n" , isoValue );

    /* VoxelGrid.set ??? */
    
    t = Time() , tree.maxMemoryUsage = 0;
    tree.GetMCIsoSurface( opts.density ? GetPointer(*kernelDensityWeights) : NullPointer( Real ) ,
			  opts.colorset ? & colorData : NULL, solution , isoValue , *mesh , true ,
			  !opts.nonManifold , opts.polygon );

    if( opts.polygon )
	DumpOutput2( *comments , "#         Got polygons in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    else
	DumpOutput2( *comments , "#        Got triangles in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    DumpOutput2( *comments , "#             Total Solve: %9.1f (s), %9.1f (MB)\n" , Time()-tt , maxMemoryUsage );
    
    kernelDensityWeights->clear();
    delete kernelDensityWeights;
    kernelDensityWeights = NULL;

    // Cleanup
    FreePointer( solution );
    Reset< Real >();

    return 0;
}

template< class Real, class Vertex > int
spr_surface_build_cpp(XForm4x4< Real>& xForm,
		      OrientedPointStream< float >* pointStream,
		      const struct spr_options &opts,
		      CoredFileMeshData< Vertex >* mesh)
{
    double maxMemoryUsage;
    double t;
    double tt=Time();
    std::vector< char *>* comments = (std::vector< char *>*) opts.comments;
    
    // Probably unnecessary but here to be consistent with original code
    Reset< Real >();
    Octree< Real > tree;
    typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >* pointInfo = new typename Octree< Real >::template SparseNodeData< typename Octree< Real >::PointData >();    
    typename Octree< Real >::template SparseNodeData< Point3D< Real > >* normalInfo = new typename Octree< Real >::template SparseNodeData< Point3D< Real > >();
    std::vector< Real >* kernelDensityWeights = new std::vector< Real >();
    std::vector< Real >* centerWeights = new std::vector< Real >();

    t=Time() , tree.maxMemoryUsage=0;
    tree.threads = opts.thread_cnt;

    OctNode< TreeNodeData >::SetAllocator( MEMORY_ALLOCATOR_BLOCK_SIZE );
    
    typedef typename Octree< Real >::template ProjectiveData< Point3D< Real > > ProjectiveColor;
    typename Octree< Real >::template SparseNodeData< ProjectiveColor > colorData;    
    int pointCount = tree.template SetTree< float >(pointStream , opts.mindepth, opts.depth, opts.fulldepth, opts.kerneldepth,
						    opts.samples_per_node, opts.scale, opts.confidenceset, opts.normalweightsset,
						    opts.pointweight, opts.adaptiveexponent, *kernelDensityWeights, *pointInfo,
						    *normalInfo , *centerWeights, xForm, opts.boundarytype, opts.complete);
    
    DumpOutput2( *comments , "#             Tree set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Input Points: %d\n" , pointCount );
    DumpOutput( "Leaves/Nodes: %d/%d\n" , tree.tree.leaves() , tree.tree.nodes() );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    
    maxMemoryUsage = tree.maxMemoryUsage;
    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) constraints = tree.SetLaplacianConstraints( *normalInfo );
    delete normalInfo;
    DumpOutput2( *comments , "#      Constraints set in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage())/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );

    t=Time() , tree.maxMemoryUsage=0;
    Pointer( Real ) solution = tree.SolveSystem(*pointInfo, constraints, opts.showresidual, opts.iters, opts.maxsolvedepth,
						opts.cgdepth, float(opts.cssolveraccuracy));
    delete pointInfo;
    FreePointer(constraints);
    
    DumpOutput2( *comments , "# Linear system solved in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    DumpOutput( "Memory Usage: %.3f MB\n" , float( MemoryInfo::Usage() )/(1<<20) );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    
    t=Time();
    Real isoValue = tree.GetIsoValue( solution , *centerWeights );
    centerWeights->clear();
    delete centerWeights;
    centerWeights = NULL;
    DumpOutput( "Got average in: %f\n" , Time()-t );
    DumpOutput( "Iso-Value: %e\n" , isoValue );

    /* VoxelGrid.set ??? */
    
    t = Time() , tree.maxMemoryUsage = 0;
    tree.GetMCIsoSurface( opts.density ? GetPointer(*kernelDensityWeights) : NullPointer( Real ) ,
			  opts.colorset ? & colorData : NULL, solution , isoValue , *mesh , true ,
			  !opts.nonManifold , opts.polygon );

    if( opts.polygon )
	DumpOutput2( *comments , "#         Got polygons in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    else
	DumpOutput2( *comments , "#        Got triangles in: %9.1f (s), %9.1f (MB)\n" , Time()-t , tree.maxMemoryUsage );
    maxMemoryUsage = std::max< double >( maxMemoryUsage , tree.maxMemoryUsage );
    DumpOutput2( *comments , "#             Total Solve: %9.1f (s), %9.1f (MB)\n" , Time()-tt , maxMemoryUsage );
    
    kernelDensityWeights->clear();
    delete kernelDensityWeights;
    kernelDensityWeights = NULL;

    // Cleanup
    FreePointer( solution );
    Reset< Real >();

    return 0;
}

extern "C" int
spr_surface_build_opts(int **faces, int *num_faces, double **points, int *num_pnts,
		const struct cvertex **verts, int cnt, struct spr_options &opts)
{
    if (!num_pnts || !num_faces || !points || !faces) return -1;

    std::vector< char* > comments;
    XForm4x4< double > xForm = XForm4x4< double >::Identity();
    OrientedPointStream< float >* pointStream = new CVertexPointStream< float >( cnt, verts );
    CoredFileMeshData< PlyVertex <float> > mesh;

    opts.comments = (void *) &comments;
    int result = spr_surface_build_cpp< double, PlyVertex< float > >(xForm, pointStream, opts, &mesh);
    delete pointStream;

    /* mesh to triangles */
    (*num_pnts) = int(mesh.outOfCorePointCount()+mesh.inCorePoints.size());
    (*num_faces) = mesh.polygonCount();
    (*points) = (double *)calloc(((*num_pnts) + 1)*3, sizeof(double));
    (*faces) = (int *)calloc(3*3*(*num_faces) + 1, sizeof(int));
    std::cout << "Point cnt: " << *num_pnts << "\n";
    std::cout << "Face cnt: " << *num_faces << "\n";
    int pnt_ind = 0;
    mesh.resetIterator();
    for (int i = 0 ; i < int(mesh.inCorePoints.size()); i++) {
        PlyVertex<float> vertex = mesh.inCorePoints[i];
	(*points)[pnt_ind*3] = vertex.point[0];
	(*points)[pnt_ind*3 + 1] = vertex.point[1];
	(*points)[pnt_ind*3 + 2] = vertex.point[2];
        pnt_ind++;
    }
    for (int i = 0 ; i < mesh.outOfCorePointCount(); i++) {
        PlyVertex<float> vertex;
        mesh.nextOutOfCorePoint(vertex);
	(*points)[pnt_ind*3] = vertex.point[0];
	(*points)[pnt_ind*3 + 1] = vertex.point[1];
	(*points)[pnt_ind*3 + 2] = vertex.point[2];
        pnt_ind++;
    }
    
    std::vector< CoredVertexIndex > polygon;
    for (int i = 0; i < (*num_faces); i++) {
        mesh.nextPolygon(polygon);
        (*faces)[i*3] = (polygon[0].inCore) ? polygon[0].idx : polygon[0].idx + int(mesh.inCorePoints.size());
        (*faces)[i*3+1] = (polygon[1].inCore) ? polygon[1].idx : polygon[1].idx + int(mesh.inCorePoints.size());
        (*faces)[i*3+2] = (polygon[2].inCore) ? polygon[2].idx : polygon[2].idx + int(mesh.inCorePoints.size());
    }

    return result;
}

extern "C" int
spr_surface_build(int **faces, int *num_faces, double **points, int *num_pnts,
		const struct cvertex **verts, int cnt, int fidelity)
{
    //struct spr_options opts = SPR_OPTIONS_DEFAULT_INIT;
    struct spr_options opts = SPR_OPTIONS_LOW_INIT;
    if (fidelity == 1) {
	struct spr_options tempopt = SPR_OPTIONS_MED_INIT;
	memcpy(&opts, &tempopt, sizeof(struct spr_options));
    } else if (fidelity == 2) {
	struct spr_options tempopt = SPR_OPTIONS_HIGH_INIT;
	memcpy(&opts, &tempopt, sizeof(struct spr_options));
    }

    return spr_surface_build_opts(faces, num_faces, points, num_pnts, verts, cnt, opts);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

