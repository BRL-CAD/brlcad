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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>

#ifdef __WIN32__
 #include <windows.h>
#endif


#include "cpuconfig.h"

#include "cc.h"
#include "mm.h"
#include "mmhash.h"
#include "mmbitmap.h"
#include "math3d.h"

#include "ballpivoting.h"



/********/




/*
#define DEBUG_VERBOSE

#define DEBUG_VERBOSE_PICK
#define DEBUG_VERBOSE_GRID
#define DEBUG_VERBOSE_SEED
#define DEBUG_VERBOSE_PIVOT
#define DEBUG_VERBOSE_PIVOTMULTI
#define DEBUG_VERBOSE_FILTER
#define DEBUG_VERBOSE_HINGE
#define DEBUG_VERBOSE_STRIP
#define DEBUG_VERBOSE_CONNECT
#define DEBUG_VERBOSE_MEMORY
#define DEBUG_VERBOSE_FAILURE
*/
/*
#define DEBUG_VERBOSE_WARNING
#define DEBUG_VERBOSE_ERROR
*/
/*
#define DEBUG_VERBOSE_SPHERE
*/
/*
#define DEBUG_PROGRESS
*/



/********/



/* Configuration */

typedef int32_t bpi;
#define BP_SIZEOF_BPI (4)



#ifdef BP_CONF_DOUBLE_PRECISION

 #define bpfmin(x,y) fmin((x),(y))
 #define bpfmax(x,y) fmax((x),(y))
 #define bpffloor(x) floor(x)
 #define bpfceil(x) ceil(x)
 #define bpfround(x) round(x)
 #define bpfsqrt(x) sqrt(x)
 #define bpfcbrt(x) cbrt(x)
 #define bpfabs(x) fabs(x)
 #define bpflog2(x) log2(x)

#else

 #define bpfmin(x,y) fminf((x),(y))
 #define bpfmax(x,y) fmaxf((x),(y))
 #define bpffloor(x) floorf(x)
 #define bpfceil(x) ceilf(x)
 #define bpfround(x) roundf(x)
 #define bpfsqrt(x) sqrtf(x)
 #define bpfcbrt(x) cbrtf(x)
 #define bpfabs(x) fabsf(x)
 #define bpflog2(x) log2f(x)

#endif


#define BP_CLAMP(x,min,max) ((x)<(min)?(min):((x)>(max)?(max):(x)))





/*
#define CONFIG_PIVOT_COSPHERICAL (0.1)
#define CONFIG_PIVOT_COSPHERICAL (0.001)
#define CONFIG_PIVOT_COSPHERICAL (0.0001)
#define CONFIG_PIVOT_COSPHERICAL (0.00001)
#define CONFIG_PIVOT_COSPHERICAL (0.05)
#define CONFIG_PIVOT_COSPHERICAL (0.15)
#define CONFIG_PIVOT_COSPHERICAL (0.05)
#define CONFIG_PIVOT_COSPHERICAL (0.20)
*/

/*
#define CONFIG_PIVOT_COSPHERICAL (0.01)
*/
#define CONFIG_PIVOT_COSPHERICAL (0.01)




#define CONFIG_PIVOT_COSPHERICAL_OVERRIDE (0.05)
/*
#define CONFIG_PIVOT_COSPHERICAL_OVERRIDE (0.0)
*/



#define CONF_COLLINEAR_MAGNITUDE_MIN (0.000001)




/*
#define CONFIG_SPHERE_COPLANAR_FACTOR (0.001)
*/
#define CONFIG_SPHERE_COPLANAR_FACTOR (0.0001)





/*
#define CONFIG_SPHERE_PLANAR_TEST
*/
/*
#define CONFIG_SPHERE_JOIN_PRIORITY
*/

/*
#define CONFIG_ENABLE_JOINCOUNT_JOIN_OVERRIDE
*/
/*
#define CONFIG_ENABLE_JOINCOUNT_FORCE_OVERRIDE
*/

#define CONFIG_ENABLE_HINGE_ORPHAN_REBUILD


#define CONFIG_ENABLE_HINGE_ORPHAN_JOIN


#define CONFIG_ENABLE_PIVOT_NORMAL_PRIORITIZATION


/*
#define CONFIG_ENABLE_FILTER_CUT_ONLY_MIDPLANE
*/
/*
#define CONFIG_ENABLE_JOIN_NO_FILTERING
*/

#define CONFIG_ENABLE_OVERRIDE_VALIDATION

#define CONFIG_ENABLE_GRADE_OVERRIDE

/*
#define CONFIG_ENABLE_CONFLICT_PIVOT
*/

#define CONFIG_ENABLE_PIVOT_COSPHERICAL_ONLY


#define CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE


#define CONFIG_ENABLE_BAD_GRADE_GENERATE_CONNECT



/* Conflict depth maximum can't be set higher than 255 unless you fix some hinge flags storage stuff */
#define BP_CONFLICTDEPTH_MAX (0xf)
#define BP_CONFLICTDEPTH_LOST_INCREMENT (8)
#define BP_CONFLICTDEPTH_CHECK_FORCEOVERRIDE(c) ((c)<0xC)

#define BP_CONFLICTDEPTH_CHECK_NORMAL(c) ((c)<0x4)
#define BP_CONFLICTDEPTH_CHECK_PIVOT(c) ((c)<0x8)
#define BP_CONFLICTDEPTH_CHECK_PLANAR(c) (1)

#define BP_CONFLICTDEPTH_CHECK_OVERLAP(c) ((c)>=0x4)



#define BP_BALL_COUNT_MAX (16)


#define BP_CONFIG_FAILURE_MAX_COUNT (16)


#define BP_CONFIG_ENABLE_PROGRESS



/********/



#define BP_AXIS_X (0)
#define BP_AXIS_Y (1)
#define BP_AXIS_Z (2)
#define BP_AXIS_COUNT (3)


#ifdef CPUCONF_CORES_COUNT
 #define BP_THREAD_COUNT_DEFAULT CPUCONF_CORES_COUNT
#else
 #define BP_THREAD_COUNT_DEFAULT (4)
#endif
#define BP_THREAD_COUNT_MAX (64)



#if defined(__GNUC__)
 #define FORCEINLINE __attribute__((always_inline))
#else
 #define FORCEINLINE
#endif



/********/



typedef struct
{
  mtMutex mutex;
  mtSignal signal;
  int resetcount;
  volatile int index;
  volatile int count[2];
} bpBarrier;

static void bpBarrierInit( bpBarrier *barrier, int count )
{
  mtMutexInit( &barrier->mutex );
  mtSignalInit( &barrier->signal );
  barrier->resetcount = count;
  barrier->index = 0;
  barrier->count[0] = count;
  barrier->count[1] = count;
  return;
}

static void bpBarrierDestroy( bpBarrier *barrier )
{
  mtMutexDestroy( &barrier->mutex );
  mtSignalDestroy( &barrier->signal );
  return;
}

static int bpBarrierSync( bpBarrier *barrier )
{
  int index, ret;
  mtMutexLock( &barrier->mutex );
  index = barrier->index;
  ret = 0;
  if( !( --barrier->count[index] ) )
  {
    ret = 1;
    mtSignalBroadcast( &barrier->signal );
    index ^= 1;
    barrier->index = index;
    barrier->count[index] = barrier->resetcount;
  }
  else
  {
    for( ; barrier->count[index] ; )
      mtSignalWait( &barrier->signal, &barrier->mutex );
  }
  mtMutexUnlock( &barrier->mutex );
  return ret;
}

static int bpBarrierSyncTimeout( bpBarrier *barrier, long miliseconds )
{
  int index, ret;
  mtMutexLock( &barrier->mutex );
  index = barrier->index;
  ret = 0;
  if( !( --barrier->count[index] ) )
  {
    ret = 1;
    mtSignalBroadcast( &barrier->signal );
    index ^= 1;
    barrier->index = index;
    barrier->count[index] = barrier->resetcount;
  }
  else
  {
    mtSignalWaitTimeout( &barrier->signal, &barrier->mutex, miliseconds );
    if( !( barrier->count[index] ) )
      ret = 1;
    else
      barrier->count[index]++;
  }
  mtMutexUnlock( &barrier->mutex );
  return ret;
}



/****/



typedef struct
{
  bpi vertexindex;
  bpf distsquared;
} bpClusterVertex;

typedef struct
{
  size_t count;
  size_t alloc;
  bpClusterVertex *vertex;
  bpClusterVertex *vertexextra;
} bpCluster;

static void bpClusterInit( bpCluster *cluster, int prealloc )
{
  size_t memsize;
  cluster->count = 0;
  cluster->alloc = prealloc;
  memsize = prealloc * sizeof(bpClusterVertex);
  cluster->vertex = malloc( 2 * memsize );
  cluster->vertexextra = ADDRESS( cluster->vertex, memsize );
  return;
}

static void bpClusterGrow( bpCluster *cluster, int count )
{
  size_t memsize;
  cluster->alloc = count;
  memsize = count * sizeof(bpClusterVertex);
  cluster->vertex = realloc( cluster->vertex, 2 * memsize );
  cluster->vertexextra = ADDRESS( cluster->vertex, memsize );
  return;
}

static void bpClusterFree( bpCluster *cluster )
{
  free( cluster->vertex );
  cluster->vertex = 0;
  cluster->vertexextra = 0;
  cluster->alloc = 0;
  cluster->count = 0;
  return;
}




/********/



/* Try to aim for 64 bytes struct padding */
#if CPUCONF_WORD_SIZE == 64
 #define BP_CELL_VERTEX_COUNT (16-3)
#else
 #define BP_CELL_VERTEX_COUNT (16-2)
#endif


/* This is so suboptimal compared to the atomic operation solution */
#ifndef MM_ATOMIC_SUPPORT
 #define BP_CELL_GROUP_AXIS_SHIFT (1)
 #define BP_CELL_GROUP_SHIFT (BP_AXIS_COUNT*BP_CELL_GROUP_AXIS_SHIFT)
 #define BP_CELL_GROUP_SIZE (1<<BP_CELL_GROUP_SHIFT)
#endif


typedef struct
{
  void *next;
  uint32_t count;
  volatile bpi vertex[BP_CELL_VERTEX_COUNT];
} bpGridCell;

typedef struct
{
  int size[BP_AXIS_COUNT];
  int stride[BP_AXIS_COUNT];
  int max[BP_AXIS_COUNT];
  bpf origin[BP_AXIS_COUNT];
  bpf range[BP_AXIS_COUNT];
  bpf scale[BP_AXIS_COUNT];
  size_t memsize;

#ifdef MM_ATOMIC_SUPPORT
  mmAtomic32 globallock;
  mmAtomicP cell[];
#else
  mtMutex globalmutex;
  int groupcount;
  int groupstride[BP_AXIS_COUNT];
  mtMutex *groupmutex;
  bpGridCell *cell[];
#endif

} bpGrid;

/* Assuming regular distribution, aim for X vertices per cell of 3D grid */
#define BP_GRID_VERTEXPERCELL_MIN (1.0)
#define BP_GRID_VERTEXPERCELL_MAX (16.0)

#define BP_GRID_SIZE_FACTOR (0.3)



typedef struct
{
#ifdef MM_ATOMIC_SUPPORT
  mmAtomic32 atomiclock;
#else
  mtMutex mutex;
#endif
} bpLock MM_CACHE_ALIGN;

typedef struct
{
  bpLock *lock;
  long lockcount;
  long lockmask;
  bpf origin[BP_AXIS_COUNT];
  bpf scale[BP_AXIS_COUNT];
} bpPointLockMap;

#define BP_POINT_LOCK_INDEX_NONE (-(long)1)


typedef struct
{
  ssize_t limit[6];
  int globalflag;
} bpSpaceLock;

typedef struct
{
  void *trilist;
  bpLock lock;
} bpSpaceCell;

typedef struct
{
  bpSpaceCell *cell;
  int size[BP_AXIS_COUNT];
  int max[BP_AXIS_COUNT];
  int stride[BP_AXIS_COUNT];
  bpf origin[BP_AXIS_COUNT];
  bpf scale[BP_AXIS_COUNT];
  size_t memsize;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomic32 globallock;
#else
  mtMutex globalmutex;
#endif
} bpSpaceMap;

#define BP_SPACE_MAP_VERTEXPERCELL_MIN (1.0)
#define BP_SPACE_MAP_VERTEXPERCELL_MAX (16.0)

#define BP_SPACE_MAP_SIZE_FACTOR (0.4)


typedef struct
{
  int threadid;
  int flags;

  /* Allocated anywhere */
  mmBlockHead cellblock;

  /* Allocated on NUMA node! */
  mmBlockHead hingeblock;
  mmBlockHead triangleblock;
  mmBlockHead pathblock;

  /* Seed search */
  bpi seedvertexbase;
  bpi seedvertexindex;

  /* Count of vertices added to grid by thread */
  size_t activevertexcount;

  /* Count of triangles in thread's mesh */
  volatile size_t statustricount;

  /* Cluster vertex buffer */
  bpCluster cluster;

  /* Triangle mesh : write count of triangles to be read by main thread */
  size_t trianglecount;
  /* Triangle mesh : triangle store address to be read once given by main thread */
  void *trianglestore;
  size_t triangleindexsize;

  /* Very quick tiny random number generator state */
  ccQuickRandState32 randstate;

  /* Output information */
  bpInfo info;

} bpThreadData;

#define BP_THREAD_DATA_FLAGS_NOSEED (0x1)


typedef struct
{
  int threadcount;
  uint32_t operationflags;
  int updatestatusflag;

  /* Input data */
  size_t vertexcount;
  bpf *vertex;
  size_t vertexstride;
  bpf *normal;
  size_t normalstride;

  /* Output data */
  int trianglewinding;
  size_t trianglestride;

  /* Ball settings */
  bpf ballradiusmin;
  bpf ballradiusmax;
  int ballcount;
  bpf ballradius[BP_BALL_COUNT_MAX];

  mmBitMap vertexmap;  /* Bitmap to track tested and used vertices */
  mmVolumeHead volume; /* Global memory volume */
  bpGrid *grid; /* Voxel sort grid */

  /* Synchronization stuff */
  bpBarrier workbarrier;
  bpBarrier globalbarrier;
  mtMutex syncmutex;
  mtSignal syncsignal;
  volatile int syncmeshworkcount;
  volatile int syncbuildflag;
  volatile int syncstoreflag;
  size_t activevertexcount;
  size_t packedvertexcount;

  /* Hash table to track registered hinges */
  void *hingehashtable;

  /* Point lock map for normal smoothing */
  bpPointLockMap pointlockmap;

  /* Spatial lock map for exclusive multi-hits filtering */
  bpSpaceMap spacemap;

  /* Vertex packing, index redirection table */
  bpi *redirectiontable;
  void *packopaquepointer;
  void (*packcallback)( void *opaquepointer, long newvertexindex, long oldvertexindex );

  /* Advanced configuration settings */
  int hashlockpageshift;
  bpf ballradiuspivot;
  bpf ballradiusoverride;
  bpf balldistmin;
  bpf pivotcosphericalrange;
  bpf pivotcoplanardist;
  bpf pivotjoinradiusfactor;
  bpf pivotforceradiusfactor;
  bpf pivotoverriderange;
  bpf dupvertexdistancesoft;
  bpf dupvertexdistancehard;
  bpf dupvertexnormalagreement;
  bpf reductionfactor;
  bpf seedballradius;
  bpf seedcoplanardist;
  bpf seednormalagreement;
  bpf seednormalagreementsum;
  bpf seedmarkradius;
  bpf hingenormalagreement;
  bpf pivotnormalagreementsoft;
  bpf pivotnormalagreementhard;
  bpf pivotnormalagreementsumsoft;
  bpf pivotnormalagreementsumhard;
  bpf filterplanecutdistance;

} bpMainData;


#define BP_GRID_CHECK_SUBGRID(p) (((uintptr_t)(p))&0x2)
#define BP_GRID_GET_SUBGRID(p) (bpGrid *)(((uintptr_t)(p))&~0x2)
#define BP_GRID_GET_CELL(p) (bpGridCell *)(((uintptr_t)(p))&~0x0)

#define BP_GRID_SET_SUBGRID(p) (void *)(((uintptr_t)(p))|0x2)
#define BP_GRID_SET_CELL(p) (void *)(((uintptr_t)(p))|0x0)



/********/



#define BP_TRIANGLE_PER_BUNDLE (16384)

typedef struct
{
  bpi v[3];
  mmListNode list;
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
  bpf plane[4];
#endif
} bpTriangle;

typedef struct
{
  bpi v[3];
  mmListNode list;
} bpTriangleSeed;

typedef struct
{
  int count;
  bpTriangle triangle[BP_TRIANGLE_PER_BUNDLE];
  mmListNode list;
} bpTriangleBundle;

typedef struct
{
  size_t trianglecount;
  void *trianglelist;
  void *seedlist;
} bpMesh;



/********/



#define BP_GRID_LIMIT_EXTEND (0.001)

static void bpSetGridLimits( bpGrid *grid, bpf *limit )
{
  grid->range[BP_AXIS_X] = limit[1] - limit[0];
  grid->range[BP_AXIS_Y] = limit[3] - limit[2];
  grid->range[BP_AXIS_Z] = limit[5] - limit[4];
  grid->origin[BP_AXIS_X] = limit[0] - ( BP_GRID_LIMIT_EXTEND * grid->size[BP_AXIS_X] );
  grid->origin[BP_AXIS_Y] = limit[2] - ( BP_GRID_LIMIT_EXTEND * grid->size[BP_AXIS_Y] );
  grid->origin[BP_AXIS_Z] = limit[4] - ( BP_GRID_LIMIT_EXTEND * grid->size[BP_AXIS_Z] );
  grid->range[BP_AXIS_X] *= ( 1.0 + 2.0 * BP_GRID_LIMIT_EXTEND );
  grid->range[BP_AXIS_Y] *= ( 1.0 + 2.0 * BP_GRID_LIMIT_EXTEND );
  grid->range[BP_AXIS_Z] *= ( 1.0 + 2.0 * BP_GRID_LIMIT_EXTEND );
  grid->scale[BP_AXIS_X] = (bpf)grid->size[BP_AXIS_X] / grid->range[BP_AXIS_X];
  grid->scale[BP_AXIS_Y] = (bpf)grid->size[BP_AXIS_Y] / grid->range[BP_AXIS_Y];
  grid->scale[BP_AXIS_Z] = (bpf)grid->size[BP_AXIS_Z] / grid->range[BP_AXIS_Z];
  return;
}


static bpGrid *bpGridAlloc( bpMainData *bpdata, int sizex, int sizey, int sizez, bpf *limit )
{
  size_t cellcount, memsize, cellrange;
  bpGrid *grid;
#ifndef MM_ATOMIC_SUPPORT
  size_t groupsizex, groupsizey, groupsizez;
  size_t groupcount;
  mtMutex *groupmutex;
#endif

  cellcount = sizex * sizey * sizez;
  cellrange = cellcount * sizeof(mmAtomicP);
  memsize = sizeof(bpGrid) + cellrange;
  grid = mmAlignRelayAlloc( (void *)mmVolumeAlloc, &bpdata->volume, memsize, 0x40, 0x0 );
  grid->size[BP_AXIS_X] = sizex;
  grid->size[BP_AXIS_Y] = sizey;
  grid->size[BP_AXIS_Z] = sizez;
  grid->stride[BP_AXIS_X] = 1;
  grid->stride[BP_AXIS_Y] = sizex;
  grid->stride[BP_AXIS_Z] = sizex * sizey;
  grid->max[BP_AXIS_X] = sizex - 1;
  grid->max[BP_AXIS_Y] = sizey - 1;
  grid->max[BP_AXIS_Z] = sizez - 1;
  grid->memsize = memsize;
  memset( grid->cell, 0, cellrange );

#ifdef MM_ATOMIC_SUPPORT
  mmAtomicWrite32( &grid->globallock, 0x0 );
#else
  mtMutexInit( &grid->globalmutex );
  groupsizex = sizex >> BP_CELL_GROUP_AXIS_SHIFT;
  groupsizey = sizey >> BP_CELL_GROUP_AXIS_SHIFT;
  groupsizez = sizez >> BP_CELL_GROUP_AXIS_SHIFT;
  grid->groupstride[BP_AXIS_X] = 1;
  grid->groupstride[BP_AXIS_Y] = groupsizex;
  grid->groupstride[BP_AXIS_Z] = groupsizex * groupsizey;
  grid->groupcount = groupsizex * groupsizey * groupsizez;
  grid->groupmutex = mmAlignRelayAlloc( (void *)mmVolumeAlloc, &bpdata->volume, grid->groupcount * sizeof(mtMutex), 0x40, 0x0 );
  groupmutex = grid->groupmutex;
  for( groupcount = grid->groupcount ; groupcount ; groupcount--, groupmutex++ )
    mtMutexInit( groupmutex );
#endif

  bpSetGridLimits( grid, limit );

  return grid;
}


/* We finished building the grid, free up stuff that we only needed to build it */
static void bpGridFinish( bpMainData *bpdata, bpGrid *grid )
{
#ifndef MM_ATOMIC_SUPPORT
  size_t groupcount;
  mtMutex *groupmutex;

  mtMutexDestroy( &grid->globalmutex );
  groupmutex = grid->groupmutex;
  for( groupcount = grid->groupcount ; groupcount ; groupcount--, groupmutex++ )
    mtMutexDestroy( groupmutex );
  mmAlignRelayFree( (void *)mmVolumeFree, &bpdata->volume, grid->groupmutex, grid->groupcount * sizeof(mtMutex) );
  grid->groupmutex = 0;
#endif
  return;
}


static void bpGridFree( bpMainData *bpdata, bpGrid *grid )
{
  mmAlignRelayFree( (void *)mmVolumeFree, &bpdata->volume, grid, grid->memsize );
  return;
}


#define BP_GRID_CELL_LOCK(c) (bpGridCell *)(((uintptr_t)c)|((uintptr_t)0x1))
#define BP_GRID_CELL_UNLOCK(c) (bpGridCell *)(((uintptr_t)c)&(~(uintptr_t)0x1))
#define BP_GRID_CELL_ISLOCKED(c) (((uintptr_t)c)&((uintptr_t)0x1))
#define BP_GRID_CELL_POINTER(c) (bpGridCell *)(((uintptr_t)c)&(~(uintptr_t)0x1))


static int bpGridLockForce( bpGrid *grid, ssize_t *limit )
{
  ssize_t x, y, z, index, rowy, rowz;
  bpGridCell *cell;

#ifdef MM_ATOMIC_SUPPORT
  index = ( limit[0] * grid->stride[BP_AXIS_X] ) + ( limit[2] * grid->stride[BP_AXIS_Y] ) + ( limit[4] * grid->stride[BP_AXIS_Z] );
  rowy = grid->stride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->stride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->stride[BP_AXIS_Y] );
#else
  index = ( limit[0] * grid->groupstride[BP_AXIS_X] ) + ( limit[2] * grid->groupstride[BP_AXIS_Y] ) + ( limit[4] * grid->groupstride[BP_AXIS_Z] );
  rowy = grid->groupstride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->groupstride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->groupstride[BP_AXIS_Y] );
#endif

  for( z = limit[4] ; z <= limit[5] ; z++ )
  {
    for( y = limit[2] ; y <= limit[3] ; y++ )
    {
      for( x = limit[0] ; x <= limit[1] ; x++, index++ )
      {
#ifdef MM_ATOMIC_SUPPORT
        for( ; ; )
        {
          cell = mmAtomicReadP( &grid->cell[index] );
          if( BP_GRID_CELL_ISLOCKED(cell) )
            continue;
          if( mmAtomicCmpXchgP( &grid->cell[index], cell, BP_GRID_CELL_LOCK(cell) ) == cell )
            break;
        }
#else
        mtMutexLock( &grid->groupmutex[index] );
#endif
      }
      index += rowy;
    }
    index += rowz;
  }

  return 1;
}

static int bpGridLock( bpGrid *grid, ssize_t *limit, size_t *lockfail )
{
  ssize_t x, y, z, index, rowy, rowz;
  bpGridCell *cell;

#ifdef MM_ATOMIC_SUPPORT
  index = ( limit[0] * grid->stride[BP_AXIS_X] ) + ( limit[2] * grid->stride[BP_AXIS_Y] ) + ( limit[4] * grid->stride[BP_AXIS_Z] );
  rowy = grid->stride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->stride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->stride[BP_AXIS_Y] );
#else
  index = ( limit[0] * grid->groupstride[BP_AXIS_X] ) + ( limit[2] * grid->groupstride[BP_AXIS_Y] ) + ( limit[4] * grid->groupstride[BP_AXIS_Z] );
  rowy = grid->groupstride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->groupstride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->groupstride[BP_AXIS_Y] );
#endif

  for( z = limit[4] ; z <= limit[5] ; z++ )
  {
    for( y = limit[2] ; y <= limit[3] ; y++ )
    {
      for( x = limit[0] ; x <= limit[1] ; x++, index++ )
      {
#ifdef MM_ATOMIC_SUPPORT
        cell = mmAtomicReadP( &grid->cell[index] );
        if( ( BP_GRID_CELL_ISLOCKED(cell) ) || ( mmAtomicCmpXchgP( &grid->cell[index], cell, BP_GRID_CELL_LOCK(cell) ) != cell ) )
        {
          *lockfail = index;
          return 0;
        }
#else
        if( !( mtMutexTryLock( &grid->groupmutex[index] ) ) )
        {
          *lockfail = index;
          return 0;
        }
#endif
      }
      index += rowy;
    }
    index += rowz;
  }

  return 1;
}

static void bpGridUnlock( bpGrid *grid, ssize_t *limit, size_t lockfail )
{
  size_t index;
  ssize_t x, y, z, rowy, rowz;
  bpGridCell *cell;

#ifdef MM_ATOMIC_SUPPORT
  index = ( limit[0] * grid->stride[BP_AXIS_X] ) + ( limit[2] * grid->stride[BP_AXIS_Y] ) + ( limit[4] * grid->stride[BP_AXIS_Z] );
  rowy = grid->stride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->stride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->stride[BP_AXIS_Y] );
#else
  index = ( limit[0] * grid->groupstride[BP_AXIS_X] ) + ( limit[2] * grid->groupstride[BP_AXIS_Y] ) + ( limit[4] * grid->groupstride[BP_AXIS_Z] );
  rowy = grid->groupstride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->groupstride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->groupstride[BP_AXIS_Y] );
#endif

  for( z = limit[4] ; z <= limit[5] ; z++ )
  {
    for( y = limit[2] ; y <= limit[3] ; y++ )
    {
      for( x = limit[0] ; x <= limit[1] ; x++, index++ )
      {
        if( index == lockfail )
          return;
#ifdef MM_ATOMIC_SUPPORT
        cell = mmAtomicReadP( &grid->cell[index] );
        mmAtomicWriteP( &grid->cell[index], BP_GRID_CELL_UNLOCK(cell) );
#else
        index = ( x * grid->groupstride[BP_AXIS_X] ) + ( y * grid->groupstride[BP_AXIS_Y] ) + ( z * grid->groupstride[BP_AXIS_Z] );
        mtMutexUnlock( &grid->groupmutex[index] );
#endif
      }
      index += rowy;
    }
    index += rowz;
  }

  return;
}


static int bpGridCellDuplicateCheck( bpMainData *bpdata, bpGrid *grid, ssize_t *limit, bpi vertexindex, bpf dupdistsqsoft, bpf dupdistsqhard )
{
  bpi vertexcmpindex;
  ssize_t x, y, z, cellindex, rowy, rowz;
  int32_t index, count;
  bpf *vertex, *vertexcmp, *normal, *normalcmp;
  bpf vecta[BP_AXIS_COUNT], distsquared;
  bpGridCell *cell;

  vertex = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
  normal = ADDRESS( bpdata->normal, vertexindex * bpdata->normalstride );
  cellindex = ( limit[0] * grid->stride[BP_AXIS_X] ) + ( limit[2] * grid->stride[BP_AXIS_Y] ) + ( limit[4] * grid->stride[BP_AXIS_Z] );
  rowy = grid->stride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->stride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->stride[BP_AXIS_Y] );

  for( z = limit[4] ; z <= limit[5] ; z++ )
  {
    for( y = limit[2] ; y <= limit[3] ; y++ )
    {
      for( x = limit[0] ; x <= limit[1] ; x++, cellindex++ )
      {
#ifdef MM_ATOMIC_SUPPORT
        cell = BP_GRID_CELL_POINTER( mmAtomicReadP( &grid->cell[cellindex] ) );
#else
        cell = grid->cell[cellindex];
#endif
        for( ; cell ; cell = cell->next )
        {
          count = cell->count;
          for( index = 0 ; index < count ; index++ )
          {
            vertexcmpindex = cell->vertex[index];
            vertexcmp = ADDRESS( bpdata->vertex, vertexcmpindex * bpdata->vertexstride );
            normalcmp = ADDRESS( bpdata->normal, vertexcmpindex * bpdata->normalstride );
            M3D_VectorSubStore( vecta, vertexcmp, vertex );
            distsquared = vecta[0]*vecta[0] + vecta[1]*vecta[1] + vecta[2]*vecta[2];
            if( ( distsquared < dupdistsqsoft ) && ( ( distsquared < dupdistsqhard ) || ( M3D_VectorDotProduct( normal, normalcmp ) > bpdata->dupvertexnormalagreement ) ) )
            {
#ifdef DEBUG_VERBOSE_GRID
printf( "    Discard Duplicate Vertex ( %f < %f )\n", M3D_VectorMagnitude(vecta), bpfsqrt(dupdistsq) );
#endif
              return 1;
            }
          }
        }
      }
      cellindex += rowy;
    }
    cellindex += rowz;
  }

  return 0;
}


static int bpGridAddVertex( bpMainData *bpdata, bpThreadData *tdata, bpGrid *grid, bpi vertexindex )
{
  int retvalue;
  size_t cellindex, lockfail;
  ssize_t index[BP_AXIS_COUNT], limit[6];
  bpf dupdistsqsoft, dupdistsqhard;
  bpf offset[BP_AXIS_COUNT];
  bpGridCell *cell, *newcell;
  bpf *vertex;
#ifndef MM_ATOMIC_SUPPORT
  ssize_t locklimit[6];
#endif

  vertex = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
  offset[BP_AXIS_X] = vertex[BP_AXIS_X] - grid->origin[BP_AXIS_X];
  offset[BP_AXIS_Y] = vertex[BP_AXIS_Y] - grid->origin[BP_AXIS_Y];
  offset[BP_AXIS_Z] = vertex[BP_AXIS_Z] - grid->origin[BP_AXIS_Z];

  index[BP_AXIS_X] = (size_t)bpffloor( offset[BP_AXIS_X] * grid->scale[BP_AXIS_X] );
  index[BP_AXIS_Y] = (size_t)bpffloor( offset[BP_AXIS_Y] * grid->scale[BP_AXIS_Y] );
  index[BP_AXIS_Z] = (size_t)bpffloor( offset[BP_AXIS_Z] * grid->scale[BP_AXIS_Z] );

  limit[0] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] - bpdata->dupvertexdistancesoft ) * grid->scale[BP_AXIS_X] );
  limit[1] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] + bpdata->dupvertexdistancesoft ) * grid->scale[BP_AXIS_X] );
  limit[2] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] - bpdata->dupvertexdistancesoft ) * grid->scale[BP_AXIS_Y] );
  limit[3] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] + bpdata->dupvertexdistancesoft ) * grid->scale[BP_AXIS_Y] );
  limit[4] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] - bpdata->dupvertexdistancesoft ) * grid->scale[BP_AXIS_Z] );
  limit[5] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] + bpdata->dupvertexdistancesoft ) * grid->scale[BP_AXIS_Z] );

#ifdef DEBUG_VERBOSE_GRID
printf( "  Sort Vertex %d\n", (int)vertexindex );
#endif

  limit[0] = BP_CLAMP( limit[0], 0, grid->max[BP_AXIS_X] );
  limit[1] = BP_CLAMP( limit[1], 0, grid->max[BP_AXIS_X] );
  limit[2] = BP_CLAMP( limit[2], 0, grid->max[BP_AXIS_Y] );
  limit[3] = BP_CLAMP( limit[3], 0, grid->max[BP_AXIS_Y] );
  limit[4] = BP_CLAMP( limit[4], 0, grid->max[BP_AXIS_Z] );
  limit[5] = BP_CLAMP( limit[5], 0, grid->max[BP_AXIS_Z] );

  index[BP_AXIS_X] = BP_CLAMP( index[BP_AXIS_X], limit[0], limit[1] );
  index[BP_AXIS_Y] = BP_CLAMP( index[BP_AXIS_Y], limit[2], limit[3] );
  index[BP_AXIS_Z] = BP_CLAMP( index[BP_AXIS_Z], limit[4], limit[5] );

#ifndef MM_ATOMIC_SUPPORT
  locklimit[0] = limit[0] >> BP_CELL_GROUP_AXIS_SHIFT;
  locklimit[1] = limit[1] >> BP_CELL_GROUP_AXIS_SHIFT;
  locklimit[2] = limit[2] >> BP_CELL_GROUP_AXIS_SHIFT;
  locklimit[3] = limit[3] >> BP_CELL_GROUP_AXIS_SHIFT;
  locklimit[4] = limit[4] >> BP_CELL_GROUP_AXIS_SHIFT;
  locklimit[5] = limit[5] >> BP_CELL_GROUP_AXIS_SHIFT;
#endif

#ifdef MM_ATOMIC_SUPPORT
  if( !( bpGridLock( grid, limit, &lockfail ) ) )
  {
    bpGridUnlock( grid, limit, lockfail );
    mmAtomicSpin32( &grid->globallock, 0x0, 0x1 );
    bpGridLockForce( grid, limit );
    mmAtomicWrite32( &grid->globallock, 0x0 );
  }
#else
  if( !( bpGridLock( grid, locklimit, &lockfail ) ) )
  {
    bpGridUnlock( grid, locklimit, lockfail );
    mtMutexLock( &grid->globalmutex );
    bpGridLockForce( grid, locklimit );
    mtMutexUnlock( &grid->globalmutex );
  }
#endif

  retvalue = 1;
  dupdistsqsoft = bpdata->dupvertexdistancesoft * bpdata->dupvertexdistancesoft;
  dupdistsqhard = bpdata->dupvertexdistancehard * bpdata->dupvertexdistancehard;
  if( bpGridCellDuplicateCheck( bpdata, grid, limit, vertexindex, dupdistsqsoft, dupdistsqhard ) )
  {
#ifdef DEBUG_VERBOSE_GRID
printf( "    Discard Vertex %d\n", (int)vertexindex );
#endif
    /* Forbid using the vertex as triangle seed */
    mmBitMapSet( &bpdata->vertexmap, vertexindex );
    retvalue = 0;
    tdata->info.pointdupcount++;
    goto end;
  }

  cellindex = ( index[BP_AXIS_X] * grid->stride[BP_AXIS_X] ) + ( index[BP_AXIS_Y] * grid->stride[BP_AXIS_Y] ) + ( index[BP_AXIS_Z] * grid->stride[BP_AXIS_Z] );
#ifdef MM_ATOMIC_SUPPORT
  cell = BP_GRID_CELL_POINTER( mmAtomicReadP( &grid->cell[cellindex] ) );
#else
  cell = grid->cell[cellindex];
#endif

#ifdef DEBUG_VERBOSE_GRID
printf( "    Vertex %d : Add To Cell %d (%d;%d;%d)\n", (int)vertexindex, (int)cellindex, (int)index[0], (int)index[1], (int)index[2] );
#endif

  if( !( cell ) )
  {
    create:
    newcell = mmBlockAlloc( &tdata->cellblock );
    newcell->next = cell;
    newcell->count = 1;
    newcell->vertex[0] = vertexindex;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWriteP( &grid->cell[cellindex], BP_GRID_CELL_LOCK(newcell) );
#else
    grid->cell[cellindex] = newcell;
#endif
  }
  else
  {
    if( cell->count >= BP_CELL_VERTEX_COUNT )
      goto create;
    cell->vertex[cell->count] = vertexindex;
    cell->count++;
  }

  end:
#ifdef MM_ATOMIC_SUPPORT
  bpGridUnlock( grid, limit, -1 );
#else
  bpGridUnlock( grid, locklimit, -1 );
#endif

  return retvalue;
}


/* Pick all vertices in grid within specified radius around point, walk subgrids, call specified callback for each */
static int bpGridPickRadius( bpMainData *bpdata, bpGrid *grid, bpf *center, bpf radius, int (*callback)( bpMainData *bpdata, void *opaquepointer, bpi vertexindex, bpf distsquared ), void *opaquepointer )
{
  int32_t index, count;
  ssize_t x, y, z, cellindex, rowy, rowz;
  ssize_t limit[6];
  bpi vertexindex;
  void *sub;
  bpf *vertex;
  bpf offset[BP_AXIS_COUNT];
  bpf radsquared, dx, dy, dz, distsquared;
  bpGridCell *cell;

  M3D_VectorSubStore( offset, center, grid->origin );
  limit[0] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] - radius ) * grid->scale[BP_AXIS_X] );
  limit[1] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] + radius ) * grid->scale[BP_AXIS_X] );
  limit[2] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] - radius ) * grid->scale[BP_AXIS_Y] );
  limit[3] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] + radius ) * grid->scale[BP_AXIS_Y] );
  limit[4] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] - radius ) * grid->scale[BP_AXIS_Z] );
  limit[5] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] + radius ) * grid->scale[BP_AXIS_Z] );

#ifdef DEBUG_VERBOSE_PICK
printf( "    PickGridRadius : Center %f %f %f, Radius %f\n", center[0], center[1], center[2], radius );
#endif

  limit[0] = BP_CLAMP( limit[0], 0, grid->max[BP_AXIS_X] );
  limit[1] = BP_CLAMP( limit[1], 0, grid->max[BP_AXIS_X] );
  limit[2] = BP_CLAMP( limit[2], 0, grid->max[BP_AXIS_Y] );
  limit[3] = BP_CLAMP( limit[3], 0, grid->max[BP_AXIS_Y] );
  limit[4] = BP_CLAMP( limit[4], 0, grid->max[BP_AXIS_Z] );
  limit[5] = BP_CLAMP( limit[5], 0, grid->max[BP_AXIS_Z] );


/*
limit[0] = 0;
limit[2] = 0;
limit[4] = 0;
limit[1] = grid->max[BP_AXIS_X];
limit[3] = grid->max[BP_AXIS_Y];
limit[5] = grid->max[BP_AXIS_Z];
*/


#ifdef DEBUG_VERBOSE_PICK
printf( "    PickGridRadius : Iterate %d,%d ; %d,%d ; %d,%d\n", (int)limit[0], (int)limit[1], (int)limit[2], (int)limit[3], (int)limit[4], (int)limit[5] );
#endif

  cellindex = ( limit[0] * grid->stride[BP_AXIS_X] ) + ( limit[2] * grid->stride[BP_AXIS_Y] ) + ( limit[4] * grid->stride[BP_AXIS_Z] );
  rowy = grid->stride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = grid->stride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * grid->stride[BP_AXIS_Y] );

  radsquared = radius * radius;
  for( z = limit[4] ; z <= limit[5] ; z++ )
  {
    for( y = limit[2] ; y <= limit[3] ; y++ )
    {
      for( x = limit[0] ; x <= limit[1] ; x++, cellindex++ )
      {
#ifdef MM_ATOMIC_SUPPORT
        sub = MM_ATOMIC_ACCESS_P( &grid->cell[cellindex] );
#else
        sub = grid->cell[cellindex];
#endif

        /* No cell or grid, skip */
        if( !( sub ) )
          continue;

        /* Subgrid, go recursive */
        if( BP_GRID_CHECK_SUBGRID(sub) )
        {
          if( !( bpGridPickRadius( bpdata, BP_GRID_GET_SUBGRID(sub), center, radius, callback, opaquepointer ) ) )
            return 0;
          continue;
        }

        /* Iterate all linked cells and their vertices */
        for( cell = BP_GRID_GET_CELL(sub) ; cell ; cell = cell->next )
        {
          count = cell->count;
          for( index = 0 ; index < count ; index++ )
          {
            vertexindex = cell->vertex[index];
            vertex = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
            dx = vertex[BP_AXIS_X] - center[BP_AXIS_X];
            dy = vertex[BP_AXIS_Y] - center[BP_AXIS_Y];
            dz = vertex[BP_AXIS_Z] - center[BP_AXIS_Z];
            distsquared = dx*dx + dy*dy + dz*dz;
            if( distsquared > radsquared )
              continue;
            if( !( callback( bpdata, opaquepointer, vertexindex, distsquared ) ) )
              return 0;
          }
        }
      }
      cellindex += rowy;
    }
    cellindex += rowz;
  }

  return 1;
}



/********/



static void bpPointLockInit( bpMainData *bpdata, bpPointLockMap *plm, bpf *limit, int threadcount )
{
  int lockcount, bits, axisbest, axisbits[BP_AXIS_COUNT];
  bpf size[BP_AXIS_COUNT], range[BP_AXIS_COUNT], rangemax;
#ifndef MM_ATOMIC_SUPPORT
  bpLock *lock;
#endif

  if( threadcount <= 1 )
  {
    plm->lock = 0;
    return;
  }

  /* Amount of locks must be power of 2 for quick masking */
  /* Round to next highest power of two */
  lockcount = ccPow2Round32( threadcount * 256 );
  bits = ccLog2IntL( lockcount );

  /* Subdivide the bits over the 3 axis */
  size[BP_AXIS_X] = limit[1] - limit[0];
  size[BP_AXIS_Y] = limit[3] - limit[2];
  size[BP_AXIS_Z] = limit[5] - limit[4];
  M3D_VectorCopy( range, size );
  axisbits[BP_AXIS_X] = 0;
  axisbits[BP_AXIS_Y] = 0;
  axisbits[BP_AXIS_Z] = 0;
  for( ; bits ; bits-- )
  {
    axisbest = BP_AXIS_X;
    rangemax = range[BP_AXIS_X];
    if( range[BP_AXIS_Y] > rangemax )
    {
      axisbest = BP_AXIS_Y;
      rangemax = range[BP_AXIS_Y];
    }
    if( range[BP_AXIS_Z] > rangemax )
    {
      axisbest = BP_AXIS_Z;
      rangemax = range[BP_AXIS_Z];
    }
    axisbits[ axisbest ]++;      
    range[ axisbest ] *= 0.5;
  }

  plm->scale[BP_AXIS_X] = (bpf)( 1 << axisbits[BP_AXIS_X] ) / size[BP_AXIS_X];
  plm->scale[BP_AXIS_Y] = (bpf)( 1 << axisbits[BP_AXIS_Y] ) / size[BP_AXIS_Y];
  plm->scale[BP_AXIS_Z] = (bpf)( 1 << axisbits[BP_AXIS_Z] ) / size[BP_AXIS_Z];

  plm->lockcount = lockcount;
  plm->lockmask = lockcount - 1;
  plm->lock = mmAlignRelayAlloc( (void *)mmVolumeAlloc, &bpdata->volume, lockcount * sizeof(bpLock), 0x40, 0x0 );
#ifdef MM_ATOMIC_SUPPORT
  memset( plm->lock, 0, lockcount * sizeof(bpLock) );
#else
  for( lock = plm->lock ; lockcount ; lockcount--, lock++ )
    mtMutexInit( &lock->mutex );
#endif

  return;
}


static void bpPointLockFree( bpMainData *bpdata, bpPointLockMap *plm )
{
#ifndef MM_ATOMIC_SUPPORT
  int lockcount;
  bpLock *lock;
  for( lockcount = plm->lockcount, lock = plm->lock ; lockcount ; lockcount--, lock++ )
    mtMutexDestroy( &lock->mutex );
#endif
  if( plm->lock )
    mmAlignRelayFree( (void *)mmVolumeFree, &bpdata->volume, plm->lock, plm->lockcount * sizeof(bpLock) );
  return;
}


static void bpPointLockAcquire( bpMainData *bpdata, bpPointLockMap *plm, long *lockindex, bpf *vertex )
{
  long index;

  if( !( plm->lock ) )
    return;

  /* Compute the lockindex for vertex */
  index = (long)( ( ( vertex[BP_AXIS_X] - plm->origin[BP_AXIS_X] ) * plm->scale[BP_AXIS_X] ) + ( ( vertex[BP_AXIS_Y] - plm->origin[BP_AXIS_Y] ) * plm->scale[BP_AXIS_Y] ) + ( ( vertex[BP_AXIS_Z] - plm->origin[BP_AXIS_Z] ) * plm->scale[BP_AXIS_Z] ) ) & plm->lockmask;

  /* Switch locks */
  if( index == *lockindex )
    return;
#ifdef MM_ATOMIC_SUPPORT
  if( *lockindex >= 0 )
    mmAtomicWrite32( &plm->lock[ *lockindex ].atomiclock, 0 );
  mmAtomicSpin32( &plm->lock[ index ].atomiclock, 0, 1 );
#else
  if( *lockindex >= 0 )
    mtMutexUnlock( &plm->lock[ *lockindex ].mutex );
  mtMutexLock( &plm->lock[ index ].mutex );
#endif
  *lockindex = index;

  return;
}


static void bpPointLockRelease( bpMainData *bpdata, bpPointLockMap *plm, long *lockindex )
{
#ifdef MM_ATOMIC_SUPPORT
  if( *lockindex >= 0 )
    mmAtomicWrite32( &plm->lock[ *lockindex ].atomiclock, 0 );
#else
  if( *lockindex >= 0 )
    mtMutexUnlock( &plm->lock[ *lockindex ].mutex );
#endif
  *lockindex = -1;
  return;
}



/********/



static void bpSpaceMapInit( bpMainData *bpdata, bpSpaceMap *slm, size_t vertexcount, bpf *limit, bpf ballradius )
{
  int lockcount;
  size_t memsize;
  bpf cellcount, cellcountmax, cellcountmin, factor;
  bpf limitsize[BP_AXIS_COUNT], sizef[BP_AXIS_COUNT];
  bpSpaceCell *cell;
#ifndef MM_ATOMIC_SUPPORT
  bpLock *lock;
#endif

  limitsize[BP_AXIS_X] = limit[1] - limit[0];
  limitsize[BP_AXIS_Y] = limit[3] - limit[2];
  limitsize[BP_AXIS_Z] = limit[5] - limit[4];

  factor = BP_SPACE_MAP_SIZE_FACTOR / ballradius;
  sizef[BP_AXIS_X] = limitsize[BP_AXIS_X] * factor;
  sizef[BP_AXIS_Y] = limitsize[BP_AXIS_Y] * factor;
  sizef[BP_AXIS_Z] = limitsize[BP_AXIS_Z] * factor;

  cellcount = sizef[BP_AXIS_X] * sizef[BP_AXIS_Y] * sizef[BP_AXIS_Z];
  cellcountmax = vertexcount / BP_SPACE_MAP_VERTEXPERCELL_MIN;
  cellcountmin = vertexcount / BP_SPACE_MAP_VERTEXPERCELL_MAX;
  factor = 1.0;
  if( cellcount < cellcountmin )
    factor = bpfcbrt( cellcountmin / cellcount );
  else if( cellcount > cellcountmax )
    factor = bpfcbrt( cellcountmax / cellcount );

#ifdef DEBUG_VERBOSE
printf( "Space Map Cell Count Estimate : %f ( min %f ; max %f )\n", cellcount, cellcountmin, cellcountmax );
#endif

  slm->size[BP_AXIS_X] = (int)bpfceil( sizef[BP_AXIS_X] * factor );
  slm->size[BP_AXIS_Y] = (int)bpfceil( sizef[BP_AXIS_Y] * factor );
  slm->size[BP_AXIS_Z] = (int)bpfceil( sizef[BP_AXIS_Z] * factor );
  if( slm->size[BP_AXIS_X] < 2 )
    slm->size[BP_AXIS_X] = 2;
  if( slm->size[BP_AXIS_Y] < 2 )
    slm->size[BP_AXIS_Y] = 2;
  if( slm->size[BP_AXIS_Z] < 2 )
    slm->size[BP_AXIS_Z] = 2;
  slm->stride[BP_AXIS_X] = 1;
  slm->stride[BP_AXIS_Y] = slm->size[BP_AXIS_X];
  slm->stride[BP_AXIS_Z] = slm->size[BP_AXIS_X] * slm->size[BP_AXIS_Y];

  lockcount = slm->size[BP_AXIS_X] * slm->size[BP_AXIS_Y] * slm->size[BP_AXIS_Z];
  slm->max[BP_AXIS_X] = slm->size[BP_AXIS_X] - 1;
  slm->max[BP_AXIS_Y] = slm->size[BP_AXIS_Y] - 1;
  slm->max[BP_AXIS_Z] = slm->size[BP_AXIS_Z] - 1;

#ifdef DEBUG_VERBOSE
printf( "Space Map Final Cell Count : %d ( %d x %d x %d )\n", (int)lockcount, (int)slm->size[BP_AXIS_X], (int)slm->size[BP_AXIS_Y], (int)slm->size[BP_AXIS_Z] );
#endif

  slm->origin[BP_AXIS_X] = limit[0];
  slm->origin[BP_AXIS_Y] = limit[2];
  slm->origin[BP_AXIS_Z] = limit[4];
  slm->scale[BP_AXIS_X] = (bpf)slm->size[BP_AXIS_X] / limitsize[BP_AXIS_X];
  slm->scale[BP_AXIS_Y] = (bpf)slm->size[BP_AXIS_Y] / limitsize[BP_AXIS_Y];
  slm->scale[BP_AXIS_Z] = (bpf)slm->size[BP_AXIS_Z] / limitsize[BP_AXIS_Z];

  memsize = lockcount * sizeof(bpSpaceCell);
  slm->cell = mmAlignRelayAlloc( (void *)mmVolumeAlloc, &bpdata->volume, memsize, 0x40, 0x0 );
  slm->memsize = memsize;
  for( cell = slm->cell ; lockcount ; lockcount--, cell++ )
  {
    cell->trilist = 0;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWrite32( &cell->lock.atomiclock, 0x0 );
#else
    mtMutexInit( &cell->lock.mutex );
#endif
  }
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicWrite32( &slm->globallock, 0x0 );
#else
  mtMutexInit( &slm->globalmutex );
#endif

  return;
}

static void bpSpaceMapFree( bpMainData *bpdata, bpSpaceMap *slm )
{
#ifndef MM_ATOMIC_SUPPORT
  int lockcount;
  bpSpaceCell *cell;

  lockcount = slm->size[BP_AXIS_X] * slm->size[BP_AXIS_Y] * slm->size[BP_AXIS_Z];
  for( cell = slm->cell ; lockcount ; lockcount--, cell++ )
  {
    cell->trilist = 0;
    mtMutexDestroy( &cell->lock.mutex );
  }
  mtMutexDestroy( &slm->globalmutex );
#endif
  if( slm->cell )
  {
    mmAlignRelayFree( (void *)mmVolumeFree, &bpdata->volume, slm->cell, slm->memsize );
    slm->cell = 0;
  }
  return;
}


static void bpSpaceMapAddTriangle( bpSpaceMap *slm, bpTriangle *triangle, bpf *center )
{
  ssize_t x, y, z, cellindex;
  bpSpaceCell *cell;

  x = (ssize_t)bpffloor( ( center[BP_AXIS_X] - slm->origin[BP_AXIS_X] ) * slm->scale[BP_AXIS_X] );
  y = (ssize_t)bpffloor( ( center[BP_AXIS_Y] - slm->origin[BP_AXIS_Y] ) * slm->scale[BP_AXIS_Y] );
  z = (ssize_t)bpffloor( ( center[BP_AXIS_Z] - slm->origin[BP_AXIS_Z] ) * slm->scale[BP_AXIS_Z] );
  x = BP_CLAMP( x, 0, slm->max[BP_AXIS_X] );
  y = BP_CLAMP( y, 0, slm->max[BP_AXIS_Y] );
  z = BP_CLAMP( z, 0, slm->max[BP_AXIS_Z] );
  cellindex = ( x * slm->stride[BP_AXIS_X] ) + ( y * slm->stride[BP_AXIS_Y] ) + ( z * slm->stride[BP_AXIS_Z] );

  cell = &slm->cell[ cellindex ];
  mmListAdd( &cell->trilist, triangle, offsetof(bpTriangle,list) );

  return;
}


static int bpSpaceMapPickRadius( bpMainData *bpdata, bpSpaceMap *slm, bpf *center, bpf radius, int (*callback)( bpMainData *bpdata, void *opaquepointer, bpTriangle *triangle ), void *opaquepointer )
{
  ssize_t x, y, z, cellindex, rowy, rowz;
  ssize_t limit[6];
  bpf offset[BP_AXIS_COUNT];
  bpSpaceCell *cell;
  bpTriangle *triangle;

  M3D_VectorSubStore( offset, center, slm->origin );
  limit[0] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] - radius ) * slm->scale[BP_AXIS_X] );
  limit[1] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] + radius ) * slm->scale[BP_AXIS_X] );
  limit[2] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] - radius ) * slm->scale[BP_AXIS_Y] );
  limit[3] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] + radius ) * slm->scale[BP_AXIS_Y] );
  limit[4] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] - radius ) * slm->scale[BP_AXIS_Z] );
  limit[5] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] + radius ) * slm->scale[BP_AXIS_Z] );

#ifdef DEBUG_VERBOSE_PICK
printf( "    SpaceMapPickRadius : Center %f %f %f, Radius %f\n", center[0], center[1], center[2], radius );
#endif

  limit[0] = BP_CLAMP( limit[0], 0, slm->max[BP_AXIS_X] );
  limit[1] = BP_CLAMP( limit[1], 0, slm->max[BP_AXIS_X] );
  limit[2] = BP_CLAMP( limit[2], 0, slm->max[BP_AXIS_Y] );
  limit[3] = BP_CLAMP( limit[3], 0, slm->max[BP_AXIS_Y] );
  limit[4] = BP_CLAMP( limit[4], 0, slm->max[BP_AXIS_Z] );
  limit[5] = BP_CLAMP( limit[5], 0, slm->max[BP_AXIS_Z] );

#ifdef DEBUG_VERBOSE_PICK
printf( "    SpaceMapPickRadius : Iterate %d,%d ; %d,%d ; %d,%d\n", (int)limit[0], (int)limit[1], (int)limit[2], (int)limit[3], (int)limit[4], (int)limit[5] );
#endif

  cellindex = ( limit[0] * slm->stride[BP_AXIS_X] ) + ( limit[2] * slm->stride[BP_AXIS_Y] ) + ( limit[4] * slm->stride[BP_AXIS_Z] );
  rowy = slm->stride[BP_AXIS_Y] - ( limit[1] - limit[0] + 1 );
  rowz = slm->stride[BP_AXIS_Z] - ( ( limit[3] - limit[2] + 1 ) * slm->stride[BP_AXIS_Y] );

  for( z = limit[4] ; z <= limit[5] ; z++ )
  {
    for( y = limit[2] ; y <= limit[3] ; y++ )
    {
      for( x = limit[0] ; x <= limit[1] ; x++, cellindex++ )
      {
        cell = &slm->cell[ cellindex ];
        for( triangle = cell->trilist ; triangle ; triangle = triangle->list.next )
          callback( bpdata, opaquepointer, triangle );
      }
      cellindex += rowy;
    }
    cellindex += rowz;
  }

  return 1;
}


static void bpSpaceLockAcquire( bpMainData *bpdata, bpSpaceMap *slm, bpSpaceLock *spacelock, bpf *center, bpf radius )
{
  ssize_t x, y, z, cellindex, cellindexfail, cellindexbase, rowy, rowz;
  bpLock *lock;
  bpf offset[BP_AXIS_COUNT];

  offset[BP_AXIS_X] = center[BP_AXIS_X] - slm->origin[BP_AXIS_X];
  offset[BP_AXIS_Y] = center[BP_AXIS_Y] - slm->origin[BP_AXIS_Y];
  offset[BP_AXIS_Z] = center[BP_AXIS_Z] - slm->origin[BP_AXIS_Z];

  spacelock->limit[0] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] - radius ) * slm->scale[BP_AXIS_X] );
  spacelock->limit[1] = (ssize_t)bpffloor( ( offset[BP_AXIS_X] + radius ) * slm->scale[BP_AXIS_X] );
  spacelock->limit[2] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] - radius ) * slm->scale[BP_AXIS_Y] );
  spacelock->limit[3] = (ssize_t)bpffloor( ( offset[BP_AXIS_Y] + radius ) * slm->scale[BP_AXIS_Y] );
  spacelock->limit[4] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] - radius ) * slm->scale[BP_AXIS_Z] );
  spacelock->limit[5] = (ssize_t)bpffloor( ( offset[BP_AXIS_Z] + radius ) * slm->scale[BP_AXIS_Z] );

  spacelock->limit[0] = BP_CLAMP( spacelock->limit[0], 0, slm->max[BP_AXIS_X] );
  spacelock->limit[1] = BP_CLAMP( spacelock->limit[1], 0, slm->max[BP_AXIS_X] );
  spacelock->limit[2] = BP_CLAMP( spacelock->limit[2], 0, slm->max[BP_AXIS_Y] );
  spacelock->limit[3] = BP_CLAMP( spacelock->limit[3], 0, slm->max[BP_AXIS_Y] );
  spacelock->limit[4] = BP_CLAMP( spacelock->limit[4], 0, slm->max[BP_AXIS_Z] );
  spacelock->limit[5] = BP_CLAMP( spacelock->limit[5], 0, slm->max[BP_AXIS_Z] );

  spacelock->globalflag = 0;

  cellindexbase = ( spacelock->limit[0] * slm->stride[BP_AXIS_X] ) + ( spacelock->limit[2] * slm->stride[BP_AXIS_Y] ) + ( spacelock->limit[4] * slm->stride[BP_AXIS_Z] );
  rowy = slm->stride[BP_AXIS_Y] - ( spacelock->limit[1] - spacelock->limit[0] + 1 );
  rowz = slm->stride[BP_AXIS_Z] - ( ( spacelock->limit[3] - spacelock->limit[2] + 1 ) * slm->stride[BP_AXIS_Y] );

  for( ; ; )
  {
    cellindex = cellindexbase;
    for( z = spacelock->limit[4] ; z <= spacelock->limit[5] ; z++ )
    {
      for( y = spacelock->limit[2] ; y <= spacelock->limit[3] ; y++ )
      {
        for( x = spacelock->limit[0] ; x <= spacelock->limit[1] ; x++, cellindex++ )
        {
          lock = &slm->cell[ cellindex ].lock;
          for( ; ; )
          {
#ifdef MM_ATOMIC_SUPPORT
            if( mmAtomicCmpXchg32( &lock->atomiclock, 0x0, 0x1 ) == 0x0 )
              break;
#else
            if( mtMutexTryLock( &lock->mutex ) )
              break;
#endif
            if( !( spacelock->globalflag ) )
            {
              cellindexfail = cellindex;
              goto fail;
            }
          }
        }
        cellindex += rowy;
      }
      cellindex += rowz;
    }
    return;

    fail:

    cellindex = cellindexbase;
    for( z = spacelock->limit[4] ; z <= spacelock->limit[5] ; z++ )
    {
      for( y = spacelock->limit[2] ; y <= spacelock->limit[3] ; y++ )
      {
        for( x = spacelock->limit[0] ; x <= spacelock->limit[1] ; x++, cellindex++ )
        {
          if( cellindex == cellindexfail )
            goto restart;
          lock = &slm->cell[ cellindex ].lock;
#ifdef MM_ATOMIC_SUPPORT
          mmAtomicWrite32( &lock->atomiclock, 0x0 );
#else
          mtMutexUnlock( &lock->mutex );
#endif
        }
        cellindex += rowy;
      }
      cellindex += rowz;
    }

    restart:

    spacelock->globalflag = 1;
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicSpin32( &slm->globallock, 0x0, 0x1 );
#else
    mtMutexLock( &slm->globalmutex );
#endif
  }

  return;
}


static void bpSpaceLockRelease( bpMainData *bpdata, bpSpaceMap *slm, bpSpaceLock *spacelock )
{
  ssize_t x, y, z, cellindex, rowy, rowz;
  bpLock *lock;

  cellindex = ( spacelock->limit[0] * slm->stride[BP_AXIS_X] ) + ( spacelock->limit[2] * slm->stride[BP_AXIS_Y] ) + ( spacelock->limit[4] * slm->stride[BP_AXIS_Z] );
  rowy = slm->stride[BP_AXIS_Y] - ( spacelock->limit[1] - spacelock->limit[0] + 1 );
  rowz = slm->stride[BP_AXIS_Z] - ( ( spacelock->limit[3] - spacelock->limit[2] + 1 ) * slm->stride[BP_AXIS_Y] );

  for( z = spacelock->limit[4] ; z <= spacelock->limit[5] ; z++ )
  {
    for( y = spacelock->limit[2] ; y <= spacelock->limit[3] ; y++ )
    {
      for( x = spacelock->limit[0] ; x <= spacelock->limit[1] ; x++, cellindex++ )
      {
        lock = &slm->cell[ cellindex ].lock;
#ifdef MM_ATOMIC_SUPPORT
        mmAtomicWrite32( &lock->atomiclock, 0x0 );
#else
        mtMutexUnlock( &lock->mutex );
#endif
      }
      cellindex += rowy;
    }
    cellindex += rowz;
  }

  if( spacelock->globalflag )
  {
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWrite32( &slm->globallock, 0x0 );
#else
    mtMutexUnlock( &slm->globalmutex );
#endif
  }

  return;
}



/********/



static size_t bpWorkGridSort( bpMainData *bpdata, bpThreadData *tdata, bpi vertexbase, bpi vertexmax )
{
  size_t activevertexcount;
  bpGrid *grid;
  bpf magn;
  bpf *vertex, *normal;
  bpi vertexindex;

  /* Did the user ask us to normalize normals? */
  if( bpdata->operationflags & BP_FLAGS_NORMALIZE_NORMALS )
  {
    normal = ADDRESS( bpdata->normal, vertexbase * bpdata->normalstride );
    for( vertexindex = vertexbase ; vertexindex < vertexmax ; vertexindex++ )
    {
      magn = bpfsqrt( normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2] );
      if( magn <= FLT_MIN )
        memset( normal, 0, 3*sizeof(bpf) );
      else
      {
        magn = 1.0 / magn;
        normal[0] *= magn;
        normal[1] *= magn;
        normal[2] *= magn;
      }
      normal = ADDRESS( normal, bpdata->normalstride );
    }
  }

  activevertexcount = 0;
  grid = bpdata->grid;
  vertex = ADDRESS( bpdata->vertex, vertexbase * bpdata->vertexstride );
  for( vertexindex = vertexbase ; vertexindex < vertexmax ; vertexindex++ )
  {
    activevertexcount += bpGridAddVertex( bpdata, tdata, grid, vertexindex );
    vertex = ADDRESS( vertex, bpdata->vertexstride );
  }

  return activevertexcount;
}



/********/



#define BP_HINGE_PER_BUNDLE (512)

enum
{
  BP_PATCH_HINGE_LEVEL_NORMAL,
  BP_PATCH_HINGE_LEVEL_JOIN,
  BP_PATCH_HINGE_LEVEL_REPAIR,
  BP_PATCH_HINGE_LEVEL_CONFLICT,
  BP_PATCH_HINGE_LEVEL_ERROR,
  BP_PATCH_HINGE_LEVEL_NEXT,

  BP_PATCH_HINGE_LEVEL_COUNT
};

#define BP_EDGE_GRADE_DELETED (0x0) /* Deleted hinge with queued rebuild operation */
#define BP_EDGE_GRADE_BAD (0x1)
#define BP_EDGE_GRADE_LINK (0x2)
#define BP_EDGE_GRADE_COPLANAR (0x10000000) /* Hinges within a cospherical coplanar surface */
#define BP_EDGE_GRADE_LOW (0x20000000) /* Cospherical hinge, can be overwritten by secure hinges */
#define BP_EDGE_GRADE_HIGH (0x30000000) /* Primary pivot hinge, secure, can overwrite */
#define BP_EDGE_GRADE_REPAIR (0x40000000)
#define BP_EDGE_GRADE_REPAIR_CAP (BP_EDGE_GRADE_REPAIR+0xf)
#define BP_EDGE_GRADE_SEED (0x50000000)

/* Final clean up step, mark edges connected to seed edges */
#define BP_EDGE_GRADE_CONNECTED (0x7fffffff) 

#define BP_EDGE_GRADE_PATHMIN (0x3)


#define BP_EDGE_GRADE_SUBRANGEMAX (0x0ffffff)


typedef struct
{
  /* Vertex v[2] isn't really part of the hinge, but we track it to check for existence of whole triangles */
  bpi v[3];
  int32_t grade;
  void *triangle;
} bpEdge;

typedef struct
{
  bpEdge edge;
  bpf spherecenter[3];
  bpf normal[3];
  uint32_t flags;
#ifdef CONFIG_ENABLE_CONFLICT_PIVOT
  bpi conflictpivot;
#endif
} bpHinge;

#define BP_HINGE_FLAGS_PIVOT (0x1)
#define BP_HINGE_FLAGS_JOIN (0x2)
#define BP_HINGE_FLAGS_ORPHAN (0x4)
#define BP_HINGE_FLAGS_FORCE (0x8)
#define BP_HINGE_FLAGS_LOST (0x10)
#define BP_HINGE_FLAGS_END (0x20)

#define BP_HINGE_FLAGS_SPHEREPIVOT (0x80)

#define BP_HINGE_COMPUTE_FLAGS(f,c) ((f)|((c)<<8))
#define BP_HINGE_SET_FLAGS_LEVEL(h,f,l) ((h)->flags=(f)|((l)<<16))
#define BP_HINGE_GET_FLAGS(h) ((h)->flags&0xffff)
#define BP_HINGE_SET_LEVEL(h) ((h)->flags=((h)->flags&0xffff)|((l)<<16))
#define BP_HINGE_GET_LEVEL(h) ((h)->flags>>16)
#define BP_HINGE_SET_CONFLICTDEPTH(h,c) ((h)->flags|=(c)<<8)
#define BP_HINGE_GET_CONFLICTDEPTH(f) (((f)>>8)&0xff)



typedef struct
{
  int writeindex;
  int readindex;
  bpHinge hinge[BP_HINGE_PER_BUNDLE];
  mmListNode list;
} bpHingeBundle;

typedef struct
{
  void *hingelist[BP_PATCH_HINGE_LEVEL_COUNT];
} bpPatch;



/********/



static void bpHingeHashClearEntry( void *entry )
{
  bpEdge *edge;
  edge = entry;
  edge->v[0] = -1;
  return;
}

static int bpHingeHashEntryValid( void *entry )
{
  bpEdge *edge;
  edge = entry;
  return ( edge->v[0] >= 0 ? 1 : 0 );
}

static uint32_t bpHingeHashEntryKey( void *entry )
{
  uint32_t hashkey;
  bpEdge *edge;
  edge = entry;

  /* Compute hash just from the first 2 vertices of hinge */
#if BP_SIZEOF_BPI == 4
 #if CPUCONF_WORD_SIZE == 64
  {
    uint64_t *v = (uint64_t *)edge->v;
    hashkey = ccHash32Int64Inline( *v );
  }
 #else
  hashkey = ccHash32Data4Inline( (void *)edge->v );
 #endif
#elif BP_SIZEOF_BPI == 8
 #if CPUCONF_WORD_SIZE == 64
  hashkey = ccHash32Array64( (uint64_t *)edge->v, 2 );
 #else
  hashkey = ccHash32Array32( (uint32_t *)edge->v, 4 );
 #endif
#else
  hashkey = ccHash32Data( edge->v, 2*sizeof(bpi) );
#endif

  return hashkey;
}

static int bpHingeHashEntryCmp( void *entry, void *entryref )
{
  bpEdge *edge, *edgeref;
  edge = entry;
  edgeref = entryref;
  if( edge->v[0] == -1 )
    return MM_HASH_ENTRYCMP_INVALID;
  if( ( edge->v[0] == edgeref->v[0] ) && ( edge->v[1] == edgeref->v[1] ) )
    return MM_HASH_ENTRYCMP_FOUND;
  return MM_HASH_ENTRYCMP_SKIP;
}

static int bpHingeHashEntryCmpFull( void *entry, void *entryref )
{
  bpEdge *edge, *edgeref;
  edge = entry;
  edgeref = entryref;
  if( edge->v[0] == -1 )
    return MM_HASH_ENTRYCMP_INVALID;
  if( ( edge->v[0] == edgeref->v[0] ) && ( edge->v[1] == edgeref->v[1] ) && ( edge->v[2] == edgeref->v[2] ) )
    return MM_HASH_ENTRYCMP_FOUND;
  return MM_HASH_ENTRYCMP_SKIP;
}

static int bpHingeHashEntryCmpGrade( void *entry, void *entryref )
{
  bpEdge *edge, *edgeref;
  edge = entry;
  edgeref = entryref;
  if( edge->v[0] == -1 )
    return MM_HASH_ENTRYCMP_INVALID;
  if( ( edge->v[0] == edgeref->v[0] ) && ( edge->v[1] == edgeref->v[1] ) && ( edge->grade >= edgeref->grade ) )
    return MM_HASH_ENTRYCMP_FOUND;
  return MM_HASH_ENTRYCMP_SKIP;
}

static int bpHingeHashEntryCmpStrip( void *entry, void *entryref )
{
  bpEdge *edge, *edgeref;
  edge = entry;
  edgeref = entryref;
  if( edge->v[0] == -1 )
    return MM_HASH_ENTRYCMP_INVALID;
  if( ( edge->v[0] == edgeref->v[0] ) && ( edge->v[1] == edgeref->v[1] ) && ( edge->v[2] == edgeref->v[2] ) && ( edge->grade <= edgeref->grade ) )
    return MM_HASH_ENTRYCMP_FOUND;
  return MM_HASH_ENTRYCMP_SKIP;
}

static int bpHingeHashEntryCmpClean( void *entry, void *entryref )
{
  bpEdge *edge, *edgeref;
  edge = entry;
  edgeref = entryref;
  if( edge->v[0] == -1 )
    return MM_HASH_ENTRYCMP_INVALID;
  if( ( edge->v[0] == edgeref->v[0] ) && ( edge->v[1] == edgeref->v[1] ) && ( edge->v[2] == edgeref->v[2] ) && ( edge->grade >= edgeref->grade ) )
  {
    edge->grade = BP_EDGE_GRADE_DELETED;
    return MM_HASH_ENTRYCMP_FOUND;
  }
  return MM_HASH_ENTRYCMP_SKIP;
}


static mmHashAccess bpHingeHashEdge =
{
  .clearentry = bpHingeHashClearEntry,
  .entryvalid = bpHingeHashEntryValid,
  .entrykey = bpHingeHashEntryKey,
  .entrycmp = bpHingeHashEntryCmp
};

static mmHashAccess bpHingeHashFull =
{
  .clearentry = bpHingeHashClearEntry,
  .entryvalid = bpHingeHashEntryValid,
  .entrykey = bpHingeHashEntryKey,
  .entrycmp = bpHingeHashEntryCmpFull
};

static mmHashAccess bpHingeHashGrade =
{
  .clearentry = bpHingeHashClearEntry,
  .entryvalid = bpHingeHashEntryValid,
  .entrykey = bpHingeHashEntryKey,
  .entrycmp = bpHingeHashEntryCmpGrade
};

static mmHashAccess bpHingeHashStrip =
{
  .clearentry = bpHingeHashClearEntry,
  .entryvalid = bpHingeHashEntryValid,
  .entrykey = bpHingeHashEntryKey,
  .entrycmp = bpHingeHashEntryCmpStrip
};

static mmHashAccess bpHingeHashClean =
{
  .clearentry = bpHingeHashClearEntry,
  .entryvalid = bpHingeHashEntryValid,
  .entrykey = bpHingeHashEntryKey,
  .entrycmp = bpHingeHashEntryCmpClean
};


static int bpHingeHashInit( bpMainData *bpdata, size_t vertexcount, bpf hashextrabits, uint32_t lockpageshift, size_t maxmemorysize )
{
  size_t edgecount, trianglecount, hashmemsize, trianglememsize, totalmemorysize;
  uint32_t hashbits, hashbitsmin, hashbitsmax;

  /* Decide hash table size given count of vertices */
  /* EdgeCount = 3*TriangleCount = 6*VertexCount, 1 extra bits to be safe, 50~100% expected occupancy */

  /* Estimates of edge and triangle count */
  edgecount = vertexcount * 6;
  trianglecount = vertexcount * 2;

  /* Hard minimum count of hash table bits */
#if CPUCONF_WORD_SIZE == 64
  hashbitsmin = ccLog2Int64( ( edgecount * 5 ) >> 2 ) + 1;
#else
  hashbitsmin = ccLog2Int32( ( edgecount * 5 ) >> 2 ) + 1;
#endif
  hashbitsmax = hashbitsmin + 4;

  hashbits = (uint32_t)bpfround( bpflog2( (bpf)edgecount ) + hashextrabits );
  if( hashbits < hashbitsmin )
    hashbits = hashbitsmin;
  else if( hashbits > hashbitsmax )
    hashbits = hashbitsmax;

  if( hashbits < 12 )
    hashbits = 12;
  /* lockpageshift = 7; works great, 128 hash entries per lock page */
  if( lockpageshift < 3 )
    lockpageshift = 3;
  else if( lockpageshift > 16 )
    lockpageshift = 16;

  trianglememsize = trianglecount * sizeof(bpTriangle);
  for( ; ; hashbits-- )
  {
    if( hashbits < hashbitsmin )
      return 0;
    hashmemsize = mmHashRequiredSize( sizeof(bpEdge), hashbits, lockpageshift );
    totalmemorysize = hashmemsize + trianglememsize;

    /* Increase estimate of memory consumption by 25% to account for extra stuff not counted here */
    totalmemorysize += totalmemorysize >> 2;

#ifdef DEBUG_VERBOSE_MEMORY
printf( "  Hash bits : %d (%d)\n", hashbits, hashbitsmin );
printf( "    Estimated Memory Requirements : %lld bytes (%lld MB)\n", (long long)totalmemorysize, (long long)totalmemorysize >> 20 );
printf( "    Memory Hard Limit : %lld bytes (%lld MB)\n", (long long)maxmemorysize, (long long)maxmemorysize >> 20 );
#endif

    if( totalmemorysize > maxmemorysize )
      continue;
    bpdata->hingehashtable = malloc( hashmemsize );
    if( bpdata->hingehashtable )
      break;
  }

  mmHashInit( bpdata->hingehashtable, &bpHingeHashEdge, sizeof(bpEdge), hashbits, lockpageshift, MM_HASH_FLAGS_NO_COUNT );

  return 1;
}

static void bpHingeHashFree( bpMainData *bpdata )
{
  if( bpdata->hingehashtable )
    free( bpdata->hingehashtable );
  bpdata->hingehashtable = 0;
  return;
}



/********/



static void bpPatchInit( bpThreadData *tdata, bpPatch *patch )
{
  int level;
  for( level = 0 ; level < BP_PATCH_HINGE_LEVEL_COUNT ; level++ )
    patch->hingelist[level] = 0;
  return;
}

static void bpPatchFree( bpThreadData *tdata, bpPatch *patch )
{
  int level;
  bpHingeBundle *bundle, *bundlenext;
  for( level = 0 ; level < BP_PATCH_HINGE_LEVEL_COUNT ; level++ )
  {
    for( bundle = patch->hingelist[level] ; bundle ; bundle = bundlenext )
    {
      bundlenext = bundle->list.next;
      mmBlockFree( &tdata->hingeblock, bundle );
    }
    patch->hingelist[level] = 0;
  }
  return;
}

static int bpPatchGetHinge( bpThreadData *tdata, bpPatch *patch, bpHinge *rethinge, int levelmax )
{
  int level, levelcap, index, readindex;
  bpHingeBundle *bundle;

  levelcap = ( levelmax < BP_PATCH_HINGE_LEVEL_CONFLICT ? levelmax : BP_PATCH_HINGE_LEVEL_CONFLICT );
  for( level = 0 ; level <= levelcap ; level++ )
  {
    bundle = patch->hingelist[level];
    if( !( bundle ) )
      continue;
    *rethinge = bundle->hinge[bundle->readindex];
    bundle->readindex++;
    if( bundle->readindex == bundle->writeindex )
    {
      mmListRemove( bundle, offsetof(bpHingeBundle,list) );
      mmBlockFree( &tdata->hingeblock, bundle );
    }
    return 1;
  }

  if( levelmax >= BP_PATCH_HINGE_LEVEL_CONFLICT )
  {
    bundle = patch->hingelist[BP_PATCH_HINGE_LEVEL_CONFLICT];
    if( bundle )
    {
      /* To help avoid infinite loops in conflict resolution, pick a random hinge */
      readindex = 0;
      if( bundle->writeindex > 1 )
        readindex = ccQuickRand32( &tdata->randstate ) % bundle->writeindex;
      *rethinge = bundle->hinge[readindex];
      bundle->writeindex--;
      for( index = readindex ; index < bundle->writeindex ; index++ )
        bundle->hinge[index] = bundle->hinge[index+1];
      if( !( bundle->writeindex ) )
      {
        mmListRemove( bundle, offsetof(bpHingeBundle,list) );
        mmBlockFree( &tdata->hingeblock, bundle );
      }
      return 1;
    }
  }

  return 0;
}

static void bpPatchAddHinge( bpThreadData *tdata, bpPatch *patch, int level, bpi v0, bpi v1, bpi v2, bpf *spherecenter, bpf *normal, int32_t grade, uint32_t flags, bpi conflictpivot )
{
  bpHingeBundle *bundle;
  bpHinge *hinge;

  bundle = patch->hingelist[level];
  if( !( bundle ) || ( bundle->writeindex >= BP_HINGE_PER_BUNDLE ) )
  {
    bundle = mmBlockAlloc( &tdata->hingeblock );
    bundle->writeindex = 0;
    bundle->readindex = 0;
    mmListAdd( &patch->hingelist[level], bundle, offsetof(bpHingeBundle,list) );
  }

#ifdef DEBUG_VERBOSE_HINGE
printf( "          New Hinge Queued : %d,%d (%d), Level %d, Grade 0x%x, Flags 0x%x ( ", (int)v0, (int)v1, (int)v2, level, (int)grade, (int)flags );
if( flags & BP_HINGE_FLAGS_PIVOT )
  printf( "PIVOT " );
if( flags & BP_HINGE_FLAGS_JOIN )
  printf( "JOIN " );
if( flags & BP_HINGE_FLAGS_ORPHAN )
  printf( "ORPHAN " );
if( flags & BP_HINGE_FLAGS_FORCE )
  printf( "FORCE " );
if( flags & BP_HINGE_FLAGS_LOST )
  printf( "LOST " );
if( BP_HINGE_GET_CONFLICTDEPTH( flags ) )
  printf( "DEPTH:%d ", BP_HINGE_GET_CONFLICTDEPTH( flags ) );
printf( ")\n" );
#endif

  hinge = &bundle->hinge[bundle->writeindex];
  hinge->edge.v[0] = v0;
  hinge->edge.v[1] = v1;
  hinge->edge.v[2] = v2;
  hinge->edge.grade = grade;
  M3D_VectorCopy( hinge->spherecenter, spherecenter );
  M3D_VectorCopy( hinge->normal, normal );
  BP_HINGE_SET_FLAGS_LEVEL( hinge, flags, level );
#ifdef CONFIG_ENABLE_CONFLICT_PIVOT
  hinge->conflictpivot = conflictpivot;
#endif
  bundle->writeindex++;

  return;
}

static void bpPatchNextBatch( bpThreadData *tdata, bpPatch *patch )
{
  mmListMoveList( &patch->hingelist[BP_PATCH_HINGE_LEVEL_NORMAL], &patch->hingelist[BP_PATCH_HINGE_LEVEL_NEXT], offsetof(bpHingeBundle,list) );
  return;
}

/*
static void bpPatchErrorBatch( bpThreadData *tdata, bpPatch *patch )
{
  mmListMoveList( &patch->hingelist[BP_PATCH_HINGE_LEVEL_NORMAL], &patch->hingelist[BP_PATCH_HINGE_LEVEL_ERROR], offsetof(bpHingeBundle,list) );
  return;
}
*/


/********/



static void bpMeshInit( bpThreadData *tdata, bpMesh *mesh )
{
  mesh->trianglecount = 0;
  mesh->trianglelist = 0;
  mesh->seedlist = 0;
  return;
}


static void bpMeshFree( bpThreadData *tdata, bpMesh *mesh )
{
  bpTriangleBundle *bundle, *bundlenext;
  bpTriangleSeed *seed, *seednext;
  for( bundle = mesh->trianglelist ; bundle ; bundle = bundlenext )
  {
    bundlenext = bundle->list.next;
    mmBlockFree( &tdata->triangleblock, bundle );
  }
  mesh->trianglelist = 0;
  for( seed = mesh->seedlist ; seed ; seed = seednext )
  {
    seednext = seed->list.next;
    free( seed );
  }
  mesh->seedlist = 0;
  return;
}


static bpTriangle *bpMeshQueryTriangle( bpThreadData *tdata, bpMesh *mesh )
{
  int index;
  bpTriangle *tridst;
  bpTriangleBundle *bundle;

  bundle = mesh->trianglelist;
  if( !( bundle ) || ( bundle->count >= BP_TRIANGLE_PER_BUNDLE ) )
  {
    bundle = mmBlockAlloc( &tdata->triangleblock );
    bundle->count = 0;
    mmListAdd( &mesh->trianglelist, bundle, offsetof(bpTriangleBundle,list) );
  }

  index = bundle->count;
  tridst = &bundle->triangle[index];

  return tridst;
}


static inline void bpMeshAddTriangle( bpThreadData *tdata, bpMesh *mesh )
{
  bpTriangleBundle *bundle;

  bundle = mesh->trianglelist;
  bundle->count++;
  mesh->trianglecount++;

  return;
}


static void bpMeshAddSeed( bpThreadData *tdata, bpMesh *mesh, bpTriangle *triangle )
{
  bpTriangleSeed *seed;

  /* Urgh malloc, blocks worth it? Probably not */
  seed = malloc( sizeof(bpTriangleSeed) );
  seed->v[0] = triangle->v[0];
  seed->v[1] = triangle->v[1];
  seed->v[2] = triangle->v[2];
  mmListAdd( &mesh->seedlist, seed, offsetof(bpTriangleSeed,list) );

  return;
}



/********/



static int bpComputeSphereCenter( bpf *vertex0, bpf *vertex1, bpf *vertex2, bpf *surfacenormal, bpf radiusmin, bpf radiusmax, bpf distmin, bpf *center, bpf *normal, int *flipflag )
{
  bpf vecta[3];
  bpf vectb[3];
  bpf normalmagn, normalmagninv;
  bpf coreradius;
  bpf height;
  bpf aa, ab, bb, aabb, abab, dista, distb, m;

  /* Compute normal of triangle */
  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( normal, vectb, vecta );

  normalmagn = M3D_VectorMagnitude( normal );
  /* If the 3 points are practically collinear, fail */
  if( normalmagn <= CONF_COLLINEAR_MAGNITUDE_MIN * M3D_VectorMagnitude(vecta) * M3D_VectorMagnitude(vectb) )
    return 0;
  normalmagninv = 1.0 / normalmagn;
  *flipflag = 0;
  if( M3D_VectorDotProduct( normal, surfacenormal ) < 0.0 )
  {
    normalmagninv = -normalmagninv;
    *flipflag = 1;
  }
  normal[0] *= normalmagninv;
  normal[1] *= normalmagninv;
  normal[2] *= normalmagninv;

  /* Find center of minimal radius sphere lying on the plane of the points */
  aa = M3D_VectorDotProduct( vecta, vecta );
  ab = M3D_VectorDotProduct( vecta, vectb );
  bb = M3D_VectorDotProduct( vectb, vectb );
  aabb = aa * bb;
  abab = ab * ab;
  if( aabb <= abab )
    return 0;
  m = 0.5 / ( aabb - abab );
  dista = m * ( aabb - bb*ab );
  distb = m * ( aabb - ab*aa );
  center[0] = ( dista * vecta[0] ) + ( distb * vectb[0] );
  center[1] = ( dista * vecta[1] ) + ( distb * vectb[1] );
  center[2] = ( dista * vecta[2] ) + ( distb * vectb[2] );
  coreradius = M3D_VectorMagnitude( center );

  /* Sphere is bigger than threshold radius */
  if( coreradius > radiusmax )
    return 0;
  height = distmin;
  if( coreradius < radiusmin )
    height = bpfmax( height, bpfsqrt( radiusmin*radiusmin - coreradius*coreradius ) );

  /* Displace center relative to vertex0, and push away from plane of points for extra radius */
  center[0] += vertex0[0] + ( height * normal[0] );
  center[1] += vertex0[1] + ( height * normal[1] );
  center[2] += vertex0[2] + ( height * normal[2] );

  return 1;
}


static int bpComputePivotSphere( const bpf * const origin, const bpf * const vecta, const bpf * const vectb, const bpf * const normal, bpf radiusmin, bpf radiusmax, bpf distmin, bpf *center, bpf *baseradius )
{
  bpf coreradius;
  bpf height;
  bpf aa, ab, bb, aabb, abab, dista, distb, m;

  /* Find center of minimal radius sphere lying on the plane of the points */
  aa = M3D_VectorDotProduct( vecta, vecta );
  ab = M3D_VectorDotProduct( vecta, vectb );
  bb = M3D_VectorDotProduct( vectb, vectb );
  aabb = aa * bb;
  abab = ab * ab;
  if( aabb <= abab )
    return 0;
  m = 0.5 / ( aabb - abab );
  dista = m * ( aabb - bb*ab );
  distb = m * ( aabb - ab*aa );
  center[0] = ( dista * vecta[0] ) + ( distb * vectb[0] );
  center[1] = ( dista * vecta[1] ) + ( distb * vectb[1] );
  center[2] = ( dista * vecta[2] ) + ( distb * vectb[2] );
  coreradius = M3D_VectorMagnitude( center );

#ifdef DEBUG_VERBOSE_SPHERE
printf( "                Sphere Minimum Center %f %f %f\n", center[0] + origin[0], center[1] + origin[1], center[2] + origin[2] );
printf( "                Sphere Radius %f < %f\n", coreradius, radiusmax );
#endif

  /* Sphere is bigger than maximum radius */
  if( coreradius > radiusmax )
    return 0;
  *baseradius = coreradius;
  height = distmin;
  if( coreradius < radiusmin )
    height = bpfmax( height, bpfsqrt( radiusmin*radiusmin - coreradius*coreradius ) );

#ifdef DEBUG_VERBOSE_SPHERE
printf( "                Sphere Height %f\n", height );
#endif

  /* Displace center relative to origin, and push away from plane of points for extra radius */
  center[0] += origin[0] + ( height * normal[0] );
  center[1] += origin[1] + ( height * normal[1] );
  center[2] += origin[2] + ( height * normal[2] );

#ifdef DEBUG_VERBOSE_SPHERE
printf( "                Sphere Displaced Center %f %f %f\n", center[0], center[1], center[2] );
#endif

  return 1;
}


#if 0  // 2012-10-22 ch3: this function not currently used, so commented
static bpf bpComputeMinimumSphere( const bpf * const origin, const bpf * const vecta, const bpf * const vectb, bpf *center )
{
  bpf radius;
  bpf aa, ab, bb, aabb, abab, dista, distb, m;

  /* Find center of minimal radius sphere lying on the plane of the points */
  aa = M3D_VectorDotProduct( vecta, vecta );
  ab = M3D_VectorDotProduct( vecta, vectb );
  bb = M3D_VectorDotProduct( vectb, vectb );
  aabb = aa * bb;
  abab = ab * ab;
  if( aabb <= abab )
    return 0;
  m = 0.5 / ( aabb - abab );
  dista = m * ( aabb - bb*ab );
  distb = m * ( aabb - ab*aa );
  center[0] = ( dista * vecta[0] ) + ( distb * vectb[0] );
  center[1] = ( dista * vecta[1] ) + ( distb * vectb[1] );
  center[2] = ( dista * vecta[2] ) + ( distb * vectb[2] );
  radius = M3D_VectorMagnitude( center );

  center[0] += origin[0];
  center[1] += origin[1];
  center[2] += origin[2];

  return radius;
}
#endif



/********/



static inline FORCEINLINE void rbTriTriIsect( const bpf * MM_RESTRICT v0, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, bpf d0, bpf d1, bpf d2, bpf *point )
{
  bpf f;
  f = d0 / ( d0 - d1 );
  point[0] = v0[0] + f * ( v1[0] - v0[0] );
  point[1] = v0[1] + f * ( v1[1] - v0[1] );
  point[2] = v0[2] + f * ( v1[2] - v0[2] );
  f = d0 / ( d0 - d2 );
  point[3] = v0[0] + f * ( v2[0] - v0[0] );
  point[4] = v0[1] + f * ( v2[1] - v0[1] );
  point[5] = v0[2] + f * ( v2[2] - v0[2] );
  return;
}


int rbTriTriIntersection( const bpf * MM_RESTRICT v0, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, const bpf * MM_RESTRICT u0, const bpf * MM_RESTRICT u1, const bpf * MM_RESTRICT u2, bpf *hit )
{
  int index, hitindex0, hitindex1;
  bpf edge0[3], edge1[3];
  bpf plane0[4], plane1[4];
  bpf du0, du1, du2, dv0, dv1, dv2;
  bpf d[3];
  bpf isect[4];
  bpf du0du1, du0du2, dv0dv1, dv0dv2;
  bpf bb, max, tmp;
  bpf point[4*3];
  void *ptsrc0, *ptsrc1;

  M3D_VectorSubStore( edge0, v1, v0 );
  M3D_VectorSubStore( edge1, v2, v0 );
  M3D_VectorCrossProduct( plane0, edge0, edge1 );
  plane0[3] = -M3D_VectorDotProduct( plane0, v0 );
  du0 = M3D_PlanePoint( plane0, u0 );
  du1 = M3D_PlanePoint( plane0, u1 );
  du2 = M3D_PlanePoint( plane0, u2 );
  du0du1 = du0 * du1;
  du0du2 = du0 * du2;
  if( ( du0du1 >= 0.0 ) && ( du0du2 >= 0.0 ) )
    return 0;

  M3D_VectorSubStore( edge0, u1, u0 );
  M3D_VectorSubStore( edge1, u2, u0 );
  M3D_VectorCrossProduct( plane1, edge0, edge1 );
  plane1[3] = -M3D_VectorDotProduct( plane1, u0 );
  dv0 = M3D_PlanePoint( plane1, v0 );
  dv1 = M3D_PlanePoint( plane1, v1 );
  dv2 = M3D_PlanePoint( plane1, v2 );
  dv0dv1 = dv0 * dv1;
  dv0dv2 = dv0 * dv2;
  if( ( dv0dv1 >= 0.0 ) && ( dv0dv2 >= 0.0 ) )
    return 0;

  if( dv0dv1 > 0.0 )
    rbTriTriIsect( v2, v0, v1, dv2, dv0, dv1, &point[0] );
  else if( dv0dv2 > 0.0 )
    rbTriTriIsect( v1, v0, v2, dv1, dv0, dv2, &point[0] );
  else if( ( dv1*dv2 > 0.0 ) || ( dv0 != 0.0 ) )
    rbTriTriIsect( v0, v1, v2, dv0, dv1, dv2, &point[0] );
  else if( dv1 != 0.0 )
    rbTriTriIsect( v1, v0, v2, dv1, dv0, dv2, &point[0] );
  else if( dv2 != 0.0 )
    rbTriTriIsect( v2, v0, v1, dv2, dv0, dv1, &point[0] );
  else
    return 0;

  if( du0du1 > 0.0 )
    rbTriTriIsect( u2, u0, u1, du2, du0, du1, &point[6] );
  else if( du0du2 > 0.0 )
    rbTriTriIsect( u1, u0, u2, du1, du0, du2, &point[6] );
  else if( ( du1*du2 > 0.0 ) || ( du0 != 0.0 ) )
    rbTriTriIsect( u0, u1, u2, du0, du1, du2, &point[6] );
  else if( du1 != 0.0 )
    rbTriTriIsect( u1, u0, u2, du1, du0, du2, &point[6] );
  else if( du2 != 0.0 )
    rbTriTriIsect( u2, u0, u1, du2, du0, du1, &point[6] );
  else
    return 0;

  M3D_VectorCrossProduct( d, plane0, plane1 );
  index = 0;
  max = bpfabs( d[0] );
  bb = bpfabs( d[1] );
  if( max < bb )
  {
    index = 1;
    max = bb;
  }
  if( max < bpfabs( d[2] ) )
    index = 2;
  isect[0] = point[0+index];
  isect[1] = point[3+index];
  isect[2] = point[6+index];
  isect[3] = point[9+index];

  hitindex0 = 0;
  if( isect[0] > isect[1] )
  {
    tmp = isect[0];
    isect[0] = isect[1];
    isect[1] = tmp;
    hitindex0 = 3*sizeof(bpf);
  }
  hitindex1 = 0;
  if( isect[2] > isect[3] )
  {
    tmp = isect[2];
    isect[2] = isect[3];
    isect[3] = tmp;
    hitindex1 = 3*sizeof(bpf);
  }
  if( ( isect[1] < isect[2] ) || ( isect[3] < isect[0] ) )
    return 0;

  if( isect[2] < isect[0] )
    ptsrc0 = ADDRESS( point, 0*sizeof(bpf) + hitindex0 );
  else
    ptsrc0 = ADDRESS( point, 6*sizeof(bpf) + hitindex1 );
  if( isect[3] < isect[1] )
    ptsrc1 = ADDRESS( point, 9*sizeof(bpf) - hitindex1 );
  else
    ptsrc1 = ADDRESS( point, 3*sizeof(bpf) - hitindex0 );
  memcpy( &hit[0], ptsrc0, 3*sizeof(bpf) );
  memcpy( &hit[3], ptsrc1, 3*sizeof(bpf) );

  return 1;
}


static inline FORCEINLINE void rbTriTriIsectCheck( const bpf * MM_RESTRICT v0, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, bpf d0, bpf d1, bpf d2, bpf *isect, int index )
{
  isect[0] = v0[index] + ( d0 / ( d0 - d1 ) ) * ( v1[index] - v0[index] );
  isect[1] = v0[index] + ( d0 / ( d0 - d2 ) ) * ( v2[index] - v0[index] );
  return;
}

int rbTriTriIntersectionCheck( const bpf * MM_RESTRICT v0, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, const bpf * MM_RESTRICT u0, const bpf * MM_RESTRICT u1, const bpf * MM_RESTRICT u2 )
{
  int index;
  bpf edge0[3], edge1[3];
  bpf plane0[4], plane1[4];
  bpf du0, du1, du2, dv0, dv1, dv2;
  bpf d[3];
  bpf isect[4];
  bpf du0du1, du0du2, dv0dv1, dv0dv2;
  bpf bb, max, tmp;

  M3D_VectorSubStore( edge0, v1, v0 );
  M3D_VectorSubStore( edge1, v2, v0 );
  M3D_VectorCrossProduct( plane0, edge0, edge1 );
  plane0[3] = -M3D_VectorDotProduct( plane0, v0 );
  du0 = M3D_PlanePoint( plane0, u0 );
  du1 = M3D_PlanePoint( plane0, u1 );
  du2 = M3D_PlanePoint( plane0, u2 );
  du0du1 = du0 * du1;
  du0du2 = du0 * du2;
  if( ( du0du1 >= 0.0 ) && ( du0du2 >= 0.0 ) )
    return 0;

  M3D_VectorSubStore( edge0, u1, u0 );
  M3D_VectorSubStore( edge1, u2, u0 );
  M3D_VectorCrossProduct( plane1, edge0, edge1 );
  plane1[3] = -M3D_VectorDotProduct( plane1, u0 );
  dv0 = M3D_PlanePoint( plane1, v0 );
  dv1 = M3D_PlanePoint( plane1, v1 );
  dv2 = M3D_PlanePoint( plane1, v2 );
  dv0dv1 = dv0 * dv1;
  dv0dv2 = dv0 * dv2;
  if( ( dv0dv1 >= 0.0 ) && ( dv0dv2 >= 0.0 ) )
    return 0;

  M3D_VectorCrossProduct( d, plane0, plane1 );
  index = 0;
  max = bpfabs( d[0] );
  bb = bpfabs( d[1] );
  if( max < bb )
  {
    index = 1;
    max = bb;
  }
  if( max < bpfabs( d[2] ) )
    index = 2;

  if( dv0dv1 > 0.0 )
    rbTriTriIsectCheck( v2, v0, v1, dv2, dv0, dv1, &isect[0], index );
  else if( dv0dv2 > 0.0 )
    rbTriTriIsectCheck( v1, v0, v2, dv1, dv0, dv2, &isect[0], index );
  else if( ( dv1*dv2 > 0.0 ) || ( dv0 != 0.0 ) )
    rbTriTriIsectCheck( v0, v1, v2, dv0, dv1, dv2, &isect[0], index );
  else if( dv1 != 0.0 )
    rbTriTriIsectCheck( v1, v0, v2, dv1, dv0, dv2, &isect[0], index );
  else if( dv2 != 0.0 )
    rbTriTriIsectCheck( v2, v0, v1, dv2, dv0, dv1, &isect[0], index );
  else
    return 0;

  if( du0du1 > 0.0 )
    rbTriTriIsectCheck( u2, u0, u1, du2, du0, du1, &isect[2], index );
  else if( du0du2 > 0.0 )
    rbTriTriIsectCheck( u1, u0, u2, du1, du0, du2, &isect[2], index );
  else if( ( du1*du2 > 0.0 ) || ( du0 != 0.0 ) )
    rbTriTriIsectCheck( u0, u1, u2, du0, du1, du2, &isect[2], index );
  else if( du1 != 0.0 )
    rbTriTriIsectCheck( u1, u0, u2, du1, du0, du2, &isect[2], index );
  else if( du2 != 0.0 )
    rbTriTriIsectCheck( u2, u0, u1, du2, du0, du1, &isect[2], index );
  else
    return 0;

  if( isect[0] > isect[1] )
  {
    tmp = isect[0];
    isect[0] = isect[1];
    isect[1] = tmp;
  }
  if( isect[2] > isect[3] )
  {
    tmp = isect[2];
    isect[2] = isect[3];
    isect[3] = tmp;
  }
  if( ( isect[1] < isect[2] ) || ( isect[3] < isect[0] ) )
    return 0;

  return 1;
}

int rbTriTriIntersectionCheckP( const bpf * MM_RESTRICT v0, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, const bpf * MM_RESTRICT vplane, const bpf * MM_RESTRICT u0, const bpf * MM_RESTRICT u1, const bpf * MM_RESTRICT u2 )
{
  int index;
  bpf edge0[3], edge1[3];
  bpf plane1[4];
  bpf du0, du1, du2, dv0, dv1, dv2;
  bpf d[3];
  bpf isect[4];
  bpf du0du1, du0du2, dv0dv1, dv0dv2;
  bpf bb, max, tmp;

  du0 = M3D_PlanePoint( vplane, u0 );
  du1 = M3D_PlanePoint( vplane, u1 );
  du2 = M3D_PlanePoint( vplane, u2 );
  du0du1 = du0 * du1;
  du0du2 = du0 * du2;
  if( ( du0du1 >= 0.0 ) && ( du0du2 >= 0.0 ) )
    return 0;

  M3D_VectorSubStore( edge0, u1, u0 );
  M3D_VectorSubStore( edge1, u2, u0 );
  M3D_VectorCrossProduct( plane1, edge0, edge1 );
  plane1[3] = -M3D_VectorDotProduct( plane1, u0 );
  dv0 = M3D_PlanePoint( plane1, v0 );
  dv1 = M3D_PlanePoint( plane1, v1 );
  dv2 = M3D_PlanePoint( plane1, v2 );
  dv0dv1 = dv0 * dv1;
  dv0dv2 = dv0 * dv2;
  if( ( dv0dv1 >= 0.0 ) && ( dv0dv2 >= 0.0 ) )
    return 0;

  M3D_VectorCrossProduct( d, vplane, plane1 );
  index = 0;
  max = bpfabs( d[0] );
  bb = bpfabs( d[1] );
  if( max < bb )
  {
    index = 1;
    max = bb;
  }
  if( max < bpfabs( d[2] ) )
    index = 2;

  if( dv0dv1 > 0.0 )
    rbTriTriIsectCheck( v2, v0, v1, dv2, dv0, dv1, &isect[0], index );
  else if( dv0dv2 > 0.0 )
    rbTriTriIsectCheck( v1, v0, v2, dv1, dv0, dv2, &isect[0], index );
  else if( ( dv1*dv2 > 0.0 ) || ( dv0 != 0.0 ) )
    rbTriTriIsectCheck( v0, v1, v2, dv0, dv1, dv2, &isect[0], index );
  else if( dv1 != 0.0 )
    rbTriTriIsectCheck( v1, v0, v2, dv1, dv0, dv2, &isect[0], index );
  else if( dv2 != 0.0 )
    rbTriTriIsectCheck( v2, v0, v1, dv2, dv0, dv1, &isect[0], index );
  else
    return 0;

  if( du0du1 > 0.0 )
    rbTriTriIsectCheck( u2, u0, u1, du2, du0, du1, &isect[2], index );
  else if( du0du2 > 0.0 )
    rbTriTriIsectCheck( u1, u0, u2, du1, du0, du2, &isect[2], index );
  else if( ( du1*du2 > 0.0 ) || ( du0 != 0.0 ) )
    rbTriTriIsectCheck( u0, u1, u2, du0, du1, du2, &isect[2], index );
  else if( du1 != 0.0 )
    rbTriTriIsectCheck( u1, u0, u2, du1, du0, du2, &isect[2], index );
  else if( du2 != 0.0 )
    rbTriTriIsectCheck( u2, u0, u1, du2, du0, du1, &isect[2], index );
  else
    return 0;

  if( isect[0] > isect[1] )
  {
    tmp = isect[0];
    isect[0] = isect[1];
    isect[1] = tmp;
  }
  if( isect[2] > isect[3] )
  {
    tmp = isect[2];
    isect[2] = isect[3];
    isect[3] = tmp;
  }
  if( ( isect[1] < isect[2] ) || ( isect[3] < isect[0] ) )
    return 0;

  return 1;
}

int rbTriTriIntersectionCheckPP( const bpf * MM_RESTRICT v0, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, const bpf * MM_RESTRICT vplane, const bpf * MM_RESTRICT u0, const bpf * MM_RESTRICT u1, const bpf * MM_RESTRICT u2, const bpf * MM_RESTRICT uplane )
{
  int index;
  bpf du0, du1, du2, dv0, dv1, dv2;
  bpf d[3];
  bpf isect[4];
  bpf du0du1, du0du2, dv0dv1, dv0dv2;
  bpf bb, max, tmp;

  du0 = M3D_PlanePoint( vplane, u0 );
  du1 = M3D_PlanePoint( vplane, u1 );
  du2 = M3D_PlanePoint( vplane, u2 );
  du0du1 = du0 * du1;
  du0du2 = du0 * du2;
  if( ( du0du1 >= 0.0 ) && ( du0du2 >= 0.0 ) )
    return 0;

  dv0 = M3D_PlanePoint( uplane, v0 );
  dv1 = M3D_PlanePoint( uplane, v1 );
  dv2 = M3D_PlanePoint( uplane, v2 );
  dv0dv1 = dv0 * dv1;
  dv0dv2 = dv0 * dv2;
  if( ( dv0dv1 >= 0.0 ) && ( dv0dv2 >= 0.0 ) )
    return 0;

  M3D_VectorCrossProduct( d, vplane, uplane );
  index = 0;
  max = bpfabs( d[0] );
  bb = bpfabs( d[1] );
  if( max < bb )
  {
    index = 1;
    max = bb;
  }
  if( max < bpfabs( d[2] ) )
    index = 2;

  if( dv0dv1 > 0.0 )
    rbTriTriIsectCheck( v2, v0, v1, dv2, dv0, dv1, &isect[0], index );
  else if( dv0dv2 > 0.0 )
    rbTriTriIsectCheck( v1, v0, v2, dv1, dv0, dv2, &isect[0], index );
  else if( ( dv1*dv2 > 0.0 ) || ( dv0 != 0.0 ) )
    rbTriTriIsectCheck( v0, v1, v2, dv0, dv1, dv2, &isect[0], index );
  else if( dv1 != 0.0 )
    rbTriTriIsectCheck( v1, v0, v2, dv1, dv0, dv2, &isect[0], index );
  else if( dv2 != 0.0 )
    rbTriTriIsectCheck( v2, v0, v1, dv2, dv0, dv1, &isect[0], index );
  else
    return 0;

  if( du0du1 > 0.0 )
    rbTriTriIsectCheck( u2, u0, u1, du2, du0, du1, &isect[2], index );
  else if( du0du2 > 0.0 )
    rbTriTriIsectCheck( u1, u0, u2, du1, du0, du2, &isect[2], index );
  else if( ( du1*du2 > 0.0 ) || ( du0 != 0.0 ) )
    rbTriTriIsectCheck( u0, u1, u2, du0, du1, du2, &isect[2], index );
  else if( du1 != 0.0 )
    rbTriTriIsectCheck( u1, u0, u2, du1, du0, du2, &isect[2], index );
  else if( du2 != 0.0 )
    rbTriTriIsectCheck( u2, u0, u1, du2, du0, du1, &isect[2], index );
  else
    return 0;

  if( isect[0] > isect[1] )
  {
    tmp = isect[0];
    isect[0] = isect[1];
    isect[1] = tmp;
  }
  if( isect[2] > isect[3] )
  {
    tmp = isect[2];
    isect[2] = isect[3];
    isect[3] = tmp;
  }
  if( ( isect[1] < isect[2] ) || ( isect[3] < isect[0] ) )
    return 0;

  return 1;
}




int rbTriTriIntersectionSharedCheck( const bpf * MM_RESTRICT vshared, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, const bpf * MM_RESTRICT u1, const bpf * MM_RESTRICT u2, bpf biasrange )
{
  int index;
  bpf edge0[3], edge1[3];
  bpf plane0[4], plane1[4];
  bpf du1, du2, dv1, dv2;
  bpf d[3];
  bpf isect[4];
  bpf bb, max, tmp;
  bpf sideleft[4], sideright[4];

  M3D_VectorSubStore( edge0, v1, vshared );
  M3D_VectorSubStore( edge1, v2, vshared );
  M3D_VectorCrossProduct( plane0, edge0, edge1 );
  M3D_VectorNormalize( plane0 );
  plane0[3] = -M3D_VectorDotProduct( plane0, vshared );
  du1 = M3D_PlanePoint( plane0, u1 );
  du2 = M3D_PlanePoint( plane0, u2 );
  if( ( du1 * du2 > 0.0 ) && ( ( bpfabs( du1 ) > biasrange ) || ( bpfabs( du2 ) > biasrange ) ) )
    return 0;

  M3D_VectorSubStore( edge0, u1, vshared );
  M3D_VectorSubStore( edge1, u2, vshared );
  M3D_VectorCrossProduct( plane1, edge0, edge1 );
  M3D_VectorNormalize( plane1 );
  plane1[3] = -M3D_VectorDotProduct( plane1, vshared );
  dv1 = M3D_PlanePoint( plane1, v1 );
  dv2 = M3D_PlanePoint( plane1, v2 );
  if( ( dv1 * dv2 > 0.0 ) && ( ( bpfabs( dv1 ) > biasrange ) || ( bpfabs( dv2 ) > biasrange ) ) )
    return 0;

  M3D_VectorCrossProduct( d, plane0, plane1 );
  index = 0;
  max = bpfabs( d[0] );
  bb = bpfabs( d[1] );
  if( max < bb )
  {
    index = 1;
    max = bb;
  }
  if( max < bpfabs( d[2] ) )
    index = 2;

  if( bpfabs( dv1 ) > biasrange )
    rbTriTriIsectCheck( v1, vshared, v2, dv1, 0.0, dv2, &isect[0], index );
  else if( bpfabs( dv2 ) > biasrange )
    rbTriTriIsectCheck( v2, vshared, v1, dv2, 0.0, dv1, &isect[0], index );
  else
    goto coplanar;

  if( bpfabs( du1 ) > biasrange )
    rbTriTriIsectCheck( u1, vshared, u2, du1, 0.0, du2, &isect[2], index );
  else if( bpfabs( du2 ) > biasrange )
    rbTriTriIsectCheck( u2, vshared, u1, du2, 0.0, du1, &isect[2], index );
  else
    goto coplanar;

  if( isect[0] > isect[1] )
  {
    tmp = isect[0];
    isect[0] = isect[1];
    isect[1] = tmp;
  }
  if( isect[2] > isect[3] )
  {
    tmp = isect[2];
    isect[2] = isect[3];
    isect[3] = tmp;
  }
  if( ( isect[1] < isect[2] + biasrange ) || ( isect[3] < isect[0] + biasrange ) )
    return 0;
  return 1;

  coplanar:

  M3D_VectorSubStore( edge0, v1, vshared );
  M3D_VectorCrossProduct( sideleft, edge0, plane0 );
  sideleft[3] = -M3D_VectorDotProduct( sideleft, vshared );
  M3D_VectorSubStore( edge1, v2, vshared );
  M3D_VectorCrossProduct( sideright, edge1, plane0 );
  sideright[3] = -M3D_VectorDotProduct( sideright, vshared );
  if( ( M3D_PlanePoint( sideleft, u1 ) < 0.0 ) && ( M3D_PlanePoint( sideright, u1 ) > 0.0 ) )
    return 1;
  if( ( M3D_PlanePoint( sideleft, u2 ) < 0.0 ) && ( M3D_PlanePoint( sideright, u2 ) > 0.0 ) )
    return 1;

  M3D_VectorSubStore( edge0, u1, vshared );
  M3D_VectorCrossProduct( sideleft, edge0, plane1 );
  sideleft[3] = -M3D_VectorDotProduct( sideleft, vshared );
  M3D_VectorSubStore( edge1, u2, vshared );
  M3D_VectorCrossProduct( sideright, edge1, plane1 );
  sideright[3] = -M3D_VectorDotProduct( sideright, vshared );
  if( ( M3D_PlanePoint( sideleft, v1 ) < 0.0 ) && ( M3D_PlanePoint( sideright, v1 ) > 0.0 ) )
    return 1;
  if( ( M3D_PlanePoint( sideleft, v2 ) < 0.0 ) && ( M3D_PlanePoint( sideright, v2 ) > 0.0 ) )
    return 1;

  return 0;
}

int rbTriTriIntersectionSharedCheckP( const bpf * MM_RESTRICT vshared, const bpf * MM_RESTRICT v1, const bpf * MM_RESTRICT v2, const bpf * MM_RESTRICT vplane, const bpf * MM_RESTRICT u1, const bpf * MM_RESTRICT u2, bpf biasrange )
{
  int index;
  bpf edge0[3], edge1[3];
  bpf plane0[4], plane1[4], vplanedist;
  bpf du1, du2, dv1, dv2;
  bpf d[3];
  bpf isect[4];
  bpf bb, max, tmp;
  bpf sideleft[4], sideright[4];

  vplanedist = -M3D_VectorDotProduct( vplane, vshared );
  du1 = M3D_VectorDotProduct( vplane, u1 ) + vplanedist;
  du2 = M3D_VectorDotProduct( vplane, u2 ) + vplanedist;
  if( ( du1 * du2 > 0.0 ) && ( ( bpfabs( du1 ) > biasrange ) || ( bpfabs( du2 ) > biasrange ) ) )
    return 0;

  M3D_VectorSubStore( edge0, u1, vshared );
  M3D_VectorSubStore( edge1, u2, vshared );
  M3D_VectorCrossProduct( plane1, edge0, edge1 );
  M3D_VectorNormalize( plane1 );
  plane1[3] = -M3D_VectorDotProduct( plane1, vshared );
  dv1 = M3D_PlanePoint( plane1, v1 );
  dv2 = M3D_PlanePoint( plane1, v2 );
  if( ( dv1 * dv2 > 0.0 ) && ( ( bpfabs( dv1 ) > biasrange ) || ( bpfabs( dv2 ) > biasrange ) ) )
    return 0;

  M3D_VectorCrossProduct( d, vplane, plane1 );
  index = 0;
  max = bpfabs( d[0] );
  bb = bpfabs( d[1] );
  if( max < bb )
  {
    index = 1;
    max = bb;
  }
  if( max < bpfabs( d[2] ) )
    index = 2;

  if( bpfabs( dv1 ) > biasrange )
    rbTriTriIsectCheck( v1, vshared, v2, dv1, 0.0, dv2, &isect[0], index );
  else if( bpfabs( dv2 ) > biasrange )
    rbTriTriIsectCheck( v2, vshared, v1, dv2, 0.0, dv1, &isect[0], index );
  else
    goto coplanar;

  if( bpfabs( du1 ) > biasrange )
    rbTriTriIsectCheck( u1, vshared, u2, du1, 0.0, du2, &isect[2], index );
  else if( bpfabs( du2 ) > biasrange )
    rbTriTriIsectCheck( u2, vshared, u1, du2, 0.0, du1, &isect[2], index );
  else
    goto coplanar;

  if( isect[0] > isect[1] )
  {
    tmp = isect[0];
    isect[0] = isect[1];
    isect[1] = tmp;
  }
  if( isect[2] > isect[3] )
  {
    tmp = isect[2];
    isect[2] = isect[3];
    isect[3] = tmp;
  }

  if( ( isect[1] < isect[2] + biasrange ) || ( isect[3] < isect[0] + biasrange ) )
    return 0;
  return 1;

  coplanar:

  M3D_VectorSubStore( edge0, v1, vshared );
  M3D_VectorSubStore( edge1, v2, vshared );
  M3D_VectorCrossProduct( plane0, edge0, edge1 );
  M3D_VectorNormalize( plane0 );
  plane0[3] = -M3D_VectorDotProduct( plane0, vshared );

  M3D_VectorSubStore( edge0, v1, vshared );
  M3D_VectorCrossProduct( sideleft, edge0, plane0 );
  sideleft[3] = -M3D_VectorDotProduct( sideleft, vshared );
  M3D_VectorSubStore( edge1, v2, vshared );
  M3D_VectorCrossProduct( sideright, edge1, plane0 );
  sideright[3] = -M3D_VectorDotProduct( sideright, vshared );
  if( ( M3D_PlanePoint( sideleft, u1 ) < 0.0 ) && ( M3D_PlanePoint( sideright, u1 ) > 0.0 ) )
    return 1;
  if( ( M3D_PlanePoint( sideleft, u2 ) < 0.0 ) && ( M3D_PlanePoint( sideright, u2 ) > 0.0 ) )
    return 1;
  M3D_VectorSubStore( edge0, u1, vshared );
  M3D_VectorCrossProduct( sideleft, edge0, plane1 );
  sideleft[3] = -M3D_VectorDotProduct( sideleft, vshared );
  M3D_VectorSubStore( edge1, u2, vshared );
  M3D_VectorCrossProduct( sideright, edge1, plane1 );
  sideright[3] = -M3D_VectorDotProduct( sideright, vshared );
  if( ( M3D_PlanePoint( sideleft, v1 ) < 0.0 ) && ( M3D_PlanePoint( sideright, v1 ) > 0.0 ) )
    return 1;
  if( ( M3D_PlanePoint( sideleft, v2 ) < 0.0 ) && ( M3D_PlanePoint( sideright, v2 ) > 0.0 ) )
    return 1;

  return 0;
}



/********/



static int bpRebuildHinge( bpMainData *bpdata, bpThreadData *tdata, bpPatch *patch, bpi v0, bpi v1, bpi conflictpivot, int conflictdepth )
{
  int flipflag, newhingelevel, newhingeflags;
  bpi v2;
  bpEdge edge;
  bpEdge *edgefind;
  bpf surfacenormal[BP_AXIS_COUNT];
  bpf normal[BP_AXIS_COUNT];
  bpf spherecenter[BP_AXIS_COUNT];
  bpf *vertex0, *vertex1, *vertex2;
  bpf *normal0, *normal1, *normal2;
  mmHashLock hashlock;
  mmHashLockRange lockrange;

#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Rebuild : %d,%d, ConflictDepth : %d\n", (int)v0, (int)v1, conflictdepth );
#endif

  edge.v[0] = v1;
  edge.v[1] = v0;
  mmHashLockInit( &hashlock, 3 );
  mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edge, &hashlock, &lockrange );
  mmHashLockAcquire( bpdata->hingehashtable, &bpHingeHashEdge, &hashlock );
  edgefind = mmHashDirectFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge );
  if( !( edgefind ) )
  {
    mmHashLockRelease( bpdata->hingehashtable, &hashlock );

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Rebuild Failed, No edge : %d,%d\n", (int)v0, (int)v1 );
#endif

    return 0;
  }
  v2 = edgefind->v[2];
  mmHashLockRelease( bpdata->hingehashtable, &hashlock );

  vertex0 = ADDRESS( bpdata->vertex, v0 * bpdata->vertexstride );
  normal0 = ADDRESS( bpdata->normal, v0 * bpdata->normalstride );
  vertex1 = ADDRESS( bpdata->vertex, v1 * bpdata->vertexstride );
  normal1 = ADDRESS( bpdata->normal, v1 * bpdata->normalstride );
  vertex2 = ADDRESS( bpdata->vertex, v2 * bpdata->vertexstride );
  normal2 = ADDRESS( bpdata->normal, v2 * bpdata->normalstride );
  surfacenormal[BP_AXIS_X] = normal0[BP_AXIS_X] + normal1[BP_AXIS_X] + normal2[BP_AXIS_X];
  surfacenormal[BP_AXIS_Y] = normal0[BP_AXIS_Y] + normal1[BP_AXIS_Y] + normal2[BP_AXIS_Y];
  surfacenormal[BP_AXIS_Z] = normal0[BP_AXIS_Z] + normal1[BP_AXIS_Z] + normal2[BP_AXIS_Z];

  if( !( bpComputeSphereCenter( vertex0, vertex1, vertex2, surfacenormal, bpdata->ballradiusmin, bpdata->ballradiusmax, bpdata->balldistmin, spherecenter, normal, &flipflag ) ) )
    return 0;

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Rebuild Queued : %d,%d (%d)\n", (int)v0, (int)v1, (int)v2 );
#endif


  if( ++conflictdepth < BP_CONFLICTDEPTH_MAX )
  {
#ifdef CONFIG_ENABLE_HINGE_ORPHAN_JOIN
    newhingelevel = conflictdepth ? BP_PATCH_HINGE_LEVEL_CONFLICT : BP_PATCH_HINGE_LEVEL_JOIN;
    newhingeflags = BP_HINGE_FLAGS_PIVOT | BP_HINGE_FLAGS_JOIN | BP_HINGE_FLAGS_ORPHAN;
#else
    newhingelevel = conflictdepth ? BP_PATCH_HINGE_LEVEL_CONFLICT : BP_PATCH_HINGE_LEVEL_NORMAL;;
    newhingeflags = BP_HINGE_FLAGS_PIVOT | BP_HINGE_FLAGS_ORPHAN;
#endif
  }
  else
  {
    newhingelevel = BP_PATCH_HINGE_LEVEL_ERROR;
    newhingeflags = BP_HINGE_FLAGS_PIVOT | BP_HINGE_FLAGS_JOIN | BP_HINGE_FLAGS_ORPHAN;
    conflictdepth = 0;
    tdata->info.orphancount++;
#ifdef DEBUG_VERBOSE_ERROR
    printf( "WARNING : Conflict Depth Exceeded, Orphaned Hinge %d,%d (%d) Promoted To ERROR Level\n", (int)v0, (int)v1, (int)conflictpivot );
#endif
  }
  bpPatchAddHinge( tdata, patch, newhingelevel, v0, v1, v2, spherecenter, normal, BP_EDGE_GRADE_REPAIR, BP_HINGE_COMPUTE_FLAGS( newhingeflags, conflictdepth ), conflictpivot );


  return 1;
}



/********/



static bpf bpOverridePivotAngle( bpMainData *bpdata, bpi v0, bpi v1, bpi v2, bpf *midpoint, bpf *basepivot, bpf *pivotplane, bpf *triplane, bpf ballradius )
{
  bpf angle, baseradius;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], normal[BP_AXIS_COUNT];
  bpf spherecenter[BP_AXIS_COUNT];
  bpf pivot[BP_AXIS_COUNT];
  bpf *vertex0, *vertex1, *vertex2;

  vertex0 = ADDRESS( bpdata->vertex, v0 * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, v1 * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, v2 * bpdata->vertexstride );
  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( normal, vectb, vecta );
  M3D_VectorNormalize( normal );

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Override Pivot Vertex %d,%d (%d)\n", (int)v0, (int)v1, (int)v2 );
printf( "              Override Pivot Try -> %.3f,%.3f,%.3f ; %.3f,%.3f,%.3f ; %.3f,%.3f,%.3f\n", vertex0[0], vertex0[1], vertex0[2], vertex1[0], vertex1[1], vertex1[2], vertex2[0], vertex2[1], vertex2[2] );
printf( "              Override Pivot Normal %f,%f,%f\n", normal[0], normal[1], normal[2] );
#endif

  if( !( bpComputePivotSphere( vertex0, vecta, vectb, normal, ballradius, FLT_MAX, bpdata->balldistmin, spherecenter, &baseradius ) ) )
    return -4.0;

  M3D_VectorSubStore( pivot, spherecenter, midpoint );
  angle = M3D_VectorDotProduct( basepivot, pivot ) / M3D_VectorMagnitude( pivot );

#ifdef DEBUG_VERBOSE_HINGE
printf( "                Spherecenter %f %f %f\n", spherecenter[0], spherecenter[1], spherecenter[2] );
printf( "                Basepivot %f %f %f ; Pivot %f %f %f\n", basepivot[0], basepivot[1], basepivot[2], pivot[0] / M3D_VectorMagnitude( pivot ), pivot[1] / M3D_VectorMagnitude( pivot ), pivot[2] / M3D_VectorMagnitude( pivot ) );
printf( "                PlaneDist %f ; Dotpr %f\n", M3D_PlanePoint( pivotplane, spherecenter ), angle );
#endif

  /* If spherecenter is under plane, invert angle direction */
  if( ( M3D_PlanePoint( pivotplane, spherecenter ) < 0.0 ) && ( angle < 0.99 ) && ( ( M3D_PlanePoint( triplane, vertex2 ) < 0.0 ) || ( M3D_PlanePoint( pivotplane, vertex2 ) < 0.0 ) ) )
    angle = -2.0 - angle;

  return angle;
}


enum
{
  BP_OVERRIDE_DENIED,
  BP_OVERRIDE_UNCERTAIN,
  BP_OVERRIDE_ACCEPTED
};

/* Assumes lock is already in place over {v1,v0}, direct hash access */
static int bpOverridePivotCheck( bpMainData *bpdata, bpi v0, bpi v1, bpi oldv2, bpi newv2, bpf uncertainrange )
{
  bpf midpoint[BP_AXIS_COUNT];
  bpf spherecenter[BP_AXIS_COUNT];
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT];
  bpf basepivot[BP_AXIS_COUNT], pivotplane[4], triplane[4];
  bpf oldangle, newangle, baseradius;
  bpf *vertex0, *vertex1, *vertex2;
  bpEdge edge;
  bpEdge *edgefind;

#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Override Check : %d,%d ; Old %d ; New %d\n", (int)v0, (int)v1, (int)oldv2, (int)newv2 );
#endif

  edge.v[0] = v1;
  edge.v[1] = v0;
  edgefind = mmHashDirectFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge );
  if( !( edgefind ) )
  {
#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override Check Success, No edge : %d,%d\n", (int)edge.v[0], (int)edge.v[1] );
#endif
    return BP_OVERRIDE_DENIED;
  }
  edge.v[2] = edgefind->v[2];

  /* Compute neighbor triangle normal and surface plane */
  vertex0 = ADDRESS( bpdata->vertex, edge.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, edge.v[1] * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, edge.v[2] * bpdata->vertexstride );

  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( triplane, vectb, vecta );
  M3D_VectorNormalize( triplane );
  triplane[3] = -( triplane[0]*vertex0[0] + triplane[1]*vertex0[1] + triplane[2]*vertex0[2] );

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override Check : Compute spherecenter of %d,%d,%d ; %d,%d,%d ; %d,%d,%d\n", (int)edge.v[0], (int)edge.v[1], (int)edge.v[2], (int)edge.v[1], (int)edge.v[2], (int)edge.v[0], (int)edge.v[2], (int)edge.v[0], (int)edge.v[1] );
printf( "              Hinge Override Check : Normal %f,%f,%f\n", triplane[0], triplane[1], triplane[2] );
#endif

  /* Compute sphere of neighbor triangle to the conflicting hinge */
  if( !( bpComputePivotSphere( vertex0, vecta, vectb, triplane, bpdata->ballradiuspivot, FLT_MAX, bpdata->balldistmin, spherecenter, &baseradius ) ) )
    return BP_OVERRIDE_ACCEPTED;

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override Check : Spherecenter %f,%f,%f\n", spherecenter[0], spherecenter[1], spherecenter[2] );
#endif

  /* Conflicting hinge midpoint and base pivot */
  midpoint[BP_AXIS_X] = 0.5 * ( vertex0[BP_AXIS_X] + vertex1[BP_AXIS_X] );
  midpoint[BP_AXIS_Y] = 0.5 * ( vertex0[BP_AXIS_Y] + vertex1[BP_AXIS_Y] );
  midpoint[BP_AXIS_Z] = 0.5 * ( vertex0[BP_AXIS_Z] + vertex1[BP_AXIS_Z] );
  M3D_VectorSubStore( basepivot, spherecenter, midpoint );
  M3D_VectorNormalize( basepivot );

  /* Compute pivot plane equation going through hinge and spherecenter */
  M3D_VectorCrossProduct( pivotplane, basepivot, vecta );
  M3D_VectorNormalize( pivotplane );
  pivotplane[3] = -( pivotplane[0]*midpoint[0] + pivotplane[1]*midpoint[1] + pivotplane[2]*midpoint[2] );

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override ; Pivotplane %f,%f,%f,%f\n", pivotplane[0], pivotplane[1], pivotplane[2], pivotplane[3] );
#endif

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override ; Midpoint %f,%f,%f\n", midpoint[0], midpoint[1], midpoint[2] );
#endif

  oldangle = bpOverridePivotAngle( bpdata, v0, v1, oldv2, midpoint, basepivot, pivotplane, triplane, bpdata->ballradiusoverride );
  newangle = bpOverridePivotAngle( bpdata, v0, v1, newv2, midpoint, basepivot, pivotplane, triplane, bpdata->ballradiusoverride );

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override ; Old Angle %f , New Angle %f\n", oldangle, newangle );
#endif

/*
  if( newangle < oldangle )
    return BP_OVERRIDE_DENIED;
  return BP_OVERRIDE_ACCEPTED;
*/

  if( newangle < ( oldangle + uncertainrange ) )
  {
    if( newangle < ( oldangle - uncertainrange ) )
    {
#ifdef DEBUG_VERBOSE_HINGE
printf( "                Hinge Override Denied\n" );
#endif
      return BP_OVERRIDE_DENIED;
    }
#ifdef DEBUG_VERBOSE_HINGE
printf( "                Hinge Override Uncertain\n" );
#endif
    return BP_OVERRIDE_UNCERTAIN;
  }
#ifdef DEBUG_VERBOSE_HINGE
printf( "                Hinge Override Accepted\n" );
#endif
  return BP_OVERRIDE_ACCEPTED;
}





static bpf bpOverridePlanarAngle( bpMainData *bpdata, bpi v0, bpi v1, bpi v2, bpf *triplane )
{
  bpf angle;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], normal[BP_AXIS_COUNT];
  bpf *vertex0, *vertex1, *vertex2;

  vertex0 = ADDRESS( bpdata->vertex, v0 * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, v1 * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, v2 * bpdata->vertexstride );
  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( normal, vectb, vecta );
  M3D_VectorNormalize( normal );

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Override Pivot Vertex %d,%d (%d)\n", (int)v0, (int)v1, (int)v2 );
printf( "              Override Pivot Try -> %.3f,%.3f,%.3f ; %.3f,%.3f,%.3f ; %.3f,%.3f,%.3f\n", vertex0[0], vertex0[1], vertex0[2], vertex1[0], vertex1[1], vertex1[2], vertex2[0], vertex2[1], vertex2[2] );
printf( "              Override Pivot Normal %f,%f,%f\n", normal[0], normal[1], normal[2] );
#endif

  angle = 1.0 - ( M3D_VectorDotProduct( triplane, normal ) / M3D_VectorMagnitude( normal ) );

#ifdef DEBUG_VERBOSE_HINGE
printf( "                PlaneDist %f ; Dotpr %f\n", M3D_PlanePoint( triplane, vertex2 ), angle );
#endif

  /* If spherecenter is under plane, invert angle direction */
  if( M3D_PlanePoint( triplane, vertex2 ) < 0.0 )
    angle = -2.0 - angle;

  return angle;
}

/* Assumes lock is already in place over {v1,v0}, direct hash access */
static int bpOverridePlanarCheck( bpMainData *bpdata, bpi v0, bpi v1, bpi oldv2, bpi newv2, bpf uncertainrange )
{
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT];
  bpf triplane[4];
  bpf oldangle, newangle;
  bpf *vertex0, *vertex1, *vertex2;
  bpEdge edge;
  bpEdge *edgefind;

#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Override Check : %d,%d ; Old %d ; New %d\n", (int)v0, (int)v1, (int)oldv2, (int)newv2 );
#endif

  edge.v[0] = v1;
  edge.v[1] = v0;
  edgefind = mmHashDirectFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge );
  if( !( edgefind ) )
  {
#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override Check Success, No edge : %d,%d\n", (int)edge.v[0], (int)edge.v[1] );
#endif
    return BP_OVERRIDE_DENIED;
  }
  edge.v[2] = edgefind->v[2];

  /* Compute neighbor triangle normal and surface plane */
  vertex0 = ADDRESS( bpdata->vertex, edge.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, edge.v[1] * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, edge.v[2] * bpdata->vertexstride );

  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( triplane, vectb, vecta );
  M3D_VectorNormalize( triplane );
  triplane[3] = -( triplane[0]*vertex0[0] + triplane[1]*vertex0[1] + triplane[2]*vertex0[2] );

#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override Check : Compute spherecenter of %d,%d,%d ; %d,%d,%d ; %d,%d,%d\n", (int)edge.v[0], (int)edge.v[1], (int)edge.v[2], (int)edge.v[1], (int)edge.v[2], (int)edge.v[0], (int)edge.v[2], (int)edge.v[0], (int)edge.v[1] );
printf( "              Hinge Override Check : Normal %f,%f,%f\n", triplane[0], triplane[1], triplane[2] );
#endif

  oldangle = bpOverridePlanarAngle( bpdata, v0, v1, oldv2, triplane );
  newangle = bpOverridePlanarAngle( bpdata, v0, v1, newv2, triplane );


/*
  bpf hingeplane[4];
  if( bpfabs( newangle - oldangle ) < 0.1 )
  {
    M3D_VectorCrossProduct( hingeplane, triplane, vecta );
    M3D_VectorNormalize( hingeplane );
    hingeplane[3] = -( hingeplane[0]*vertex0[0] + hingeplane[1]*vertex0[1] + hingeplane[2]*vertex0[2] );

bpf olddist, newdist;

    vtarget = ADDRESS( bpdata->vertex, oldv2 * bpdata->vertexstride );
    olddist = M3D_PlanePoint( hingeplane, vtarget );
    vtarget = ADDRESS( bpdata->vertex, newv2 * bpdata->vertexstride );
    newdist = M3D_PlanePoint( hingeplane, vtarget );

printf( "COPLANAR %f %f : DIST %f %f\n", oldangle, newangle, olddist, newdist );

  }
*/


#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override ; Old Angle %f , New Angle %f\n", oldangle, newangle );
#endif

/*
  if( newangle < oldangle )
    return BP_OVERRIDE_DENIED;
  return BP_OVERRIDE_ACCEPTED;
*/

  if( newangle < ( oldangle + uncertainrange ) )
  {
    if( newangle < ( oldangle - uncertainrange ) )
    {
#ifdef DEBUG_VERBOSE_HINGE
printf( "                Hinge Override Denied\n" );
#endif
      return BP_OVERRIDE_DENIED;
    }
#ifdef DEBUG_VERBOSE_HINGE
printf( "                Hinge Override Uncertain\n" );
#endif
    return BP_OVERRIDE_UNCERTAIN;
  }
#ifdef DEBUG_VERBOSE_HINGE
printf( "                Hinge Override Accepted\n" );
#endif
  return BP_OVERRIDE_ACCEPTED;
}



typedef struct
{
  bpi v0, v1, v2;
  bpf radsquared;
  bpf plane[4];
  bpf coplanardist;
} bpOverlapCheck;

static int bpOverrideOverlapCheckPick( bpMainData *bpdata, void *opaquepointer, bpi vertexindex, bpf distsquared )
{
  bpf *vertex;
  bpOverlapCheck *overlapcheck;

  overlapcheck = opaquepointer;
  if( ( vertexindex == overlapcheck->v0 ) || ( vertexindex == overlapcheck->v1 ) || ( vertexindex == overlapcheck->v2 ) )
    return 1;
  if( distsquared > overlapcheck->radsquared )
  {
    vertex = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
    if( bpfabs( M3D_PlanePoint( overlapcheck->plane, vertex ) ) < overlapcheck->coplanardist )
      return 1;
  }

  return 0;
}


static int bpOverrideOverlapCheck( bpMainData *bpdata, bpi v0, bpi v1, bpi v2 )
{
  bpf baseradius;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], center[BP_AXIS_COUNT];
  bpf *vertex0, *vertex1, *vertex2;
  bpOverlapCheck overlapcheck;

  vertex0 = ADDRESS( bpdata->vertex, v0 * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, v1 * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, v2 * bpdata->vertexstride );

  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( overlapcheck.plane, vectb, vecta );
  M3D_VectorNormalize( overlapcheck.plane );
  if( !( bpComputePivotSphere( vertex0, vecta, vectb, overlapcheck.plane, bpdata->ballradiuspivot, bpdata->ballradiusmin, bpdata->balldistmin, center, &baseradius ) ) )
    return 0;

  overlapcheck.v0 = v0;
  overlapcheck.v1 = v1;
  overlapcheck.v2 = v2;
  overlapcheck.radsquared = bpdata->seedballradius * bpdata->seedballradius;
  overlapcheck.plane[3] = -M3D_VectorDotProduct( overlapcheck.plane, vertex0 );
  overlapcheck.coplanardist = bpdata->seedcoplanardist;

  /* Check if any vertex overlaps the space */
  if( !( bpGridPickRadius( bpdata, bpdata->grid, center, bpdata->ballradiusmin, bpOverrideOverlapCheckPick, (void *)&overlapcheck ) ) )
    return 0;

  return 1;
}



static bpf bpOverrideNormalAgreement( bpMainData *bpdata, bpi v0, bpi v1, bpi v2 )
{
  bpf agreementsum;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], normal[BP_AXIS_COUNT];
  bpf *vertex0, *vertex1, *vertex2;
  bpf *normal0, *normal1, *normal2;

  vertex0 = ADDRESS( bpdata->vertex, v0 * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, v1 * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, v2 * bpdata->vertexstride );

  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( normal, vectb, vecta );
  M3D_VectorNormalize( normal );

  normal0 = ADDRESS( bpdata->normal, v0 * bpdata->normalstride );
  normal1 = ADDRESS( bpdata->normal, v1 * bpdata->normalstride );
  normal2 = ADDRESS( bpdata->normal, v2 * bpdata->normalstride );

  agreementsum  = M3D_VectorDotProduct( normal, normal0 );
  agreementsum += M3D_VectorDotProduct( normal, normal1 );
  agreementsum += M3D_VectorDotProduct( normal, normal2 );

  return agreementsum;
}

static bpf bpOverrideNormalCheck( bpMainData *bpdata, bpi v0, bpi v1, bpi oldv2, bpi newv2 )
{
  bpf oldsum, newsum;

  oldsum = bpOverrideNormalAgreement( bpdata, v0, v1, oldv2 );
  newsum = bpOverrideNormalAgreement( bpdata, v0, v1, newv2 );

#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Override Normal Check : %d,%d ; Old %d (%f) ; New %d (%f)\n", (int)v0, (int)v1, (int)oldv2, oldsum, (int)newv2, newsum );
#endif

  if( newsum > oldsum )
  {
#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override Normal Accepted\n" );
#endif
    return BP_OVERRIDE_ACCEPTED;
  }
#ifdef DEBUG_VERBOSE_HINGE
printf( "              Hinge Override Normal Denied\n" );
#endif
  return BP_OVERRIDE_DENIED;
}



/********/



enum
{
  BP_VALIDATE_TRIANGLE_FAILURE,
  BP_VALIDATE_TRIANGLE_SUCCESS,
  BP_VALIDATE_TRIANGLE_DISCARD
};

/* Validate the addition of a triangle's hinges */
static int bpValidateTriangle( bpMainData *bpdata, bpThreadData *tdata, bpPatch *patch, bpTriangle *triangle, int32_t grade, int hingeflags )
{
  int index, dupcount, linkflags, overrideret, conflictdepth;
  bpEdge edge[3], edgelink, edgeinv;
  bpEdge edgedelbuffer[6];
  bpEdge *edgefind[3], *edgedel, *edgedelend;
  bpi edgeinvindex[3], oldv2, newv2;
  bpTriangle *deltri;
  mmHashLock hashlock;
  mmHashLockRange *lockrange, lockrangebuffer[3+1+3+6];

#ifdef DEBUG_VERBOSE_HINGE
printf( "        Validate Triangle : %d,%d,%d ; grade 0x%x\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2], grade );
#endif

  conflictdepth = BP_HINGE_GET_CONFLICTDEPTH( hingeflags );

  edge[0].v[0] = triangle->v[0];
  edge[0].v[1] = triangle->v[1];
  edge[0].v[2] = triangle->v[2];
  edge[0].triangle = triangle;
  edge[1].v[0] = triangle->v[1];
  edge[1].v[1] = triangle->v[2];
  edge[1].v[2] = triangle->v[0];
  edge[1].triangle = triangle;
  edge[2].v[0] = triangle->v[2];
  edge[2].v[1] = triangle->v[0];
  edge[2].v[2] = triangle->v[1];
  edge[2].triangle = triangle;

  edgeinvindex[0] = -1;
  edgeinvindex[1] = -1;
  edgeinvindex[2] = -1;

  edgedel = edgedelbuffer;
  for( ; ; )
  {
    restart:

    mmHashLockInit( &hashlock, 3 );
    lockrange = lockrangebuffer;
    for( index = 0 ; index < 3 ; index++ )
    {
      mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edge[index], &hashlock, lockrange );
      lockrange++;
      /* Do we need to lock opposite edges? Required to guarantee overrides in a multithreaded safe way. */
      if( edgeinvindex[index] != -1 )
      {
        edgeinv.v[0] = edge[index].v[1];
        edgeinv.v[1] = edge[index].v[0];
        mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edgeinv, &hashlock, lockrange );
        lockrange++;
      }
    }

    /* If we have edges planned for deletion, we need to lock their hash entries too. */
    edgedelend = edgedel;
    for( edgedel = edgedelbuffer ; edgedel < edgedelend ; edgedel++ )
    {
      mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edgedel, &hashlock, lockrange );
      lockrange++;
    }
    edgedel = edgedelbuffer;

    /* If we require a check for an inverse linked edge, lock that too. */
    if( hingeflags & BP_HINGE_FLAGS_PIVOT )
    {
      edgelink.v[0] = triangle->v[1];
      edgelink.v[1] = triangle->v[0];
      mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edgelink, &hashlock, lockrange );
      lockrange++;
    }
    mmHashLockAcquire( bpdata->hingehashtable, &bpHingeHashEdge, &hashlock );

    /* Check for inverse link */
    if( ( hingeflags & BP_HINGE_FLAGS_PIVOT ) && !( mmHashDirectFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgelink ) ) )
    {
#ifdef DEBUG_VERBOSE_HINGE
printf( "          Hinge Link Check Failure : %d,%d\n", (int)edgelink.v[0], (int)edgelink.v[1] );
#endif
      mmHashLockRelease( bpdata->hingehashtable, &hashlock );
      return BP_VALIDATE_TRIANGLE_DISCARD;
    }

    linkflags = 0x0;
    dupcount = 0;
    for( index = 0 ; index < 3 ; index++ )
    {
      edgefind[index] = mmHashDirectFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge[index] );
      if( !( edgefind[index] ) )
        continue;
      linkflags |= ( 1 << index );
      if( edgefind[index]->v[2] == edge[index].v[2] )
      {
        dupcount++;
        continue;
      }

      /* Add edges to list of deleted edges, the opposite edges of overridden triangles */
      edgedel->v[0] = edgefind[index]->v[1];
      edgedel->v[1] = edgefind[index]->v[2];
      edgedel->v[2] = edgefind[index]->v[0];
      edgedel->triangle = edgefind[index]->triangle;
      edgedel++;
      edgedel->v[0] = edgefind[index]->v[2];
      edgedel->v[1] = edgefind[index]->v[0];
      edgedel->v[2] = edgefind[index]->v[1];
      edgedel->triangle = edgefind[index]->triangle;
      edgedel++;

      /* Record the v2 of the triangle on the conflicting edge, restart with wider hash lock */
      if( edgeinvindex[index] != edgefind[index]->v[2] )
      {
        edgeinvindex[index] = edgefind[index]->v[2];
        mmHashLockRelease( bpdata->hingehashtable, &hashlock );
        goto restart;
      }

      oldv2 = edgefind[index]->v[2];
      newv2 = edge[index].v[2];

      /* Force hinges can override anything */
      if( ( hingeflags & BP_HINGE_FLAGS_FORCE ) && ( BP_CONFLICTDEPTH_CHECK_FORCEOVERRIDE(conflictdepth) ) )
      {
#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Force Override : %d,%d (%d->%d), grade 0x%x->0x%x\n", (int)edge[index].v[0], (int)edge[index].v[1], (int)oldv2, (int)newv2, edgefind[index]->grade, grade );
#endif
        continue;
      }

      /* Is the new pivot better than the previous one? */
#ifdef CONFIG_ENABLE_OVERRIDE_VALIDATION

      if( BP_CONFLICTDEPTH_CHECK_NORMAL( conflictdepth ) )
        overrideret = bpOverrideNormalCheck( bpdata, edge[index].v[0], edge[index].v[1], oldv2, newv2 );
      else if( BP_CONFLICTDEPTH_CHECK_PIVOT( conflictdepth ) )
        overrideret = bpOverridePivotCheck( bpdata, edge[index].v[0], edge[index].v[1], oldv2, newv2, bpdata->pivotoverriderange );
      else if( BP_CONFLICTDEPTH_CHECK_PLANAR( conflictdepth ) )
        overrideret = bpOverridePlanarCheck( bpdata, edge[index].v[0], edge[index].v[1], oldv2, newv2, bpdata->pivotoverriderange );
      else
        overrideret = BP_OVERRIDE_DENIED;

      if( BP_CONFLICTDEPTH_CHECK_OVERLAP( conflictdepth ) )
      {
        if( ( overrideret != BP_OVERRIDE_DENIED ) && ( ( bpOverrideOverlapCheck( bpdata, edge[index].v[0], edge[index].v[1], oldv2 ) || !( bpOverrideOverlapCheck( bpdata, edge[index].v[0], edge[index].v[1], newv2 ) ) ) ) )
        {
#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Override Overlap Denied %d,%d (%d->%d)\n", (int)edge[index].v[0], (int)edge[index].v[1], (int)oldv2, (int)newv2 );
#endif
          overrideret = BP_OVERRIDE_DENIED;
        }
      }

#else
      overrideret = BP_OVERRIDE_UNCERTAIN;
#endif

      if( overrideret == BP_OVERRIDE_DENIED )
        goto failure;
      else if( overrideret == BP_OVERRIDE_UNCERTAIN )
      {
#ifdef CONFIG_ENABLE_GRADE_OVERRIDE
        if( grade >= BP_EDGE_GRADE_REPAIR )
        {
          if( edgefind[index]->grade >= BP_EDGE_GRADE_REPAIR_CAP )
            goto failure;
          if( grade <= edgefind[index]->grade )
          {
            grade = BP_EDGE_GRADE_REPAIR + 1;
#ifdef DEBUG_VERBOSE_HINGE
printf( "          Hinge Join Grade Promotion : 0x%x\n", grade );
#endif
          }
        }
        else if( grade <= edgefind[index]->grade )
        {
#ifdef DEBUG_VERBOSE_HINGE
printf( "          Hinge Grade Override Failure : %d,%d (%d;%d), grade 0x%x < 0x%x\n", (int)edge[index].v[0], (int)edge[index].v[1], (int)oldv2, (int)newv2, grade, edgefind[index]->grade );
#endif
          goto failure;
        }
#endif
      }
      else
      {
/*
#ifdef CONFIG_ENABLE_GRADE_OVERRIDE
        if( grade < edgefind[index]->grade )
        {
#ifdef DEBUG_VERBOSE_HINGE
printf( "          Hinge Grade Override Failure : %d,%d (%d;%d), grade 0x%x < 0x%x\n", (int)edge[index].v[0], (int)edge[index].v[1], (int)edgefind[index]->v[2], (int)edge[index].v[2], grade, edgefind[index]->grade );
#endif
          goto failure;
        }
#endif
*/
      }


#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Grade Override : %d,%d (%d->%d), grade 0x%x->0x%x\n", (int)edge[index].v[0], (int)edge[index].v[1], (int)oldv2, (int)newv2, edgefind[index]->grade, grade );
#endif
    }

    /* Lock succesful */
    break;
  }

  if( dupcount == 3 )
  {
#ifdef DEBUG_VERBOSE_HINGE
printf( "            Duplicate Triangle, Upgraded Grade 0x%x->0x%x\n", edgefind[0]->grade, grade );
#endif
    for( index = 0 ; index < 3 ; index++ )
    {
      if( edgefind[index] )
        edgefind[index]->grade = grade;
    }

    mmHashLockRelease( bpdata->hingehashtable, &hashlock );
    return BP_VALIDATE_TRIANGLE_DISCARD;
  }

  for( index = 0 ; index < 3 ; index++ )
  {
    if( edgefind[index] )
    {
      edgefind[index]->grade = grade;
      edgefind[index]->v[2] = edge[index].v[2];
      edgefind[index]->triangle = triangle;
      continue;
    }
    edge[index].grade = grade;
#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Accepted : %d,%d (%d), grade 0x%x\n", (int)edge[index].v[0], (int)edge[index].v[1], (int)edge[index].v[2], grade );
#endif
    if( mmHashDirectAddEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge[index], 1 ) != MM_HASH_SUCCESS )
    {
      printf( "INTERNAL ERROR %s:%d\n", __FILE__, __LINE__ );
      fflush( stdout );
      exit( 1 );
    }
  }

  edgedelend = edgedel;
  for( edgedel = edgedelbuffer ; edgedel < edgedelend ; edgedel++ )
  {
    /* Delete orphaned triangle away */
    deltri = edgedel->triangle;
    if( deltri->v[0] != -1 )
    {
      deltri->v[0] = -1;
      mmListRemove( deltri, offsetof(bpTriangle,list) );
    }
#ifdef DEBUG_VERBOSE_HINGE
printf( "            Hinge Orphaned : %d,%d (%d)\n", (int)edgedel->v[0], (int)edgedel->v[1], (int)edgedel->v[2] );
#endif
    if( mmHashDirectDeleteEntry( bpdata->hingehashtable, &bpHingeHashFull, edgedel, 0 ) != MM_HASH_SUCCESS )
    {
      printf( "INTERNAL ERROR %s:%d\n", __FILE__, __LINE__ );
      fflush( stdout );
    }
  }

  mmHashLockRelease( bpdata->hingehashtable, &hashlock );

#ifdef CONFIG_ENABLE_HINGE_ORPHAN_REBUILD
  for( edgedel = edgedelbuffer ; edgedel < edgedelend ; edgedel++ )
    bpRebuildHinge( bpdata, tdata, patch, edgedel->v[0], edgedel->v[1], edgedel->v[2], conflictdepth );
#endif

  return BP_VALIDATE_TRIANGLE_SUCCESS;

/****/

  failure:
  mmHashLockRelease( bpdata->hingehashtable, &hashlock );
  return BP_VALIDATE_TRIANGLE_FAILURE;
}



/********/



#define BP_PIVOT_STATUS_FLAGS_BAD (0x1)
#define BP_PIVOT_STATUS_FLAGS_FALLBACK (0x2)
#define BP_PIVOT_STATUS_FLAGS_INVALID (0x8000)


static int bpPivotComputeStatusFlags( bpMainData *bpdata, bpi v0, bpi v1, bpi v2, bpf *trinormal )
{
  int retvalue;
  bpf agreement, agreementsum, dist;
  bpf *normal0, *normal1, *normal2;
  bpf *vertex0, *vertex1, *vertex2;

  retvalue = 0;
  agreementsum = 0.0;
  normal0 = ADDRESS( bpdata->normal, v0 * bpdata->normalstride );
  agreement = M3D_VectorDotProduct( trinormal, normal0 );
  if( agreement < bpdata->pivotnormalagreementsoft )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Bad : Disagreement with vertex0 normal %f < %f ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", M3D_VectorDotProduct( trinormal, normal0 ), bpdata->pivotnormalagreementsoft, trinormal[0], trinormal[1], trinormal[2], normal0[0], normal0[1], normal0[2] );
#endif
    if( agreement < bpdata->pivotnormalagreementhard )
      return BP_PIVOT_STATUS_FLAGS_INVALID;
    retvalue = BP_PIVOT_STATUS_FLAGS_BAD;
  }
  agreementsum += agreement;
  normal1 = ADDRESS( bpdata->normal, v1 * bpdata->normalstride );
  agreement = M3D_VectorDotProduct( trinormal, normal1 );
  if( agreement < bpdata->pivotnormalagreementsoft )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Bad : Disagreement with vertex1 normal %f < %f ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", M3D_VectorDotProduct( trinormal, normal1 ), bpdata->pivotnormalagreementsoft, trinormal[0], trinormal[1], trinormal[2], normal1[0], normal1[1], normal1[2] );
#endif
    if( agreement < bpdata->pivotnormalagreementhard )
      return BP_PIVOT_STATUS_FLAGS_INVALID;
    retvalue = BP_PIVOT_STATUS_FLAGS_BAD;
  }
  agreementsum += agreement;
  normal2 = ADDRESS( bpdata->normal, v2 * bpdata->normalstride );
  agreement = M3D_VectorDotProduct( trinormal, normal2 );
  if( agreement < bpdata->pivotnormalagreementsoft )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Bad : Disagreement with vertex2 normal %f < %f ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", M3D_VectorDotProduct( trinormal, normal2 ), bpdata->pivotnormalagreementsoft, trinormal[0], trinormal[1], trinormal[2], normal2[0], normal2[1], normal2[2] );
#endif
    if( agreement < bpdata->pivotnormalagreementhard )
      return BP_PIVOT_STATUS_FLAGS_INVALID;
    retvalue = BP_PIVOT_STATUS_FLAGS_BAD;
  }
  agreementsum += agreement;

  if( agreementsum < bpdata->pivotnormalagreementsumsoft )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Bad : Overall disagreement with normals %f < %f (%f)\n", agreementsum, bpdata->pivotnormalagreementsumsoft, bpdata->pivotnormalagreementsumhard );
#endif
    if( agreementsum < bpdata->pivotnormalagreementsumhard )
      return BP_PIVOT_STATUS_FLAGS_INVALID;
    retvalue = BP_PIVOT_STATUS_FLAGS_BAD;
  }



  retvalue |= BP_PIVOT_STATUS_FLAGS_FALLBACK;
  vertex0 = ADDRESS( bpdata->vertex, v0 * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, v1 * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, v2 * bpdata->vertexstride );
  agreement = M3D_VectorDotProduct( normal0, normal2 );
  if( agreement < -0.5 )
  {
    dist = M3D_VectorDotProduct( normal0, vertex2 ) - M3D_VectorDotProduct( normal0, vertex0 );
    if( dist < 0.0 )
      goto done;
    dist = M3D_VectorDotProduct( normal2, vertex0 ) - M3D_VectorDotProduct( normal2, vertex2 );
    if( dist < 0.0 )
      goto done;
  }
  agreement = M3D_VectorDotProduct( normal1, normal2 );
  if( agreement < -0.5 )
  {
    dist = M3D_VectorDotProduct( normal1, vertex2 ) - M3D_VectorDotProduct( normal1, vertex1 );
    if( dist < 0.0 )
      goto done;
    dist = M3D_VectorDotProduct( normal2, vertex1 ) - M3D_VectorDotProduct( normal2, vertex2 );
    if( dist < 0.0 )
      goto done;
  }
  retvalue &= ~BP_PIVOT_STATUS_FLAGS_FALLBACK;
  done:



#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Pivot Triangle Normal Agreement %f ( %f ; %f )\n", agreementsum, bpdata->pivotnormalagreementsumsoft, bpdata->pivotnormalagreementsumhard );
#endif

  return retvalue;
}



/********/



#define BP_HITBUFFER_STATIC_COUNT (64)

typedef struct
{
  bpi vertexindex;
  bpf pivotangle;
  bpf baseradius;
  bpf spherecenter[BP_AXIS_COUNT];
  bpf normal[BP_AXIS_COUNT];
  int statusflags;
} bpHit;

/* We need to track a list of alternative v2 vertices strictly sorted by angle gap in case of coplanar pivot targets. Grrr. */
/* Dynamic memory allocation supported just in case */
typedef struct
{
  int allocflag;
  int hitcount;
  int hitalloc;
  bpHit hit[2*BP_HITBUFFER_STATIC_COUNT];
  bpHit *hitlist;
  bpHit *hitextra;
} bpHitBuffer;

typedef struct
{
  bpTriangle triangle;

  bpf spherecenter[3];
  bpf normal[BP_AXIS_COUNT];

/*
  bpf midpoint[BP_AXIS_COUNT];
  bpf vecta[BP_AXIS_COUNT];
*/

  bpHitBuffer hitbuffer;
  int spacelockflag;
  bpSpaceLock spacelock;
} bpPivotResult;


static void bpHitBufferInit( bpHitBuffer *buffer )
{
  buffer->allocflag = 0;
  buffer->hitcount = 0;
  buffer->hitalloc = BP_HITBUFFER_STATIC_COUNT;
  buffer->hitlist = buffer->hit;
  buffer->hitextra = &buffer->hit[BP_HITBUFFER_STATIC_COUNT];
  return;
}

static void bpHitBufferReset( bpHitBuffer *buffer )
{
  buffer->hitcount = 0;
  return;
}

static inline void bpHitBufferSet( bpHitBuffer *buffer, bpi vertexindex, bpf pivotangle, bpf baseradius, bpf *spherecenter, bpf *normal, int statusflags )
{
  bpHit *hit;
  hit = buffer->hitlist;
  hit->vertexindex = vertexindex;
  hit->pivotangle = pivotangle;
  hit->baseradius = baseradius;
  M3D_VectorCopy( hit->spherecenter, spherecenter );
  M3D_VectorCopy( hit->normal, normal );
  hit->statusflags = statusflags;
  buffer->hitcount = 1;
  return;
}

static void bpHitBufferAdd( bpHitBuffer *buffer, bpi vertexindex, bpf pivotangle, bpf baseradius, bpf *spherecenter, bpf *normal, int statusflags )
{
  size_t memsize;
  bpHit *hitlist, *hitstore;

  if( buffer->hitcount >= buffer->hitalloc )
  {
    buffer->hitalloc <<= 2;
    memsize = buffer->hitalloc * sizeof(bpHit);
    if( !( buffer->allocflag ) )
    {
      hitlist = buffer->hitlist;
      buffer->hitlist = malloc( memsize );
      buffer->hitextra = malloc( memsize );
      memcpy( buffer->hitlist, hitlist, buffer->hitcount * sizeof(bpHit) );
      buffer->allocflag = 1;
    }
    else
    {
      buffer->hitlist = realloc( buffer->hitlist, memsize );
      buffer->hitextra = realloc( buffer->hitextra, memsize );
    }
  }

  /* Write our entry */
  hitstore = &buffer->hitlist[buffer->hitcount];
  hitstore->vertexindex = vertexindex;
  hitstore->pivotangle = pivotangle;
  hitstore->baseradius = baseradius;
  M3D_VectorCopy( hitstore->spherecenter, spherecenter );
  M3D_VectorCopy( hitstore->normal, normal );
  hitstore->statusflags = statusflags;
  buffer->hitcount++;

  return;
}

/*
static void bpHitBufferCopy( bpHitBuffer *dstbuffer, bpHitBuffer *srcbuffer )
{
  size_t memsize;

  memsize = srcbuffer->hitcount * sizeof(bpHit);
  if( srcbuffer->hitcount > BP_HITBUFFER_STATIC_COUNT )
  {
    dstbuffer->hitalloc = srcbuffer->hitcount;
    dstbuffer->hitcount = srcbuffer->hitcount;
    dstbuffer->hitlist = malloc( memsize );
    dstbuffer->hitextra = malloc( memsize );
    dstbuffer->allocflag = 1;
  }
  memcpy( dstbuffer->hitlist, srcbuffer->hitlist, memsize );

  return;
}
*/

static void bpHitBufferFree( bpHitBuffer *buffer )
{
  buffer->hitcount = 0;
  if( buffer->allocflag )
  {
    free( buffer->hitlist );
    free( buffer->hitextra );
    buffer->hitlist = buffer->hit;
    buffer->hitextra = &buffer->hit[BP_HITBUFFER_STATIC_COUNT];
    buffer->hitalloc = BP_HITBUFFER_STATIC_COUNT;
  }
  return;
}

static void bpHitBufferPack( bpHitBuffer *buffer )
{
  bpHit *hitsrc, *hitdst, *hitsrcend, *hitdstbase;

  hitsrc = buffer->hitlist;
  hitdst = buffer->hitextra;
  hitsrcend = &hitsrc[buffer->hitcount];
  hitdstbase = hitdst;
  for( ; hitsrc < hitsrcend ; hitsrc++ )
  {
    if( hitsrc->vertexindex == -1 )
      continue;
    *hitdst = *hitsrc;
    hitdst++;
  }

  buffer->hitextra = buffer->hitlist;
  buffer->hitlist = hitdstbase;
  buffer->hitcount = hitdst - hitdstbase;
  return;
}


static inline void bpHitBufferSortCopy( bpHit *dst, bpHit *src )
{
  *dst = *src;
  return;
}

static inline int bpHitBufferSortCmp( bpHit *t0, bpHit *t1 )
{
#ifdef CONFIG_ENABLE_PIVOT_NORMAL_PRIORITIZATION
  if( ( t0->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) > ( t1->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) )
    return 1;
  if( ( t0->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) < ( t1->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) )
    return 0;
#endif
  if( t0->pivotangle > t1->pivotangle )
    return 0;
  else
    return 1;
}

#define MSORT_MAIN bpHitBufferSortInternal
#define MSORT_CMP bpHitBufferSortCmp
#define MSORT_COPY bpHitBufferSortCopy
#define MSORT_TYPE bpHit
#include "ccmergesort.h"
#undef MSORT_MAIN
#undef MSORT_CMP
#undef MSORT_COPY
#undef MSORT_TYPE


static inline int bpHitBufferSortFullCmp( bpHit *t0, bpHit *t1 )
{
#ifdef CONFIG_ENABLE_PIVOT_NORMAL_PRIORITIZATION
  if( ( t0->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) > ( t1->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) )
    return 1;
  if( ( t0->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) < ( t1->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) )
    return 0;
#endif
  if( t0->baseradius == t1->baseradius )
  {
    if( t0->pivotangle > t1->pivotangle )
      return 0;
    else
      return 1;
  }
  else if( t0->baseradius < t1->baseradius )
    return 0;
  else
    return 1;
}

#define MSORT_MAIN bpHitBufferSortFullInternal
#define MSORT_CMP bpHitBufferSortFullCmp
#define MSORT_COPY bpHitBufferSortCopy
#define MSORT_TYPE bpHit
#include "ccmergesort.h"
#undef MSORT_MAIN
#undef MSORT_CMP
#undef MSORT_COPY
#undef MSORT_TYPE


static void bpHitBufferSort( bpHitBuffer *buffer )
{
  bpHit *hitlist;

  hitlist = bpHitBufferSortInternal( buffer->hitlist, buffer->hitextra, buffer->hitcount );
  if( hitlist == buffer->hitextra )
  {
    buffer->hitextra = buffer->hitlist;
    buffer->hitlist = hitlist;
  }

  return;
}


static void bpHitBufferSortFull( bpHitBuffer *buffer )
{
  bpHit *hitlist;

  hitlist = bpHitBufferSortFullInternal( buffer->hitlist, buffer->hitextra, buffer->hitcount );
  if( hitlist == buffer->hitextra )
  {
    buffer->hitextra = buffer->hitlist;
    buffer->hitlist = hitlist;
  }

  return;
}



/********/



/*
Triangle Seeding Step
- Find a point not yet used by reconstruction
- Consider all pairs of points near p, from closest to farthest
- Attempt building triangles using set of 3 points
- Orient triangle properly to agree with point normals
- Compute sphere touching all 3 points on the correct side of triangle
- Verify that no other vertex lie inside the sphere
*/


/* Callback to build the thread's cluster of vertices within radius of point */
static int bpNewTriangleSeedClusterPick( bpMainData *bpdata, void *opaquepointer, bpi vertexindex, bpf distsquared )
{
  bpCluster *cluster;
  bpClusterVertex *cv;
  cluster = opaquepointer;

  /* Don't pick already used vertices */
  /* This also skips our current neighborhood test vertex */
  if( mmBitMapGet( &bpdata->vertexmap, vertexindex ) )
  {
#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed Pick : %d Skipped\n", vertexindex );
#endif
    return 1;
  }

#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed Pick : %d, distsquared %f (%f)\n", vertexindex, distsquared, bpfsqrt( distsquared ) );
#endif

  /* Store vertexindes and distance squared */
  if( cluster->count >= cluster->alloc )
    bpClusterGrow( cluster, cluster->count << 1 );
  cv = &cluster->vertex[cluster->count++];
  cv->vertexindex = vertexindex;
  cv->distsquared = distsquared;

  return 1;
}



static inline int bpClusterVertexCmp( bpClusterVertex *t0, bpClusterVertex *t1 )
{
  if( t0->distsquared < t1->distsquared )
    return 0;
  else
    return 1;
}

static inline void bpClusterVertexCopy( bpClusterVertex *dst, bpClusterVertex *src )
{
  *dst = *src;
  return;
}

#define MSORT_MAIN bpClusterVertexSort
#define MSORT_CMP bpClusterVertexCmp
#define MSORT_COPY bpClusterVertexCopy
#define MSORT_TYPE bpClusterVertex
#include "ccmergesort.h"
#undef MSORT_MAIN
#undef MSORT_CMP
#undef MSORT_COPY
#undef MSORT_TYPE



typedef struct
{
  bpi v0, v1, v2;
  bpf radsquared;
  bpf plane[4];
  bpf coplanardist;
} bpSeedCheck;

static int bpNewTriangleSeedCheckPick( bpMainData *bpdata, void *opaquepointer, bpi vertexindex, bpf distsquared )
{
  bpf *vertex;
  bpSeedCheck *seedcheck;

  seedcheck = opaquepointer;
  if( ( vertexindex == seedcheck->v0 ) || ( vertexindex == seedcheck->v1 ) || ( vertexindex == seedcheck->v2 ) )
    return 1;
  if( distsquared > seedcheck->radsquared )
  {
    vertex = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
    /* Accept only if coplanar and not already used */
    if( bpfabs( M3D_PlanePoint( seedcheck->plane, vertex ) ) < seedcheck->coplanardist )
    {
      if( mmBitMapGet( &bpdata->vertexmap, vertexindex ) )
      {
#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed Check : Coplanar Vertex %d Already Linked\n", (int)vertexindex );
#endif
        return 0;
      }
#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed Check : Coplanar Vertex %d Accepted : %f,%f,%f\n", (int)vertexindex, vertex[0], vertex[1], vertex[2] );
#endif
      return 1;
    }
  }

#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed Spoiled : Overlap By Vertex %d : %f < %f\n", (int)vertexindex, distsquared, seedcheck->radsquared );
#endif

  return 0;
}



static void bpMathPlaneEquation( bpf *plane, bpf *vertex0, bpf *vertex1, bpf *vertex2 )
{
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT];
  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );
  M3D_VectorCrossProduct( plane, vecta, vectb );
  M3D_VectorNormalize( plane );
  plane[3] = -( plane[0]*vertex0[0] + plane[1]*vertex0[1] + plane[2]*vertex0[2] );
  return;
}


static int bpNewTriangleSeedMarkPick( bpMainData *bpdata, void *opaquepointer, bpi vertexindex, bpf distsquared )
{
  mmBitMapSet( &bpdata->vertexmap, vertexindex );
  return 1;
}


/* Attempt to form a new seed triangle using vertex */
static int bpNewTriangleSeedVertex( bpMainData *bpdata, bpThreadData *tdata, bpi v0, bpPivotResult *result )
{
  int flipflag;
  ssize_t clcount1, clcount2;
  bpi v1, v2;
  bpCluster *cluster;
  bpClusterVertex *clvertex, *clvertex1, *clvertex2;
  bpf *vertex0, *vertex1, *vertex2;
  bpf *normal0, *normal1, *normal2;
  bpf hingenormal[BP_AXIS_COUNT], hintnormal[BP_AXIS_COUNT];
  bpf seedballradius, radsquared, agreement, agreementsum;
  bpSeedCheck seedcheck;

  /* Mark the vertex as tested */
  mmBitMapSet( &bpdata->vertexmap, v0 );

  vertex0 = ADDRESS( bpdata->vertex, v0 * bpdata->vertexstride );
  normal0 = ADDRESS( bpdata->normal, v0 * bpdata->normalstride );

#ifdef DEBUG_VERBOSE_SEED
printf( "  Seed Try Vertex %d : %f %f %f\n", (int)v0, vertex0[0], vertex0[1], vertex0[2] );
#endif

  /* Build a list of all vertices near our vertex within a distance of 2.0 * ball radius */
  cluster = &tdata->cluster;
  cluster->count = 0;
  bpGridPickRadius( bpdata, bpdata->grid, vertex0, 2.0 * bpdata->ballradiusmin, bpNewTriangleSeedClusterPick, (void *)cluster );

#ifdef DEBUG_VERBOSE_SEED
printf( "  Seed Cluster Count : %d\n", (int)cluster->count );
#endif

  /* This vertex is alone in the world, leave it be */
  if( cluster->count < 2 )
    return 0;

  /* Sort cluster vertices from nearest to farthest */
  clvertex = cluster->vertex;
  if( cluster->count >= 6 )
    clvertex = bpClusterVertexSort( cluster->vertex, cluster->vertexextra, cluster->count );

#ifdef DEBUG_VERBOSE_SEED
  clvertex1 = clvertex;
  for( clcount1 = 0 ; clcount1 < cluster->count ; clcount1++, clvertex1++ )
    printf( "    Seed Cluster Sort %d : %d, distsquared %f\n", (int)clcount1, (int)clvertex1->vertexindex, clvertex1->distsquared );
#endif

  /* Square of ball radius */
  seedballradius = bpdata->seedballradius;
  radsquared = seedballradius * seedballradius;

  result->spacelockflag = 0;

  clvertex1 = clvertex;
  for( clcount1 = cluster->count ; clcount1 > 0 ; clcount1--, clvertex1++ )
  {
    v1 = clvertex1->vertexindex;
    vertex1 = ADDRESS( bpdata->vertex, v1 * bpdata->vertexstride );
    normal1 = ADDRESS( bpdata->normal, v1 * bpdata->normalstride );

    hingenormal[BP_AXIS_X] = normal0[BP_AXIS_X] + normal1[BP_AXIS_X];
    hingenormal[BP_AXIS_Y] = normal0[BP_AXIS_Y] + normal1[BP_AXIS_Y];
    hingenormal[BP_AXIS_Z] = normal0[BP_AXIS_Z] + normal1[BP_AXIS_Z];

    for( clcount2 = clcount1-1, clvertex2 = clvertex1+1 ; clcount2 > 0 ; clcount2--, clvertex2++ )
    {
      v2 = clvertex2->vertexindex;
      vertex2 = ADDRESS( bpdata->vertex, v2 * bpdata->vertexstride );
      normal2 = ADDRESS( bpdata->normal, v2 * bpdata->normalstride );

      /* Compute an approximate surface normal from the three vertices picked */
      /* There's no need for the normal to be normalized, it's only used in a dotproduct for orientation */
      hintnormal[BP_AXIS_X] = hingenormal[BP_AXIS_X] + normal2[BP_AXIS_X];
      hintnormal[BP_AXIS_Y] = hingenormal[BP_AXIS_Y] + normal2[BP_AXIS_Y];
      hintnormal[BP_AXIS_Z] = hingenormal[BP_AXIS_Z] + normal2[BP_AXIS_Z];

#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed Try %d,%d,%d\n", v0, v1, v2 );
printf( "    Seed Try -> %.3f,%.3f,%.3f ; %.3f,%.3f,%.3f ; %.3f,%.3f,%.3f\n", vertex0[0], vertex0[1], vertex0[2], vertex1[0], vertex1[1], vertex1[2], vertex2[0], vertex2[1], vertex2[2] );
#endif

      /* Build sphere out of vertexindex,v1,v2 */
      if( !( bpComputeSphereCenter( vertex0, vertex1, vertex2, hintnormal, bpdata->ballradiusmin, bpdata->ballradiusmin, bpdata->balldistmin, result->spherecenter, result->normal, &flipflag ) ) )
        continue;

      agreementsum = 0.0;
      agreement = M3D_VectorDotProduct( result->normal, normal0 );
      if( agreement < bpdata->seednormalagreement )
      {
#ifdef DEBUG_VERBOSE_SEED
printf( "        Fail : Disagreement with vertex0 normal %f ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", agreement, result->normal[0], result->normal[1], result->normal[2], normal0[0], normal0[1], normal0[2] );
#endif
        continue;
      }
      agreementsum += agreement;
      agreement = M3D_VectorDotProduct( result->normal, normal1 );
      if( agreement < bpdata->seednormalagreement )
      {
#ifdef DEBUG_VERBOSE_SEED
printf( "        Fail : Disagreement with vertex1 normal %f ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", agreement, result->normal[0], result->normal[1], result->normal[2], normal1[0], normal1[1], normal1[2] );
#endif
        continue;
      }
      agreementsum += agreement;
      agreement = M3D_VectorDotProduct( result->normal, normal2 );
      if( agreement < bpdata->seednormalagreement )
      {
#ifdef DEBUG_VERBOSE_SEED
printf( "        Fail : Disagreement with vertex2 normal %f ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", agreement, result->normal[0], result->normal[1], result->normal[2], normal2[0], normal2[1], normal2[2] );
#endif
        continue;
      }
      agreementsum += agreement;

      if( agreementsum < bpdata->seednormalagreementsum )
      {
#ifdef DEBUG_VERBOSE_SEED
printf( "        Fail : Overall disagreement with normals %f\n", agreementsum );
#endif
        continue;
      }

#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed Sphere Center %.2f,%.2f,%.2f\n", result->spherecenter[0], result->spherecenter[1], result->spherecenter[2] );
#endif

      /* Check if any other vertex overlaps our sphere */
      seedcheck.v0 = v0;
      seedcheck.v1 = v1;
      seedcheck.v2 = v2;
      seedcheck.radsquared = radsquared;
      bpMathPlaneEquation( seedcheck.plane, vertex0, vertex1, vertex2 );
      seedcheck.coplanardist = bpdata->seedcoplanardist;

      /* Acquire a spatial lock, ensure consistant bitmap for cospherical coplanar vertices */
      bpSpaceLockAcquire( bpdata, &bpdata->spacemap, &result->spacelock, result->spherecenter, seedballradius );

      /* Check if any vertex overlaps the space */
      if( !( bpGridPickRadius( bpdata, bpdata->grid, result->spherecenter, seedballradius, bpNewTriangleSeedCheckPick, (void *)&seedcheck ) ) )
      {
        bpSpaceLockRelease( bpdata, &bpdata->spacemap, &result->spacelock );
        continue;
      }

      /* We have found a new seed triangle */
      result->spacelockflag = 1;
      result->triangle.v[0] = v0;
      if( !( flipflag ) )
      {
        result->triangle.v[1] = v1;
        result->triangle.v[2] = v2;
      }
      else
      {
        result->triangle.v[1] = v2;
        result->triangle.v[2] = v1;
      }

      /* Mark all vertices to disallow testing them as seed */
      if( bpdata->seedmarkradius > 0.0 )
        bpGridPickRadius( bpdata, bpdata->grid, result->spherecenter, bpdata->seedmarkradius, bpNewTriangleSeedMarkPick, 0 );
      return 1;
    }
  }

  return 0;
}


static int bpNewTriangleSeed( bpMainData *bpdata, bpThreadData *tdata, bpPivotResult *result )
{
  bpi vertexindex;
  size_t retentryindex;
  if( tdata->flags & BP_THREAD_DATA_FLAGS_NOSEED )
    return 0;
  for( ; ; )
  {
    if( !( mmBitMapFindClear( &bpdata->vertexmap, tdata->seedvertexindex, tdata->seedvertexbase, &retentryindex ) ) )
    {
#ifdef DEBUG_VERBOSE_SEED
printf( "    Seed List Exhausted\n" );
#endif
      tdata->flags |= BP_THREAD_DATA_FLAGS_NOSEED;
      break;
    }
    tdata->seedvertexindex = retentryindex;
    vertexindex = retentryindex;

    /* Try to find a new seed triangle around vertexindex */
    if( bpNewTriangleSeedVertex( bpdata, tdata, vertexindex, result ) )
      return 1;
  }
  return 0;
}



/********/



typedef struct
{
  /* First two vertices of new triangle */
  bpi v0, v1, vprev;
  bpf *vertex0;
  bpf *vertex1;
  bpf *normal0;
  bpf *normal1;
  bpf midpoint[3]; /* 0.5 * ( vertex1 + vertex0 ) */
  bpf vecta[3]; /* vertex1 - vertex0 */
  bpf vectamagn; /* magnitude of vecta */
  bpf basepivot[3]; /* vector from sphere to midpoint */
  bpf pivotplane[4]; /* plane equation from sphere to hinge */
  int hingeflags;

  /* Plane from hinge */
  bpf hingeplane[4];

  /* Current ball radius */
  bpf ballradius;
  bpf anglecollectrange;

  /* Result */
  bpf bestangle;
  bpf bestanglemin, bestanglemax;
  bpPivotResult *result;

} bpPivotPickData;


/* Best case, equilateral triangle : area = 0.433012702 * sidemax * sidemax */
/*
#define BP_TRIANGLE_COLLINEAR_DOWNGRADE (0.05)

static inline int bpTriangleCollinear( bpMainData *bpdata, bpf *vecta, bpf *vectb, bpf *vectc, bpf vectamagn, bpf vectbmagn, bpf vectcmagn )
{
  bpf dotpr, doublearea, sidemax;
  dotpr = M3D_VectorDotProduct( vectb, vecta ) / ( vectamagn * vectbmagn );
  doublearea = vectamagn * vectbmagn * bpfsqrt( 1.0 - dotpr*dotpr );
  sidemax = bpfmax( vectamagn, bpfmax( vectbmagn, vectcmagn ) );

  if( doublearea > (2.0*BP_TRIANGLE_COLLINEAR_DOWNGRADE) * sidemax * sidemax )
    return 0;

#ifdef DEBUG_VERBOSE_PIVOT
  printf( "          Triangle Collinear Downgrade : %f < %f\n", doublearea, (2.0*BP_TRIANGLE_COLLINEAR_DOWNGRADE) * sidemax * sidemax );
#endif

  return 1;
}
*/


#define BP_TRIANGLE_JOIN_COUNT_FULL (0x100)
#define BP_TRIANGLE_JOIN_COUNT_PARTIAL (0x10)

static inline int bpTriangleJoinCount( bpMainData *bpdata, bpi v0, bpi v1, bpi pivotvertex, int hingeflags, int joinvalue )
{
  int joincount, retval;
  bpEdge edgejoin[4];
  mmHashLock hashlock;
  mmHashLockRange lockrange[4];

  edgejoin[0].v[0] = pivotvertex;
  edgejoin[0].v[1] = v1;
  edgejoin[0].grade = BP_EDGE_GRADE_DELETED+1;
  edgejoin[1].v[0] = v1;
  edgejoin[1].v[1] = pivotvertex;
  edgejoin[2].v[0] = v0;
  edgejoin[2].v[1] = pivotvertex;
  edgejoin[2].grade = BP_EDGE_GRADE_DELETED+1;
  edgejoin[3].v[0] = pivotvertex;
  edgejoin[3].v[1] = v0;

  mmHashLockInit( &hashlock, 0 );
  mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edgejoin[0], &hashlock, &lockrange[0] );
  mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edgejoin[1], &hashlock, &lockrange[1] );
  mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edgejoin[2], &hashlock, &lockrange[2] );
  mmHashLockAdd( bpdata->hingehashtable, &bpHingeHashEdge, &edgejoin[3], &hashlock, &lockrange[3] );
  mmHashLockAcquire( bpdata->hingehashtable, &bpHingeHashEdge, &hashlock );

  retval = 0;
  joincount = 0x0;
  if( mmHashDirectFindEntry( bpdata->hingehashtable, &bpHingeHashGrade, &edgejoin[0] ) )
  {
    joincount += BP_TRIANGLE_JOIN_COUNT_PARTIAL;
    if( mmHashDirectReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgejoin[1] ) != MM_HASH_SUCCESS )
      joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
    else
    {
      if( edgejoin[1].grade <= BP_EDGE_GRADE_DELETED )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
      else if( edgejoin[1].v[2] == v0 )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
#ifdef CONFIG_ENABLE_JOINCOUNT_JOIN_OVERRIDE
      else if( bpOverridePivotCheck( bpdata, edgejoin[1].v[0], edgejoin[1].v[1], edgejoin[1].v[2], v0, bpdata->pivotoverriderange ) == BP_OVERRIDE_ACCEPTED )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
#elif defined(CONFIG_ENABLE_JOINCOUNT_FORCE_OVERRIDE)
      else if( ( hingeflags & BP_HINGE_FLAGS_FORCE ) && ( bpOverridePivotCheck( bpdata, edgejoin[1].v[0], edgejoin[1].v[1], edgejoin[1].v[2], v0, bpdata->pivotoverriderange ) == BP_OVERRIDE_ACCEPTED ) )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
#endif
    }
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Join Link %d,%d\n", (int)edgejoin[0].v[0], (int)edgejoin[0].v[1] );
#endif
  }

  if( joincount >= joinvalue )
  {
    retval = 1;
    goto end;
  }
  if( ( joincount + BP_TRIANGLE_JOIN_COUNT_FULL ) < joinvalue )
    goto end;

  if( mmHashDirectFindEntry( bpdata->hingehashtable, &bpHingeHashGrade, &edgejoin[2] ) )
  {
    joincount += BP_TRIANGLE_JOIN_COUNT_PARTIAL;
    if( mmHashDirectReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgejoin[3] ) != MM_HASH_SUCCESS )
      joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
    else
    {
      if( edgejoin[3].grade <= BP_EDGE_GRADE_DELETED )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
      else if( edgejoin[3].v[2] == v1 )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
#ifdef CONFIG_ENABLE_JOINCOUNT_JOIN_OVERRIDE
      else if( bpOverridePivotCheck( bpdata, edgejoin[3].v[0], edgejoin[3].v[1], edgejoin[3].v[2], v1, bpdata->pivotoverriderange ) == BP_OVERRIDE_ACCEPTED )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
#elif defined(CONFIG_ENABLE_JOINCOUNT_FORCE_OVERRIDE)
      else if( ( hingeflags & BP_HINGE_FLAGS_FORCE ) && ( bpOverridePivotCheck( bpdata, edgejoin[3].v[0], edgejoin[3].v[1], edgejoin[3].v[2], v1, bpdata->pivotoverriderange ) == BP_OVERRIDE_ACCEPTED ) )
        joincount += BP_TRIANGLE_JOIN_COUNT_FULL - BP_TRIANGLE_JOIN_COUNT_PARTIAL;
#endif
    }
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Join Link %d,%d\n", (int)edgejoin[2].v[0], (int)edgejoin[2].v[1] );
#endif
  }

  if( joincount >= joinvalue )
    retval = 1;

  end:
  mmHashLockRelease( bpdata->hingehashtable, &hashlock );
  return retval;
}



/********/



static int bpPivotPick( bpMainData *bpdata, void *opaquepointer, bpi vertexindex, bpf distsquared )
{
  int statusflags;
  bpf angle;
  bpf *vertex2;
  bpf vectb[3];
  bpf normal[3];
  bpf normalmagn, normalmagninv, baseradius;
  bpf spherecenter[3];
  bpf pivot[3];
  bpf pivotmagn;
  bpPivotPickData *pivotdata;
  bpPivotResult *result;

  pivotdata = opaquepointer;

  /* Discard vertices that belong to v0,v1 or past hinge */
  if( ( vertexindex == pivotdata->v0 ) || ( vertexindex == pivotdata->v1 ) || ( vertexindex == pivotdata->vprev ) )
    return 1;

  vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
  M3D_VectorSubStore( vectb, vertex2, pivotdata->vertex0 );
  M3D_VectorCrossProduct( normal, vectb, pivotdata->vecta );
  normalmagn = M3D_VectorMagnitude( normal );

#ifdef DEBUG_VERBOSE_PIVOT
printf( "      Pivot Pick : %d, distance %f\n", vertexindex, bpfsqrt( distsquared ) );
printf( "      Pivot Try -> %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f\n", pivotdata->vertex0[0], pivotdata->vertex0[1], pivotdata->vertex0[2], pivotdata->vertex1[0], pivotdata->vertex1[1], pivotdata->vertex1[2], vertex2[0], vertex2[1], vertex2[2] );
printf( "      Pivot Collinear Check : %.3f ( %.3f / ( %.3f * %.3f ) )\n", normalmagn / ( pivotdata->vectamagn * M3D_VectorMagnitude(vectb) ), normalmagn, pivotdata->vectamagn, M3D_VectorMagnitude(vectb) );
#endif

  /* If the 3 points are practically collinear, fail */
  if( normalmagn <= CONF_COLLINEAR_MAGNITUDE_MIN * pivotdata->vectamagn * M3D_VectorMagnitude(vectb) )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Fail : Collinear ( %f, %f, %f )\n", normalmagn, pivotdata->vectamagn, M3D_VectorMagnitude(vectb) );
#endif
    return 1;
  }

  /* Normalize normal vector */
  normalmagninv = 1.0 / normalmagn;
  normal[0] *= normalmagninv;
  normal[1] *= normalmagninv;
  normal[2] *= normalmagninv;

  /* If angle is too step from previous triangle, discard */
  if( M3D_VectorDotProduct( normal, pivotdata->hingeplane ) < bpdata->hingenormalagreement )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Fail : Angle too different from hinge normal %f < %f ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", M3D_VectorDotProduct( normal, pivotdata->hingeplane ), bpdata->hingenormalagreement, normal[0], normal[1], normal[2], pivotdata->hingeplane[0], pivotdata->hingeplane[1], pivotdata->hingeplane[2] );
#endif
    return 1;
  }

  /* Try to build a sphere from the three vertices */
  if( !( bpComputePivotSphere( pivotdata->vertex0, pivotdata->vecta, vectb, normal, bpdata->ballradiuspivot, pivotdata->ballradius, bpdata->balldistmin, spherecenter, &baseradius ) ) )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Fail : Build Sphere Failed\n" );
#endif
    return 1;
  }

  /* Vector from new sphere center to midpoint */
  M3D_VectorSubStore( pivot, spherecenter, pivotdata->midpoint );
  pivotmagn = M3D_VectorMagnitude( pivot );
  /* Compute angle between basepivot and new pivot vector */
  angle = M3D_VectorDotProduct( pivotdata->basepivot, pivot ) / pivotmagn;

#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Normal %f %f %f\n", normal[0], normal[1], normal[2] );
printf( "        Basepivot %f %f %f ; Pivot %f %f %f\n", pivotdata->basepivot[0], pivotdata->basepivot[1], pivotdata->basepivot[2], pivot[0] / pivotmagn, pivot[1] / pivotmagn, pivot[2] / pivotmagn );
printf( "        Baseradius %f ; Dotpr %f ; Planedist %f ; Hingedist %f ; Pivotdist %f\n", baseradius, angle, M3D_PlanePoint( pivotdata->pivotplane, spherecenter ), M3D_PlanePoint( pivotdata->hingeplane, vertex2 ), M3D_PlanePoint( pivotdata->pivotplane, vertex2 ) );
#endif

  /* If spherecenter is under plane, invert angle direction */
  if( ( M3D_PlanePoint( pivotdata->pivotplane, spherecenter ) < 0.0 ) && ( angle < 0.99 ) && ( ( M3D_PlanePoint( pivotdata->hingeplane, vertex2 ) < 0.0 ) || ( M3D_PlanePoint( pivotdata->pivotplane, vertex2 ) < 0.0 ) ) )
    angle = -2.0 - angle;

#ifdef DEBUG_VERBOSE_PIVOT
printf( "        SphereCenter %f,%f,%f ; Angle %f\n", spherecenter[0], spherecenter[1], spherecenter[2], angle );
#endif

  result = pivotdata->result;

  if( angle < pivotdata->bestanglemin )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Fail : Less Than Best ( %f < %f : %f )\n", angle, pivotdata->bestanglemin, pivotdata->bestangle );
#endif
    return 1;
  }


  statusflags = bpPivotComputeStatusFlags( bpdata, pivotdata->v0, pivotdata->v1, vertexindex, normal );
  if( statusflags & BP_PIVOT_STATUS_FLAGS_INVALID )
    return 1;


  /* Truncate minimum sphere radius to ballradiuspivot */
  if( baseradius < bpdata->ballradiuspivot )
    baseradius = bpdata->ballradiuspivot;

  if( pivotdata->hingeflags & BP_HINGE_FLAGS_JOIN )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Okay : New Join Hit ( %f )\n", angle );
#endif
    result->triangle.v[2] = vertexindex;
    bpHitBufferAdd( &result->hitbuffer, vertexindex, angle, baseradius, spherecenter, normal, statusflags );
    return 1;
  }

  if( angle < pivotdata->bestanglemax )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Okay : Hit Cospherical ( %f )\n", angle );
#endif
    bpHitBufferAdd( &result->hitbuffer, vertexindex, angle, baseradius, spherecenter, normal, statusflags );
    if( angle > pivotdata->bestangle )
    {
      result->triangle.v[2] = vertexindex;
      pivotdata->bestangle = angle;
      pivotdata->bestanglemin = angle - pivotdata->anglecollectrange;
      pivotdata->bestanglemax = angle + pivotdata->anglecollectrange;
    }
  }
  else
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "        Okay : Best So Far ( %f > %f )\n", angle, pivotdata->bestangle );
#endif
    result->triangle.v[2] = vertexindex;
    pivotdata->bestangle = angle;
    pivotdata->bestanglemin = angle - pivotdata->anglecollectrange;
    pivotdata->bestanglemax = angle + pivotdata->anglecollectrange;
    bpHitBufferSet( &result->hitbuffer, vertexindex, angle, baseradius, spherecenter, normal, statusflags );
  }

  return 1;
}





enum
{
  BP_PIVOT_DISCARD,
  BP_PIVOT_FAILURE,
  BP_PIVOT_JOINMESH,
  BP_PIVOT_SPHEREMESH
};


#if 0
static int bpPivotFilterVersion1( bpMainData *bpdata, bpThreadData *tdata, bpPivotResult *result )
{
  int hitcount, joincount, deleteflag;
  bpi vertexindex;
  bpf planedist, planedistmin, normalmagn, normalmagninv, vectbmagn;
  bpf *vertex0, *vertex1, *vertex2;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], planenormal[BP_AXIS_COUNT];
  bpf plane[4], sideplane[4], vectamagn;
  bpHit *hit, *hitfind, *hitfix, *hitend;
  bpEdge edgecheck;

  if( result->hitbuffer.hitcount < 2 )
    return BP_PIVOT_JOINMESH;

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "      Hit Results Filtering, Count %d, Sphere %f,%f,%f\n", (int)result->hitbuffer.hitcount, result->spherecenter[0], result->spherecenter[1], result->spherecenter[2] );
#endif

  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );

  plane[0] = 0.0;
  plane[1] = 0.0;
  plane[2] = 0.0;
  plane[3] = 1.0;
  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  vectamagn = M3D_VectorMagnitude( vecta );

#ifdef CONFIG_SPHERE_JOIN_PRIORITY
  /* Scan all cospherical hits to find pending join edges */
  joincount = 0;
  hit = result->hitbuffer.hitlist;
  for( hitcount = result->hitbuffer.hitcount ; hitcount ; hitcount--, hit++ )
  {
    vertexindex = hit->vertexindex;
    if( !( bpTriangleJoinCount( bpdata, result->triangle.v[0], result->triangle.v[1], vertexindex, 0x0, BP_TRIANGLE_JOIN_COUNT_FULL ) ) )
      continue;

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Cospherical Vertexindex %d Linked\n", (int)vertexindex );
#endif

    hit->pivotangle = 1.0;
    joincount++;
  }
  /* If we found edges to join, discard all other non-join results */
  if( joincount )
  {
    hit = result->hitbuffer.hitlist;
    for( hitcount = result->hitbuffer.hitcount ; hitcount ; hitcount--, hit++ )
    {
      if( hit->pivotangle < 0.5 )
        hit->vertexindex = -1;
    }
    bpHitBufferPack( &result->hitbuffer );
    if( result->hitbuffer.hitcount == 1 )
      return BP_PIVOT_JOINMESH;
  }
#endif

  /* Find top plane */
  hit = result->hitbuffer.hitlist;
  for( hitcount = result->hitbuffer.hitcount ; hitcount ; hitcount--, hit++ )
  {
    vertexindex = hit->vertexindex;
    vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
    planedist = M3D_PlanePoint( plane, vertex2 );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Vertexindex %d, Planedist %f\n", (int)vertexindex, planedist );
#endif

    /* Sort by normalized dot product with hinge vector */
    M3D_VectorSubStore( vectb, vertex2, vertex0 );
    vectbmagn = M3D_VectorMagnitude( vectb );
    hit->pivotangle = M3D_VectorDotProduct( vecta, vectb ) / ( vectamagn * vectbmagn );

    if( planedist <= 0.0 )
      continue;
    M3D_VectorCrossProduct( planenormal, vectb, vecta );
    normalmagn = M3D_VectorMagnitude( planenormal );
    if( normalmagn > CONF_COLLINEAR_MAGNITUDE_MIN * vectamagn * vectbmagn )
    {
      normalmagninv = 1.0 / normalmagn;
      M3D_VectorMulScalarStore( plane, planenormal, normalmagninv );
      plane[3] = -( plane[0]*vertex0[0] + plane[1]*vertex0[1] + plane[2]*vertex0[2] );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Top Plane Set To Vertexindex %d\n", (int)vertexindex );
#endif

    }
  }

  /* Side plane */
  M3D_VectorSubStore( vectb, result->spherecenter, vertex0 );
  M3D_VectorCrossProduct( planenormal, vectb, vecta );
  M3D_VectorCrossProduct( sideplane, vecta, planenormal );
  sideplane[3] = -( sideplane[0]*vertex0[0] + sideplane[1]*vertex0[1] + sideplane[2]*vertex0[2] );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Top Plane %f %f %f %f ; Side Plane %f %f %f %f\n", plane[0], plane[1], plane[2], plane[3], sideplane[0], sideplane[1], sideplane[2], sideplane[3] );
#endif


#ifdef CONFIG_SPHERE_PLANAR_TEST
  /* Delete all entries outside of plane range */
  deleteflag = 0;
  planedistmin = -bpdata->pivotcoplanardist;
  hit = result->hitbuffer.hitlist;
  for( hitcount = result->hitbuffer.hitcount ; hitcount ; hitcount--, hit++ )
  {
    vertexindex = hit->vertexindex;
    vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
    planedist = M3D_PlanePoint( plane, vertex2 );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Vertexindex %d, Plane dist %f ( %f )\n", (int)vertexindex, planedist, planedistmin );
#endif

    if( planedist < planedistmin )
    {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Far From Plane, Discard Vertexindex %d\n", (int)vertexindex );
#endif
      hit->vertexindex = -1;
      deleteflag = 1;
      continue;
    }

    if( M3D_PlanePoint( sideplane, vertex2 ) < 0.0 )
    {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Behind Side Plane, Discard Vertexindex %d\n", (int)vertexindex );
#endif
      hit->vertexindex = -1;
      deleteflag = 1;
      continue;
    }
  }

  /* Sort and pack the hits */
  if( deleteflag )
    bpHitBufferPack( &result->hitbuffer );
#endif
  bpHitBufferAdd( &result->hitbuffer, result->triangle.v[0], 2.0, 0.0, 0, 0, 0 );
  bpHitBufferAdd( &result->hitbuffer, result->triangle.v[1], 1.5, 0.0, 0, 0, 0 );
  bpHitBufferSort( &result->hitbuffer );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
  hitend = &result->hitbuffer.hitlist[result->hitbuffer.hitcount];
  for( hit = result->hitbuffer.hitlist ; hit < hitend ; hit++ )
    printf( "*** LIST %d : %f\n", (int)hit->vertexindex, hit->pivotangle );
#endif

  /* Find all "end" edges looking away from hinge midpoint, discard vertices behind them */
  edgecheck.grade = BP_EDGE_GRADE_HIGH;
  deleteflag = 0;
  hitend = &result->hitbuffer.hitlist[result->hitbuffer.hitcount];
  for( hit = result->hitbuffer.hitlist ; hit < hitend ; hit++ )
  {
    vertexindex = hit->vertexindex;
    if( vertexindex == -1 )
      continue;
    for( hitfind = hit+2 ; hitfind < hitend ; hitfind++ )
    {
      if( hitfind->vertexindex == -1 )
        continue;
      edgecheck.v[0] = hitfind->vertexindex;
      edgecheck.v[1] = vertexindex;
      if( !( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashGrade, &edgecheck ) ) )
        continue;
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Active End Edge Found : %d,%d\n", edgecheck.v[0], edgecheck.v[1] );
#endif
      if( vertexindex == result->triangle.v[0] )
      {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "            Drop, Primary Hinge Behind End Edge : %d,%d\n", vertexindex, hitfind->vertexindex );
#endif
        return BP_PIVOT_FAILURE;
      }
      for( hitfix = hit+1 ; hitfix < hitfind ; hitfix++ )
      {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
if( hitfix->vertexindex != -1 )
printf( "            Behind End Edge, Discard Vertexindex %d\n", (int)hitfix->vertexindex );
#endif
        hitfix->vertexindex = -1;
        deleteflag = 1;
      }
    }
  }

  /* Find all "start" edges looking towards hinge midpoint, drop tesselation if any is found */
  for( hit = result->hitbuffer.hitlist ; hit < hitend ; hit++ )
  {
    vertexindex = hit->vertexindex;
    if( vertexindex == -1 )
      continue;
    for( hitfind = hit+1 ; hitfind < hitend ; hitfind++ )
    {
      if( hitfind->vertexindex == -1 )
        continue;
      edgecheck.v[0] = vertexindex;
      edgecheck.v[1] = hitfind->vertexindex;
      if( !( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashGrade, &edgecheck ) ) )
        continue;
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Active Start Edge Found : %d,%d\n", edgecheck.v[0], edgecheck.v[1] );
#endif
      if( vertexindex != result->triangle.v[0] )
      {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "            Drop, Primary Hinge Behind Start Edge : %d,%d\n", edgecheck.v[0], edgecheck.v[1] );
#endif
        return BP_PIVOT_FAILURE;
      }
      for( hitfix = hitfind+1 ; hitfix < hitend ; hitfix++ )
      {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
if( hitfix->vertexindex != -1 )
printf( "            Behind End Edge, Discard Vertexindex %d\n", (int)hitfix->vertexindex );
#endif
        hitfix->vertexindex = -1;
        deleteflag = 1;
      }
      for( hitfix = result->hitbuffer.hitlist ; hitfix < hit ; hitfix++ )
      {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
if( hitfix->vertexindex != -1 )
printf( "            Behind End Edge, Discard Vertexindex %d\n", (int)hitfix->vertexindex );
#endif
        hitfix->vertexindex = -1;
        deleteflag = 1;
      }
    }
  }

  if( deleteflag )
    bpHitBufferPack( &result->hitbuffer );

  return ( result->hitbuffer.hitcount < 2 ? BP_PIVOT_JOINMESH : BP_PIVOT_SPHEREMESH );
}



typedef struct
{
  bpf plane[4];
  bpf distmax;
  bpHitBuffer hitbuffer;
} bpPlaneFilter;

static int bpPlaneFilterPick( bpMainData *bpdata, void *opaquepointer, bpi vertexindex, bpf distsquared )
{
  bpf *vertex;
  bpf dist;
  bpPlaneFilter *planefilter;

  planefilter = opaquepointer;

  vertex = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
  dist = bpfabs( M3D_PlanePoint( planefilter->plane, vertex ) );

#ifdef DEBUG_VERBOSE_FILTER
printf( "  PlaneFilter %d : %f < %f\n", (int)vertexindex, dist, planefilter->distmax );
#endif

  if( dist > planefilter->distmax )
    return 1;

#ifdef DEBUG_VERBOSE_FILTER
printf( "    Added Vertexindex %d\n", (int)vertexindex );
#endif

  bpHitBufferAdd( &planefilter->hitbuffer, vertexindex, 0.0, 0.0, 0, 0, 0 );

  return 1;
}


static int bpPivotFilterVersion2( bpMainData *bpdata, bpThreadData *tdata, bpPivotResult *result, bpf ballradius )
{
  int hitcount, cutflag;
  bpi vertexindex;
  bpf *vertex0, *vertex1, *vertex2;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], midpoint[BP_AXIS_COUNT];
  bpHit *hit, *hitfind, *hitend;
  bpPlaneFilter planefilter;
  bpf checkcenter[BP_AXIS_COUNT], pivotcenter[BP_AXIS_COUNT];
  bpf checkradius, pivotradius;
  bpEdge edgecheck[2];

  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );
  M3D_VectorSubStore( vecta, vertex1, vertex0 );

  midpoint[BP_AXIS_X] = 0.5 * ( vertex0[BP_AXIS_X] + vertex1[BP_AXIS_X] );
  midpoint[BP_AXIS_Y] = 0.5 * ( vertex0[BP_AXIS_Y] + vertex1[BP_AXIS_Y] );
  midpoint[BP_AXIS_Z] = 0.5 * ( vertex0[BP_AXIS_Z] + vertex1[BP_AXIS_Z] );

  /* Find biggest triangle? Kind of redundant calculations... store biggest core radius? */
  hit = result->hitbuffer.hitlist;
  for( hitcount = result->hitbuffer.hitcount ; hitcount ; hitcount--, hit++ )
  {
    vertexindex = hit->vertexindex;
    vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
    M3D_VectorSubStore( vectb, vertex2, vertex0 );
    pivotradius = bpComputeMinimumSphere( vertex0, vecta, vectb, pivotcenter );
    if( pivotradius > checkradius )
    {
      checkradius = pivotradius;
      M3D_VectorCopy( checkcenter, pivotcenter );
    }
  }
/*
  checkradius = bpfmax( checkradius, ballradius );
*/

  /* FOO */
  M3D_VectorCrossProduct( planefilter.plane, vectb, vecta );
  M3D_VectorNormalize( planefilter.plane );
  planefilter.plane[3] = -( planefilter.plane[0]*vertex0[0] + planefilter.plane[1]*vertex0[1] + planefilter.plane[2]*vertex0[2] );
  planefilter.distmax = 0.1 * bpdata->ballradiusmin;

  bpHitBufferInit( &planefilter.hitbuffer );
  bpGridPickRadius( bpdata, bpdata->grid, checkcenter, checkradius, bpPlaneFilterPick, &planefilter );

#ifdef DEBUG_VERBOSE_FILTER
printf( "    Filter Hinge %d,%d ; Center %f,%f,%f ; Radius %f / %f\n", (int)result->triangle.v[0], (int)result->triangle.v[1], checkcenter[0], checkcenter[1], checkcenter[2], checkradius, ballradius );
printf( "      %d Hits ; %d Filters\n", result->hitbuffer.hitcount, planefilter.hitbuffer.hitcount );
#endif

  cutflag = 0;
  hit = planefilter.hitbuffer.hitlist;
  hitend = &hit[ planefilter.hitbuffer.hitcount ];
  for( ; hit < hitend ; hit++ )
  {
    vertexindex = hit->vertexindex;
    for( hitfind = hit+1 ; hitfind < hitend ; hitfind++ )
    {
      if( ( vertexindex == result->triangle.v[0] ) && ( hitfind->vertexindex == result->triangle.v[1] ) )
        continue;
      if( ( vertexindex == result->triangle.v[1] ) && ( hitfind->vertexindex == result->triangle.v[0] ) )
        continue;
      edgecheck[0].v[0] = hitfind->vertexindex;
      edgecheck[0].v[1] = vertexindex;
      edgecheck[1].v[0] = vertexindex;
      edgecheck[1].v[1] = hitfind->vertexindex;
      if( ( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[0] ) ) || ( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[1] ) ) )
        cutflag |= bpPivotFilterPlaneCut( bpdata, tdata, result, midpoint, edgecheck[0].v );
    }
  }

  if( cutflag )
    bpHitBufferPack( &result->hitbuffer );

  bpHitBufferFree( &planefilter.hitbuffer );

  return ( result->hitbuffer.hitcount < 2 ? BP_PIVOT_JOINMESH : BP_PIVOT_SPHEREMESH );
}


static int bpPivotFilterVersion3( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpPivotResult *result, bpf ballradius )
{
  int index, cutflag;
  bpi vertexindex;
  bpf *vertex0, *vertex1, *vertex2;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], midpoint[BP_AXIS_COUNT], triplane[4];
  bpHit *hit, *hitfind, *hitend;
  bpEdge edgecheck[2];

#ifdef DEBUG_VERBOSE_FILTER
printf( "      Filter Hinge %d,%d Start ; %d Cospherical Hits\n", (int)result->triangle.v[0], (int)result->triangle.v[1], result->hitbuffer.hitcount );
hit = result->hitbuffer.hitlist;
hitend = &hit[ result->hitbuffer.hitcount ];
for( ; hit < hitend ; hit++ )
printf( "        Hit %d : Angle %f\n", (int)hit->vertexindex, hit->pivotangle );
#endif

  if( result->hitbuffer.hitcount < 2 )
    return BP_PIVOT_JOINMESH;

  result->spacelockflag = 1;
  bpSpaceLockAcquire( bpdata, &bpdata->spacemap, &result->spacelock, result->spherecenter, ballradius );

  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, result->triangle.v[2] * bpdata->vertexstride );
  M3D_VectorSubStore( vecta, vertex1, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex0 );

  midpoint[BP_AXIS_X] = 0.5 * ( vertex0[BP_AXIS_X] + vertex1[BP_AXIS_X] );
  midpoint[BP_AXIS_Y] = 0.5 * ( vertex0[BP_AXIS_Y] + vertex1[BP_AXIS_Y] );
  midpoint[BP_AXIS_Z] = 0.5 * ( vertex0[BP_AXIS_Z] + vertex1[BP_AXIS_Z] );

  M3D_VectorCrossProduct( triplane, vectb, vecta );
  M3D_VectorNormalize( triplane );
  triplane[3] = -( triplane[0]*vertex0[0] + triplane[1]*vertex0[1] + triplane[2]*vertex0[2] );

  cutflag = 0;
  hit = result->hitbuffer.hitlist;
  hitend = &hit[ result->hitbuffer.hitcount ];
  for( ; hit < hitend ; hit++ )
  {
    vertexindex = hit->vertexindex;
    for( hitfind = hit+1 ; hitfind < hitend ; hitfind++ )
    {
      edgecheck[0].v[0] = hitfind->vertexindex;
      edgecheck[0].v[1] = vertexindex;
      if( mmHashLockReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[0] ) == MM_HASH_SUCCESS )
      {
        cutflag |= bpPivotFilterPlaneCut( bpdata, tdata, result, midpoint, edgecheck[0].v );
        continue;
      }
      edgecheck[1].v[0] = vertexindex;
      edgecheck[1].v[1] = hitfind->vertexindex;
      if( mmHashLockReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[1] ) == MM_HASH_SUCCESS )
      {
        cutflag |= bpPivotFilterPlaneCut( bpdata, tdata, result, midpoint, edgecheck[1].v );
        continue;
      }
    }
  }

  for( index = 0 ; index < BP_AXIS_COUNT ; index++ )
  {
    vertexindex = result->triangle.v[index];
    for( hitfind = result->hitbuffer.hitlist ; hitfind < hitend ; hitfind++ )
    {
      edgecheck[0].v[0] = hitfind->vertexindex;
      edgecheck[0].v[1] = vertexindex;
      if( mmHashLockReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[0] ) == MM_HASH_SUCCESS )
      {
        cutflag |= bpPivotFilterPlaneCut( bpdata, tdata, result, midpoint, edgecheck[0].v );
        continue;
      }
      edgecheck[1].v[0] = vertexindex;
      edgecheck[1].v[1] = hitfind->vertexindex;
      if( mmHashLockReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[1] ) == MM_HASH_SUCCESS )
      {
        cutflag |= bpPivotFilterPlaneCut( bpdata, tdata, result, midpoint, edgecheck[1].v );
        continue;
      }
    }
  }

  if( cutflag )
  {
    for( hitfind = result->hitbuffer.hitlist ; hitfind < hitend ; hitfind++ )
    {
      if( hitfind->pivotangle < -2.0 )
        hitfind->vertexindex = -1;
    }
    bpHitBufferPack( &result->hitbuffer );
  }
  bpHitBufferSort( &result->hitbuffer );

#ifdef DEBUG_VERBOSE_FILTER
printf( "      Filter Hinge %d,%d End ; %d Cospherical Hits\n", (int)result->triangle.v[0], (int)result->triangle.v[1], result->hitbuffer.hitcount );
hit = result->hitbuffer.hitlist;
hitend = &hit[ result->hitbuffer.hitcount ];
for( ; hit < hitend ; hit++ )
printf( "        Hit %d : Angle %f\n", (int)hit->vertexindex, hit->pivotangle );
#endif

  if( result->hitbuffer.hitcount )
  {
    if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
      result->hitbuffer.hitcount = 1;
    return BP_PIVOT_JOINMESH;
  }
  return BP_PIVOT_FAILURE;
}
#endif




#if 0  // 2012-10-22 ch3: this function not currently used, so commented
static int bpPivotFilterPlaneCut( bpMainData *bpdata, bpThreadData *tdata, bpPivotResult *result, bpf *midpoint, bpi *cutsegment )
{
  int cutflag;
  bpi vertexindex;
  bpf *vertex0, *vertex1, *vertex2, *vcut0, *vcut1;
  bpf vectside[BP_AXIS_COUNT], vectcut[BP_AXIS_COUNT];
  bpf cutplane[4], sideplane[4], planedist;
  bpHit *hit, *hitend;

#ifdef DEBUG_VERBOSE_FILTER
printf( "        Edge Between %d,%d ; %d,%d\n", (int)cutsegment[0], (int)cutsegment[1], (int)cutsegment[1], (int)cutsegment[0] );
#endif

  vcut0 = ADDRESS( bpdata->vertex, cutsegment[0] * bpdata->vertexstride );
  vcut1 = ADDRESS( bpdata->vertex, cutsegment[1] * bpdata->vertexstride );
  M3D_VectorSubStore( vectcut, vcut1, vcut0 );

  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );

  /* Verify all our buffered pivots */
  cutflag = 0;
  hit = result->hitbuffer.hitlist;
  hitend = &hit[ result->hitbuffer.hitcount ];
  for( ; hit < hitend ; hit++ )
  {
    if( hit->pivotangle < -2.0 )
      continue;
    vertexindex = hit->vertexindex;
    if( ( vertexindex == cutsegment[0] ) || ( vertexindex == cutsegment[1] ) )
      continue;

    vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );

    /* Compute cutplane, skip if hit and midpoint are on the same side of cutplane */
    M3D_VectorCrossProduct( cutplane, hit->normal, vectcut );
    cutplane[3] = -( cutplane[0]*vcut0[0] + cutplane[1]*vcut0[1] + cutplane[2]*vcut0[2] );
    planedist = M3D_PlanePoint( cutplane, vertex2 );
    if( planedist * M3D_PlanePoint( cutplane, midpoint ) > 0.0 )
      continue;

#ifdef DEBUG_VERBOSE_FILTER
printf( "          Test Vertexindex %d : %f,%f,%f : dist %f %f\n", vertexindex, vertex2[0], vertex2[1], vertex2[2], M3D_PlanePoint( cutplane, vertex2 ), M3D_PlanePoint( cutplane, midpoint ) );
#endif

#ifdef CONFIG_ENABLE_FILTER_CUT_ONLY_MIDPLANE
    /* Do not cut if cutedge segment do not cross the triangle midpoint plane */
    M3D_VectorSubStore( vectside, vertex2, midpoint );
    M3D_VectorCrossProduct( sideplane, hit->normal, vectside );
    sideplane[3] = -( sideplane[0]*midpoint[0] + sideplane[1]*midpoint[1] + sideplane[2]*midpoint[2] );
    if( M3D_PlanePoint( sideplane, vcut0 ) * M3D_PlanePoint( sideplane, vcut1 ) > 0.0 )
      continue;
#else
    /* Do not cut if cutedge segment do not cross any of triangle side plane */
    M3D_VectorSubStore( vectside, vertex2, vertex0 );
    M3D_VectorCrossProduct( sideplane, hit->normal, vectside );
    sideplane[3] = -( sideplane[0]*midpoint[0] + sideplane[1]*midpoint[1] + sideplane[2]*midpoint[2] );
    if( M3D_PlanePoint( sideplane, vcut0 ) * M3D_PlanePoint( sideplane, vcut1 ) > 0.0 )
    {
      M3D_VectorSubStore( vectside, vertex2, vertex1 );
      M3D_VectorCrossProduct( sideplane, hit->normal, vectside );
      sideplane[3] = -( sideplane[0]*midpoint[0] + sideplane[1]*midpoint[1] + sideplane[2]*midpoint[2] );
      if( M3D_PlanePoint( sideplane, vcut0 ) * M3D_PlanePoint( sideplane, vcut1 ) > 0.0 )
        continue;
    }
#endif

    /* Magnitude test is cheap yet done last because rarely true */
    if( bpfabs( planedist ) * M3D_VectorMagnitude( cutplane ) < bpdata->filterplanecutdistance )
      continue;

#ifdef DEBUG_VERBOSE_FILTER
printf( "          Cut Away Vertexindex %d, Angle %f\n", (int)vertexindex, hit->pivotangle );
#endif

    hit->pivotangle = -4.0;
    cutflag = 1;
  }

  return cutflag;
}
#endif



#if 0  // 2012-10-22 ch3: this function not currently used, so commented
static int bpPivotFilterCutTriEdge( bpMainData *bpdata, bpPivotResult *result, bpi pivotindex, bpf *vcut0, bpf *vcut1 )
{
  bpf *vertex0, *vertex1, *vertex2;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT];
  bpf triplane[4], sideplane0[4], sideplane1[4];

  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, pivotindex * bpdata->vertexstride );

  /* Do not cut if the cut segment does not intersect our speculative triangle's splane */
  M3D_VectorSubStore( vecta, vertex2, vertex0 );
  M3D_VectorSubStore( vectb, vertex2, vertex1 );
  M3D_VectorCrossProduct( triplane, vectb, vecta );
  triplane[3] = -( triplane[0]*vertex2[0] + triplane[1]*vertex2[1] + triplane[2]*vertex2[2] );
  if( ( M3D_PlanePoint( triplane, vcut0 ) * M3D_PlanePoint( triplane, vcut1 ) ) > 0.0 )
    return 0;

  /* Cut if any end of the cut segment is inside our speculative triangle's edges */
  M3D_VectorCrossProduct( sideplane0, triplane, vecta );
  sideplane0[3] = -( sideplane0[0]*vertex2[0] + sideplane0[1]*vertex2[1] + sideplane0[2]*vertex2[2] );
  M3D_VectorCrossProduct( sideplane1, triplane, vectb );
  sideplane1[3] = -( sideplane1[0]*vertex2[0] + sideplane1[1]*vertex2[1] + sideplane1[2]*vertex2[2] );
  if( ( M3D_PlanePoint( sideplane0, vcut0 ) > 0.0 ) && ( M3D_PlanePoint( sideplane1, vcut0 ) < 0.0 ) )
    return 1;
  if( ( M3D_PlanePoint( sideplane0, vcut1 ) > 0.0 ) && ( M3D_PlanePoint( sideplane1, vcut1 ) < 0.0 ) )
    return 1;

  return 0;
}
#endif




typedef struct
{
  bpPivotResult *result;
  int hitcount;
  int hitvalid;
} bpPivotFilter;


#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
void bpPivotFilterTriangleShared( bpMainData *bpdata, bpPivotFilter *filter, bpi hingeindex, bpi sharedindex, bpf *vcut0, bpf *vcut1, bpf *cutplane )
#else
void bpPivotFilterTriangleShared( bpMainData *bpdata, bpPivotFilter *filter, bpi hingeindex, bpi sharedindex, bpf *vcut0, bpf *vcut1 )
#endif
{
  bpi vertexindex;
  bpf *vhinge, *vshared, *vpivot;
  bpHit *hit, *hitend;
  bpPivotResult *result;

  result = filter->result;
  vhinge = ADDRESS( bpdata->vertex, hingeindex * bpdata->vertexstride );
  vshared = ADDRESS( bpdata->vertex, sharedindex * bpdata->vertexstride );
/*
  if( ( vhinge == vcut0 ) || ( vhinge == vcut1 ) )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "            Inverse Hinge, Skip\n" );
#endif
    return;
  }
*/

  if( vhinge == vcut0 )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "            Inverse Hinge, Skip\n" );
#endif
    return;
  }


  hit = result->hitbuffer.hitlist;
  hitend = &hit[ filter->hitcount ];
  for( ; hit < hitend ; hit++ )
  {
    vertexindex = hit->vertexindex;
    if( vertexindex == -1 )
      continue;
    vpivot = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );

    /* Dual shared, intersection is about impossible */
    if( ( vpivot == vcut0 ) || ( vpivot == vcut1 ) )
      continue;
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
    if( !( rbTriTriIntersectionSharedCheckP( vshared, vcut0, vcut1, cutplane, vhinge, vpivot, bpdata->filterplanecutdistance ) ) )
      continue;
#else
    if( !( rbTriTriIntersectionSharedCheck( vshared, vcut0, vcut1, vhinge, vpivot, bpdata->filterplanecutdistance ) ) )
      continue;
#endif

#ifdef DEBUG_VERBOSE_FILTER
printf( "            Test Vertexindex %d,%d,%d\n", (int)sharedindex, (int)hingeindex, (int)vertexindex );
printf( "              Cut Away Vertexindex %d\n", (int)vertexindex );
#endif

    hit->vertexindex = -1;
    filter->hitvalid--;
  }

  return;
}

int bpPivotFilterTrianglePick( bpMainData *bpdata, void *opaquepointer, bpTriangle *triangle )
{
  bpi vertexindex;
  bpf *vertex0, *vertex1, *vertex2;
  bpf *vcut0, *vcut1, *vcut2;
  bpHit *hit, *hitend;
  bpPivotFilter *filter;
  bpPivotResult *result;

  filter = opaquepointer;
  result = filter->result;

  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );
  vcut0 = ADDRESS( bpdata->vertex, triangle->v[0] * bpdata->vertexstride );
  vcut1 = ADDRESS( bpdata->vertex, triangle->v[1] * bpdata->vertexstride );
  vcut2 = ADDRESS( bpdata->vertex, triangle->v[2] * bpdata->vertexstride );

  if( triangle->v[0] == result->triangle.v[0] )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "          Filter Triangle Shared %d,%d,%d\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[1], result->triangle.v[0], vcut2, vcut1, triangle->plane );
#else
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[1], result->triangle.v[0], vcut2, vcut1 );
#endif
  }
  else if( triangle->v[0] == result->triangle.v[1] )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "          Filter Triangle Shared %d,%d,%d\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[0], result->triangle.v[1], vcut1, vcut2, triangle->plane );
#else
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[0], result->triangle.v[1], vcut1, vcut2 );
#endif
  }
  else if( triangle->v[1] == result->triangle.v[0] )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "          Filter Triangle Shared %d,%d,%d\n", (int)triangle->v[1], (int)triangle->v[2], (int)triangle->v[0] );
#endif
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[1], result->triangle.v[0], vcut0, vcut2, triangle->plane );
#else
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[1], result->triangle.v[0], vcut0, vcut2 );
#endif
  }
  else if( triangle->v[1] == result->triangle.v[1] )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "          Filter Triangle Shared %d,%d,%d\n", (int)triangle->v[1], (int)triangle->v[2], (int)triangle->v[0] );
#endif
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[0], result->triangle.v[1], vcut2, vcut0, triangle->plane );
#else
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[0], result->triangle.v[1], vcut2, vcut0 );
#endif
  }
  else if( triangle->v[2] == result->triangle.v[0] )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "          Filter Triangle Shared %d,%d,%d\n", (int)triangle->v[2], (int)triangle->v[0], (int)triangle->v[1] );
#endif
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[1], result->triangle.v[0], vcut1, vcut0, triangle->plane );
#else
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[1], result->triangle.v[0], vcut1, vcut0 );
#endif
  }
  else if( triangle->v[2] == result->triangle.v[1] )
  {
#ifdef DEBUG_VERBOSE_FILTER
printf( "          Filter Triangle Shared %d,%d,%d\n", (int)triangle->v[2], (int)triangle->v[0], (int)triangle->v[1] );
#endif
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[0], result->triangle.v[1], vcut0, vcut1, triangle->plane );
#else
    bpPivotFilterTriangleShared( bpdata, filter, result->triangle.v[0], result->triangle.v[1], vcut0, vcut1 );
#endif
  }
  else
  {
    hit = result->hitbuffer.hitlist;
    hitend = &hit[ filter->hitcount ];
    for( ; hit < hitend ; hit++ )
    {
      vertexindex = hit->vertexindex;
      if( vertexindex == -1 )
        continue;
      vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );
      if( vertexindex == triangle->v[0] )
      {
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
        if( !( rbTriTriIntersectionSharedCheckP( vertex2, vcut1, vcut2, triangle->plane, vertex0, vertex1, bpdata->filterplanecutdistance ) ) )
          continue;
#else
        if( !( rbTriTriIntersectionSharedCheck( vertex2, vcut1, vcut2, vertex0, vertex1, bpdata->filterplanecutdistance ) ) )
          continue;
#endif
      }
      else if( vertexindex == triangle->v[1] )
      {
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
        if( !( rbTriTriIntersectionSharedCheckP( vertex2, vcut2, vcut0, triangle->plane, vertex0, vertex1, bpdata->filterplanecutdistance ) ) )
          continue;
#else
        if( !( rbTriTriIntersectionSharedCheck( vertex2, vcut2, vcut0, vertex0, vertex1, bpdata->filterplanecutdistance ) ) )
          continue;
#endif
      }
      else if( vertexindex == triangle->v[2] )
      {
#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
        if( !( rbTriTriIntersectionSharedCheckP( vertex2, vcut0, vcut1, triangle->plane, vertex0, vertex1, bpdata->filterplanecutdistance ) ) )
          continue;
#else
        if( !( rbTriTriIntersectionSharedCheck( vertex2, vcut0, vcut1, vertex0, vertex1, bpdata->filterplanecutdistance ) ) )
          continue;
#endif
      }
      else
      {


#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
        if( !( rbTriTriIntersectionCheckP( vcut0, vcut1, vcut2, triangle->plane, vertex0, vertex1, vertex2 ) ) )
          continue;
#else
        if( !( rbTriTriIntersectionCheck( vcut0, vcut1, vcut2, vertex0, vertex1, vertex2 ) ) )
          continue;
#endif


#ifdef DEBUG_VERBOSE_FILTER
printf( "          INTERSECTION BETWEEN %d,%d,%d AND %d,%d,%d\n", (int)result->triangle.v[0], (int)result->triangle.v[1], (int)vertexindex, (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif
      }

#ifdef DEBUG_VERBOSE_FILTER
printf( "          Triangle Intersection %d,%d,%d\n", triangle->v[0], triangle->v[1], triangle->v[2] );
printf( "            Test Vertexindex %d,%d,%d\n", (int)result->triangle.v[0], (int)result->triangle.v[1], (int)vertexindex );
printf( "              Cut Away Vertexindex %d\n", (int)vertexindex );
#endif

      hit->vertexindex = -1;
      filter->hitvalid--;
    }
  }

  if( !( filter->hitvalid ) )
    return 0;

  return 1;
}



/********/



static int bpPivotTryTriangle( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpHit *hit, bpMesh *mesh, bpPatch *patch )
{
  int trival, grade, level, newhingeflags, conflictdepth;
  bpf *vertex0, *vertex1, *vertex2;
  bpf center[BP_AXIS_COUNT];
  bpTriangle *triangle;

  triangle = bpMeshQueryTriangle( tdata, mesh );
  triangle->v[0] = hinge->edge.v[0];
  triangle->v[1] = hinge->edge.v[1];
  triangle->v[2] = hit->vertexindex;

  grade = hinge->edge.grade;
  if( hit->statusflags & BP_PIVOT_STATUS_FLAGS_BAD )
    grade = BP_EDGE_GRADE_BAD;

  /* Make sure the triangle doesn't already exist */
  trival = bpValidateTriangle( bpdata, tdata, patch, triangle, grade, hinge->flags );
  if( trival != BP_VALIDATE_TRIANGLE_SUCCESS )
    return trival;

  /* Mark new vertex as used, forbid seed triangles on it */
  mmBitMapSet( &bpdata->vertexmap, triangle->v[2] );

  /* Add the triangle to the thread's mesh */
  vertex0 = ADDRESS( bpdata->vertex, triangle->v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, triangle->v[1] * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, triangle->v[2] * bpdata->vertexstride );
  center[BP_AXIS_X] = (1.0/3.0) * ( vertex0[BP_AXIS_X] + vertex1[BP_AXIS_X] + vertex2[BP_AXIS_X] );
  center[BP_AXIS_Y] = (1.0/3.0) * ( vertex0[BP_AXIS_Y] + vertex1[BP_AXIS_Y] + vertex2[BP_AXIS_Y] );
  center[BP_AXIS_Z] = (1.0/3.0) * ( vertex0[BP_AXIS_Z] + vertex1[BP_AXIS_Z] + vertex2[BP_AXIS_Z] );

#ifdef CONFIG_ENABLE_TRIANGLE_PLANE_STORAGE
  M3D_VectorCopy( triangle->plane, hit->normal );
  M3D_VectorNormalize( triangle->plane );
  triangle->plane[3] = -M3D_VectorDotProduct( triangle->plane, vertex0 );
#endif

  bpMeshAddTriangle( tdata, mesh );
  bpSpaceMapAddTriangle( &bpdata->spacemap, triangle, center );

  /* If grade is bad, don't add hinges to triangle */
#ifndef CONFIG_ENABLE_BAD_GRADE_GENERATE_CONNECT
  if( hit->statusflags & BP_PIVOT_STATUS_FLAGS_BAD )
    return BP_VALIDATE_TRIANGLE_SUCCESS;
#endif

  /* Add the triangle's edges as active hinges to the patch */
  newhingeflags = BP_HINGE_FLAGS_PIVOT;
  level = BP_PATCH_HINGE_LEVEL_NORMAL;
  grade = BP_EDGE_GRADE_HIGH;
  conflictdepth = BP_HINGE_GET_CONFLICTDEPTH( hinge->flags );
  if( conflictdepth )
  {
    newhingeflags |= BP_HINGE_FLAGS_ORPHAN;
    level = BP_PATCH_HINGE_LEVEL_CONFLICT;
    grade = BP_EDGE_GRADE_REPAIR;
    conflictdepth++;
    if( conflictdepth >= BP_CONFLICTDEPTH_MAX )
    {
      level = BP_PATCH_HINGE_LEVEL_ERROR;
      conflictdepth = 0;
      tdata->info.conflictcount++;
 #ifdef DEBUG_VERBOSE_ERROR
      printf( "WARNING : Conflict Depth Exceeded, Hinge %d,%d (%d) Promoted To ERROR Level\n", (int)triangle->v[2], (int)triangle->v[1], (int)triangle->v[0] );
      printf( "WARNING : Conflict Depth Exceeded, Hinge %d,%d (%d) Promoted To ERROR Level\n", (int)triangle->v[0], (int)triangle->v[2], (int)triangle->v[1] );
 #endif
    }
  }

#ifdef CONFIG_ENABLE_BAD_GRADE_GENERATE_CONNECT
  if( hit->statusflags & BP_PIVOT_STATUS_FLAGS_BAD )
    newhingeflags |= BP_HINGE_FLAGS_JOIN | BP_HINGE_FLAGS_FORCE | BP_HINGE_FLAGS_LOST | BP_HINGE_FLAGS_END;
#endif

  bpPatchAddHinge( tdata, patch, level, triangle->v[2], triangle->v[1], triangle->v[0], hit->spherecenter, hit->normal, grade, BP_HINGE_COMPUTE_FLAGS( newhingeflags, conflictdepth ), -1 );
  bpPatchAddHinge( tdata, patch, level, triangle->v[0], triangle->v[2], triangle->v[1], hit->spherecenter, hit->normal, grade, BP_HINGE_COMPUTE_FLAGS( newhingeflags, conflictdepth ), -1 );

  return BP_VALIDATE_TRIANGLE_SUCCESS;
}


static void bpHingeUpgrade( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpPatch *patch, int hitflag )
{
  // 2012-10-22 ch3: hingelevel was set but never used, so commented
  int /*hingelevel,*/ newhingelevel, newhingegrade, newhingeflags, conflictdepth;

  if( hinge->flags & BP_HINGE_FLAGS_END )
    return;

  if( !( hitflag ) )
  {
    newhingeflags = BP_HINGE_FLAGS_PIVOT;
    newhingelevel = BP_PATCH_HINGE_LEVEL_NEXT;
    newhingegrade = BP_EDGE_GRADE_HIGH;
    conflictdepth = 0;
  }
  else
  {
    conflictdepth = BP_HINGE_GET_CONFLICTDEPTH( hinge->flags );
    newhingeflags = BP_HINGE_FLAGS_PIVOT | ( hinge->flags & ( BP_HINGE_FLAGS_ORPHAN | BP_HINGE_FLAGS_LOST ) );
    if( conflictdepth )
    {
      newhingelevel = BP_PATCH_HINGE_LEVEL_CONFLICT;
      if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
        newhingeflags |= BP_HINGE_FLAGS_JOIN;
      else if( !( hinge->flags & BP_HINGE_FLAGS_FORCE ) )
        newhingeflags |= BP_HINGE_FLAGS_JOIN | BP_HINGE_FLAGS_FORCE;
      else
      {
        conflictdepth += BP_CONFLICTDEPTH_LOST_INCREMENT;
        if( !( hinge->flags & BP_HINGE_FLAGS_LOST ) && ( conflictdepth < BP_CONFLICTDEPTH_MAX ) )
        {
          newhingeflags |= BP_HINGE_FLAGS_LOST;
          newhingelevel = BP_PATCH_HINGE_LEVEL_CONFLICT;
        }
        else
        {
          conflictdepth = 1;
          newhingeflags &= ~BP_HINGE_FLAGS_LOST;
          newhingelevel = BP_PATCH_HINGE_LEVEL_NEXT;
 #ifdef DEBUG_VERBOSE_WARNING
          printf( "WARNING : Unresolved Hanging Hinge %d,%d (%d)\n", (int)hinge->edge.v[0], (int)hinge->edge.v[1], (int)hinge->edge.v[2] );
 #endif
/*
printf( "f %d %d 0\n", (int)hinge->edge.v[0]+1, (int)hinge->edge.v[1]+1 );
*/
          tdata->info.hingelostcount++;
        }
      }
      newhingegrade = hinge->edge.grade;
    }
    else
    {
      //hingelevel = BP_HINGE_GET_LEVEL( hinge );
      newhingeflags = BP_HINGE_FLAGS_PIVOT | ( hinge->flags & BP_HINGE_FLAGS_ORPHAN );
      if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
      {
        newhingeflags |= BP_HINGE_FLAGS_JOIN;
        newhingelevel = BP_PATCH_HINGE_LEVEL_JOIN;
        newhingegrade = hinge->edge.grade;
      }
      else if( !( hinge->flags & BP_HINGE_FLAGS_FORCE ) )
      {
        newhingeflags |= BP_HINGE_FLAGS_JOIN | BP_HINGE_FLAGS_FORCE;
        newhingelevel = BP_PATCH_HINGE_LEVEL_REPAIR;
        newhingegrade = BP_EDGE_GRADE_REPAIR;
      }
      else
      {
        newhingeflags = BP_HINGE_FLAGS_PIVOT;
        newhingelevel = BP_PATCH_HINGE_LEVEL_CONFLICT;
        newhingegrade = BP_EDGE_GRADE_REPAIR;
        conflictdepth = 1;
      }
    }
  }

#ifdef CONFIG_ENABLE_CONFLICT_PIVOT
  bpPatchAddHinge( tdata, patch, newhingelevel, hinge->edge.v[0], hinge->edge.v[1], hinge->edge.v[2], hinge->spherecenter, hinge->normal, newhingegrade, BP_HINGE_COMPUTE_FLAGS( newhingeflags, conflictdepth ), hinge->conflictpivot );
#else
  bpPatchAddHinge( tdata, patch, newhingelevel, hinge->edge.v[0], hinge->edge.v[1], hinge->edge.v[2], hinge->spherecenter, hinge->normal, newhingegrade, BP_HINGE_COMPUTE_FLAGS( newhingeflags, conflictdepth ), -1 );
#endif

  return;
}



/********/



static void bpPivotFilterJoin( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpPivotResult *result )
{
  int joinvalue;
  bpHit *hit, *hitfind, *hitend;

  joinvalue = BP_TRIANGLE_JOIN_COUNT_PARTIAL + BP_TRIANGLE_JOIN_COUNT_PARTIAL;
  if( hinge->flags & BP_HINGE_FLAGS_FORCE )
    joinvalue = BP_TRIANGLE_JOIN_COUNT_FULL + BP_TRIANGLE_JOIN_COUNT_PARTIAL;

  hit = result->hitbuffer.hitlist;
  hitend = &hit[ result->hitbuffer.hitcount ];
  for( hitfind = hit ; hitfind < hitend ; hitfind++ )
  {
    if( !( bpTriangleJoinCount( bpdata, result->triangle.v[0], result->triangle.v[1], hitfind->vertexindex, hinge->flags, joinvalue ) ) )
    {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "          Pivot %d Discarded : Join Unlinked Hinges %d,%d ; %d,%d ( 0x%x )\n", (int)hitfind->vertexindex, (int)hitfind->vertexindex, (int)result->triangle.v[1], (int)result->triangle.v[0], (int)hitfind->vertexindex, joinvalue );
#endif
      hitfind->vertexindex = -1;
    }
  }

  return;
}


static void bpPivotFilterVersion4( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpPivotResult *result, bpf ballradius, bpMesh *mesh, bpPatch *patch )
{
  int hitcount, trival;
  bpf anglerange, collisionradius;
  bpf *vertex0, *vertex1, *vertex2;
  bpf midpoint[BP_AXIS_COUNT], center[BP_AXIS_COUNT];
  bpHit *hit, *hitfind, *hitend;
  bpPivotFilter filter;

  /* Compute midpoint of hinge and center of best pivot ; it's a good approximation for locking and triangle search */
  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, result->triangle.v[2] * bpdata->vertexstride );
  midpoint[BP_AXIS_X] = vertex0[BP_AXIS_X] + vertex1[BP_AXIS_X];
  midpoint[BP_AXIS_Y] = vertex0[BP_AXIS_Y] + vertex1[BP_AXIS_Y];
  midpoint[BP_AXIS_Z] = vertex0[BP_AXIS_Z] + vertex1[BP_AXIS_Z];
  center[BP_AXIS_X] = (1.0/3.0) * ( midpoint[BP_AXIS_X] + vertex2[BP_AXIS_X] );
  center[BP_AXIS_Y] = (1.0/3.0) * ( midpoint[BP_AXIS_Y] + vertex2[BP_AXIS_Y] );
  center[BP_AXIS_Z] = (1.0/3.0) * ( midpoint[BP_AXIS_Z] + vertex2[BP_AXIS_Z] );
  midpoint[BP_AXIS_X] *= 0.5;
  midpoint[BP_AXIS_Y] *= 0.5;
  midpoint[BP_AXIS_Z] *= 0.5;

  /* Sort all hits from best to worst */
  if( ballradius <= bpdata->ballradiuspivot )
    bpHitBufferSort( &result->hitbuffer );
  else
    bpHitBufferSortFull( &result->hitbuffer );

#ifdef DEBUG_VERBOSE_FILTER
printf( "      Filter Hinge %d,%d ; %d Hits\n", (int)result->triangle.v[0], (int)result->triangle.v[1], result->hitbuffer.hitcount );
hit = result->hitbuffer.hitlist;
hitend = &hit[ result->hitbuffer.hitcount ];
for( ; hit < hitend ; hit++ )
printf( "        Hit %d : Angle %f\n", (int)hit->vertexindex, hit->pivotangle );
#endif

  /* Acquire spatial lock */
  result->spacelockflag = 1;
  bpSpaceLockAcquire( bpdata, &bpdata->spacemap, &result->spacelock, center, ballradius );

#ifdef CONFIG_ENABLE_JOIN_NO_FILTERING
  /* Hinge JOIN filtering */
  if( hinge->flags & BP_HINGE_FLAGS_JOIN )
  {
    bpPivotFilterJoin( bpdata, tdata, hinge, result );
    bpHitBufferPack( &result->hitbuffer );
#ifdef DEBUG_VERBOSE_PIVOT
printf( "      Filter Join Hinge %d,%d ; %d Hits\n", (int)result->triangle.v[0], (int)result->triangle.v[1], result->hitbuffer.hitcount );
#endif

    hit = result->hitbuffer.hitlist;
    hitend = &hit[ result->hitbuffer.hitcount ];
    for( hitfind = hit ; hitfind < hitend ; hitfind++ )
    {
 #ifdef CONFIG_ENABLE_CONFLICT_PIVOT
      if( hitfind->vertexindex == hinge->conflictpivot )
        continue;
 #endif
      if( hitfind->vertexindex == -1 )
        continue;
#ifdef DEBUG_VERBOSE_PIVOT
printf( "      Try Triangle %d,%d,%d ; Angle %f\n", (int)hinge->edge.v[0], (int)hinge->edge.v[1], (int)hitfind->vertexindex, hitfind->pivotangle );
#endif
      trival = bpPivotTryTriangle( bpdata, tdata, hinge, hitfind, mesh, patch );
      if( trival == BP_VALIDATE_TRIANGLE_SUCCESS )
        return;
      else if( trival == BP_VALIDATE_TRIANGLE_DISCARD )
      {
        bpHingeUpgrade( bpdata, tdata, hinge, patch, 0 );
        return;
      }
    }

    bpHingeUpgrade( bpdata, tdata, hinge, patch, 1 );
    return;
  }
#endif

  hit = result->hitbuffer.hitlist;
  hitend = &hit[ result->hitbuffer.hitcount ];
#ifdef CONFIG_ENABLE_CONFLICT_PIVOT
  if( hinge->conflictpivot != -1 )
  {
    for( hitfind = hit ; hitfind < hitend ; hitfind++ )
    {
      if( hitfind->vertexindex == hinge->conflictpivot )
        hitfind->vertexindex = -1;
    }
  }
#endif

  /* Hinge JOIN filtering */
#ifndef CONFIG_ENABLE_JOIN_NO_FILTERING
  if( hinge->flags & BP_HINGE_FLAGS_JOIN )
  {
    bpPivotFilterJoin( bpdata, tdata, hinge, result );
#ifdef DEBUG_VERBOSE_PIVOT
    bpHitBufferPack( &result->hitbuffer );
printf( "      Filter Join Hinge %d,%d ; %d Hits\n", (int)result->triangle.v[0], (int)result->triangle.v[1], result->hitbuffer.hitcount );
#endif
  }

#endif




#ifdef CONFIG_ENABLE_PIVOT_NORMAL_PRIORITIZATION
  int fallbackflag;
#endif


/*

If normal prioritization is enabled, "good" pivots were sorted before "bad" ones

- Try all the pivots cospherical to the best "good" pivot
- Try all the pivots cospherical to the best "bad" pivot

*/


#if 0
printf( "PIVOT ( Join %d )\n", hinge->flags & BP_HINGE_FLAGS_JOIN );
  hit = result->hitbuffer.hitlist;
  hitend = &hit[ result->hitbuffer.hitcount ];
  for( hitfind = hit ; hitfind < hitend ; hitfind++ )
  {
if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
    printf( " List %f (%d) : %d\n", hitfind->pivotangle, hitfind->statusflags, hitfind->vertexindex );
  }
#endif


  for( ; ; )
  {
    hit = result->hitbuffer.hitlist;
    hitend = &hit[ result->hitbuffer.hitcount ];
    anglerange = hit->pivotangle - bpdata->pivotcosphericalrange;

#ifdef CONFIG_ENABLE_PIVOT_NORMAL_PRIORITIZATION
    fallbackflag = hit->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK;
#endif



#if 0
if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
printf( "  Try %f (%d)\n", hit->pivotangle, hit->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK );
#endif



    /* Count how many hits are cospherical */
    hitcount = 0;
    for( hitfind = hit ; hitfind < hitend ; hitfind++ )
    {
      if( hitfind->pivotangle < anglerange )
        break;
#ifdef CONFIG_ENABLE_PIVOT_NORMAL_PRIORITIZATION
      if( ( hitfind->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK ) != fallbackflag )
        break;
#endif
      hitcount++;
    }

    if( !( hitcount ) )
      break;
    filter.result = result;
    filter.hitcount = hitcount;
    filter.hitvalid = hitcount;

    /* TODO: Determine proper radius to search for intersecting triangles */
    collisionradius = 1.0 * ballradius;

    /* Filter out all triangles intersected by other geometry */
    bpSpaceMapPickRadius( bpdata, &bpdata->spacemap, center, collisionradius, bpPivotFilterTrianglePick, &filter );

    /* Attempt validating the triangles in order, break when one is found */
    hit = result->hitbuffer.hitlist;
    hitend = &hit[ hitcount ];
    for( hitfind = hit ; hitfind < hitend ; hitfind++ )
    {
      if( hitfind->vertexindex == -1 )
        continue;
#ifdef DEBUG_VERBOSE_PIVOT
printf( "      Try Triangle %d,%d,%d ; Angle %f\n", (int)hinge->edge.v[0], (int)hinge->edge.v[1], (int)hitfind->vertexindex, hitfind->pivotangle );
#endif
      trival = bpPivotTryTriangle( bpdata, tdata, hinge, hitfind, mesh, patch );
      if( trival == BP_VALIDATE_TRIANGLE_SUCCESS )
        return;
      else if( trival == BP_VALIDATE_TRIANGLE_DISCARD )
      {
        bpHingeUpgrade( bpdata, tdata, hinge, patch, 0 );
        return;
      }
      hitfind->vertexindex = -1;
    }



#if 0
if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
printf( "    NO HIT\n" );
#endif



#ifdef CONFIG_ENABLE_PIVOT_COSPHERICAL_ONLY
    if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
    {
 #ifdef CONFIG_ENABLE_PIVOT_NORMAL_PRIORITIZATION
      if( fallbackflag )
        break;
      /* We tried cospherical pivots that weren't marked bad, next try cospherical to best bad pivot */
      hitend = &hit[ result->hitbuffer.hitcount ];
      for( hitfind = hit ; hitfind < hitend ; hitfind++ )
      {
        if( hitfind->statusflags & BP_PIVOT_STATUS_FLAGS_FALLBACK )
          break;
        hitfind->vertexindex = -1;
      }
 #else
      break;
 #endif
    }
#endif

    /* Remove all discarded vertices from result buffer */
    bpHitBufferPack( &result->hitbuffer );



#if 0
if( !( hinge->flags & BP_HINGE_FLAGS_JOIN ) )
printf( "    Pack %d\n", result->hitbuffer.hitcount );
#endif



    if( !( result->hitbuffer.hitcount ) )
      break;
  }

  bpHingeUpgrade( bpdata, tdata, hinge, patch, 1 );

  return;
}





static void bpPivot( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpPivotResult *result, bpf ballradius, bpMesh *mesh, bpPatch *patch )
{
  bpf radsquared;
  bpf thorusradius;
  bpf hingelensquared;
  bpPivotPickData pivotdata;

  /* Build pivot data struct which is read in the callback as we pick each vertex within the thorus space */
  pivotdata.v0 = hinge->edge.v[0];
  pivotdata.v1 = hinge->edge.v[1];
  pivotdata.vprev = hinge->edge.v[2];
  pivotdata.vertex0 = ADDRESS( bpdata->vertex, pivotdata.v0 * bpdata->vertexstride );
  pivotdata.vertex1 = ADDRESS( bpdata->vertex, pivotdata.v1 * bpdata->vertexstride );
  pivotdata.normal0 = ADDRESS( bpdata->normal, pivotdata.v0 * bpdata->normalstride );
  pivotdata.normal1 = ADDRESS( bpdata->normal, pivotdata.v1 * bpdata->normalstride );

  /* Hinge vector */
  M3D_VectorSubStore( pivotdata.vecta, pivotdata.vertex1, pivotdata.vertex0 );
  hingelensquared = pivotdata.vecta[BP_AXIS_X]*pivotdata.vecta[BP_AXIS_X] + pivotdata.vecta[BP_AXIS_Y]*pivotdata.vecta[BP_AXIS_Y] + pivotdata.vecta[BP_AXIS_Z]*pivotdata.vecta[BP_AXIS_Z];
  pivotdata.vectamagn = bpfsqrt( hingelensquared );

  /* Determine midpoint of hinge and center of thorus */
  pivotdata.midpoint[BP_AXIS_X] = 0.5 * ( pivotdata.vertex0[BP_AXIS_X] + pivotdata.vertex1[BP_AXIS_X] );
  pivotdata.midpoint[BP_AXIS_Y] = 0.5 * ( pivotdata.vertex0[BP_AXIS_Y] + pivotdata.vertex1[BP_AXIS_Y] );
  pivotdata.midpoint[BP_AXIS_Z] = 0.5 * ( pivotdata.vertex0[BP_AXIS_Z] + pivotdata.vertex1[BP_AXIS_Z] );

  /* Compute base pivot vector */
  M3D_VectorSubStore( pivotdata.vecta, pivotdata.vertex1, pivotdata.vertex0 );
  M3D_VectorSubStore( pivotdata.basepivot, hinge->spherecenter, pivotdata.midpoint );
  M3D_VectorNormalize( pivotdata.basepivot );

#ifdef DEBUG_VERBOSE_PIVOT
printf( "      Midpoint : %f,%f,%f\n", pivotdata.midpoint[0], pivotdata.midpoint[1], pivotdata.midpoint[2] );
printf( "      Basepivot : %f,%f,%f\n", pivotdata.basepivot[0], pivotdata.basepivot[1], pivotdata.basepivot[2] );
#endif

  /* Compute pivot plane equation going through hinge and spherecenter */
  M3D_VectorCrossProduct( pivotdata.pivotplane, pivotdata.vecta, pivotdata.basepivot );
  M3D_VectorNormalize( pivotdata.pivotplane );
  pivotdata.pivotplane[3] = -( pivotdata.pivotplane[0]*pivotdata.midpoint[0] + pivotdata.pivotplane[1]*pivotdata.midpoint[1] + pivotdata.pivotplane[2]*pivotdata.midpoint[2] );

  /* Store hinge surface plane */
  M3D_VectorCopy( pivotdata.hingeplane, hinge->normal );
  pivotdata.hingeplane[3] = -( pivotdata.hingeplane[0]*pivotdata.midpoint[0] + pivotdata.hingeplane[1]*pivotdata.midpoint[1] + pivotdata.hingeplane[2]*pivotdata.midpoint[2] );

  /* Where results for the best pick will be stored */
  pivotdata.result = result;
  result->triangle.v[0] = hinge->edge.v[0];
  result->triangle.v[1] = hinge->edge.v[1];
  result->triangle.v[2] = -1;
  pivotdata.bestangle = -4.0;
  pivotdata.bestanglemin = -4.0;
  pivotdata.bestanglemax = -4.0;
/*
  pivotdata.anglecollectrange = bpdata->pivotcosphericalrange;
*/
  pivotdata.anglecollectrange = 4.0;

  result->spacelockflag = 0;
  bpHitBufferReset( &result->hitbuffer );

  /* If joining existing hinges is required for placement */
  pivotdata.hingeflags = hinge->flags;

  /* Extend search radius for JOIN hinges */
  if( hinge->flags & BP_HINGE_FLAGS_JOIN )
    ballradius *= bpdata->pivotjoinradiusfactor;
  if( hinge->flags & BP_HINGE_FLAGS_FORCE )
    ballradius *= bpdata->pivotforceradiusfactor;


/*
  if( BP_HINGE_GET_CONFLICTDEPTH( hinge->flags ) >= (BP_CONFLICTDEPTH_MAX/2) )
    ballradius *= 1.5;
*/
/*
  if( BP_HINGE_GET_CONFLICTDEPTH( hinge->flags ) )
    ballradius *= 1.5;
*/


  /* All possible spheres around the hinge make a thorus */
  /* Determine radius of thorus */
  /* Hinge too long for ball radius? */
  radsquared = ballradius * ballradius;
  hingelensquared *= 0.25;
  if( hingelensquared < radsquared )
    thorusradius = bpfsqrt( radsquared - hingelensquared );
  else
    thorusradius = 0.0;


  /* Pick all vertices inside our thorus and try to build a new triangle with them */
  pivotdata.ballradius = ballradius;
  bpGridPickRadius( bpdata, bpdata->grid, pivotdata.midpoint, thorusradius + ballradius, bpPivotPick, (void *)&pivotdata );

  /* Nothing found? */
  if( !( result->hitbuffer.hitcount ) )
  {
#ifdef DEBUG_VERBOSE_PIVOT
printf( "      No Pivot Found\n" );
#endif
    bpHingeUpgrade( bpdata, tdata, hinge, patch, 0 );
    return;
  }


/*
  pivotval = BP_PIVOT_JOINMESH;
  if( pivotdata.hingeflags & BP_HINGE_FLAGS_JOIN )
    bpHitBufferSort( &result->hitbuffer );
  else
  {
    pivotval = bpPivotFilterVersion1( bpdata, tdata, result );
  }
*/
/*
  pivotval = bpPivotFilterVersion2( bpdata, tdata, result, ballradius );
*/

/*
  pivotval = bpPivotFilterVersion3( bpdata, tdata, hinge, result, ballradius );
*/

  bpPivotFilterVersion4( bpdata, tdata, hinge, result, ballradius, mesh, patch );


  if( result->spacelockflag )
    bpSpaceLockRelease( bpdata, &bpdata->spacemap, &result->spacelock );


  return;
}


/****/


#if 0

typedef struct
{
  bpEdge edge;
  bpf normal[BP_AXIS_COUNT];
  int flags;
  void *next;
} bpSegment;


static void bpBuildSphereMesh( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpPivotResult *result, bpMesh *mesh, bpPatch *patch )
{
  int hitcount, coplanarflag, trival, newhingelevel, newhingegrade, newhingeflags, conflictdepth;
  int32_t grade;
  bpi vertexindex, bestvertexindex;
  bpf normalmagn, normalmagninv, agreement, agreementsum;
  bpf cpdist, bestcpdist, vectamagn;
#ifdef CONFIG_SPHERE_PLANAR_TEST
  bpf sortdist, sortdistmin, sortdistmax;
#endif
  bpf *vertex0, *vertex1, *vertex2;
  bpf *normal0, *normal1, *normal2;
  bpf normal[BP_AXIS_COUNT];
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], center[BP_AXIS_COUNT], spherecenter[BP_AXIS_COUNT];
  bpf sortplane[4], distplane[4], triplane[4];
  bpTriangle *triangle;
  bpSegment *segment;
  bpSegment *segmentlist;
  bpHit *hit;
  bpEdge edgecheck[2];

  segmentlist = 0;

  segment = mmBlockAlloc( &tdata->segmentblock );
  segment->edge.v[0] = hinge->edge.v[0];
  segment->edge.v[1] = hinge->edge.v[1];
  segment->edge.v[2] = hinge->edge.v[2];
  M3D_VectorCopy( segment->normal, hinge->normal );
  segment->flags = hinge->flags;
  segment->next = segmentlist;
  segmentlist = segment;

#ifdef CONFIG_SPHERE_PLANAR_TEST
  sortdistmin = -bpdata->pivotcoplanardist;
  sortdistmax = bpdata->pivotcoplanardist;
#endif


  hit = result->hitbuffer.hitlist;
  M3D_VectorCopy( spherecenter, hit->spherecenter );

  for( ; ; )
  {
    segment = segmentlist;
    if( !( segment ) )
      break;
    segmentlist = segment->next;

    /* Set up segment for processing */
    vertex0 = ADDRESS( bpdata->vertex, segment->edge.v[0] * bpdata->vertexstride );
    vertex1 = ADDRESS( bpdata->vertex, segment->edge.v[1] * bpdata->vertexstride );
    M3D_VectorSubStore( vecta, vertex1, vertex0 );
    vectamagn = M3D_VectorMagnitude( vecta );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "      Sphere Segment %d,%d (%d), flags 0x%x\n", (int)segment->edge.v[0], (int)segment->edge.v[1], (int)segment->edge.v[2], segment->flags );
#endif

    /* Set up a sort plane that goes through the sphere center */
    M3D_VectorSubStore( vectb, spherecenter, vertex0 );
    M3D_VectorCrossProduct( sortplane, vectb, vecta );
    M3D_VectorNormalize( sortplane );
    sortplane[3] = -( sortplane[0]*vertex0[0] + sortplane[1]*vertex0[1] + sortplane[2]*vertex0[2] );

    /* Set up a distance plane that always returns true */
    distplane[0] = 0.0;
    distplane[1] = 0.0;
    distplane[2] = 0.0;
    distplane[3] = bpdata->ballradiusmin;
    bestcpdist = -1.0;

    bestvertexindex = -1;
    coplanarflag = 0;

    /* Find the bottom most vertex */
    hit = result->hitbuffer.hitlist;
    for( hitcount = result->hitbuffer.hitcount ; hitcount ; hitcount--, hit++ )
    {
      vertexindex = hit->vertexindex;
      if( vertexindex == -1 )
        continue;
      if( ( vertexindex == segment->edge.v[0] ) || ( vertexindex == segment->edge.v[1] ) || ( vertexindex == segment->edge.v[2] ) )
        continue;
      vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );

#ifdef CONFIG_SPHERE_PLANAR_TEST

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere Vertex %d : Dist %f ( min %f, max %f )\n", (int)vertexindex, M3D_PlanePoint( sortplane, vertex2 ), sortdistmin, sortdistmax );
#endif

      sortdist = M3D_PlanePoint( sortplane, vertex2 );
      if( sortdist > sortdistmax )
        continue;

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Sphere Coplanar Dist Plane %f ( best %f )\n", M3D_PlanePoint( distplane, vertex2 ), bestcpdist );
#endif

      coplanarflag = 0;
      if( sortdist > sortdistmin )
      {
        coplanarflag = 1;
        cpdist = M3D_PlanePoint( distplane, vertex2 );
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Sphere Coplanar\n" );
#endif
        /* Coplanar and behind segment? Drop that */
        if( cpdist <= 0.0 )
          continue;
        /* Is it closer than our previous best coplanar hit? */
        if( cpdist > bestcpdist )
          continue;
      }
#else

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Sphere Coplanar Dist Plane %f ( best %f )\n", M3D_PlanePoint( distplane, vertex2 ), bestcpdist );
#endif

      cpdist = M3D_PlanePoint( distplane, vertex2 );
      if( cpdist <= 0.0 )
        continue;
#endif


      M3D_VectorSubStore( vectb, vertex2, vertex0 );
      M3D_VectorCrossProduct( normal, vectb, vecta );
      normalmagn = M3D_VectorMagnitude( normal );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
if( normalmagn <= CONF_COLLINEAR_MAGNITUDE_MIN * vectamagn * M3D_VectorMagnitude(vectb) )
printf( "          Warning : Collinear ( %f, %f )\n", normalmagn, M3D_VectorMagnitude(vectb) );
#endif

      if( normalmagn > CONF_COLLINEAR_MAGNITUDE_MIN * vectamagn * M3D_VectorMagnitude(vectb) )
      {
        normalmagninv = 1.0 / normalmagn;
        M3D_VectorMulScalarStore( sortplane, normal, normalmagninv );
        sortplane[3] = -( sortplane[0]*vertex0[0] + sortplane[1]*vertex0[1] + sortplane[2]*vertex0[2] );
      }


/****/

      M3D_VectorCrossProduct( distplane, vecta, sortplane );
      M3D_VectorNormalize( distplane );
      distplane[3] = -( distplane[0]*vertex0[0] + distplane[1]*vertex0[1] + distplane[2]*vertex0[2] );


      bestcpdist = M3D_PlanePoint( distplane, vertex2 );

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Best Vertex %d\n", (int)vertexindex );
#endif

      bestvertexindex = vertexindex;
    }

    if( bestvertexindex < 0 )
    {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere No Result For Segment %d,%d\n", segment->edge.v[0], segment->edge.v[1] );
#endif
      goto addballhinge;
    }

    vertex2 = ADDRESS( bpdata->vertex, bestvertexindex * bpdata->vertexstride );
    M3D_VectorSubStore( vectb, vertex2, vertex0 );
    M3D_VectorCrossProduct( triplane, vectb, vecta );
    M3D_VectorNormalize( triplane );
    triplane[3] = -( triplane[0]*vertex2[0] + triplane[1]*vertex2[1] + triplane[2]*vertex2[2] );

    if( M3D_VectorDotProduct( triplane, segment->normal ) < bpdata->hingenormalagreement )
    {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Fail : Angle too different from hinge normal ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", triplane[0], triplane[1], triplane[2], segment->normal[0], segment->normal[1], segment->normal[2] );
#endif
      goto addballhinge;
    }

    normal0 = ADDRESS( bpdata->normal, segment->edge.v[0] * bpdata->normalstride );

    agreementsum = 0.0;
    agreement = M3D_VectorDotProduct( triplane, normal0 );
    if( agreement < bpdata->pivotnormalagreementsoft )
    {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Fail : Disagreement with vertex0 normal ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", triplane[0], triplane[1], triplane[2], normal0[0], normal0[1], normal0[2] );
#endif
      goto addballhinge;
    }
    agreementsum += agreement;
    normal1 = ADDRESS( bpdata->normal, segment->edge.v[1] * bpdata->normalstride );
    agreement = M3D_VectorDotProduct( triplane, normal1 );
    if( agreement < bpdata->pivotnormalagreementsoft )
    {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Fail : Disagreement with vertex1 normal ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", triplane[0], triplane[1], triplane[2], normal1[0], normal1[1], normal1[2] );
#endif
      goto addballhinge;
    }
    agreementsum += agreement;
    normal2 = ADDRESS( bpdata->normal, segment->edge.v[2] * bpdata->normalstride );
    agreement = M3D_VectorDotProduct( triplane, normal2 );
    if( agreement < bpdata->pivotnormalagreementsoft )
    {
#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Fail : Disagreement with vertex2 normal ( %.2f,%.2f,%.2f ; %.2f,%.2f,%.2f )\n", triplane[0], triplane[1], triplane[2], normal2[0], normal2[1], normal2[2] );
#endif
      goto addballhinge;
    }
    agreementsum += agreement;

    if( agreementsum < bpdata->pivotnormalagreementsumsoft )
    {
#ifdef DEBUG_VERBOSE_SEED
printf( "        Fail : Overall disagreement with normals %f < %f\n", agreementsum, bpdata->pivotnormalagreementsumsoft );
#endif
      goto addballhinge;
    }

    triangle = bpMeshQueryTriangle( tdata, mesh );
    triangle->v[0] = segment->edge.v[0];
    triangle->v[1] = segment->edge.v[1];
    triangle->v[2] = bestvertexindex;

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere Try Triangle %d,%d,%d...\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif

    grade = BP_EDGE_GRADE_LOW;

    /* Make sure the triangle doesn't already exist */
    trival = bpValidateTriangle( bpdata, tdata, patch, triangle, grade, BP_HINGE_FLAGS_PIVOT );
    if( trival == BP_VALIDATE_TRIANGLE_FAILURE )
    {
      if( coplanarflag )
        goto cptry;
/*
      else
        goto addballhinge;
*/
    }

    validtriangle:
    mmBlockRelease( &tdata->segmentblock, segment );
    if( trival != BP_VALIDATE_TRIANGLE_SUCCESS )
      continue;

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere Triangle %d,%d,%d Validated\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif

    /* Mark new vertex as used, forbid seed triangles on it */
    mmBitMapSet( &bpdata->vertexmap, bestvertexindex );

    /* Add the triangle to the thread's mesh */
    center[BP_AXIS_X] = (1.0/3.0) * ( vertex0[BP_AXIS_X] + vertex1[BP_AXIS_X] + vertex2[BP_AXIS_X] );
    center[BP_AXIS_Y] = (1.0/3.0) * ( vertex0[BP_AXIS_Y] + vertex1[BP_AXIS_Y] + vertex2[BP_AXIS_Y] );
    center[BP_AXIS_Z] = (1.0/3.0) * ( vertex0[BP_AXIS_Z] + vertex1[BP_AXIS_Z] + vertex2[BP_AXIS_Z] );
    bpMeshAddTriangle( tdata, mesh );
    bpSpaceMapAddTriangle( &bpdata->spacemap, triangle, center );

    /* Add the triangle's edges as active segments to the sphere mesh */
    segment = mmBlockAlloc( &tdata->segmentblock );
    segment->edge.v[0] = triangle->v[2];
    segment->edge.v[1] = triangle->v[1];
    segment->edge.v[2] = triangle->v[0];
    M3D_VectorCopy( segment->normal, triplane );
    segment->flags = BP_HINGE_FLAGS_SPHEREPIVOT;
    segment->next = segmentlist;
    segmentlist = segment;

    segment = mmBlockAlloc( &tdata->segmentblock );
    segment->edge.v[0] = triangle->v[0];
    segment->edge.v[1] = triangle->v[2];
    segment->edge.v[2] = triangle->v[1];
    M3D_VectorCopy( segment->normal, triplane );
    segment->flags = BP_HINGE_FLAGS_SPHEREPIVOT;
    segment->next = segmentlist;
    segmentlist = segment;

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere Add Hinge %d,%d (%d)\n", (int)triangle->v[2], (int)triangle->v[1], (int)triangle->v[0] );
printf( "        Sphere Add Hinge %d,%d (%d)\n", (int)triangle->v[0], (int)triangle->v[2], (int)triangle->v[1] );
#endif

    continue;

/****/

    cptry:

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "      Coplanar Search\n" );
#endif
    hit = result->hitbuffer.hitlist;
    for( hitcount = result->hitbuffer.hitcount ; hitcount ; hitcount--, hit++ )
    {
      vertexindex = hit->vertexindex;
      if( vertexindex == -1 )
        continue;
      if( ( vertexindex == segment->edge.v[0] ) || ( vertexindex == segment->edge.v[1] ) || ( vertexindex == segment->edge.v[2] ) )
        continue;
      vertex2 = ADDRESS( bpdata->vertex, vertexindex * bpdata->vertexstride );


#ifdef CONFIG_SPHERE_PLANAR_TEST
 #ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere Vertex %d : Dist %f ( min %f, max %f )\n", (int)vertexindex, M3D_PlanePoint( sortplane, vertex2 ), sortdistmin, sortdistmax );
 #endif
      sortdist = M3D_PlanePoint( sortplane, vertex2 );
      if( sortdist > sortdistmax )
        continue;
      if( sortdist < sortdistmin )
        continue;
#else
 #ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere Vertex %d\n", (int)vertexindex );
 #endif
#endif


#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "        Sphere Dist Plane %f ( best %f )\n", M3D_PlanePoint( distplane, vertex2 ), bestcpdist );
#endif

      /* Coplanar and behind segment? Drop that */
      cpdist = M3D_PlanePoint( distplane, vertex2 );
      if( cpdist < 0.0 )
        continue;

      edgecheck[0].v[0] = segment->edge.v[0];
      edgecheck[0].v[1] = segment->edge.v[1];
      if( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[0] ) )
        continue;

#ifdef DEBUG_VERBOSE_PIVOTMULTI
printf( "          Good Vertex %d\n", (int)vertexindex );
#endif

      /* Search for pending unattached segments */
      edgecheck[0].v[0] = vertexindex;
      edgecheck[0].v[1] = segment->edge.v[1];
      edgecheck[1].v[0] = segment->edge.v[0];
      edgecheck[1].v[1] = vertexindex;
      if( !( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[0] ) ) && !( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edgecheck[1] ) ) )
        continue;

      triangle->v[0] = segment->edge.v[0];
      triangle->v[1] = segment->edge.v[1];
      triangle->v[2] = vertexindex;
      trival = bpValidateTriangle( bpdata, tdata, patch, triangle, BP_EDGE_GRADE_LOW, hinge->flags );
      if( trival != BP_VALIDATE_TRIANGLE_FAILURE )
        goto validtriangle;
    }

    goto addballhinge;

/****/

    addballhinge:
    conflictdepth = BP_HINGE_GET_CONFLICTDEPTH( hinge->flags );
    if( conflictdepth )
      conflictdepth++;
    if( conflictdepth < BP_CONFLICTDEPTH_MAX )
    {
      newhingeflags = BP_HINGE_FLAGS_PIVOT | ( hinge->flags & BP_HINGE_FLAGS_ORPHAN );
      if( segment->flags & BP_HINGE_FLAGS_SPHEREPIVOT )
      {
        /* Add a regular hinge to patch for ball pivoting processing */
        newhingelevel = BP_PATCH_HINGE_LEVEL_NORMAL;
        newhingegrade = BP_EDGE_GRADE_LOW;
      }
      else
      {
        newhingelevel = BP_HINGE_GET_LEVEL( hinge ) + 1;
        newhingegrade = hinge->edge.grade;
        if( newhingelevel >= BP_PATCH_HINGE_LEVEL_JOIN )
        {
          newhingeflags |= BP_HINGE_FLAGS_JOIN;
          newhingegrade = BP_EDGE_GRADE_REPAIR;
          if( newhingelevel >= BP_PATCH_HINGE_LEVEL_REPAIR )
            newhingeflags |= BP_HINGE_FLAGS_FORCE;
        }
      }
      if( conflictdepth )
        newhingelevel = BP_PATCH_HINGE_LEVEL_CONFLICT;
      bpPatchAddHinge( tdata, patch, newhingelevel, segment->edge.v[0], segment->edge.v[1], segment->edge.v[2], spherecenter, segment->normal, newhingegrade, BP_HINGE_COMPUTE_FLAGS( newhingeflags, conflictdepth ), -1 );
    }
    else
    {
 #ifdef DEBUG_VERBOSE_WARNING
      printf( "WARNING : Conflict Depth Exceeded, Hinge %d,%d Discarded\n", (int)segment->edge.v[0], (int)segment->edge.v[1] );
 #endif
    }

    mmBlockRelease( &tdata->segmentblock, segment );
  }

  return;
}

#endif


static void bpBuildJoinSeedTriangle( bpMainData *bpdata, bpThreadData *tdata, bpHinge *hinge, bpPivotResult *result, bpMesh *mesh, bpPatch *patch )
{
  bpf *vertex0, *vertex1, *vertex2;
  bpf center[BP_AXIS_COUNT];
  bpTriangle *triangle;

  /* Will fail if another thread already added that triangle */
  triangle = bpMeshQueryTriangle( tdata, mesh );
  memcpy( triangle, &result->triangle, sizeof(bpTriangle) );
  if( bpValidateTriangle( bpdata, tdata, patch, triangle, BP_EDGE_GRADE_SEED, 0 ) != BP_VALIDATE_TRIANGLE_SUCCESS )
    return;

#ifdef DEBUG_VERBOSE
printf( "      Seed Triangle %d,%d,%d Validated\n", (int)result->triangle.v[0], (int)result->triangle.v[1], (int)result->triangle.v[2] );
#endif

  vertex0 = ADDRESS( bpdata->vertex, result->triangle.v[0] * bpdata->vertexstride );
  vertex1 = ADDRESS( bpdata->vertex, result->triangle.v[1] * bpdata->vertexstride );
  vertex2 = ADDRESS( bpdata->vertex, result->triangle.v[2] * bpdata->vertexstride );

  /* Add the triangle to the thread's mesh */
  center[BP_AXIS_X] = (1.0/3.0) * ( vertex0[BP_AXIS_X] + vertex1[BP_AXIS_X] + vertex2[BP_AXIS_X] );
  center[BP_AXIS_Y] = (1.0/3.0) * ( vertex0[BP_AXIS_Y] + vertex1[BP_AXIS_Y] + vertex2[BP_AXIS_Y] );
  center[BP_AXIS_Z] = (1.0/3.0) * ( vertex0[BP_AXIS_Z] + vertex1[BP_AXIS_Z] + vertex2[BP_AXIS_Z] );
  bpMeshAddTriangle( tdata, mesh );
  bpSpaceMapAddTriangle( &bpdata->spacemap, triangle, center );

  /* Register the seed triangle */
  bpMeshAddSeed( tdata, mesh, &result->triangle );

  /* Mark the vertices as used, forbid seed triangles on them */
  mmBitMapSet( &bpdata->vertexmap, result->triangle.v[0] );
  mmBitMapSet( &bpdata->vertexmap, result->triangle.v[1] );
  mmBitMapSet( &bpdata->vertexmap, result->triangle.v[2] );

  /* Add the triangle's edges as active hinges to the patch */
  bpPatchAddHinge( tdata, patch, BP_PATCH_HINGE_LEVEL_NORMAL, result->triangle.v[1], result->triangle.v[0], result->triangle.v[2], result->spherecenter, result->normal, BP_EDGE_GRADE_HIGH, BP_HINGE_COMPUTE_FLAGS( BP_HINGE_FLAGS_PIVOT, 0x0 ), -1 );
  bpPatchAddHinge( tdata, patch, BP_PATCH_HINGE_LEVEL_NORMAL, result->triangle.v[2], result->triangle.v[1], result->triangle.v[0], result->spherecenter, result->normal, BP_EDGE_GRADE_HIGH, BP_HINGE_COMPUTE_FLAGS( BP_HINGE_FLAGS_PIVOT, 0x0 ), -1 );
  bpPatchAddHinge( tdata, patch, BP_PATCH_HINGE_LEVEL_NORMAL, result->triangle.v[0], result->triangle.v[2], result->triangle.v[1], result->spherecenter, result->normal, BP_EDGE_GRADE_HIGH, BP_HINGE_COMPUTE_FLAGS( BP_HINGE_FLAGS_PIVOT, 0x0 ), -1 );

  return;
}



/********/



static void bpWorkGrowMesh( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh, bpPatch *patch, bpf ballradius, int hingelevel )
{
  int newflag;
  bpHinge hinge;
  bpPivotResult result;

  bpHitBufferInit( &result.hitbuffer );

#ifdef DEBUG_VERBOSE
printf( "GrowMesh : Ball Radius %f, Hinge Level %d\n", ballradius, hingelevel );
#endif

  /* Loop as long as there are untested vertices to seed */
  for( ; ; )
  {
    for( ; ; )
    {
#ifdef DEBUG_VERBOSE
printf( "  GrowMesh : Get Hinge\n" );
#endif

      /* Acquire first hinge from patch */
      if( !( bpPatchGetHinge( tdata, patch, &hinge, hingelevel ) ) )
      {
#ifdef DEBUG_VERBOSE
printf( "    Hinge List Empty\n" );
#endif
        break;
      }

#ifdef DEBUG_VERBOSE
printf( "    Hinge %d,%d (%d), grade 0x%x, flags 0x%x\n", (int)hinge.edge.v[0], (int)hinge.edge.v[1], (int)hinge.edge.v[2], hinge.edge.grade, hinge.flags );
#endif

      /* If hinge was already used to form a triangle, discard and try the next one */
      if( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashGrade, &hinge.edge ) )
      {
#ifdef DEBUG_VERBOSE
printf( "    Hinge Discard Duplicate\n" );
#endif
        continue;
      }

      bpPivot( bpdata, tdata, &hinge, &result, ballradius, mesh, patch );

#ifdef DEBUG_PROGRESS
if( !( mesh->trianglecount % 10000 ) )
  printf( "Thread %d ; Triangle Count : %d\n", (int)tdata->threadid, (int)mesh->trianglecount );
#endif

      /* Update thread's count of triangles */
      tdata->statustricount = mesh->trianglecount;
    }

#ifdef DEBUG_VERBOSE
printf( "  GrowMesh : Get Seed\n" );
#endif

    newflag = bpNewTriangleSeed( bpdata, tdata, &result );

#ifdef DEBUG_VERBOSE
if( newflag )
  printf( "    Seed True ; Triangle %d,%d,%d\n", (int)result.triangle.v[0], (int)result.triangle.v[1], (int)result.triangle.v[2] );
else
  printf( "    Seed False\n" );
#endif

    if( newflag )
    {
      /* Will fail if another thread already added that triangle */
      bpBuildJoinSeedTriangle( bpdata, tdata, &hinge, &result, mesh, patch );

      /* Release any spatial lock */
      if( result.spacelockflag )
        bpSpaceLockRelease( bpdata, &bpdata->spacemap, &result.spacelock );
    }
    else
    {
#ifdef DEBUG_VERBOSE
printf( "    Hinge Level %d Completed\n", hingelevel );
#endif
        break;
    }

#ifdef DEBUG_PROGRESS
printf( "Thread %d ; Add Seed, Triangle Count : %d\n", (int)tdata->threadid, (int)mesh->trianglecount );
#endif

  }

  bpHitBufferFree( &result.hitbuffer );

  return;
}



/********/



/* Strip bad edge triangles */

static int bpWorkMeshStripTriangle( bpMainData *bpdata, bpThreadData *tdata, bpEdge *triedge, int skipflags, int depth )
{
  int edgeflags, vertexflags, edgecount, goodcount, retvalue, resultgrade;
  bpEdge edge[3], linkedge;

  edge[0].v[0] = triedge->v[1];
  edge[0].v[1] = triedge->v[0];
  edge[1].v[0] = triedge->v[2];
  edge[1].v[1] = triedge->v[1];
  edge[2].v[0] = triedge->v[0];
  edge[2].v[1] = triedge->v[2];

  if( depth < 0 )
  {
#ifdef DEBUG_VERBOSE_STRIP
printf( "    Warning: Strip Triangle Depth Exceeded\n" );
#endif
    return 1;
  }

#ifdef DEBUG_VERBOSE_STRIP
printf( "    Test Triangle %d,%d,%d\n", (int)triedge->v[0], (int)triedge->v[1], (int)triedge->v[2] );
#endif

  retvalue = 1;
  resultgrade = BP_EDGE_GRADE_LINK;

  edgeflags = 0x0;
  vertexflags = 0x0;
  edgecount = 0;
  goodcount = 0;
  if( mmHashLockReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge[0] ) == MM_HASH_SUCCESS )
  {
    edgeflags |= 0x1;
    edgecount++;
    if( edge[0].grade > BP_EDGE_GRADE_BAD )
    {
      vertexflags |= 0x1 | 0x2;
      goodcount++;
    }
  }
  if( mmHashLockReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge[1] ) == MM_HASH_SUCCESS )
  {
    edgeflags |= 0x2;
    edgecount++;
    if( edge[1].grade > BP_EDGE_GRADE_BAD )
    {
      vertexflags |= 0x2 | 0x4;
      goodcount++;
    }
  }
  if( mmHashLockReadEntry( bpdata->hingehashtable, &bpHingeHashEdge, &edge[2] ) == MM_HASH_SUCCESS )
  {
    edgeflags |= 0x4;
    edgecount++;
    if( edge[2].grade > BP_EDGE_GRADE_BAD )
    {
      vertexflags |= 0x4 | 0x1;
      goodcount++;
    }
  }


#ifdef DEBUG_VERBOSE_STRIP
printf( "      Edgecount : %d, Goodcount : %d\n", edgecount, goodcount );
printf( "      Edgeflags : 0x%x, Vertexflags 0x%x\n", edgeflags, vertexflags );
#endif


  /* Lost bad triangle, just delete it */
  if( edgecount <= 1 )
  {
    retvalue = 0;
    resultgrade = BP_EDGE_GRADE_DELETED;
    goto done;
  }

  /* Solid inner link, promote all edges of triangle to LINK grade */
  if( goodcount >= 2 )
    goto done;

  /* Traverse in a fan over any bad vertex, seek any solid link */
  depth--;
  edgeflags &= ~skipflags;
  skipflags = 0x0;
  if( !( vertexflags & 0x1 ) )
  {
    if( edgeflags & 0x4 )
    {
      if( bpWorkMeshStripTriangle( bpdata, tdata, &edge[2], 0x1, depth ) )
        goto done;
    }
    if( edgeflags & 0x1 )
    {
      if( bpWorkMeshStripTriangle( bpdata, tdata, &edge[0], 0x1, depth ) )
        goto done;
    }
    edgeflags &= ~( 0x4 | 0x1 );
  }
  if( !( vertexflags & 0x2 ) )
  {
    if( edgeflags & 0x1 )
    {
      if( bpWorkMeshStripTriangle( bpdata, tdata, &edge[0], 0x1, depth ) )
        goto done;
    }
    if( edgeflags & 0x2 )
    {
      if( bpWorkMeshStripTriangle( bpdata, tdata, &edge[1], 0x1, depth ) )
        goto done;
    }
    edgeflags &= ~( 0x1 | 0x2 );
  }
  if( !( vertexflags & 0x4 ) )
  {
    if( edgeflags & 0x2 )
    {
      if( bpWorkMeshStripTriangle( bpdata, tdata, &edge[1], 0x1, depth ) )
        goto done;
    }
    if( edgeflags & 0x4 )
    {
      if( bpWorkMeshStripTriangle( bpdata, tdata, &edge[2], 0x1, depth ) )
        goto done;
    }
    edgeflags &= ~( 0x2 | 0x4 );
  }

  retvalue = 0;
  resultgrade = BP_EDGE_GRADE_DELETED;

  done:

  triedge->grade = resultgrade;
  linkedge.grade = resultgrade;
  if( mmHashLockReplaceEntry( bpdata->hingehashtable, &bpHingeHashEdge, triedge, 0 ) != MM_HASH_SUCCESS )
    printf( "NOT SUPPOSED TO HAPPEN!! %s:%d\n", __FILE__, __LINE__ );
  linkedge.v[0] = triedge->v[1];
  linkedge.v[1] = triedge->v[2];
  linkedge.v[2] = triedge->v[0];
  if( mmHashLockReplaceEntry( bpdata->hingehashtable, &bpHingeHashEdge, &linkedge, 0 ) != MM_HASH_SUCCESS )
    printf( "NOT SUPPOSED TO HAPPEN!! %s:%d\n", __FILE__, __LINE__ );
  linkedge.v[0] = triedge->v[2];
  linkedge.v[1] = triedge->v[0];
  linkedge.v[2] = triedge->v[1];
  if( mmHashLockReplaceEntry( bpdata->hingehashtable, &bpHingeHashEdge, &linkedge, 0 ) != MM_HASH_SUCCESS )
    printf( "NOT SUPPOSED TO HAPPEN!! %s:%d\n", __FILE__, __LINE__ );

  return retvalue;
}


static void bpWorkMeshStrip( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh )
{
  size_t index, trianglecount;
  bpTriangle *triangle;
  bpTriangleBundle *bundle;
  bpEdge triedge;

#ifdef DEBUG_VERBOSE
printf( "Mesh Strip\n" );
#endif

  trianglecount = 0;
  for( bundle = mesh->trianglelist ; bundle ; bundle = bundle->list.next )
  {
    triangle = bundle->triangle;
    trianglecount += bundle->count;
    for( index = bundle->count ; index ; index--, triangle++ )
    {
      /* If the triangle is not graded BAD, skip */
      if( triangle->v[0] == -1 )
      {
        trianglecount--;
        continue;
      }
      triedge.v[0] = triangle->v[0];
      triedge.v[1] = triangle->v[1];
      triedge.v[2] = triangle->v[2];
      triedge.grade = BP_EDGE_GRADE_BAD;
      if( !( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashStrip, &triedge ) ) )
        continue;

#ifdef DEBUG_VERBOSE_STRIP
printf( "  Strip %d,%d,%d\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif

      if( !( bpWorkMeshStripTriangle( bpdata, tdata, &triedge, 0x0, 32 ) ) )
      {
#ifdef DEBUG_VERBOSE_STRIP
printf( "    Bad Triangle Discarded %d,%d,%d\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif
        triangle->v[0] = -1;
        trianglecount--;
      }
#ifdef DEBUG_VERBOSE_STRIP
      else
      {
printf( "    Bad Triangle Promoted to Link %d,%d,%d\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
      }
#endif

    }
  }

  mesh->trianglecount = trianglecount;
  tdata->statustricount = trianglecount;

  return;
}



/********/



static void bpWorkMeshCount( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh )
{
  size_t index, trianglecount;
  bpTriangle *triangle;
  bpTriangleBundle *bundle;

#ifdef DEBUG_VERBOSE
printf( "Mesh Count\n" );
#endif

  trianglecount = 0;
  for( bundle = mesh->trianglelist ; bundle ; bundle = bundle->list.next )
  {
    triangle = bundle->triangle;
    trianglecount += bundle->count;
    for( index = bundle->count ; index ; index--, triangle++ )
    {
      if( triangle->v[0] == -1 )
        trianglecount--;
    }
  }

  mesh->trianglecount = trianglecount;
  tdata->statustricount = trianglecount;

  return;
}



/********/



#define BP_PATH_PER_BUNDLE (256)

typedef struct
{
  int writeindex;
  int readindex;
  bpEdge edge[BP_PATH_PER_BUNDLE];
  mmListNode list;
} bpPathBundle;

typedef struct
{
  void *pathlist;
} bpPath;



static void bpPathInit( bpThreadData *tdata, bpPath *path )
{
  path->pathlist = 0;
  return;
}

static void bpPathFree( bpThreadData *tdata, bpPath *path )
{
  bpPathBundle *bundle, *bundlenext;

  for( bundle = path->pathlist ; bundle ; bundle = bundlenext )
  {
    bundlenext = bundle->list.next;
    mmBlockFree( &tdata->pathblock, bundle );
  }
  path->pathlist = 0;

  return;
}

static int bpPathGetEdge( bpThreadData *tdata, bpPath *path, bpEdge *retedge )
{
  bpPathBundle *bundle;

  bundle = path->pathlist;
  if( !( bundle ) )
    return 0;
  *retedge = bundle->edge[bundle->readindex];
  bundle->readindex++;
  if( bundle->readindex == bundle->writeindex )
  {
    mmListRemove( bundle, offsetof(bpPathBundle,list) );
    mmBlockFree( &tdata->pathblock, bundle );
  }

  return 1;
}

static void bpPathAddEdge( bpThreadData *tdata, bpPath *path, bpi v0, bpi v1 )
{
  bpPathBundle *bundle;
  bpEdge *edge;

  bundle = path->pathlist;
  if( !( bundle ) || ( bundle->writeindex >= BP_PATH_PER_BUNDLE ) )
  {
    bundle = mmBlockAlloc( &tdata->pathblock );
    bundle->writeindex = 0;
    bundle->readindex = 0;
    mmListAdd( &path->pathlist, bundle, offsetof(bpPathBundle,list) );
  }

  edge = &bundle->edge[bundle->writeindex];
  edge->v[0] = v0;
  edge->v[1] = v1;
  bundle->writeindex++;

  return;
}



typedef struct
{
  bpPath *path;
  int pathflag;
  int v2;
} bpMeshConnectData;

static void bpMeshConnectCallback( void *opaque, void *entry, int newflag )
{
  bpMeshConnectData *cdata;
  bpEdge *edge;

  cdata = opaque;
  edge = entry;

  /* Already connected */
  if( edge->grade == BP_EDGE_GRADE_CONNECTED )
    return;
  if( edge->grade == BP_EDGE_GRADE_DELETED )
    return;

  if( edge->grade >= BP_EDGE_GRADE_PATHMIN )
    cdata->pathflag = 1;

#ifdef DEBUG_VERBOSE_CONNECT
printf( "    Path %d,%d (%d) ; Grade 0x%x ; Pathflag %d\n", (int)edge->v[0], (int)edge->v[1], (int)edge->v[2], edge->grade, cdata->pathflag );
#endif

  edge->grade = BP_EDGE_GRADE_CONNECTED;
  cdata->v2 = edge->v[2];

  return;
}

static void bpMeshConnectLinkCallback( void *opaque, void *entry, int newflag )
{
  bpEdge *edge;
  edge = entry;
  edge->grade = BP_EDGE_GRADE_CONNECTED;

#ifdef DEBUG_VERBOSE_CONNECT
printf( "      Link %d,%d (%d)\n", (int)edge->v[0], (int)edge->v[1], (int)edge->v[2] );
#endif

  return;
}


static void bpMeshConnectPath( bpMainData *bpdata, bpThreadData *tdata, bpPath *path, bpEdge *pathedge )
{
  bpMeshConnectData cdata;
  bpEdge linkedge;

  cdata.path = path;
  cdata.pathflag = 0;
  cdata.v2 = -1;
  mmHashLockCallEntry( bpdata->hingehashtable, &bpHingeHashEdge, pathedge, bpMeshConnectCallback, &cdata, 0 );

  if( cdata.v2 != -1 )
  {
    linkedge.v[0] = pathedge->v[1];
    linkedge.v[1] = cdata.v2;
    mmHashLockCallEntry( bpdata->hingehashtable, &bpHingeHashEdge, &linkedge, bpMeshConnectLinkCallback, 0, 0 );
    linkedge.v[0] = cdata.v2;
    linkedge.v[1] = pathedge->v[0];
    mmHashLockCallEntry( bpdata->hingehashtable, &bpHingeHashEdge, &linkedge, bpMeshConnectLinkCallback, 0, 0 );
    if( cdata.pathflag )
    {

#ifdef DEBUG_VERBOSE_CONNECT
printf( "    Path Add %d,%d\n", (int)cdata.v2, (int)pathedge->v[1] );
printf( "    Path Add %d,%d\n", (int)pathedge->v[0], (int)cdata.v2 );
#endif

      bpPathAddEdge( tdata, path, cdata.v2, pathedge->v[1] );
      bpPathAddEdge( tdata, path, pathedge->v[0], cdata.v2 );
    }
  }

  return;
}

static void bpWorkMeshConnect( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh )
{
  bpTriangleSeed *seed;
  bpPath path;
  bpEdge pathedge;

  bpPathInit( tdata, &path );

#ifdef DEBUG_VERBOSE
printf( "Mesh Connect\n" );
#endif

  /* Build path from initial seeds */
  for( seed = mesh->seedlist ; seed ; seed = seed->list.next )
  {
#ifdef DEBUG_VERBOSE_CONNECT
printf( "  Connect Seed %d,%d (%d)\n", (int)seed->v[0], (int)seed->v[1], (int)seed->v[2] );
#endif

    bpPathAddEdge( tdata, &path, seed->v[0], seed->v[1] );
  }

  /* Propagate connectivity from seeds */
  for( ; ; )
  {
    if( !( bpPathGetEdge( tdata, &path, &pathedge ) ) )
      break;

#ifdef DEBUG_VERBOSE_CONNECT
printf( "  Connect Path %d,%d\n", (int)pathedge.v[0], (int)pathedge.v[1] );
#endif

    bpMeshConnectPath( bpdata, tdata, &path, &pathedge );
  }

  bpPathFree( tdata, &path );

  return;
}



/********/



static void bpWorkCleanMesh( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh )
{
  int bestaxis, bestindex;
  size_t index;
  bpTriangle *triangle;
  bpTriangleBundle *bundle;
  bpEdge edge;

#ifdef DEBUG_VERBOSE
printf( "Mesh Clean\n" );
#endif

  edge.grade = BP_EDGE_GRADE_DELETED+1;
  if( !( bpdata->operationflags & BP_FLAGS_SKIP_CONNECT_PASS ) )
    edge.grade = BP_EDGE_GRADE_CONNECTED;

  for( bundle = mesh->trianglelist ; bundle ; bundle = bundle->list.next )
  {
    triangle = bundle->triangle;
    for( index = bundle->count ; index ; index--, triangle++ )
    {
      if( triangle->v[0] == -1 )
        continue;

      /* To ensure proper detection of duplicate triangles, check from the lowest indexed vertex */
      bestaxis = 0;
      bestindex = triangle->v[0];
      if( triangle->v[1] < bestindex )
      {
        bestaxis = 1;
        bestindex = triangle->v[1];
      }
      if( triangle->v[2] < bestindex )
      {
        bestaxis = 2;
        bestindex = triangle->v[2];
      }

      edge.v[0] = triangle->v[bestaxis];
      edge.v[1] = triangle->v[(bestaxis+1)%3];
      edge.v[2] = triangle->v[(bestaxis+2)%3];

#ifdef DEBUG_VERBOSE
printf( "  Test Triangle %d,%d,%d\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
#endif
      if( !( mmHashLockFindEntry( bpdata->hingehashtable, &bpHingeHashClean, &edge ) ) )
      {
#ifdef DEBUG_VERBOSE
printf( "  Discard Triangle %d,%d,%d\n", (int)triangle->v[0], (int)triangle->v[1], (int)triangle->v[2] );
printf( "    Hinge %d,%d (%d) Not Found\n", (int)edge.v[0], (int)edge.v[1], (int)edge.v[2] );
#endif
        triangle->v[0] = -1;
        mesh->trianglecount--;
      }
    }
  }

  tdata->statustricount = mesh->trianglecount;

  return;
}



/********/



static void bpStoreIndices8( void *dst, bpi *indices )
{
  uint8_t *dst8;
  dst8 = dst;
  dst8[0] = indices[0];
  dst8[1] = indices[1];
  dst8[2] = indices[2];
  return;
}

static void bpStoreIndices16( void *dst, bpi *indices )
{
  uint16_t *dst16;
  dst16 = dst;
  dst16[0] = indices[0];
  dst16[1] = indices[1];
  dst16[2] = indices[2];
  return;
}

static void bpStoreIndices32( void *dst, bpi *indices )
{
  uint32_t *dst32;
  dst32 = dst;
  dst32[0] = indices[0];
  dst32[1] = indices[1];
  dst32[2] = indices[2];
  return;
}

static void bpStoreIndices64( void *dst, bpi *indices )
{
  uint64_t *dst64;
  dst64 = dst;
  dst64[0] = indices[0];
  dst64[1] = indices[1];
  dst64[2] = indices[2];
  return;
}


static void bpStoreSwapIndices8( void *dst, bpi *indices )
{
  uint8_t *dst8;
  dst8 = dst;
  dst8[0] = indices[2];
  dst8[1] = indices[1];
  dst8[2] = indices[0];
  return;
}

static void bpStoreSwapIndices16( void *dst, bpi *indices )
{
  uint16_t *dst16;
  dst16 = dst;
  dst16[0] = indices[2];
  dst16[1] = indices[1];
  dst16[2] = indices[0];
  return;
}

static void bpStoreSwapIndices32( void *dst, bpi *indices )
{
  uint32_t *dst32;
  dst32 = dst;
  dst32[0] = indices[2];
  dst32[1] = indices[1];
  dst32[2] = indices[0];
  return;
}

static void bpStoreSwapIndices64( void *dst, bpi *indices )
{
  uint64_t *dst64;
  dst64 = dst;
  dst64[0] = indices[2];
  dst64[1] = indices[1];
  dst64[2] = indices[0];
  return;
}


/* Store a thread's buffered mesh data within the final mesh allocated by main thread */
static void bpWorkStoreMesh( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh )
{
  size_t index, stride;
  bpi v[3];
  void *store;
  bpTriangle *triangle;
  bpTriangleBundle *bundle;
  bpi *redirectiontable;
  void (*storeindices)( void *dst, bpi *indices );

  storeindices = 0;
  if( bpdata->trianglewinding == BP_TRIANGLE_WINDING_CW )
  {
    switch( tdata->triangleindexsize )
    {
      case 1:
        storeindices = bpStoreIndices8;
        break;
      case 2:
        storeindices = bpStoreIndices16;
        break;
      case 4:
        storeindices = bpStoreIndices32;
        break;
      case 8:
        storeindices = bpStoreIndices64;
        break;
    }
  }
  else if( bpdata->trianglewinding == BP_TRIANGLE_WINDING_CCW )
  {
    switch( tdata->triangleindexsize )
    {
      case 1:
        storeindices = bpStoreSwapIndices8;
        break;
      case 2:
        storeindices = bpStoreSwapIndices16;
        break;
      case 4:
        storeindices = bpStoreSwapIndices32;
        break;
      case 8:
        storeindices = bpStoreSwapIndices64;
        break;
    }
  }
  if( !( storeindices ) )
    return;

  store = tdata->trianglestore;
  stride = bpdata->trianglestride;
  redirectiontable = bpdata->redirectiontable;
  if( redirectiontable )
  {
    for( bundle = mesh->trianglelist ; bundle ; bundle = bundle->list.next )
    {
      triangle = bundle->triangle;
      for( index = bundle->count ; index ; index--, triangle++ )
      {
        if( triangle->v[0] == -1 )
          continue;
        v[0] = redirectiontable[ triangle->v[0] ];
        v[1] = redirectiontable[ triangle->v[1] ];
        v[2] = redirectiontable[ triangle->v[2] ];
        storeindices( store, v );
        store = ADDRESS( store, stride );
      }
    }
  }
  else
  {
    for( bundle = mesh->trianglelist ; bundle ; bundle = bundle->list.next )
    {
      triangle = bundle->triangle;
      for( index = bundle->count ; index ; index--, triangle++ )
      {
        if( triangle->v[0] == -1 )
          continue;
        storeindices( store, triangle->v );
        store = ADDRESS( store, stride );
      }
    }
  }

  return;
}


/****/


static void bpMeshSmoothClear( bpMainData *bpdata, bpThreadData *tdata )
{
  size_t base, count;
  bpf *normal;

  count = bpdata->vertexcount / bpdata->threadcount;
  base = tdata->threadid * count;
  if( tdata->threadid == bpdata->threadcount-1 )
    count = bpdata->vertexcount - base;

  normal = ADDRESS( bpdata->normal, base * bpdata->normalstride );
  for( ; count ; count-- )
  {
    normal[0] = 0.0;
    normal[1] = 0.0;
    normal[2] = 0.0;
    normal = ADDRESS( normal, bpdata->normalstride );
  }

  return;
}


static void bpMeshSmoothAccumulate( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh )
{
  size_t index;
  long lockindex;
  bpTriangle *triangle;
  bpTriangleBundle *bundle;
  bpPointLockMap *plm;
  bpf *vertex0, *vertex1, *vertex2;
  bpf *normal0, *normal1, *normal2;
  bpf vecta[BP_AXIS_COUNT], vectb[BP_AXIS_COUNT], normal[BP_AXIS_COUNT];

#ifdef DEBUG_VERBOSE
printf( "Mesh Normal Smooth\n" );
#endif

  plm = &bpdata->pointlockmap;
  lockindex = BP_POINT_LOCK_INDEX_NONE;

  for( bundle = mesh->trianglelist ; bundle ; bundle = bundle->list.next )
  {
    triangle = bundle->triangle;
    for( index = bundle->count ; index ; index--, triangle++ )
    {
      if( triangle->v[0] == -1 )
        continue;
      vertex0 = ADDRESS( bpdata->vertex, triangle->v[0] * bpdata->vertexstride );
      vertex1 = ADDRESS( bpdata->vertex, triangle->v[1] * bpdata->vertexstride );
      vertex2 = ADDRESS( bpdata->vertex, triangle->v[2] * bpdata->vertexstride );

      normal0 = ADDRESS( bpdata->normal, triangle->v[0] * bpdata->normalstride );
      normal1 = ADDRESS( bpdata->normal, triangle->v[1] * bpdata->normalstride );
      normal2 = ADDRESS( bpdata->normal, triangle->v[2] * bpdata->normalstride );

      /* Compute normal of triangle */
      M3D_VectorSubStore( vecta, vertex1, vertex0 );
      M3D_VectorSubStore( vectb, vertex2, vertex0 );
      M3D_VectorCrossProduct( normal, vectb, vecta );
      M3D_VectorNormalize( normal );

      /* Add the normal to each vertex. TODO: Proportional to triangle angle at point? */

      bpPointLockAcquire( bpdata, plm, &lockindex, vertex0 );
      M3D_VectorAdd( normal0, normal );
      bpPointLockAcquire( bpdata, plm, &lockindex, vertex1 );
      M3D_VectorAdd( normal1, normal );
      bpPointLockAcquire( bpdata, plm, &lockindex, vertex2 );
      M3D_VectorAdd( normal2, normal );
    }
  }

  /* Release lock */
  bpPointLockRelease( bpdata, plm, &lockindex );

  return;
}


static void bpMeshSmoothNormalize( bpMainData *bpdata, bpThreadData *tdata )
{
  size_t base, count;
  bpf f;
  bpf *normal;

  count = bpdata->vertexcount / bpdata->threadcount;
  base = tdata->threadid * count;
  if( tdata->threadid == bpdata->threadcount-1 )
    count = bpdata->vertexcount - base;

  normal = ADDRESS( bpdata->normal, base * bpdata->normalstride );
  for( ; count ; count-- )
  {
    f = M3D_VectorMagnitude( normal );
    if( f > 0.0 )
    {
      f = 1.0 / f;
      normal[BP_AXIS_X] *= f;
      normal[BP_AXIS_Y] *= f;
      normal[BP_AXIS_Z] *= f;
    }
    normal = ADDRESS( normal, bpdata->normalstride );
  }

  return;
}



/********/



static void bpMeshFlagVertexUse( bpMainData *bpdata, bpThreadData *tdata, bpMesh *mesh )
{
  size_t index;
  bpTriangle *triangle;
  bpTriangleBundle *bundle;

#ifdef DEBUG_VERBOSE
printf( "Mesh Pack Flag\n" );
#endif

  for( bundle = mesh->trianglelist ; bundle ; bundle = bundle->list.next )
  {
    triangle = bundle->triangle;
    for( index = bundle->count ; index ; index--, triangle++ )
    {
      if( triangle->v[0] == -1 )
        continue;
      mmBitMapSet( &bpdata->vertexmap, triangle->v[0] );
      mmBitMapSet( &bpdata->vertexmap, triangle->v[1] );
      mmBitMapSet( &bpdata->vertexmap, triangle->v[2] );
    }
  }

  return;
}



/********/



#define BP_THREAD_CLUSTER_PREALLOC (4096)


typedef struct
{
  int threadid;
  void *bpdata;
  int sortbase, sortmax;
  bpi seedvertexbase;
  bpi seedvertexindex;
  void *tdata;
  int stage;
} bpThreadInit;



static void *bpThreadMain( void *value )
{
  int ballindex, nodeindex;
  bpThreadData tdata;
  bpThreadInit *tinit;
  bpMainData *bpdata;
  bpMesh mesh;
  bpPatch patch;

  tinit = value;
  tinit->tdata = &tdata;
  bpdata = tinit->bpdata;
  tdata.threadid = tinit->threadid;
  tdata.flags = 0;
  tdata.seedvertexbase = tinit->seedvertexbase;
  tdata.seedvertexindex = tinit->seedvertexindex;
  tdata.statustricount = 0;
  memset( &tdata.info, 0, sizeof(bpInfo) );

  nodeindex = -1;
  if( mmcontext.numaflag )
  {
    mmThreadBindToCpu( tdata.threadid );
    nodeindex = mmCpuGetNode( tdata.threadid );
    mmBlockNodeInit( &tdata.cellblock, nodeindex, sizeof(bpGridCell), 4096, 4096, 0x10 );
    mmBlockNodeInit( &tdata.hingeblock, nodeindex, sizeof(bpHingeBundle), 512, 512, 0x10 );
    mmBlockNodeInit( &tdata.triangleblock, nodeindex, sizeof(bpTriangleBundle), 64, 64, 0x10 );
    mmBlockNodeInit( &tdata.pathblock, nodeindex, sizeof(bpPathBundle), 512, 512, 0x10 );
  }
  else
  {
    mmBlockInit( &tdata.cellblock, sizeof(bpGridCell), 4096, 4096, 0x10 );
    mmBlockInit( &tdata.hingeblock, sizeof(bpHingeBundle), 512, 512, 0x10 );
    mmBlockInit( &tdata.triangleblock, sizeof(bpTriangleBundle), 64, 64, 0x10 );
    mmBlockInit( &tdata.pathblock, sizeof(bpPathBundle), 512, 512, 0x10 );
  }

  /* Initialize random number generator */
  ccQuickRand32Seed( &tdata.randstate, tdata.threadid );

  /* Initialize the thread's cluster vertex buffer */
  bpClusterInit( &tdata.cluster, BP_THREAD_CLUSTER_PREALLOC );

  /* Wait until all threads have properly initialized */
  if( bpdata->updatestatusflag )
    bpBarrierSync( &bpdata->globalbarrier );

  /**** STEP 0 : THREADS SORT VERTICES ****/
#ifdef DEBUG_VERBOSE
printf( "Thread %d : GridSort %d to %d\n", (int)tdata.threadid, (int)tinit->sortbase, (int)tinit->sortmax );
#endif
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_SORT;
  for( ; ; )
  {
    /* Work : sort vertices spatially in a hierarchical grid */
    tdata.activevertexcount = bpWorkGridSort( bpdata, &tdata, tinit->sortbase, tinit->sortmax );
#ifdef DEBUG_VERBOSE
printf( "Thread %d : GridSort Done\n", (int)tdata.threadid );
#endif
    /* Wake main thread to produce a verdict regarding the count of active vertices */
    bpBarrierSync( &bpdata->globalbarrier );
    /* Wait for the verdict */
    bpBarrierSync( &bpdata->globalbarrier );
    if( bpdata->syncbuildflag )
      break;
    mmBlockFreeAll( &tdata.cellblock );
    if( nodeindex >= 0 )
      mmBlockNodeInit( &tdata.cellblock, nodeindex, sizeof(bpGridCell), 4096, 4096, 0x10 );
    else
      mmBlockInit( &tdata.cellblock, sizeof(bpGridCell), 4096, 4096, 0x10 );
  }
  /**** STEP 0 : THREADS SORT VERTICES ****/




  /**** STEP 1 : THREADS GROW MESHES ****/
#ifdef DEBUG_VERBOSE
printf( "Thread %d : GrowMesh\n", (int)tdata.threadid );
#endif
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_MESH;
  bpMeshInit( &tdata, &mesh );
  bpPatchInit( &tdata, &patch );
  for( ballindex = 0 ; ballindex < bpdata->ballcount ; ballindex++ )
  {
    bpWorkGrowMesh( bpdata, &tdata, &mesh, &patch, bpdata->ballradius[ballindex], BP_PATCH_HINGE_LEVEL_NORMAL );
    bpBarrierSync( &bpdata->workbarrier );
    bpWorkGrowMesh( bpdata, &tdata, &mesh, &patch, bpdata->ballradius[ballindex], BP_PATCH_HINGE_LEVEL_JOIN );
    bpBarrierSync( &bpdata->workbarrier );
    bpWorkGrowMesh( bpdata, &tdata, &mesh, &patch, bpdata->ballradius[ballindex], BP_PATCH_HINGE_LEVEL_REPAIR );
    bpBarrierSync( &bpdata->workbarrier );

    bpWorkGrowMesh( bpdata, &tdata, &mesh, &patch, bpdata->ballradius[ballindex], BP_PATCH_HINGE_LEVEL_CONFLICT );
    bpBarrierSync( &bpdata->workbarrier );

    bpPatchNextBatch( &tdata, &patch );
  }
  bpPatchFree( &tdata, &patch );
#ifdef DEBUG_VERBOSE
printf( "Thread %d : Final Mesh Size : %d triangles\n", (int)tdata.threadid, mesh.trianglecount );
#endif
  /**** STEP 1 : THREADS GROW MESHES ****/



  /**** STEP 2 : THREADS STRIP POORLY CONNECTED BAD TRIANGLES ****/
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_STRIP;
  if( !( bpdata->operationflags & BP_FLAGS_SKIP_STRIP_PASS ) )
    bpWorkMeshStrip( bpdata, &tdata, &mesh );
  else
    bpWorkMeshCount( bpdata, &tdata, &mesh );
  bpBarrierSync( &bpdata->workbarrier );
  /**** STEP 2 : THREADS STRIP POORLY CONNECTED BAD TRIANGLES ****/



  /**** STEP 3 : THREADS PROCESS HINGE CONNECTIVITY OF THEIR SEEDS ****/
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_CONNECT;
  if( !( bpdata->operationflags & BP_FLAGS_SKIP_CONNECT_PASS ) )
  {
    bpWorkMeshConnect( bpdata, &tdata, &mesh );
    bpBarrierSync( &bpdata->workbarrier );
  }
  /**** STEP 3 : THREADS PROCESS HINGE CONNECTIVITY OF THEIR SEEDS ****/



  /**** STEP 4 : THREADS CLEAN THEIR MESHES ****/
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_CLEAN;
  if( !( bpdata->operationflags & BP_FLAGS_SKIP_CLEAN_PASS ) )
    bpWorkCleanMesh( bpdata, &tdata, &mesh );
  tdata.trianglecount = mesh.trianglecount;
  /**** STEP 4 : THREADS CLEAN THEIR MESHES ****/



  /**** STEP 4a : OPTIONAL, RECOMPUTE NORMALS ****/
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_NORMALS;
  if( bpdata->operationflags & BP_FLAGS_RECOMPUTE_NORMALS )
  {
    bpMeshSmoothClear( bpdata, &tdata );
    bpBarrierSync( &bpdata->workbarrier );
    bpMeshSmoothAccumulate( bpdata, &tdata, &mesh );
    bpBarrierSync( &bpdata->workbarrier );
    bpMeshSmoothNormalize( bpdata, &tdata );
  }
  /**** STEP 4a : OPTIONAL, RECOMPUTE NORMALS ****/



  /**** STEP 4b : OPTIONAL, FLAG USED VERTICES TO PACK THEM ****/
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_PACK;
  if( bpdata->operationflags & BP_FLAGS_PACK_VERTICES )
  {
    /* Reset the vertex bitmap */
    if( tdata.threadid == 0 )
      mmBitMapReset( &bpdata->vertexmap, 0x0 );
    bpBarrierSync( &bpdata->workbarrier );
    /* Have each thread flag its vertices */
    bpMeshFlagVertexUse( bpdata, &tdata, &mesh );
  }
  /**** STEP 4b : OPTIONAL, FLAG USED VERTICES TO PACK THEM ****/


  /* If all threads are done working on their mesh, wake up main thread */
  mtMutexLock( &bpdata->syncmutex );
  if( !( --bpdata->syncmeshworkcount ) )
    mtSignalWake( &bpdata->syncsignal );
  mtMutexUnlock( &bpdata->syncmutex );
  /* Global barrier */
  bpBarrierSync( &bpdata->globalbarrier );



  /**** STEP 5 : THREADS WRITE THEIR MESHES AS ONE GLOBAL MESH ****/
  if( !( tdata.threadid ) )
    tinit->stage = BP_STATUS_STAGE_STORE;
  if( bpdata->syncstoreflag )
    bpWorkStoreMesh( bpdata, &tdata, &mesh );
  /**** STEP 5 : THREADS WRITE THEIR MESHES AS ONE GLOBAL MESH ****/



  /* Wait for all threads to be done */
  bpBarrierSync( &bpdata->workbarrier );

  /* Free thread's data */
  bpClusterFree( &tdata.cluster );
  bpMeshFree( &tdata, &mesh );
  mmBlockFreeAll( &tdata.cellblock );
  mmBlockFreeAll( &tdata.hingeblock );
  mmBlockFreeAll( &tdata.triangleblock );
  mmBlockFreeAll( &tdata.pathblock );

  /* Wait for main thread to be done summing per-thread results before exiting */
  bpBarrierSync( &bpdata->globalbarrier );
  mtThreadExit();
  return 0;
}



/********/



static void bpFindLimits( bpf *limit, bpf *vertex, size_t vertexcount, size_t vertexstride )
{
  limit[0] = FLT_MAX;
  limit[1] = -FLT_MAX;
  limit[2] = FLT_MAX;
  limit[3] = -FLT_MAX;
  limit[4] = FLT_MAX;
  limit[5] = -FLT_MAX;
  for( ; vertexcount ; vertexcount--, vertex = ADDRESS( vertex, vertexstride ) )
  {
    limit[0] = bpfmin( limit[0], vertex[BP_AXIS_X] );
    limit[1] = bpfmax( limit[1], vertex[BP_AXIS_X] );
    limit[2] = bpfmin( limit[2], vertex[BP_AXIS_Y] );
    limit[3] = bpfmax( limit[3], vertex[BP_AXIS_Y] );
    limit[4] = bpfmin( limit[4], vertex[BP_AXIS_Z] );
    limit[5] = bpfmax( limit[5], vertex[BP_AXIS_Z] );
  }
  return;
}


static void bpGridSizePick( int *gridsize, size_t vertexcount, bpf *limit, bpf ballradius )
{
  bpf sizef[BP_AXIS_COUNT], cellcount, cellcountmin, cellcountmax, factor;

  factor = BP_GRID_SIZE_FACTOR / ballradius;
  sizef[BP_AXIS_X] = ( limit[1] - limit[0] ) * factor;
  sizef[BP_AXIS_Y] = ( limit[3] - limit[2] ) * factor;
  sizef[BP_AXIS_Z] = ( limit[5] - limit[4] ) * factor;

  cellcount = sizef[BP_AXIS_X] * sizef[BP_AXIS_Y] * sizef[BP_AXIS_Z];
  cellcountmax = vertexcount / BP_GRID_VERTEXPERCELL_MIN;
  cellcountmin = vertexcount / BP_GRID_VERTEXPERCELL_MAX;

#ifdef DEBUG_VERBOSE
printf( "Main Grid Cell Count Estimate : %f ( min %f ; max %f )\n", cellcount, cellcountmin, cellcountmax );
#endif

  factor = 1.0;
  if( cellcount < cellcountmin )
    factor = bpfcbrt( cellcountmin / cellcount );
  else if( cellcount > cellcountmax )
    factor = bpfcbrt( cellcountmax / cellcount );

  gridsize[BP_AXIS_X] = (int)bpfceil( sizef[BP_AXIS_X] * factor );
  gridsize[BP_AXIS_Y] = (int)bpfceil( sizef[BP_AXIS_Y] * factor );
  gridsize[BP_AXIS_Z] = (int)bpfceil( sizef[BP_AXIS_Z] * factor );
  if( gridsize[BP_AXIS_X] < 2 )
    gridsize[BP_AXIS_X] = 2;
  if( gridsize[BP_AXIS_Y] < 2 )
    gridsize[BP_AXIS_Y] = 2;
  if( gridsize[BP_AXIS_Z] < 2 )
    gridsize[BP_AXIS_Z] = 2;

#ifdef DEBUG_VERBOSE
printf( "Main Grid Final Cell Count : %d ( %d x %d x %d )\n", (int)( gridsize[0] * gridsize[1] * gridsize[2] ), (int)gridsize[0], (int)gridsize[1], (int)gridsize[2] );
#endif

  return;
}


static size_t bpBuildRedirectionTable( bpMainData *bpdata )
{
  bpi vertexindex, vertexcount;
  vertexcount = 0;
  for( vertexindex = 0 ; vertexindex < bpdata->vertexcount ; vertexindex++ )
  {
    if( !( mmBitMapGet( &bpdata->vertexmap, vertexindex ) ) )
      continue;
    bpdata->packcallback( bpdata->packopaquepointer, vertexcount, vertexindex );
    bpdata->redirectiontable[vertexindex] = vertexcount++;
  }
  return vertexcount;
}


/********/



static void bpInitThreadData( bpMainData *bpdata, bpThreadInit *tinit, int threadcount, long randseed )
{
  int threadindex;
  size_t countperthread;
  size_t countindex;

#ifdef DEBUG_VERBOSE
printf( "RandSeed : %ld\n", (long)randseed );
#endif

#if CPUCONF_LONG_SIZE == 8
  randseed = ccHash32Int64Inline( randseed );
#else
  randseed = ccHash32Int32Inline( randseed );
#endif

  /* Count of vertex to sort per thread */
  countindex = 0;
  countperthread = ( bpdata->vertexcount / threadcount ) + 1;
  for( threadindex = 0 ; threadindex < threadcount ; threadindex++, tinit++ )
  {
    tinit->sortbase = countindex;
    countindex += countperthread;
    tinit->sortmax = countindex;
    if( tinit->sortmax > bpdata->vertexcount )
      tinit->sortmax = bpdata->vertexcount;
    tinit->seedvertexbase = ( countindex + randseed ) % bpdata->vertexcount;
    tinit->seedvertexindex = tinit->seedvertexbase;
    tinit->tdata = 0;

#ifdef DEBUG_VERBOSE
printf( "Thread %d : Seed Base %d\n", threadindex, (int)tinit->seedvertexbase );
#endif

  }

  return;
}


static void bpInitSetConfig( bpMainData *bpdata, bpOperation *op, bpf referenceradius )
{
  bpdata->ballradiuspivot = op->ballradiuspivot * referenceradius;
  bpdata->ballradiusoverride = op->ballradiusoverride * referenceradius;
  bpdata->balldistmin = op->balldistmin * referenceradius;
  bpdata->pivotcosphericalrange = op->pivotcosphericalrange;
  bpdata->pivotcoplanardist = op->pivotcoplanardist * referenceradius;
  bpdata->pivotjoinradiusfactor = op->pivotjoinradiusfactor;
  bpdata->pivotforceradiusfactor = op->pivotforceradiusfactor;
  bpdata->pivotoverriderange = op->pivotoverriderange;
  bpdata->dupvertexdistancesoft = op->dupvertexdistancesoft * referenceradius;
  bpdata->dupvertexdistancehard = op->dupvertexdistancehard * referenceradius;
  bpdata->dupvertexnormalagreement = op->dupvertexnormalagreement;
  bpdata->reductionfactor = op->reductionfactor;
  bpdata->seedballradius = op->seedballradius * referenceradius;
  bpdata->seedcoplanardist = op->seedcoplanardist * referenceradius;
  bpdata->seednormalagreement = op->seednormalagreement;
  bpdata->seednormalagreementsum = 3.0 * op->seednormalagreementsum;
  bpdata->seedmarkradius = bpdata->seedmarkradius * referenceradius;
  bpdata->hingenormalagreement = op->hingenormalagreement;
  bpdata->pivotnormalagreementsoft = op->pivotnormalagreementsoft;
  bpdata->pivotnormalagreementhard = bpfmin( op->pivotnormalagreementhard, op->pivotnormalagreementsoft );
  bpdata->pivotnormalagreementsumsoft = 3.0 * op->pivotnormalagreementsumsoft;
  bpdata->pivotnormalagreementsumhard = 3.0 * bpfmin( op->pivotnormalagreementsumhard, op->pivotnormalagreementsumsoft );
  bpdata->filterplanecutdistance = op->filterplanecutdistance * referenceradius;
  return;
}


static void bpInitSetBallRange( bpMainData *bpdata, bpf ballminimum, bpf ballmaximum )
{
  int count;
  bpf ball;

  bpdata->ballradiusmin = ballminimum;
  bpdata->ballradiusmax = ballmaximum;

  if( ballmaximum <= ballminimum )
  {
    bpdata->ballradius[0] = ballminimum;
    bpdata->ballcount = 1;
    return;
  }
  for( count = 0, ball = ballminimum ; count < BP_BALL_COUNT_MAX ; count++, ball *= 2.0 )
  {
    if( ball < ballmaximum )
      bpdata->ballradius[count] = ball;
    else
    {
      bpdata->ballradius[count] = ballmaximum;
      break;
    }
  }
  bpdata->ballcount = count;

  return;
}



/********/



static const char *bpStatusStageName[] =
{
 [BP_STATUS_STAGE_INIT] = "Initializing",
 [BP_STATUS_STAGE_SORT] = "Sorting Points",
 [BP_STATUS_STAGE_MESH] = "Building Mesh",
 [BP_STATUS_STAGE_STRIP] = "Stripping Mesh",
 [BP_STATUS_STAGE_CONNECT] = "Connectivity Check",
 [BP_STATUS_STAGE_CLEAN] = "Cleaning up Mesh",
 [BP_STATUS_STAGE_NORMALS] = "Computing Normals",
 [BP_STATUS_STAGE_PACK] = "Packing Vertices",
 [BP_STATUS_STAGE_STORE] = "Storing Indices",
 [BP_STATUS_STAGE_DONE] = "Done"
};

static double bpStatusStageProgress[] =
{
 [BP_STATUS_STAGE_INIT] = 0.0,
 [BP_STATUS_STAGE_SORT] = 5.0,
 [BP_STATUS_STAGE_MESH] = 83.0,
 [BP_STATUS_STAGE_STRIP] = 2.0,
 [BP_STATUS_STAGE_CONNECT] = 5.0,
 [BP_STATUS_STAGE_CLEAN] = 2.0,
 [BP_STATUS_STAGE_NORMALS] = 1.0,
 [BP_STATUS_STAGE_PACK] = 1.0,
 [BP_STATUS_STAGE_STORE] = 1.0,
 [BP_STATUS_STAGE_DONE] = 0.0
};

static void bpUpdateStatus( bpMainData *bpdata, bpThreadInit *threadinit, int stage, int restartcount, bpStatus *status )
{
  int threadid, stageindex;
  long vertexcount;
  double progress, subprogress;
  bpThreadInit *tinit;
  bpThreadData *tdata;

  if( !( threadinit ) )
    status->stage = stage;
  else
  {
    tinit = &threadinit[0];
    status->stage = tinit->stage;
    tdata = tinit->tdata;
    if( (unsigned)status->stage >= BP_STATUS_STAGE_COUNT )
      return;
    status->trianglecount = 0;
    for( threadid = 0 ; threadid < bpdata->threadcount ; threadid++ )
    {
      tinit = &threadinit[threadid];
      tdata = tinit->tdata;
      status->trianglecount += tdata->statustricount;
    }
  }

  if( status->stage > BP_STATUS_STAGE_MESH )
    status->triangleguess = status->trianglecount;
  else
  {
    vertexcount = ( bpdata->activevertexcount ? bpdata->activevertexcount : bpdata->vertexcount );
    status->triangleguess = vertexcount * 2;
  }

  progress = 0.0;
  status->stagename = bpStatusStageName[status->stage];
  for( stageindex = 0 ; stageindex < status->stage ; stageindex++ )
    progress += bpStatusStageProgress[stageindex];

  subprogress = 0.0;
  if( status->stage == BP_STATUS_STAGE_SORT )
    subprogress = (double)restartcount / 5.0;
  else if( status->stage == BP_STATUS_STAGE_MESH )
    subprogress = (double)status->trianglecount / (double)status->triangleguess;
  subprogress = fmax( 0.0, fmin( 1.0, subprogress ) );

  progress += subprogress * bpStatusStageProgress[status->stage];
  if( progress > status->progress )
    status->progress = progress;

  return;
}



/********/



void bpOperationInit( bpOperation *op )
{
  /* Default stuff */
  op->ballradiusmin = 1.0;
  op->ballradiusmax = 1.0;
  op->trianglewinding = BP_TRIANGLE_WINDING_CW;
  op->triangleindexsize = sizeof(int);
  op->trianglestride = 3*sizeof(int);
  op->triangleoffset = 0;
  op->trianglealloc = 0;
  op->allocopaque = 0;

  /* Input */
  op->vertexcount = 0;
  op->vertex = 0;
  op->vertexstride = 0;
  op->normal = 0;
  op->normalstride = 0;

  /* Vertex packing callback */
  op->packopaquepointer = 0;
  op->packcallback = 0;

  /* Status callback */
  op->statusmiliseconds = 0;
  op->statusopaquepointer = 0;
  op->statuscallback = 0;

  /* Advanced settings, default values */
  op->hashextrabits = 2.0;
  op->hashlockpageshift = 0;
  op->ballradiuspivot = 1.0;
  op->ballradiusoverride = 1.0;
  op->balldistmin = 0.01;
  op->pivotcosphericalrange = CONFIG_PIVOT_COSPHERICAL;
  op->pivotcoplanardist = CONFIG_SPHERE_COPLANAR_FACTOR;
  op->pivotjoinradiusfactor = 1.0;
  op->pivotforceradiusfactor = 1.5;
  op->pivotoverriderange = CONFIG_PIVOT_COSPHERICAL_OVERRIDE;
  op->dupvertexdistancesoft = 0.10;
  op->dupvertexdistancehard = 0.001;
  op->dupvertexnormalagreement = 0.6;
  op->reductionfactor = 1.5;
  op->seedballradius = 0.99;
  op->seedcoplanardist = 0.001;
  op->seednormalagreement = 0.5;
  op->seednormalagreementsum = 0.7;
  op->seedmarkradius = 0.0;
  op->hingenormalagreement = -0.98;
/*
  op->pivotnormalagreementsoft = -0.5;
  op->pivotnormalagreementsumsoft = -0.3;
  op->pivotnormalagreementhard = -0.8;
  op->pivotnormalagreementsumhard = -0.6;
*/
  bpOperationNormalAgreement( op, 0.5, 0.5 );



  op->filterplanecutdistance = 0.01;
  op->randseed = time( 0 );
  mmInit();
  op->maxmemorysize = ((long long)2)*1024*1024*1024;
  if( mmcontext.sysmemory )
  {
    op->maxmemorysize = mmcontext.sysmemory >> 1; /* By default, allow to allocate up to half of system memory */
    if( op->maxmemorysize < 1024*1024*1024 )
      op->maxmemorysize = 1024*1024*1024;
  }

  return;
}

void bpOperationSetInput( bpOperation *op, size_t vertexcount, bpf *vertex, size_t vertexstride, bpf *normal, size_t normalstride )
{
  op->vertexcount = vertexcount;
  op->vertex = vertex;
  op->vertexstride = vertexstride;
  op->normal = normal;
  op->normalstride = normalstride;
  return;
}

void bpOperationSetBall( bpOperation *op, bpf ballradius )
{
  op->ballradiusmin = ballradius;
  op->ballradiusmax = ballradius;
  return;
}

void bpOperationSetBallRange( bpOperation *op, bpf ballminimum, bpf ballmaximum )
{
  op->ballradiusmin = ballminimum;
  op->ballradiusmax = ballmaximum;
  return;
}

int bpOperationSetOutput( bpOperation *op, int trianglewinding, size_t trianglestride, size_t triangleindexsize, size_t triangleoffset, void *(*trianglealloc)( void *allocopaque, size_t size ), void *allocopaque )
{
  if( ( trianglewinding != BP_TRIANGLE_WINDING_CW ) && ( trianglewinding != BP_TRIANGLE_WINDING_CCW ) )
    return 0;
  if( ( triangleindexsize != 1 ) && ( triangleindexsize != 2 ) && ( triangleindexsize != 4 ) && ( triangleindexsize != 8 ) )
    return 0;
  op->trianglewinding = trianglewinding;
  op->trianglestride = trianglestride;
  op->triangleindexsize = triangleindexsize;
  op->triangleoffset = triangleoffset;
  op->trianglealloc = trianglealloc;
  op->allocopaque = allocopaque;
  return 1;
}

void bpOperationNormalAgreement( bpOperation *op, bpf softstrictness, bpf hardstrictness )
{
  if( softstrictness < 0.0 )
    softstrictness = 0.0;
  if( softstrictness > 1.0 )
    softstrictness = 1.0;
  if( hardstrictness < 0.0 )
    hardstrictness = 0.0;
  if( hardstrictness > 1.0 )
    hardstrictness = 1.0;
  op->pivotnormalagreementsoft = -0.5 + ( softstrictness * 0.8 );
  op->pivotnormalagreementsumsoft = -0.2 + ( softstrictness * 1.0 );
  op->pivotnormalagreementhard = -1.01 + ( hardstrictness * 0.6 );
  op->pivotnormalagreementsumhard = -1.01 + ( hardstrictness * 0.9 );
  return;
}

void bpOperationVertexPackCallback( bpOperation *op, void (*packcallback)( void *opaquepointer, long newvertexindex, long oldvertexindex ), void *opaquepointer )
{
  op->packopaquepointer = opaquepointer;
  op->packcallback = packcallback;
  return;
}

void bpOperationStatusCallback( bpOperation *op, void (*statuscallback)( void *opaquepointer, const bpStatus *status ), void *opaquepointer, long miliseconds )
{
  op->statusmiliseconds = miliseconds;
  op->statusopaquepointer = opaquepointer;
  op->statuscallback = statuscallback;
  return;
}



/****/



static void *bpConstructMeshTry( bpOperation *op, bpOutput *output, int threadcount, uint32_t flags )
{
  int threadid, gridsize[3], restartcount, maxthreadcount;
  long statuswait;
  // 2012-10-22 ch3: dupvertexdistancesoft was set but never used, so commented
  bpf limit[6], ballradiusmin, ballradiusmax /*, dupvertexdistancesoft*/;
  bpMainData bpdata;
  mtThread thread[BP_THREAD_COUNT_MAX];
  bpThreadInit threadinit[BP_THREAD_COUNT_MAX];
  bpThreadInit *tinit;
  bpThreadData *tdata;
  size_t trianglecount;
  void *trianglestore, *store;
  bpStatus status;

  /* Initialization of the memory manager, filling struct mmcontext with info about CPUs and NUMA */
  mmInit();

  output->msecs = 0;
  output->vertexcount = 0;
  output->trianglecount = 0;
  memset( &output->info, 0, sizeof(bpInfo) );
  output->warningcount = 0;
  output->errorcount = 1;

  if( op->vertexcount <= 2 )
    return 0;

  maxthreadcount = op->vertexcount / 1024;
  if( threadcount > maxthreadcount )
    threadcount = maxthreadcount;
  if( threadcount <= 0 )
  {
    threadcount = mmcontext.cpucount;
    if( threadcount <= 0 )
      threadcount = BP_THREAD_COUNT_DEFAULT;
  }
  if( threadcount > BP_THREAD_COUNT_MAX )
    threadcount = BP_THREAD_COUNT_MAX;

  /* Input settings */
  bpdata.threadcount = threadcount;
  bpdata.operationflags = flags;
  bpdata.vertexcount = op->vertexcount;
  bpdata.vertex = op->vertex;
  bpdata.vertexstride = op->vertexstride;
  bpdata.normal = op->normal;
  bpdata.normalstride = op->normalstride;
  bpdata.activevertexcount = 0;
  bpdata.hingehashtable = 0;
  trianglestore = 0;
  output->msecs = mmGetMillisecondsTime();

  bpdata.updatestatusflag = 0;
  status.progress = 0.0;
  statuswait = ( op->statusmiliseconds > 10 ? op->statusmiliseconds : 10 );
  status.trianglecount = 0;
  if( op->statuscallback )
  {
    bpdata.updatestatusflag = 1;
    bpUpdateStatus( &bpdata, 0, BP_STATUS_STAGE_INIT, 0, &status );
    op->statuscallback( op->statusopaquepointer, &status );
  }

  mmVolumeInit( &bpdata.volume, 1048576, 0x40, 1048576, 0x10 );
  bpBarrierInit( &bpdata.workbarrier, threadcount );
  bpBarrierInit( &bpdata.globalbarrier, threadcount+1 );
  mtMutexInit( &bpdata.syncmutex );
  mtSignalInit( &bpdata.syncsignal );
  bpdata.syncmeshworkcount = threadcount;

  /* Output settings */
  bpdata.trianglewinding = op->trianglewinding;
  bpdata.trianglestride = op->trianglestride;

  /* Advanced settings */
  bpInitSetConfig( &bpdata, op, op->ballradiusmin );

  /* Initialize a giant bitmap for all vertices, marking vertices tried as seed */
  mmBitMapInit( &bpdata.vertexmap, op->vertexcount, 0x0 );

  /* The limit search could be multithreaded */
  bpFindLimits( limit, bpdata.vertex, bpdata.vertexcount, bpdata.vertexstride );

#ifdef DEBUG_VERBOSE
printf( "Main grid limits : %.2f,%.2f ; %.2f,%.2f ; %.2f,%.2f\n", limit[0], limit[1], limit[2], limit[3], limit[4], limit[5] );
#endif

  /* Initialize per-thread data */
  for( threadid = 0 ; threadid < threadcount ; threadid++ )
  {
    tinit = &threadinit[threadid];
    tinit->threadid = threadid;
    tinit->bpdata = &bpdata;
    tinit->stage = BP_STATUS_STAGE_INIT;
  }
  bpInitThreadData( &bpdata, threadinit, threadcount, op->randseed );

  /* If we recompute normals, we need the spatial lock map */
  bpdata.pointlockmap.lock = 0;
  bpdata.pointlockmap.lockcount = 0;
  if( bpdata.operationflags & BP_FLAGS_RECOMPUTE_NORMALS )
    bpPointLockInit( &bpdata, &bpdata.pointlockmap, limit, threadcount );

  /* Allocate spatial grid */
  ballradiusmin = op->ballradiusmin;
  ballradiusmax = op->ballradiusmax;
  //dupvertexdistancesoft = op->dupvertexdistancesoft;
  bpGridSizePick( gridsize, bpdata.vertexcount, limit, ballradiusmin );
  bpdata.grid = bpGridAlloc( &bpdata, gridsize[0], gridsize[1], gridsize[2], limit );

  /* Launch! */
  for( threadid = 0 ; threadid < threadcount ; threadid++ )
    mtThreadCreate( &thread[threadid], bpThreadMain, &threadinit[threadid], MT_THREAD_FLAGS_JOINABLE, 0, 0 );

  /* Wait until all threads have properly initialized */
  if( bpdata.updatestatusflag )
    bpBarrierSync( &bpdata.globalbarrier );

  /* Instruct threads to loop the grid building until we get it under our memory constraints */
  bpdata.syncbuildflag = 0;
  for( restartcount = 0 ; ; restartcount++ )
  {
    /* Wait for all threads to populate the grid */
#ifdef BP_CONFIG_ENABLE_PROGRESS
    if( !( bpdata.updatestatusflag ) )
      bpBarrierSync( &bpdata.globalbarrier );
    else
    {
      for( ; !( bpBarrierSyncTimeout( &bpdata.globalbarrier, statuswait ) ) ; )
      {
        bpUpdateStatus( &bpdata, 0, BP_STATUS_STAGE_SORT, restartcount, &status );
        op->statuscallback( op->statusopaquepointer, &status );
      }
    }
#else
    bpBarrierSync( &bpdata.globalbarrier );
#endif

    bpGridFinish( &bpdata, bpdata.grid );

    /* Count sum of active vertices */
    bpdata.activevertexcount = 0;
    for( threadid = 0 ; threadid < threadcount ; threadid++ )
    {
      tinit = &threadinit[threadid];
      tdata = tinit->tdata;
      bpdata.activevertexcount += tdata->activevertexcount;
    }

#ifdef DEBUG_VERBOSE_MEMORY
printf( "Active Vertex Count : %lld / %lld\n", (long long)bpdata.activevertexcount, (long long)bpdata.vertexcount );
#endif

    /* Try to allocate hash table given count of active vertices */
    if( bpHingeHashInit( &bpdata, bpdata.activevertexcount, op->hashextrabits, op->hashlockpageshift, op->maxmemorysize ) )
      break;

    /* If the user didn't allow us to reduce the point cloud, then we have an error we can't deal with */
    if( !( bpdata.operationflags & BP_FLAGS_ALLOW_REDUCTION ) )
      goto error;

    /* If it failed, free the grid and try again with a bigger vertex dup value */
    bpGridFree( &bpdata, bpdata.grid );

    /* Reset all vertices as unmarked */
    mmBitMapReset( &bpdata.vertexmap, 0x0 );

    /* Adjust ballradius and dupvertex and try again */
    ballradiusmin *= bpdata.reductionfactor;
    op->dupvertexdistancesoft = bpfmax( 0.25, op->dupvertexdistancesoft );
    bpInitSetConfig( &bpdata, op, ballradiusmin );

    /* Allocate new spatial grid */
    bpGridSizePick( gridsize, bpdata.activevertexcount, limit, ballradiusmin );
    bpdata.grid = bpGridAlloc( &bpdata, gridsize[0], gridsize[1], gridsize[2], limit );

    /* Threads were waiting for main thread to build a new grid */
    bpBarrierSync( &bpdata.globalbarrier );
  }

  /* Space Lock Map, for exclusive access during multi-hits filtering */
  bpSpaceMapInit( &bpdata, &bpdata.spacemap, bpdata.activevertexcount, limit, ballradiusmin );

  /* Build ball range */
  if( ballradiusmax < ballradiusmin )
    ballradiusmax = ballradiusmin;
  bpInitSetBallRange( &bpdata, ballradiusmin, ballradiusmax );

  /* Vertex packing */
  bpdata.packopaquepointer = op->packopaquepointer;
  bpdata.packcallback = op->packcallback;
  if( !( bpdata.packcallback ) )
    bpdata.operationflags &= ~BP_FLAGS_PACK_VERTICES;

  /* Notify threads that we are all good to go */
  bpdata.syncbuildflag = 1;
  bpBarrierSync( &bpdata.globalbarrier );

  /* Wait for all threads to be done building their own meshes */
  mtMutexLock( &bpdata.syncmutex );
#ifdef BP_CONFIG_ENABLE_PROGRESS
  if( !( bpdata.updatestatusflag ) )
  {
    for( ; bpdata.syncmeshworkcount ; )
      mtSignalWait( &bpdata.syncsignal, &bpdata.syncmutex );
  }
  else
  {
    for( ; bpdata.syncmeshworkcount ; )
    {
      mtSignalWaitTimeout( &bpdata.syncsignal, &bpdata.syncmutex, statuswait );
      if( bpdata.syncmeshworkcount )
      {
        bpUpdateStatus( &bpdata, threadinit, 0, 0, &status );
        op->statuscallback( op->statusopaquepointer, &status );
      }
    }
  }
#else
  for( ; bpdata.syncmeshworkcount ; )
    mtSignalWait( &bpdata.syncsignal, &bpdata.syncmutex );
#endif
  mtMutexUnlock( &bpdata.syncmutex );

  /* Count total of triangles */
  trianglecount = 0;
  for( threadid = 0 ; threadid < threadcount ; threadid++ )
  {
    tinit = &threadinit[threadid];
    tdata = tinit->tdata;
    trianglecount += tdata->trianglecount;
  }
  output->trianglecount = trianglecount;

  /* Check for successful mesh construction */
  bpdata.syncstoreflag = 0;
  bpdata.redirectiontable = 0;
  if( trianglecount )
  {
    bpdata.syncstoreflag = 1;

    /* Allocate triangle memory */
    if( op->trianglealloc )
      trianglestore = op->trianglealloc( op->allocopaque, trianglecount * op->trianglestride );
    else
      trianglestore = malloc( trianglecount * op->trianglestride );

    /* Assign triangle store address to each thread */
    store = ADDRESS( trianglestore, op->triangleoffset );
    for( threadid = 0 ; threadid < threadcount ; threadid++ )
    {
      tinit = &threadinit[threadid];
      tdata = tinit->tdata;
      tdata->trianglestore = store;
      tdata->triangleindexsize = op->triangleindexsize;
      store = ADDRESS( store, tdata->trianglecount * op->trianglestride );
    }

    /* If requested by the user, repack all vertices */
    bpdata.packedvertexcount = bpdata.vertexcount;
    if( bpdata.operationflags & BP_FLAGS_PACK_VERTICES )
    {
      /* Threads have already flagged the vertices */
      bpdata.redirectiontable = malloc( bpdata.vertexcount * sizeof(bpi) );
      bpdata.packedvertexcount = bpBuildRedirectionTable( &bpdata );
    }
    output->vertexcount = bpdata.packedvertexcount;
  }

  /* STEP 5 : Wake up all threads blocked here to store triangles */
  bpBarrierSync( &bpdata.globalbarrier );

  /* Sum per-thread information before we free all per-thread memory */
  for( threadid = 0 ; threadid < threadcount ; threadid++ )
  {
    tinit = &threadinit[threadid];
    tdata = tinit->tdata;
    output->info.orphancount += tdata->info.orphancount;
    output->info.hingelostcount += tdata->info.hingelostcount;
    output->info.conflictcount += tdata->info.conflictcount;
    output->info.pointdupcount += tdata->info.pointdupcount;
  }

  /* Wait for all threads to complete */
#ifdef BP_CONFIG_ENABLE_PROGRESS
  if( !( bpdata.updatestatusflag ) )
    bpBarrierSync( &bpdata.globalbarrier );
  else
  {
    for( ; !( bpBarrierSyncTimeout( &bpdata.globalbarrier, statuswait ) ) ; )
    {
      bpUpdateStatus( &bpdata, threadinit, 0, 0, &status );
      op->statuscallback( op->statusopaquepointer, &status );
    }
  }
#else
  bpBarrierSync( &bpdata.globalbarrier );
#endif
  for( threadid = 0 ; threadid < threadcount ; threadid++ )
    mtThreadJoin( &thread[threadid] );

  /* Set status to done */
  if( bpdata.updatestatusflag )
  {
    bpUpdateStatus( &bpdata, 0, BP_STATUS_STAGE_DONE, 0, &status );
    op->statuscallback( op->statusopaquepointer, &status );
  }

  /* Free vertex redirection table if we have one */
  if( bpdata.redirectiontable )
    free( bpdata.redirectiontable );

  output->warningcount = output->info.hingelostcount;
  output->errorcount = output->info.orphancount + output->info.conflictcount;

  /* Requires mmhash.c compiled with MM_HASH_DEBUG_STATISTICS */
#ifdef MM_HASH_DEBUG_STATISTICS
  {
    long accesscount, collisioncount, relocationcount;
    long entrycount, entrycountmax, hashsizemax;

    mmHashStatistics( bpdata.hingehashtable, &accesscount, &collisioncount, &relocationcount, &entrycount, &entrycountmax, &hashsizemax );

    printf( "Hash Access     : %ld\n", accesscount );
    printf( "Hash Collision  : %ld\n", collisioncount );
    printf( "Hash Relocation : %ld\n", relocationcount );
    printf( "Entry Count     : %ld\n", entrycount );
    printf( "Entry Count Max : %ld\n", entrycountmax );
    printf( "Hash Size Max   : %ld\n", hashsizemax );
  }
#endif

  error:

  /* Final clean up */
  bpBarrierDestroy( &bpdata.workbarrier );
  bpBarrierDestroy( &bpdata.globalbarrier );
  mtMutexDestroy( &bpdata.syncmutex );
  mtSignalDestroy( &bpdata.syncsignal );
  bpHingeHashFree( &bpdata );
  bpPointLockFree( &bpdata, &bpdata.pointlockmap );
  bpSpaceMapFree( &bpdata, &bpdata.spacemap );
  mmBitMapFree( &bpdata.vertexmap );
  mmVolumeFreeAll( &bpdata.volume );

  /* Store total processing time */
  output->msecs = mmGetMillisecondsTime() - output->msecs;

  return trianglestore;
}



void *bpConstructMesh( bpOperation *op, bpOutput *output, int threadcount, uint32_t flags )
{
  int failurecount;
  void *trianglestore;
  bpOperation oplocal;

  oplocal = *op;

  /* Loop over if mesh reconstruction totally fails, so we can try automatically fiddling with some settings. */
  for( failurecount = 0 ; failurecount < BP_CONFIG_FAILURE_MAX_COUNT ; failurecount++ )
  {
    trianglestore = bpConstructMeshTry( &oplocal, output, threadcount, flags );
    if( trianglestore )
      break;

    /* Mesh construction failed. */

    /* Strategy #1 : Relax seed normal agreement */
    if( ( oplocal.seednormalagreement > oplocal.pivotnormalagreementsoft ) || ( oplocal.seednormalagreementsum > oplocal.pivotnormalagreementsumsoft ) )
    {
      oplocal.seednormalagreement = bpfmax( oplocal.seednormalagreement - 0.4, oplocal.pivotnormalagreementsoft );
      oplocal.seednormalagreementsum = bpfmax( oplocal.seednormalagreementsum - 0.2, oplocal.pivotnormalagreementsumsoft );
#ifdef DEBUG_VERBOSE_FAILURE
      printf( "******** Reconstruction Failure, Reducing Seed Normal Agreement To %f %f\n", oplocal.seednormalagreement, oplocal.seednormalagreementsum );
#endif
      continue;
    }

    /* Strategy #2 : Increase ball minimum size up to maximum */
    if( oplocal.ballradiusmin < oplocal.ballradiusmax )
    {
      oplocal.ballradiusmin = bpfmin( 2.0 * oplocal.ballradiusmin, oplocal.ballradiusmax );
#ifdef DEBUG_VERBOSE_FAILURE
      printf( "******** Reconstruction Failure, Increasing Minimum Ball Radius To %f\n", oplocal.ballradiusmin );
#endif
      continue;
    }

#ifdef DEBUG_VERBOSE_FAILURE
      printf( "******** Reconstruction Failure\n" );
#endif

    break;
  }

  return trianglestore;
}



