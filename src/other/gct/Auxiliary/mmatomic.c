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

#include "cpuconfig.h"
#include "mm.h"


void mmAtomicListAdd( mmAtomicP *list, void *item, intptr_t offset )
{
  mmAtomicListNode *node, *nextnode;
  void *nextitem;

  node = ADDRESS( item, offset );
  mmAtomicWriteP( &node->prev, list );
  for( ; ; )
  {
    /* Mark prev->next as busy */
    nextitem = mmAtomicReadP( list );
    if( ( nextitem == MM_ATOMIC_LIST_BUSY ) || !( mmAtomicCmpReplaceP( list, nextitem, MM_ATOMIC_LIST_BUSY ) ) )
      continue;
    mmAtomicWriteP( &node->next, nextitem );
    if( nextitem )
    {
      nextnode = ADDRESS( nextitem, offset );
      /* Set next->prev to node->next */
      if( !( mmAtomicCmpReplaceP( &nextnode->prev, list, &(node->next) ) ) )
      {
        mmAtomicWriteP( list, nextitem );
        continue;
      }
    }
    mmAtomicWrite32( &node->status, MM_ATOMIC_LIST_VALID );
    /* Finally set prev->next to point to the item */
    mmAtomicWriteP( list, item );
    break;
  }

  return;
}


void mmAtomicListRemove( void *item, intptr_t offset )
{
  mmAtomicListNode *node, *nextnode;
  mmAtomicP *prevnext;
  void *nextitem;

  node = ADDRESS( item, offset );
  if( !( mmAtomicCmpReplace32( &node->status, MM_ATOMIC_LIST_VALID, MM_ATOMIC_LIST_DELETED ) ) )
    return;
  for( ; ; )
  {
    /* Mark item->next as busy */
    nextitem = mmAtomicReadP( &node->next );
    if( ( nextitem == MM_ATOMIC_LIST_BUSY ) || !( mmAtomicCmpReplaceP( &node->next, nextitem, MM_ATOMIC_LIST_BUSY ) ) )
      continue;

    /* Mark item->prev as busy */
    prevnext = mmAtomicReadP( &node->prev );
    if( ( prevnext == MM_ATOMIC_LIST_BUSY ) || !( mmAtomicCmpReplaceP( &node->prev, prevnext, MM_ATOMIC_LIST_BUSY ) ) )
    {
      mmAtomicWriteP( &node->next, nextitem );
      continue;
    }

    /* Mark prev->next as busy */
    if( !( mmAtomicCmpReplaceP( prevnext, item, MM_ATOMIC_LIST_BUSY ) ) )
    {
      mmAtomicWriteP( &node->next, nextitem );
      mmAtomicWriteP( &node->prev, prevnext );
      continue;
    }

    /* Fix next item */
    if( nextitem )
    {
      nextnode = ADDRESS( nextitem, offset );
      if( !( mmAtomicCmpReplaceP( &nextnode->prev, &node->next, prevnext ) ) )
      {
        mmAtomicWriteP( &node->next, nextitem );
        mmAtomicWriteP( &node->prev, prevnext );
        mmAtomicWriteP( prevnext, item );
        continue;
      }
    }

    /* Finally set prev->next to point to the next item */
    mmAtomicWriteP( prevnext, nextitem );
    break;
  }

  return;
}





void mmAtomicListDualInit( mmAtomicListDualHead *head )
{
  mmAtomicWriteP( &head->first, 0 );
  mmAtomicWriteP( &head->last, &head->first );
  return;
}


