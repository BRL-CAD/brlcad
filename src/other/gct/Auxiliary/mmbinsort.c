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


#include "cpuconfig.h"

#include "cc.h"
#include "mm.h"

#include "mmbinsort.h"



/*
#define MM_BINSORT_INSERTION_SORT
*/



typedef float mmbsf;
#define mmbsffloor(x) floorf(x)
#define mmbsfceil(x) ceilf(x)



typedef struct
{
  int itemcount;
  int flags;
  mmbsf min, max;
  void *p;
  void **last;
} mmBinSortBucket;

#define MM_BINSORT_BUCKET_FLAGS_SUBGROUP (0x1)


typedef struct
{
  mmbsf groupbase, groupmax, bucketrange;
  int bucketmax;
  mmBinSortBucket bucket[];
} mmBinSortGroup;


typedef struct
{
  int numanodeindex;
  size_t memsize;
  size_t itemlistoffset;
  int rootbucketcount; /* Count of buckets for root */
  int groupbucketcount; /* Count of buckets per group */
  int groupthreshold; /* Count of items required in a bucket  */
  int collapsethreshold;
  int maxdepth;

  /* Callback to user code to obtain the value of an item */
  double (*itemvalue)( void *item );

  /* Memory management */
  mmBlockHead bucketblock;
  mmBlockHead groupblock;

  /* Top-level group, *MUST* be at end of struct due to bucket[] zero-length array */
  mmBinSortGroup root;

} mmBinSortHead;


/****/


void *mmBinSortInit( size_t itemlistoffset, int rootbucketcount, int groupbucketcount, double rootmin, double rootmax, int groupthreshold, double (*itemvaluecallback)( void *item ), int maxdepth, int numanodeindex )
{
  int bucketindex;
  size_t memsize;
  mmBinSortHead *bsort;
  mmBinSortGroup *group;
  mmBinSortBucket *bucket;

  if( !( mmcontext.numaflag ) )
    numanodeindex = -1;
  memsize = sizeof(mmBinSortHead) + ( rootbucketcount * sizeof(mmBinSortBucket) );
  if( numanodeindex >= 0 )
  {
    bsort = mmNodeAlloc( numanodeindex, memsize );
    mmBlockNodeInit( &bsort->bucketblock, numanodeindex, sizeof(mmBinSortBucket), 1024, 1024, 0x40 );
    mmBlockNodeInit( &bsort->groupblock, numanodeindex, sizeof(mmBinSortGroup) + ( groupbucketcount * sizeof(mmBinSortBucket) ), 16, 16, 0x40 );
  }
  else
  {
    bsort = malloc( memsize );
    mmBlockInit( &bsort->bucketblock, sizeof(mmBinSortBucket), 1024, 1024, 0x40 );
    mmBlockInit( &bsort->groupblock, sizeof(mmBinSortGroup) + ( groupbucketcount * sizeof(mmBinSortBucket) ), 16, 16, 0x40 );
  }
  bsort->numanodeindex = numanodeindex;
  bsort->memsize = memsize;

  bsort->itemlistoffset = itemlistoffset;
  bsort->rootbucketcount = rootbucketcount;
  bsort->groupbucketcount = groupbucketcount;
  bsort->groupthreshold = groupthreshold;
  bsort->collapsethreshold = groupthreshold >> 2;
  bsort->maxdepth = maxdepth;

  bsort->itemvalue = itemvaluecallback;

  group = &bsort->root;
  group->groupbase = rootmin;
  group->groupmax = rootmax;
  group->bucketrange = ( rootmax - rootmin ) / (double)rootbucketcount;
  group->bucketmax = rootbucketcount - 1;
  bucket = group->bucket;
  for( bucketindex = 0 ; bucketindex < rootbucketcount ; bucketindex++ )
  {
    bucket->flags = 0;
    bucket->itemcount = 0;
    bucket->min =  FLT_MAX;
    bucket->max = -FLT_MAX;
    bucket->p = 0;
    bucket++;
  }

  return bsort;
}


