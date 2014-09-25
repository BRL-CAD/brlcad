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
 * Global memory management routines.
 *
 * This file includes all the generic memory management routines used
 * throughout the code : linked lists, balanced trees, block memory allocation,
 * growing memory allocation, page-based pointer directories, aligned memory
 * allocation, memory volume management, memory leak or buffer overrun
 * tracking, etc.
 */


#define MM_THREADING


#ifdef MM_THREADING
 #define _GNU_SOURCE
 #include <pthread.h>
 #include <sched.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>


#include "cpuconfig.h"
#include "cc.h"
#include "mm.h"


#if defined(MM_UNIX)
 #include <fcntl.h>
 #include <dirent.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <sys/sysinfo.h>
#elif defined(MM_WINDOWS)
 #include <windows.h>
#endif

#if _POSIX_MAPPED_FILES > 0
 #include <sys/mman.h>
 #define MM_ZONE_SUPPORT 1
#endif

#if defined(MM_LINUX)
 #include <utmpx.h>
#endif


#ifndef ADDRESS
#define ADDRESS(p,o) ((void *)(((char *)p)+(o)))
#endif

#ifndef ADDRESSDIFF
#define ADDRESSDIFF(a,b) (((char *)a)-((char *)b))
#endif


#if MM_DEBUG
 #define MM_PASSPARAMS , file, line
#else
 #define MM_PASSPARAMS
#endif


////


int mmInitStatus = 0;

mmContext mmcontext;

#ifdef MT_MUTEX_INITIALIZER
static mtMutex mmGlobalMutex = MT_MUTEX_INITIALIZER;
#else
static mtMutex mmGlobalMutex;
#endif

#ifdef MM_NUMA

int mmNumaInit()
{
  int nodeindex, cpuindex, cpurange;
  int nodecpucount;
  struct bitmask *cpus;
  if( numa_available() == -1 )
    return 0;
  mmcontext.numaflag = 1;
  cpurange = numa_num_configured_cpus();
  if( cpurange >= MM_CPU_COUNT_MAXIMUM )
  {
    fprintf( stderr, "CPU count %d exceeds %d, increase MM_CPU_COUNT_MAXIMUM in mm.h and try again.\n", cpurange, MM_CPU_COUNT_MAXIMUM );
    exit( 1 );
  }
  mmcontext.nodecount = numa_num_configured_nodes();
  if( mmcontext.nodecount >= MM_NODE_COUNT_MAXIMUM )
  {
    fprintf( stderr, "Node count %d exceeds %d, increase MM_NODE_COUNT_MAXIMUM in mm.h and try again.\n", mmcontext.nodecount, MM_NODE_COUNT_MAXIMUM );
    exit( 1 );
  }
  mmcontext.cpucount = 0;
  mmcontext.sysmemory = 0;
  for( nodeindex = 0 ; nodeindex < mmcontext.nodecount ; nodeindex++ )
  {
    mmcontext.nodesize[nodeindex] = numa_node_size64( nodeindex, 0 );
    mmcontext.sysmemory += mmcontext.nodesize[nodeindex];
    mmcontext.nodecpucount[nodeindex] = 0;
    if( !( numa_bitmask_isbitset( numa_all_nodes_ptr, nodeindex ) ) )
      continue;
    nodecpucount = 0;
    cpus = numa_bitmask_alloc( cpurange );
    if( numa_node_to_cpus( nodeindex, cpus ) >= 0 )
    {
      for( cpuindex = 0 ; cpuindex < cpurange ; cpuindex++ )
      {
        if( numa_bitmask_isbitset( cpus, cpuindex ) )
        {
          mmcontext.cpunode[ cpuindex ] = nodeindex;
          nodecpucount++;
        }
      }
    }
    numa_bitmask_free( cpus );
    mmcontext.nodecpucount[nodeindex] = nodecpucount;
    mmcontext.cpucount += nodecpucount;
  }
  return 1;
}

#endif

void numa_warn( int num, char *fmt, ... )
{
  return;
}

void mmInit()
{
  int64_t sysmemory;
  if( mmInitStatus )
    return;
  mmcontext.numaflag = 0;
#ifdef MM_NUMA
  mmNumaInit();
#endif
  if( !( mmcontext.numaflag ) )
  {
    mmcontext.nodecount = 1;
    sysmemory = -1;
 #if defined(MM_LINUX)
    mmcontext.cpucount = get_nprocs();
    mmcontext.pagesize = sysconf(_SC_PAGESIZE);
 #elif defined(MM_UNIX)
    mmcontext.cpucount = sysconf( _SC_NPROCESSORS_CONF );
    mmcontext.pagesize = sysconf(_SC_PAGESIZE);
 #elif defined(MM_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo( &sysinfo );
    mmcontext.cpucount = sysinfo.dwNumberOfProcessors;
    mmcontext.pagesize = sysinfo.dwPageSize;
 #else
    mmcontext.cpucount = 1;
 #endif
 #if defined(MM_UNIX) && defined(_SC_PHYS_PAGES)
    sysmemory = sysconf( _SC_PHYS_PAGES );
    if( sysmemory > 0 )
      sysmemory *= mmcontext.pagesize;
 #endif
    mmcontext.sysmemory = sysmemory;
    mmcontext.nodesize[0] = mmcontext.sysmemory;
    mmcontext.nodecpucount[0] = mmcontext.cpucount;
  }
  mtMutexInit( &mmGlobalMutex );
  mmInitStatus = 1;
/*
  {
  int nodeindex, cpuindex;
  printf( "NUMA nodes : %d\n", mmcontext.nodecount );
  for( nodeindex = 0 ; nodeindex < mmcontext.nodecount ; nodeindex++ )
    printf( "  NUMA node %d, size %lld, CPU count %d\n", nodeindex, (long long)mmcontext.nodesize[nodeindex], mmcontext.nodecpucount[nodeindex] );
  printf( "CPUs : %d\n", mmcontext.cpucount );
  for( cpuindex = 0 ; cpuindex < mmcontext.cpucount ; cpuindex++ )
    printf( "  CPU %d on node %d\n", cpuindex, mmcontext.cpunode[ cpuindex ] );
  }
*/
  return;
}


void mmEnd()
{
  mtMutexDestroy( &mmGlobalMutex );
  return;
}


////


void mmThreadBindToNode( int nodeindex )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
  {
    numa_run_on_node( nodeindex );
    return;
  }
#endif
#if defined(MM_LINUX)
  int cpuindex;
  cpu_set_t cpuset;
  CPU_ZERO( &cpuset );
  for( cpuindex = 0 ; cpuindex < mmcontext.cpucount ; cpuindex++ )
    CPU_SET( cpuindex, &cpuset );
  sched_setaffinity( 0, sizeof(cpu_set_t), &cpuset );
#endif
  return;
}

void mmThreadBindToCpu( int cpuindex )
{
#if defined(MM_LINUX)
  cpu_set_t cpuset;
  CPU_ZERO( &cpuset );
  CPU_SET( cpuindex, &cpuset );
  sched_setaffinity( 0, sizeof(cpu_set_t), &cpuset );
#endif
  return;
}

int mmCpuGetNode( int cpuindex )
{
  return mmcontext.cpunode[ cpuindex ];
}


////


void *mmNodeAlloc( int nodeindex, size_t size )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
    return numa_alloc_onnode( size, nodeindex );
#endif
  return malloc( size );
}

void mmNodeFree( int nodeindex, void *v, size_t size )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
  {
    numa_free( v, size );
    return;
  }
#endif
  free( v );
  return;
}

void mmNodeMap( int nodeindex, void *start, size_t bytes )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
    numa_tonode_memory( start, bytes, nodeindex );
#endif
  return;
}

static void *mmNodeRelayAlloc( void *head, size_t bytes MM_PARAMS )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
    return numa_alloc_onnode( bytes, (int)((intptr_t)head) );
#endif
  return malloc( bytes );
}

static void mmNodeRelayFree( void *head, void *v, size_t bytes MM_PARAMS )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
  {
    numa_free( v, bytes );
    return;
  }
#endif
  free( v );
  return;
}


void *mmNodeAlignAlloc( int nodeindex, size_t bytes, intptr_t align )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
    return numa_alloc_onnode( bytes, nodeindex );
#endif
  return mmAlignAlloc( bytes, align );
}

void mmNodeAlignFree( int nodeindex, void *v, size_t bytes )
{
#ifdef MM_NUMA
  if( mmcontext.numaflag )
  {
    numa_free( v, bytes );
    return;
  }
#endif
  mmAlignFree( v );
  return;
}




////


#ifndef MM_INLINE_LIST_FUNCTIONS

/**
 * Add the item to a linked list.
 *
 * The head of the linked list should be defined as a void pointer. The list
 * parameter can be a pointer to it, or a pointer to the mmListNode.next
 * variable of the preceeding item. The offset parameter should be the offset
 * of the mmListNode structure within the structure of the item. It can be
 * easily obtained with the offsetof(,) macro.
 */
void mmListAdd( void **list, void *item, intptr_t offset )
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

/**
 * Remove the item from a linked list.
 *
 * The offset parameter should be the offset of the mmListNode  structure
 * within the structure of the item. It can be easily obtained with the
 * offsetof(,) macro.
 */
void mmListRemove( void *item, intptr_t offset )
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



void mmListMergeList( void **listdst, void **listsrc, intptr_t offset )
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


void mmListMoveList( void **listdst, void **listsrc, intptr_t offset )
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



/**
 * Initialize a dual-direction linked list.
 *
 * The head of the linked list should be defined as a mmListDualHead
 * structure, with the head parameter being a pointer to it.
 */
void mmListDualInit( mmListDualHead *head )
{
  head->first = 0;
  head->last = &head->first;
  return;
}


/**
 * Add the item to the beginning of a dual-direction linked list.
 *
 * The head of the linked list should be defined as a mmListDualHead structure,
 * with the head parameter being a pointer to it. The offset parameter should
 * be the offset of the mmListNode structure within the structure of the item.
 * It can be easily obtained with the offsetof(,) macro.
 */
void mmListDualAddFirst( mmListDualHead *head, void *item, intptr_t offset )
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


/**
 * Add the item to the end of a dual-direction linked list.
 *
 * The head of the linked list should be defined as a mmListDualHead structure,
 * with the head parameter being a pointer to it. The offset parameter should
 * be the offset of the mmListNode structure within the structure of the item.
 * It can be easily obtained with the offsetof(,) macro.
 */
void mmListDualAddLast( mmListDualHead *head, void *item, intptr_t offset )
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


/**
 * Remove the item from a dual-direction linked list.
 *
 * The head of the linked list should be defined as a mmListDualHead structure,
 * with the head parameter being a pointer to it. The offset parameter should
 * be the offset of the mmListNode structure within the structure of the item.
 * It can be easily obtained with the offsetof(,) macro.
 */
void mmListDualRemove( mmListDualHead *head, void *item, intptr_t offset )
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


void *mmListDualLast( mmListDualHead *head, intptr_t offset )
{
  if( !( head->first ) )
    return 0;
  return ADDRESS( head->last, -( offset + offsetof(mmListNode,next) ) );
}


#endif


void mmListLoopInit( mmListLoopHead *head )
{
  head->first = 0;
  head->last = 0;
  return;
}


void mmListLoopAddFirst( mmListLoopHead *head, void *item, intptr_t offset )
{
  mmListNode *node, *prev, *next;
  node = ADDRESS( item, offset );
  if( !( head->first ) )
  {
    head->first = item;
    head->last = item;
    node->prev = item;
    node->next = item;
    return;
  }
  node->prev = head->last;
  node->next = head->first;
  next = ADDRESS( head->first, offset );
  next->prev = item;
  prev = ADDRESS( head->last, offset );
  prev->next = item;
  head->first = item;
  return;
}


void mmListLoopAddLast( mmListLoopHead *head, void *item, intptr_t offset )
{
  mmListNode *node, *prev, *next;
  node = ADDRESS( item, offset );
  if( !( head->first ) )
  {
    head->first = item;
    head->last = item;
    node->prev = item;
    node->next = item;
    return;
  }
  prev = ADDRESS( head->last, offset );
  prev->next = item;
  next = ADDRESS( head->first, offset );
  next->prev = item;
  node->prev = head->last;
  node->next = head->first;
  head->last = item;
  return;
}


void mmListLoopInsert( mmListLoopHead *head, void *previtem, void *item, intptr_t offset )
{
  mmListNode *prev, *node, *next;
  node = ADDRESS( item, offset );
  if( !( head->first ) )
  {
    head->first = item;
    head->last = item;
    node->prev = item;
    node->next = item;
    return;
  }
  node->prev = previtem;
  prev = ADDRESS( previtem, offset );
  node->next = prev->next;
  prev->next = item;
  next = ADDRESS( node->next, offset );
  next->prev = item;
  if( head->last == previtem )
    head->last = item;
  return;
}


