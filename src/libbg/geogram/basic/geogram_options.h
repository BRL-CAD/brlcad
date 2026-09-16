/*
 *  Copyright (c) 2000-2022 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

#ifndef GEOBRLCAD_BASIC_GEOBRLCAD_OPTIONS
#define GEOBRLCAD_BASIC_GEOBRLCAD_OPTIONS

#include <string>

/**
 * \file geogram/basic/geogram_options.h
 * \brief Typed options struct for Geogram algorithm configuration.
 *
 * All options previously accessed via CmdLine::get_arg_*("group:key") are
 * represented here as plain typed fields.  Callers construct a GeoOptions
 * value (defaults are appropriate for most uses) and pass it by const
 * reference to every algorithm entry point.  There is no global singleton
 * and no thread-local storage; parallel callers simply pass independent
 * GeoOptions values.
 */

namespace GEOBRL {

    /**
     * \brief Typed runtime options for Geogram algorithms.
     * \details Construct a value, adjust any fields you need, and pass it
     * by const reference to algorithm functions.  The default values
     * match the historic CmdLine defaults.
     */
    struct GeoOptions {

        // ----------------------------------------------------------------
        // algo group
        // ----------------------------------------------------------------

        /** algo:nn_search — nearest-neighbour search backend ("BNN", ...) */
        std::string algo_nn_search    {"BNN"};

        /** algo:delaunay — Delaunay algorithm name ("NN", "BPOW", ...) */
        std::string algo_delaunay     {"NN"};

        /** algo:hole_filling — hole filling algorithm ("loop_split", "ear_cut", ...) */
        std::string algo_hole_filling {"loop_split"};

        /** algo:predicates — geometric predicates mode ("fast", "exact") */
        std::string algo_predicates   {"fast"};

        /** algo:parallel — enable parallel standard algorithms */
        bool        algo_parallel     {true};

        // ----------------------------------------------------------------
        // sys group
        // ----------------------------------------------------------------

        /** sys:multithread — enable multi-threaded computations */
        bool        sys_multithread   {true};

        // ----------------------------------------------------------------
        // remesh group
        // ----------------------------------------------------------------

        /** remesh:multi_nerve — insert vertices to preserve topology */
        bool        remesh_multi_nerve    {true};

        /** remesh:RVC_centroids — use RVC centroids for remeshing */
        bool        remesh_RVC_centroids  {false};

        // ----------------------------------------------------------------
        // dbg group
        // ----------------------------------------------------------------

        /** dbg:save_ANN_histo — save ANN histogram to file */
        bool        dbg_save_ANN_histo    {false};
    };

} // namespace GEOBRL

#endif