void mmBinSortFree( void *binsort )
{
  mmBinSortHead *bsort;
  bsort = binsort;
  mmBlockFreeAll( &bsort->bucketblock );
  mmBlockFreeAll( &bsort->groupblock );
  if( bsort->numanodeindex >= 0 )
    mmNodeFree( bsort->numanodeindex, bsort, bsort->memsize );
  else
    free( bsort );
  return;
}


static int MM_NOINLINE mmBinSortBucketIndex( mmBinSortGroup *group, mmbsf value )
{
  int bucketindex = (int)mmbsffloor( ( value - group->groupbase ) / group->bucketrange );
  if( bucketindex < 0 )
    bucketindex = 0;
  if( bucketindex > group->bucketmax )
    bucketindex = group->bucketmax;
  return bucketindex;
}


/****/


static mmBinSortGroup *mmBinSortSpawnGroup( mmBinSortHead *bsort, void *itembase, mmbsf base, mmbsf range )
{
  int bucketindex;
  mmbsf value;
  void *item, *itemnext;
  mmBinSortGroup *group;
  mmBinSortBucket *bucket;

  group = mmBlockAlloc( &bsort->groupblock );
  group->groupbase = base;
  group->groupmax = base + range;
  group->bucketrange = range / (mmbsf)bsort->groupbucketcount;
  group->bucketmax = bsort->groupbucketcount - 1;

  bucket = group->bucket;
  for( bucketindex = 0 ; bucketindex < bsort->groupbucketcount ; bucketindex++ )
  {
    bucket->flags = 0;
    bucket->itemcount = 0;
    bucket->min =  FLT_MAX;
    bucket->max = -FLT_MAX;
    bucket->p = 0;
    bucket->last = &bucket->p;
    bucket++;
  }

  for( item = itembase ; item ; item = itemnext )
  {
    itemnext = ((mmListNode *)ADDRESS( item, bsort->itemlistoffset ))->next;

    value = bsort->itemvalue( item );
    bucketindex = mmBinSortBucketIndex( group, value );

    if( value < bucket->min )
      bucket->min = value;
    if( value > bucket->max )
      bucket->max = value;

    bucket = &group->bucket[ bucketindex ];
    bucket->itemcount++;
    mmListAdd( bucket->last, item, bsort->itemlistoffset );
    bucket->last = &((mmListNode *)ADDRESS( item, bsort->itemlistoffset ))->next;
  }

  return group;
}


/* Debugging */
/*
static int mmBinSortBucketCount( mmBinSortHead *bsort, mmBinSortBucket *bucket )
{
  int itemcount;
  void  *item;
  itemcount = 0;
  for( item = bucket->p ; item ; item = ((mmListNode *)ADDRESS( item, bsort->itemlistoffset ))->next )
    itemcount++;
  return itemcount;
}


static int mmBinSortGroupCount( mmBinSortHead *bsort, mmBinSortGroup *group )
{
  int itemcount, bucketindex;
  mmBinSortBucket *bucket;
  bucket = group->bucket;
  itemcount = 0;
  for( bucketindex = 0, bucket = group->bucket ; bucketindex < bsort->groupbucketcount ; bucketindex++, bucket++ )
  {
    if( bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP )
      itemcount += mmBinSortGroupCount( bsort, bucket->p );
    else
      itemcount += mmBinSortBucketCount( bsort, bucket );
  }
  return itemcount;
}


static void mmBinSortBucketCheck( mmBinSortHead *bsort, mmBinSortBucket *bucket )
{
  int itemcount;
  if( bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP )
    itemcount = mmBinSortGroupCount( bsort, bucket->p );
  else
    itemcount = mmBinSortBucketCount( bsort, bucket );
  printf( "Countcheck (0x%x) %d ~ %d\n", bucket->flags, itemcount, bucket->itemcount );
  if( itemcount != bucket->itemcount )
    exit( 1 );
  return;
}
*/




