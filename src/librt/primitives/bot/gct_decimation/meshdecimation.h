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


#ifndef MD_MESHDECIMATION_H
#define MD_MESHDECIMATION_H


#include "common.h"

#include <stddef.h>


#define MD_ATTRIB_MAX (16)


typedef struct {
    void *base;
    int width;
    size_t count;
    size_t stride;
    int flags;
} mdOpAttrib;


typedef struct {
    double progress;
    int stage;
    const char *stagename;
    long trianglecount;
} mdStatus;

enum {
    MD_STATUS_STAGE_INIT,
    MD_STATUS_STAGE_BUILDVERTICES,
    MD_STATUS_STAGE_BUILDTRIANGLES,
    MD_STATUS_STAGE_BUILDTRIREFS,
    MD_STATUS_STAGE_BUILDQUEUE,
    MD_STATUS_STAGE_DECIMATION,
    MD_STATUS_STAGE_STORE,
    MD_STATUS_STAGE_DONE,

    MD_STATUS_STAGE_COUNT
};


typedef struct {
    size_t vertexcount;
    void *vertex;
    int vertexwidth;
    size_t vertexstride;
    size_t vertexalloc;

    void *indices;
    int indiceswidth;
    size_t indicesstride;
    size_t tricount;

    double decimationstrength;
    int decimationcount;

    int attribcount;
    mdOpAttrib attrib[MD_ATTRIB_MAX];
    mdOpAttrib normalattrib;

    long msecs;

    /* Status callback */
    long statusmiliseconds;
    void *statusopaquepointer;
    void (*statuscallback)(void *opaquepointer, const mdStatus *status);

    /* Advanced configuration options */
    double compactnesstarget;
    double compactnesspenalty;
    int syncstepcount;
    double normalsearchangle;
    size_t maxmemorysize;

} mdOperation;


/*
 * This function exists only to read CPUID and NUMA information once in a thread safe
 * manner, in case you want to launch multiple mdMeshDecimation() simultaneously.
 */
void mdInit();


/*
 * Set up the mdOperation struct to configure a mesh decimation task.
 */
void mdOperationInit(mdOperation *op);
void mdOperationData(mdOperation *op, size_t vertexcount, void *vertex, int vertexwidth, size_t vertexstride, size_t tricount, void *indices, int indiceswidth, size_t indicesstride);
int mdOperationAddAttrib(mdOperation *op, void *base, int width, size_t count, size_t stride, int flags);
void mdOperationStrength(mdOperation *op, double decimationstrength);
void mdOperationStatusCallback(mdOperation *op, void (*statuscallback)(void *opaquepointer, const mdStatus *status), void *opaquepointer, long miliseconds);

#define MD_ATTRIB_FLAGS_NORMALIZE (0x1)
#define MD_ATTRIB_FLAGS_COMPUTE_NORMALS (0x2)


/*
 * Decimate the mesh specifed by the mdOperation struct.
 */
int mdMeshDecimation(mdOperation *operation, int flags);

/* Slightly increases the quality of the mesh decimation */
#define MD_FLAGS_CONTINUOUS_UPDATE (0x1)
/* Do not pack vertices */
#define MD_FLAGS_NO_VERTEX_PACKING (0x2)
/* Allow cloning/splitting of vertices with diverging normals when recomputing normals */
#define MD_FLAGS_NORMAL_VERTEX_SPLITTING (0x4)
/* Define orientation of triangles when rebuilding normals */
#define MD_FLAGS_TRIANGLE_WINDING_CW (0x8)
#define MD_FLAGS_TRIANGLE_WINDING_CCW (0x10)
/* Do not actually do any decimation, in case all you actually want to do is to recompute normals */
#define MD_FLAGS_NO_DECIMATION (0x20)


/* Optimize the mesh */
int moOptimizeMesh(size_t vertexcount, size_t tricount, void *indices, int indiceswidth, size_t indicesstride, void (*shufflecallback)(void *opaquepointer, long newvertexindex, long oldvertexindex), void *shuffleopaquepointer, int vertexcachesize, int flags);

#define MO_FLAGS_FIXED_CACHE_SIZE (0x1)
#define MO_FLAGS_DISABLE_LOOK_AHEAD (0x2)
#define MO_FLAGS_ENABLE_LAZY_SEARCH (0x4)
#define MO_FLAGS_FAST_SEED_SELECT (0x8)

#endif
