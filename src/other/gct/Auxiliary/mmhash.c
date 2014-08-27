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

#include "cpuconfig.h"
#include "cc.h"
#include "mm.h"
#include "mmhash.h"




typedef struct
{
#ifdef MM_ATOMIC_SUPPORT
  /* Powerful atomic read/write lock */
  mmAtomicLock32 lock;
  mmAtomicP owner;
#else
  /* Mutex, or how to ruin performance */
  mtMutex mutex;
  void *owner;
#endif
} mmHashPage MM_CACHE_ALIGN;

typedef struct
{
  uint32_t status;
  uint32_t flags;
  size_t entrysize;

  /* Page locks */
  uint32_t pageshift;
  uint32_t pagecount;
  uint32_t pagemask;
  mmHashPage *page;

  /* Hash size */
  uint32_t minhashbits;
  uint32_t hashbits;
  uint32_t hashsize;
  uint32_t hashmask;

  /* Entry count tracking if hash table is dynamic */
#ifdef MM_ATOMIC_SUPPORT
  mmAtomic32 entrycount;
#else
  mtMutex countmutex;
  uint32_t entrycount;
#endif
  uint32_t lowcount;
  uint32_t highcount;

  /* Global lock as the final word to resolve fighting between threads trying to access the same entry */
  char paddingA[64];
#ifdef MM_ATOMIC_SUPPORT
  mmAtomic32 globallock;
#else
  mtMutex globalmutex;
#endif
  char paddingB[64];

#ifdef MM_HASH_DEBUG_STATISTICS
  long entrycountmax;
  mmAtomicL statentrycount;
  mmAtomicL accesscount;
  mmAtomicL collisioncount;
  mmAtomicL relocationcount;
#endif

} mmHashTable;

#define MM_HASH_ATOMIC_TRYCOUNT (16)

#define MM_HASH_SIZEOF_ALIGN16(x) ((sizeof(x)+0xF)&~0xF)
#define MM_HASH_SIZEOF_ALIGN64(x) ((sizeof(x)+0x3F)&~0x3F)
#define MM_HASH_ALIGN64(x) ((x+0x3F)&~0x3F)
#define MM_HASH_ENTRYLIST(table) (void *)ADDRESS(table,MM_HASH_SIZEOF_ALIGN64(mmHashTable))
#define MM_HASH_ENTRY(table,index) (void *)ADDRESS(table,MM_HASH_SIZEOF_ALIGN64(mmHashTable)+((index)*(table)->entrysize))
#define MM_HASH_PAGELIST(table) (void *)ADDRESS(table,MM_HASH_ALIGN64(MM_HASH_SIZEOF_ALIGN64(mmHashTable)+((table)->hashsize*(table)->entrysize)))



////



static void mmHashSetBounds( mmHashTable *table )
{
  table->lowcount = 0;
  if( table->hashbits > table->minhashbits )
    table->lowcount = table->hashsize / 5;
  table->highcount = table->hashsize / 2;
  return;
}


size_t mmHashRequiredSize( size_t entrysize, uint32_t hashbits, uint32_t pageshift )
{
  uint32_t entrycount;
  uint32_t pagecount;
  entrycount = 1 << hashbits;
  pagecount = entrycount >> pageshift;
  if( !( pagecount ) )
    pagecount = 1;
  return MM_HASH_ALIGN64( MM_HASH_SIZEOF_ALIGN16(mmHashTable) + ( entrycount * entrysize ) ) + ( pagecount * sizeof(mmHashPage) );
}


void mmHashInit( void *hashtable, mmHashAccess *access, size_t entrysize, uint32_t hashbits, uint32_t pageshift, uint32_t flags )
{
  uint32_t hashkey, pageindex;
  void *entry;
  mmHashTable *table;
  mmHashPage *page;

  table = hashtable;
  table->status = MM_HASH_STATUS_NORMAL;
  if( flags & MM_HASH_FLAGS_NO_COUNT )
    table->status = MM_HASH_STATUS_UNKNOWN;
  table->flags = flags;
  table->entrysize = entrysize;
  table->minhashbits = hashbits;
  table->hashbits = hashbits;
  table->hashsize = 1 << table->hashbits;
  table->hashmask = table->hashsize - 1;
  table->pageshift = pageshift;
  table->pagecount = table->hashsize >> pageshift;
  if( !( table->pagecount ) )
    table->pagecount = 1;
  table->pagemask = table->pagecount - 1;
  table->page = MM_HASH_PAGELIST( table );
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicWrite32( &table->entrycount, 0 );
#else
  mtMutexInit( &table->countmutex );
  table->entrycount = 0;
#endif
  mmHashSetBounds( table );

  /* Clear the table */
  entry = MM_HASH_ENTRYLIST( table );
  for( hashkey = 0 ; hashkey < table->hashsize ; hashkey++ )
  {
    access->clearentry( entry );
    entry = ADDRESS( entry, entrysize );
  }

  /* Clear the lock pages */
  page = table->page;
#ifdef MM_ATOMIC_SUPPORT
  for( pageindex = table->pagecount ; pageindex ; pageindex--, page++ )
  {
    mmAtomicLockInit32( &page->lock );
    mmAtomicWriteP( &page->owner, 0 );
  }
  mmAtomicWrite32( &table->globallock, 0x0 );
#else
  for( pageindex = table->pagecount ; pageindex ; pageindex--, page++ )
  {
    mtMutexInit( &page->mutex );
    page->owner = 0;
  }
  mtMutexInit( &table->globalmutex );
#endif

#ifdef MM_HASH_DEBUG_STATISTICS
  table->entrycountmax = 0;
  mmAtomicWriteL( &table->statentrycount, 0 );
  mmAtomicWriteL( &table->accesscount, 0 );
  mmAtomicWriteL( &table->collisioncount, 0 );
  mmAtomicWriteL( &table->relocationcount, 0 );
#endif

  return;
}