void mmAtomicListDualAddFirst( mmAtomicListDualHead *head, void *item, intptr_t offset )
{
  mmAtomicListNode *node, *nextnode;
  mmAtomicP *nextprev;
  void *nextitem;

  node = ADDRESS( item, offset );
  mmAtomicWriteP( &node->prev, &head->first );
  for( ; ; )
  {
    /* Mark prev->next as busy */
    nextitem = mmAtomicReadP( &head->first );
    if( ( nextitem == MM_ATOMIC_LIST_BUSY ) || !( mmAtomicCmpReplaceP( &head->first, nextitem, MM_ATOMIC_LIST_BUSY ) ) )
      continue;
    mmAtomicWriteP( &node->next, nextitem );

    nextprev = &head->last;
    if( nextitem )
    {
      nextnode = ADDRESS( nextitem, offset );
      nextprev = &nextnode->prev;
    }

    /* Set next->prev to node->next */
    if( !( mmAtomicCmpReplaceP( nextprev, &head->first, &(node->next) ) ) )
    {
      mmAtomicWriteP( &head->first, nextitem );
      continue;
    }

    mmAtomicWrite32( &node->status, MM_ATOMIC_LIST_VALID );

    /* Finally set prev->next to point to the item */
    mmAtomicWriteP( &head->first, item );
    break;
  }

  return;
}


void mmAtomicListDualAddLast( mmAtomicListDualHead *head, void *item, intptr_t offset )
{
  mmAtomicListNode *node;
  mmAtomicP *lastnext;

  node = ADDRESS( item, offset );
  mmAtomicWriteP( &node->next, 0 );
  for( ; ; )
  {
    /* Mark head->last as busy */
    lastnext = mmAtomicReadP( &head->last );
    if( ( lastnext == MM_ATOMIC_LIST_BUSY ) || !( mmAtomicCmpReplaceP( &head->last, lastnext, MM_ATOMIC_LIST_BUSY ) ) )
      continue;
    mmAtomicWriteP( &node->prev, lastnext );

    /* Mark last->next as busy */
    if( !( mmAtomicCmpReplaceP( lastnext, 0, MM_ATOMIC_LIST_BUSY ) ) )
    {
      mmAtomicWriteP( &head->last, lastnext );
      continue;
    }

    /* Set head->first to item if the list is empty */
/*
TODO TODO

    firstitem = mmAtomicReadP( &head->first );
    if( ( firstitem == MM_ATOMIC_LIST_BUSY ) || ( !( firstitem ) && !( mmAtomicCmpReplaceP( &head->last, MM_ATOMIC_LIST_BUSY, &(node->next) ) ) ) )
    {
      mmAtomicWriteP( &head->last, lastnext );
      mmAtomicWriteP( lastnext, 0 );
      continue;
    }
*/

    mmAtomicWrite32( &node->status, MM_ATOMIC_LIST_VALID );

    /* Set prev->next to item */
    mmAtomicWriteP( lastnext, item );

    /* Finally set head->last to point to node->next */
    mmAtomicWriteP( &head->last, &(node->next) );
    break;
  }

  return;
}


void mmAtomicListDualRemove( mmAtomicListDualHead *head, void *item, intptr_t offset )
{
  mmAtomicListNode *node, *nextnode;
  mmAtomicP *prevnext, *nextprev;
  void *nextitem;

  node = ADDRESS( item, offset );
  if( !( mmAtomicCmpReplace32( &node->status, MM_ATOMIC_LIST_VALID, MM_ATOMIC_LIST_DELETED ) ) )
    return;
  for( ; ; )
  {
    /* Mark item->next as busy */
    nextitem = mmAtomicReadP( &node->next );
    if( ( nextitem == MM_ATOMIC_LIST_BUSY ) || !( mmAtomicCmpReplaceP( &node->next, nextitem, MM_ATOMIC_LIST_BUSY ) ) )
      continue;

    /* Mark item->prev as busy */
    prevnext = mmAtomicReadP( &node->prev );
    if( ( prevnext == MM_ATOMIC_LIST_BUSY ) || !( mmAtomicCmpReplaceP( &node->prev, prevnext, MM_ATOMIC_LIST_BUSY ) ) )
    {
      mmAtomicWriteP( &node->next, nextitem );
      continue;
    }

    /* Mark prev->next as busy */
    if( !( mmAtomicCmpReplaceP( prevnext, item, MM_ATOMIC_LIST_BUSY ) ) )
    {
      mmAtomicWriteP( &node->next, nextitem );
      mmAtomicWriteP( &node->prev, prevnext );
      continue;
    }

    /* Fix next item */
    nextprev = &head->last;
    if( nextitem )
    {
      nextnode = ADDRESS( nextitem, offset );
      nextprev = &nextnode->prev;
    }
    if( !( mmAtomicCmpReplaceP( nextprev, &node->next, prevnext ) ) )
    {
      mmAtomicWriteP( &node->next, nextitem );
      mmAtomicWriteP( &node->prev, prevnext );
      mmAtomicWriteP( prevnext, item );
      continue;
    }

    /* Finally set prev->next to point to the next item */
    mmAtomicWriteP( prevnext, nextitem );
    break;
  }

  return;
}



