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
/**
 * @file
 *
 * Global memory management header.
 */


#ifndef MM_H
#define MM_H


#ifdef __cplusplus
extern "C" {
#endif



#define MM_DEBUG 0

#define MM_INLINE_LIST_FUNCTIONS

#define MM_ALLOC_CHECK


#define MM_DEBUG_GUARD_BYTES (32)
#define MM_DEBUG_MMAP (1)
/* Enabling this will lead to ever growing memory usage! Strictly for debugging. */
#define MM_DEBUG_MMAP_LINGERING (0)



////


#ifdef MM_NUMA
 #include <numa.h>
#endif


#if defined(__linux__) || defined(__gnu_linux__) || defined(__linux) || defined(__linux)
 #define MM_LINUX (1)
 #define MM_UNIX (1)
#elif defined(__APPLE__)
 #define MM_OSX (1)
 #define MM_UNIX (1)
#elif defined(__unix__) || defined(__unix) || defined(unix)
 #define MM_UNIX (1)
#elif defined(_WIN64) || defined(__WIN64__) || defined(WIN64)
 #define MM_WIN64 (1)
 #define MM_WIN32 (1)
 #define MM_WINDOWS (1)
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
 #define MM_WIN32 (1)
 #define MM_WINDOWS (1)
#endif

#if __MINGW64__
 #define MM_MINGW32 (1)
 #define MM_MINGW64 (1)
#elif __MINGW32__
 #define MM_MINGW32 (1)
#endif


#if !MM_UNIX
 #undef MM_DEBUG_MMAP
 #define MM_DEBUG_MMAP (0)
#endif


#define MM_CPU_COUNT_MAXIMUM (1024)
#define MM_NODE_COUNT_MAXIMUM (256)


#ifndef CPUCONF_CACHE_LINE_SIZE
 #define CPUCONF_CACHE_LINE_SIZE 64
#endif


#if defined(__GNUC__)
 #define MM_CACHE_ALIGN __attribute__((aligned(CPUCONF_CACHE_LINE_SIZE)))
 #define MM_RESTRICT __restrict
 #define MM_NOINLINE __attribute__((noinline))
#else
 #define MM_CACHE_ALIGN
 #define MM_RESTRICT
 #define MM_NOINLINE
#endif


#if defined(__GNUC__) || defined(__INTEL_COMPILER)
 #define MM_ALIGN16 __attribute__((aligned(16)))
 #define MM_ALIGN16_SAFE (1)
#elif defined(_MSC_VER)
 #define MM_ALIGN16 __declspec(align(16))
 #define MM_ALIGN16_SAFE (1)
#else
 #define MM_ALIGN16
 #define MM_ALIGN16_SAFE (0)
#endif



#define MM_ERROR()  {printf("MM Error at %s:%d\n",file,line);exit(1)}

#ifndef ADDRESS
 #define ADDRESS(p,o) ((void *)(((char *)p)+(o)))
#endif

#ifndef ADDRESSDIFF
 #define ADDRESSDIFF(a,b) (((char *)a)-((char *)b))
#endif


#include <sys/time.h>

#ifdef __WIN32__
int mmGetTimeOfDay( struct timeval *tv );
#define gettimeofday(a,b) mmGetTimeOfDay(a)
#endif


typedef struct
{
  int numaflag;
  int pagesize;
  int cpucount;
  int nodecount;
  int cpunode[MM_CPU_COUNT_MAXIMUM];
  int64_t nodesize[MM_NODE_COUNT_MAXIMUM];
  int nodecpucount[MM_NODE_COUNT_MAXIMUM];
  int64_t sysmemory;
} mmContext;

extern mmContext mmcontext;


#include "mmatomic.h"
#include "mmthread.h"


#if MM_DEBUG
 #define MM_FUNC(n) mm##n##Debug
 #define MM_PARAMS , char *file, int line
#else
 #define MM_FUNC(n) mm##n
 #define MM_PARAMS
#endif


////


void mmInit();
void mmEnd();


void mmThreadBindToNode( int nodeindex );
void mmThreadBindToNode( int nodeindex );
void mmThreadBindToCpu( int cpuindex );
int mmCpuGetNode( int cpuindex );

void *mmNodeAlloc( int nodeindex, size_t size );
void mmNodeFree( int nodeindex, void *v, size_t size );
void mmNodeMap( int nodeindex, void *start, size_t bytes );

void *mmNodeAlignAlloc( int nodeindex, size_t size, intptr_t align );
void mmNodeAlignFree( int nodeindex, void *v, size_t size );



////



typedef struct
{
  void **prev;
  void *next;
} mmListNode;

typedef struct
{
  void *first;
  void **last;
} mmListDualHead;

typedef struct
{
  void *first;
  void *last;
} mmListLoopHead;

#ifndef MM_INLINE_LIST_FUNCTIONS

void mmListAdd( void **list, void *item, intptr_t offset );
void mmListRemove( void *item, intptr_t offset );
void mmListMergeList( void **listdst, void **listsrc, intptr_t offset );
void mmListMoveList( void **listdst, void **listsrc, intptr_t offset );

void mmListDualInit( mmListDualHead *head );
void mmListDualAddFirst( mmListDualHead *head, void *item, intptr_t offset );
void mmListDualAddLast( mmListDualHead *head, void *item, intptr_t offset );
void mmListDualRemove( mmListDualHead *head, void *item, intptr_t offset );
void *mmListDualLast( mmListDualHead *head, intptr_t offset );

#else

static inline void mmListAdd( void **list, void *item, intptr_t offset )
{
  mmListNode *node, *next;
  node = ADDRESS( item, offset );
  node->prev = list;
  node->next = *list;
  if( *list )
  {
    next = ADDRESS( *list, offset );
    next->prev = &(node->next);
  }
  *list = item;
  return;
}

static inline void mmListRemove( void *item, intptr_t offset )
{
  mmListNode *node, *next;
  node = ADDRESS( item, offset );
  *(node->prev) = (void *)node->next;
  if( node->next )
  {
    next = ADDRESS( node->next, offset );
    next->prev = node->prev;
  }
  return;
}

static inline void mmListMergeList( void **listdst, void **listsrc, intptr_t offset )
{
  void *item;
  mmListNode *node;
  if( !( *listsrc ) )
    return;
  for( item = *listdst ; item ; item = node->next )
  {
    node = ADDRESS( item, offset );
    listdst = &node->next;
  }
  item = *listsrc;
  node = ADDRESS( item, offset );
  node->prev = listdst;
  *listdst = item;
  *listsrc = 0;
  return;
}

static inline void mmListMoveList( void **listdst, void **listsrc, intptr_t offset )
{
  void *item;
  mmListNode *node;
  if( !( *listsrc ) )
  {
    *listdst = 0;
    return;
  }
  item = *listsrc;
  node = ADDRESS( item, offset );
  node->prev = listdst;
  *listdst = item;
  *listsrc = 0;
  return;
}

static inline void mmListDualInit( mmListDualHead *head )
{
  head->first = 0;
  head->last = &head->first;
  return;
}

static inline void mmListDualAddFirst( mmListDualHead *head, void *item, intptr_t offset )
{
  mmListNode *node, *next;
  node = ADDRESS( item, offset );
  node->prev = &head->first;
  node->next = head->first;
  if( node->next )
  {
    next = ADDRESS( node->next, offset );
    next->prev = &(node->next);
  }
  else
    head->last = &(node->next);
  head->first = item;
  return;
}

static inline void mmListDualAddLast( mmListDualHead *head, void *item, intptr_t offset )
{
  mmListNode *node;
  void **prev;
  prev = head->last;
  *prev = item;
  node = ADDRESS( item, offset );
  node->prev = head->last;
  head->last = &(node->next);
  node->next = 0;
  return;
}

static inline void mmListDualRemove( mmListDualHead *head, void *item, intptr_t offset )
{
  mmListNode *node, *next;
  node = ADDRESS( item, offset );
  *(node->prev) = (void *)node->next;
  if( node->next )
  {
    next = ADDRESS( node->next, offset );
    next->prev = node->prev;
  }
  else
    head->last = node->prev;
  return;
}

static inline void *mmListDualLast( mmListDualHead *head, intptr_t offset )
{
  if( !( head->first ) )
    return 0;
  return ADDRESS( head->last, -( offset + offsetof(mmListNode,next) ) );
}

#endif

void mmListLoopInit( mmListLoopHead *head );
void mmListLoopAddFirst( mmListLoopHead *head, void *item, intptr_t offset );
void mmListLoopAddLast( mmListLoopHead *head, void *item, intptr_t offset );
void mmListLoopInsert( mmListLoopHead *head, void *previtem, void *item, intptr_t offset );
void mmListLoopRemove( mmListLoopHead *head, void *item, intptr_t offset );
void *mmListLoopLast( mmListLoopHead *head, intptr_t offset );



////



typedef struct
{
  void *child[2];
  void *parent;
  int flags;
} mmBTreeNode;

#define MM_BTREE_FLAGS_LEFT (0)
#define MM_BTREE_FLAGS_RIGHT (1)
#define MM_BTREE_FLAGS_DIRECTION_MASK (1)
#define MM_BTREE_FLAGS_STEP (2)

void mmBTreeInsert( void *item, void *parent, int itemflag, intptr_t offset, void **root );
void mmBTreeRemove( void *item, intptr_t offset, void **root );

void *mmBtreeMostLeft( void *root, intptr_t offset );
void *mmBtreeMostRight( void *root, intptr_t offset );
void *mmBtreeNeighbourLeft( void *item, intptr_t offset );
void *mmBtreeNeighbourRight( void *item, intptr_t offset );
intptr_t mmBtreeItemCount( void *root, intptr_t offset );
int mmBtreeListOrdered( void *root, intptr_t offset, int (*callback)( void *item, void *v ), void *v );
int mmBtreeListBalanced( void *root, intptr_t offset, int (*callback)( void *item, void *v ), void *v );



////



typedef struct
{
  mmListNode listnode;
  mmBTreeNode node;
  int freecount;
  int blockindex;
} mmBlock;

typedef struct
{
  void *blocklist;
  void *freelist;
  size_t chunksize;
  int chunkperblock;
  int alignment;
  size_t allocsize;
  int keepfreecount;
  int chunkfreecount;
  void *treeroot;
  void *(*relayalloc)( void *head, size_t bytes MM_PARAMS );
  void (*relayfree)( void *head, void *v, size_t bytes MM_PARAMS );
  void *relayvalue;
  mtSpin spinlock;
} mmBlockHead;

void MM_FUNC(BlockInit)( mmBlockHead *head, size_t chunksize, int chunkperblock, int keepfreecount, int alignment MM_PARAMS );
void MM_FUNC(BlockNodeInit)( mmBlockHead *head, int nodeindex, size_t chunksize, int chunkperblock, int keepfreecount, int alignment MM_PARAMS );
void *MM_FUNC(BlockAlloc)( mmBlockHead *head MM_PARAMS );
void MM_FUNC(BlockRelease)( mmBlockHead *head, void *v MM_PARAMS );
void MM_FUNC(BlockFree)( mmBlockHead *head, void *v MM_PARAMS );
void MM_FUNC(BlockFreeAll)( mmBlockHead *head MM_PARAMS );
void MM_FUNC(BlockProcessList)( mmBlockHead *head, void *userpointer, int (*processchunk)( void *chunk, void *userpointer ) MM_PARAMS );
int MM_FUNC(BlockUseCount)( mmBlockHead *head MM_PARAMS );
int MM_FUNC(BlockFreeCount)( mmBlockHead *head MM_PARAMS );

#if MM_DEBUG
 #define mmBlockInit(v,w,x,y,z) MM_FUNC(BlockInit)(v,w,x,y,z,__FILE__,__LINE__)
 #define mmBlockNodeInit(u,v,w,x,y,z) MM_FUNC(BlockNodeInit)(u,v,w,x,y,z,__FILE__,__LINE__)
 #define mmBlockAlloc(x) MM_FUNC(BlockAlloc)(x,__FILE__,__LINE__)
 #define mmBlockRelease(x,y) MM_FUNC(BlockRelease)(x,y,__FILE__,__LINE__)
 #define mmBlockFree(x,y) MM_FUNC(BlockFree)(x,y,__FILE__,__LINE__)
 #define mmBlockFreeAll(x) MM_FUNC(BlockFreeAll)(x,__FILE__,__LINE__)
 #define mmBlockProcessList(x,y,z) MM_FUNC(BlockProcessList)(x,y,z,__FILE__,__LINE__)
 #define mmBlockUseCount(x) MM_FUNC(BlockProcessList)(x,__FILE__,__LINE__)
 #define mmBlockFreeCount(x) MM_FUNC(BlockProcessList)(x,__FILE__,__LINE__)
#endif

/*
void mmBlockRelayByVolume( mmBlockHead *head, void *volumehead );
void mmBlockRelayByZone( mmBlockHead *head, void *zonehead );
*/



////



typedef struct
{
  mmBlockHead indexblock;
  void *indextree;
  intptr_t indexlimit;
  mtSpin spinlock;
} mmIndexHead;

void mmIndexInit( mmIndexHead *head, int indexesperblock );
void mmIndexFreeAll( mmIndexHead *head );
void mmIndexAdd( mmIndexHead *head, intptr_t index );
intptr_t mmIndexGet( mmIndexHead *head );
int mmIndexRemove( mmIndexHead *head, intptr_t index );
size_t mmIndexCount( mmIndexHead *head );



////



typedef struct
{
  uintptr_t bitmask;
  int bitshift;
  uintptr_t countalign;
  uintptr_t indexshift;
  uintptr_t indexmask;
  uintptr_t initmask;
  size_t mapsize;
  uintptr_t *map;
  mtSpin spinlock;
} mmBitTableHead;

void mmBitTableInit( mmBitTableHead *head, int bitsperentry, int chunksize, int initmask );
void mmBitTableFreeAll( mmBitTableHead *head );
void mmBitTableSet( mmBitTableHead *head, uintptr_t index, int flags, int editmask );
uintptr_t mmBitTableGet( mmBitTableHead *head, uintptr_t index );



////



typedef struct
{
  size_t size;
  size_t used;
  void *next;
} mmGrowNode;

typedef struct
{
  mmGrowNode *first;
  size_t nodesize;
  mtSpin spinlock;
} mmGrow;

int MM_FUNC(GrowInit)( mmGrow *mgrow, size_t nodesize MM_PARAMS );
void MM_FUNC(GrowFreeAll)( mmGrow *mgrow MM_PARAMS );
void *MM_FUNC(GrowAlloc)( mmGrow *mgrow, size_t bytes MM_PARAMS );

#if MM_DEBUG
 #define mmGrowInit(x,y) MM_FUNC(GrowInit)(x,y,__FILE__,__LINE__)
 #define mmGrowFreeAll(x) MM_FUNC(GrowFreeAll)(x,__FILE__,__LINE__)
 #define mmGrowAlloc(x,y) MM_FUNC(GrowAlloc)(x,y,__FILE__,__LINE__)
#endif



////


#if 0

typedef struct
{
  void ***table;
  intptr_t pagecount;
  intptr_t pagesize;
  intptr_t pagemask;
  intptr_t pageshift;
  mtSpin spinlock;
} mmDirectory;

#define MM_DIR_ENTRY(dir,index) ( (dir)->table[ index >> (dir)->pageshift ][ index & (dir)->pagemask ] )

int MM_FUNC(DirInit)( mmDirectory *dir, intptr_t pageshift, intptr_t pagecount MM_PARAMS );
void MM_FUNC(DirSize)( mmDirectory *dir, intptr_t size MM_PARAMS );
void MM_FUNC(DirSet)( mmDirectory *dir, intptr_t index, void *entry MM_PARAMS );
void *MM_FUNC(DirGet)( mmDirectory *dir, intptr_t index MM_PARAMS );
void MM_FUNC(DirSetFast)( mmDirectory *dir, intptr_t index, void *entry MM_PARAMS );
void *MM_FUNC(DirGetFast)( mmDirectory *dir, intptr_t index MM_PARAMS );
void MM_FUNC(DirFree)( mmDirectory *dir MM_PARAMS );

#if MM_DEBUG
 #define mmDirInit(x,y,z) MM_FUNC(DirInit)(x,y,z,__FILE__,__LINE__)
 #define mmDirSize(x,y) MM_FUNC(DirSize)(x,y,__FILE__,__LINE__)
 #define mmDirSet(x,y,z) MM_FUNC(DirSet)(x,y,z,__FILE__,__LINE__)
 #define mmDirGet(x,y) MM_FUNC(DirGet)(x,y,__FILE__,__LINE__)
 #define mmDirSetFast(x,y,z) MM_FUNC(DirSetFast)(x,y,z,__FILE__,__LINE__)
 #define mmDirGetFast(x,y) MM_FUNC(DirGetFast)(x,y,__FILE__,__LINE__)
 #define mmDirFree(x) MM_FUNC(DirFree)(x,__FILE__,__LINE__)
#endif

#endif


////



void *MM_FUNC(AlignAlloc)( size_t bytes, intptr_t align MM_PARAMS );
void MM_FUNC(AlignFree)( void *v MM_PARAMS );
void *MM_FUNC(AlignGrow)( void *v, size_t bytes, size_t copybytes, intptr_t align MM_PARAMS );
void *MM_FUNC(AlignRelayAlloc)( void *(*relayalloc)( void *head, size_t bytes MM_PARAMS ), void *relayvalue, size_t bytes, intptr_t align, size_t displacement MM_PARAMS );
void MM_FUNC(AlignRelayFree)( void (*relayfree)( void *head, void *v, size_t bytes MM_PARAMS ), void *relayvalue, void *v, size_t bytes MM_PARAMS );

#if MM_DEBUG
 #define mmAlignAlloc(x,y) MM_FUNC(AlignAlloc)(x,y,__FILE__,__LINE__)
 #define mmAlignFree(x) MM_FUNC(AlignFree)(x,__FILE__,__LINE__)
 #define mmAlignGrow(x) MM_FUNC(AlignGrow)(x,__FILE__,__LINE__)
 #define mmAlignRelayAlloc(v,w,x,y,z) MM_FUNC(AlignRelayAlloc)(v,w,x,y,z,__FILE__,__LINE__)
 #define mmAlignRelayFree(w,x,y,z) MM_FUNC(AlignRelayFree)(w,x,y,z,__FILE__,__LINE__)
#endif



////



typedef struct
{
  size_t volumesize;
  size_t volumeblocksize;
  size_t minchunksize;
  size_t volumechunksize;
  size_t keepfreesize;
  size_t totalfreesize;
  size_t alignment;
  void *freeroot;
  void *volumelist;
  void *(*relayalloc)( void *head, size_t bytes MM_PARAMS );
  void (*relayfree)( void *head, void *v, size_t bytes MM_PARAMS );
  void *relayvalue;
  mtSpin spinlock;
} mmVolumeHead;

void MM_FUNC(VolumeInit)( mmVolumeHead *head, size_t volumesize, size_t minchunksize, size_t keepfreesize, size_t alignment MM_PARAMS );
void MM_FUNC(VolumeNodeInit)( mmVolumeHead *head, int nodeindex, size_t volumesize, size_t minchunksize, size_t keepfreesize, size_t alignment MM_PARAMS );
void *MM_FUNC(VolumeAlloc)( mmVolumeHead *head, size_t bytes MM_PARAMS );
void MM_FUNC(VolumeRelease)( mmVolumeHead *head, void *v MM_PARAMS );
void MM_FUNC(VolumeFree)( mmVolumeHead *head, void *v MM_PARAMS );
void MM_FUNC(VolumeShrink)( mmVolumeHead *head, void *v, size_t bytes MM_PARAMS );
size_t MM_FUNC(VolumeGetAllocSize)( mmVolumeHead *head, void *v );
void MM_FUNC(VolumeClean)( mmVolumeHead *head MM_PARAMS );
void MM_FUNC(VolumeFreeAll)( mmVolumeHead *head MM_PARAMS );
void *MM_FUNC(VolumeRealloc)( mmVolumeHead *head, void *v, size_t bytes MM_PARAMS );

#if MM_DEBUG
 #define mmVolumeInit(w,x,y,z,a) MM_FUNC(VolumeInit)(w,x,y,z,a,__FILE__,__LINE__);
 #define mmVolumeNodeInit(v,w,x,y,z) MM_FUNC(VolumeNodeInit)(v,w,x,y,z,__FILE__,__LINE__);
 #define mmVolumeAlloc(x,y) MM_FUNC(VolumeAlloc)(x,y,__FILE__,__LINE__);
 #define mmVolumeRelease(x,y) MM_FUNC(VolumeRelease)(x,y,__FILE__,__LINE__);
 #define mmVolumeFree(x,y) MM_FUNC(VolumeFree)(x,y,__FILE__,__LINE__);
 #define mmVolumeShrink(x,y,z) MM_FUNC(VolumeShrink)(x,y,z,__FILE__,__LINE__);
 #define mmVolumeGetAllocSize(x) MM_FUNC(VolumeGetAllocSize)(x,y,__FILE__,__LINE__);
 #define mmVolumeClean(x) MM_FUNC(VolumeClean)(x,__FILE__,__LINE__);
 #define mmVolumeFreeAll(x) MM_FUNC(VolumeFreeAll)(x,__FILE__,__LINE__);
 #define mmVolumeRealloc(x) MM_FUNC(VolumeRealloc)(x,y,z,__FILE__,__LINE__);

 #define mmVolumeAlloc MM_FUNC(VolumeAlloc)
 #define mmVolumeRelease MM_FUNC(VolumeRelease)
 #define mmVolumeFree MM_FUNC(VolumeFree)

#endif

/*
void mmVolumeRelayByZone( mmVolumeHead *head, void *zonehead );
*/



////



typedef struct
{
  void *address;
  size_t pagesize;
  size_t pagealignment;
  size_t zonesize;
  size_t alignment;
  size_t chunkheadersize;
  void *chunklist;
  mtSpin spinlock;
} mmZoneHead;

int MM_FUNC(ZoneInit)( mmZoneHead *head, size_t zonesize, intptr_t alignment MM_PARAMS );
void *MM_FUNC(ZoneAlloc)( mmZoneHead *head, size_t bytes MM_PARAMS );
void MM_FUNC(ZoneFree)( mmZoneHead *head, void *v MM_PARAMS );
void MM_FUNC(ZoneFreeAll)( mmZoneHead *head MM_PARAMS );

#if MM_DEBUG
 #define mmZoneInit(x,y,z) MM_FUNC(ZoneInit)(x,y,z,__FILE__,__LINE__);
 #define mmZoneAlloc(x,y) MM_FUNC(ZoneAlloc)(x,y,__FILE__,__LINE__);
 #define mmZoneFree(x,y) MM_FUNC(ZoneFree)(x,y,__FILE__,__LINE__);
 #define mmZoneFreeAll(x) MM_FUNC(ZoneFreeAll)(x,__FILE__,__LINE__);
#endif



////



void *mmAlloc( void *unused, size_t bytes MM_PARAMS );
void *mmRealloc( void *unused, void *v, size_t bytes MM_PARAMS );
void mmFree( void *unused, void *v, size_t bytes MM_PARAMS );

void *mmDebugAlloc( size_t bytes, char *file, int line );
void *mmDebugRealloc( void *v, size_t bytes, char *file, int line );
void mmDebugFree( void *v, char *file, int line );
#define mmDebugAlloc(x) mmDebugAlloc(x,__FILE__,__LINE__)
#define mmDebugFree(x) mmDebugFree(x,__FILE__,__LINE__)
#define mmDebugRealloc(x,y) mmDebugRealloc(x,y,__FILE__,__LINE__)

void mmListUses( char *file, int line );
#define mmListUses() mmListUses(__FILE__,__LINE__);


////


#if MM_DEBUG
 #define malloc(x) mmAlloc(0,(x),__FILE__,__LINE__)
 #define realloc(x,y) mmRealloc(0,(x),(y),__FILE__,__LINE__)
 #define free(x) mmFree(0,(x),0,__FILE__,__LINE__)
#elif defined(MM_ALLOC_CHECK)

static inline void *mmAllocCheck( size_t size, char *file, int line )
{
  void *p;
  p = malloc( size );
#if MM_WINDOWS
  if( !( p ) )
    fprintf( stderr, "WARNING : Denied memory allocation ( %ld bytes ) at %s:%d\n", (long)size, file, line );
#else
  if( !( p ) )
    fprintf( stderr, "WARNING : Denied memory allocation ( %lld bytes ) at %s:%d\n", (long long)size, file, line );
#endif
  return p;
}

static inline void *mmReallocCheck( void *p, size_t size, char *file, int line )
{
  p = realloc( p, size );
#if MM_WINDOWS
  if( !( p ) )
    fprintf( stderr, "WARNING : Denied memory allocation ( %ld bytes ) at %s:%d\n", (long)size, file, line );
#else
  if( !( p ) )
    fprintf( stderr, "WARNING : Denied memory allocation ( %lld bytes ) at %s:%d\n", (long long)size, file, line );
#endif
  return p;
}

 #define malloc(x) mmAllocCheck((x),__FILE__,__LINE__)
 #define realloc(x,y) mmReallocCheck((x),(y),__FILE__,__LINE__)
#endif




////



static inline uint64_t mmGetMillisecondsTime()
{
  struct timeval lntime;
  gettimeofday( &lntime, 0 );
  return ( (uint64_t)lntime.tv_sec * 1000 ) + ( (uint64_t)lntime.tv_usec / 1000 );
}


static inline uint64_t mmGetMicrosecondsTime()
{
  struct timeval lntime;
  gettimeofday( &lntime, 0 );
  return ( (uint64_t)lntime.tv_sec * 1000000 ) + (uint64_t)lntime.tv_usec;
}


static inline uint64_t mmGetNanosecondsTime()
{
  struct timeval lntime;
  gettimeofday( &lntime, 0 );
  return ( (uint64_t)lntime.tv_sec * 1000000000 ) + ( (uint64_t)lntime.tv_usec * 1000 );
}



////



#ifdef __cplusplus
}
#endif


#endif