int mmHashGetStatus( void *hashtable, int *rethashbits )
{
  mmHashTable *table;
  table = hashtable;
  if( rethashbits )
    *rethashbits = table->hashbits;
  return table->status;
}



////



#ifdef MM_ATOMIC_SUPPORT

 #define MM_HASH_LOCK_TRY_READ(t,p) (mmAtomicLockTryRead32(&t->page[p].lock,MM_HASH_ATOMIC_TRYCOUNT))
 #define MM_HASH_LOCK_TRY_WRITE(t,p) (mmAtomicLockTryWrite32(&t->page[p].lock,MM_HASH_ATOMIC_TRYCOUNT))
 #define MM_HASH_LOCK_DONE_READ(t,p) (mmAtomicLockDoneRead32(&t->page[p].lock))
 #define MM_HASH_LOCK_DONE_WRITE(t,p) (mmAtomicLockDoneWrite32(&t->page[p].lock))
 #define MM_HASH_GLOBAL_LOCK(t) (mmAtomicSpin32(&t->globallock,0x0,0x1))
 #define MM_HASH_GLOBAL_UNLOCK(t) (mmAtomicWrite32(&t->globallock,0x0))
 #define MM_HASH_ENTRYCOUNT_ADD_READ(t,c) (mmAtomicAddRead32(&table->entrycount,c))

#else

 #define MM_HASH_LOCK_TRY_READ(t,p) (mtMutexTryLock(&t->page[p].mutex))
 #define MM_HASH_LOCK_TRY_WRITE(t,p) (mtMutexTryLock(&t->page[p].mutex))
 #define MM_HASH_LOCK_DONE_READ(t,p) (mtMutexUnlock(&t->page[p].mutex))
 #define MM_HASH_LOCK_DONE_WRITE(t,p) (mtMutexUnlock(&t->page[p].mutex))
 #define MM_HASH_GLOBAL_LOCK(t) (mtMutexLock(&t->globalmutex))
 #define MM_HASH_GLOBAL_UNLOCK(t) (mtMutexUnlock(&t->globalmutex))

static inline uint32_t MM_HASH_ENTRYCOUNT_ADD_READ( mmHashTable *t, int32_t c )
{
  uint32_t entrycount;
  mtMutexLock( &t->countmutex );
  t->entrycount += c;
  entrycount = t->entrycount;
  mtMutexUnlock( &t->countmutex );
  return entrycount;
}

#endif



////



void *mmHashDirectFindEntry( void *hashtable, mmHashAccess *access, void *findentry )
{
  int cmpvalue;
  uint32_t hashkey;
  void *entry;
  mmHashTable *table;

  table = hashtable;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Hash key of entry */
  hashkey = access->entrykey( findentry ) & table->hashmask;

  /* Search the entry */
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, findentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
      break;
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
      return entry;
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  return 0;
}


static int mmHashTryFindEntry( mmHashTable *table, mmHashAccess *access, void *findentry, void **retentry )
{
  uint32_t hashkey;
  uint32_t pageindex, pagestart, pagefinal;
  int cmpvalue, retvalue;
  void *entry;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Hash key of entry */
  hashkey = access->entrykey( findentry ) & table->hashmask;

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
  if( !( MM_HASH_LOCK_TRY_READ( table, pagestart ) ) )
    return MM_HASH_TRYAGAIN;

  /* Search the entry */
  entry = 0;
  retvalue = MM_HASH_SUCCESS;
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    /* Lock new pages */
    pageindex = hashkey >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_READ( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        break;
      }
      pagefinal = pageindex;
    }

    /* Check for entry match */
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, findentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
    {
      retvalue = MM_HASH_FAILURE;
      entry = 0;
      break;
    }
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
      break;
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  /* Unlock all pages */
  for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
  {
    MM_HASH_LOCK_DONE_READ( table, pageindex );
    if( pageindex == pagefinal )
      break;
  }

  *retentry = entry;
  return retvalue;
}


void *mmHashLockFindEntry( void *hashtable, mmHashAccess *access, void *findentry )
{
  int retvalue;
  void *entry;
  mmHashTable *table;

  table = hashtable;
  retvalue = mmHashTryFindEntry( table, access, findentry, &entry );
  if( retvalue == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      retvalue = mmHashTryFindEntry( table, access, findentry, &entry );
    } while( retvalue == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return entry;
}



////



void mmHashDirectListEntry( void *hashtable, mmHashAccess *access, void *listentry, void *opaque )
{
  int cmpvalue;
  uint32_t hashkey;
  void *entry;
  mmHashTable *table;

  table = hashtable;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Hash key of entry */
  hashkey = access->entrykey( listentry ) & table->hashmask;

  /* Search the entry */
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrylist( opaque, entry, listentry );
    if( cmpvalue == MM_HASH_ENTRYLIST_BREAK )
      break;
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  return;
}


static int mmHashTryListEntry( mmHashTable *table, mmHashAccess *access, void *listentry, void *opaque )
{
  uint32_t hashkey;
  uint32_t pageindex, pagestart, pagefinal;
  int cmpvalue, retvalue;
  void *entry;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Hash key of entry */
  hashkey = access->entrykey( listentry ) & table->hashmask;

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
  if( !( MM_HASH_LOCK_TRY_READ( table, pagestart ) ) )
    return MM_HASH_TRYAGAIN;

  /* Search the entry */
  entry = 0;
  retvalue = MM_HASH_SUCCESS;
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    /* Lock new pages */
    pageindex = hashkey >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_READ( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        break;
      }
      pagefinal = pageindex;
    }

    /* Check for entry match */
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrylist( opaque, entry, listentry );
    if( cmpvalue == MM_HASH_ENTRYLIST_BREAK )
      break;
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  /* Unlock all pages */
  for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
  {
    MM_HASH_LOCK_DONE_READ( table, pageindex );
    if( pageindex == pagefinal )
      break;
  }

  return retvalue;
}