////



void mmAtomicBarrierBuild( mmAtomicBarrier *barrier, int childcount, mmAtomicBarrier *parent )
{
  mmAtomicWrite32( &barrier->flag, 0 );
  barrier->flagref = 0;
  barrier->resetvalue = childcount;
  mmAtomicWrite32( &barrier->counter, childcount );
  barrier->parent = parent;
  return;
}



#ifdef CPUCONF_STORE_REODERING_AFTER_STORE
 #ifndef MM_ATOMIC_BARRIER_DELAYED_RESET
  #undef MM_ATOMIC_BARRIER_DELAYED_RESET
 #endif
#endif

#define MM_ATOMIC_BARRIER_NEXTFLAGREF(x) (x+1)

/*
int fooooooo = 0;
*/

int mmAtomicBarrierWait( mmAtomicBarrier *barrier, int32_t spinwaitcounter, mmAtomicBarrierStat *barrierstat )
{
  int32_t flagref, nextflagref, spincountdown;
  mmAtomicBarrier *leaf;

  leaf = barrier;
  flagref = barrier->flagref;

#ifdef MM_ATOMIC_BARRIER_DEBUG
  if( mmAtomicRead32( &barrier->flag ) != flagref )
    printf( "Error, %d != %d\n", mmAtomicRead32( &barrier->flag ), flagref );
#endif

  nextflagref = MM_ATOMIC_BARRIER_NEXTFLAGREF( flagref );
  for( ; ; )
  {
    if( !( mmAtomicAddTestZero32( &barrier->counter, -1 ) ) )
      break;

#ifndef MM_ATOMIC_BARRIER_DELAYED_RESET
    barrier->flagref = nextflagref;
    MM_ATOMIC_ACCESS_32(&barrier->counter) += barrier->resetvalue;
#endif

    if( barrier->parent )
      barrier = barrier->parent;
    else
    {
      /* Barrier fully cleared */
      barrierstat->clearcounter++;

#ifndef MM_ATOMIC_BARRIER_DELAYED_RESET
 #ifdef CPUCONF_STORE_REODERING_AFTER_STORE
      mmWriteBarrier();
 #endif
      for( barrier = leaf ; barrier ; barrier = barrier->parent )
        mmAtomicWrite32( &barrier->flag, nextflagref );
#else
      for( barrier = leaf ; barrier ; barrier = barrier->parent )
      {
        barrier->flagref = nextflagref;
        MM_ATOMIC_ACCESS_32(&barrier->counter) += barrier->resetvalue;
        /* No barrier : earlier, we disallowed delayed reset if CPUCONF_STORE_REODERING_AFTER_STORE */
        mmAtomicWrite32( &barrier->flag, nextflagref );
      }
#endif

      return 1;
    }
  }


  /* Spin! */
  if( !( spinwaitcounter ) )
    goto spinyield;
  if( !( spincountdown = mmAtomicSpinWaitNeq32Count( &barrier->flag, flagref, spinwaitcounter ) ) )
  {
    barrierstat->yieldcounter++;
    spinyield:


/*
printf( "YIELD %d\n", fooooooo++ );
*/


    for( ; mmAtomicRead32( &barrier->flag ) == flagref ; )
      mtYield();
  }



#ifndef MM_ATOMIC_BARRIER_DELAYED_RESET
 #ifdef CPUCONF_STORE_REODERING_AFTER_STORE
  mmWriteBarrier();
 #endif
  for( ; leaf != barrier ; leaf = leaf->parent )
    mmAtomicWrite32( &leaf->flag, nextflagref );
#else
  for( ; leaf != barrier ; leaf = leaf->parent )
  {
    leaf->flagref = nextflagref;
    MM_ATOMIC_ACCESS_32(&leaf->counter) += leaf->resetvalue;
    /* No barrier : earlier, we disallowed delayed reset if CPUCONF_STORE_REODERING_AFTER_STORE */
    mmAtomicWrite32( &leaf->flag, nextflagref );
  }
#endif


  return 0;
}






