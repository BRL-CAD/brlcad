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
#include <stdint.h>
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
#define BG_3D_SPSR_DEFAULT_DEPTH             8
#define BG_3D_SPSR_DEFAULT_KERNELDEPTH       0
#define BG_3D_SPSR_DEFAULT_ITERATIONS        0
#define BG_3D_SPSR_DEFAULT_FULL_DEPTH        0
#define BG_3D_SPSR_DEFAULT_BASE_DEPTH        0
#define BG_3D_SPSR_DEFAULT_BASEVCYCLES       0
#define BG_3D_SPSR_DEFAULT_MAX_MEM           0
#define BG_3D_SPSR_DEFAULT_THREADS           0
#define BG_3D_SPSR_DEFAULT_SAMPLES_PER_NODE  1.5
#define BG_3D_SPSR_DEFAULT_SCALE             1.1
#define BG_3D_SPSR_DEFAULT_WIDTH             0.0
#define BG_3D_SPSR_DEFAULT_CONFIDENCE        0.0
#define BG_3D_SPSR_DEFAULT_CONFIDENCE_BIAS   0.0
#define BG_3D_SPSR_DEFAULT_CGSOLVER_ACCURACY 1.0e-3
#define BG_3D_SPSR_DEFAULT_POINT_WEIGHT      2.0
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
DEPRECATED BG_EXPORT int bg_3d_spsr(int **faces, int *num_faces, point_t **vertices, int *num_vertices,
			 const point_t *input_points_3d, const vect_t *input_normals_3d,
			 int num_input_pnts, struct bg_3d_spsr_opts *opts);

/** Reasons the SPSR solver may request additional source samples. */
#define BG_3D_SPSR_REFINE_RESIDUAL          0x01u
#define BG_3D_SPSR_REFINE_NORMAL            0x02u
#define BG_3D_SPSR_REFINE_DENSITY           0x04u
#define BG_3D_SPSR_REFINE_SURFACE_VARIATION 0x08u

/** Adaptive SPSR termination states. */
#define BG_3D_SPSR_COMPLETE                 0
#define BG_3D_SPSR_CALLBACK_STOP            1
#define BG_3D_SPSR_POINT_LIMIT              2
#define BG_3D_SPSR_TIME_LIMIT               3
#define BG_3D_SPSR_NO_NEW_SAMPLES           4
#define BG_3D_SPSR_SOLVER_ERROR              5

/**
 * One oriented source sample.  A positive @p thickness records the length of
 * the solid ray partition that supplied the sample.  Non-zero equal pair IDs
 * identify the entry and exit samples of the same partition.
 */
struct bg_3d_spsr_sample {
    point_t point;
    vect_t normal;
    fastf_t thickness;
    uint64_t pair_id;
};

/** A spatial location at which the solver would benefit from more samples. */
struct bg_3d_spsr_refinement_hint {
    point_t point;
    vect_t normal;
    fastf_t error;
    unsigned int reasons;
};

/** Source-to-candidate measurements supplied by an adaptive callback. */
struct bg_3d_spsr_validation {
    size_t ray_count;
    size_t source_hit_rays;
    size_t partition_mismatch_rays;
    size_t thin_partition_count;
    size_t thin_partition_mismatches;
    fastf_t endpoint_error_p95;
    fastf_t endpoint_error_p99;
    fastf_t chord_error_fraction;
    fastf_t surface_area_error_fraction;
    fastf_t volume_error_fraction;
    int passed;
};

/** Information passed to an adaptive source-sampling callback. */
struct bg_3d_spsr_refinement_request {
    size_t pass;
    fastf_t target_feature_size;
    double remaining_time;
    const point_t *vertices;
    size_t vertex_count;
    const int *faces;
    size_t face_count;
    const struct bg_3d_spsr_refinement_hint *hints;
    size_t hint_count;
};

/**
 * Callback response.  Sample storage remains owned by the callback and need
 * only remain valid until the callback returns; libbg copies it immediately.
 */
struct bg_3d_spsr_refinement_response {
    const struct bg_3d_spsr_sample *samples;
    size_t sample_count;
    struct bg_3d_spsr_validation validation;
    /** Non-zero asks the adaptive driver to stop after this candidate. */
    int stop_refinement;
};

typedef int (*bg_3d_spsr_refinement_func_t)(
    struct bg_3d_spsr_refinement_response *response,
    const struct bg_3d_spsr_refinement_request *request,
    void *client_data);

/** Options controlling a bounded adaptive SPSR reconstruction. */
struct bg_3d_spsr_adaptive_opts {
    struct bg_3d_spsr_opts solver;
    size_t max_refinement_passes;
    size_t max_points;
    fastf_t target_feature_size;
    double max_time;
};

/** Last-resort solve budget; callbacks may stop earlier when progress stalls. */
#define BG_3D_SPSR_DEFAULT_REFINEMENT_PASSES 8
#define BG_3D_SPSR_ADAPTIVE_OPTS_DEFAULT { \
	BG_3D_SPSR_OPTS_DEFAULT, \
	BG_3D_SPSR_DEFAULT_REFINEMENT_PASSES, \
	0, \
	0.0, \
	0.0 }

/** Summary of an adaptive SPSR attempt. */
struct bg_3d_spsr_report {
    size_t solve_count;
    size_t initial_sample_count;
    size_t final_sample_count;
    size_t requested_sample_count;
    size_t accepted_sample_count;
    double elapsed_time;
    int termination;
    struct bg_3d_spsr_validation validation;
};

/**
 * Reconstruct a surface and, when requested, obtain targeted additional
 * samples from @p refine.  A callback is required when refinement passes are
 * enabled.  The final candidate must be marked as passed by the callback.
 */
BG_EXPORT int bg_3d_spsr_adaptive(int **faces, int *num_faces,
        point_t **vertices, int *num_vertices,
        const struct bg_3d_spsr_sample *input_samples, size_t sample_count,
        const struct bg_3d_spsr_adaptive_opts *opts,
        bg_3d_spsr_refinement_func_t refine, void *client_data,
        struct bg_3d_spsr_report *report);

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
