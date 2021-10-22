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

#if defined(__GNUC__) && (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
#endif


#include "common.h"

#include "meshdecimation.h"

#include "./util.h"
#include "bu/tc.h"

#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/simd.h"
#include "bu/time.h"
#include "vmath.h"

#include <string.h>

#if defined(HAVE_ISNAN) && !defined(HAVE_DECL_ISNAN) && !defined(isnan)
extern int isnan(double x);
#endif


/* Define to use double floating point precision */
#define MD_CONF_DOUBLE_PRECISION


/* Define to use double precision just quadric maths. Very strongly recommended. */
#define MD_CONF_QUADRICS_DOUBLE_PRECISION

/* Define to call a status callback function. */
#define MD_CONF_ENABLE_PROGRESS


/**/


#define MD_BOUNDARY_WEIGHT (5.0)

#define MD_COLLAPSE_PENALTY_COMPACT_TARGET (0.25)

#define MD_COLLAPSE_PENALTY_COMPACT_FACTOR (0.00125)

#define MD_GLOBAL_LOCK_THRESHOLD (16)


/**/


/* Required with multithreading! */
#define MD_CONFIG_DELAYED_OP_REDIRECT


#if defined(__x86_64__) || defined(__i386__)
/* disabled pending testing */
/* #define MD_CONFIG_SSE_SUPPORT */
#endif


#define MD_CONFIG_HIGH_QUADRICS


#define MD_CONFIG_AREA_QUADRICS


/**/


#if defined(__GNUC__) || defined(__INTEL_COMPILER)
#if !defined(__OpenBSD__)
#define RF_ALIGN16 __attribute__((aligned(16)))
#define RF_ALIGN64 __attribute__((aligned(64)))
#else
#define RF_ALIGN16
#define RF_ALIGN64
#endif
#elif defined(_MSC_VER)
#define RF_ALIGN16 __declspec(align(16))
#define RF_ALIGN64 __declspec(align(64))
#else
#define RF_ALIGN16
#define RF_ALIGN64
#ifdef MD_CONFIG_SSE_SUPPORT
#undef MD_CONFIG_SSE_SUPPORT
#endif
#endif


#ifdef MD_CONFIG_SSE_SUPPORT
extern int mdPathSSE4p1;
float mdEdgeCollapsePenaltyTriangleSSE4p1f(float *newpoint, float *oldpoint, float *leftpoint, float *rightpoint, int *denyflag, float compactnesstarget);
double mdEdgeCollapsePenaltyTriangleSSE4p1d(double *newpoint, double *oldpoint, double *leftpoint, double *rightpoint, int *denyflag, double compactnesstarget);
extern int mdPathSSE3;
float mdEdgeCollapsePenaltyTriangleSSE3f(float *newpoint, float *oldpoint, float *leftpoint, float *rightpoint, int *denyflag, float compactnesstarget);
double mdEdgeCollapsePenaltyTriangleSSE3d(double *newpoint, double *oldpoint, double *leftpoint, double *rightpoint, int *denyflag, double compactnesstarget);
extern int mdPathSSE2;
float mdEdgeCollapsePenaltyTriangleSSE2f(float *newpoint, float *oldpoint, float *leftpoint, float *rightpoint, int *denyflag, float compactnesstarget);
double mdEdgeCollapsePenaltyTriangleSSE2d(double *newpoint, double *oldpoint, double *leftpoint, double *rightpoint, int *denyflag, double compactnesstarget);
#endif


/**/


static int mdInitFlag = 0;


/**/


#ifdef MD_CONF_DOUBLE_PRECISION

typedef double mdf;
#  define mdfabs(x) fabs(x)
#  define mdfacos(x) acos(x)
#  define mdfcbrt(x) cbrt(x)
#  define mdfceil(x) ceil(x)
#  define mdffloor(x) floor(x)
#  define mdfmax(x,y) FMAX((x),(y))
#  define mdfsqrt(x) sqrt(x)

#  ifdef HAVE_LOG2
#    define mdflog2(x) log2(x)
#  else
#    define mdflog2(x) (log(x) / log(2))
#  endif
#  define mdfmin(x,y) FMIN((x),(y))
#  define mdfround(x) rint(x)
#else

typedef float mdf;
#  define mdfabs(x) fabsf(x)
#  define mdfacos(x) acosf(x)
#  define mdfcbrt(x) cbrtf(x)
#  define mdfceil(x) ceilf(x)
#  define mdffloor(x) floorf(x)
#  define mdfmax(x,y) fmaxf((x),(y))
#  define mdfsqrt(x) sqrtf(x)

#  ifdef HAVE_LOG2F
#    define mdflog2(x) log2f(x)
#  else
#    define mdflog2(x) (logf(x) / logf(2))
#  endif
#  define mdfmin(x,y) fminf((x),(y))
#  define mdfround(x) roundf(x)

#endif

#ifdef MD_CONF_DOUBLE_PRECISION
#  ifndef MD_CONF_QUADRICS_DOUBLE_PRECISION
#    define MD_CONF_QUADRICS_DOUBLE_PRECISION
#  endif
#endif


#ifdef MD_CONF_QUADRICS_DOUBLE_PRECISION
typedef double mdqf;
#define mdqfabs(x) fabs(x)
#else
typedef float mdqf;
#define mdqfabs(x) fabsf(x)
#endif


#ifdef MD_CONFIG_HIGH_QUADRICS
#if ( __GNUC__ > 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ >= 6 ) ) && !defined(__INTEL_COMPILER) && (defined __x86_64__ || defined(__i386__) || defined(__ia64__))
typedef __float128 mdqfhigh;
#else
#undef MD_CONFIG_HIGH_QUADRICS
#endif
#endif


typedef int32_t mdi;


typedef struct {
    mdqf area;
    mdqf a2, ab, ac, ad;
    mdqf b2, bc, bd;
    mdqf c2, cd;
#ifdef MD_CONFIG_HIGH_QUADRICS
    mdqfhigh d2;
#else
    mdqf d2;
#endif
} mathQuadric;

static void mathQuadricInit(mathQuadric *q, mdqf a, mdqf b, mdqf c, mdqf d, mdqf area)
{
    q->area = area;

    q->a2 = a * a;
    q->ab = a * b;
    q->ac = a * c;
    q->ad = a * d;
    q->b2 = b * b;
    q->bc = b * c;
    q->bd = b * d;
    q->c2 = c * c;
    q->cd = c * d;
#ifdef MD_CONFIG_HIGH_QUADRICS
    q->d2 = (mdqfhigh)d * (mdqfhigh)d;
#else
    q->d2 = d * d;
#endif
}


/****/


static mdqf mathMatrix3x3Determinant(mdqf *m)
{
    mdqf det;
    det  = m[0 * 3 + 0] * (m[2 * 3 + 2] * m[1 * 3 + 1] - m[2 * 3 + 1] * m[1 * 3 + 2]);
    det -= m[1 * 3 + 0] * (m[2 * 3 + 2] * m[0 * 3 + 1] - m[2 * 3 + 1] * m[0 * 3 + 2]);
    det += m[2 * 3 + 0] * (m[1 * 3 + 2] * m[0 * 3 + 1] - m[1 * 3 + 1] * m[0 * 3 + 2]);
    return det;
}

static void mathMatrix3x3MulVector(mdqf *vdst, mdqf *m, mdqf *v)
{
    vdst[0] = v[0] * m[0 * 3 + 0] + v[1] * m[1 * 3 + 0] + v[2] * m[2 * 3 + 0];
    vdst[1] = v[0] * m[0 * 3 + 1] + v[1] * m[1 * 3 + 1] + v[2] * m[2 * 3 + 1];
    vdst[2] = v[0] * m[0 * 3 + 2] + v[1] * m[1 * 3 + 2] + v[2] * m[2 * 3 + 2];
}


static void mathQuadricToMatrix3x3(mdqf *m, mathQuadric *q)
{
    m[0 * 3 + 0] = q->a2;
    m[0 * 3 + 1] = q->ab;
    m[0 * 3 + 2] = q->ac;
    m[1 * 3 + 0] = q->ab;
    m[1 * 3 + 1] = q->b2;
    m[1 * 3 + 2] = q->bc;
    m[2 * 3 + 0] = q->ac;
    m[2 * 3 + 1] = q->bc;
    m[2 * 3 + 2] = q->c2;
}


static void mathMatrix3x3Invert(mdqf *mdst, mdqf *m, mdqf det)
{
    mdf detinv;
    detinv = 1.0 / det;
    mdst[0 * 3 + 0] = (m[2 * 3 + 2] * m[1 * 3 + 1] - m[2 * 3 + 1] * m[1 * 3 + 2]) * detinv;
    mdst[0 * 3 + 1] = -(m[2 * 3 + 2] * m[0 * 3 + 1] - m[2 * 3 + 1] * m[0 * 3 + 2]) * detinv;
    mdst[0 * 3 + 2] = (m[1 * 3 + 2] * m[0 * 3 + 1] - m[1 * 3 + 1] * m[0 * 3 + 2]) * detinv;
    mdst[1 * 3 + 0] = -(m[2 * 3 + 2] * m[1 * 3 + 0] - m[2 * 3 + 0] * m[1 * 3 + 2]) * detinv;
    mdst[1 * 3 + 1] = (m[2 * 3 + 2] * m[0 * 3 + 0] - m[2 * 3 + 0] * m[0 * 3 + 2]) * detinv;
    mdst[1 * 3 + 2] = -(m[1 * 3 + 2] * m[0 * 3 + 0] - m[1 * 3 + 0] * m[0 * 3 + 2]) * detinv;
    mdst[2 * 3 + 0] = (m[2 * 3 + 1] * m[1 * 3 + 0] - m[2 * 3 + 0] * m[1 * 3 + 1]) * detinv;
    mdst[2 * 3 + 1] = -(m[2 * 3 + 1] * m[0 * 3 + 0] - m[2 * 3 + 0] * m[0 * 3 + 1]) * detinv;
    mdst[2 * 3 + 2] = (m[1 * 3 + 1] * m[0 * 3 + 0] - m[1 * 3 + 0] * m[0 * 3 + 1]) * detinv;
}


/****/


static int mathQuadricSolve(mathQuadric *q, mdf *v)
{
    mdqf det, m[9], minv[9], vector[3], vres[3];

    mathQuadricToMatrix3x3(m, q);
    det = mathMatrix3x3Determinant(m);

    if (mdqfabs(det) < 0.00001) {
	/* det fail */
	return 0;
    }

    mathMatrix3x3Invert(minv, m, det);
    vector[0] = -q->ad;
    vector[1] = -q->bd;
    vector[2] = -q->cd;
    mathMatrix3x3MulVector(vres, minv, vector);
    v[0] =  vres[0];
    v[1] =  vres[1];
    v[2] =  vres[2];

    return 1;
}

static void mathQuadricZero(mathQuadric *q)
{
    q->area = 0.0;
    q->a2 = 0.0;
    q->ab = 0.0;
    q->ac = 0.0;
    q->ad = 0.0;
    q->b2 = 0.0;
    q->bc = 0.0;
    q->bd = 0.0;
    q->c2 = 0.0;
    q->cd = 0.0;
    q->d2 = 0.0;
}


static void mathQuadricAddStoreQuadric(mathQuadric *qdst, mathQuadric *q0, mathQuadric *q1)
{
    qdst->area = q0->area + q1->area;
    qdst->a2 = q0->a2 + q1->a2;
    qdst->ab = q0->ab + q1->ab;
    qdst->ac = q0->ac + q1->ac;
    qdst->ad = q0->ad + q1->ad;
    qdst->b2 = q0->b2 + q1->b2;
    qdst->bc = q0->bc + q1->bc;
    qdst->bd = q0->bd + q1->bd;
    qdst->c2 = q0->c2 + q1->c2;
    qdst->cd = q0->cd + q1->cd;
    qdst->d2 = q0->d2 + q1->d2;
}

static void mathQuadricAddQuadric(mathQuadric *qdst, mathQuadric *q)
{
    qdst->area += q->area;
    qdst->a2 += q->a2;
    qdst->ab += q->ab;
    qdst->ac += q->ac;
    qdst->ad += q->ad;
    qdst->b2 += q->b2;
    qdst->bc += q->bc;
    qdst->bd += q->bd;
    qdst->c2 += q->c2;
    qdst->cd += q->cd;
    qdst->d2 += q->d2;
}

/* A volatile variable is used to force the compiler to do the math strictly in the order specified. */
static mdf mathQuadricEvaluate(mathQuadric *q, mdf *v)
{
#ifdef MD_CONFIG_HIGH_QUADRICS
    volatile mdqfhigh d;
    mdqfhigh vh[3];
    vh[0] = v[0];
    vh[1] = v[1];
    vh[2] = v[2];

    d = vh[0] * vh[0] * (mdqfhigh)q->a2 + vh[1] * vh[1] * (mdqfhigh)q->b2 + vh[2] * vh[2] * (mdqfhigh)q->c2;
    d += 2.0 * (vh[0] * vh[1] * (mdqfhigh)q->ab + vh[0] * vh[2] * (mdqfhigh)q->ac + vh[1] * vh[2] * (mdqfhigh)q->bc);
    d += 2.0 * (vh[0] * (mdqfhigh)q->ad + vh[1] * (mdqfhigh)q->bd + vh[2] * (mdqfhigh)q->cd);
    d += q->d2;
#else
    volatile mdqf d;
    mdqf vd[3];
    vd[0] = v[0];
    vd[1] = v[1];
    vd[2] = v[2];

    d = vd[0] * vd[0] * q->a2 + vd[1] * vd[1] * q->b2 + vd[2] * vd[2] * q->c2;
    d += 2.0 * (vd[0] * vd[1] * q->ab + vd[0] * vd[2] * q->ac + vd[1] * vd[2] * q->bc);
    d += 2.0 * (vd[0] * q->ad + vd[1] * q->bd + vd[2] * q->cd);
    d += q->d2;
#endif

    return (mdf)d;
}

static void mathQuadricMul(mathQuadric *qdst, mdf f)
{
    qdst->a2 *= f;
    qdst->ab *= f;
    qdst->ac *= f;
    qdst->ad *= f;
    qdst->b2 *= f;
    qdst->bc *= f;
    qdst->bd *= f;
    qdst->c2 *= f;
    qdst->cd *= f;
    qdst->d2 *= f;
}


/****/


typedef struct {
    bu_mtx_t mutex;
    bu_cnd_t signal;
    int resetcount;
    volatile int index;
    volatile int count[2];
} mdBarrier;

static void mdBarrierInit(mdBarrier *barrier, int count)
{
    if (bu_mtx_init(&barrier->mutex) == bu_thrd_error)
	bu_bomb("bu_mtx_init() failed");

    if (bu_cnd_init(&barrier->signal) != bu_thrd_success)
	bu_bomb("bu_cnd_init() failed");

    barrier->resetcount = count;
    barrier->index = 0;
    barrier->count[0] = count;
    barrier->count[1] = count;
}

static void mdBarrierDestroy(mdBarrier *barrier)
{
    bu_mtx_destroy(&barrier->mutex);
    bu_cnd_destroy(&barrier->signal);
}

static int mdBarrierSync(mdBarrier *barrier)
{
    int vindex, ret;

    if (bu_mtx_lock(&barrier->mutex) == bu_thrd_error)
	bu_bomb("bu_mtx_lock() failed");

    vindex = barrier->index;
    ret = 0;

    if (!(--barrier->count[vindex])) {
	ret = 1;

	if (bu_cnd_broadcast(&barrier->signal) == bu_thrd_error)
	    bu_bomb("bu_cnd_broadcast() failed");

	vindex ^= 1;
	barrier->index = vindex;
	barrier->count[vindex] = barrier->resetcount;
    } else {
	for (; barrier->count[vindex];)
	    if (bu_cnd_wait(&barrier->signal, &barrier->mutex) == bu_thrd_error)
		bu_bomb("bu_cnd_wait() failed");
    }

    if (bu_mtx_unlock(&barrier->mutex) == bu_thrd_error)
	bu_bomb("bu_mtx_unload() failed");

    return ret;
}