static void mmBinSortCollapseGroup( mmBinSortHead *bsort, mmBinSortBucket *parentbucket )
{
  int bucketindex, itemcount;
  mmBinSortGroup *group;
  mmBinSortBucket *bucket;
  void *item, *itemnext;

  group = parentbucket->p;
  parentbucket->p = 0;
  parentbucket->last = &parentbucket->p;

  itemcount = 0;
  for( bucketindex = 0, bucket = group->bucket ; bucketindex < bsort->groupbucketcount ; bucketindex++, bucket++ )
  {
    if( bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP )
      mmBinSortCollapseGroup( bsort, bucket );
    for( item = bucket->p ; item ; item = itemnext )
    {
      itemnext = ((mmListNode *)ADDRESS( item, bsort->itemlistoffset ))->next;
      mmListAdd( parentbucket->last, item, bsort->itemlistoffset );
      parentbucket->last = &((mmListNode *)ADDRESS( item, bsort->itemlistoffset ))->next;
      itemcount++;
    }
  }
  parentbucket->flags &= ~MM_BINSORT_BUCKET_FLAGS_SUBGROUP;
  parentbucket->itemcount = itemcount;

  return;
}


/****/


void mmBinSortAdd( void *binsort, void *item, double itemvalue )
{
  int bucketindex, depth;
  mmbsf value;
  mmBinSortHead *bsort;
  mmBinSortGroup *group, *subgroup;
  mmBinSortBucket *bucket;

  bsort = binsort;
  value = itemvalue;
  group = &bsort->root;
  for( depth = 0 ; ; group = bucket->p, depth++ )
  {
    bucketindex = mmBinSortBucketIndex( group, value );
    bucket = &group->bucket[ bucketindex ];
    bucket->itemcount++;
    if( bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP )
      continue;
    if( ( bucket->itemcount >= bsort->groupthreshold ) && ( depth < bsort->maxdepth ) )
    {
      /* Build a new sub group, sort all entries into it */
      subgroup = mmBinSortSpawnGroup( bsort, bucket->p, group->groupbase + ( (mmbsf)bucketindex * group->bucketrange ), group->bucketrange );
      bucket->flags |= MM_BINSORT_BUCKET_FLAGS_SUBGROUP;
      bucket->p = subgroup;
      continue;
    }
    if( value < bucket->min )
      bucket->min = value;
    if( value > bucket->max )
      bucket->max = value;

#ifdef MM_BINSORT_INSERTION_SORT
    void *itemfind, *itemnext;
    void **insert;
    insert = &bucket->p;
    for( itemfind = bucket->p ; itemfind ; itemfind = itemnext )
    {
      itemnext = ((mmListNode *)ADDRESS( itemfind, bsort->itemlistoffset ))->next;
      if( itemvalue <= bsort->itemvalue( itemfind ) )
        break;
      insert = &((mmListNode *)ADDRESS( itemfind, bsort->itemlistoffset ))->next;
    }
    mmListAdd( insert, item, bsort->itemlistoffset );
#else
    mmListAdd( &bucket->p, item, bsort->itemlistoffset );
#endif

    break;
  }

  return;
}

void mmBinSortRemove( void *binsort, void *item, double itemvalue )
{
  int bucketindex;
  mmbsf value;
  mmBinSortHead *bsort;
  mmBinSortGroup *group;
  mmBinSortBucket *bucket;

  bsort = binsort;
  value = itemvalue;
  group = &bsort->root;
  for( ; ; group = bucket->p )
  {
    bucketindex = mmBinSortBucketIndex( group, value );
    bucket = &group->bucket[ bucketindex ];
    bucket->itemcount--;
    if( bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP )
    {
      if( bucket->itemcount >= bsort->collapsethreshold )
        continue;
      mmBinSortCollapseGroup( bsort, bucket );
    }
    break;
  }
  mmListRemove( item, bsort->itemlistoffset );

  return;
}


