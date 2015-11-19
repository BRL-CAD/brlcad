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

#ifndef SPR_INCLUDED
#define SPR_INCLUDED

#include "cvertex.h"

#ifndef SPR_EXPORT
#  if defined(SPR_DLL_EXPORTS) && defined(SPR_DLL_IMPORTS)
#    error "Only SPR_DLL_EXPORTS or SPR_DLL_IMPORTS can be defined, not both."
#  elif defined(SPR_DLL_EXPORTS)
#    define SPR_EXPORT __declspec(dllexport)
#  elif defined(SPR_DLL_IMPORTS)
#    define SPR_EXPORT __declspec(dllimport)
#  else
#    define SPR_EXPORT
#  endif
#endif

#ifndef __BEGIN_DECLS
#  ifdef __cplusplus
#    define __BEGIN_DECLS   extern "C" {
#    define __END_DECLS     }
#  else
#    define __BEGIN_DECLS
#    define __END_DECLS
#  endif
#endif

struct spr_options {
    const char *xform;
    const char *voxelgrid;
    const char *confidence;
    const char *normalweights;
    /* boolean */
    int density;
    int confidenceset;
    int normalweightsset;
    int nonManifold;
    int polygon;
    int colorset;
    int complete;
    int showresidual;
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
    double color;

    /* comments */
    void* comments;  // needs to be of type std::vector< char* >
};

// forward declarations
template< class Real > struct Point3D;
template< class Real > struct XForm4x4;
template< class Real > class OrientedPointStream;
template< class Real , class Data > class OrientedPointStreamWithData;
template< class Real > class PlyVertex;
template< class Vertex > class CoredFileMeshData;

SPR_EXPORT template< class Real, class Vertex > int
spr_surface_build_cpp(XForm4x4< Real>& xForm,
		      OrientedPointStreamWithData< float, Point3D< unsigned char > >* pointStream,
		      const struct spr_options &opts,
		      CoredFileMeshData< Vertex >* mesh);

SPR_EXPORT template< class Real, class Vertex > int
spr_surface_build_cpp_with_data(XForm4x4< Real>& xForm,
				OrientedPointStream< float >* pointStream,
				const struct spr_options &opts,
				CoredFileMeshData< PlyVertex <float> >* mesh);

__BEGIN_DECLS

SPR_EXPORT extern int
spr_surface_build_opts(int **faces, int *num_faces, double **points, int *num_pnts,
		const struct cvertex **verts, int cnt, struct spr_options &opts);

SPR_EXPORT extern int
spr_surface_build(int **faces, int *num_faces, double **points, int *num_pnts,
		const struct cvertex **verts, int cnt, int fidelity);

__END_DECLS

#endif /* SPR_INCLUDE */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