void mmListLoopRemove( mmListLoopHead *head, void *item, intptr_t offset )
{
  mmListNode *node, *prev, *next;
  node = ADDRESS( item, offset );
  prev = ADDRESS( node->prev, offset );
  prev->next = node->next;
  if( head->first == item )
  {
    head->first = node->next;
    if( head->first == item )
      head->first = 0;
  }
  next = ADDRESS( node->next, offset );
  next->prev = node->prev;
  if( head->last == item )
  {
    head->last = node->prev;
    if( head->last == item )
      head->last = 0;
  }
  return;
}



////



/**
 * Private function to balance a branch of the tree after an insertion.
 *
 * The details of the implementation are left as an exercise to the reader.
 */
static void mmBTreeInsertBalance( void *item, intptr_t offset, void **root )
{
  void *parent, *relative, *ancestor, *link;
  mmBTreeNode *node, *pnode, *rnode, *anode, *lnode, *tnode;

  node = ADDRESS( item, offset );
  parent = node->parent;

  if( !( parent ) )
  {
    node->flags |= MM_BTREE_FLAGS_STEP;
    *root = item;
    return;
  }

  pnode = ADDRESS( parent, offset );
  if( pnode->flags & MM_BTREE_FLAGS_STEP )
    return;

  ancestor = pnode->parent;
  anode = ADDRESS( ancestor, offset );

  relative = anode->child[ ( pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) ^ 1 ];
  if( ( relative ) && !( ( rnode = ADDRESS( relative, offset ) )->flags & MM_BTREE_FLAGS_STEP ) )
  {
    anode->flags &= ~MM_BTREE_FLAGS_STEP;
    pnode->flags |= MM_BTREE_FLAGS_STEP;
    rnode->flags |= MM_BTREE_FLAGS_STEP;
    mmBTreeInsertBalance( ancestor, offset, root );
    return;
  }

  if( ( node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) != ( pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) )
  {
    if( ( node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) == MM_BTREE_FLAGS_RIGHT )
    {
      node->flags = ( anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) | MM_BTREE_FLAGS_STEP;
      link = anode->parent;

      anode->parent = item;
      anode->flags = MM_BTREE_FLAGS_RIGHT;
      anode->child[0] = node->child[1];
      if( anode->child[0] )
      {
        tnode = ADDRESS( anode->child[0], offset );
        tnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
        tnode->parent = ancestor;
      }

      pnode->parent = item;
      pnode->child[1] = node->child[0];
      if( pnode->child[1] )
      {
        tnode = ADDRESS( pnode->child[1], offset );
        tnode->flags |= MM_BTREE_FLAGS_RIGHT;
        tnode->parent = parent;
      }

      if( relative )
        ( (mmBTreeNode *)ADDRESS( relative, offset ) )->flags |= MM_BTREE_FLAGS_STEP;

      node->child[0] = parent;
      node->child[1] = ancestor;
      node->parent = link;
      if( link )
      {
        lnode = ADDRESS( link, offset );
        lnode->child[ node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = item;
        return;
      }
      *root = item;
      return;
    }
    else
    {
      node->flags = ( anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) | MM_BTREE_FLAGS_STEP;
      link = anode->parent;

      anode->parent = item;
      anode->flags = 0;
      anode->child[1] = node->child[0];
      if( anode->child[1] )
      {
        tnode = ADDRESS( anode->child[1], offset );
        tnode->flags |= MM_BTREE_FLAGS_RIGHT;
        tnode->parent = ancestor;
      }

      pnode->parent = item;
      pnode->child[0] = node->child[1];
      if( pnode->child[0] )
      {
        tnode = ADDRESS( pnode->child[0], offset );
        tnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
        tnode->parent = parent;
      }

      if( relative )
        ( (mmBTreeNode *)ADDRESS( relative, offset ) )->flags |= MM_BTREE_FLAGS_STEP;

      node->child[0] = ancestor;
      node->child[1] = parent;
      node->parent = link;
      if( link )
      {
        lnode = ADDRESS( link, offset );
        lnode->child[ node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = item;
        return;
      }
      *root = item;
      return;
    }
  }

  if( ( node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) == MM_BTREE_FLAGS_RIGHT )
  {
    pnode->flags = ( anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) | MM_BTREE_FLAGS_STEP;
    link = anode->parent;

    anode->parent = parent;
    anode->flags = 0;
    anode->child[1] = pnode->child[0];
    if( anode->child[1] )
    {
      tnode = ADDRESS( anode->child[1], offset );
      tnode->flags |= MM_BTREE_FLAGS_RIGHT;
      tnode->parent = ancestor;
    }

    pnode->child[0] = ancestor;
    pnode->child[1] = item;
    pnode->parent = link;
    if( link )
    {
      lnode = ADDRESS( link, offset );
      lnode->child[ pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = parent;
      return;
    }
    *root = parent;
    return;
  }
  else
  {
    pnode->flags = ( anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) | MM_BTREE_FLAGS_STEP;
    link = anode->parent;

    anode->parent = parent;
    anode->flags = MM_BTREE_FLAGS_RIGHT;
    anode->child[0] = pnode->child[1];
    if( anode->child[0] )
    {
      tnode = ADDRESS( anode->child[0], offset );
      tnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
      tnode->parent = ancestor;
    }

    pnode->child[0] = item;
    pnode->child[1] = ancestor;
    pnode->parent = link;
    if( link )
    {
      lnode = ADDRESS( link, offset );
      lnode->child[ pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = parent;
      return;
    }
    *root = parent;
    return;
  }

  return;
}


/**
 * Insert an item within the balanced tree
 *
 * Inserts the item specified at the position specified by the parent pointer
 * and the itemflag, which can be either MM_BTREE_FLAGS_LEFT or
 * MM_BTREE_FLAGS_RIGHT to indicate on which side of the parent to insert. The
 * parent pointer can be null to indicate the top of the tree. The offset
 * parameter should be the offset of the mmBTreeNode structure within the
 * structure of the item. The root parameter is a pointer to the root of the
 * tree.
 */
void mmBTreeInsert( void *item, void *parent, int itemflag, intptr_t offset, void **root )
{
  mmBTreeNode *node, *pnode;

  node = ADDRESS( item, offset );
  node->parent = parent;
  node->child[0] = 0;
  node->child[1] = 0;
  node->flags = itemflag;
  if( parent )
  {
    pnode = ADDRESS( parent, offset );
    pnode->child[itemflag] = item;
  }

  mmBTreeInsertBalance( item, offset, root );

  return;
}


/**
 * Find lowest left gap to use as pivot for removal.
 */
static void *mmBTreeRemoveSeek( void *item, intptr_t offset )
{
  mmBTreeNode *node;

  node = ADDRESS( item, offset );
  item = node->child[1];
  for( ; ; )
  {
    node = ADDRESS( item, offset );
    if( !( node->child[0] ) )
      break;
    item = node->child[0];
  }

  return item;
}


static void mmBTreeRemoveBalanceLeft( void *item, intptr_t offset, void **root );
static void mmBTreeRemoveBalanceRight( void *item, intptr_t offset, void **root );


/**
 * Private function to balance a left branch of the tree after a removal.
 *
 * The details of the implementation are left as an exercise to the reader.
 */
static void mmBTreeRemoveBalanceLeft( void *item, intptr_t offset, void **root )
{
  int mask;
  void **plink;
  void *litem, *llitem, *lritem, *lrritem, *lrlitem;
  mmBTreeNode *node, *lnode, *llnode, *lrnode, *lrrnode, *lrlnode, *tempnode;

  node = ADDRESS( item, offset );
  litem = node->child[0];
  lnode = ADDRESS( litem, offset );
  lritem = lnode->child[1];
  lrnode = ADDRESS( lritem, offset );

  plink = root;
  if( node->parent )
    plink = &( (mmBTreeNode *)ADDRESS( node->parent, offset ) )->child[ node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ];

  if( !( lnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    lrlitem = lrnode->child[0];
    lrlnode = ADDRESS( lrlitem, offset );
    lrritem = lrnode->child[1];
    lrrnode = ADDRESS( lrritem, offset );

    if( !( lrlitem ) )
    {
      if( lrritem )
      {
        lnode->flags = node->flags;
        lnode->parent = node->parent;
        lnode->child[1] = lrritem;
        *plink = litem;

        lrrnode->flags = MM_BTREE_FLAGS_RIGHT;
        lrrnode->parent = litem;
	lrrnode->child[0] = lritem;
	lrrnode->child[1] = item;

        lrnode->flags = MM_BTREE_FLAGS_STEP;
        lrnode->parent = lrritem;
        lrnode->child[1] = 0;

        node->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
        node->parent = lrritem;
        node->child[0] = 0;
      }
      else
        goto lshift;
    }
    else if( !( lrlnode->flags & MM_BTREE_FLAGS_STEP ) )
    {
      lrnode->flags = node->flags;
      lrnode->parent = node->parent;
      lrnode->child[0] = litem;
      lrnode->child[1] = item;
      *plink = lritem;

      node->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
      node->parent = lritem;
      node->child[0] = lrritem;
      if( lrritem )
      {
        lrrnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
        lrrnode->parent = item;
      }

      lnode->flags = 0;
      lnode->parent = lritem;
      lnode->child[1] = lrlitem;
      lrlnode->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
      lrlnode->parent = litem;
    }
    else if( lrrnode->flags & MM_BTREE_FLAGS_STEP )
    {
      lshift:
      lnode->flags = node->flags;
      lnode->parent = node->parent;
      lnode->child[1] = item;
      *plink = litem;

      node->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
      node->parent = litem;
      node->child[0] = lritem;
      lrnode->flags = 0;
      lrnode->parent = item;
    }
    else
    {
      lnode->flags = node->flags;
      lnode->parent = node->parent;
      lnode->child[1] = lrritem;
      *plink = litem;

      node->flags = MM_BTREE_FLAGS_RIGHT;
      node->parent = lrritem;
      node->child[0] = lrrnode->child[1];
      if( node->child[0] )
      {
        tempnode = ADDRESS( node->child[0], offset );
        tempnode->flags = MM_BTREE_FLAGS_STEP;
        tempnode->parent = item;
      }

      lrnode->flags = 0;
      lrnode->parent = lrritem;
      lrnode->child[1] = lrrnode->child[0];
      if( lrnode->child[1] )
      {
        tempnode = ADDRESS( lrnode->child[1], offset );
        tempnode->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
        tempnode->parent = lritem;
      }

      lrrnode->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
      lrrnode->parent = litem;
      lrrnode->child[0] = lritem;
      lrrnode->child[1] = item;
    }

    return;
  }

  mask = node->flags & MM_BTREE_FLAGS_STEP;
  llitem = lnode->child[0];
  llnode = ADDRESS( llitem, offset );

  if( ( lritem ) && !( lrnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    lrlitem = lrnode->child[0];
    lrritem = lrnode->child[1];

    lrnode->flags = node->flags;
    lrnode->parent = node->parent;
    lrnode->child[0] = litem;
    lrnode->child[1] = item;
    *plink = lritem;

    node->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
    node->parent = lritem;
    node->child[0] = lrritem;
    if( lrritem )
    {
      lrrnode = ADDRESS( lrritem, offset );
      lrrnode->parent = item;
      lrrnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
    }

    lnode->flags = MM_BTREE_FLAGS_STEP;
    lnode->parent = lritem;
    lnode->child[1] = lrlitem;
    if( lrlitem )
    {
      lrlnode = ADDRESS( lrlitem, offset );
      lrlnode->parent = litem;
      lrlnode->flags |= MM_BTREE_FLAGS_RIGHT;
    }
  }
  else if( ( llitem ) && !( llnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    lnode->flags = node->flags | MM_BTREE_FLAGS_STEP;
    lnode->parent = node->parent;
    lnode->child[1] = item;
    *plink = litem;

    node->flags = MM_BTREE_FLAGS_RIGHT | mask;
    node->parent = litem;
    node->child[0] = lritem;
    if( lritem )
    {
      lrnode->parent = item;
      lrnode->flags = MM_BTREE_FLAGS_STEP;
    }

    llnode->flags = mask;
  }
  else if( !( mask ) )
  {
    node->flags |= MM_BTREE_FLAGS_STEP;
    lnode->flags = 0;
  }
  else
  {
    lnode->flags = 0;
    if( node->parent )
      ( node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ? mmBTreeRemoveBalanceLeft : mmBTreeRemoveBalanceRight )( node->parent, offset, root );
  }

  return;
}


/**
 * Private function to balance a right branch of the tree after a removal.
 *
 * The details of the implementation are left as an exercise to the reader.
 */
static void mmBTreeRemoveBalanceRight( void *item, intptr_t offset, void **root )
{
  int mask;
  void **plink;
  void *ritem, *rritem, *rlitem, *rllitem, *rlritem;
  mmBTreeNode *node, *rnode, *rrnode, *rlnode, *rllnode, *rlrnode, *tempnode;

  node = ADDRESS( item, offset );
  ritem = node->child[1];
  rnode = ADDRESS( ritem, offset );
  rlitem = rnode->child[0];
  rlnode = ADDRESS( rlitem, offset );

  plink = root;
  if( node->parent )
    plink = &( (mmBTreeNode *)ADDRESS( node->parent, offset ) )->child[ node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ];

  if( !( rnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    rlritem = rlnode->child[1];
    rlrnode = ADDRESS( rlritem, offset );
    rllitem = rlnode->child[0];
    rllnode = ADDRESS( rllitem, offset );

    if( !( rlritem ) )
    {
      if( rllitem )
      {
        rnode->flags = node->flags;
        rnode->parent = node->parent;
        rnode->child[0] = rllitem;
        *plink = ritem;

        rllnode->flags = 0;
        rllnode->parent = ritem;
	rllnode->child[1] = rlitem;
	rllnode->child[0] = item;

        rlnode->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
        rlnode->parent = rllitem;
        rlnode->child[0] = 0;

        node->flags = MM_BTREE_FLAGS_STEP;
        node->parent = rllitem;
        node->child[1] = 0;
      }
      else
        goto rshift;
    }
    else if( !( rlrnode->flags & MM_BTREE_FLAGS_STEP ) )
    {
      rlnode->flags = node->flags;
      rlnode->parent = node->parent;
      rlnode->child[1] = ritem;
      rlnode->child[0] = item;
      *plink = rlitem;

      node->flags = MM_BTREE_FLAGS_STEP;
      node->parent = rlitem;
      node->child[1] = rllitem;
      if( rllitem )
      {
        rllnode->flags |= MM_BTREE_FLAGS_RIGHT;
        rllnode->parent = item;
      }

      rnode->flags = MM_BTREE_FLAGS_RIGHT;
      rnode->parent = rlitem;
      rnode->child[0] = rlritem;
      rlrnode->flags = MM_BTREE_FLAGS_STEP;
      rlrnode->parent = ritem;
    }
    else if( rllnode->flags & MM_BTREE_FLAGS_STEP )
    {
      rshift:
      rnode->flags = node->flags;
      rnode->parent = node->parent;
      rnode->child[0] = item;
      *plink = ritem;

      node->flags = MM_BTREE_FLAGS_STEP;
      node->parent = ritem;
      node->child[1] = rlitem;
      rlnode->flags = MM_BTREE_FLAGS_RIGHT;
      rlnode->parent = item;
    }
    else
    {
      rnode->flags = node->flags;
      rnode->parent = node->parent;
      rnode->child[0] = rllitem;
      *plink = ritem;

      node->flags = 0;
      node->parent = rllitem;
      node->child[1] = rllnode->child[0];
      if( node->child[1] )
      {
        tempnode = ADDRESS( node->child[1], offset );
        tempnode->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
        tempnode->parent = item;
      }

      rlnode->flags = MM_BTREE_FLAGS_RIGHT;
      rlnode->parent = rllitem;
      rlnode->child[0] = rllnode->child[1];
      if( rlnode->child[0] )
      {
        tempnode = ADDRESS( rlnode->child[0], offset );
        tempnode->flags = MM_BTREE_FLAGS_STEP;
        tempnode->parent = rlitem;
      }

      rllnode->flags = MM_BTREE_FLAGS_STEP;
      rllnode->parent = ritem;
      rllnode->child[1] = rlitem;
      rllnode->child[0] = item;
    }

    return;
  }

  mask = node->flags & MM_BTREE_FLAGS_STEP;
  rritem = rnode->child[1];
  rrnode = ADDRESS( rritem, offset );

  if( ( rlitem ) && !( rlnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    rlritem = rlnode->child[1];
    rllitem = rlnode->child[0];

    rlnode->flags = node->flags;
    rlnode->parent = node->parent;
    rlnode->child[1] = ritem;
    rlnode->child[0] = item;
    *plink = rlitem;

    node->flags = MM_BTREE_FLAGS_STEP;
    node->parent = rlitem;
    node->child[1] = rllitem;
    if( rllitem )
    {
      rllnode = ADDRESS( rllitem, offset );
      rllnode->parent = item;
      rllnode->flags |= MM_BTREE_FLAGS_RIGHT;
    }

    rnode->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
    rnode->parent = rlitem;
    rnode->child[0] = rlritem;
    if( rlritem )
    {
      rlrnode = ADDRESS( rlritem, offset );
      rlrnode->parent = ritem;
      rlrnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
    }
  }
  else if( ( rritem ) && !( rrnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    rnode->flags = node->flags | MM_BTREE_FLAGS_STEP;
    rnode->parent = node->parent;
    rnode->child[0] = item;
    *plink = ritem;

    node->flags = mask;
    node->parent = ritem;
    node->child[1] = rlitem;
    if( rlitem )
    {
      rlnode->parent = item;
      rlnode->flags = MM_BTREE_FLAGS_RIGHT | MM_BTREE_FLAGS_STEP;
    }

    rrnode->flags = MM_BTREE_FLAGS_RIGHT | mask;
  }
  else if( !( mask ) )
  {
    node->flags |= MM_BTREE_FLAGS_STEP;
    rnode->flags = MM_BTREE_FLAGS_RIGHT;
  }
  else
  {
    rnode->flags = MM_BTREE_FLAGS_RIGHT;
    if( node->parent )
      ( node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ? mmBTreeRemoveBalanceLeft : mmBTreeRemoveBalanceRight )( node->parent, offset, root );
  }

  return;
}


/**
 * Remove an item within the balanced tree
 *
 * Remove the item specified from the balanced tree. The offset parameter
 * should be the offset of the mmBTreeNode structure within the structure
 * of the item. The root parameter is a pointer to the root of the tree.
 */
void mmBTreeRemove( void *item, intptr_t offset, void **root )
{
  void *target, *parent, *child;
  mmBTreeNode *node, *tnode, *pnode, *cnode;

  node = ADDRESS( item, offset );
  if( !( node->child[0] ) || !( node->child[1] ) )
    target = item;
  else
    target = mmBTreeRemoveSeek( item, offset );

  tnode = ADDRESS( target, offset );
  child = tnode->child[0];
  if( tnode->child[1] )
    child = tnode->child[1];
  cnode = ADDRESS( child, offset );

  parent = tnode->parent;
  pnode = ADDRESS( parent, offset );

  if( !( tnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    if( child )
    {
      cnode->parent = parent;
      cnode->flags = ( tnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) | MM_BTREE_FLAGS_STEP;
    }
    if( parent )
      pnode->child[ tnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = child;
    else
      *root = child;
  }
  else if( ( child ) && !( cnode->flags & MM_BTREE_FLAGS_STEP ) )
  {
    cnode->parent = parent;
    cnode->flags = ( tnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) | MM_BTREE_FLAGS_STEP;
    if( parent )
      pnode->child[ tnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = child;
    else
      *root = child;
  }
  else
  {
    if( child )
    {
      cnode->parent = parent;
      cnode->flags = tnode->flags;
    }
    if( parent )
    {
      pnode->child[ tnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = child;
      ( tnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ? mmBTreeRemoveBalanceLeft : mmBTreeRemoveBalanceRight )( parent, offset, root );
    }
    else
      *root = child;
  }

  if( item != target )
  {
    memcpy( tnode, node, sizeof(mmBTreeNode) );
    if( tnode->parent )
      ( (mmBTreeNode *)ADDRESS( tnode->parent, offset ) )->child[ tnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK ] = target;
    else
      *root = target;
    if( tnode->child[0] )
      ( (mmBTreeNode *)ADDRESS( tnode->child[0], offset ) )->parent = target;
    if( tnode->child[1] )
      ( (mmBTreeNode *)ADDRESS( tnode->child[1], offset ) )->parent = target;
  }

  return;
}


void *mmBtreeMostLeft( void *root, intptr_t offset )
{
  mmBTreeNode *node;
  if( !( root ) )
    return 0;
  node = ADDRESS( root, offset );
  while( node->child[MM_BTREE_FLAGS_LEFT] )
  {
    root = node->child[MM_BTREE_FLAGS_LEFT];
    node = ADDRESS( root, offset );
  }
  return root;
}


void *mmBtreeMostRight( void *root, intptr_t offset )
{
  mmBTreeNode *node;
  if( !( root ) )
    return 0;
  node = ADDRESS( root, offset );
  while( node->child[MM_BTREE_FLAGS_RIGHT] )
  {
    root = node->child[MM_BTREE_FLAGS_RIGHT];
    node = ADDRESS( root, offset );
  }
  return root;
}


void *mmBtreeNeighbourLeft( void *item, intptr_t offset )
{
  mmBTreeNode *node;
  node = ADDRESS( item, offset );
  if( node->child[MM_BTREE_FLAGS_LEFT] )
  {
    item = node->child[MM_BTREE_FLAGS_LEFT];
    node = ADDRESS( item, offset );
    while( node->child[MM_BTREE_FLAGS_RIGHT] )
    {
      item = node->child[MM_BTREE_FLAGS_RIGHT];
      node = ADDRESS( item, offset );
    }
    return item;
  }
  while( node->parent )
  {
    node = ADDRESS( item, offset );
    item = node->parent;
    if( ( node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) == MM_BTREE_FLAGS_RIGHT )
      return item;
  }
  return 0;
}


void *mmBtreeNeighbourRight( void *item, intptr_t offset )
{
  mmBTreeNode *node;
  node = ADDRESS( item, offset );
  if( node->child[MM_BTREE_FLAGS_RIGHT] )
  {
    item = node->child[MM_BTREE_FLAGS_RIGHT];
    node = ADDRESS( item, offset );
    while( node->child[MM_BTREE_FLAGS_LEFT] )
    {
      item = node->child[MM_BTREE_FLAGS_LEFT];
      node = ADDRESS( item, offset );
    }
    return item;
  }
  while( node->parent )
  {
    node = ADDRESS( item, offset );
    item = node->parent;
    if( ( node->flags & MM_BTREE_FLAGS_DIRECTION_MASK ) == MM_BTREE_FLAGS_LEFT )
      return item;
  }
  return 0;
}


int mmBtreeListOrdered( void *root, intptr_t offset, int (*callback)( void *item, void *v ), void *v )
{
  mmBTreeNode *node;
  node = ADDRESS( root, offset );
  if( !( root ) )
    return 1;
  if( !( mmBtreeListOrdered( node->child[MM_BTREE_FLAGS_LEFT], offset, callback, v ) ) )
    return 0;
  if( !( callback( root, v ) ) )
    return 0;
  if( !( mmBtreeListOrdered( node->child[MM_BTREE_FLAGS_RIGHT], offset, callback, v ) ) )
    return 0;
  return 1;
}


int mmBtreeListBalanced( void *root, intptr_t offset, int (*callback)( void *item, void *v ), void *v )
{
  mmBTreeNode *node;
  node = ADDRESS( root, offset );
  if( !( root ) )
    return 1;
  if( !( callback( root, v ) ) )
    return 0;
  if( !( mmBtreeListOrdered( node->child[MM_BTREE_FLAGS_LEFT], offset, callback, v ) ) )
    return 0;
  if( !( mmBtreeListOrdered( node->child[MM_BTREE_FLAGS_RIGHT], offset, callback, v ) ) )
    return 0;
  return 1;
}


intptr_t mmBtreeItemCount( void *root, intptr_t offset )
{
  mmBTreeNode *node;
  if( !( root ) )
    return 0;
  node = ADDRESS( root, offset );
  return 1 + mmBtreeItemCount( node->child[0], offset ) + mmBtreeItemCount( node->child[1], offset );
}



////



/**
 * Initialize the memory block head specified.
 *
 * The parameters chunksize and chunkperblock define the size of each memory
 * chunk and the number of chunks per block.
 */
void MM_FUNC(BlockInit)( mmBlockHead *head, size_t chunksize, int chunkperblock, int keepfreecount, int alignment MM_PARAMS )
{
  memset( head, 0, sizeof(mmBlockHead) );
  head->alignment = 0;

  head->chunksize = chunksize;
  if( head->chunksize < sizeof(mmListNode) )
    head->chunksize = sizeof(mmListNode);
  if( alignment >= 0x10 )
  {
    head->alignment = alignment - 1;
    head->chunksize = ( chunksize + head->alignment ) & ~head->alignment;
  }
  head->chunkfreecount = 0;
  head->relayalloc = mmAlloc;
  head->relayfree = mmFree;
  head->relayvalue = 0;
  head->chunkperblock = chunkperblock;
  head->allocsize = sizeof(mmBlock) + head->chunksize * head->chunkperblock;
  head->keepfreecount = keepfreecount + chunkperblock;
  mtSpinInit( &head->spinlock );
  return;
}


void MM_FUNC(BlockNodeInit)( mmBlockHead *head, int nodeindex, size_t chunksize, int chunkperblock, int keepfreecount, int alignment MM_PARAMS )
{
  MM_FUNC(BlockInit)( head, chunksize, chunkperblock, keepfreecount, alignment MM_PASSPARAMS );
  head->relayalloc = mmNodeRelayAlloc;
  head->relayfree = mmNodeRelayFree;
  head->relayvalue = (void *)((intptr_t)nodeindex);
  return;
}


static void mmBlockTreeInsert( mmBlock *block, void **treeroot )
{
  mmBlock *root = *treeroot;
  if( !( root ) )
  {
    mmBTreeInsert( block, 0, 0, offsetof(mmBlock,node), treeroot );
    return;
  }
  for( ; ; )
  {
    if( block < root )
    {
      if( root->node.child[0] )
      {
        root = root->node.child[0];
        continue;
      }
      mmBTreeInsert( block, root, MM_BTREE_FLAGS_LEFT, offsetof(mmBlock,node), treeroot );
      break;
    }
    else
    {
      if( root->node.child[1] )
      {
        root = root->node.child[1];
        continue;
      }
      mmBTreeInsert( block, root, MM_BTREE_FLAGS_RIGHT, offsetof(mmBlock,node), treeroot );
      break;
    }
  }
  return;
}


static mmBlock *mmBlockResolveChunk( void *p, mmBlock *root )
{
  mmBlock *best = 0;
  for( ; root ; )
  {
    if( p < (void *)root )
      root = root->node.child[0];
    else
    {
      best = root;
      root = root->node.child[1];
    }
  }
  return best;
}


/**
 * Allocates a chunk of memory from the block.
 *
 * The size of the memory chunk returned was defined during the initialisation
 * of the specified memory block head.
 */
void *MM_FUNC(BlockAlloc)( mmBlockHead *head MM_PARAMS )
{
  int a;
  mmBlock *block;
  void *chunk;
  mtSpinLock( &head->spinlock );
  if( !( head->freelist ) )
  {
    if( head->alignment )
      block = MM_FUNC(AlignRelayAlloc)( head->relayalloc, head->relayvalue, head->allocsize, head->alignment, sizeof(mmBlock) MM_PASSPARAMS );
    else
      block = head->relayalloc( head->relayvalue, head->allocsize MM_PASSPARAMS );
    if( !( block ) )
    {
      fprintf( stderr, "ERROR %s:%d\n", __FILE__, __LINE__ );
      return 0;
    }
    block->freecount = head->chunkperblock;
    mmListAdd( &head->blocklist, block, offsetof(mmBlock,listnode) );
    chunk = ADDRESS( block, sizeof(mmBlock) );
    for( a = 0 ; a < head->chunkperblock ; a++, chunk = ADDRESS( chunk, head->chunksize ) )
      mmListAdd( &head->freelist, chunk, 0 );
    mmBlockTreeInsert( block, &head->treeroot );
    head->chunkfreecount += head->chunkperblock;
  }
  chunk = head->freelist;
  block = mmBlockResolveChunk( chunk, head->treeroot );
  mmListRemove( chunk, 0 );
  block->freecount--;
  head->chunkfreecount--;
  mtSpinUnlock( &head->spinlock );
  return chunk;
}


/**
 * Free a chunk of memory previously allocated by mmBlockAlloc().
 */
void MM_FUNC(BlockFree)( mmBlockHead *head, void *v MM_PARAMS )
{
  int a;
  mmBlock *block;
  void *chunk;
  chunk = v;
  mtSpinLock( &head->spinlock );
  block = mmBlockResolveChunk( chunk, head->treeroot );
  block->freecount++;
  head->chunkfreecount++;
  mmListAdd( &head->freelist, chunk, 0 );
  if( ( block->freecount == head->chunkperblock ) && ( head->chunkfreecount >= head->keepfreecount ) )
  {
    mmListRemove( block, offsetof(mmBlock,listnode) );
    chunk = ADDRESS( block, sizeof(mmBlock) );
    for( a = 0 ; a < head->chunkperblock ; a++, chunk = ADDRESS( chunk, head->chunksize ) )
      mmListRemove( chunk, 0 );
    mmBTreeRemove( block, offsetof(mmBlock,node), &head->treeroot );
    if( head->alignment )
      MM_FUNC(AlignRelayFree)( head->relayfree, head->relayvalue, block, head->allocsize MM_PASSPARAMS );
    else
      head->relayfree( head->relayvalue, block, head->allocsize MM_PASSPARAMS );
    head->chunkfreecount -= head->chunkperblock;
  }
  mtSpinUnlock( &head->spinlock );
  return;
}


/**
 * Release a chunk of memory previously allocated by mmBlockAlloc().
 *
 * Unlike mmBlockFree(), this function will collect but never free unused
 * memory, improving performance if the memory pages are to be soon reused.
 */
void MM_FUNC(BlockRelease)( mmBlockHead *head, void *v MM_PARAMS )
{
  mmBlock *block;
  void *chunk;
  chunk = v;
  mtSpinLock( &head->spinlock );
  block = mmBlockResolveChunk( chunk, head->treeroot );
  block->freecount++;
  head->chunkfreecount++;
  mmListAdd( &head->freelist, chunk, 0 );
  mtSpinUnlock( &head->spinlock );
  return;
}


/**
 * Free all memory allocated by a block head.
 */
void MM_FUNC(BlockFreeAll)( mmBlockHead *head MM_PARAMS )
{
  mmBlock *block, *blocknext;
  mtSpinLock( &head->spinlock );
  for( block = head->blocklist ; block ; block = blocknext )
  {
    blocknext = block->listnode.next;
    if( head->alignment )
      MM_FUNC(AlignRelayFree)( head->relayfree, head->relayvalue, block, head->allocsize MM_PASSPARAMS );
    else
      head->relayfree( head->relayvalue, block, head->allocsize MM_PASSPARAMS );
  }
  head->blocklist = 0;
  head->freelist = 0;
  head->treeroot = 0;
  mtSpinUnlock( &head->spinlock );
  mtSpinDestroy( &head->spinlock );
  return;
}


void MM_FUNC(BlockProcessList)( mmBlockHead *head, void *userpointer, int (*processchunk)( void *chunk, void *userpointer ) MM_PARAMS )
{
  int i, blockcount, blockrefsize, chunkperblock;
  intptr_t **bitsref;
  intptr_t *blockmask;
  intptr_t blockindex, chunkindex;
  size_t chunksize;
  void *p, *chunk;
  mmBlock *block;
  mmListNode *list;

  mtSpinLock( &head->spinlock );

  blockcount = 0;
  for( block = head->blocklist ; block ; block = block->listnode.next )
    block->blockindex = blockcount++;

  chunksize = head->chunksize;
  chunkperblock = head->chunkperblock;
  blockrefsize = ( ( chunkperblock + CPUCONF_INTPTR_BITS - 1 ) >> CPUCONF_INTPTR_BITSHIFT ) * sizeof(intptr_t);
  bitsref = malloc( blockcount * ( sizeof(intptr_t *) + blockrefsize ) );
  memset( bitsref, 0, blockcount * ( sizeof(intptr_t *) + blockrefsize ) );

  p = ADDRESS( bitsref, blockcount * sizeof(intptr_t *) );
  for( i = 0 ; i < blockcount ; i++ )
  {
    bitsref[i] = p;
    p = ADDRESS( p, blockrefsize );
  }
  for( list = head->freelist ; list ; list = list->next )
  {
    block = mmBlockResolveChunk( list, head->treeroot );
    chunkindex = ADDRESSDIFF( list, ADDRESS( block, sizeof(mmBlock) ) ) / chunksize;
    bitsref[ block->blockindex ][ chunkindex >> CPUCONF_INTPTR_BITSHIFT ] |= (intptr_t)1 << ( chunkindex & (CPUCONF_INTPTR_BITS-1) );
  }

  blockindex = 0;
  for( block = head->blocklist ; block ; block = block->listnode.next )
  {
    blockmask = bitsref[ blockindex ];
    chunk = ADDRESS( block, sizeof(mmBlock) );
    for( chunkindex = 0 ; chunkindex < chunkperblock ; chunkindex++, chunk = ADDRESS( chunk, chunksize ) )
    {
      if( blockmask[ chunkindex >> CPUCONF_INTPTR_BITSHIFT ] & ( (intptr_t)1 << ( chunkindex & (CPUCONF_INTPTR_BITS-1) ) ) )
        continue;
      if( processchunk( chunk, userpointer ) )
        goto end;
    }
    blockindex++;
  }

  end:
  free( bitsref );
  mtSpinUnlock( &head->spinlock );

  return;
}


int MM_FUNC(BlockUseCount)( mmBlockHead *head MM_PARAMS )
{
  int blockcount, chunkcount;
  mmBlock *block;
  mmListNode *list;

  mtSpinLock( &head->spinlock );

  blockcount = 0;
  for( block = head->blocklist ; block ; block = block->listnode.next )
    blockcount++;
  chunkcount = blockcount * head->chunkperblock;
  for( list = head->freelist ; list ; list = list->next )
    chunkcount--;

  mtSpinUnlock( &head->spinlock );

  return chunkcount;
}


int MM_FUNC(BlockFreeCount)( mmBlockHead *head MM_PARAMS )
{
  int chunkcount;

  mtSpinLock( &head->spinlock );
  chunkcount = head->chunkfreecount;
  mtSpinUnlock( &head->spinlock );

  return chunkcount;
}



////



typedef struct
{
  intptr_t index;
  mmBTreeNode node;
} mmIndex;

void mmIndexInit( mmIndexHead *head, int indexesperblock )
{
  mmBlockInit( &head->indexblock, sizeof(mmIndex), indexesperblock, indexesperblock, 0x10 );
  head->indextree = 0;
  mtSpinInit( &head->spinlock );
  return;
}

void mmIndexFreeAll( mmIndexHead *head )
{
  mmBlockFreeAll( &head->indexblock );
  head->indextree = 0;
  mtSpinDestroy( &head->spinlock );
  return;
}

void mmIndexAdd( mmIndexHead *head, intptr_t index )
{
  mmIndex *indexnode, *root;
  mtSpinLock( &head->spinlock );
  indexnode = mmBlockAlloc( &head->indexblock );
  indexnode->index = index;
  if( !( head->indextree ) )
  {
    mmBTreeInsert( indexnode, 0, 0, offsetof(mmIndex,node), &head->indextree );
    mtSpinUnlock( &head->spinlock );
    return;
  }
  for( root = head->indextree ; ; )
  {
    if( index < root->index )
    {
      if( root->node.child[0] )
      {
        root = root->node.child[0];
        continue;
      }
      mmBTreeInsert( indexnode, root, MM_BTREE_FLAGS_LEFT, offsetof(mmIndex,node), &head->indextree );
      break;
    }
    else
    {
      if( root->node.child[1] )
      {
        root = root->node.child[1];
        continue;
      }
      mmBTreeInsert( indexnode, root, MM_BTREE_FLAGS_RIGHT, offsetof(mmIndex,node), &head->indextree );
      break;
    }
  }
  mtSpinUnlock( &head->spinlock );
  return;
}

intptr_t mmIndexGet( mmIndexHead *head )
{
  intptr_t index;
  mmIndex *indexnode;
  mtSpinLock( &head->spinlock );
  if( !( head->indextree ) )
    index = head->indexlimit++;
  else
  {
    for( indexnode = head->indextree ; indexnode->node.child[0] ; )
      indexnode = indexnode->node.child[0];
    index = indexnode->index;
    mmBTreeRemove( indexnode, offsetof(mmIndex,node), &head->indextree );
    mmBlockFree( &head->indexblock, indexnode );
  }
  mtSpinUnlock( &head->spinlock );
  return index;
}

int mmIndexRemove( mmIndexHead *head, intptr_t index )
{
  intptr_t limit;
  mmIndex *indexnode;
  mtSpinLock( &head->spinlock );
  if( index >= head->indexlimit )
  {
    for( limit = head->indexlimit ; limit < index ; limit++ )
      mmIndexAdd( head, index );
    head->indexlimit = index + 1;
    mtSpinUnlock( &head->spinlock );
    return 1;
  }
  for( indexnode = head->indextree ; ; )
  {
    if( !( indexnode ) )
    {
      mtSpinUnlock( &head->spinlock );
      return 0;
    }
    if( index == indexnode->index )
      break;
    if( index < indexnode->index )
      indexnode = indexnode->node.child[0];
    else
      indexnode = indexnode->node.child[1];
  }
  mmBTreeRemove( indexnode, offsetof(mmIndex,node), &head->indextree );
  mmBlockFree( &head->indexblock, indexnode );
  mtSpinUnlock( &head->spinlock );
  return 1;
}

size_t mmIndexCount( mmIndexHead *head )
{
  intptr_t count;
  mtSpinLock( &head->spinlock );
  count = mmBtreeItemCount( head->indextree, offsetof(mmIndex,node) );
  mtSpinUnlock( &head->spinlock );
  return count;
}



////



#define MM_BITTABLE_SIZEBYTES(head) ((((head->mapsize)<<head->bitshift))>>CPUCONF_CHAR_BITSHIFT)
#define MM_BITTABLE_MINALIGN (4096)

void mmBitTableInit( mmBitTableHead *head, int bitsperentry, int chunksize, int initmask )
{
  int i;
  head->bitshift = ccLog2Int32( ccAlignInt32( bitsperentry ) );
  head->bitmask = ( 1 << bitsperentry ) - 1;
  head->countalign = ccAlignIntPtr( chunksize );
  if( head->countalign < MM_BITTABLE_MINALIGN )
    head->countalign = MM_BITTABLE_MINALIGN;
  head->indexshift = CPUCONF_INTPTR_BITSHIFT >> head->bitshift;
  head->indexmask = ( 1 << head->indexshift ) - 1;
  head->mapsize = 0;
  head->map = 0;
  initmask &= ( 1 << head->bitmask ) - 1;
  head->initmask = 0;
  for( i = 0 ; i < CPUCONF_INTPTR_BITS ; i += head->bitmask )
    head->initmask |= initmask << i;
  mtSpinInit( &head->spinlock );
  return;
}

void mmBitTableFreeAll( mmBitTableHead *head )
{
  free( head->map );
  head->map = 0;
  head->mapsize = 0;
  mtSpinDestroy( &head->spinlock );
  return;
}

void mmBitTableSet( mmBitTableHead *head, uintptr_t index, int flags, int editmask )
{
  uintptr_t *map;
  uintptr_t offset, shift, mapindex, mapindexend;
  mtSpinLock( &head->spinlock );
  map = head->map;
  if( index >= head->mapsize )
  {
    mapindex = head->mapsize >> CPUCONF_INTPTR_BITSHIFT;
    head->mapsize = ( index + head->countalign ) & ( head->countalign - 1 );
    head->map = realloc( head->map, MM_BITTABLE_SIZEBYTES( head ) );
    mapindexend = head->mapsize >> CPUCONF_INTPTR_BITSHIFT;
    map = head->map;
    for( ; mapindex < mapindexend ; mapindex++ )
      map[mapindex] = head->initmask;
  }
  offset = index >> head->indexshift;
  shift = ( index & head->indexmask ) << head->bitshift;
  map[ offset ] &= ~( editmask << shift );
  map[ offset ] |= ( ( flags & editmask ) << shift );
  mtSpinUnlock( &head->spinlock );
  return;
}

uintptr_t mmBitTableGet( mmBitTableHead *head, uintptr_t index )
{
  uintptr_t *map;
  uintptr_t offset, shift, value;
  mtSpinLock( &head->spinlock );
  if( index >= head->mapsize )
  {
    mtSpinUnlock( &head->spinlock );
    return head->initmask;
  }
  map = head->map;
  offset = index >> head->indexshift;
  shift = ( index & head->indexmask ) << head->bitshift;
  value = ( map[ offset ] >> shift ) & head->bitmask;
  mtSpinUnlock( &head->spinlock );
  return value;
}



////



#define MM_GROW_NODE_MEM(x,b) ADDRESS(x,sizeof(mmGrowNode)+b)

/**
 * Private function to add a new node to the packed growing memory allocator
 */
static int mmGrowAdd( mmGrow *mgrow, int size MM_PARAMS )
{
  mmGrowNode *mnode;
  if( !( mnode = mmAlloc( 0, sizeof(mmGrowNode) + size MM_PASSPARAMS ) ) )
    return 0;
  mnode->size = size;
  mnode->used = 0;
  mnode->next = mgrow->first;
  mgrow->first = mnode;
  return 1;
}


/**
 * Initialize a packed growing memory allocator.
 *
 * While no space is wasted between allocated chunks of memory, it is
 * impossible to free individual allocations. A growing memory allocator
 * must be freed entirely at once.
 */
int MM_FUNC(GrowInit)( mmGrow *mgrow, size_t nodesize MM_PARAMS )
{
  mgrow->first = 0;
  mgrow->nodesize = nodesize;
  mtSpinInit( &mgrow->spinlock );
  return mmGrowAdd( mgrow, nodesize MM_PASSPARAMS );
}


/**
 * Free all chunks of a growing memory allocator.
 */
void MM_FUNC(GrowFreeAll)( mmGrow *mgrow MM_PARAMS )
{
  mmGrowNode *mnode, *next;
  mtSpinLock( &mgrow->spinlock );
  for( mnode = mgrow->first ; mnode ; mnode = next )
  {
    next = mnode->next;
    mmFree( 0, mnode, 0 MM_PASSPARAMS );
  }
  mgrow->first = 0;
  mtSpinUnlock( &mgrow->spinlock );
  mtSpinDestroy( &mgrow->spinlock );
  return;
}


/**
 * Allocate a chunk of memory from a growing memoy allocator.
 *
 * The memory can only be freed with mmGrowFreeAll() for the whole allocator.
 * There are no memory alignment garantees for the allocations, yet as all
 * memory is tightly packed, the alignment is directly dependant on the size
 * of the previous allocations.
 */
void *MM_FUNC(GrowAlloc)( mmGrow *mgrow, size_t bytes MM_PARAMS )
{
  size_t a, fbytes;
  mmGrowNode *mnode;
  mtSpinLock( &mgrow->spinlock );
  mnode = mgrow->first;
  fbytes = mnode->size - mnode->used;
  if( fbytes >= bytes )
  {
    a = mnode->used;
    mnode->used += bytes;
    mtSpinUnlock( &mgrow->spinlock );
    return MM_GROW_NODE_MEM( mnode, a );
  }
  mmGrowAdd( mgrow, ( bytes > mgrow->nodesize ) ? bytes : mgrow->nodesize MM_PASSPARAMS );
  mnode = mgrow->first;
  mnode->used = bytes;
  mtSpinUnlock( &mgrow->spinlock );
  return MM_GROW_NODE_MEM( mnode, 0 );
}



////



#if 0

/**
 * Initialize a directory structure.
 *
 * The pageshift parameter indicates the power of two defining the count of
 * entries per page. The pagecount parameter specifies the initial count of
 * pages, which can grow as required.
 */
int MM_FUNC(DirInit)( mmDirectory *dir, intptr_t pageshift, intptr_t pagecount MM_PARAMS )
{
  if( !( dir->table = mmAlloc( 0, pagecount * sizeof(void **) MM_PASSPARAMS ) ) )
    return 0;
  memset( dir->table, 0, pagecount * sizeof(void **) );
  dir->pagecount = pagecount;
  dir->pageshift = pageshift;
  dir->pagesize = 1 << pageshift;
  dir->pagemask = dir->pagesize - 1;
  mtSpinInit( &dir->spinlock );
  return 1;
}


/**
 * Private function to resize a directory page table.
 */
static inline void mmDirResizeTable( mmDirectory *dir, intptr_t newcount MM_PARAMS )
{
  intptr_t page;
  for( page = newcount ; page < dir->pagecount ; page++ )
  {
    if( dir->table[ page ] )
      mmFree( 0, dir->table[ page ], 0 MM_PASSPARAMS );
  }
  if( !( dir->table = mmRealloc( 0, dir->table, newcount * sizeof(void **) MM_PASSPARAMS ) ) )
    return;
  if( newcount > dir->pagecount )
    memset( ADDRESS( dir->table, dir->pagecount * sizeof(void **) ), 0, ( newcount - dir->pagecount ) * sizeof(void **) );
  dir->pagecount = newcount;
  return;
}


void MM_FUNC(DirSize)( mmDirectory *dir, intptr_t size MM_PARAMS )
{
  intptr_t page, pagecount;
  pagecount = ( size >> dir->pageshift ) + 1;
  size &= dir->pagemask;
  mtSpinLock( &dir->spinlock );
  if( pagecount != dir->pagecount )
    mmDirResizeTable( dir, pagecount MM_PASSPARAMS );
  for( page = 0 ; page < pagecount ; page++ )
  {
    if( dir->table[ page ] )
      continue;
    if( !( dir->table[ page ] = mmAlloc( 0, dir->pagesize * sizeof(void *) MM_PASSPARAMS ) ) )
      return;
  }
  mtSpinUnlock( &dir->spinlock );
  return;
}


/**
 * Sets the directory's item specified by the index to the pointer of the entry
 * parameter.
 *
 * If the directory is too small to accept the new entry, the internal table of
 * pages grow in size. The MM_DIR_ENTRY(dir,index) macro can be used to access
 * directory entries, both for reading and writing, only if the entry is known
 * to have been previously set.
 */
void MM_FUNC(DirSet)( mmDirectory *dir, intptr_t index, void *entry MM_PARAMS )
{
  intptr_t page;
  page = index >> dir->pageshift;
  index &= dir->pagemask;
  mtSpinLock( &dir->spinlock );
  if( page >= dir->pagecount )
    mmDirResizeTable( dir, ( dir->pagecount ? dir->pagecount << 1 : 1 ) MM_PASSPARAMS );
  if( !( dir->table[ page ] ) )
  {
    if( !( dir->table[ page ] = mmAlloc( 0, dir->pagesize * sizeof(void *) MM_PASSPARAMS ) ) )
      return;
  }
  dir->table[ page ][ index ] = entry;
  mtSpinUnlock( &dir->spinlock );
  return;
}


void *MM_FUNC(DirGet)( mmDirectory *dir, intptr_t index MM_PARAMS )
{
  intptr_t page;
  void *entry;
  page = index >> dir->pageshift;
  index &= dir->pagemask;
  mtSpinLock( &dir->spinlock );
  if( (uintptr_t)page >= dir->pagecount )
    return 0;
  if( !( dir->table[ page ] ) )
    return 0;
  entry = dir->table[ page ][ index ];
  mtSpinUnlock( &dir->spinlock );
  return entry;
}


void MM_FUNC(DirSetFast)( mmDirectory *dir, intptr_t index, void *entry MM_PARAMS )
{
  mtSpinLock( &dir->spinlock );
  dir->table[ index >> dir->pageshift ][ index & dir->pagemask ] = entry;
  mtSpinUnlock( &dir->spinlock );
  return;
}

void *MM_FUNC(DirGetFast)( mmDirectory *dir, intptr_t index MM_PARAMS )
{
  void *entry;
  mtSpinLock( &dir->spinlock );
  entry = dir->table[ index >> dir->pageshift ][ index & dir->pagemask ];
  mtSpinUnlock( &dir->spinlock );
  return entry;
}


/**
 * Free a directory structure.
 */
void MM_FUNC(DirFree)( mmDirectory *dir MM_PARAMS )
{
  intptr_t page;
  if( !( dir->table ) )
    return;
  mtSpinLock( &dir->spinlock );
  for( page = dir->pagecount-1 ; page >= 0 ; page-- )
  {
    if( dir->table[ page ] )
      mmFree( 0, dir->table[ page ], 0 MM_PASSPARAMS );
  }
  dir->pagecount = 0;
  mmFree( 0, dir->table, 0 MM_PASSPARAMS );
  dir->table = 0;
  mtSpinUnlock( &dir->spinlock );
  mtSpinDestroy( &dir->spinlock );
  return;
}

#endif



////



typedef struct
{
  int padding;
} mmAlign;


/**
 * Allocate a chunk of memory with the alignment specified.
 *
 * The alignment parameter must be a power of two. The allocated memory must be
 * freed with the mmAlignFree() function.
 */
void *MM_FUNC(AlignAlloc)( size_t bytes, intptr_t align MM_PARAMS )
{
  intptr_t i;
  void *v;
  mmAlign *malign;
  align--;
  if( !( v = mmAlloc( 0, bytes + align + sizeof(mmAlign) MM_PASSPARAMS ) ) )
    return 0;
  i = ( (intptr_t)v + align + sizeof(mmAlign) ) & ~align;
  malign = ADDRESS( (void *)i, -(int)sizeof(mmAlign) );
  malign->padding = ADDRESSDIFF( i, v );
  return (void *)i;
}

/**
 * Free a chunk of memory that was allocated by mmAlignAlloc().
 */
void MM_FUNC(AlignFree)( void *v MM_PARAMS )
{
  mmAlign *malign;
  malign = ADDRESS( v, -sizeof(mmAlign) );
  mmFree( 0, ADDRESS( v, -malign->padding ), 0 MM_PASSPARAMS );
  return;
}

void *MM_FUNC(AlignGrow)( void *v, size_t bytes, size_t copybytes, intptr_t align MM_PARAMS )
{
  void *newv;
  newv = MM_FUNC(AlignAlloc)( bytes, align );
  memcpy( newv, v, copybytes );
  MM_FUNC(AlignFree)( v );
  return newv;
}


void *MM_FUNC(AlignRelayAlloc)( void *(*relayalloc)( void *head, size_t bytes MM_PARAMS ), void *relayvalue, size_t bytes, intptr_t align, size_t displacement MM_PARAMS )
{
  intptr_t i;
  void *v;
  mmAlign *malign;
  align--;
  if( !( v = relayalloc( relayvalue, bytes + align + sizeof(mmAlign) + displacement MM_PASSPARAMS ) ) )
    return 0;
  i = ( (intptr_t)v + align + sizeof(mmAlign) + displacement ) & ~align;
  i -= displacement;
  malign = ADDRESS( (void *)i, -(int)sizeof(mmAlign) );
  malign->padding = ADDRESSDIFF( i, v );
  return (void *)i;
}


void MM_FUNC(AlignRelayFree)( void (*relayfree)( void *head, void *v, size_t bytes MM_PARAMS ), void *relayvalue, void *v, size_t bytes MM_PARAMS )
{
  mmAlign *malign;
  malign = ADDRESS( v, -sizeof(mmAlign) );
  relayfree( relayvalue, ADDRESS( v, -malign->padding ), bytes MM_PASSPARAMS );
  return;
}



////



typedef struct
{
  size_t bigsize;
  mmListNode list;
  int alignment;
} mmVolume;

typedef struct
{
  int32_t nextoffset;
  int32_t prevoffset;
} mmVolumeChunkHeader;

typedef struct
{
  mmVolumeChunkHeader h;
  mmBTreeNode node;
} mmVolumeChunk;

#define MM_VOLUME_CHUNK_FLAGS_FREE (0x1)
#define MM_VOLUME_CHUNK_FLAGS_LAST (0x2)
#define MM_VOLUME_CHUNK_FLAGS_MASK (0x3)

#define MM_VOLUME_CHUNK_GET_FLAGS(c) ((c)->h.nextoffset&MM_VOLUME_CHUNK_FLAGS_MASK)
#define MM_VOLUME_CHUNK_SET_FLAGS(c,f) (c)->h.nextoffset=(((c)->h.nextoffset&~MM_VOLUME_CHUNK_FLAGS_MASK)|(f))
#define MM_VOLUME_CHUNK_GET_NEXTOFFSET(c) ((c)->h.nextoffset&~MM_VOLUME_CHUNK_FLAGS_MASK)
#define MM_VOLUME_CHUNK_SET_NEXTOFFSET(c,n) (c)->h.nextoffset=((c)->h.nextoffset&MM_VOLUME_CHUNK_FLAGS_MASK)|(n)
#define MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS(c,n,f) (c)->h.nextoffset=((n)|(f))

#define MM_VOLUME_BIGCHUNK_THRESHOLD (1<<30)


/**
 * Initialize a memory volume.
 *
 * The parameter volumesize defines the default size of each chunk of memory
 * internally allocated by the volume manager, chunks which will be sliced
 * accordingly to memory allocation needs. The parameter minchunksize defines
 * the minimum size a chunk of memory must have not to be fused.
 */
void MM_FUNC(VolumeInit)( mmVolumeHead *head, size_t volumesize, size_t minchunksize, size_t keepfreesize, size_t alignment MM_PARAMS )
{
  head->alignment = 3;
  if( alignment > 4 )
    head->alignment = alignment - 1;
  head->volumesize = volumesize;
  if( head->volumesize > (((size_t)1)<<31)-1 )
    head->volumesize = (((size_t)1)<<31)-1;
  head->volumeblocksize = ( sizeof(mmVolume) + head->alignment ) & ~head->alignment;
  head->minchunksize = ( ( minchunksize > sizeof(mmVolumeChunk) ? minchunksize : sizeof(mmVolumeChunk) ) + head->alignment ) & ~head->alignment;
  head->volumechunksize = ( sizeof(mmVolumeChunkHeader) + head->alignment ) & ~head->alignment;
  head->keepfreesize = keepfreesize;
  head->totalfreesize = 0;
  head->freeroot = 0;
  head->volumelist = 0;
  head->relayalloc = mmAlloc;
  head->relayfree = mmFree;
  head->relayvalue = 0;
  mtSpinInit( &head->spinlock );
  return;
}


void MM_FUNC(VolumeNodeInit)( mmVolumeHead *head, int nodeindex, size_t volumesize, size_t minchunksize, size_t keepfreesize, size_t alignment MM_PARAMS )
{
  MM_FUNC(VolumeInit)( head, volumesize, minchunksize, keepfreesize, alignment MM_PASSPARAMS );
  head->relayalloc = mmNodeRelayAlloc;
  head->relayfree = mmNodeRelayFree;
  head->relayvalue = (void *)((intptr_t)nodeindex);
  return;
}


/**
 * Private function to search for the best match given a desired memory chunk size.
 */
static mmVolumeChunk *mmVolumeFindFreeChunk( int32_t bytes, mmVolumeChunk *root )
{
  int32_t chunksize;
  mmVolumeChunk *best = 0;
  for( ; root ; )
  {
    chunksize = MM_VOLUME_CHUNK_GET_NEXTOFFSET( root );
    if( bytes <= chunksize )
    {
      best = root;
      if( bytes == chunksize )
        return best;
      root = root->node.child[0];
    }
    else
      root = root->node.child[1];
  }
  return best;
}


/**
 * Private function to add a new free chunk within the balanced binary tree.
 */
static void mmVolumeAddFreeChunk( mmVolumeChunk *chunk, void **rootfree )
{
  int32_t nextoffset = chunk->h.nextoffset;
  mmVolumeChunk *root = *rootfree;
  if( !( root ) )
  {
    mmBTreeInsert( chunk, 0, 0, offsetof(mmVolumeChunk,node), rootfree );
    return;
  }
  for( ; ; )
  {
    if( nextoffset < root->h.nextoffset )
    {
      if( root->node.child[0] )
      {
        root = root->node.child[0];
        continue;
      }
      mmBTreeInsert( chunk, root, MM_BTREE_FLAGS_LEFT, offsetof(mmVolumeChunk,node), rootfree );
      break;
    }
    else
    {
      if( root->node.child[1] )
      {
        root = root->node.child[1];
        continue;
      }
      mmBTreeInsert( chunk, root, MM_BTREE_FLAGS_RIGHT, offsetof(mmVolumeChunk,node), rootfree );
      break;
    }
  }
  return;
}



void mmVolumeDebugList( mmVolumeHead *volumehead )
{
  int32_t prevoffset, nextoffset, chunkflags, volumesize, nextcheck;
  mmVolume *volume;
  mmVolumeChunk *chunk;

  printf( "\n==== BEGIN ====\n" );
  fflush( stdout );

  for( volume = volumehead->volumelist ; volume ; volume = volume->list.next )
  {
    chunk = ADDRESS( volume, volumehead->volumeblocksize );

    volumesize = 0;
    nextcheck = 0;

    for( ; ; )
    {
      chunkflags = MM_VOLUME_CHUNK_GET_FLAGS( chunk );
      nextoffset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
      prevoffset = chunk->h.prevoffset;

      printf( "Chunk %p\n", chunk );
      printf( "  Prevoffset : %d\n", prevoffset );
      printf( "  Nextoffset : %d\n", nextoffset );
      printf( "  Chunkflags : %d\n", chunkflags );
      fflush( stdout );

      volumesize += nextoffset;
      if( ( nextcheck ) && ( prevoffset != nextcheck ) )
        exit( 1 );
      nextcheck = nextoffset;

      if( chunkflags & MM_VOLUME_CHUNK_FLAGS_LAST )
        break;

      chunk = ADDRESS( chunk, nextoffset );
    }

    printf( "Volume size : %d\n", volumesize );
    printf( "\n" );
  }

  printf( "==== END ====\n\n" );
  fflush( stdout );

  return;
}




/**
 * Allocate memory from a volume.
 *
 * The function allocates a chunk of memory of arbitrary size from the
 * specified volume manager.
 */
void *MM_FUNC(VolumeAlloc)( mmVolumeHead *head, size_t bytes MM_PARAMS )
{
  int32_t chunkflags, nextoffset;
  size_t allocsize, minsize, extrasize;
  intptr_t vmem;
  mmVolume *volume;
  mmVolumeChunk *chunk, *newchunk, *nextchunk;

  mtSpinLock( &head->spinlock );

  /* Big chunk handling */
  if( bytes >= MM_VOLUME_BIGCHUNK_THRESHOLD )
  {
    allocsize = bytes + ( head->volumeblocksize + head->volumechunksize );

    vmem = (intptr_t)head->relayalloc( head->relayvalue, allocsize MM_PASSPARAMS );
    volume = (void *)( ( vmem + head->alignment ) & ~head->alignment );
    volume->bigsize = allocsize;
    volume->alignment = (intptr_t)volume - vmem;
    mmListAdd( &head->volumelist, volume, offsetof(mmVolume,list) );

    chunk = ADDRESS( volume, head->volumeblocksize );
    chunk->h.prevoffset = 0;
    chunk->h.nextoffset = 0;

    mtSpinUnlock( &head->spinlock );
    return ADDRESS( chunk, head->volumechunksize );
  }

  /* Find best match among free chunks */
  bytes += head->volumechunksize;
  if( bytes < head->minchunksize )
    bytes = head->minchunksize;
  bytes = ( bytes + head->alignment ) & ~head->alignment;
  chunk = mmVolumeFindFreeChunk( bytes, head->freeroot );

  /* Allocate a new volume */
  if( !( chunk ) )
  {
    allocsize = head->volumesize;

    /* Allocate new volume and add to list */
    minsize = bytes + ( head->volumeblocksize + head->volumechunksize );
    if( minsize > allocsize )
      allocsize = minsize;

    head->totalfreesize += allocsize - ( head->volumeblocksize + head->volumechunksize );

    vmem = (intptr_t)head->relayalloc( head->relayvalue, allocsize MM_PASSPARAMS );
    volume = (void *)( ( vmem + head->alignment ) & ~head->alignment );
    volume->bigsize = 0;
    volume->alignment = (intptr_t)volume - vmem;
    mmListAdd( &head->volumelist, volume, offsetof(mmVolume,list) );

    /* Add a free chunk to cover the whole volume */
    chunk = ADDRESS( volume, head->volumeblocksize );

    chunk->h.prevoffset = 0;
    allocsize -= head->volumeblocksize + volume->alignment;
    MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS( chunk, allocsize, MM_VOLUME_CHUNK_FLAGS_FREE | MM_VOLUME_CHUNK_FLAGS_LAST );

    mmVolumeAddFreeChunk( chunk, &head->freeroot );
  }

  /* Remove best match of free chunk from btree */
  mmBTreeRemove( chunk, offsetof(mmVolumeChunk,node), &head->freeroot );
  chunkflags = MM_VOLUME_CHUNK_GET_FLAGS( chunk );
  MM_VOLUME_CHUNK_SET_FLAGS( chunk, chunkflags & MM_VOLUME_CHUNK_FLAGS_LAST );

  nextoffset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
  head->totalfreesize -= nextoffset - head->volumechunksize;
  extrasize = nextoffset - bytes;
  if( extrasize >= head->minchunksize )
  {
    /* Split and spawn a new free chunk */
    MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS( chunk, bytes, 0 );
    newchunk = ADDRESS( chunk, bytes );
    newchunk->h.prevoffset = bytes;
    MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS( newchunk, extrasize, MM_VOLUME_CHUNK_FLAGS_FREE | ( chunkflags & MM_VOLUME_CHUNK_FLAGS_LAST ) );
    if( !( chunkflags & MM_VOLUME_CHUNK_FLAGS_LAST ) )
    {
      nextchunk = ADDRESS( chunk, nextoffset );
      nextchunk->h.prevoffset = extrasize;
    }
    head->totalfreesize += extrasize - head->volumechunksize;
    mmVolumeAddFreeChunk( newchunk, &head->freeroot );
  }

  mtSpinUnlock( &head->spinlock );
  return ADDRESS( chunk, head->volumechunksize );
}


static mmVolumeChunk *mmVolumeMergeFree( mmVolumeHead *head, mmVolumeChunk *chunk )
{
  int32_t nextoffset, chunkflags, next2offset;
  mmVolumeChunk *prev, *next, *next2;

  prev = 0;
  if( chunk->h.prevoffset )
    prev = ADDRESS( chunk, -chunk->h.prevoffset );
  chunkflags = MM_VOLUME_CHUNK_GET_FLAGS( chunk );
  nextoffset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
  head->totalfreesize += nextoffset - head->volumechunksize;
  next = 0;
  if( !( chunkflags & MM_VOLUME_CHUNK_FLAGS_LAST ) )
    next = ADDRESS( chunk, nextoffset );
  if( ( prev ) && ( MM_VOLUME_CHUNK_GET_FLAGS( prev ) & MM_VOLUME_CHUNK_FLAGS_FREE ) )
  {
    /* Merge free space with previous chunk */
    mmBTreeRemove( prev, offsetof(mmVolumeChunk,node), &head->freeroot );
    prev->h.nextoffset += nextoffset;
    MM_VOLUME_CHUNK_SET_FLAGS( prev, chunkflags & MM_VOLUME_CHUNK_FLAGS_LAST );
    chunk = prev;
    head->totalfreesize += head->volumechunksize;
/*
    prev = 0;
    if( chunk->h.prevoffset )
      prev = ADDRESS( chunk, -chunk->h.prevoffset );
*/
    if( next )
    {
      next->h.prevoffset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
      if( MM_VOLUME_CHUNK_GET_FLAGS( next ) & MM_VOLUME_CHUNK_FLAGS_FREE )
        goto mergenext;
    }
  }
  else if( ( next ) && ( MM_VOLUME_CHUNK_GET_FLAGS( next ) & MM_VOLUME_CHUNK_FLAGS_FREE ) )
  {
    /* Merge free space with next chunk */
    mergenext:
    mmBTreeRemove( next, offsetof(mmVolumeChunk,node), &head->freeroot );
    next2offset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( next );
    chunk->h.nextoffset += next2offset;
    head->totalfreesize += head->volumechunksize;
    if( MM_VOLUME_CHUNK_GET_FLAGS( next ) & MM_VOLUME_CHUNK_FLAGS_LAST )
      MM_VOLUME_CHUNK_SET_FLAGS( chunk, MM_VOLUME_CHUNK_FLAGS_FREE | MM_VOLUME_CHUNK_FLAGS_LAST );
    else
    {
      MM_VOLUME_CHUNK_SET_FLAGS( chunk, MM_VOLUME_CHUNK_FLAGS_FREE );
      next2 = ADDRESS( next, next2offset );
      next2->h.prevoffset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
    }
  }
  else
  {
    /* Solitary free chunk */
    MM_VOLUME_CHUNK_SET_FLAGS( chunk, chunkflags | MM_VOLUME_CHUNK_FLAGS_FREE );
  }

  return chunk;
}


void MM_FUNC(VolumeRelease)( mmVolumeHead *head, void *v MM_PARAMS )
{
  mmVolumeChunk *chunk;
  mmVolume *volume;

  chunk = ADDRESS( v, -head->volumechunksize );
  mtSpinLock( &head->spinlock );

  /* Big chunk handling */
  if( !( chunk->h.nextoffset ) )
  {
    volume = ADDRESS( chunk, -head->volumeblocksize );
    mmListRemove( volume, offsetof(mmVolume,list) );
    head->relayfree( head->relayvalue, ADDRESS( volume, -volume->alignment ), volume->bigsize MM_PASSPARAMS );
    mtSpinUnlock( &head->spinlock );
    return;
  }

  chunk = mmVolumeMergeFree( head, chunk );

  /* Register the new free chunk */
  mmVolumeAddFreeChunk( chunk, &head->freeroot );

  mtSpinUnlock( &head->spinlock );
  return;
}


/**
 * Free a chunk of memory that was allocated by mmVolumeAlloc().
 *
 * The flags parameter may contain the flag MM_VOLUME_FLAGS_RELEASE, which
 * instructs the volume manager not to free unused memory, improving
 * performance if the blocks are to be soon reused.
 */
void MM_FUNC(VolumeFree)( mmVolumeHead *head, void *v MM_PARAMS )
{
  size_t totalfreesize;
  mmVolumeChunk *chunk;
  mmVolume *volume;

  chunk = ADDRESS( v, -head->volumechunksize );
  mtSpinLock( &head->spinlock );

  /* Big chunk handling */
  if( !( chunk->h.nextoffset ) )
  {
    volume = ADDRESS( chunk, -head->volumeblocksize );
    mmListRemove( volume, offsetof(mmVolume,list) );
    head->relayfree( head->relayvalue, ADDRESS( volume, -volume->alignment ), volume->bigsize MM_PASSPARAMS );
    mtSpinUnlock( &head->spinlock );
    return;
  }

  chunk = mmVolumeMergeFree( head, chunk );

  totalfreesize = head->totalfreesize - ( MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk ) - head->volumechunksize );

  /* If our free chunk is alone in there, free the whole volume */
  if( ( MM_VOLUME_CHUNK_GET_FLAGS( chunk ) & MM_VOLUME_CHUNK_FLAGS_LAST ) && !( chunk->h.prevoffset ) && ( totalfreesize >= head->keepfreesize ) )
  {
    head->totalfreesize = totalfreesize;
    volume = ADDRESS( chunk, -head->volumeblocksize );
    mmListRemove( volume, offsetof(mmVolume,list) );
    head->relayfree( head->relayvalue, ADDRESS( volume, -volume->alignment ), MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk ) MM_PASSPARAMS );
    mtSpinUnlock( &head->spinlock );
    return;
  }

  /* Register the new free chunk */
  mmVolumeAddFreeChunk( chunk, &head->freeroot );

  mtSpinUnlock( &head->spinlock );
  return;
}




/**
 * Shrink a chunk of memory that was allocated by mmVolumeAlloc().
 *
 * The bytes parameter defines the new size of the chunk of memory. The
 * specified pointer remains valid as the chunk is never relocated elsewhere.
 * This function can not be used to attempt to increase the size of a chunk.
 */
void MM_FUNC(VolumeShrink)( mmVolumeHead *head, void *v, size_t bytes MM_PARAMS )
{
  int32_t nextoffset, newoffset, chunkflags, nextflags, next2offset;
  size_t freesize;
  mmVolumeChunk *chunk, *next, *next2, *newchunk;

  chunk = ADDRESS( v, -head->volumechunksize );
  mtSpinLock( &head->spinlock );
  next = 0;
  chunkflags = MM_VOLUME_CHUNK_GET_FLAGS( chunk );
  nextoffset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
  if( !( chunkflags & MM_VOLUME_CHUNK_FLAGS_LAST ) )
  {
    next = ADDRESS( chunk, nextoffset );
    nextflags = MM_VOLUME_CHUNK_GET_FLAGS( next );
    next2offset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( next );
  }

  bytes += head->volumechunksize;
  if( bytes < head->minchunksize )
    bytes = head->minchunksize;
  bytes = ( bytes + head->alignment ) & ~head->alignment;
  freesize = nextoffset - bytes;
  if( ( freesize < head->minchunksize ) && ( !( next ) || !( nextflags & MM_VOLUME_CHUNK_FLAGS_FREE ) ) )
  {
    mtSpinUnlock( &head->spinlock );
    return;
  }
  newoffset = bytes;

  newchunk = ADDRESS( chunk, newoffset );
  MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS( chunk, newoffset, 0 );
  newchunk->h.prevoffset = newoffset;
  head->totalfreesize += freesize - head->volumechunksize;
  if( !( next ) )
    MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS( newchunk, freesize, MM_VOLUME_CHUNK_FLAGS_FREE | MM_VOLUME_CHUNK_FLAGS_LAST );
  else
  {
    MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS( newchunk, freesize, MM_VOLUME_CHUNK_FLAGS_FREE );
    if( !( nextflags & MM_VOLUME_CHUNK_FLAGS_FREE ) )
      next->h.prevoffset = freesize;
    else
    {
      mmBTreeRemove( next, offsetof(mmVolumeChunk,node), &head->freeroot );
      MM_VOLUME_CHUNK_SET_NEXTOFFSET_AND_FLAGS( newchunk, freesize + next2offset, nextflags );
      if( !( nextflags & MM_VOLUME_CHUNK_FLAGS_LAST ) )
      {
        next2 = ADDRESS( next, next2offset );
        next2->h.prevoffset = freesize + next2offset;
      }
    }
  }
  mmVolumeAddFreeChunk( newchunk, &head->freeroot );

  mtSpinUnlock( &head->spinlock );
  return;
}


size_t MM_FUNC(VolumeGetAllocSize)( mmVolumeHead *head, void *v )
{
  mmVolumeChunk *chunk;
  chunk = ADDRESS( v, -head->volumechunksize );
  return MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk ) - head->volumechunksize;
}


/**
 * Inspect a volume manager and free unused pages of memory.
 *
 * This function may only free memory if MM_VOLUME_FLAGS_RELEASE was used when
 * freeing chunks of memory allocated by the manager.
 */
void MM_FUNC(VolumeClean)( mmVolumeHead *head MM_PARAMS )
{
  mmVolume *volume, *next;
  mmVolumeChunk *chunk;
  mtSpinLock( &head->spinlock );
  for( volume = head->volumelist ; volume ; volume = next )
  {
    next = volume->list.next;
    chunk = ADDRESS( volume, head->volumeblocksize );
    if( ( MM_VOLUME_CHUNK_GET_FLAGS(chunk) & MM_VOLUME_CHUNK_FLAGS_LAST ) && !( chunk->h.prevoffset ) )
    {
      head->totalfreesize -= MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
      mmListRemove( volume, offsetof(mmVolume,list) );
      head->relayfree( head->relayvalue, ADDRESS( volume, -volume->alignment ), MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk ) MM_PASSPARAMS );
    }
  }
  mtSpinUnlock( &head->spinlock );
  return;
}


/**
 * Free all memory allocated by the memory volume manager.
 */
void MM_FUNC(VolumeFreeAll)( mmVolumeHead *head MM_PARAMS )
{
  int32_t nextoffset;
  size_t volumesize;
  mmVolume *volume, *next;
  mmVolumeChunk *chunk;
  mtSpinLock( &head->spinlock );
  for( volume = head->volumelist ; volume ; volume = next )
  {
    next = volume->list.next;
    chunk = ADDRESS( volume, head->volumeblocksize );
    volumesize = volume->bigsize;
    if( chunk->h.nextoffset )
    {
      volumesize = 0;
      for( ; ; )
      {
        nextoffset = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk );
        volumesize += nextoffset;
        if( MM_VOLUME_CHUNK_GET_FLAGS( chunk ) & MM_VOLUME_CHUNK_FLAGS_LAST )
          break;
        chunk = ADDRESS( chunk, nextoffset );
      }
    }
    head->relayfree( head->relayvalue, ADDRESS( volume, -volume->alignment ), volumesize MM_PASSPARAMS );
  }
  mtSpinUnlock( &head->spinlock );
  mtSpinDestroy( &head->spinlock );
  return;
}


void *MM_FUNC(VolumeRealloc)( mmVolumeHead *head, void *v, size_t bytes MM_PARAMS )
{
  size_t chunksize;
  void *newv;
  mmVolumeChunk *chunk;
  if( !( v ) )
    return MM_FUNC(VolumeAlloc)( head, bytes MM_PASSPARAMS );
  chunk = ADDRESS( v, -head->volumechunksize );
  chunksize = MM_VOLUME_CHUNK_GET_NEXTOFFSET( chunk ) - head->volumechunksize;
  if( bytes < chunksize )
  {
    MM_FUNC(VolumeShrink)( head, v, bytes MM_PASSPARAMS );
    return v;
  }
  newv = MM_FUNC(VolumeAlloc)( head, bytes MM_PASSPARAMS );
  memcpy( newv, v, chunksize );
  MM_FUNC(VolumeFree)( head, v MM_PASSPARAMS );
  return newv;
}



////


#ifdef MM_ZONE_SUPPORT


typedef struct
{
  int32_t flags;
  void *next, *prev;
} mmZoneChunk;

#define MM_ZONE_CHUNK_FLAGS_FREE (0x1)
#define MM_ZONE_CHUNK_FLAGS_LAST (0x2)


int MM_FUNC(ZoneInit)( mmZoneHead *head, size_t zonesize, intptr_t alignment MM_PARAMS )
{
  mmZoneChunk *chunk;

  memset( head, 0, sizeof(mmZoneHead) );
  mtSpinInit( &head->spinlock );
  head->pagesize = sysconf( _SC_PAGESIZE ) - 1;
  head->pagealignment = head->pagesize - 1;
  zonesize = ( zonesize + head->pagealignment ) & ~head->pagealignment;
  head->zonesize = zonesize;
  mtSpinLock( &head->spinlock );

  head->address = mmap( 0x0, head->zonesize, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
  if( head->address == MAP_FAILED )
  {
#ifdef MAP_NORESERVE
    head->address = mmap( 0x0, head->zonesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0 );
    if( head->address == MAP_FAILED )
      return 0;
#else
    return 0;
#endif
  }

  head->alignment = 3;
  if( alignment > 4 )
    head->alignment = alignment - 1;
  head->chunkheadersize = ( sizeof(mmZoneChunk) + head->alignment ) & ~head->alignment;

  mprotect( head->address, head->pagesize, PROT_READ | PROT_WRITE );
  madvise( head->address, head->pagesize, MADV_NORMAL );
  chunk = head->address;
  chunk->flags = MM_ZONE_CHUNK_FLAGS_FREE | MM_ZONE_CHUNK_FLAGS_LAST;
  chunk->prev = 0;
  chunk->next = ADDRESS( chunk, head->zonesize );
  head->chunklist = chunk;

  mtSpinUnlock( &head->spinlock );
  return 1;
}


void *MM_FUNC(ZoneAlloc)( mmZoneHead *head, size_t bytes MM_PARAMS )
{
  int32_t lastflag, chunkflags;
  mmZoneChunk *chunk, *newchunk, *nextchunk;
  size_t chunknewsize, chunksize, unlocksize;

  bytes += head->chunkheadersize;
  bytes = ( bytes + head->alignment ) & ~head->alignment;

  lastflag = 0;
  for( chunk = head->chunklist ; ; chunk = chunk->next )
  {
    if( lastflag )
    {
      /* Our zone runs out of space? This is bad. Bad bad bad. */
      printf( "CRITICAL : Memory zone exhausted all virtual mapping space!\n" );
      return 0;
    }
    lastflag = chunk->flags & MM_ZONE_CHUNK_FLAGS_LAST;
    if( !( chunk->flags & MM_ZONE_CHUNK_FLAGS_FREE ) )
      continue;
    chunksize = ADDRESSDIFF( chunk->next, chunk );
    if( chunksize >= bytes )
      break;
  }

  chunkflags = chunk->flags;
  chunknewsize = ( bytes + head->pagealignment ) & ~head->pagealignment;
  if( ( chunksize - chunknewsize ) >= head->pagesize )
  {
    chunk->flags = 0;
    newchunk = ADDRESS( chunk, chunknewsize );
    unlocksize = chunknewsize + head->chunkheadersize;
    mprotect( chunk, unlocksize, PROT_READ | PROT_WRITE );
    madvise( chunk, unlocksize, MADV_NORMAL );
    newchunk->flags = chunkflags;
    newchunk->prev = chunk;
    newchunk->next = chunk->next;
    if( !( chunkflags & MM_ZONE_CHUNK_FLAGS_LAST ) )
    {
      nextchunk = chunk->next;
      nextchunk->prev = newchunk;
    }
    chunk->next = newchunk;
  }
  else
  {
    chunk->flags &= ~MM_ZONE_CHUNK_FLAGS_FREE;
    mprotect( chunk, chunknewsize, PROT_READ | PROT_WRITE );
    madvise( chunk, chunknewsize, MADV_NORMAL );
  }

  return ADDRESS( chunk, head->chunkheadersize );
}


void MM_FUNC(ZoneFree)( mmZoneHead *head, void *v MM_PARAMS )
{
  mmZoneChunk *chunk, *prevchunk, *nextchunk, *next2chunk;
  size_t extrasize;
  void *page;

  chunk = ADDRESS( v, -head->chunkheadersize );
  page = (void *)( ( (intptr_t)v + head->pagealignment ) & ~head->pagealignment );
  extrasize = ADDRESSDIFF( chunk->next, page );
  mprotect( page, extrasize, PROT_READ );
  madvise( page, extrasize, MADV_DONTNEED );

  chunk->flags |= MM_ZONE_CHUNK_FLAGS_FREE;
  prevchunk = chunk->prev;
  nextchunk = 0;
  if( !( chunk->flags & MM_ZONE_CHUNK_FLAGS_LAST ) )
    nextchunk = chunk->next;

  if( ( nextchunk ) && ( nextchunk->flags & MM_ZONE_CHUNK_FLAGS_FREE ) )
  {
    chunk->next = nextchunk->next;
    chunk->flags |= nextchunk->flags & MM_ZONE_CHUNK_FLAGS_LAST;
    next2chunk = 0;
    if( !( nextchunk->flags & MM_ZONE_CHUNK_FLAGS_LAST ) )
    {
      next2chunk = nextchunk->next;
      next2chunk->prev = chunk;
    }
    mprotect( nextchunk, head->pagesize, PROT_READ );
    madvise( nextchunk, head->pagesize, MADV_DONTNEED );
    nextchunk = 0;
    if( !( chunk->flags & MM_ZONE_CHUNK_FLAGS_LAST ) )
      nextchunk = chunk->next;
  }

  if( ( prevchunk ) && ( prevchunk->flags & MM_ZONE_CHUNK_FLAGS_FREE ) )
  {
    prevchunk->next = chunk->next;
    if( nextchunk )
      nextchunk->prev = prevchunk;
    mprotect( chunk, head->pagesize, PROT_READ );
    madvise( chunk, head->pagesize, MADV_DONTNEED );
  }

  return;
}


void MM_FUNC(ZoneFreeAll)( mmZoneHead *head MM_PARAMS )
{
  mtSpinLock( &head->spinlock );
  munmap( head->address, head->zonesize );
  mtSpinUnlock( &head->spinlock );
  return;
}


#endif



////



#if MM_DEBUG

#undef malloc
#undef free
#undef realloc

#endif

#undef mmDebugAlloc
#undef mmDebugFree
#undef mmDebugRealloc
#undef mmListUses


typedef struct
{
  char *file;
  int line;
  size_t size;
  size_t count;
  mmListNode list;
#if MM_DEBUG_GUARD_BYTES
  char guard[MM_DEBUG_GUARD_BYTES];
#endif
} mmChunk;

static void *mmChunkList = 0;
static size_t mmCount = 0;
static size_t mmMaxCount = 0;


void *mmDebugAlloc( size_t bytes, char *file, int line )
{
#if MM_DEBUG_GUARD_BYTES
  int guard;
  char *endguard;
#endif
  mmChunk *chunk;
  mtMutexLock( &mmGlobalMutex );
#if MM_DEBUG_MMAP
  if( !( chunk = mmap( 0, sizeof(mmChunk) + bytes + MM_DEBUG_GUARD_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 ) ) )
#else
  if( !( chunk = malloc( sizeof(mmChunk) + bytes + MM_DEBUG_GUARD_BYTES ) ) )
#endif
  {
#if MM_WINDOWS
    fprintf( stderr, "FATAL : Denied memory allocation ( %ld ) at %s:%d !\nExiting\n", (long)bytes, file, line );
#else
    fprintf( stderr, "FATAL : Denied memory allocation ( %lld ) at %s:%d !\nExiting\n", (long long)bytes, file, line );
#endif
    exit( 1 );
  }
  mmListAdd( &mmChunkList, chunk, offsetof(mmChunk,list) );
  chunk->file = file;
  chunk->line = line;
  chunk->size = bytes;
#if MM_DEBUG_GUARD_BYTES
  endguard = ADDRESS( chunk, sizeof(mmChunk) + bytes );
  for( guard = 0 ; guard < MM_DEBUG_GUARD_BYTES ; guard++ )
  {
    chunk->guard[guard] = 0x55;
    endguard[guard] = 0x75;
  }
#endif
  mmCount += bytes;
  if( mmCount > mmMaxCount )
    mmMaxCount = mmCount;
  mtMutexUnlock( &mmGlobalMutex );
  return ADDRESS( chunk, sizeof(mmChunk) );
}


void *mmDebugRealloc( void *v, size_t bytes, char *file, int line )
{
  size_t size;
  mmChunk *chunk;
  void *newchunk = 0;
  newchunk = mmDebugAlloc( bytes, file, line );
  if( v )
  {
    size = bytes;
    chunk = ADDRESS( v, -sizeof(mmChunk) );
    if( chunk->size < size )
      size = chunk->size;
    if( size )
      memcpy( newchunk, v, size );
    mmDebugFree( v, file, line );
  }
  return newchunk;
}


void mmDebugFree( void *v, char *file, int line )
{
  mmChunk *chunk;
#if MM_DEBUG_GUARD_BYTES
  int guard;
  char *endguard;
#endif
  chunk = ADDRESS( v, -sizeof(mmChunk) );
#if MM_DEBUG_GUARD_BYTES
  endguard = ADDRESS( chunk, sizeof(mmChunk) + chunk->size );
  for( guard = 0 ; guard < MM_DEBUG_GUARD_BYTES ; guard++ )
  {
    if( chunk->guard[guard] != 0x55 )
    {
      fprintf( stderr, "MALLOC START[%d] GUARD ERROR : Corruption of %s:%d\n", guard, chunk->file, chunk->line );
      exit( 1 );
    }
    if( endguard[guard] != 0x75 )
    {
      fprintf( stderr, "MALLOC END[%d] GUARD ERROR : Corruption of %s:%d\n", guard, chunk->file, chunk->line );
      exit( 1 );
    }
  }
#endif
  mtMutexLock( &mmGlobalMutex );
  mmListRemove( chunk, offsetof(mmChunk,list) );
  mmCount -= chunk->size;
#if MM_DEBUG_MMAP
 #if MM_DEBUG_MMAP_LINGERING
  mprotect( chunk, sizeof(mmChunk) + chunk->size + MM_DEBUG_GUARD_BYTES, PROT_NONE );
 #else
  munmap( chunk, sizeof(mmChunk) + chunk->size + MM_DEBUG_GUARD_BYTES );
 #endif
#else
  free( chunk );
#endif
  mtMutexUnlock( &mmGlobalMutex );
  return;
}


void *mmAlloc( void *unused, size_t bytes MM_PARAMS )
{
#if MM_DEBUG
  return mmDebugAlloc( bytes MM_PASSPARAMS );
#else
  void *chunk;
 #if MM_WINDOWS
  if( !( chunk = malloc( bytes ) ) )
    fprintf( stderr, "WARNING : Denied memory allocation ( %ld )!\nExiting\n", (long)bytes );
 #else
  if( !( chunk = malloc( bytes ) ) )
    fprintf( stderr, "WARNING : Denied memory allocation ( %lld )!\nExiting\n", (long long)bytes );
 #endif
  return chunk;
#endif
}


void *mmRealloc( void *unused, void *v, size_t bytes MM_PARAMS )
{
#if MM_DEBUG
  return mmDebugRealloc( v, bytes MM_PASSPARAMS );
#else
 #if MM_WINDOWS
  if( !( v = realloc( v, bytes ) ) && ( bytes ) )
    fprintf( stderr, "WARNING : Denied memory reallocation ( %ld )!\nExiting\n", (long)bytes );
 #else
  if( !( v = realloc( v, bytes ) ) && ( bytes ) )
    fprintf( stderr, "WARNING : Denied memory reallocation ( %lld )!\nExiting\n", (long long)bytes );
 #endif
  return v;
#endif
}


void mmFree( void *unused, void *v, size_t bytes MM_PARAMS )
{
#if MM_DEBUG
  mmDebugFree( v MM_PASSPARAMS );
#else
  free( v );
#endif
  return;
}



/**
 * Private function to add the sorted memory item in the linked list.
 */
static void mmListSortAdd( mmChunk *chunk, void **sortlist )
{
  intptr_t i;
  char *s0, *s1;
  mmChunk *sortchunk;
  s0 = chunk->file;
  for( ; *sortlist ; sortlist = &sortchunk->list.next )
  {
    sortchunk = *sortlist;
    s1 = sortchunk->file;
    for( i = 0 ; ; i++ )
    {
      if( s0[i] < s1[i] )
        goto insert;
      if( s0[i] > s1[i] )
        break;
      if( s1[i] )
        continue;
      if( chunk->line > sortchunk->line )
        break;
      goto insert;
    }
  }
  insert:
  mmListAdd( sortlist, chunk, offsetof(mmChunk,list) );
  return;
}


/**
 * List all memory allocations at any time and print the results on stdout.
 */
void mmListUses( char *file, int line )
{
  mmChunk *chunk;
  mmChunk *chunksort;
  void *sortlist;
  size_t totalsize;
  mtMutexLock( &mmGlobalMutex );
  printf( "-- Memory allocation listing at %s:%d --\n", file, line );
  if( !( mmChunkList ) )
  {
    printf( " List empty\n\n" );
    mtMutexUnlock( &mmGlobalMutex );
    return;
  }
  sortlist = 0;
  for( chunk = mmChunkList ; chunk ; chunk = chunk->list.next )
  {
    for( chunksort = sortlist ; chunksort ; chunksort = chunksort->list.next )
    {
      if( chunk->file != chunksort->file )
        continue;
      if( chunk->line != chunksort->line )
        continue;
      goto found;
    }
    if( !( chunksort = malloc( sizeof(mmChunk) ) ) )
    {
      mtMutexUnlock( &mmGlobalMutex );
      return;
    }
    memset( chunksort, 0, sizeof(mmChunk) );
    chunksort->file = chunk->file;
    chunksort->line = chunk->line;
    chunksort->count = 0;
    mmListSortAdd( chunksort, &sortlist );
    found:
    chunksort->size += chunk->size;
    chunksort->count++;
  }
  totalsize = 0;
  for( chunksort = sortlist ; chunksort ; chunksort = chunk )
  {
    printf( " %10ld bytes  in  %6ld chunk(s)  allocated at  %s:%d\n", (long)chunksort->size, (long)chunksort->count, chunksort->file, chunksort->line );
    totalsize += chunksort->size;
    chunk = chunksort->list.next;
    free( chunksort );
  }
#if MM_WINDOWS
  printf( " %10ld bytes total\n", (long)totalsize );
  printf( " %10ld bytes maximum reached\n\n", (long)mmMaxCount );
#else
  printf( " %10lld bytes total\n", (long long)totalsize );
  printf( " %10lld bytes maximum reached\n\n", (long long)mmMaxCount );
#endif
  mtMutexUnlock( &mmGlobalMutex );
  return;
}



////



#ifdef __WIN32__

#include <time.h>
#include <windows.h>

#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL

int mmGetTimeOfDay( struct timeval *tv )
{
  FILETIME ft;
  uint64_t curtime;
  if( tv )
  {
    GetSystemTimeAsFileTime( &ft );
    curtime = ft.dwHighDateTime;
    curtime <<= 32;
    curtime |= ft.dwLowDateTime;
    curtime /= 10;
    curtime -= DELTA_EPOCH_IN_MICROSECS;
    tv->tv_sec = (long)( curtime / 1000000UL );
    tv->tv_usec = (long)( curtime % 1000000UL );
  }
  return 0;
}

#endif