void mmHashLockListEntry( void *hashtable, mmHashAccess *access, void *listentry, void *opaque )
{
  int retvalue;
  mmHashTable *table;

  table = hashtable;
  retvalue = mmHashTryListEntry( table, access, listentry, opaque );
  if( retvalue == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      retvalue = mmHashTryListEntry( table, access, listentry, opaque );
    } while( retvalue == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return;
}



////



int mmHashDirectReadEntry( void *hashtable, mmHashAccess *access, void *readentry )
{
  int cmpvalue;
  uint32_t hashkey;
  void *entry;
  mmHashTable *table;

  table = hashtable;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Hash key of entry */
  hashkey = access->entrykey( readentry ) & table->hashmask;

  /* Search the entry */
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, readentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
      break;
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
    {
      memcpy( readentry, entry, table->entrysize );
      return MM_HASH_SUCCESS;
    }
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  return MM_HASH_FAILURE;
}


static int mmHashTryReadEntry( mmHashTable *table, mmHashAccess *access, void *readentry )
{
  uint32_t hashkey;
  uint32_t pageindex, pagestart, pagefinal;
  int cmpvalue, retvalue;
  void *entry;

  /* Hash key of entry */
  hashkey = access->entrykey( readentry ) & table->hashmask;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
  if( !( MM_HASH_LOCK_TRY_READ( table, pagestart ) ) )
    return MM_HASH_TRYAGAIN;

  /* Search the entry */
  entry = 0;
  retvalue = MM_HASH_SUCCESS;
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    /* Lock new pages */
    pageindex = hashkey >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_READ( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        break;
      }
      pagefinal = pageindex;
    }

    /* Check for entry match */
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, readentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
    {
      retvalue = MM_HASH_FAILURE;
      entry = 0;
      break;
    }
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
      break;
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  if( entry )
    memcpy( readentry, entry, table->entrysize );

  /* Unlock all pages */
  for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
  {
    MM_HASH_LOCK_DONE_READ( table, pageindex );
    if( pageindex == pagefinal )
      break;
  }

  return retvalue;
}


int mmHashLockReadEntry( void *hashtable, mmHashAccess *access, void *readentry )
{
  int retvalue;
  mmHashTable *table;

  table = hashtable;
  retvalue = mmHashTryReadEntry( table, access, readentry );
  if( retvalue == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      retvalue = mmHashTryReadEntry( table, access, readentry );
    } while( retvalue == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return retvalue;
}



////



int mmHashDirectCallEntry( void *hashtable, mmHashAccess *access, void *callentry, void (*callback)( void *opaque, void *entry, int newflag ), void *opaque, int addflag )
{
  int cmpvalue;
  uint32_t hashkey, entrycount;
  void *entry;
  mmHashTable *table;

  table = hashtable;

  /* Hash key of entry */
  hashkey = access->entrykey( callentry ) & table->hashmask;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Search an available entry */
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, callentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
      break;
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
    {
      callback( opaque, entry, 0 );
      return MM_HASH_SUCCESS;
    }
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  if( !( addflag ) )
    return MM_HASH_FAILURE;

  /* Store new entry */
  memcpy( entry, callentry, table->entrysize );
  callback( opaque, entry, 1 );

  /* Increment count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, 1 );
    if( entrycount >= table->highcount )
      table->status = MM_HASH_STATUS_MUSTGROW;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  int statentrycount = mmAtomicAddReadL( &table->statentrycount, 1 );
  if( statentrycount > table->entrycountmax )
    table->entrycountmax = statentrycount;
#endif

  return MM_HASH_SUCCESS;
}


static int mmHashTryCallEntry( mmHashTable *table, mmHashAccess *access, void *callentry, void (*callback)( void *opaque, void *entry, int newflag ), void *opaque, int addflag )
{
  uint32_t hashkey, entrycount;
  uint32_t pageindex, pagestart, pagefinal;
  int cmpvalue, retvalue;
  void *entry;

  /* Hash key of entry */
  hashkey = access->entrykey( callentry ) & table->hashmask;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
  if( !( MM_HASH_LOCK_TRY_WRITE( table, pagestart ) ) )
    return MM_HASH_TRYAGAIN;

  /* Search an available entry */
  retvalue = MM_HASH_SUCCESS;
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    /* Lock new pages */
    pageindex = hashkey >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_WRITE( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        goto end;
      }
      pagefinal = pageindex;
    }

    /* Check for entry available */
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, callentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
      break;
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
    {
      callback( opaque, entry, 0 );
      retvalue = MM_HASH_SUCCESS;
      goto end;
    }
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  if( !( addflag ) )
  {
    retvalue = MM_HASH_FAILURE;
    goto end;
  }

  /* Store new entry */
  memcpy( entry, callentry, table->entrysize );
  callback( opaque, entry, 1 );

  /* Increment count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, 1 );
    if( entrycount >= table->highcount )
      table->status = MM_HASH_STATUS_MUSTGROW;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  int statentrycount = mmAtomicAddReadL( &table->statentrycount, 1 );
  if( statentrycount > table->entrycountmax )
    table->entrycountmax = statentrycount;
#endif

  end:

  /* Unlock all pages */
  for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
  {
    MM_HASH_LOCK_DONE_WRITE( table, pageindex );
    if( pageindex == pagefinal )
      break;
  }

  return retvalue;
}


