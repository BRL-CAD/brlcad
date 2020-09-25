/*                        S P S R . H
 *
 * Copyright (c) 2015 mkazhdan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Copyright (c) 2006, Michael Kazhdan and Matthew Bolitho
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * Neither the name of the Johns Hopkins University nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE
 * IMPLIED WARRANTIES  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*----------------------------------------------------------------------*/
/* @file spsr.h */
/** @addtogroup bg_surf_recon_spsr */
/** @{ */

/**
 *  @brief Screened Poisson Surface Reconstruction from oriented point sets
 *
 *  This functionality is a refactoring of work published by Dr. Michael
 *  Kazhdan et. al. at https://github.com/mkazhdan/PoissonRecon implementing a
 *  technique for fitting surfaces to oriented point sets by expressing the
 *  surface reconstruction as the solution to a Poisson equation.
 */

#ifndef BG_SPSR_H
#define BG_SPSR_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/**
 * Options passed when running a Screened Poisson Surface Reconstruction
 * process to control how it performs the fitting
 */
struct bg_3d_spsr_opts {
    int degree;                  /**< finite element degree */
    int btype;                   /**< finite element boundary type */
    int depth;                   /**< maximum reconstruction depth */
    int kerneldepth;             /**< kernelDepth */
    int iterations;              /**< iterations */
    int full_depth;              /**< full depth */
    int base_depth;              /**< coarse MG solver depth */
    int baseVcycles;             /**< coarse MG solver v-cycles */
    int max_memory_GB;           /**< maximum memory (in GB) */
    size_t threads;              /**< number of threads to use (default is max available) */
    fastf_t samples_per_node;    /**< minimum number of samples per node */
    fastf_t scale;               /**< scale factor */
    fastf_t width;               /**< voxel width */
    fastf_t confidence;          /**< normal confidence exponent */
    fastf_t confidence_bias;     /**< normal confidence bias exponent */
    fastf_t cgsolver_accuracy;   /**< cg solver accuracy */
    fastf_t point_weight;        /**< interpolation weight */
    int nonManifold;             /**< NonManifold */
    int linearFit;               /**< LinearFit */
    int exact;                   /**< exact interpolation */
};


/**
 * Default Screened Poisson Surface Reconstruction options based on
 * upstream code.
 */
#define BG_3D_SPSR_BOUNDARY_FREE             1
#define BG_3D_SPSR_BOUNDARY_NEUMANN          2
#define BG_3D_SPSR_BOUNDARY_DIRICHLET        3

#define BG_3D_SPSR_DEFAULT_DEGREE            1     /* DEFAULT_FEM_DEGREE */
#define BG_3D_SPSR_DEFAULT_DEPTH             11
#define BG_3D_SPSR_DEFAULT_KERNELDEPTH       0
#define BG_3D_SPSR_DEFAULT_ITERATIONS        0
#define BG_3D_SPSR_DEFAULT_FULL_DEPTH        0
#define BG_3D_SPSR_DEFAULT_BASE_DEPTH        0
#define BG_3D_SPSR_DEFAULT_BASEVCYCLES       0
#define BG_3D_SPSR_DEFAULT_MAX_MEM           0
#define BG_3D_SPSR_DEFAULT_THREADS           0
#define BG_3D_SPSR_DEFAULT_SAMPLES_PER_NODE  1.1
#define BG_3D_SPSR_DEFAULT_SCALE             1.0
#define BG_3D_SPSR_DEFAULT_WIDTH             0.0
#define BG_3D_SPSR_DEFAULT_CONFIDENCE        0.0
#define BG_3D_SPSR_DEFAULT_CONFIDENCE_BIAS   0.0
#define BG_3D_SPSR_DEFAULT_CGSOLVER_ACCURACY 1.0e-3
#define BG_3D_SPSR_DEFAULT_POINT_WEIGHT      8.0    /* DefaultPointWeightMultiplier * Degree */
#define BG_3D_SPSR_DEFAULT_NONMANIFOLD       0
#define BG_3D_SPSR_DEFAULT_LINEARFIT         0
#define BG_3D_SPSR_DEFAULT_EXACT             1

#define BG_3D_SPSR_OPTS_DEFAULT  { \
	BG_3D_SPSR_DEFAULT_DEGREE           , \
	BG_3D_SPSR_BOUNDARY_NEUMANN         , \
	BG_3D_SPSR_DEFAULT_DEPTH            , \
	BG_3D_SPSR_DEFAULT_KERNELDEPTH      , \
	BG_3D_SPSR_DEFAULT_ITERATIONS       , \
	BG_3D_SPSR_DEFAULT_FULL_DEPTH       , \
	BG_3D_SPSR_DEFAULT_BASE_DEPTH       , \
	BG_3D_SPSR_DEFAULT_BASEVCYCLES      , \
	BG_3D_SPSR_DEFAULT_MAX_MEM          , \
	BG_3D_SPSR_DEFAULT_THREADS          , \
	BG_3D_SPSR_DEFAULT_SAMPLES_PER_NODE , \
	BG_3D_SPSR_DEFAULT_SCALE            , \
	BG_3D_SPSR_DEFAULT_WIDTH            , \
	BG_3D_SPSR_DEFAULT_CONFIDENCE       , \
	BG_3D_SPSR_DEFAULT_CONFIDENCE_BIAS  , \
	BG_3D_SPSR_DEFAULT_CGSOLVER_ACCURACY, \
	BG_3D_SPSR_DEFAULT_POINT_WEIGHT     , \
	BG_3D_SPSR_DEFAULT_NONMANIFOLD      , \
	BG_3D_SPSR_DEFAULT_LINEARFIT        , \
	BG_3D_SPSR_DEFAULT_EXACT            }

/**
 *@brief
 * Applies Screened Poisson Surface Reconstruction to build a
 * triangle mesh defining a surface, based on a set of points
 * with associated normals
 *
 * @param[out]  faces set of faces in the output surface, stored as integer indices to the vertices.  The first three indices are the vertices of the face, the second three define the second face, and so forth.
 * @param[out]  num_faces the number of faces in the faces array
 * @param[out]  vertices the set of vertices used by the surface.
 * @param[out]  num_vertices the number of vertices in the surface.
 * @param       input_points_3d The input points
 * @param       input_normals_3d The normals associated with the points
 * @param       num_input_pnts the number of points in the input set
 * @param       opts container holding options to be used in SPSR processing
 * @return 0 if successful, else error
 *
 */
BG_EXPORT int bg_3d_spsr(int **faces, int *num_faces, point_t **vertices, int *num_vertices,
			 const point_t *input_points_3d, const vect_t *input_normals_3d,
			 int num_input_pnts, struct bg_3d_spsr_opts *opts);

__END_DECLS

#endif  /* BG_SPSR_H */
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
