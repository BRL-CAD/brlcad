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
void DumpOutput( const char* format , ... ) {};
void DumpOutput2( char* str , const char* format , ... ) {};
#include "MultiGridOctreeData.h"

struct spr_options {
	const char *xform;
	const char *voxelgrid;
	const char *confidence;
	const char *normalweights;
	/* boolean */
	int nonManifold;
	int polygon;
	/* int */
	int depth;
	int cgdepth;
	int kerneldepth;
	int adaptiveexponent;
	int iters;
	int voxeldepth;
	int fulldepth;
	int mindepth;
	int maxsolvedepth;
	int boundarytype;
	int thread_cnt;

	double samples_per_node;
	double scale;
	double cssolveraccuracy;
	double pointweight;
};

//DEFAULT - LOW
#define SPR_OPTIONS_LOW_INIT { NULL, NULL, NULL, NULL, 0, 0, 8, 0, 6, 1, 8, -1, 5, 0, 8, 1, 1, 1.0, 1.1, 0.001, 10.0 }
//GOOD - MEDIUM
#define SPR_OPTIONS_MED_INIT { NULL, NULL, NULL, NULL, 0, 0, 8, 0, 6, 1, 8, -1, 5, 0, 8, 1, 1, 1.0, 2.0, 0.001, 15.0 }
//BEST (lots of fidelity) - HIGH
#define SPR_OPTIONS_HIGH_INIT { NULL, NULL, NULL, NULL, 0, 0, 8, 0, 6, 1, 8, -1, 5, 0, 8, 1, 1, 0.25, 2.0, 0.001, 20.0 }


extern "C" int
spr_surface_build(int **faces, int *num_faces, double **points, int *num_pnts,
		const struct cvertex **verts, int cnt, int fidelity)
{
    if (!num_pnts || !num_faces || !points || !faces) return -1;

    //struct spr_options opts = SPR_OPTIONS_DEFAULT_INIT;
    struct spr_options opts = SPR_OPTIONS_LOW_INIT;
    if (fidelity == 1) {
	struct spr_options tempopt = SPR_OPTIONS_MED_INIT;
	memcpy(&opts, &tempopt, sizeof(struct spr_options));
    } else if (fidelity == 2) {
	struct spr_options tempopt = SPR_OPTIONS_HIGH_INIT;
	memcpy(&opts, &tempopt, sizeof(struct spr_options));
    }
    
    // Probably unnecessary but here to be consistent with original code
    Reset< double >();
    XForm4x4< double > xForm;
    xForm = XForm4x4< double >::Identity();
    Octree< double > tree;
    tree.threads = opts.thread_cnt;
    OctNode< TreeNodeData >::SetAllocator( MEMORY_ALLOCATOR_BLOCK_SIZE );
    double maxMemoryUsage;
    Octree< double >::PointInfo* pointInfo = new Octree< double >::PointInfo();
    Octree< double >::NormalInfo* normalInfo = new Octree< double >::NormalInfo();
    std::vector< double >* kernelDensityWeights = new std::vector< double >();
    std::vector< double >* centerWeights = new std::vector< double >();
    PointStream< float >* pointStream = new CVertexPointStream< float >( cnt, verts );
    int pointCount = tree.SetTree< float >(pointStream , opts.mindepth, opts.depth, opts.fulldepth, opts.kerneldepth,
	    opts.samples_per_node, opts.scale, 0, 0, opts.pointweight, opts.adaptiveexponent, *pointInfo,
	    *normalInfo , *kernelDensityWeights , *centerWeights , opts.boundarytype, xForm , 0);
    kernelDensityWeights->clear();
    delete kernelDensityWeights;
    kernelDensityWeights = NULL;
    Pointer( double ) constraints = tree.SetLaplacianConstraints( *normalInfo );
    delete normalInfo;
    Pointer( double ) solution = tree.SolveSystem( *pointInfo , constraints , 0 , opts.iters , opts.maxsolvedepth , opts.cgdepth , float(opts.cssolveraccuracy) );
    delete pointInfo;
    FreePointer(constraints);
    CoredFileMeshData< PlyVertex <float> > mesh;
    double isoValue = tree.GetIsoValue( solution , *centerWeights );
    centerWeights->clear();
    delete centerWeights;
    centerWeights = NULL;
    delete pointStream;
    tree.GetMCIsoSurface( NullPointer< double >() , solution , isoValue , mesh , true , 1 , 0 );
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
    // Cleanup
    Reset< double>();
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