#if 0
static int mdBarrierSyncTimeout(mdBarrier *barrier, long miliseconds)
{
    int vindex, ret;

    if (bu_mtx_lock(&barrier->mutex) == bu_thrd_error)
	bu_bomb("bu_mtx_lock() failed");

    vindex = barrier->index;
    ret = 0;

    if (!(--barrier->count[vindex])) {
	ret = 1;

	if (bu_cnd_broadcast(&barrier->signal) == bu_thrd_error)
	    bu_bomb("bu_cnd_broadcast() failed");

	vindex ^= 1;
	barrier->index = vindex;
	barrier->count[vindex] = barrier->resetcount;
    } else {
	mtSignalWaitTimeout(&barrier->signal, &barrier->mutex, miliseconds);

	if (!(barrier->count[vindex]))
	    ret = 1;
	else
	    barrier->count[vindex]++;
    }

    if (bu_mtx_unlock(&barrier->mutex) == bu_thrd_error)
	bu_bomb("bu_mtx_unlock() failed");

    return ret;
}
#endif

/****/


/* 16 bytes; OK */
typedef struct {
    mdi v[3];
    union {
	int edgeflags;
	mdi redirectindex;
    } u;
} mdTriangle;

typedef struct {
    mdi v[2];
    mdi triindex;
    void *op;
} mdEdge;

/* 76 bytes, try packing to 64 bytes? */
typedef struct RF_ALIGN16 {
#ifdef MD_CONFIG_SSE_SUPPORT
    mdf RF_ALIGN16 point[4];
#else
    mdf point[3];
#endif
#ifdef MM_ATOMIC_SUPPORT
    mmAtomic32 atomicowner;
#else
    int owner;
    mtSpin ownerspinlock;
#endif
    mdi trirefbase;
    mdi trirefcount;
    mdi redirectindex;
    mathQuadric quadric;
} mdVertex;


typedef struct {
    int threadcount;
    uint32_t operationflags;
    int updatestatusflag;

    /* User supplied raw data */
    mdf *point;
    size_t pointstride;
    void *indices;
    size_t indicesstride;
    void (*indicesUserToNative)(mdi *dst, void *src);
    void (*indicesNativeToUser)(void *dst, mdi *src);
    void (*vertexUserToNative)(mdf *dst, void *src);
    void (*vertexNativeToUser)(void *dst, mdf *src);

    /* Per-vertex triangle references */
    mdi *trireflist;
    mdi trirefcount;
    long trirefalloc;
    char paddingA[64];
#ifdef MM_ATOMIC_SUPPORT
    mmAtomic32 trireflock;
#else
    mtSpin trirefspinlock;
#endif
    char paddingB[64];

    /* Synchronization stuff */
    mdBarrier workbarrier;
    mdBarrier globalbarrier;
    int updatebuffercount;
    int updatebuffershift;

    /* List of triangles */
    mdTriangle *trilist;
    long tricount;
    long tripackcount;

    /* List of vertices */
    mdVertex *vertexlist;
    long vertexcount;
    long vertexalloc;
    long vertexpackcount;

    /* Hash table to locate edges from their vertex indices */
    void *edgehashtable;

    /* Collapse penalty function */
    mdf(*collapsepenalty)(mdf *newpoint, mdf *oldpoint, mdf *leftpoint, mdf *rightpoint, int *denyflag, mdf compactnesstarget);

    /* Custom vertex attributes besides point position */
    int attribcount;
    mdOpAttrib *attrib;

    /* Decimation strength, max cost */
    mdf maxcollapsecost;

    char paddingC[64];
#ifdef MM_ATOMIC_SUPPORT
    mmAtomic32 globalvertexlock;
#else
    mtSpin globalvertexspinlock;
#endif
    char paddingD[64];

    /* Advanced configuration options */
    mdf compactnesstarget;
    mdf compactnesspenalty;
    int syncstepcount;
    mdf normalsearchangle;

    /* Normal recomputation buffers */
    int clonesearchindex;
    void *vertexnormal;
    void *trinormal;
} mdMesh;