int mmHashLockCallEntry( void *hashtable, mmHashAccess *access, void *callentry, void (*callback)( void *opaque, void *entry, int newflag ), void *opaque, int addflag )
{
  int retvalue;
  mmHashTable *table;

  table = hashtable;
  retvalue = mmHashTryCallEntry( table, access, callentry, callback, opaque, addflag );
  if( retvalue == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      retvalue = mmHashTryCallEntry( table, access, callentry, callback, opaque, addflag );
    } while( retvalue == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return retvalue;
}



////



int mmHashDirectReplaceEntry( void *hashtable, mmHashAccess *access, void *replaceentry, int addflag )
{
  int cmpvalue;
  uint32_t hashkey, entrycount;
  void *entry;
  mmHashTable *table;

  table = hashtable;

  /* Hash key of entry */
  hashkey = access->entrykey( replaceentry ) & table->hashmask;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Search an available entry */
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, replaceentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
      break;
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
    {
      memcpy( entry, replaceentry, table->entrysize );
      return MM_HASH_SUCCESS;
    }
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  if( !( addflag ) )
    return MM_HASH_FAILURE;

  /* Store new entry */
  memcpy( entry, replaceentry, table->entrysize );

  /* Increment count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, 1 );
    if( entrycount >= table->highcount )
      table->status = MM_HASH_STATUS_MUSTGROW;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  int statentrycount = mmAtomicAddReadL( &table->statentrycount, 1 );
  if( statentrycount > table->entrycountmax )
    table->entrycountmax = statentrycount;
#endif

  return MM_HASH_SUCCESS;
}


static int mmHashTryReplaceEntry( mmHashTable *table, mmHashAccess *access, void *replaceentry, int addflag )
{
  uint32_t hashkey, entrycount;
  uint32_t pageindex, pagestart, pagefinal;
  int cmpvalue, retvalue;
  void *entry;

  /* Hash key of entry */
  hashkey = access->entrykey( replaceentry ) & table->hashmask;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
  if( !( MM_HASH_LOCK_TRY_WRITE( table, pagestart ) ) )
    return MM_HASH_TRYAGAIN;


  /* Search an available entry */
  retvalue = MM_HASH_SUCCESS;
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    /* Lock new pages */
    pageindex = hashkey >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_WRITE( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        goto end;
      }
      pagefinal = pageindex;
    }

    /* Check for entry available */
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, replaceentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
      break;
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
    {
      memcpy( entry, replaceentry, table->entrysize );
      retvalue = MM_HASH_SUCCESS;
      goto end;
    }
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  if( !( addflag ) )
  {
    retvalue = MM_HASH_FAILURE;
    goto end;
  }

  /* Store new entry */
  memcpy( entry, replaceentry, table->entrysize );

  /* Increment count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, 1 );
    if( entrycount >= table->highcount )
      table->status = MM_HASH_STATUS_MUSTGROW;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  int statentrycount = mmAtomicAddReadL( &table->statentrycount, 1 );
  if( statentrycount > table->entrycountmax )
    table->entrycountmax = statentrycount;
#endif

  end:

  /* Unlock all pages */
  for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
  {
    MM_HASH_LOCK_DONE_WRITE( table, pageindex );
    if( pageindex == pagefinal )
      break;
  }

  return retvalue;
}


int mmHashLockReplaceEntry( void *hashtable, mmHashAccess *access, void *replaceentry, int addflag )
{
  int retvalue;
  mmHashTable *table;

  table = hashtable;
  retvalue = mmHashTryReplaceEntry( table, access, replaceentry, addflag );
  if( retvalue == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      retvalue = mmHashTryReplaceEntry( table, access, replaceentry, addflag );
    } while( retvalue == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return retvalue;
}



////



int mmHashDirectAddEntry( void *hashtable, mmHashAccess *access, void *addentry, int nodupflag )
{
  int cmpvalue;
  uint32_t hashkey, entrycount;
  void *entry;
  mmHashTable *table;

  table = hashtable;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Hash key of entry */
  hashkey = access->entrykey( addentry ) & table->hashmask;

  /* Search an available entry */
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    entry = MM_HASH_ENTRY( table, hashkey );
    /* Do we allow duplicate entries? */
    if( nodupflag )
    {
      cmpvalue = access->entrycmp( entry, addentry );
      if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
        break;
      else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
        return MM_HASH_FAILURE;
    }
    else
    {
      if( !( access->entryvalid( entry ) ) )
        break;
    }
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  /* Store new entry */
  memcpy( entry, addentry, table->entrysize );

  /* Increment count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, 1 );
    if( entrycount >= table->highcount )
      table->status = MM_HASH_STATUS_MUSTGROW;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  int statentrycount = mmAtomicAddReadL( &table->statentrycount, 1 );
  if( statentrycount > table->entrycountmax )
    table->entrycountmax = statentrycount;
#endif

  return MM_HASH_SUCCESS;
}


static int mmHashTryAddEntry( mmHashTable *table, mmHashAccess *access, void *addentry, int nodupflag )
{
  uint32_t hashkey, entrycount;
  uint32_t pageindex, pagestart, pagefinal;
  int cmpvalue, retvalue;
  void *entry;

  /* Hash key of entry */
  hashkey = access->entrykey( addentry ) & table->hashmask;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
  if( !( MM_HASH_LOCK_TRY_WRITE( table, pagestart ) ) )
    return MM_HASH_TRYAGAIN;

  /* Search an available entry */
  retvalue = MM_HASH_SUCCESS;
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    /* Lock new pages */
    pageindex = hashkey >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_WRITE( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        goto end;
      }
      pagefinal = pageindex;
    }

    /* Check for entry available */
    entry = MM_HASH_ENTRY( table, hashkey );
    /* Do we allow duplicate entries? */
    if( nodupflag )
    {
      cmpvalue = access->entrycmp( entry, addentry );
      if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
        break;
      else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
      {
        retvalue = MM_HASH_FAILURE;
        goto end;
      }
    }
    else
    {
      if( !( access->entryvalid( entry ) ) )
        break;
    }
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  /* Store new entry */
  memcpy( entry, addentry, table->entrysize );

  /* Increment count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, 1 );
    if( entrycount >= table->highcount )
      table->status = MM_HASH_STATUS_MUSTGROW;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  int statentrycount = mmAtomicAddReadL( &table->statentrycount, 1 );
  if( statentrycount > table->entrycountmax )
    table->entrycountmax = statentrycount;
#endif

  end:

  /* Unlock all pages */
  for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
  {
    MM_HASH_LOCK_DONE_WRITE( table, pageindex );
    if( pageindex == pagefinal )
      break;
  }

  return retvalue;
}