void mmBinSortUpdate( void *binsort, void *item, double olditemvalue, double newitemvalue )
{
  mmBinSortRemove( binsort, item, olditemvalue );
  mmBinSortAdd( binsort, item, newitemvalue );
  return;
}


/****/


void *mmBinSortGetRootBucket( void *binsort, int bucketindex, int *itemcount )
{
  mmBinSortHead *bsort;
  mmBinSortBucket *bucket;

  bsort = binsort;
  bucket = &bsort->root.bucket[ bucketindex ];

  *itemcount = bucket->itemcount;
  return bucket->p;
}


void *mmBinSortGetGroupBucket( void *binsortgroup, int bucketindex, int *itemcount )
{
  mmBinSortGroup *group;
  mmBinSortBucket *bucket;

  group = binsortgroup;
  bucket = &group->bucket[ bucketindex ];

  *itemcount = bucket->itemcount;
  return bucket->p;
}


/****/


static void *mmBinSortGroupFirstItem( mmBinSortHead *bsort, mmBinSortGroup *group, mmbsf failmax )
{
  int bucketindex, topbucket;
  void *item;
  mmBinSortBucket *bucket;
  mmBinSortGroup *subgroup;

  if( failmax > group->groupmax )
    topbucket = group->bucketmax;
  else
  {
    topbucket = (int)mmbsfceil( ( failmax - group->groupbase ) / group->bucketrange );
    if( ( topbucket < 0 ) || ( topbucket > group->bucketmax ) )
      topbucket = group->bucketmax;
  }
/*
printf( "  Failmax %f ; Base %f ; Range %f ; Index %f\n", failmax, group->groupbase, group->bucketrange, mmbsfceil( failmax - group->groupbase ) / group->bucketrange );
printf( "  Top bucket : %d\n", topbucket );
*/
  bucket = group->bucket;
  for( bucketindex = 0 ; bucketindex <= topbucket ; bucketindex++, bucket++ )
  {
/*
printf( "  Bucket %d ; Count %d ; Flags 0x%x ; %p\n", bucketindex, bucket->itemcount, bucket->flags, bucket->p );
*/
    if( bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP )
    {
      subgroup = bucket->p;
      item = mmBinSortGroupFirstItem( bsort, subgroup, failmax );
      if( item )
        return item;
    }
    else if( bucket->p )
      return bucket->p;
  }

  return 0;
}


void *mmBinSortGetFirstItem( void *binsort, double failmax )
{
  int bucketindex, topbucket;
  mmBinSortHead *bsort;
  mmBinSortGroup *group;
  mmBinSortBucket *bucket;
  void *item;

  bsort = binsort;
  if( failmax > bsort->root.groupmax )
    topbucket = bsort->root.bucketmax;
  else
  {
    topbucket = (int)mmbsfceil( ( (mmbsf)failmax - bsort->root.groupbase ) / bsort->root.bucketrange );
    if( ( topbucket < 0 ) || ( topbucket > bsort->root.bucketmax ) )
      topbucket = bsort->root.bucketmax;
  }

/*
  printf( "TopBucket : %d / %d\n", topbucket, bsort->root.bucketmax );
*/
  bucket = bsort->root.bucket;
  for( bucketindex = 0 ; bucketindex <= topbucket ; bucketindex++, bucket++ )
  {
/*
printf( "  Bucket %d ; Count %d ; Flags 0x%x ; %p\n", bucketindex, bucket->itemcount, bucket->flags, bucket->p );
*/
    if( bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP )
    {
      group = bucket->p;
      item = mmBinSortGroupFirstItem( bsort, group, (mmbsf)failmax );
      if( item )
        return item;
    }
    else if( bucket->p )
      return bucket->p;
  }

  return 0;
}


/****/