static void mdIndicesInt8ToNative(mdi *dst, void *src)
{
    uint8_t *s = (uint8_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
}

static void mdIndicesInt16ToNative(mdi *dst, void *src)
{
    uint16_t *s = (uint16_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
}

static void mdIndicesInt32ToNative(mdi *dst, void *src)
{
    uint32_t *s = (uint32_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
}

static void mdIndicesInt64ToNative(mdi *dst, void *src)
{
    uint64_t *s = (uint64_t *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
}


static void mdIndicesNativeToInt8(void *dst, mdi *src)
{
    uint8_t *d = (uint8_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
}

static void mdIndicesNativeToInt16(void *dst, mdi *src)
{
    uint16_t *d = (uint16_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
}

static void mdIndicesNativeToInt32(void *dst, mdi *src)
{
    uint32_t *d = (uint32_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
}

static void mdIndicesNativeToInt64(void *dst, mdi *src)
{
    uint64_t *d = (uint64_t *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
}


static void mdVertexFloatToNative(mdf *dst, void *src)
{
    float *s = (float *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
}

static void mdVertexDoubleToNative(mdf *dst, void *src)
{
    double *s = (double *)src;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
}

static void mdVertexNativeToFloat(void *dst, mdf *src)
{
    float *d = (float *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
}

static void mdVertexNativeToDouble(void *dst, mdf *src)
{
    double *d = (double *)dst;
    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
}


static void mdTriangleComputeQuadric(mdMesh *mesh, mdTriangle *tri, mathQuadric *q)
{
    mdf area, norm, norminv, vecta[3], vectb[3], plane[4];
    mdVertex *vertex0, *vertex1, *vertex2;

    vertex0 = &mesh->vertexlist[tri->v[0]];
    vertex1 = &mesh->vertexlist[tri->v[1]];
    vertex2 = &mesh->vertexlist[tri->v[2]];

    VSUB2(vecta, vertex1->point, vertex0->point);
    VSUB2(vectb, vertex2->point, vertex0->point);
    VCROSS(plane, vectb, vecta);

    norm = mdfsqrt(VDOT(plane, plane));

    if (!ZERO(norm)) {
	area = 0.5 * norm;
#ifdef MD_CONFIG_AREA_QUADRICS
	norminv = 0.5;
#else
	norminv = 1.0 / norm;
#endif
	plane[0] *= norminv;
	plane[1] *= norminv;
	plane[2] *= norminv;
	plane[3] = -VDOT(plane, vertex0->point);
    } else {
	area = 0.0;
	plane[0] = 0.0;
	plane[1] = 0.0;
	plane[2] = 0.0;
	plane[3] = 0.0;
    }

    mathQuadricInit(q, plane[0], plane[1], plane[2], plane[3], area);
}


static mdf mdEdgeSolvePoint(mdVertex *vertex0, mdVertex *vertex1, mdf *point)
{
    mdf cost, bestcost;
    mdf midpoint[3];
    mdf *bestpoint;
    mathQuadric q;

    mathQuadricAddStoreQuadric(&q, &vertex0->quadric, &vertex1->quadric);

    if (mathQuadricSolve(&q, point))
	bestcost = mathQuadricEvaluate(&q, point);
    else {
	midpoint[0] = 0.5 * (vertex0->point[0] + vertex1->point[0]);
	midpoint[1] = 0.5 * (vertex0->point[1] + vertex1->point[1]);
	midpoint[2] = 0.5 * (vertex0->point[2] + vertex1->point[2]);
	bestcost = mathQuadricEvaluate(&q, midpoint);

	bestpoint = midpoint;
	cost = mathQuadricEvaluate(&q, vertex0->point);

	if (cost < bestcost) {
	    bestcost = cost;
	    bestpoint = vertex0->point;
	}

	cost = mathQuadricEvaluate(&q, vertex1->point);

	if (cost < bestcost) {
	    bestcost = cost;
	    bestpoint = vertex1->point;
	}

	VMOVE(point, bestpoint);
    }

    return bestcost;
}


/****/


static void mdMeshAccumulateBoundary(mdVertex *vertex0, mdVertex *vertex1, mdVertex *vertex2)
{
    mdf normal[3], sideplane[4], vecta[3], vectb[3], norm, norminv, area;
    mathQuadric q;

    VSUB2(vecta, vertex1->point, vertex0->point);
    VSUB2(vectb, vertex2->point, vertex0->point);
    VCROSS(normal, vectb, vecta);
    norm = mdfsqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    area = 0.5 * norm;
    norminv = 1.0 / norm;
    VSCALE(normal, normal, norminv);

    VCROSS(sideplane, vecta, normal);
    VUNITIZE(sideplane);
    sideplane[3] = -VDOT(sideplane, vertex0->point);

    mathQuadricInit(&q, sideplane[0], sideplane[1], sideplane[2], sideplane[3], area);
    mathQuadricMul(&q, MD_BOUNDARY_WEIGHT);

#ifdef MM_ATOMIC_SUPPORT
    mmAtomicSpin32(&vertex0->atomicowner, -1, 0xffff);
    mathQuadricAddQuadric(&vertex0->quadric, &q);
    mmAtomicWrite32(&vertex0->atomicowner, -1);
#else
    mtSpinLock(&vertex0->ownerspinlock);
    mathQuadricAddQuadric(&vertex0->quadric, &q);
    mtSpinUnlock(&vertex0->ownerspinlock);
#endif

#ifdef MM_ATOMIC_SUPPORT
    mmAtomicSpin32(&vertex1->atomicowner, -1, 0xffff);
    mathQuadricAddQuadric(&vertex1->quadric, &q);
    mmAtomicWrite32(&vertex1->atomicowner, -1);
#else
    mtSpinLock(&vertex1->ownerspinlock);
    mathQuadricAddQuadric(&vertex1->quadric, &q);
    mtSpinUnlock(&vertex1->ownerspinlock);
#endif
}


/****/


static void mdEdgeHashClearEntry(void *entry)
{
    mdEdge *edge = (mdEdge *)entry;
    edge->v[0] = -1;
}

static int mdEdgeHashEntryValid(void *entry)
{
    mdEdge *edge = (mdEdge *)entry;
    return (edge->v[0] >= 0 ? 1 : 0);
}

static uint32_t mdEdgeHashEntryKey(void *entry)
{
    mdEdge *edge = (mdEdge *)entry;
    uint32_t hashkey;

    hashkey  = edge->v[0];
    hashkey += hashkey << 10;
    hashkey ^= hashkey >> 6;
    hashkey += edge->v[1];
    hashkey += hashkey << 10;
    hashkey ^= hashkey >> 6;
    hashkey += hashkey << 6;
    hashkey ^= hashkey >> 11;
    hashkey += hashkey << 15;

    return hashkey;
}

static int mdEdgeHashEntryCmp(void *entry, void *entryref)
{
    mdEdge *edge = (mdEdge *)entry, *edgeref = (mdEdge *)entryref;

    if (edge->v[0] == -1)
	return MM_HASH_ENTRYCMP_INVALID;

    if ((edge->v[0] == edgeref->v[0]) && (edge->v[1] == edgeref->v[1]))
	return MM_HASH_ENTRYCMP_FOUND;

    return MM_HASH_ENTRYCMP_SKIP;
}

static mmHashAccess mdEdgeHashEdge = {
    mdEdgeHashClearEntry,
    mdEdgeHashEntryValid,
    mdEdgeHashEntryKey,
    mdEdgeHashEntryCmp,
    NULL
};

static int mdMeshHashInit(mdMesh *mesh, size_t trianglecount, mdf hashextrabits, uint32_t lockpageshift, size_t maxmemorysize)
{
    size_t edgecount, hashmemsize, meshmemsize, totalmemorysize;
    uint32_t hashbits, hashbitsmin, hashbitsmax;

    edgecount = trianglecount * 3;

    /* Hard minimum count of hash table bits */
#if defined(CPUCONF_WORD_SIZE) && CPUCONF_WORD_SIZE == 64
    hashbitsmin = ccLog2Int64(edgecount) + 1;
#else
    hashbitsmin = ccLog2Int(edgecount) + 1;
#endif
    hashbitsmax = hashbitsmin + 4;

    hashbits = mdfround(mdflog2((mdf)edgecount) + hashextrabits);

    if (hashbits < hashbitsmin)
	hashbits = hashbitsmin;
    else if (hashbits > hashbitsmax)
	hashbits = hashbitsmax;

    if (hashbits < 12)
	hashbits = 12;

    /* lockpageshift = 7; works great, 128 hash entries per lock page */
    if (lockpageshift < 3)
	lockpageshift = 3;
    else if (lockpageshift > 16)
	lockpageshift = 16;

    meshmemsize = (mesh->tricount * sizeof(mdTriangle)) + (mesh->vertexcount * sizeof(mdVertex));

    for (;; hashbits--) {
	if (hashbits < hashbitsmin)
	    return 0;

	hashmemsize = mmHashRequiredSize(sizeof(mdEdge), hashbits, lockpageshift);
	totalmemorysize = hashmemsize + meshmemsize;

	/* Increase estimate of memory consumption by 25% to account for extra stuff not counted here */
	totalmemorysize += totalmemorysize >> 2;

	if (totalmemorysize > maxmemorysize)
	    continue;

	mesh->edgehashtable = bu_malloc(hashmemsize, "edgehashtable");

	if (mesh->edgehashtable)
	    break;
    }

    mmHashInit(mesh->edgehashtable, &mdEdgeHashEdge, sizeof(mdEdge), hashbits, lockpageshift, MM_HASH_FLAGS_NO_COUNT);

    return 1;
}

static void mdMeshHashEnd(mdMesh *mesh)
{
    bu_free(mesh->edgehashtable, "edgehashtable");
}


/****/


typedef struct RF_ALIGN64 {
    void **opbuffer;
    int opcount;
    int opalloc;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomic32 atomlock;
#else
    mtSpin spinlock;
#endif
} mdUpdateBuffer;


/* 56 bytes, try padding to 64? */
typedef struct {
#ifdef MM_ATOMIC_SUPPORT
    mmAtomic32 flags;
#else
    int flags;
    mtSpin spinlock;
#endif
    mdUpdateBuffer *updatebuffer;
    mdi v0, v1;
#ifdef MD_CONFIG_SSE_SUPPORT
    mdf RF_ALIGN16 collapsepoint[4];
#else
    mdf collapsepoint[3];
#endif
    mdf collapsecost;
    mdf value;
    mdf penalty;
    mmListNode list;
} mdOp;

#define MD_OP_FLAGS_DETACHED (0x1)
#define MD_OP_FLAGS_DELETION_PENDING (0x2)
#define MD_OP_FLAGS_UPDATE_QUEUED (0x4)
#define MD_OP_FLAGS_UPDATE_NEEDED (0x8)
#define MD_OP_FLAGS_DELETED (0x10)


/* If threadcount exceeds this number, updatebuffers will be shared by nearby cores */
#define MD_THREAD_UPDATE_BUFFER_COUNTMAX (8)

typedef struct RF_ALIGN64 {
    int threadid;

    /* Memory block for ops */
    mmBlockHead opblock;

    /* Hierarchical bucket sort of ops */
    void *binsort;

    /* List of ops flagged by other threads in need of update */
    mdUpdateBuffer updatebuffer[MD_THREAD_UPDATE_BUFFER_COUNTMAX];

    /* Per-thread status trackers */
    volatile long statusbuildtricount;
    volatile long statusbuildrefcount;
    volatile long statuspopulatecount;
    volatile long statusdeletioncount;
} mdThreadData;


static void mdUpdateBufferInit(mdUpdateBuffer *updatebuffer, int opalloc)
{
    updatebuffer->opbuffer = (void **)bu_malloc(opalloc * sizeof(mdOp *), "opbuffer");
    updatebuffer->opcount = 0;
    updatebuffer->opalloc = opalloc;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWrite32(&updatebuffer->atomlock, 0x0);
#else
    mtSpinInit(&updatebuffer->spinlock);
#endif
}

static void mdUpdateBufferEnd(mdUpdateBuffer *updatebuffer)
{
#ifndef MM_ATOMIC_SUPPORT
    mtSpinDestroy(&updatebuffer->spinlock);
#endif
    bu_free(updatebuffer->opbuffer, "opbuffer");
}

static void mdUpdateBufferAdd(mdUpdateBuffer *updatebuffer, mdOp *op, int orflags)
{
    int32_t flags;

#ifdef MM_ATOMIC_SUPPORT

    for (;;) {
	flags = mmAtomicRead32(&op->flags);

	if (flags & MD_OP_FLAGS_UPDATE_QUEUED) {
	    if (!(flags & MD_OP_FLAGS_UPDATE_NEEDED) || (orflags))
		mmAtomicOr32(&op->flags, orflags | MD_OP_FLAGS_UPDATE_NEEDED);

	    return;
	}

	if (mmAtomicCmpReplace32(&op->flags, flags, flags | orflags | MD_OP_FLAGS_UPDATE_QUEUED | MD_OP_FLAGS_UPDATE_NEEDED))
	    break;
    }

    /* TODO: Avoid spin lock, use atomic increment for write offset? Careful with this realloc, pointer could become invalid */
    mmAtomicSpin32(&updatebuffer->atomlock, 0x0, 0x1);

    if (updatebuffer->opcount >= updatebuffer->opalloc) {
	updatebuffer->opalloc <<= 1;
	updatebuffer->opbuffer = (void **)bu_realloc(updatebuffer->opbuffer, updatebuffer->opalloc * sizeof(mdOp *), "opbuffer");
    }

    updatebuffer->opbuffer[updatebuffer->opcount++] = op;
    mmAtomicWrite32(&updatebuffer->atomlock, 0x0);

#else

    mtSpinLock(&op->spinlock);
    op->flags |= orflags;
    flags = op->flags;

    if (flags & MD_OP_FLAGS_UPDATE_QUEUED) {
	if (!(flags & MD_OP_FLAGS_UPDATE_NEEDED))
	    op->flags |= MD_OP_FLAGS_UPDATE_NEEDED;

	mtSpinUnlock(&op->spinlock);
	return;
    }

    op->flags |= MD_OP_FLAGS_UPDATE_QUEUED | MD_OP_FLAGS_UPDATE_NEEDED;
    mtSpinUnlock(&op->spinlock);
    mtSpinLock(&updatebuffer->spinlock);

    if (updatebuffer->opcount >= updatebuffer->opalloc) {
	updatebuffer->opalloc <<= 1;
	updatebuffer->opbuffer = (void **)bu_realloc(updatebuffer->opbuffer, updatebuffer->opalloc * sizeof(mdOp *), "opbuffer");
    }

    updatebuffer->opbuffer[updatebuffer->opcount++] = op;
    mtSpinUnlock(&updatebuffer->spinlock);

#endif
}


/****/


#define MD_COMPACTNESS_NORMALIZATION_FACTOR (0.5*4.0*1.732050808)

static mdf mdEdgeCollapsePenaltyTriangle(mdf *newpoint, mdf *oldpoint, mdf *leftpoint, mdf *rightpoint, int *denyflag, mdf compactnesstarget)
{
    mdf penalty, compactness, oldcompactness, newcompactness, vecta2, norm;
    mdf vecta[3], oldvectb[3], oldvectc[3], newvectb[3], newvectc[3], oldnormal[3], newnormal[3];

    /* Normal of old triangle */
    VSUB2(vecta, rightpoint, leftpoint);
    VSUB2(oldvectb, oldpoint, leftpoint);
    VCROSS(oldnormal, vecta, oldvectb);

    /* Normal of new triangle */
    VSUB2(newvectb, newpoint, leftpoint);
    VCROSS(newnormal, vecta, newvectb);

    /* Detect normal inversion */
    if (VDOT(oldnormal, newnormal) < 0.0) {
	*denyflag = 1;
	return 0.0;
    }

    /* Penalize long thin triangles */
    penalty = 0.0;
    vecta2 = VDOT(vecta, vecta);
    VSUB2(newvectc, newpoint, rightpoint);
    newcompactness = MD_COMPACTNESS_NORMALIZATION_FACTOR * mdfsqrt(VDOT(newnormal, newnormal));
    norm = vecta2 + VDOT(newvectb, newvectb) + VDOT(newvectc, newvectc);

    if (newcompactness < (compactnesstarget * norm)) {
	newcompactness /= norm;
	VSUB2(oldvectc, oldpoint, rightpoint);
	oldcompactness = (MD_COMPACTNESS_NORMALIZATION_FACTOR * mdfsqrt(VDOT(oldnormal, oldnormal))) / (vecta2 + VDOT(oldvectb, oldvectb) + VDOT(oldvectc, oldvectc));
	compactness = mdfmin(compactnesstarget, oldcompactness) - newcompactness;

	if (compactness > 0.0)
	    penalty = compactness;
    }

    return penalty;
}


static mdf mdEdgeCollapsePenaltyAll(mdMesh *mesh, mdThreadData *UNUSED(tdata), mdi *trireflist, mdi trirefcount, mdi pivotindex, mdi skipindex, mdf *collapsepoint, int *denyflag)
{
    int vindex;
    mdf penalty;
    mdi triindex;
    mdTriangle *tri;
    mdf(*collapsepenalty)(mdf * newpoint, mdf * oldpoint, mdf * leftpoint, mdf * rightpoint, int *denyflag, mdf compactnesstarget);

    collapsepenalty = mesh->collapsepenalty;
    penalty = 0.0;

    for (vindex = 0; vindex < trirefcount; vindex++) {
	if (*denyflag)
	    break;

	triindex = trireflist[vindex];
	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	if (tri->v[0] == pivotindex) {
	    if ((tri->v[1] == skipindex) || (tri->v[2] == skipindex))
		continue;

	    penalty += collapsepenalty(collapsepoint, mesh->vertexlist[tri->v[0]].point, mesh->vertexlist[tri->v[2]].point, mesh->vertexlist[tri->v[1]].point, denyflag, mesh->compactnesstarget);
	} else if (tri->v[1] == pivotindex) {
	    if ((tri->v[2] == skipindex) || (tri->v[0] == skipindex))
		continue;

	    penalty += collapsepenalty(collapsepoint, mesh->vertexlist[tri->v[1]].point, mesh->vertexlist[tri->v[0]].point, mesh->vertexlist[tri->v[2]].point, denyflag, mesh->compactnesstarget);
	} else if (tri->v[2] == pivotindex) {
	    if ((tri->v[0] == skipindex) || (tri->v[1] == skipindex))
		continue;

	    penalty += collapsepenalty(collapsepoint, mesh->vertexlist[tri->v[2]].point, mesh->vertexlist[tri->v[1]].point, mesh->vertexlist[tri->v[0]].point, denyflag, mesh->compactnesstarget);
	} else {
	    bu_bomb("SHOULD NOT HAPPEN");
	}
    }

    penalty *= mesh->compactnesspenalty;

    return penalty;
}


static mdf mdEdgeCollapsePenalty(mdMesh *mesh, mdThreadData *tdata, mdi v0, mdi v1, mdf *collapsepoint, int *denyflag)
{
    mdf penalty, collapsearea, penaltyfactor;
    mdVertex *vertex0, *vertex1;

    vertex0 = &mesh->vertexlist[v0];
    vertex1 = &mesh->vertexlist[v1];

    collapsearea = vertex0->quadric.area + vertex1->quadric.area;
    penaltyfactor = collapsearea * collapsearea / (vertex0->trirefcount + vertex1->trirefcount);

    *denyflag = 0;
    penalty  = mdEdgeCollapsePenaltyAll(mesh, tdata, &mesh->trireflist[vertex0->trirefbase], vertex0->trirefcount, v0, v1, collapsepoint, denyflag);
    penalty += mdEdgeCollapsePenaltyAll(mesh, tdata, &mesh->trireflist[vertex1->trirefbase], vertex1->trirefcount, v1, v0, collapsepoint, denyflag);

    penalty *= penaltyfactor;

    return penalty;
}


/****/


static void mdMeshEdgeOpCallback(void *opaque, void *entry, int UNUSED(newflag))
{
    mdEdge *edge = (mdEdge *)entry;
    edge->op = opaque;
}


static double mdMeshOpValueCallback(void *item)
{
    mdOp *op = (mdOp *)item;
    return (double)op->collapsecost;
}

static void mdMeshAddOp(mdMesh *mesh, mdThreadData *tdata, mdi v0, mdi v1)
{
    int denyflag, opflags;
    mdOp *op;
    mdEdge edge;

    op = (mdOp *)mmBlockAlloc(&tdata->opblock);
    opflags = 0x0;
    op->updatebuffer = tdata->updatebuffer;
    op->v0 = v0;
    op->v1 = v1;
    op->value = mdEdgeSolvePoint(&mesh->vertexlist[v0], &mesh->vertexlist[v1], op->collapsepoint);
#ifdef MD_CONFIG_SSE_SUPPORT
    op->collapsepoint[3] = 0.0;
#endif
    op->penalty = mdEdgeCollapsePenalty(mesh, tdata, v0, v1, op->collapsepoint, &denyflag);
    op->collapsecost = op->value + op->penalty;

    if ((denyflag) || (op->collapsecost > mesh->maxcollapsecost))
	opflags |= MD_OP_FLAGS_DETACHED;
    else
	mmBinSortAdd(tdata->binsort, op, op->collapsecost);

#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWrite32(&op->flags, opflags);
#else
    op->flags = opflags;
    mtSpinInit(&op->spinlock);
#endif

    memset(&edge, 0, sizeof(mdEdge));    /*DRH added so it's initialized*/
    edge.v[0] = v0;
    edge.v[1] = v1;

    if (mmHashLockCallEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, mdMeshEdgeOpCallback, op, 0) != MM_HASH_SUCCESS)
	bu_bomb("SHOULD NOT HAPPEN");
}

static void mdMeshPopulateOpList(mdMesh *mesh, mdThreadData *tdata, mdi tribase, mdi tricount)
{
    mdTriangle *tri, *tristart, *triend;
    long populatecount;

    tristart = &mesh->trilist[tribase];
    triend = &tristart[tricount];

    populatecount = 0;

    for (tri = tristart; tri < triend; tri++) {
	if ((tri->v[0] < tri->v[1]) || (tri->u.edgeflags & 0x1))
	    mdMeshAddOp(mesh, tdata, tri->v[0], tri->v[1]);

	if ((tri->v[1] < tri->v[2]) || (tri->u.edgeflags & 0x2))
	    mdMeshAddOp(mesh, tdata, tri->v[1], tri->v[2]);

	if ((tri->v[2] < tri->v[0]) || (tri->u.edgeflags & 0x4))
	    mdMeshAddOp(mesh, tdata, tri->v[2], tri->v[0]);

	populatecount++;
	tdata->statuspopulatecount = populatecount;
    }
}


/****/


/* Merge vertex attributes of v0 and v1, write to v0 */
static void mdEdgeCollapseAttrib(mdMesh *mesh, mdThreadData *UNUSED(tdata), mdi v0, mdi v1, mdf *collapsepoint)
{
    size_t vindex;
    mdVertex *vertex0, *vertex1;
    mdf dist[3], dist0, dist1, weightsum, weightsuminv;
    mdf weight0, weight1;
    mdOpAttrib *attrib, *attribend;
    void *attr0p, *attr1p;
    float *attr0pf, *attr1pf, sumf, suminvf;
    double *attr0pd, *attr1pd, sumd, suminvd;

    if (!(mesh->attribcount))
	return;

    vertex0 = &mesh->vertexlist[v0];
    VSUB2(dist, collapsepoint, vertex0->point);
    dist0 = MAGNITUDE(dist);

    vertex1 = &mesh->vertexlist[v1];
    VSUB2(dist, collapsepoint, vertex1->point);
    dist1 = MAGNITUDE(dist);

    weight0 = dist1 * vertex0->quadric.area;
    weight1 = dist0 * vertex1->quadric.area;
    weightsum = weight0 + weight1;

    if (!ZERO(weightsum)) {
	weightsuminv = 1.0 / weightsum;
	weight0 *= weightsuminv;
	weight1 *= weightsuminv;
    } else {
	weight0 = 0.5;
	weight1 = 0.5;
    }

    attrib = mesh->attrib;
    attribend = &attrib[mesh->attribcount];

    for (; attrib < attribend; attrib++) {
	attr0p = ADDRESS(attrib->base, v0 * attrib->stride);
	attr1p = ADDRESS(attrib->base, v1 * attrib->stride);

	if (attrib->width == sizeof(float)) {
	    attr0pf = (float *)attr0p;
	    attr1pf = (float *)attr1p;

	    for (vindex = 0; vindex < attrib->count; vindex++)
		attr0pf[vindex] = (attr0pf[vindex] * (float)weight0) + (attr1pf[vindex] * (float)weight1);

	    if (attrib->flags & MD_ATTRIB_FLAGS_NORMALIZE) {
		sumf = 0.0;

		for (vindex = 0; vindex < attrib->count; vindex++)
		    sumf += attr0pf[vindex] * attr0pf[vindex];

		if (!ZERO(sumf)) {
		    suminvf = 1.0 / sumf;

		    for (vindex = 0; vindex < attrib->count; vindex++)
			attr0pf[vindex] *= suminvf;
		}
	    }
	} else if (attrib->width == sizeof(double)) {
	    attr0pd = (double *)attr0p;
	    attr1pd = (double *)attr1p;

	    for (vindex = 0; vindex < attrib->count; vindex++)
		attr0pd[vindex] = (attr0pd[vindex] * (double)weight0) + (attr1pd[vindex] * (double)weight1);

	    if (attrib->flags & MD_ATTRIB_FLAGS_NORMALIZE) {
		sumd = 0.0;

		for (vindex = 0; vindex < attrib->count; vindex++)
		    sumd += attr0pd[vindex] * attr0pd[vindex];

		if (!ZERO(sumd)) {
		    suminvd = 1.0 / sumd;

		    for (vindex = 0; vindex < attrib->count; vindex++)
			attr0pd[vindex] *= suminvd;
		}
	    }
	}
    }
}


/* Delete triangle and return outer vertex */
static mdi mdEdgeCollapseDeleteTriangle(mdMesh *mesh, mdThreadData *tdata, mdi v0, mdi v1, int *retdelflags)
{
    int delflags = 0;
    mdi outer = 0;
    mdEdge edge;
    mdTriangle *tri;
    mdOp *op;

    memset(&edge, 0, sizeof(mdEdge));    /*DRH added so it's initialized*/
    *retdelflags = 0x0;

    edge.v[0] = v0;
    edge.v[1] = v1;

    if (mmHashLockDeleteEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	return -1;

    op = (mdOp *)edge.op;

    if (op)
	mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, MD_OP_FLAGS_DELETION_PENDING);

    tri = &mesh->trilist[edge.triindex];

    if (tri->v[0] != v0) {
	edge.v[0] = tri->v[0];
	edge.v[1] = tri->v[1];

	if (mmHashLockDeleteEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");

	op = (mdOp *)edge.op;

	if (op)
	    mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, MD_OP_FLAGS_DELETION_PENDING);
    }

    if (tri->v[1] != v0) {
	edge.v[0] = tri->v[1];
	edge.v[1] = tri->v[2];

	if (mmHashLockDeleteEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");

	op = (mdOp *)edge.op;

	if (op)
	    mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, MD_OP_FLAGS_DELETION_PENDING);
    }

    if (tri->v[2] != v0) {
	edge.v[0] = tri->v[2];
	edge.v[1] = tri->v[0];

	if (mmHashLockDeleteEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");

	op = (mdOp *)edge.op;

	if (op)
	    mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, MD_OP_FLAGS_DELETION_PENDING);
    }

    /* Determine outer vertex */
    /* TODO: Replace branches with bitwise arithmetics */
    if (tri->v[0] == v0) {
	outer = tri->v[2];
	delflags = 0x0;

	if (tri->u.edgeflags & 0x2)
	    delflags |= 0x1;

	if (tri->u.edgeflags & 0x4)
	    delflags |= 0x2;
    } else if (tri->v[1] == v0) {
	outer = tri->v[0];
	delflags = 0x0;

	if (tri->u.edgeflags & 0x4)
	    delflags |= 0x1;

	if (tri->u.edgeflags & 0x1)
	    delflags |= 0x2;
    } else if (tri->v[2] == v0) {
	outer = tri->v[1];
	delflags = 0x0;

	if (tri->u.edgeflags & 0x1)
	    delflags |= 0x1;

	if (tri->u.edgeflags & 0x2)
	    delflags |= 0x2;
    } else
	bu_bomb("SHOULD NOT HAPPEN");

    *retdelflags = delflags;

    /* Invalidate triangle */
    tri->v[0] = -1;

    return outer;
}


static void mdEdgeCollapseUpdateTriangle(mdMesh *mesh, mdThreadData *tdata, mdTriangle *tri, mdi newv, int pivot, int left, int right)
{
    mdEdge edge;
    mdOp *op;

    memset(&edge, 0, sizeof(mdEdge));    /*DRH added so it's initialized*/

    edge.v[0] = tri->v[pivot];
    edge.v[1] = tri->v[right];

    if (edge.v[0] == newv) {
	if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");
    } else {
	if (mmHashLockDeleteEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");

	edge.v[0] = newv;

	if (mmHashLockAddEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");
    }

    op = (mdOp *)edge.op;

    if (op) {
#ifdef MD_CONFIG_DELAYED_OP_REDIRECT
	op->value = mdEdgeSolvePoint(&mesh->vertexlist[edge.v[0]], &mesh->vertexlist[edge.v[1]], op->collapsepoint);
#ifdef MD_CONFIG_SSE_SUPPORT
	op->collapsepoint[3] = 0.0;
#endif
	mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, 0x0);
#else
	op->v0 = newv;
	op->value = mdEdgeSolvePoint(&mesh->vertexlist[op->v0], &mesh->vertexlist[op->v1], op->collapsepoint);
#ifdef MD_CONFIG_SSE_SUPPORT
	op->collapsepoint[3] = 0.0;
#endif
	mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, 0x0);
#endif
    }

    edge.v[0] = tri->v[left];
    edge.v[1] = tri->v[pivot];

    if (edge.v[1] == newv) {
	if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");
    } else {
	if (mmHashLockDeleteEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");

	edge.v[1] = newv;

	if (mmHashLockAddEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
	    bu_bomb("SHOULD NOT HAPPEN");
    }

    op = (mdOp *)edge.op;

    if (op) {
#ifdef MD_CONFIG_DELAYED_OP_REDIRECT
	op->value = mdEdgeSolvePoint(&mesh->vertexlist[edge.v[0]], &mesh->vertexlist[edge.v[1]], op->collapsepoint);
#ifdef MD_CONFIG_SSE_SUPPORT
	op->collapsepoint[3] = 0.0;
#endif
	mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, 0x0);
#else
	op->v1 = newv;
	op->value = mdEdgeSolvePoint(&mesh->vertexlist[op->v0], &mesh->vertexlist[op->v1], op->collapsepoint);
#ifdef MD_CONFIG_SSE_SUPPORT
	op->collapsepoint[3] = 0.0;
#endif
	mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, 0x0);
#endif
    }

    tri->v[pivot] = newv;
}


/*
Walk through the list of all triangles attached to a vertex
- Update cost of collapse for other edges of triangles
- Build up the updated list of triangle references for new vertex
*/
static mdi *mdEdgeCollapseUpdateAll(mdMesh *mesh, mdThreadData *tdata, mdi *trireflist, mdi trirefcount, mdi oldv, mdi newv, mdi *trirefstore)
{
    int vindex;
    mdi triindex;
    mdTriangle *tri;

    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];
	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	*trirefstore = triindex;
	trirefstore++;

	if (tri->v[0] == oldv)
	    mdEdgeCollapseUpdateTriangle(mesh, tdata, tri, newv, 0, 2, 1);
	else if (tri->v[1] == oldv)
	    mdEdgeCollapseUpdateTriangle(mesh, tdata, tri, newv, 1, 0, 2);
	else if (tri->v[2] == oldv)
	    mdEdgeCollapseUpdateTriangle(mesh, tdata, tri, newv, 2, 1, 0);
	else
	    bu_bomb("SHOULD NOT HAPPEN");
    }

    return trirefstore;
}


static void mdVertexInvalidateTri(mdMesh *mesh, mdThreadData *tdata, mdi v0, mdi v1)
{
    mdEdge edge;
    mdOp *op;

    memset(&edge, 0, sizeof(mdEdge));    /*DRH added so it's initialized*/
    edge.v[0] = v0;
    edge.v[1] = v1;

    if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) != MM_HASH_SUCCESS)
	bu_bomb("SHOULD NOT HAPPEN");

    op = (mdOp *)edge.op;

    if (!(op))
	return;

    mdUpdateBufferAdd(&op->updatebuffer[tdata->threadid >> mesh->updatebuffershift], op, 0x0);
}

static void mdVertexInvalidate(mdMesh *mesh, mdThreadData *tdata, mdi vertexindex)
{
    mdi vindex, triindex, trirefcount;
    mdi *trireflist;
    mdTriangle *tri;
    mdVertex *vertex;

    /* Vertices of the collapsed edge */
    vertex = &mesh->vertexlist[vertexindex];
    trireflist = &mesh->trireflist[vertex->trirefbase];
    trirefcount = vertex->trirefcount;

    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];
	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	if (tri->v[0] == vertexindex) {
	    mdVertexInvalidateTri(mesh, tdata, tri->v[0], tri->v[1]);
	    mdVertexInvalidateTri(mesh, tdata, tri->v[2], tri->v[0]);
	} else if (tri->v[1] == vertexindex) {
	    mdVertexInvalidateTri(mesh, tdata, tri->v[1], tri->v[2]);
	    mdVertexInvalidateTri(mesh, tdata, tri->v[0], tri->v[1]);
	} else if (tri->v[2] == vertexindex) {
	    mdVertexInvalidateTri(mesh, tdata, tri->v[2], tri->v[0]);
	    mdVertexInvalidateTri(mesh, tdata, tri->v[1], tri->v[2]);
	} else
	    bu_bomb("SHOULD NOT HAPPEN");
    }
}

static void mdVertexInvalidateAll(mdMesh *mesh, mdThreadData *tdata, mdi *trireflist, mdi trirefcount, mdi pivotindex)
{
    int vindex;
    mdi triindex;
    mdTriangle *tri;

    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];
	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	if (tri->v[0] == pivotindex) {
	    mdVertexInvalidate(mesh, tdata, tri->v[1]);
	    mdVertexInvalidate(mesh, tdata, tri->v[2]);
	} else if (tri->v[1] == pivotindex) {
	    mdVertexInvalidate(mesh, tdata, tri->v[2]);
	    mdVertexInvalidate(mesh, tdata, tri->v[0]);
	} else if (tri->v[2] == pivotindex) {
	    mdVertexInvalidate(mesh, tdata, tri->v[0]);
	    mdVertexInvalidate(mesh, tdata, tri->v[1]);
	} else
	    bu_bomb("SHOULD NOT HAPPEN");
    }
}


static void mdEdgeCollapseLinkOuter(mdMesh *mesh, mdThreadData *tdata, mdi newv, mdi outer)
{
    int sideflags;
    mdEdge edge;
    mdOp *op;

    if (outer == -1)
	return;

    sideflags = 0x0;

    edge.v[0] = newv;
    edge.v[1] = outer;

    if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) == MM_HASH_SUCCESS) {
	sideflags |= 0x1;
	op = (mdOp *)edge.op;

	if (op)
	    return;
    }

    edge.v[0] = outer;
    edge.v[1] = newv;

    if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) == MM_HASH_SUCCESS) {
	sideflags |= 0x2;
	op = (mdOp *)edge.op;

	if (op)
	    return;
    }

    if ((newv < outer) && (sideflags & 0x1))
	mdMeshAddOp(mesh, tdata, newv, outer);
    else if (sideflags & 0x2)
	mdMeshAddOp(mesh, tdata, outer, newv);
}