int mmHashLockAddEntry( void *hashtable, mmHashAccess *access, void *addentry, int nodupflag )
{
  int retvalue;
  mmHashTable *table;

  table = hashtable;
  retvalue = mmHashTryAddEntry( table, access, addentry, nodupflag );
  if( retvalue == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      retvalue = mmHashTryAddEntry( table, access, addentry, nodupflag );
    } while( retvalue == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return retvalue;
}



////


#define MM_ROBUST_DELETION


int mmHashDirectDeleteEntry( void *hashtable, mmHashAccess *access, void *deleteentry, int readflag )
{
  int cmpvalue;
  uint32_t hashkey, srckey, srcpos, targetpos = 0, targetkey, entrycount;
#ifdef MM_ROBUST_DELETION
  uint32_t delbase;
#else
  int32_t halftablesize, distance;
#endif
  void *entry, *srcentry, *targetentry;
  mmHashTable *table;

  table = hashtable;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Hash key of entry */
  hashkey = access->entrykey( deleteentry ) & table->hashmask;

  /* Search the entry */
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, deleteentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
      return MM_HASH_FAILURE;
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
      break;
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  if( readflag )
    memcpy( deleteentry, entry, table->entrysize );

#ifdef MM_ROBUST_DELETION
  for( delbase = hashkey ; ; )
  {
    delbase = ( delbase - 1 ) & table->hashmask;
    if( !( access->entryvalid( MM_HASH_ENTRY( table, delbase ) ) ) )
      break;
  }
  delbase = ( delbase + 1 ) & table->hashmask;
#else
  halftablesize = table->hashsize >> 1;
#endif

  /* Entry found, delete it */
  for( ; ; )
  {
    /* Find replacement target */
    targetkey = hashkey;
    targetentry = 0;
    for( srcpos = hashkey ; ; )
    {
      srcpos = ( srcpos + 1 ) & table->hashmask;

      /* Try next entry */
      srcentry = MM_HASH_ENTRY( table, srcpos );
      if( !( access->entryvalid( srcentry ) ) )
        break;
      srckey = access->entrykey( srcentry ) & table->hashmask;

#ifdef MM_ROBUST_DELETION
      if( targetkey >= delbase )
      {
        if( ( srckey < delbase ) || ( srckey > targetkey ) )
          continue;
      }
      else if( ( srckey > targetkey ) && ( srckey < delbase ) )
        continue;
      targetentry = srcentry;
      targetkey = srckey;
      targetpos = srcpos;
#else
      /* Check if moving the entry backwards is allowed without breaking chain */
      distance = (int32_t)targetkey - (int32_t)srckey;
      if( distance > halftablesize )
        distance -= table->hashsize;
      else if( distance < -halftablesize )
        distance += table->hashsize;
      if( distance >= 0 )
      {
        targetentry = srcentry;
        targetkey = srckey;
        targetpos = srcpos;
      }
#endif
    }

    /* No replacement found, just clear it */
    entry = MM_HASH_ENTRY( table, hashkey );
    if( !( targetentry ) )
    {
      access->clearentry( entry );
      break;
    }

    /* Move entry in place and continue the repair process */
    memcpy( entry, targetentry, table->entrysize );

#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->relocationcount, 1 );
#endif

    hashkey = targetpos;
  }

  /* Decrement count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, -1 );
    if( entrycount < table->lowcount )
      table->status = MM_HASH_STATUS_MUSTSHRINK;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->statentrycount, -1 );
#endif

  return MM_HASH_SUCCESS;
}


static int mmHashTryDeleteEntry( mmHashTable *table, mmHashAccess *access, void *deleteentry, int readflag )
{
  uint32_t hashkey, srckey, srcpos, srcend, targetpos = 0, targetkey, entrycount;
  uint32_t pageindex, pagestart, pagefinal;
#ifdef MM_ROBUST_DELETION
  uint32_t delbase;
#else
  int32_t halftablesize, distance;
#endif
  int cmpvalue, retvalue;
  void *entry, *srcentry, *targetentry;

  /* Hash key of entry */
  hashkey = access->entrykey( deleteentry ) & table->hashmask;

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->accesscount, 1 );
#endif

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
  if( !( MM_HASH_LOCK_TRY_WRITE( table, pagestart ) ) )
    return MM_HASH_TRYAGAIN;

  /* Search the entry */
  retvalue = MM_HASH_SUCCESS;
  for( ; ; hashkey = ( hashkey + 1 ) & table->hashmask )
  {
    /* Lock new pages */
    pageindex = hashkey >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_WRITE( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        goto end;
      }
      pagefinal = pageindex;
    }

    /* Check for entry match */
    entry = MM_HASH_ENTRY( table, hashkey );
    cmpvalue = access->entrycmp( entry, deleteentry );
    if( cmpvalue == MM_HASH_ENTRYCMP_INVALID )
    {
      retvalue = MM_HASH_FAILURE;
      goto end;
    }
    else if( cmpvalue == MM_HASH_ENTRYCMP_FOUND )
      break;
#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->collisioncount, 1 );
#endif
  }

  if( readflag )
    memcpy( deleteentry, entry, table->entrysize );

