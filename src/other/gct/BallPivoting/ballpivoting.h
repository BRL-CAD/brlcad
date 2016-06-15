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



/* Define to use double floating point precision */
/*
#define BP_CONF_DOUBLE_PRECISION
*/



#ifdef BP_CONF_DOUBLE_PRECISION
typedef double bpf;
#else
typedef float bpf;
#endif



////



typedef struct
{
  int orphancount;
  int hingelostcount;
  int conflictcount;
  int pointdupcount;
} bpInfo;


typedef struct
{
  double progress;
  int stage;
  const char *stagename;
  long trianglecount;
  long triangleguess;
} bpStatus;

enum
{
  BP_STATUS_STAGE_INIT,
  BP_STATUS_STAGE_SORT,
  BP_STATUS_STAGE_MESH,
  BP_STATUS_STAGE_STRIP,
  BP_STATUS_STAGE_CONNECT,
  BP_STATUS_STAGE_CLEAN,
  BP_STATUS_STAGE_NORMALS,
  BP_STATUS_STAGE_PACK,
  BP_STATUS_STAGE_STORE,
  BP_STATUS_STAGE_DONE,

  BP_STATUS_STAGE_COUNT
};



typedef struct
{
  /* Input format configuration */
  size_t vertexcount;
  bpf *vertex;
  size_t vertexstride;
  bpf *normal;
  size_t normalstride;

  /* Ball settings */
  bpf ballradiusmin;
  bpf ballradiusmax;

  /* Ouput format configuration */
  int trianglewinding; /* BP_TRIANGLE_WINDING_CW or BP_TRIANGLE_WINDING_CCW */
  size_t trianglestride; /* Stride between triangles for indices */
  size_t triangleindexsize; /* Size of index integers : 1, 2, 4 or 8 ; a value of 0 defaults to 4 */
  size_t triangleoffset; /* Offset within triangle entry where to store indices sequentially */
  void *(*trianglealloc)( void *allocopaque, size_t size );
  void *allocopaque;

  /* Vertex packing callback */
  void *packopaquepointer;
  void (*packcallback)( void *opaquepointer, long newvertexindex, long oldvertexindex );

  /* Status callback */
  long statusmiliseconds;
  void *statusopaquepointer;
  void (*statuscallback)( void *opaquepointer, const bpStatus *status );

  /* Advanced configuration options */
  int hashextrabits;
  int hashlockpageshift;
  bpf ballradiuspivot;
  bpf ballradiusoverride;
  bpf balldistmin; /* Minimum distance away from triangle's surface plane, 0.0 can cause glitches in very rare cases */
  bpf pivotcosphericalrange;
  bpf pivotcoplanardist;
  bpf pivotjoinradiusfactor;
  bpf pivotforceradiusfactor;
  bpf pivotoverriderange;
  bpf dupvertexdistancesoft; /* Discard any vertex closer than this distance to another vertex, stored as ratio of default ball size */
  bpf dupvertexdistancehard;
  bpf dupvertexnormalagreement;
  bpf reductionfactor;
  bpf seedballradius;
  bpf seedcoplanardist;
  bpf seednormalagreement;
  bpf seednormalagreementsum;
  bpf seedmarkradius;
  bpf hingenormalagreement;
  bpf pivotnormalagreementsoft; /* Normal agreement betwene vertices and created triangles */
  bpf pivotnormalagreementhard;
  bpf pivotnormalagreementsumsoft;
  bpf pivotnormalagreementsumhard;
  bpf filterplanecutdistance;
  long randseed;
  size_t maxmemorysize;

} bpOperation;

enum
{
  BP_TRIANGLE_WINDING_CW,
  BP_TRIANGLE_WINDING_CCW
};


typedef struct
{
  size_t trianglecount;
  size_t vertexcount; /* Final count of used vertices, if BP_FLAGS_REBUILD_VERTICES was specified */

  void *triangle; /* Allocated triangle data */

  /* Process information sum */
  bpInfo info;

  int warningcount;
  int errorcount;
  long msecs;

} bpOutput;


#define BP_FLAGS_NORMALIZE_NORMALS (0x1)
#define BP_FLAGS_PACK_VERTICES (0x2)
#define BP_FLAGS_ALLOW_REDUCTION (0x4)
#define BP_FLAGS_RECOMPUTE_NORMALS (0x8)
#define BP_FLAGS_SKIP_STRIP_PASS (0x10)
#define BP_FLAGS_SKIP_CONNECT_PASS (0x20)
#define BP_FLAGS_SKIP_CLEAN_PASS (0x40)



void bpOperationInit( bpOperation *op );
void bpOperationSetInput( bpOperation *op, size_t vertexcount, bpf *vertex, size_t vertexstride, bpf *normal, size_t normalstride );
void bpOperationSetBall( bpOperation *op, bpf ballradius );
void bpOperationSetBallRange( bpOperation *op, bpf ballminimum, bpf ballmaximum );
int bpOperationSetOutput( bpOperation *op, int trianglewinding, size_t trianglestride, size_t triangleindexsize, size_t triangleoffset, void *(*trianglealloc)( void *allocopaque, size_t size ), void *allocopaque );
void bpOperationNormalAgreement( bpOperation *op, bpf softstrictness, bpf hardstrictness );
void bpOperationVertexPackCallback( bpOperation *op, void (*packcallback)( void *opaquepointer, long newvertexindex, long oldvertexindex ), void *packopaquepointer );
void bpOperationStatusCallback( bpOperation *op, void (*statuscallback)( void *opaquepointer, const bpStatus *status ), void *opaquepointer, long miliseconds );


void *bpConstructMesh( bpOperation *op, bpOutput *output, int threadcount, uint32_t flags );




#ifdef __cplusplus
}
#endif