static void mdEdgeCollapsePropagateBoundary(mdMesh *mesh, mdi v0, mdi v1)
{
    mdEdge edge;
    mdTriangle *tri;

    memset(&edge, 0, sizeof(mdEdge));    /*DRH added so it's initialized*/
    edge.v[0] = v1;
    edge.v[1] = v0;

    if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) == MM_HASH_SUCCESS)
	return;

    edge.v[0] = v0;
    edge.v[1] = v1;

    if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) != MM_HASH_SUCCESS)
	return;

    tri = &mesh->trilist[edge.triindex];

    if (tri->v[0] == v0)
	tri->u.edgeflags |= 0x1;
    else if (tri->v[1] == v0)
	tri->u.edgeflags |= 0x2;
    else if (tri->v[2] == v0)
	tri->u.edgeflags |= 0x4;
    else
	bu_bomb("SHOULD NOT HAPPEN");

    /* TODO: Grow the boundary for affected vertex quadrics */
}


#define MD_LOCK_BUFFER_STATIC (512)

typedef struct {
    mdi vertexstatic[MD_LOCK_BUFFER_STATIC];
    mdi *vertexlist;
    int vertexcount;
    int vertexalloc;
} mdLockBuffer;

static inline void mdLockBufferInit(mdLockBuffer *buffer, int maxvertexcount)
{
    buffer->vertexlist = buffer->vertexstatic;
    buffer->vertexalloc = MD_LOCK_BUFFER_STATIC;

    if (maxvertexcount > MD_LOCK_BUFFER_STATIC) {
	buffer->vertexlist = (mdi *)bu_malloc(maxvertexcount * sizeof(mdi), "vertexlist");
	buffer->vertexalloc = maxvertexcount;
    }

    buffer->vertexcount = 0;
}

static inline void mdLockBufferResize(mdLockBuffer *buffer, int maxvertexcount)
{
    int vindex;
    mdi *vertexlist;

    if (maxvertexcount > buffer->vertexalloc) {
	vertexlist = (mdi *)bu_malloc(maxvertexcount * sizeof(mdi), "vertexlist");

	for (vindex = 0; vindex < buffer->vertexcount; vindex++)
	    vertexlist[vindex] = buffer->vertexlist[vindex];

	if (buffer->vertexlist != buffer->vertexstatic)
	    bu_free(buffer->vertexlist, "vertexlist");

	buffer->vertexlist = vertexlist;
	buffer->vertexalloc = maxvertexcount;
    }
}

static inline void mdLockBufferEnd(mdLockBuffer *buffer)
{
    if (buffer->vertexlist != buffer->vertexstatic)
	bu_free(buffer->vertexlist, "vertexlist");

    buffer->vertexlist = buffer->vertexstatic;
    buffer->vertexalloc = MD_LOCK_BUFFER_STATIC;
    buffer->vertexcount = 0;
}

void mdLockBufferUnlockAll(mdMesh *mesh, mdThreadData *tdata, mdLockBuffer *buffer)
{
    int vindex;
    mdVertex *vertex;

    for (vindex = 0; vindex < buffer->vertexcount; vindex++) {
	vertex = &mesh->vertexlist[buffer->vertexlist[vindex]];
#ifdef MM_ATOMIC_SUPPORT

	if (mmAtomicRead32(&vertex->atomicowner) == tdata->threadid)
	    mmAtomicCmpXchg32(&vertex->atomicowner, tdata->threadid, -1);

#else
	mtSpinLock(&vertex->ownerspinlock);

	if (vertex->owner == tdata->threadid)
	    vertex->owner = -1;

	mtSpinUnlock(&vertex->ownerspinlock);
#endif
    }

    buffer->vertexcount = 0;
}

/* If it fails, release all locks */
int mdLockBufferTryLock(mdMesh *mesh, mdThreadData *tdata, mdLockBuffer *buffer, mdi vertexindex)
{
    int32_t owner;
    mdVertex *vertex;
    vertex = &mesh->vertexlist[vertexindex];

#ifdef MM_ATOMIC_SUPPORT
    owner = mmAtomicRead32(&vertex->atomicowner);

    if (owner == tdata->threadid)
	return 1;

    if ((owner != -1) || !(mmAtomicCmpReplace32(&vertex->atomicowner, -1, tdata->threadid))) {
	mdLockBufferUnlockAll(mesh, tdata, buffer);
	return 0;
    }

#else
    mtSpinLock(&vertex->ownerspinlock);
    owner = vertex->owner;

    if (owner == tdata->threadid) {
	mtSpinUnlock(&vertex->ownerspinlock);
	return 1;
    }

    if (owner != -1) {
	mtSpinUnlock(&vertex->ownerspinlock);
	mdLockBufferUnlockAll(mesh, tdata, buffer);
	return 0;
    }

    vertex->owner = tdata->threadid;
    mtSpinUnlock(&vertex->ownerspinlock);
#endif

    buffer->vertexlist[buffer->vertexcount++] = vertexindex;
    return 1;
}

/* If it fails, release all locks then wait for the desired lock to become available */
int mdLockBufferLock(mdMesh *mesh, mdThreadData *tdata, mdLockBuffer *buffer, mdi vertexindex)
{
    int32_t owner;
    mdVertex *vertex;
    vertex = &mesh->vertexlist[vertexindex];

#ifdef MM_ATOMIC_SUPPORT
    owner = mmAtomicRead32(&vertex->atomicowner);

    if (owner == tdata->threadid)
	return 1;

    if ((owner == -1) && mmAtomicCmpReplace32(&vertex->atomicowner, -1, tdata->threadid)) {
	buffer->vertexlist[buffer->vertexcount++] = vertexindex;
	return 1;
    }

    /* Lock failed, release all locks and wait until we get the lock we got stuck on */
    mdLockBufferUnlockAll(mesh, tdata, buffer);
    mmAtomicSpinWaitEq32(&vertex->atomicowner, -1);
#else
    mtSpinLock(&vertex->ownerspinlock);
    owner = vertex->owner;

    if (owner == tdata->threadid) {
	mtSpinUnlock(&vertex->ownerspinlock);
	return 1;
    }

    if (owner == -1) {
	vertex->owner = tdata->threadid;
	mtSpinUnlock(&vertex->ownerspinlock);
	buffer->vertexlist[buffer->vertexcount++] = vertexindex;
	return 1;
    }

    mtSpinUnlock(&vertex->ownerspinlock);
    /* Lock failed, release all locks */
    mdLockBufferUnlockAll(mesh, tdata, buffer);
#endif

    /* Alternatively, we could acquire that lock that bugged us */
    /*
      for(;; )
      {
    mmAtomicPause();
    owner = mmAtomicRead32( &vertex->atomicowner );
    if( owner != -1 )
      continue;
    if( mmAtomicCmpReplace32( &vertex->atomicowner, -1, tdata->threadid ) )
      break;
      }

      buffer->vertexlist[buffer->vertexcount++] = vertexindex;
    */

    return 0;
}


static int mdPivotLockRefs(mdMesh *mesh, mdThreadData *tdata, mdLockBuffer *buffer, mdi vertexindex)
{
    int vindex;
    mdi triindex, iav = 0, ibv = 0, trirefcount;
    mdi *trireflist;
    mdTriangle *tri;
    mdVertex *vertex;

    vertex = &mesh->vertexlist[vertexindex];
    trireflist = &mesh->trireflist[vertex->trirefbase];
    trirefcount = vertex->trirefcount;

    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];
	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	if (tri->v[0] == vertexindex) {
	    iav = tri->v[1];
	    ibv = tri->v[2];
	} else if (tri->v[1] == vertexindex) {
	    iav = tri->v[2];
	    ibv = tri->v[0];
	} else if (tri->v[2] == vertexindex) {
	    iav = tri->v[0];
	    ibv = tri->v[1];
	} else
	    bu_bomb("SHOULD NOT HAPPEN");

	if (!(mdLockBufferLock(mesh, tdata, buffer, iav)))
	    return 0;

	if (!(mdLockBufferLock(mesh, tdata, buffer, ibv)))
	    return 0;
    }

    return 1;
}


static void mdOpResolveLockEdge(mdMesh *mesh, mdThreadData *tdata, mdLockBuffer *lockbuffer, mdOp *op)
{
    int failcount, globalflag;
    mdVertex *vertex0, *vertex1;

    failcount = 0;
    globalflag = 0;

    for (;;) {
	if ((failcount > MD_GLOBAL_LOCK_THRESHOLD) && !(globalflag)) {
	    mdLockBufferUnlockAll(mesh, tdata, lockbuffer);
#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicSpin32(&mesh->globalvertexlock, 0x0, 0x1);
#else
	    mtSpinLock(&mesh->globalvertexspinlock);
#endif
	    globalflag = 1;
	}

	if (!(mdLockBufferLock(mesh, tdata, lockbuffer, op->v0)) || !(mdLockBufferLock(mesh, tdata, lockbuffer, op->v1))) {
	    failcount++;
	    continue;
	}

	vertex0 = &mesh->vertexlist[op->v0];
	vertex1 = &mesh->vertexlist[op->v1];
#ifdef MD_CONFIG_DELAYED_OP_REDIRECT

	if (vertex0->redirectindex != -1)
	    op->v0 = vertex0->redirectindex;
	else if (vertex1->redirectindex != -1)
	    op->v1 = vertex1->redirectindex;
	else
	    break;

#else
	break;
#endif
    }

#ifdef MM_ATOMIC_SUPPORT

    if (globalflag)
	mmAtomicWrite32(&mesh->globalvertexlock, 0x0);

#else

    if (globalflag)
	mtSpinUnlock(&mesh->globalvertexspinlock);

#endif
}