////






static int mmAtomicCounterBuildStage( mmAtomicCounter *counter, int itembase, int itemcount, int counterstagesize, void *parent, int * const nodeindex )
{
  int nodesize, nodebasesize, itemremainder, childcount, nodecount;
  mmAtomicCounterNode *node;

  if( itemcount == 1 )
  {
    counter->locknode[itembase] = parent;
    return 1;
  }
  nodebasesize = itemcount / counterstagesize;
  itemremainder = itemcount - ( nodebasesize * counterstagesize );
  for( nodecount = 0 ; itemcount ; nodecount++, itemcount -= nodesize )
  {
    nodesize = nodebasesize;
    if( nodecount < itemremainder )
      nodesize++;
    if( nodesize >= 2 )
    {
      node = &counter->nodearray[ (*nodeindex)++ ];
      childcount = mmAtomicCounterBuildStage( counter, itembase, nodesize, counterstagesize, node, nodeindex );
      node->resetvalue = childcount;
      mmAtomicWrite32( &node->counter, childcount );
      node->parent = parent;
    }
    else
      counter->locknode[itembase] = parent;
    itembase += nodesize;
  }

  return nodecount;
}


void mmAtomicCounterInit( mmAtomicCounter *counter, int lockcount, int stagesize )
{
  int stagecount, nodecount, childcount;
  void *alloc;
  mmAtomicCounterNode *node;

  counter->nodearray = 0;
  stagecount = lockcount;
  nodecount = 1;
  for( ; stagecount > 1 ; )
  {
    stagecount = ( stagecount + (stagesize-1) ) / stagesize;
    nodecount += stagecount;
  }

  alloc = mmAlignAlloc( nodecount * sizeof(mmAtomicCounterNode) + lockcount * sizeof(mmAtomicCounterNode *), CPUCONF_CACHE_LINE_SIZE );
  counter->lockcount = lockcount;
  counter->nodecount = nodecount;
  counter->nodearray = alloc;
  counter->locknode = ADDRESS( alloc, nodecount * sizeof(mmAtomicCounterNode) );

  counter->nodecount = 0;
  node = &counter->nodearray[ counter->nodecount++ ];
  childcount = mmAtomicCounterBuildStage( counter, 0, lockcount, stagesize, node, &counter->nodecount );
  if( counter->nodecount > nodecount )
  {
    printf( "What the hell?\n" );
    exit( 1 );
  }
  node->resetvalue = childcount;
  mmAtomicWrite32( &node->counter, childcount );
  node->parent = 0;

  return;
}


void mmAtomicCounterDestroy( mmAtomicCounter *counter )
{
  if( counter->nodearray )
  {
    mmAlignFree( counter->nodearray );
    counter->nodearray = 0;
  }
  return;
}


int mmAtomicCounterHit( mmAtomicCounter *counter, int lockindex )
{
  mmAtomicCounterNode *node;
  node = counter->locknode[lockindex];
  for( ; ; )
  {
    if( !( mmAtomicAddTestZero32( &node->counter, -1 ) ) )
      break;
    MM_ATOMIC_ACCESS_32(&node->counter) += node->resetvalue;

/*
printf( "Node %d ; %d\n", (int)( node - counter->nodearray ), MM_ATOMIC_ACCESS_32(&node->counter) );
*/

    if( node->parent )
      node = node->parent;
    else
      return 1;
  }
  return 0;
}




/*

Implement a counter without all threads trashing each other's cache


*/