#ifdef MM_ROBUST_DELETION
  for( delbase = hashkey ; ; )
  {
    delbase = ( delbase - 1 ) & table->hashmask;
    if( !( access->entryvalid( MM_HASH_ENTRY( table, delbase ) ) ) )
      break;
  }
  delbase = ( delbase + 1 ) & table->hashmask;
#else
  halftablesize = table->hashsize >> 1;
#endif

  /* Preemtively lock all pages in the stream before starting the operation */
  for( srcend = hashkey ; ; )
  {
    srcend = ( srcend + 1 ) & table->hashmask;

    /* Lock new pages */
    pageindex = srcend >> table->pageshift;
    if( pageindex != pagefinal )
    {
      if( !( MM_HASH_LOCK_TRY_WRITE( table, pageindex ) ) )
      {
        retvalue = MM_HASH_TRYAGAIN;
        goto end;
      }
      pagefinal = pageindex;
    }

    /* Check for valid entry */
    srcentry = MM_HASH_ENTRY( table, srcend );
    if( !( access->entryvalid( srcentry ) ) )
      break;
  }

  /* Entry found, delete it and reorder the hash stream of entries */
  for( ; ; )
  {
    /* Find replacement target */
    targetkey = hashkey;
    targetentry = 0;
    for( srcpos = hashkey ; ; )
    {
      srcpos = ( srcpos + 1 ) & table->hashmask;
      srcentry = MM_HASH_ENTRY( table, srcpos );

      /* Don't loop beyond the end of hash stream */
      if( srcpos == srcend )
        break;

      /* Try next entry */
      srckey = access->entrykey( srcentry ) & table->hashmask;

#ifdef MM_ROBUST_DELETION
      if( targetkey >= delbase )
      {
        if( ( srckey < delbase ) || ( srckey > targetkey ) )
          continue;
      }
      else if( ( srckey > targetkey ) && ( srckey < delbase ) )
        continue;
      targetentry = srcentry;
      targetkey = srckey;
      targetpos = srcpos;
#else
      /* Check if moving the entry backwards is allowed without breaking chain */
      distance = (int32_t)targetkey - (int32_t)srckey;
      if( distance > halftablesize )
        distance -= table->hashsize;
      else if( distance < -halftablesize )
        distance += table->hashsize;
      if( distance >= 0 )
      {
        targetentry = srcentry;
        targetkey = srckey;
        targetpos = srcpos;
      }
#endif
    }

    /* No replacement found, just clear it */
    entry = MM_HASH_ENTRY( table, hashkey );
    if( !( targetentry ) )
    {
      access->clearentry( entry );
      break;
    }

    /* Move entry in place and continue the repair process */
    memcpy( entry, targetentry, table->entrysize );

#ifdef MM_HASH_DEBUG_STATISTICS
    mmAtomicAddL( &table->relocationcount, 1 );
#endif

    hashkey = targetpos;
  }

  /* Decrement count of entries in table */
  if( !( table->flags & MM_HASH_FLAGS_NO_COUNT ) )
  {
    entrycount = MM_HASH_ENTRYCOUNT_ADD_READ( table, -1 );
    if( entrycount < table->lowcount )
      table->status = MM_HASH_STATUS_MUSTSHRINK;
  }

#ifdef MM_HASH_DEBUG_STATISTICS
  mmAtomicAddL( &table->statentrycount, -1 );
#endif

  end:

  /* Unlock all pages */
  for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
  {
    MM_HASH_LOCK_DONE_WRITE( table, pageindex );
    if( pageindex == pagefinal )
      break;
  }

  return retvalue;
}