static int mdOpResolveLockEdgeTry(mdMesh *mesh, mdThreadData *tdata, mdLockBuffer *lockbuffer, mdOp *op)
{
    mdVertex *vertex0, *vertex1;

    for (;;) {
	if (!(mdLockBufferTryLock(mesh, tdata, lockbuffer, op->v0)) || !(mdLockBufferTryLock(mesh, tdata, lockbuffer, op->v1)))
	    return 0;

	vertex0 = &mesh->vertexlist[op->v0];
	vertex1 = &mesh->vertexlist[op->v1];
#ifdef MD_CONFIG_DELAYED_OP_REDIRECT

	if (vertex0->redirectindex != -1)
	    op->v0 = vertex0->redirectindex;
	else if (vertex1->redirectindex != -1)
	    op->v1 = vertex1->redirectindex;
	else
	    break;

#else
	break;
#endif
    }

    return 1;
}

static void mdOpResolveLockFull(mdMesh *mesh, mdThreadData *tdata, mdLockBuffer *lockbuffer, mdOp *op)
{
    int failcount, globalflag;
    mdVertex *vertex0, *vertex1;

    failcount = 0;
    globalflag = 0;

    for (;;) {
	if ((failcount > MD_GLOBAL_LOCK_THRESHOLD) && !(globalflag)) {
	    mdLockBufferUnlockAll(mesh, tdata, lockbuffer);
#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicSpin32(&mesh->globalvertexlock, 0x0, 0x1);
#else
	    mtSpinLock(&mesh->globalvertexspinlock);
#endif
	    globalflag = 1;
	}

	if (!(mdLockBufferLock(mesh, tdata, lockbuffer, op->v0)) || !(mdLockBufferLock(mesh, tdata, lockbuffer, op->v1))) {
	    failcount++;
	    continue;
	}

	vertex0 = &mesh->vertexlist[op->v0];
	vertex1 = &mesh->vertexlist[op->v1];
#ifdef MD_CONFIG_DELAYED_OP_REDIRECT

	if (vertex0->redirectindex != -1)
	    op->v0 = vertex0->redirectindex;
	else if (vertex1->redirectindex != -1)
	    op->v1 = vertex1->redirectindex;
	else {
	    mdLockBufferResize(lockbuffer, 2 + ((vertex0->trirefcount + vertex1->trirefcount) << 1));

	    if (!(mdPivotLockRefs(mesh, tdata, lockbuffer, op->v0)) || !(mdPivotLockRefs(mesh, tdata, lockbuffer, op->v1))) {
		failcount++;
		continue;
	    }

	    break;
	}

#else
	mdLockBufferResize(lockbuffer, 2 + ((vertex0->trirefcount + vertex1->trirefcount) << 1));

	if (!(mdPivotLockRefs(mesh, tdata, lockbuffer, op->v0)) || !(mdPivotLockRefs(mesh, tdata, lockbuffer, op->v1))) {
	    failcount++;
	    continue;
	}

	break;
#endif
    }

#ifdef MM_ATOMIC_SUPPORT

    if (globalflag)
	mmAtomicWrite32(&mesh->globalvertexlock, 0x0);

#else

    if (globalflag)
	mtSpinUnlock(&mesh->globalvertexspinlock);

#endif
}


#define MD_EDGE_COLLAPSE_TRIREF_STATIC (512)

static void mdEdgeCollapse(mdMesh *mesh, mdThreadData *tdata, mdi v0, mdi v1, mdf *collapsepoint)
{
    int vindex, delflags0, delflags1;
    long deletioncount;
    mdi newv, trirefcount, trirefmax, outer0, outer1;
    mdi *trireflist, *trirefstore;
    mdi trirefstatic[MD_EDGE_COLLAPSE_TRIREF_STATIC];
    mdVertex *vertex0, *vertex1;

    /* Vertices of the collapsed edge */
    vertex0 = &mesh->vertexlist[v0];
    vertex1 = &mesh->vertexlist[v1];

    /* Collapse other custom vertex attributes */
    mdEdgeCollapseAttrib(mesh, tdata, v0, v1, collapsepoint);

    /* New vertex overwriting v0 */
    newv = v0;

    /* Delete the triangles on both sides of the edge and all associated edges */
    outer0 = mdEdgeCollapseDeleteTriangle(mesh, tdata, v0, v1, &delflags0);
    outer1 = mdEdgeCollapseDeleteTriangle(mesh, tdata, v1, v0, &delflags1);

    /* Track count of deletions */
    deletioncount = tdata->statusdeletioncount;

    if (outer0 != -1)
	deletioncount++;

    if (outer1 != -1)
	deletioncount++;

    tdata->statusdeletioncount = deletioncount;

    /* Set up new vertex over v0 */
    VMOVE(vertex0->point, collapsepoint);
    mathQuadricAddQuadric(&vertex0->quadric, &vertex1->quadric);

    /* Propagate boundaries from deleted triangles */
    if (delflags0) {
	if (delflags0 & 0x1)
	    mdEdgeCollapsePropagateBoundary(mesh, newv, outer0);

	if (delflags0 & 0x2)
	    mdEdgeCollapsePropagateBoundary(mesh, outer0, newv);
    }

    if (delflags1) {
	if (delflags1 & 0x1)
	    mdEdgeCollapsePropagateBoundary(mesh, newv, outer1);

	if (delflags1 & 0x2)
	    mdEdgeCollapsePropagateBoundary(mesh, outer1, newv);
    }

    /* Redirect vertex1 to vertex0 */
    vertex1->redirectindex = newv;

    /* Maximum theoretical count of triangle references for our new vertex, we need a chunk of memory that big */
    trirefmax = vertex0->trirefcount + vertex1->trirefcount;

    /* Buffer to temporarily store our new trirefs */
    trireflist = trirefstatic;

    if (trirefmax > MD_EDGE_COLLAPSE_TRIREF_STATIC)
	trireflist = (mdi *)bu_malloc(trirefmax * sizeof(mdi), "trireflist");

    /* Update all triangles connected to vertex0 and vertex1 */
    trirefstore = trireflist;
    trirefstore = mdEdgeCollapseUpdateAll(mesh, tdata, &mesh->trireflist[vertex0->trirefbase], vertex0->trirefcount, v0, newv, trirefstore);
    trirefstore = mdEdgeCollapseUpdateAll(mesh, tdata, &mesh->trireflist[vertex1->trirefbase], vertex1->trirefcount, v1, newv, trirefstore);

    /* Find where to store the trirefs */
    trirefcount = (int)(trirefstore - trireflist);

    if (trirefcount > vertex0->trirefcount) {
	if (trirefcount <= vertex1->trirefcount)
	    vertex0->trirefbase = vertex1->trirefbase;
	else {
	    /* Multithreading, acquire lock */
#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicSpin32(&mesh->trireflock, 0x0, 0x1);
#else
	    mtSpinLock(&mesh->trirefspinlock);
#endif
	    vertex0->trirefbase = mesh->trirefcount;
	    mesh->trirefcount += trirefcount;

	    if (mesh->trirefcount >= mesh->trirefalloc)
		bu_bomb("SHOULD NOT HAPPEN");

#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicWrite32(&mesh->trireflock, 0x0);
#else
	    mtSpinUnlock(&mesh->trirefspinlock);
#endif
	}
    }

    /* Mark vertex1 as unused */
    vertex1->trirefcount = 0;

    /* Store trirefs */
    vertex0->trirefcount = trirefcount;
    trirefstore = &mesh->trireflist[vertex0->trirefbase];

    for (vindex = 0; vindex < trirefcount; vindex++) {
	trirefstore[vindex] = trireflist[vindex];
    }

    /* Invalidate all penalty calculations in neighborhood of pivot vertex */
    mdVertexInvalidateAll(mesh, tdata, trireflist, trirefcount, newv);

    /* If buffer wasn't static, free it */
    if (trireflist != trirefstatic)
	bu_free(trireflist, "trireflist");

    /* Verify if we should create new ops between newv and outer vertices of deleted triangles */
    mdEdgeCollapseLinkOuter(mesh, tdata, newv, outer0);
    mdEdgeCollapseLinkOuter(mesh, tdata, newv, outer1);
}


/****/


typedef struct {
    int collisionflag;
    mdi trileft;
    mdi triright;
} mdEdgeCollisionData;

static void mdEdgeCollisionCallback(void *opaque, void *entry, int UNUSED(newflag))
{
    mdEdge *edge = (mdEdge *)entry;
    mdEdgeCollisionData *ecd = (mdEdgeCollisionData *)opaque;

    if ((edge->triindex != ecd->trileft) && (edge->triindex != ecd->triright))
	ecd->collisionflag = 1;
}


/*
Prevent 2D collapses

Check all triangles attached to v1 that would have to attach back to v0
If any of the edge is already present in the hash table, deny the collapse
*/
static int mdEdgeCollisionCheck(mdMesh *mesh, mdThreadData *UNUSED(tdata), mdi v0, mdi v1)
{
    int vindex, trirefcount, left, right;
    mdi triindex, vsrc, vdst;
    mdi *trireflist;
    mdEdge edge;
    mdTriangle *tri;
    mdVertex *vertex0, *vertex1;
    mdEdgeCollisionData ecd;

    memset(&edge, 0, sizeof(mdEdge));    /*DRH added so it's initialized*/
    vertex0 = &mesh->vertexlist[v0];
    vertex1 = &mesh->vertexlist[v1];

    if (vertex0->trirefcount < vertex1->trirefcount) {
	vsrc = v0;
	vdst = v1;
	trireflist = &mesh->trireflist[vertex0->trirefbase];
	trirefcount = vertex0->trirefcount;
    } else {
	vsrc = v1;
	vdst = v0;
	trireflist = &mesh->trireflist[vertex1->trirefbase];
	trirefcount = vertex1->trirefcount;
    }

    /* Find the triangles that would be deleted so that we don't detect false collisions with them */
    ecd.collisionflag = 0;
    ecd.trileft = -1;
    edge.v[0] = v0;
    edge.v[1] = v1;

    if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) == MM_HASH_SUCCESS)
	ecd.trileft = edge.triindex;

    ecd.triright = -1;
    edge.v[0] = v1;
    edge.v[1] = v0;

    if (mmHashLockReadEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge) == MM_HASH_SUCCESS)
	ecd.triright = edge.triindex;

    /* Check all trirefs for collision */
    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];

	if ((triindex == ecd.trileft) || (triindex == ecd.triright))
	    continue;

	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	if (tri->v[0] == vsrc) {
	    left = 2;
	    right = 1;
	} else if (tri->v[1] == vsrc) {
	    left = 0;
	    right = 2;
	} else if (tri->v[2] == vsrc) {
	    left = 1;
	    right = 0;
	} else {
	    bu_bomb("SHOULD NOT HAPPEN");
	    continue;
	}

	edge.v[0] = vdst;
	edge.v[1] = tri->v[right];
	mmHashLockCallEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, mdEdgeCollisionCallback, &ecd, 0);

	if (ecd.collisionflag)
	    return 0;

	edge.v[0] = tri->v[left];
	edge.v[1] = vdst;
	mmHashLockCallEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, mdEdgeCollisionCallback, &ecd, 0);

	if (ecd.collisionflag)
	    return 0;
    }

    return 1;
}


/****/


/* Mesh init step 0, allocate, NOT threaded */
static int mdMeshInit(mdMesh *mesh, size_t maxmemorysize)
{
    int retval;

    /* Allocate vertices, no extra room for vertices, we overwrite existing ones as we decimate */
    mesh->vertexlist = (mdVertex *)mmAlignAlloc(mesh->vertexalloc * sizeof(mdVertex), 0x40);

    /* Allocate space for per-vertex lists of face references, including future vertices */
    mesh->trirefcount = 0;
    mesh->trirefalloc = 2 * 6 * mesh->tricount;
    mesh->trireflist = (mdi *)bu_malloc(mesh->trirefalloc * sizeof(mdi), "trireflist");

    /* Allocate triangles */
    mesh->trilist = (mdTriangle *)bu_malloc(mesh->tricount * sizeof(mdTriangle), "trilist");

    /* Allocate edge hash table */
    retval = 1;

    if (!(mesh->operationflags & MD_FLAGS_NO_DECIMATION))
	retval = mdMeshHashInit(mesh, mesh->tricount, 2.0, 7, maxmemorysize);

    /* Custom vertex attributes besides point position */
    mesh->attribcount = 0;
    mesh->attrib = 0;

#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWrite32(&mesh->trireflock, 0x0);
    mmAtomicWrite32(&mesh->globalvertexlock, 0x0);
#else
    mtSpinInit(&mesh->trirefspinlock);
    mtSpinInit(&mesh->globalvertexspinlock);
#endif

    return retval;
}


/* Mesh init step 1, initialize vertices, threaded */
static void mdMeshInitVertices(mdMesh *mesh, mdThreadData *tdata, int threadcount)
{
    int vertexindex, vertexindexmax, vertexperthread;
    mdVertex *vertex;
    void *point;

    vertexperthread = (mesh->vertexcount / threadcount) + 1;
    vertexindex = tdata->threadid * vertexperthread;
    vertexindexmax = vertexindex + vertexperthread;

    if (vertexindexmax > mesh->vertexcount)
	vertexindexmax = mesh->vertexcount;

    vertex = &mesh->vertexlist[vertexindex];
    point = ADDRESS(mesh->point, vertexindex * mesh->pointstride);

    for (; vertexindex < vertexindexmax; vertexindex++, vertex++) {
#ifdef MM_ATOMIC_SUPPORT
	mmAtomicWrite32(&vertex->atomicowner, -1);
#else
	vertex->owner = -1;
	mtSpinInit(&vertex->ownerspinlock);
#endif
	mesh->vertexUserToNative(vertex->point, point);
#ifdef MD_CONFIG_SSE_SUPPORT
	vertex->point[3] = 0.0;
#endif
	vertex->trirefcount = 0;
	vertex->redirectindex = -1;

	mathQuadricZero(&vertex->quadric);
	point = ADDRESS(point, mesh->pointstride);
    }
}


/* Mesh init step 2, initialize triangles, threaded */
static void mdMeshInitTriangles(mdMesh *mesh, mdThreadData *tdata, int threadcount)
{
    int i, triperthread, triindex, triindexmax;
    long buildtricount;
    void *indices;
    mdTriangle *tri;
    mdVertex *vertex;
    mdEdge edge;
    mathQuadric q;

    memset(&edge, 0, sizeof(mdEdge));    /*DRH added so it's initialized*/
    triperthread = (mesh->tricount / threadcount) + 1;
    triindex = tdata->threadid * triperthread;
    triindexmax = triindex + triperthread;

    if (triindexmax > mesh->tricount)
	triindexmax = mesh->tricount;

    /* Initialize triangles */
    buildtricount = 0;
    indices = ADDRESS(mesh->indices, triindex * mesh->indicesstride);
    tri = &mesh->trilist[triindex];
    edge.op = 0;

    for (; triindex < triindexmax; triindex++, indices = ADDRESS(indices, mesh->indicesstride), tri++) {
	mesh->indicesUserToNative(tri->v, indices);
	mdTriangleComputeQuadric(mesh, tri, &q);

	for (i = 0; i < 3; i++) {
	    vertex = &mesh->vertexlist[tri->v[i]];
#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicSpin32(&vertex->atomicowner, -1, tdata->threadid);
	    mathQuadricAddQuadric(&vertex->quadric, &q);
	    vertex->trirefcount++;
	    mmAtomicWrite32(&vertex->atomicowner, -1);
#else
	    mtSpinLock(&vertex->ownerspinlock);
	    mathQuadricAddQuadric(&vertex->quadric, &q);
	    vertex->trirefcount++;
	    mtSpinUnlock(&vertex->ownerspinlock);
#endif
	}

	if (!(mesh->operationflags & MD_FLAGS_NO_DECIMATION)) {
	    edge.triindex = triindex;
	    edge.v[0] = tri->v[0];
	    edge.v[1] = tri->v[1];

	    if (mmHashLockAddEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
		bu_bomb("SHOULD NOT HAPPEN");

	    edge.v[0] = tri->v[1];
	    edge.v[1] = tri->v[2];

	    if (mmHashLockAddEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
		bu_bomb("SHOULD NOT HAPPEN");

	    edge.v[0] = tri->v[2];
	    edge.v[1] = tri->v[0];

	    if (mmHashLockAddEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge, 1) != MM_HASH_SUCCESS)
		bu_bomb("SHOULD NOT HAPPEN");
	}

	buildtricount++;
	tdata->statusbuildtricount = buildtricount;
    }
}


/* Mesh init step 3, initialize vertex trirefbase, NOT threaded */
static void mdMeshInitTrirefs(mdMesh *mesh)
{
    mdi vertexindex, trirefcount;
    mdVertex *vertex;

    /* Compute base of vertex triangle references */
    trirefcount = 0;
    vertex = mesh->vertexlist;

    for (vertexindex = 0; vertexindex < mesh->vertexcount; vertexindex++, vertex++) {
	vertex->trirefbase = trirefcount;
	trirefcount += vertex->trirefcount;
	vertex->trirefcount = 0;
    }

    mesh->trirefcount = trirefcount;
}


/* Mesh init step 4, store vertex trirefs and accumulate boundary quadrics, threaded */
static void mdMeshBuildTrirefs(mdMesh *mesh, mdThreadData *tdata, int threadcount)
{
    int i, triperthread, triindex, triindexmax;
    long buildrefcount;
    mdTriangle *tri;
    mdVertex *vertex, *trivertex[3];
    mdEdge edge;

    triperthread = (mesh->tricount / threadcount) + 1;
    triindex = tdata->threadid * triperthread;
    triindexmax = triindex + triperthread;

    if (triindexmax > mesh->tricount)
	triindexmax = mesh->tricount;

    /* Store vertex triangle references and accumulate boundary quadrics */
    buildrefcount = 0;
    tri = &mesh->trilist[triindex];

    for (; triindex < triindexmax; triindex++, tri++) {
	for (i = 0; i < 3; i++) {
	    vertex = &mesh->vertexlist[tri->v[i]];
#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicSpin32(&vertex->atomicowner, -1, tdata->threadid);
	    mesh->trireflist[vertex->trirefbase + vertex->trirefcount++] = triindex;
	    mmAtomicWrite32(&vertex->atomicowner, -1);
#else
	    mtSpinLock(&vertex->ownerspinlock);
	    mesh->trireflist[vertex->trirefbase + vertex->trirefcount++] = triindex;
	    mtSpinUnlock(&vertex->ownerspinlock);
#endif
	    trivertex[i] = vertex;
	}

	if (!(mesh->operationflags & MD_FLAGS_NO_DECIMATION)) {
	    tri->u.edgeflags = 0;
	    edge.v[0] = tri->v[1];
	    edge.v[1] = tri->v[0];

	    if (!(mmHashLockFindEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge))) {
		mdMeshAccumulateBoundary(trivertex[0], trivertex[1], trivertex[2]);
		tri->u.edgeflags |= 1;
	    }

	    edge.v[0] = tri->v[2];
	    edge.v[1] = tri->v[1];

	    if (!(mmHashLockFindEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge))) {
		mdMeshAccumulateBoundary(trivertex[1], trivertex[2], trivertex[0]);
		tri->u.edgeflags |= 2;
	    }

	    edge.v[0] = tri->v[0];
	    edge.v[1] = tri->v[2];

	    if (!(mmHashLockFindEntry(mesh->edgehashtable, &mdEdgeHashEdge, &edge))) {
		mdMeshAccumulateBoundary(trivertex[2], trivertex[0], trivertex[1]);
		tri->u.edgeflags |= 4;
	    }
	}

	buildrefcount++;
	tdata->statusbuildrefcount = buildrefcount;
    }
}


