/* meshoptimization.h
 *
 * ----------------------------------------------------------------------
 *
 * Copyright (c) 2014 SURVICE Engineering. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * ----------------------------------------------------------------------
 *
 * Revision Date:
 *  08/26/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Mesh optimization routine.
 *
 */

#ifndef MESHOPTIMIZATION_H
#define MESHOPTIMIZATION_H

// Standard headers
#include <string>

// Local headers
#include "meshoptimizer.h"

#define OPTIMIZATION_FLAGS MO_FLAGS_DISABLE_LOOK_AHEAD | \
  MO_FLAGS_ENABLE_LAZY_SEARCH | \
  MO_FLAGS_FAST_SEED_SELECT
#define DEBUG_MESH_OPTIMIZATION_VERBOSE 0

#ifdef __cplusplus
extern "C" {
#endif

void
mesh_optimization (long vertexcount,
		   long tricount,
		   void *indices,
		   int indiceswidth,
		   int threadcount,
		   int optimizationlevel
#if DEBUG_MESH_OPTIMIZATION_VERBOSE
		   , std::string rname
#endif
		   );

#ifdef __cplusplus
}
#endif

#endif