int mmHashLockDeleteEntry( void *hashtable, mmHashAccess *access, void *deleteentry, int readflag )
{
  int retvalue;
  mmHashTable *table;

  table = hashtable;
  retvalue = mmHashTryDeleteEntry( table, access, deleteentry, readflag );
  if( retvalue == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      retvalue = mmHashTryDeleteEntry( table, access, deleteentry, readflag );
    } while( retvalue == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return retvalue;
}



////



/* Must be called while NO other thread will ever access the table for writing */
void mmHashResize( void *newtable, void *oldtable, mmHashAccess *access, uint32_t hashbits, uint32_t pageshift )
{
  uint32_t hashkey, hashpos, dstkey, dstpos, pageindex;
  void *srcentry, *dstentry;
  mmHashTable *dst, *src;
  mmHashPage *page;

  dst = newtable;
  src = oldtable;
  dst->status = MM_HASH_STATUS_NORMAL;
  dst->flags = src->flags;
  dst->entrysize = src->entrysize;
  dst->minhashbits = src->minhashbits;
  dst->hashbits = hashbits;
  dst->hashsize = 1 << dst->hashbits;
  dst->hashmask = dst->hashsize - 1;
  dst->pageshift = pageshift;
  dst->pagecount = dst->hashsize >> pageshift;
  dst->pagemask = dst->pagecount - 1;
  dst->page = MM_HASH_PAGELIST( dst );
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicWrite32( &dst->entrycount, mmAtomicRead32( &src->entrycount ) );
#else
  dst->entrycount = src->entrycount;
#endif
  mmHashSetBounds( dst );

  /* Clear the whole destination table */
  dstentry = MM_HASH_ENTRYLIST( dst );
  for( hashpos = 0 ; hashpos < dst->hashsize ; hashpos++ )
  {
    access->clearentry( dstentry );
    dstentry = ADDRESS( dstentry, dst->entrysize );
  }

  /* Clear the lock pages */
#ifdef MM_ATOMIC_SUPPORT
  page = dst->page;
  for( pageindex = 0 ; pageindex < dst->pagecount ; pageindex++, page++ )
  {
    mmAtomicLockInit32( &page->lock );
    mmAtomicWriteP( &page->owner, (void *)0 );
  }
  mmAtomicWrite32( &dst->globallock, 0x0 );
#else
  page = src->page;
  for( pageindex = src->pagecount ; pageindex ; pageindex--, page++ )
    mtMutexDestroy( &page->mutex );
  mtMutexDestroy( &src->globalmutex );
  page = dst->page;
  for( pageindex = dst->pagecount ; pageindex ; pageindex--, page++ )
  {
    mtMutexInit( &page->mutex );
    page->owner = 0;
  }
  mtMutexInit( &dst->globalmutex );
#endif

  /* Move all entries from the src table to the dst table */
  srcentry = MM_HASH_ENTRYLIST( src );
  for( hashpos = 0 ; hashpos < src->hashsize ; hashpos++ )
  {
    hashkey = access->entrykey( srcentry );
    dstkey = hashkey & dst->hashmask;

    /* Search an empty spot in the destination table */
    for( dstpos = dstkey ; ; dstpos = ( dstpos + 1 ) & dst->hashmask )
    {
      dstentry = MM_HASH_ENTRY( dst, dstpos );
      if( !( access->entryvalid( dstentry ) ) )
        break;
    }

    /* Copy entry from src table to dst table */
    memcpy( dstentry, srcentry, src->entrysize );
    srcentry = ADDRESS( srcentry, src->entrysize );
  }

  return;
}



////


static int mmHashLockRangeTry( mmHashTable *table, mmHashAccess *access, mmHashLock *hashlock, mmHashLockRange *lockrange, uint32_t hashkey )
{
  int newcount;
  uint32_t srckey;
  uint32_t pageindex, pagestart, pagefinal;
  void *srcentry;

  /* Lock first page */
  pagestart = hashkey >> table->pageshift;
  pagefinal = pagestart;
#ifdef MM_ATOMIC_SUPPORT
  if( mmAtomicReadP( &table->page[pagestart].owner ) != (void *)hashlock )
  {
    if( !( MM_HASH_LOCK_TRY_WRITE( table, pagestart ) ) )
      return MM_HASH_TRYAGAIN;
  }
  mmAtomicWriteP( &table->page[pagestart].owner, (void *)hashlock );
#else
  if( table->page[pagestart].owner != (void *)hashlock )
  {
    if( !( MM_HASH_LOCK_TRY_WRITE( table, pagestart ) ) )
      return MM_HASH_TRYAGAIN;
  }
  table->page[pagestart].owner = hashlock;
#endif

  /* Lock all pages in stream */
  newcount = hashlock->newcount;
  for( srckey = hashkey ; ; )
  {
    srckey = ( srckey + 1 ) & table->hashmask;

    /* Lock new pages */
    pageindex = srckey >> table->pageshift;
    if( pageindex != pagefinal )
    {
#ifdef MM_ATOMIC_SUPPORT
      if( mmAtomicReadP( &table->page[pageindex].owner ) != (void *)hashlock )
      {
        if( MM_HASH_LOCK_TRY_WRITE( table, pageindex ) )
        {
          mmAtomicWriteP( &table->page[pageindex].owner, (void *)hashlock );
          continue;
        }
        /* Failed, unlock all pages */
        for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
        {
          mmAtomicWriteP( &table->page[pageindex].owner, (void *)0 );
          MM_HASH_LOCK_DONE_WRITE( table, pageindex );
          if( pageindex == pagefinal )
            break;
        }
        return MM_HASH_TRYAGAIN;
      }
#else
      if( table->page[pageindex].owner != (void *)hashlock )
      {
        if( MM_HASH_LOCK_TRY_WRITE( table, pageindex ) )
        {
          table->page[pageindex].owner = (void *)hashlock;
          continue;
        }
        /* Failed, unlock all pages */
        for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
        {
          table->page[pageindex].owner = 0;
          MM_HASH_LOCK_DONE_WRITE( table, pageindex );
          if( pageindex == pagefinal )
            break;
        }
        return MM_HASH_TRYAGAIN;
      }
#endif
      pagefinal = pageindex;
    }

    /* Check for valid entry */
    srcentry = MM_HASH_ENTRY( table, srckey );
    if( !( access->entryvalid( srcentry ) ) )
    {
      if( --newcount <= 0 )
        break;
    }
  }

  /* Store lock pagestart and pagefinal */
  lockrange->pagestart = pagestart;
  lockrange->pagefinal = pagefinal;

  return MM_HASH_SUCCESS;
} 


static void mmHashLockReleaseAll( mmHashTable *table, mmHashLock *hashlock )
{
  uint32_t pageindex, pagestart, pagefinal;
  mmHashLockRange *lockrange;

  for( lockrange = hashlock->rangelist ; lockrange ; lockrange = lockrange->next )
  {
    if( lockrange->pagestart == ~(uint32_t)0x0 )
      continue;
    pagestart = lockrange->pagestart;
    pagefinal = lockrange->pagefinal;
    for( pageindex = pagestart ; ; pageindex = ( pageindex + 1 ) & table->pagemask )
    {
#ifdef MM_ATOMIC_SUPPORT
      if( mmAtomicReadP( &table->page[pageindex].owner ) == (void *)hashlock )
      {
        mmAtomicWriteP( &table->page[pageindex].owner, (void *)0 );
        mmAtomicLockDoneWrite32( &table->page[pageindex].lock );
      }
#else
      if( table->page[pageindex].owner == (void *)hashlock )
      {
        table->page[pageindex].owner = (void *)0;
        MM_HASH_LOCK_DONE_WRITE( table, pageindex );
      }
#endif
      if( pageindex == pagefinal )
        break;
    }
  }

  for( lockrange = hashlock->rangelist ; lockrange ; lockrange = lockrange->next )
  {
    lockrange->pagestart = ~0x0;
    lockrange->pagefinal = ~0x0;
  }

  return;
}


static int mmHashLockApplyAll( mmHashTable *table, mmHashAccess *access, mmHashLock *hashlock )
{
  mmHashLockRange *lockrange, *lockrangenext;
  // 2012-10-22 ch3: rangelist set but never used, so commented
  //mmHashLockRange *rangelist;

  //rangelist = hashlock->rangelist;
  for( lockrange = hashlock->rangelist ; lockrange ; lockrange = lockrangenext )
  {
    lockrangenext = lockrange->next;
    if( mmHashLockRangeTry( table, access, hashlock, lockrange, lockrange->hashkey ) == MM_HASH_TRYAGAIN )
    {
      /* Failure, release all locks */
      mmHashLockReleaseAll( table, hashlock );
      return MM_HASH_TRYAGAIN;
    }
  }

  return MM_HASH_SUCCESS;
}



////



void mmHashLockInit( mmHashLock *hashlock, int newcount )
{
  memset( hashlock, 0, sizeof(mmHashLock) );
  hashlock->newcount = newcount;
  return;
}

void mmHashLockAdd( void *hashtable, mmHashAccess *access, void *entry, mmHashLock *hashlock, mmHashLockRange *lockrange )
{
  uint32_t hashkey;
  mmHashTable *table;

  /* Hash key of entry */
  table = hashtable;
  hashkey = access->entrykey( entry ) & table->hashmask;

  lockrange->hashkey = hashkey;
  lockrange->pagestart = ~0x0;
  lockrange->pagefinal = ~0x0;
  lockrange->next = hashlock->rangelist;
  hashlock->rangelist = lockrange;

  return;
}

void mmHashLockAcquire( void *hashtable, mmHashAccess *access, mmHashLock *hashlock )
{
  int status;
  mmHashTable *table;

  table = hashtable;
  status = mmHashLockApplyAll( table, access, hashlock );
  if( status == MM_HASH_TRYAGAIN )
  {
    MM_HASH_GLOBAL_LOCK( table );
    do
    {
      status = mmHashLockApplyAll( table, access, hashlock );
    } while( status == MM_HASH_TRYAGAIN );
    MM_HASH_GLOBAL_UNLOCK( table );
  }

  return;
}

void mmHashLockRelease( void *hashtable, mmHashLock *hashlock )
{
  mmHashTable *table;

  table = hashtable;
  mmHashLockReleaseAll( table, hashlock );

  return;
}


////


void mmHashGlobalLockEnable( void *hashtable )
{
  uint32_t pageindex;
  mmHashTable *table;

  table = hashtable;
  MM_HASH_GLOBAL_LOCK( table );
  for( pageindex = 0 ; pageindex < table->pagecount ; pageindex++ )
    MM_HASH_LOCK_TRY_WRITE( table, pageindex );

  return;
}


void mmHashGlobalLockDisable( void *hashtable )
{
  uint32_t pageindex;
  mmHashTable *table;

  table = hashtable;
  for( pageindex = 0 ; pageindex < table->pagecount ; pageindex++ )
    MM_HASH_LOCK_DONE_WRITE( table, pageindex );
  MM_HASH_GLOBAL_UNLOCK( table );

  return;
}


////



void mmHashDirectDebugDuplicate( void *hashtable, mmHashAccess *access, void (*callback)( void *opaque, void *entry0, void *entry1 ), void *opaque )
{
  uint32_t hashbase, hashkey;
  void *baseentry, *entry;
  mmHashTable *table;

  table = hashtable;

  for( hashbase = 0 ; hashbase < table->hashsize ; hashbase++ )
  {
    baseentry = MM_HASH_ENTRY( table, hashbase );
    for( hashkey = hashbase ; ; )
    {
      hashkey = ( hashkey + 1 ) & table->hashmask;
      entry = MM_HASH_ENTRY( table, hashkey );
      if( !( access->entryvalid( entry ) ) )
        break;
      if( access->entrycmp( entry, baseentry ) )
        callback( opaque, entry, baseentry );
    }
  }

  return;
}


void mmHashDirectDebugPages( void *hashtable )
{
  uint32_t pageindex;
  mmHashTable *table;
  mmHashPage *page;

  table = hashtable;

#ifdef MM_ATOMIC_SUPPORT
  page = table->page;
  for( pageindex = 0 ; pageindex < table->pagecount ; pageindex++, page++ )
  {
    if( ( page->lock.v.value ) || mmAtomicReadP( &page->owner ) )
      printf( "Page[%d] = 0x%x ; %p\n", pageindex, page->lock.v.value, mmAtomicReadP( &page->owner ) );
  }
  fflush( stdout );
#endif

  return;
}



void mmHashStatistics( void *hashtable, long *accesscount, long *collisioncount, long *relocationcount, long *entrycount, long *entrycountmax, long *hashsizemax )
{
  mmHashTable *table;

  table = hashtable;
#ifdef MM_HASH_DEBUG_STATISTICS
  *accesscount = mmAtomicReadL( &table->accesscount );
  *collisioncount = mmAtomicReadL( &table->collisioncount );
  *relocationcount = mmAtomicReadL( &table->relocationcount );
  *entrycount = mmAtomicReadL( &table->statentrycount );
  *entrycountmax = table->entrycountmax;
  *hashsizemax = 1 << table->hashbits;
#else
  *accesscount = 0;
  *collisioncount = 0;
  *relocationcount = 0;
  *entrycount = 0;
  *entrycountmax = 0;
  *hashsizemax = 1 << table->hashbits;
#endif

  return;
}