/* Mesh clean up */
static void mdMeshEnd(mdMesh *mesh)
{
#ifndef MM_ATOMIC_SUPPORT
    mdi vindex;
    mdVertex *vertex;
    vertex = mesh->vertexlist;

    for (vindex = 0; vindex < mesh->vertexcount; vindex++, vertex++)
	mtSpinDestroy(&vertex->ownerspinlock);

    mtSpinDestroy(&mesh->trirefspinlock);
    mtSpinDestroy(&mesh->globalvertexspinlock);
#endif
    mmAlignFree(mesh->vertexlist);
    bu_free(mesh->trireflist, "trireflist");
    bu_free(mesh->trilist, "trilist");
}


/****/


static void mdSortOp(mdMesh *mesh, mdThreadData *tdata, mdOp *op, int denyflag)
{
    mdf collapsecost;
    collapsecost = op->value + op->penalty;

    if ((denyflag) || (collapsecost > mesh->maxcollapsecost)) {
#ifdef MM_ATOMIC_SUPPORT

	if (!(mmAtomicRead32(&op->flags) & MD_OP_FLAGS_DETACHED)) {
	    mmBinSortRemove(tdata->binsort, op, op->collapsecost);
	    mmAtomicOr32(&op->flags, MD_OP_FLAGS_DETACHED);
	}

#else
	mtSpinLock(&op->spinlock);

	if (!(op->flags & MD_OP_FLAGS_DETACHED)) {
	    mmBinSortRemove(tdata->binsort, op, op->collapsecost);
	    op->flags |= MD_OP_FLAGS_DETACHED;
	}

	mtSpinUnlock(&op->spinlock);
#endif
    } else {
#ifdef MM_ATOMIC_SUPPORT

	if (mmAtomicRead32(&op->flags) & MD_OP_FLAGS_DETACHED) {
	    mmBinSortAdd(tdata->binsort, op, collapsecost);
	    mmAtomicAnd32(&op->flags, ~MD_OP_FLAGS_DETACHED);
	} else if (!EQUAL(op->collapsecost, collapsecost))
	    mmBinSortUpdate(tdata->binsort, op, op->collapsecost, collapsecost);

#else
	mtSpinLock(&op->spinlock);

	if (op->flags & MD_OP_FLAGS_DETACHED) {
	    mmBinSortAdd(tdata->binsort, op, collapsecost);
	    op->flags &= ~MD_OP_FLAGS_DETACHED;
	} else if (!EQUAL(op->collapsecost, collapsecost))
	    mmBinSortUpdate(tdata->binsort, op, op->collapsecost, collapsecost);

	mtSpinUnlock(&op->spinlock);
#endif
	op->collapsecost = collapsecost;
    }
}


static void mdUpdateOp(mdMesh *mesh, mdThreadData *tdata, mdOp *op, int32_t opflagsmask)
{
    int denyflag, flags;
#ifdef MM_ATOMIC_SUPPORT

    for (;;) {
	flags = mmAtomicRead32(&op->flags);

	if (mmAtomicCmpReplace32(&op->flags, flags, flags & opflagsmask))
	    break;
    }

#else
    mtSpinLock(&op->spinlock);
    flags = op->flags;
    op->flags &= opflagsmask;
    mtSpinUnlock(&op->spinlock);
#endif

    if (!(flags & MD_OP_FLAGS_UPDATE_NEEDED))
	return;

    if (flags & MD_OP_FLAGS_DELETED)
	return;

    if (flags & MD_OP_FLAGS_DELETION_PENDING) {
	if (!(flags & MD_OP_FLAGS_DETACHED))
	    mmBinSortRemove(tdata->binsort, op, op->collapsecost);

	/* Race condition, flag the op as deleted but don't free it. Meh, free them all at the end with FreeAll(). */
	/*
	    mmBlockFree( &tdata->opblock, op );
	*/
#ifdef MM_ATOMIC_SUPPORT
	mmAtomicOr32(&op->flags, MD_OP_FLAGS_DELETED);
#else
	mtSpinLock(&op->spinlock);
	op->flags |= MD_OP_FLAGS_DELETED;
	mtSpinUnlock(&op->spinlock);
#endif
    } else {
	op->penalty = mdEdgeCollapsePenalty(mesh, tdata, op->v0, op->v1, op->collapsepoint, &denyflag);
	mdSortOp(mesh, tdata, op, denyflag);
    }
}

static void mdUpdateBufferOps(mdMesh *mesh, mdThreadData *tdata, mdUpdateBuffer *updatebuffer, mdLockBuffer *lockbuffer)
{
    int vindex;
    mdOp *op;

#ifdef MM_ATOMIC_SUPPORT
    mmAtomicSpin32(&updatebuffer->atomlock, 0x0, 0x1);
#else
    mtSpinLock(&updatebuffer->spinlock);
#endif

    for (vindex = 0; vindex < updatebuffer->opcount; vindex++) {
	op = (mdOp *)updatebuffer->opbuffer[vindex];

	if (mdOpResolveLockEdgeTry(mesh, tdata, lockbuffer, op)) {
	    mdUpdateOp(mesh, tdata, op, ~(MD_OP_FLAGS_UPDATE_QUEUED | MD_OP_FLAGS_UPDATE_NEEDED));
	    mdLockBufferUnlockAll(mesh, tdata, lockbuffer);
	} else {
#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicWrite32(&updatebuffer->atomlock, 0x0);
#else
	    mtSpinUnlock(&updatebuffer->spinlock);
#endif
	    mdOpResolveLockEdge(mesh, tdata, lockbuffer, op);
	    mdUpdateOp(mesh, tdata, op, ~(MD_OP_FLAGS_UPDATE_QUEUED | MD_OP_FLAGS_UPDATE_NEEDED));
	    mdLockBufferUnlockAll(mesh, tdata, lockbuffer);
#ifdef MM_ATOMIC_SUPPORT
	    mmAtomicSpin32(&updatebuffer->atomlock, 0x0, 0x1);
#else
	    mtSpinLock(&updatebuffer->spinlock);
#endif
	}
    }

    updatebuffer->opcount = 0;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWrite32(&updatebuffer->atomlock, 0x0);
#else
    mtSpinUnlock(&updatebuffer->spinlock);
#endif
}


static int mdMeshProcessQueue(mdMesh *mesh, mdThreadData *tdata)
{
    int vindex, decimationcount, stepindex;
    int32_t opflags;
    mdf maxcost;
    mdOp *op;
    mdLockBuffer lockbuffer;

    mdLockBufferInit(&lockbuffer, 2);

    stepindex = 0;
    maxcost = 0.0;

    decimationcount = 0;

    for (;;) {
	/* Update all ops flagged as requiring update */
	if (mesh->operationflags & MD_FLAGS_CONTINUOUS_UPDATE) {
	    for (vindex = 0; vindex < mesh->updatebuffercount; vindex++)
		mdUpdateBufferOps(mesh, tdata, &tdata->updatebuffer[vindex], &lockbuffer);
	}

	/* Acquire first op */
	op = (mdOp *)mmBinSortGetFirstItem(tdata->binsort, maxcost);

	if (!(op)) {
	    if (++stepindex >= mesh->syncstepcount)
		break;

	    maxcost = mesh->maxcollapsecost * ((mdf)stepindex / (mdf)mesh->syncstepcount);
	    mdBarrierSync(&mesh->workbarrier);

	    /* Update all ops flagged as requiring update */
	    if (!(mesh->operationflags & MD_FLAGS_CONTINUOUS_UPDATE)) {
		for (vindex = 0; vindex < mesh->updatebuffercount; vindex++)
		    mdUpdateBufferOps(mesh, tdata, &tdata->updatebuffer[vindex], &lockbuffer);
	    }

	    continue;
	}

	/* Acquire lock for op edge and all trirefs vertices */
	mdOpResolveLockFull(mesh, tdata, &lockbuffer, op);

	/* If our op was flagged for update between mdUpdateBufferOps() and before we acquired lock, no big deal, catch the update */
#ifdef MM_ATOMIC_SUPPORT
	opflags = mmAtomicRead32(&op->flags);
#else
	mtSpinLock(&op->spinlock);
	opflags = op->flags;
	mtSpinUnlock(&op->spinlock);
#endif

	if (opflags & MD_OP_FLAGS_UPDATE_NEEDED) {
	    mdLockBufferUnlockAll(mesh, tdata, &lockbuffer);
	    mdUpdateOp(mesh, tdata, op, ~MD_OP_FLAGS_UPDATE_NEEDED);
	    continue;
	}

	if (!(mdEdgeCollisionCheck(mesh, tdata, op->v0, op->v1))) {
#ifdef MM_ATOMIC_SUPPORT

	    if (mmAtomicRead32(&op->flags) & MD_OP_FLAGS_DETACHED)
		bu_bomb("SHOULD NOT HAPPEN");

	    mmAtomicOr32(&op->flags, MD_OP_FLAGS_DETACHED);
	    mmBinSortRemove(tdata->binsort, op, op->collapsecost);
#else
	    mtSpinLock(&op->spinlock);

	    if (op->flags & MD_OP_FLAGS_DETACHED)
		bu_bomb("SHOULD NOT HAPPEN");

	    op->flags |= MD_OP_FLAGS_DETACHED;
	    mtSpinUnlock(&op->spinlock);
	    mmBinSortRemove(tdata->binsort, op, op->collapsecost);
#endif
	    goto opdone;
	}

	mdEdgeCollapse(mesh, tdata, op->v0, op->v1, op->collapsepoint);
	decimationcount++;

    opdone:
	/* Release all locks for op */
	mdLockBufferUnlockAll(mesh, tdata, &lockbuffer);
    }

    mdLockBufferEnd(&lockbuffer);

    return decimationcount;
}


/****/


typedef struct {
    mdf normal[3];
    mdf factor[3];
} mdTriNormal;


static mdi mdMeshPackCountTriangles(mdMesh *mesh)
{
    mdi tricount;
    mdTriangle *tri, *triend;

    tricount = 0;
    tri = mesh->trilist;
    triend = &tri[mesh->tricount];

    for (; tri < triend; tri++) {
	if (tri->v[0] == -1)
	    continue;

	tri->u.redirectindex = tricount;
	tricount++;
    }

    mesh->tripackcount = tricount;
    return tricount;
}

static mdf mdMeshAngleFactor(mdf dotangle)
{
    mdf factor;

    if (dotangle >= 1.0)
	factor = 0.0;
    else if (dotangle <= -1.0)
	factor = 0.5 * M_PI;
    else {
	factor = mdfacos(dotangle);

	if (isnan(factor))
	    factor = 0.0;
    }

    return factor;
}

static void mdMeshBuildTriangleNormals(mdMesh *mesh)
{
    mdTriangle *tri, *triend;
    mdVertex *vertex0, *vertex1, *vertex2;
    mdf vecta[3], vectb[3], vectc[3], normalfactor, magna, magnb, magnc, norm, norminv;
    mdTriNormal *trinormal;

    trinormal = (mdTriNormal *)mesh->trinormal;

    normalfactor = 1.0;

    if (mesh->operationflags & MD_FLAGS_TRIANGLE_WINDING_CCW)
	normalfactor = -1.0;

    tri = mesh->trilist;
    triend = &tri[mesh->tricount];

    for (; tri < triend; tri++) {
	if (tri->v[0] == -1)
	    continue;

	/* Compute triangle normal */
	vertex0 = &mesh->vertexlist[tri->v[0]];
	vertex1 = &mesh->vertexlist[tri->v[1]];
	vertex2 = &mesh->vertexlist[tri->v[2]];
	VSUB2(vecta, vertex1->point, vertex0->point);
	VSUB2(vectb, vertex2->point, vertex0->point);
	VCROSS(trinormal->normal, vectb, vecta);

	norm = mdfsqrt(VDOT(trinormal->normal, trinormal->normal));

	if (!ZERO(norm)) {
	    norminv = normalfactor / norm;
	    trinormal->normal[0] *= norminv;
	    trinormal->normal[1] *= norminv;
	    trinormal->normal[2] *= norminv;
	}

	VSUB2(vectc, vertex2->point, vertex1->point);
	magna = MAGNITUDE(vecta);
	magnb = MAGNITUDE(vectb);
	magnc = MAGNITUDE(vectc);
	trinormal->factor[0] = norm * mdMeshAngleFactor(VDOT(vecta, vectb) / (magna * magnb));
	trinormal->factor[1] = norm * mdMeshAngleFactor(-VDOT(vecta, vectc) / (magna * magnc));
	trinormal->factor[2] = norm * mdMeshAngleFactor(VDOT(vectb, vectc) / (magnb * magnc));

	trinormal++;
    }
}


static int mdMeshVertexComputeNormal(mdMesh *mesh, mdi vertexindex, mdi *trireflist, int trirefcount, mdf *normal)
{
    int vindex = 0;
    int pivot = 0;
    int validflag = 0;
    mdi triindex;
    mdf norm, norminv;
    mdTriangle *tri;
    mdTriNormal *trinormal, *tn;

    trinormal = (mdTriNormal *)mesh->trinormal;

    /* Loop through all trirefs associated with the vertex */
    validflag = 0;
    VSETALL(normal, 0.0);

    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];

	if (triindex == -1)
	    continue;

	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	if (tri->v[0] == vertexindex)
	    pivot = 0;
	else if (tri->v[1] == vertexindex)
	    pivot = 1;
	else if (tri->v[2] == vertexindex)
	    pivot = 2;
	else
	    bu_bomb("SHOULD NOT HAPPEN");

	tn = &trinormal[tri->u.redirectindex];
	VJOIN1(normal, normal, tn->factor[pivot], tn->normal);
	validflag = 1;
    }

    if (!(validflag))
	return 0;

    norm = mdfsqrt(VDOT(normal, normal));

    if (!ZERO(norm)) {
	norminv = 1.0 / norm;
	normal[0] *= norminv;
	normal[1] *= norminv;
	normal[2] *= norminv;
    }

    return 1;
}


