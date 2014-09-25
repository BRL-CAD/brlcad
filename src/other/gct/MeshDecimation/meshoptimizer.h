/* *****************************************************************************
 *
 * Copyright (c) 2014 Alexis Naveros. All rights reserved.
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
 * *****************************************************************************
 */

#ifdef __cplusplus
extern "C" {
#endif


int moOptimizeMesh( size_t vertexcount, size_t tricount, void *indices, int indiceswidth, size_t indicesstride, void (*shufflecallback)( void *opaquepointer, long newvertexindex, long oldvertexindex ), void *shuffleopaquepointer, int vertexcachesize, int threadcount, int flags );

#define MO_FLAGS_FIXED_CACHE_SIZE (0x1)
#define MO_FLAGS_DISABLE_LOOK_AHEAD (0x2)
#define MO_FLAGS_ENABLE_LAZY_SEARCH (0x4)
#define MO_FLAGS_FAST_SEED_SELECT (0x8)


double moEvaluateMesh( size_t tricount, void *indices, int indiceswidth, size_t indicesstride, int vertexcachesize, int flags );


#ifdef __cplusplus
}
#endif