static mdi mdMeshCloneVertex(mdMesh *mesh, mdi cloneindex, mdf *point)
{
    mdi vertexindex, retindex;
    mdVertex *vertex;
    mdOpAttrib *attrib, *attribend;
    void *attrsrc, *attrdst;

    retindex = -1;
    vertex = &mesh->vertexlist[mesh->clonesearchindex];

    for (vertexindex = mesh->clonesearchindex; vertexindex < mesh->vertexalloc; vertexindex++, vertex++) {
	if ((vertexindex < mesh->vertexcount) && (vertex->trirefcount))
	    continue;

	vertex->trirefcount = -1;
	vertex->redirectindex = -1;
	/* Copy the point from the cloned vertex */
	VMOVE(vertex->point, point);

	/* Copy generic vertex attributes from the cloned vertex */
	if (mesh->attribcount) {
	    attrib = mesh->attrib;
	    attribend = &mesh->attrib[mesh->attribcount];

	    for (attrib = mesh->attrib; attrib < attribend; attrib++) {
		attrsrc = ADDRESS(attrib->base, cloneindex * attrib->stride);
		attrdst = ADDRESS(attrib->base, vertexindex * attrib->stride);
		memcpy(attrdst, attrsrc, attrib->count * attrib->width);
	    }
	}

	retindex = vertexindex;

	if (vertexindex >= mesh->vertexcount)
	    mesh->vertexcount = vertexindex + 1;

	break;
    }

    mesh->clonesearchindex = vertexindex;
    return retindex;
}


static void mdMeshVertexRedirectTriRefs(mdMesh *mesh, mdi vertexindex, mdi newvertexindex, mdi *trireflist, int trirefcount)
{
    int vindex;
    mdi triindex;
    mdTriangle *tri;

    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];

	if (triindex == -1)
	    continue;

	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	if (tri->v[0] == vertexindex)
	    tri->v[0] = newvertexindex;
	else if (tri->v[1] == vertexindex)
	    tri->v[1] = newvertexindex;
	else if (tri->v[2] == vertexindex)
	    tri->v[2] = newvertexindex;
	else
	    bu_bomb("SHOULD NOT HAPPEN");
    }
}


/* Find a target normal */
static int mdMeshVertexFindTarget(mdMesh *mesh, mdi *trireflist, int trirefcount, mdf **targetnormal)
{
    int i0, i1;
    mdi triindex0, triindex1;
    mdf dotangle, bestdotangle;
    mdTriangle *tri0, *tri1;
    mdTriNormal *trinormal, *tn0, *tn1;

    /* Of all triangles, find the most diverging pair, pick one */
    targetnormal[0] = 0;
    targetnormal[1] = 0;
    trinormal = (mdTriNormal *)mesh->trinormal;
    bestdotangle = mesh->normalsearchangle;

    for (i0 = 0; i0 < trirefcount; i0++) {
	triindex0 = trireflist[i0];

	if (triindex0 == -1)
	    continue;

	tri0 = &mesh->trilist[triindex0];

	if (tri0->v[0] == -1)
	    continue;

	tn0 = &trinormal[tri0->u.redirectindex];

	for (i1 = i0 + 1; i1 < trirefcount; i1++) {
	    triindex1 = trireflist[i1];

	    if (triindex1 == -1)
		continue;

	    tri1 = &mesh->trilist[triindex1];

	    if (tri1->v[0] == -1)
		continue;

	    tn1 = &trinormal[tri1->u.redirectindex];
	    dotangle = VDOT(tn0->normal, tn1->normal);

	    if (dotangle < bestdotangle) {
		bestdotangle = dotangle;
		targetnormal[0] = tn0->normal;
		targetnormal[1] = tn1->normal;
	    }
	}
    }

    return (targetnormal[0] != 0);
}


#define MD_MESH_TRIREF_MAX (256)

static int mdMeshVertexBuildNormal(mdMesh *mesh, mdi vertexindex, mdi *trireflist, int trirefcount, mdf *point, mdf *normal)
{
    int vindex, trirefbuffercount;
    mdi triindex, newvertexindex;
    mdi trirefbuffer[MD_MESH_TRIREF_MAX];
    mdf dotangle0, dotangle1;
    mdf *newnormal, *targetnormal[2];
    mdTriangle *tri;
    mdTriNormal *trinormal, *tn;

    if (trirefcount > MD_MESH_TRIREF_MAX)
	return 1;

    /* Loop to repeat as we retire trirefs from the list */
    trinormal = (mdTriNormal *)mesh->trinormal;

    for (;;) {
	/* Compute normal for vertex */
	if (!(mdMeshVertexComputeNormal(mesh, vertexindex, trireflist, trirefcount, normal)))
	    return 0;

	/* If user doesn't allow vertex splitting, take the normal as it is */
	if (!(mesh->operationflags & MD_FLAGS_NORMAL_VERTEX_SPLITTING))
	    break;

	/* Find a pair of target normals */
	if (!(mdMeshVertexFindTarget(mesh, trireflist, trirefcount, targetnormal)))
	    break;

	/* Find all trirefs that agree with targetnormal[1] and store them independently */
	trirefbuffercount = 0;

	for (vindex = 0; vindex < trirefcount; vindex++) {
	    triindex = trireflist[vindex];

	    if (triindex == -1)
		continue;

	    tri = &mesh->trilist[triindex];

	    if (tri->v[0] == -1)
		continue;

	    tn = &trinormal[tri->u.redirectindex];
	    dotangle1 = VDOT(targetnormal[1], tn->normal);

	    if (dotangle1 < mesh->normalsearchangle)
		continue;

	    dotangle0 = VDOT(targetnormal[0], tn->normal);

	    if (dotangle0 > dotangle1)
		continue;

	    trirefbuffer[trirefbuffercount++] = triindex;
	    trireflist[vindex] = -1;
	}

	if (!(trirefbuffercount))
	    break;

	/* Find an unused vertex, bail out if none can be found */
	newvertexindex = mdMeshCloneVertex(mesh, vertexindex, point);

	if (newvertexindex == -1)
	    break;

	/* Correct all trirefs to new vertex */
	mdMeshVertexRedirectTriRefs(mesh, vertexindex, newvertexindex, trirefbuffer, trirefbuffercount);

	/* Spawn a new vertex */
	newnormal = (mdf *)ADDRESS(mesh->vertexnormal, newvertexindex * 3 * sizeof(mdf));
	mdMeshVertexBuildNormal(mesh, newvertexindex, trirefbuffer, trirefbuffercount, point, newnormal);
    }

    return 1;
}


/* In some rare circumstances, a vertex can be unused even with redirectindex = -1 and trirefs leading to deleted triangles */
static int mdMeshVertexCheckUse(mdMesh *mesh, mdi *trireflist, int trirefcount)
{
    int vindex;
    mdi triindex;
    mdTriangle *tri;

    for (vindex = 0; vindex < trirefcount; vindex++) {
	triindex = trireflist[vindex];

	if (triindex == -1)
	    continue;

	tri = &mesh->trilist[triindex];

	if (tri->v[0] == -1)
	    continue;

	return 1;
    }

    return 0;
}


static void mdMeshWriteVertices(mdMesh *mesh, mdOpAttrib *normalattrib, mdf *vertexnormal)
{
    mdi vertexindex, writeindex;
    mdf *point = NULL;
    mdf *normal = NULL;
    mdVertex *vertex = NULL;
    mdOpAttrib *attrib = NULL;
    mdOpAttrib *attribend = NULL;
    void *attrsrc = NULL;
    void *attrdst = NULL;
    void (*writenormal)(void *dst, mdf * src);

    writenormal = 0;

    if ((vertexnormal) && (normalattrib) && (normalattrib->count >= 3)) {
	if (normalattrib->width == sizeof(float))
	    writenormal = mdVertexNativeToFloat;
	else if (normalattrib->width == sizeof(double))
	    writenormal = mdVertexNativeToDouble;
    }

    point = mesh->point;
    writeindex = 0;
    vertex = mesh->vertexlist;
    attribend = &mesh->attrib[mesh->attribcount];

    for (vertexindex = 0; vertexindex < mesh->vertexcount; vertexindex++, vertex++) {
	if (!(mesh->operationflags & MD_FLAGS_NO_VERTEX_PACKING)) {
	    if (vertex->redirectindex != -1)
		continue;

	    if (!(vertex->trirefcount))
		continue;

	    /* The whole mdMeshRecomputeNormals() process already strips unused vertices */
	    if (!(vertexnormal) && (vertex->trirefcount != -1) && !(mdMeshVertexCheckUse(mesh, &mesh->trireflist[vertex->trirefbase], vertex->trirefcount)))
		continue;
	}

	vertex->redirectindex = writeindex;
	mesh->vertexNativeToUser(point, vertex->point);

	if (writenormal) {
	    normal = (mdf *)ADDRESS(vertexnormal, vertexindex * 3 * sizeof(mdf));
	    attrdst = ADDRESS(normalattrib->base, writeindex * normalattrib->stride);
	    writenormal(attrdst, normal);
	}

	if (vertexindex != writeindex) {
	    for (attrib = mesh->attrib; attrib < attribend; attrib++) {
		attrsrc = ADDRESS(attrib->base, vertexindex * attrib->stride);
		attrdst = ADDRESS(attrib->base, writeindex * attrib->stride);
		memcpy(attrdst, attrsrc, attrib->count * attrib->width);
	    }
	}

	point = (mdf *)ADDRESS(point, mesh->pointstride);
	writeindex++;
    }

    mesh->vertexpackcount = writeindex;

    if (mesh->operationflags & MD_FLAGS_NO_VERTEX_PACKING)
	mesh->vertexpackcount = mesh->vertexcount;
}


static void mdMeshWriteIndices(mdMesh *mesh)
{
    mdi finaltricount, v[3];
    mdTriangle *tri, *triend;
    mdVertex *vertex0, *vertex1, *vertex2;
    void *indices;

    indices = mesh->indices;
    finaltricount = 0;
    tri = mesh->trilist;
    triend = &tri[mesh->tricount];

    for (; tri < triend; tri++) {
	if (tri->v[0] == -1)
	    continue;

	vertex0 = &mesh->vertexlist[tri->v[0]];
	v[0] = vertex0->redirectindex;
	vertex1 = &mesh->vertexlist[tri->v[1]];
	v[1] = vertex1->redirectindex;
	vertex2 = &mesh->vertexlist[tri->v[2]];
	v[2] = vertex2->redirectindex;
	mesh->indicesNativeToUser(indices, v);
	indices = ADDRESS(indices, mesh->indicesstride);
	finaltricount++;
    }

    mesh->tripackcount = finaltricount;
}


/* Recompute normals, store them along with vertices and indices at once */
static void mdMeshRecomputeNormals(mdMesh *mesh, mdOpAttrib *normalattrib)
{
    mdi vertexindex;
    mdf *normal;
    mdVertex *vertex;

    /* Start search for free vertices to clone at 0 */
    mesh->clonesearchindex = 0;

    /* Count triangles and assign redirectindex to each in sequence */
    mdMeshPackCountTriangles(mesh);

    mesh->vertexnormal = bu_malloc(mesh->vertexalloc * 3 * sizeof(normal), "vertexnormal");
    mesh->trinormal = bu_malloc(mesh->tripackcount * sizeof(mdTriNormal), "trinormal");

    /* Build up mesh->trinormal, store normals, area and vertex angles of each triangle */
    mdMeshBuildTriangleNormals(mesh);

    /* Build each vertex normal */
    vertex = mesh->vertexlist;

    for (vertexindex = 0; vertexindex < mesh->vertexcount; vertexindex++, vertex++) {
	if (!(vertex->trirefcount) || (vertex->trirefcount == -1))
	    continue;

	normal = (mdf *)ADDRESS(mesh->vertexnormal, vertexindex * 3 * sizeof(mdf));

	if (!(mdMeshVertexBuildNormal(mesh, vertexindex, &mesh->trireflist[vertex->trirefbase], vertex->trirefcount, vertex->point, normal)))
	    vertex->trirefcount = 0;
    }

    /* Write vertices along with normals and other attributes */
    mdMeshWriteVertices(mesh, normalattrib, (mdf *)mesh->vertexnormal);
    mdMeshWriteIndices(mesh);

    bu_free(mesh->vertexnormal, "vertexnormal");
    bu_free(mesh->trinormal, "trinormal");
}


/****/


typedef struct {
    int threadid;
    mdMesh *mesh;
    int deletioncount;
    int decimationcount;
    mdThreadData *tdata;
    int stage;
} mdThreadInit;

#ifndef MM_ATOMIC_SUPPORT
int mdFreeOpCallback(void *chunk, void *UNUSED(userpointer))
{
    mdOp *op = (mdOp *)chunk;
    mtSpinDestroy(&op->spinlock);
    return 0;
}
#endif


static int mdThreadMain(void *value)
{
    int vindex, tribase, trimax, triperthread, nodeindex;
    int groupthreshold;
    mdThreadInit *tinit;
    mdThreadData tdata;
    mdMesh *mesh;

    tinit = (mdThreadInit *)value;
    mesh = tinit->mesh;
    tinit->tdata = &tdata;

    /* Thread memory initialization */
    tdata.threadid = tinit->threadid;
    tdata.statusbuildtricount = 0;
    tdata.statusbuildrefcount = 0;
    tdata.statuspopulatecount = 0;
    tdata.statusdeletioncount = 0;
    groupthreshold = mesh->tricount >> 9;

    if (groupthreshold < 256)
	groupthreshold = 256;

    nodeindex = -1;

    if (mmcontext.numaflag) {
	mmThreadBindToCpu(tdata.threadid);
	nodeindex = mmCpuGetNode(tdata.threadid);
	mmBlockNodeInit(&tdata.opblock, nodeindex, sizeof(mdOp), 16384, 16384, 0x10);
    } else
	mmBlockInit(&tdata.opblock, sizeof(mdOp), 16384, 16384, 0x10);

    tdata.binsort = mmBinSortInit(offsetof(mdOp, list), 64, 16, -0.2 * mesh->maxcollapsecost, 1.2 * mesh->maxcollapsecost, groupthreshold, mdMeshOpValueCallback, 6, nodeindex);

    for (vindex = 0; vindex < mesh->updatebuffercount; vindex++)
	mdUpdateBufferInit(&tdata.updatebuffer[vindex], BU_PAGE_SIZE);

    /* Wait until all threads have properly initialized */
    if (mesh->updatestatusflag)
	mdBarrierSync(&mesh->globalbarrier);

    /* Build mesh step 1 */
    if (!(tdata.threadid))
	tinit->stage = MD_STATUS_STAGE_BUILDVERTICES;

    mdMeshInitVertices(mesh, &tdata, mesh->threadcount);
    mdBarrierSync(&mesh->workbarrier);

    /* Build mesh step 2 */
    if (!(tdata.threadid))
	tinit->stage = MD_STATUS_STAGE_BUILDTRIANGLES;

    mdMeshInitTriangles(mesh, &tdata, mesh->threadcount);
    mdBarrierSync(&mesh->globalbarrier);

    /* Build mesh step 3 is not parallel, have the main thread run it */
    if (!(tdata.threadid))
	tinit->stage = MD_STATUS_STAGE_BUILDTRIREFS;

    mdBarrierSync(&mesh->globalbarrier);

    /* Build mesh step 4 */
    mdMeshBuildTrirefs(mesh, &tdata, mesh->threadcount);
    mdBarrierSync(&mesh->workbarrier);

    if (!(mesh->operationflags & MD_FLAGS_NO_DECIMATION)) {
	/* Initialize the thread's op queue */
	if (!(tdata.threadid))
	    tinit->stage = MD_STATUS_STAGE_BUILDQUEUE;

	triperthread = (mesh->tricount / mesh->threadcount) + 1;
	tribase = tdata.threadid * triperthread;
	trimax = tribase + triperthread;

	if (trimax > mesh->tricount)
	    trimax = mesh->tricount;

	mdMeshPopulateOpList(mesh, &tdata, tribase, trimax - tribase);

	/* Wait for all threads to reach this point */
	mdBarrierSync(&mesh->workbarrier);

	/* Process the thread's op queue */
	if (!(tdata.threadid))
	    tinit->stage = MD_STATUS_STAGE_DECIMATION;

	tinit->decimationcount = mdMeshProcessQueue(mesh, &tdata);
    }

    /* Wait for all threads to reach this point */
    tinit->deletioncount = tdata.statusdeletioncount;
    mdBarrierSync(&mesh->globalbarrier);

    /* If we didn't use atomic operations, we have spinlocks to destroy in each op */
#ifndef MM_ATOMIC_SUPPORT
    mmBlockProcessList(&tdata.opblock, 0, mdFreeOpCallback);
#endif

    /* Free thread memory allocations */
    mmBlockFreeAll(&tdata.opblock);

    for (vindex = 0; vindex < mesh->updatebuffercount; vindex++)
	mdUpdateBufferEnd(&tdata.updatebuffer[vindex]);

    mmBinSortFree(tdata.binsort);

    return 0;
}


/********/


static const char *mdStatusStageName[] = {
    "Initializing",
    "Building Vertices",
    "Building Triangles",
    "Building Trirefs",
    "Building Queues",
    "Decimating Mesh",
    "Storing Geometry",
    "Done"
};

static double mdStatusStageProgress[] = {
    0.0,
    2.0,
    6.0,
    6.0,
    8.0,
    75.0,
    3.0,
    0.0
};

static void mdUpdateStatus(mdMesh *mesh, mdThreadInit *threadinit, int stage, mdStatus *status)
{
    int threadid, stageindex;
    long buildtricount, buildrefcount, populatecount, deletioncount;
    double progress, subprogress;
    mdThreadInit *tinit;
    mdThreadData *tdata;

    subprogress = 0.0;

    if (!(threadinit))
	status->stage = stage;
    else {
	tinit = &threadinit[0];
	status->stage = tinit->stage;

	tdata = tinit->tdata;

	if ((unsigned)status->stage >= MD_STATUS_STAGE_COUNT)
	    return;

	buildtricount = 0;
	buildrefcount = 0;
	populatecount = 0;
	deletioncount = 0;

	for (threadid = 0; threadid < mesh->threadcount; threadid++) {
	    tinit = &threadinit[threadid];
	    tdata = tinit->tdata;
	    buildtricount += tdata->statusbuildtricount;
	    buildrefcount += tdata->statusbuildrefcount;
	    populatecount += tdata->statuspopulatecount;
	    deletioncount += tdata->statusdeletioncount;
	}

	status->trianglecount = mesh->tricount - deletioncount;

	if (status->stage == MD_STATUS_STAGE_DECIMATION)
	    subprogress = 1.0 - ((double)status->trianglecount / (double)mesh->tricount);
	else if (status->stage == MD_STATUS_STAGE_BUILDQUEUE)
	    subprogress = (double)populatecount / (double)mesh->tricount;
	else if (status->stage == MD_STATUS_STAGE_BUILDTRIANGLES)
	    subprogress = (double)buildtricount / (double)mesh->tricount;
	else if (status->stage == MD_STATUS_STAGE_BUILDTRIREFS)
	    subprogress = (double)buildrefcount / (double)mesh->tricount;

	subprogress = mdfmax(0.0, mdfmin(1.0, subprogress));
    }

    progress = 0.0;
    status->stagename = mdStatusStageName[status->stage];

    for (stageindex = 0; stageindex < status->stage; stageindex++)
	progress += mdStatusStageProgress[stageindex];

    progress += subprogress * mdStatusStageProgress[status->stage];

    if (progress > status->progress)
	status->progress = progress;
}


/********/


void mdInit()
{
    mmInit();
    mdInitFlag = 1;
}

void mdOperationInit(mdOperation *op)
{
    /* Input */
    op->vertexcount = 0;
    op->vertex = 0;
    op->vertexstride = 0;
    op->vertexalloc = 0;
    op->indices = 0;
    op->indiceswidth = 0;
    op->indicesstride = 0;
    op->tricount = 0;
    op->decimationstrength = 0.0;
    op->attribcount = 0;
    op->normalattrib.base = 0;

    /* Status callback */
    op->statusmiliseconds = 0;
    op->statusopaquepointer = 0;
    op->statuscallback = 0;

    /* Advanced settings, default values */
    op->compactnesstarget = MD_COLLAPSE_PENALTY_COMPACT_TARGET;
    op->compactnesspenalty = MD_COLLAPSE_PENALTY_COMPACT_FACTOR;
    op->syncstepcount = 32;
    op->normalsearchangle = 45.0;
    mmInit();
    op->maxmemorysize = (UINT64_C(2)) * 1024 * 1024 * 1024;

    if (mmcontext.sysmemory) {
	op->maxmemorysize = (mmcontext.sysmemory >> 1) + (mmcontext.sysmemory >> 2);     /* By default, allow to allocate up to 75% of system memory */

	if (op->maxmemorysize < 1024 * 1024 * 1024)
	    op->maxmemorysize = 1024 * 1024 * 1024;
    }
}

void mdOperationData(mdOperation *op, size_t vertexcount, void *vertex, int vertexwidth, size_t vertexstride, size_t tricount, void *indices, int indiceswidth, size_t indicesstride)
{
    op->vertexcount = vertexcount;
    op->vertex = vertex;
    op->vertexwidth = vertexwidth;
    op->vertexstride = vertexstride;
    op->indices = indices;
    op->indiceswidth = indiceswidth;
    op->indicesstride = indicesstride;
    op->tricount = tricount;
}

int mdOperationAddAttrib(mdOperation *op, void *base, int width, size_t count, size_t stride, int flags)
{
    mdOpAttrib *attrib;

    if (flags & MD_ATTRIB_FLAGS_COMPUTE_NORMALS)
	attrib = &op->normalattrib;
    else {
	if (op->attribcount >= MD_ATTRIB_MAX)
	    return 0;

	attrib = &op->attrib[op->attribcount++];
    }

    attrib->base = base;
    attrib->width = width;
    attrib->count = count;
    attrib->stride = stride;
    attrib->flags = flags;

    return 1;
}

void mdOperationStrength(mdOperation *op, double decimationstrength)
{
    /* pow( decimationstrength/4.0, 4.0 ) */
    decimationstrength *= 0.25;
    decimationstrength *= decimationstrength;
    op->decimationstrength = decimationstrength * decimationstrength;
}

void mdOperationStatusCallback(mdOperation *op, void (*statuscallback)(void *opaquepointer, const mdStatus *status), void *opaquepointer, long miliseconds)
{
    op->statusmiliseconds = miliseconds;
    op->statusopaquepointer = opaquepointer;
    op->statuscallback = statuscallback;
}


/****/


int mdMeshDecimation(mdOperation *operation, int flags)
{
    int threadid, threadcount;
    mdMesh mesh;
    bu_thrd_t thread[MAX_PSW];
    mdThreadInit threadinit[MAX_PSW];
    mdThreadInit *tinit;
    mdStatus status;
#if 0 /*ifdef MD_CONF_ENABLE_PROGRESS*/
    long deletioncount;
#endif

    operation->decimationcount = 0;
    operation->msecs = 0;

    if (!(mdInitFlag)) {
	mdInitFlag = 1;
	/* Initialization of the memory manager, filling struct mmcontext with info about CPUs and NUMA */
	mmInit();
    }

    threadcount = bu_avail_cpus();

    if ((size_t)threadcount > operation->tricount / 1024)
	threadcount = operation->tricount / 1024;

    if (threadcount < 1)
	threadcount = 1;

    /* Get operation general settings */
    mesh.point = (mdf *)operation->vertex;
    mesh.pointstride = operation->vertexstride;
    mesh.vertexcount = operation->vertexcount;
    mesh.indices = operation->indices;
    mesh.indicesstride = operation->indicesstride;

    switch (operation->indiceswidth) {
	case sizeof(uint8_t):
	    mesh.indicesUserToNative = mdIndicesInt8ToNative;
	    mesh.indicesNativeToUser = mdIndicesNativeToInt8;
	    break;

	case sizeof(uint16_t):
	    mesh.indicesUserToNative = mdIndicesInt16ToNative;
	    mesh.indicesNativeToUser = mdIndicesNativeToInt16;
	    break;

	case sizeof(uint32_t):
	    mesh.indicesUserToNative = mdIndicesInt32ToNative;
	    mesh.indicesNativeToUser = mdIndicesNativeToInt32;
	    break;

	case sizeof(uint64_t):
	    mesh.indicesUserToNative = mdIndicesInt64ToNative;
	    mesh.indicesNativeToUser = mdIndicesNativeToInt64;
	    break;

	default:
	    return 0;
    }

    switch (operation->vertexwidth) {
	case sizeof(float):
	    mesh.vertexUserToNative = mdVertexFloatToNative;
	    mesh.vertexNativeToUser = mdVertexNativeToFloat;
	    break;

	case sizeof(double):
	    mesh.vertexUserToNative = mdVertexDoubleToNative;
	    mesh.vertexNativeToUser = mdVertexNativeToDouble;
	    break;

	default:
	    return 0;
    }

    mesh.tricount = operation->tricount;

    if (mesh.tricount < 2)
	return 0;

    mesh.maxcollapsecost = operation->decimationstrength;

    /* Record start time */
    operation->msecs = (bu_gettime() + 500) / 1000;

    mesh.threadcount = threadcount;
    mesh.operationflags = flags;

    /* Custom vertex attributes besides point position */
    mesh.attribcount = operation->attribcount;
    mesh.attrib = operation->attrib;

    /* Advanced configuration options */
    mesh.compactnesstarget = operation->compactnesstarget;
    mesh.compactnesspenalty = operation->compactnesspenalty;
    mesh.syncstepcount = operation->syncstepcount;

    if (mesh.syncstepcount < 1)
	mesh.syncstepcount = 1;

    if (mesh.syncstepcount > 1024)
	mesh.syncstepcount = 1024;

    mesh.normalsearchangle = cos(1.0 * operation->normalsearchangle * (M_PI / 180.0));

    if (mesh.normalsearchangle > 0.9)
	mesh.normalsearchangle = 0.9;

    /* Synchronization */
    mdBarrierInit(&mesh.workbarrier, threadcount);
    mdBarrierInit(&mesh.globalbarrier, threadcount + 1);

    /* Determine update buffer shift required, find the count of updatebuffers */
    for (mesh.updatebuffershift = 0; (threadcount >> mesh.updatebuffershift) > MD_THREAD_UPDATE_BUFFER_COUNTMAX; mesh.updatebuffershift++);

    mesh.updatebuffercount = ((threadcount - 1) >> mesh.updatebuffershift) + 1;

    /* Runtime picking of collapse penalty computation path */
    mesh.collapsepenalty = mdEdgeCollapsePenaltyTriangle;

#ifdef MD_CONFIG_SSE_SUPPORT
#ifndef MD_CONF_DOUBLE_PRECISION

    if (bu_simd_supported(BU_SIMD_SSE4_1) && (mdPathSSE4p1 & 0x1)) {
	mesh.collapsepenalty = mdEdgeCollapsePenaltyTriangleSSE4p1f;
    } else if (bu_simd_supported(BU_SIMD_SSE3) && (mdPathSSE3 & 0x1)) {
	mesh.collapsepenalty = mdEdgeCollapsePenaltyTriangleSSE3f;
    } else if (bu_simd_supported(BU_SIMD_SSE2) && (mdPathSSE2 & 0x1)) {
	mesh.collapsepenalty = mdEdgeCollapsePenaltyTriangleSSE2f;
    }

#else

    if (bu_simd_supported(BU_SIMD_SSE4_1) && (mdPathSSE4p1 & 0x2)) {
	mesh.collapsepenalty = mdEdgeCollapsePenaltyTriangleSSE4p1d;
    } else if (bu_simd_supported(BU_SIMD_SSE3) && (mdPathSSE3 & 0x2)) {
	mesh.collapsepenalty = mdEdgeCollapsePenaltyTriangleSSE3d;
    } else if (bu_simd_supported(BU_SIMD_SSE2) && (mdPathSSE2 & 0x2)) {
	mesh.collapsepenalty = mdEdgeCollapsePenaltyTriangleSSE2d;
    }

#endif
#endif


    /* Initialize entire mesh storage */
    mesh.vertexalloc = operation->vertexalloc;

    if (mesh.vertexalloc < mesh.vertexcount)
	mesh.vertexalloc = mesh.vertexcount;

    if (!(mdMeshInit(&mesh, operation->maxmemorysize)))
	goto error;

    mesh.updatestatusflag = 0;
    status.progress = 0.0;
    status.trianglecount = 0;

    if (operation->statuscallback) {
	mesh.updatestatusflag = 1;
	mdUpdateStatus(&mesh, 0, MD_STATUS_STAGE_INIT, &status);
	operation->statuscallback(operation->statusopaquepointer, &status);
    }

    /* Launch threads! */
    tinit = threadinit;

    for (threadid = 0; threadid < threadcount; threadid++, tinit++) {
	tinit->threadid = threadid;
	tinit->mesh = &mesh;
	tinit->stage = MD_STATUS_STAGE_INIT;

	if (bu_thrd_create(&thread[threadid], mdThreadMain, tinit) == bu_thrd_error)
	    bu_bomb("bu_thrd_create() failed");
    }

    /* Wait until all threads have properly initialized */
    if (mesh.updatestatusflag)
	mdBarrierSync(&mesh.globalbarrier);

    /* Wait for all threads to reach step 3 */
#if 0 /*ifdef MD_CONF_ENABLE_PROGRESS*/

    if (!(mesh.updatestatusflag))
	mdBarrierSync(&mesh.globalbarrier);
    else {
	for (; !(mdBarrierSyncTimeout(&mesh.globalbarrier, statuswait));) {
	    mdUpdateStatus(&mesh, threadinit, 0, &status);
	    operation->statuscallback(operation->statusopaquepointer, &status);
	}
    }

#else
    mdBarrierSync(&mesh.globalbarrier);
#endif

    /* Build mesh step 3 is not parallel, have the main thread run it */
    mdMeshInitTrirefs(&mesh);

    /* Wake up all threads */
    mdBarrierSync(&mesh.globalbarrier);

    /* Wait for all threads to complete */
#if 0 /*ifdef MD_CONF_ENABLE_PROGRESS*/

    if (!(mesh.updatestatusflag))
	mdBarrierSync(&mesh.globalbarrier);
    else {
	for (; !(mdBarrierSyncTimeout(&mesh.globalbarrier, statuswait));) {
	    mdUpdateStatus(&mesh, threadinit, 0, &status);
	    operation->statuscallback(operation->statusopaquepointer, &status);
	}
    }

    deletioncount = 0;

    for (threadid = 0; threadid < threadcount; threadid++) {
	deletioncount += threadinit[threadid].deletioncount;

	if (bu_thrd_join(thread[threadid], NULL) == bu_thrd_error)
	    bu_bomb("bu_thrd_join() failed");
    }

    status.trianglecount = mesh.tricount - deletioncount;
#else
    mdBarrierSync(&mesh.globalbarrier);

    for (threadid = 0; threadid < threadcount; threadid++) {
	if (bu_thrd_join(thread[threadid], NULL) == bu_thrd_error)
	    bu_bomb("bu_thrd_join() failed");
    }
#endif

    /* Count sums of all threads */
    operation->decimationcount = 0;
    tinit = threadinit;

    for (threadid = 0; threadid < threadcount; threadid++, tinit++)
	operation->decimationcount += tinit->decimationcount;

    if (mesh.updatestatusflag && operation->statuscallback) {
	mdUpdateStatus(&mesh, 0, MD_STATUS_STAGE_STORE, &status);
	operation->statuscallback(operation->statusopaquepointer, &status);
    }

    /* Write out the final mesh */
    if (operation->normalattrib.base)
	mdMeshRecomputeNormals(&mesh, &operation->normalattrib);
    else {
	mdMeshWriteVertices(&mesh, 0, 0);
	mdMeshWriteIndices(&mesh);
    }

    operation->vertexcount = mesh.vertexpackcount;
    operation->tricount = mesh.tripackcount;

    if (mesh.updatestatusflag && operation->statuscallback) {
	mdUpdateStatus(&mesh, 0, MD_STATUS_STAGE_DONE, &status);
	operation->statuscallback(operation->statusopaquepointer, &status);
    }

    /* Free all global data */
error:

    if (!(mesh.operationflags & MD_FLAGS_NO_DECIMATION))
	mdMeshHashEnd(&mesh);

    mdMeshEnd(&mesh);
    mdBarrierDestroy(&mesh.workbarrier);
    mdBarrierDestroy(&mesh.globalbarrier);

    /* Store total processing time */
    operation->msecs = ((bu_gettime() + 500) / 1000) - operation->msecs;

    return 1;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
