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

#include "common.h"

#include "mmhash.h"

#include "mm.h"

#include <string.h>


#define MM_ROBUST_DELETION


enum {
    MM_HASH_STATUS_MUSTGROW,
    MM_HASH_STATUS_MUSTSHRINK,
    MM_HASH_STATUS_NORMAL,
    MM_HASH_STATUS_UNKNOWN
};


typedef struct {
#ifdef MM_ATOMIC_SUPPORT
    /* Powerful atomic read/write lock */
    mmAtomicLock32 lock;
    mmAtomicP owner;
#else
    /* Mutex, or how to ruin performance */
    bu_mtx_t mutex;
    void *owner;
#endif
} mmHashPage MM_CACHE_ALIGN;


typedef struct {
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
    bu_mtx_t countmutex;
    uint32_t entrycount;
#endif
    uint32_t lowcount;
    uint32_t highcount;

    /* Global lock as the final word to resolve fighting between threads trying to access the same entry */
    char paddingA[64];
#ifdef MM_ATOMIC_SUPPORT
    mmAtomic32 globallock;
#else
    bu_mtx_t globalmutex;
#endif
    char paddingB[64];

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    long entrycountmax;
    mmAtomicL statentrycount;
    mmAtomicL accesscount;
    mmAtomicL collisioncount;
    mmAtomicL relocationcount;
#endif
} mmHashTable;


#define MM_HASH_ALIGN64(x) ((x+0x3F)&~0x3F)
#define MM_HASH_SIZEOF_ALIGN16(x) ((sizeof(x)+0xF)&~0xF)
#define MM_HASH_SIZEOF_ALIGN64(x) ((sizeof(x)+0x3F)&~0x3F)
#define MM_HASH_PAGELIST(table) (void *)ADDRESS(table,MM_HASH_ALIGN64(MM_HASH_SIZEOF_ALIGN64(mmHashTable)+((table)->hashsize*(table)->entrysize)))
#define MM_HASH_ENTRYLIST(table) (void *)ADDRESS(table,MM_HASH_SIZEOF_ALIGN64(mmHashTable))
#define MM_HASH_ENTRY(table,index) (void *)ADDRESS(table,MM_HASH_SIZEOF_ALIGN64(mmHashTable)+((index)*(table)->entrysize))


static void mmHashSetBounds(mmHashTable *table)
{
    table->lowcount = 0;

    if (table->hashbits > table->minhashbits)
	table->lowcount = table->hashsize / 5;

    table->highcount = table->hashsize / 2;
}


#ifdef MM_ATOMIC_SUPPORT

#define MM_HASH_ATOMIC_TRYCOUNT (16)

#define MM_HASH_LOCK_TRY_READ(t,p) (mmAtomicLockTryRead32(&t->page[p].lock,MM_HASH_ATOMIC_TRYCOUNT))
#define MM_HASH_LOCK_DONE_READ(t,p) (mmAtomicLockDoneRead32(&t->page[p].lock))
#define MM_HASH_LOCK_TRY_WRITE(t,p) (mmAtomicLockTryWrite32(&t->page[p].lock,MM_HASH_ATOMIC_TRYCOUNT))
#define MM_HASH_LOCK_DONE_WRITE(t,p) (mmAtomicLockDoneWrite32(&t->page[p].lock))
#define MM_HASH_GLOBAL_LOCK(t) (mmAtomicSpin32(&t->globallock,0x0,0x1))
#define MM_HASH_GLOBAL_UNLOCK(t) (mmAtomicWrite32(&t->globallock,0x0))
#define MM_HASH_ENTRYCOUNT_ADD_READ(t,c) (mmAtomicAddRead32(&table->entrycount,c))

#else

#define MM_HASH_LOCK_TRY_READ(t,p) (mtMutexTryLock(&t->page[p].mutex))
#define MM_HASH_LOCK_DONE_READ(t,p) do {if (bu_mtx_unlock(&t->page[p].mutex) != bu_thrd_success) bu_bomb("bu_mtx_unlock() failed"); } while (0)
#define MM_HASH_LOCK_TRY_WRITE(t,p) (mtMutexTryLock(&t->page[p].mutex))
#define MM_HASH_LOCK_DONE_WRITE(t,p) MM_HASH_LOCK_DONE_READ((t), (p))
#define MM_HASH_GLOBAL_LOCK(t) do {if (bu_mtx_lock(&t->globalmutex) != bu_thrd_success) bu_bomb("bu_mtx_lock() failed"); } while (0)
#define MM_HASH_GLOBAL_UNLOCK(t) do {if (bu_mtx_unlock(&t->globalmutex) != bu_thrd_success) bu_bomb("bu_mtx_unlock() failed"); } while (0)


static inline uint32_t MM_HASH_ENTRYCOUNT_ADD_READ(mmHashTable *t, int32_t c)
{
    uint32_t entrycount;

    if (bu_mtx_lock(&t->countmutex) != bu_thrd_success)
	bu_bomb("bu_mtx_lock() failed");

    t->entrycount += c;
    entrycount = t->entrycount;

    if (bu_mtx_unlock(&t->countmutex) != bu_thrd_success)
	bu_bomb("bu_mtx_unlock() failed");

    return entrycount;
}
#endif


static int mmHashTryCallEntry(mmHashTable *table, mmHashAccess *vaccess, void *callentry, void (*callback)(void *opaque, void *entry, int newflag), void *opaque, int addflag)
{
    uint32_t hashkey, entrycount;
    uint32_t pageindex, pagestart, pagefinal;
    int cmpvalue, retvalue;
    void *entry;

    /* Hash key of entry */
    hashkey = vaccess->entrykey(callentry) & table->hashmask;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    mmAtomicAddL(&table->accesscount, 1);
#endif

    /* Lock first page */
    pagestart = hashkey >> table->pageshift;
    pagefinal = pagestart;

    if (!(MM_HASH_LOCK_TRY_WRITE(table, pagestart)))
	return MM_HASH_TRYAGAIN;

    /* Search an available entry */
    retvalue = MM_HASH_SUCCESS;

    for (;; hashkey = (hashkey + 1) & table->hashmask) {
	/* Lock new pages */
	pageindex = hashkey >> table->pageshift;

	if (pageindex != pagefinal) {
	    if (!(MM_HASH_LOCK_TRY_WRITE(table, pageindex))) {
		retvalue = MM_HASH_TRYAGAIN;
		goto end;
	    }

	    pagefinal = pageindex;
	}

	/* Check for entry available */
	entry = MM_HASH_ENTRY(table, hashkey);
	cmpvalue = vaccess->entrycmp(entry, callentry);

	if (cmpvalue == MM_HASH_ENTRYCMP_INVALID)
	    break;
	else if (cmpvalue == MM_HASH_ENTRYCMP_FOUND) {
	    callback(opaque, entry, 0);
	    retvalue = MM_HASH_SUCCESS;
	    goto end;
	}

#ifdef MM_HASH_RT_DEBUG_STATISTICS
	mmAtomicAddL(&table->collisioncount, 1);
#endif
    }

    if (!(addflag)) {
	retvalue = MM_HASH_FAILURE;
	goto end;
    }

    /* Store new entry */
    memcpy(entry, callentry, table->entrysize);
    callback(opaque, entry, 1);

    /* Increment count of entries in table */
    if (!(table->flags & MM_HASH_FLAGS_NO_COUNT)) {
	entrycount = MM_HASH_ENTRYCOUNT_ADD_READ(table, 1);

	if (entrycount >= table->highcount)
	    table->status = MM_HASH_STATUS_MUSTGROW;
    }

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    int statentrycount = mmAtomicAddReadL(&table->statentrycount, 1);

    if (statentrycount > table->entrycountmax)
	table->entrycountmax = statentrycount;

#endif

end:

    /* Unlock all pages */
    for (pageindex = pagestart;; pageindex = (pageindex + 1) & table->pagemask) {
	MM_HASH_LOCK_DONE_WRITE(table, pageindex);

	if (pageindex == pagefinal)
	    break;
    }

    return retvalue;
}


static int mmHashTryDeleteEntry(mmHashTable *table, mmHashAccess *vaccess, void *deleteentry, int readflag)
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
    hashkey = vaccess->entrykey(deleteentry) & table->hashmask;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    mmAtomicAddL(&table->accesscount, 1);
#endif

    /* Lock first page */
    pagestart = hashkey >> table->pageshift;
    pagefinal = pagestart;

    if (!(MM_HASH_LOCK_TRY_WRITE(table, pagestart)))
	return MM_HASH_TRYAGAIN;

    /* Search the entry */
    retvalue = MM_HASH_SUCCESS;

    for (;; hashkey = (hashkey + 1) & table->hashmask) {
	/* Lock new pages */
	pageindex = hashkey >> table->pageshift;

	if (pageindex != pagefinal) {
	    if (!(MM_HASH_LOCK_TRY_WRITE(table, pageindex))) {
		retvalue = MM_HASH_TRYAGAIN;
		goto end;
	    }

	    pagefinal = pageindex;
	}

	/* Check for entry match */
	entry = MM_HASH_ENTRY(table, hashkey);
	cmpvalue = vaccess->entrycmp(entry, deleteentry);

	if (cmpvalue == MM_HASH_ENTRYCMP_INVALID) {
	    retvalue = MM_HASH_FAILURE;
	    goto end;
	} else if (cmpvalue == MM_HASH_ENTRYCMP_FOUND)
	    break;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
	mmAtomicAddL(&table->collisioncount, 1);
#endif
    }

    if (readflag)
	memcpy(deleteentry, entry, table->entrysize);

#ifdef MM_ROBUST_DELETION

    for (delbase = hashkey;;) {
	delbase = (delbase - 1) & table->hashmask;

	if (!(vaccess->entryvalid(MM_HASH_ENTRY(table, delbase))))
	    break;
    }

    delbase = (delbase + 1) & table->hashmask;
#else
    halftablesize = table->hashsize >> 1;
#endif

    /* Preemptively lock all pages in the stream before starting the operation */
    for (srcend = hashkey;;) {
	srcend = (srcend + 1) & table->hashmask;

	/* Lock new pages */
	pageindex = srcend >> table->pageshift;

	if (pageindex != pagefinal) {
	    if (!(MM_HASH_LOCK_TRY_WRITE(table, pageindex))) {
		retvalue = MM_HASH_TRYAGAIN;
		goto end;
	    }

	    pagefinal = pageindex;
	}

	/* Check for valid entry */
	srcentry = MM_HASH_ENTRY(table, srcend);

	if (!(vaccess->entryvalid(srcentry)))
	    break;
    }

    /* Entry found, delete it and reorder the hash stream of entries */
    for (;;) {
	/* Find replacement target */
	targetkey = hashkey;
	targetentry = 0;

	for (srcpos = hashkey;;) {
	    srcpos = (srcpos + 1) & table->hashmask;
	    srcentry = MM_HASH_ENTRY(table, srcpos);

	    /* Don't loop beyond the end of hash stream */
	    if (srcpos == srcend)
		break;

	    /* Try next entry */
	    srckey = vaccess->entrykey(srcentry) & table->hashmask;

#ifdef MM_ROBUST_DELETION

	    if (targetkey >= delbase) {
		if ((srckey < delbase) || (srckey > targetkey))
		    continue;
	    } else if ((srckey > targetkey) && (srckey < delbase))
		continue;

	    targetentry = srcentry;
	    targetkey = srckey;
	    targetpos = srcpos;
#else
	    /* Check if moving the entry backwards is allowed without breaking chain */
	    distance = (int32_t)targetkey - (int32_t)srckey;

	    if (distance > halftablesize)
		distance -= table->hashsize;
	    else if (distance < -halftablesize)
		distance += table->hashsize;

	    if (distance >= 0) {
		targetentry = srcentry;
		targetkey = srckey;
		targetpos = srcpos;
	    }

#endif
	}

	/* No replacement found, just clear it */
	entry = MM_HASH_ENTRY(table, hashkey);

	if (!(targetentry)) {
	    vaccess->clearentry(entry);
	    break;
	}

	/* Move entry in place and continue the repair process */
	memcpy(entry, targetentry, table->entrysize);

#ifdef MM_HASH_RT_DEBUG_STATISTICS
	mmAtomicAddL(&table->relocationcount, 1);
#endif

	hashkey = targetpos;
    }

    /* Decrement count of entries in table */
    if (!(table->flags & MM_HASH_FLAGS_NO_COUNT)) {
	entrycount = MM_HASH_ENTRYCOUNT_ADD_READ(table, -1);

	if (entrycount < table->lowcount)
	    table->status = MM_HASH_STATUS_MUSTSHRINK;
    }

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    mmAtomicAddL(&table->statentrycount, -1);
#endif

end:

    /* Unlock all pages */
    for (pageindex = pagestart;; pageindex = (pageindex + 1) & table->pagemask) {
	MM_HASH_LOCK_DONE_WRITE(table, pageindex);

	if (pageindex == pagefinal)
	    break;
    }

    return retvalue;
}


static int mmHashTryReadEntry(mmHashTable *table, mmHashAccess *vaccess, void *readentry)
{
    uint32_t hashkey;
    uint32_t pageindex, pagestart, pagefinal;
    int cmpvalue, retvalue;
    void *entry;

    /* Hash key of entry */
    hashkey = vaccess->entrykey(readentry) & table->hashmask;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    mmAtomicAddL(&table->accesscount, 1);
#endif

    /* Lock first page */
    pagestart = hashkey >> table->pageshift;
    pagefinal = pagestart;

    if (!(MM_HASH_LOCK_TRY_READ(table, pagestart)))
	return MM_HASH_TRYAGAIN;

    /* Search the entry */
    entry = 0;
    retvalue = MM_HASH_SUCCESS;

    for (;; hashkey = (hashkey + 1) & table->hashmask) {
	/* Lock new pages */
	pageindex = hashkey >> table->pageshift;

	if (pageindex != pagefinal) {
	    if (!(MM_HASH_LOCK_TRY_READ(table, pageindex))) {
		retvalue = MM_HASH_TRYAGAIN;
		break;
	    }

	    pagefinal = pageindex;
	}

	/* Check for entry match */
	entry = MM_HASH_ENTRY(table, hashkey);
	cmpvalue = vaccess->entrycmp(entry, readentry);

	if (cmpvalue == MM_HASH_ENTRYCMP_INVALID) {
	    retvalue = MM_HASH_FAILURE;
	    entry = 0;
	    break;
	} else if (cmpvalue == MM_HASH_ENTRYCMP_FOUND)
	    break;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
	mmAtomicAddL(&table->collisioncount, 1);
#endif
    }

    if (entry)
	memcpy(readentry, entry, table->entrysize);

    /* Unlock all pages */
    for (pageindex = pagestart;; pageindex = (pageindex + 1) & table->pagemask) {
	MM_HASH_LOCK_DONE_READ(table, pageindex);

	if (pageindex == pagefinal)
	    break;
    }

    return retvalue;
}


static int mmHashTryAddEntry(mmHashTable *table, mmHashAccess *vaccess, void *addentry, int nodupflag)
{
    uint32_t hashkey, entrycount;
    uint32_t pageindex, pagestart, pagefinal;
    int cmpvalue, retvalue;
    void *entry;

    /* Hash key of entry */
    hashkey = vaccess->entrykey(addentry) & table->hashmask;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    mmAtomicAddL(&table->accesscount, 1);
#endif

    /* Lock first page */
    pagestart = hashkey >> table->pageshift;
    pagefinal = pagestart;

    if (!(MM_HASH_LOCK_TRY_WRITE(table, pagestart)))
	return MM_HASH_TRYAGAIN;

    /* Search an available entry */
    retvalue = MM_HASH_SUCCESS;

    for (;; hashkey = (hashkey + 1) & table->hashmask) {
	/* Lock new pages */
	pageindex = hashkey >> table->pageshift;

	if (pageindex != pagefinal) {
	    if (!(MM_HASH_LOCK_TRY_WRITE(table, pageindex))) {
		retvalue = MM_HASH_TRYAGAIN;
		goto end;
	    }

	    pagefinal = pageindex;
	}

	/* Check for entry available */
	entry = MM_HASH_ENTRY(table, hashkey);

	/* Do we allow duplicate entries? */
	if (nodupflag) {
	    cmpvalue = vaccess->entrycmp(entry, addentry);

	    if (cmpvalue == MM_HASH_ENTRYCMP_INVALID)
		break;
	    else if (cmpvalue == MM_HASH_ENTRYCMP_FOUND) {
		retvalue = MM_HASH_FAILURE;
		goto end;
	    }
	} else {
	    if (!(vaccess->entryvalid(entry)))
		break;
	}

#ifdef MM_HASH_RT_DEBUG_STATISTICS
	mmAtomicAddL(&table->collisioncount, 1);
#endif
    }

    /* Store new entry */
    memcpy(entry, addentry, table->entrysize);

    /* Increment count of entries in table */
    if (!(table->flags & MM_HASH_FLAGS_NO_COUNT)) {
	entrycount = MM_HASH_ENTRYCOUNT_ADD_READ(table, 1);

	if (entrycount >= table->highcount)
	    table->status = MM_HASH_STATUS_MUSTGROW;
    }

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    int statentrycount = mmAtomicAddReadL(&table->statentrycount, 1);

    if (statentrycount > table->entrycountmax)
	table->entrycountmax = statentrycount;

#endif

end:

    /* Unlock all pages */
    for (pageindex = pagestart;; pageindex = (pageindex + 1) & table->pagemask) {
	MM_HASH_LOCK_DONE_WRITE(table, pageindex);

	if (pageindex == pagefinal)
	    break;
    }

    return retvalue;
}


static int mmHashTryFindEntry(mmHashTable *table, mmHashAccess *vaccess, void *findentry, void **retentry)
{
    uint32_t hashkey;
    uint32_t pageindex, pagestart, pagefinal;
    int cmpvalue, retvalue;
    void *entry;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    mmAtomicAddL(&table->accesscount, 1);
#endif

    /* Hash key of entry */
    hashkey = vaccess->entrykey(findentry) & table->hashmask;

    /* Lock first page */
    pagestart = hashkey >> table->pageshift;
    pagefinal = pagestart;

    if (!(MM_HASH_LOCK_TRY_READ(table, pagestart)))
	return MM_HASH_TRYAGAIN;

    /* Search the entry */
    entry = 0;
    retvalue = MM_HASH_SUCCESS;

    for (;; hashkey = (hashkey + 1) & table->hashmask) {
	/* Lock new pages */
	pageindex = hashkey >> table->pageshift;

	if (pageindex != pagefinal) {
	    if (!(MM_HASH_LOCK_TRY_READ(table, pageindex))) {
		retvalue = MM_HASH_TRYAGAIN;
		break;
	    }

	    pagefinal = pageindex;
	}

	/* Check for entry match */
	entry = MM_HASH_ENTRY(table, hashkey);
	cmpvalue = vaccess->entrycmp(entry, findentry);

	if (cmpvalue == MM_HASH_ENTRYCMP_INVALID) {
	    retvalue = MM_HASH_FAILURE;
	    entry = 0;
	    break;
	} else if (cmpvalue == MM_HASH_ENTRYCMP_FOUND)
	    break;

#ifdef MM_HASH_RT_DEBUG_STATISTICS
	mmAtomicAddL(&table->collisioncount, 1);
#endif
    }

    /* Unlock all pages */
    for (pageindex = pagestart;; pageindex = (pageindex + 1) & table->pagemask) {
	MM_HASH_LOCK_DONE_READ(table, pageindex);

	if (pageindex == pagefinal)
	    break;
    }

    *retentry = entry;
    return retvalue;
}


/****/


void mmHashInit(void *hashtable, mmHashAccess *vaccess, size_t entrysize, uint32_t hashbits, uint32_t pageshift, uint32_t flags)
{
    uint32_t hashkey, pageindex;
    void *entry;
    mmHashTable *table;
    mmHashPage *page;

    table = (mmHashTable *)hashtable;
    table->status = MM_HASH_STATUS_NORMAL;

    if (flags & MM_HASH_FLAGS_NO_COUNT)
	table->status = MM_HASH_STATUS_UNKNOWN;

    table->flags = flags;
    table->entrysize = entrysize;
    table->minhashbits = hashbits;
    table->hashbits = hashbits;
    table->hashsize = 1 << table->hashbits;
    table->hashmask = table->hashsize - 1;
    table->pageshift = pageshift;
    table->pagecount = table->hashsize >> pageshift;

    if (!(table->pagecount))
	table->pagecount = 1;

    table->pagemask = table->pagecount - 1;
    table->page = (mmHashPage *)MM_HASH_PAGELIST(table);
#ifdef MM_ATOMIC_SUPPORT
    mmAtomicWrite32(&table->entrycount, 0);
#else

    if (bu_mtx_init(&table->countmutex) != bu_thrd_success)
	bu_bomb("bu_mtx_init() failed");

    table->entrycount = 0;
#endif
    mmHashSetBounds(table);

    /* Clear the table */
    entry = MM_HASH_ENTRYLIST(table);

    for (hashkey = 0; hashkey < table->hashsize; hashkey++) {
	vaccess->clearentry(entry);
	entry = ADDRESS(entry, entrysize);
    }

    /* Clear the lock pages */
    page = table->page;
#ifdef MM_ATOMIC_SUPPORT

    for (pageindex = table->pagecount; pageindex; pageindex--, page++) {
	mmAtomicLockInit32(&page->lock);
	mmAtomicWriteP(&page->owner, 0);
    }

    mmAtomicWrite32(&table->globallock, 0x0);
#else

    for (pageindex = table->pagecount; pageindex; pageindex--, page++) {
	if (bu_mtx_init(&page->mutex) != bu_thrd_success)
	    bu_bomb("bu_mtx_init() failed");

	page->owner = 0;
    }

    if (bu_mtx_init(&table->globalmutex) != bu_thrd_success)
	bu_bomb("bu_mtx_init() failed");

#endif

#ifdef MM_HASH_RT_DEBUG_STATISTICS
    table->entrycountmax = 0;
    mmAtomicWriteL(&table->statentrycount, 0);
    mmAtomicWriteL(&table->accesscount, 0);
    mmAtomicWriteL(&table->collisioncount, 0);
    mmAtomicWriteL(&table->relocationcount, 0);
#endif
}


size_t mmHashRequiredSize(size_t entrysize, uint32_t hashbits, uint32_t pageshift)
{
    uint32_t entrycount;
    uint32_t pagecount;
    entrycount = 1 << hashbits;
    pagecount = entrycount >> pageshift;

    if (!(pagecount))
	pagecount = 1;

    return MM_HASH_ALIGN64(MM_HASH_SIZEOF_ALIGN16(mmHashTable) + (entrycount * entrysize)) + (pagecount * sizeof(mmHashPage));
}

int mmHashLockCallEntry(void *hashtable, mmHashAccess *vaccess, void *callentry, void (*callback)(void *opaque, void *entry, int newflag), void *opaque, int addflag)
{
    int retvalue;
    mmHashTable *table;

    table = (mmHashTable *)hashtable;
    retvalue = mmHashTryCallEntry(table, vaccess, callentry, callback, opaque, addflag);

    if (retvalue == MM_HASH_TRYAGAIN) {
	MM_HASH_GLOBAL_LOCK(table);

	do {
	    retvalue = mmHashTryCallEntry(table, vaccess, callentry, callback, opaque, addflag);
	} while (retvalue == MM_HASH_TRYAGAIN);

	MM_HASH_GLOBAL_UNLOCK(table);
    }

    return retvalue;
}


int mmHashLockDeleteEntry(void *hashtable, mmHashAccess *vaccess, void *deleteentry, int readflag)
{
    int retvalue;
    mmHashTable *table;

    table = (mmHashTable *)hashtable;
    retvalue = mmHashTryDeleteEntry(table, vaccess, deleteentry, readflag);

    if (retvalue == MM_HASH_TRYAGAIN) {
	MM_HASH_GLOBAL_LOCK(table);

	do {
	    retvalue = mmHashTryDeleteEntry(table, vaccess, deleteentry, readflag);
	} while (retvalue == MM_HASH_TRYAGAIN);

	MM_HASH_GLOBAL_UNLOCK(table);
    }

    return retvalue;
}


int mmHashLockReadEntry(void *hashtable, mmHashAccess *vaccess, void *readentry)
{
    int retvalue;
    mmHashTable *table;

    table = (mmHashTable *)hashtable;
    retvalue = mmHashTryReadEntry(table, vaccess, readentry);

    if (retvalue == MM_HASH_TRYAGAIN) {
	MM_HASH_GLOBAL_LOCK(table);

	do {
	    retvalue = mmHashTryReadEntry(table, vaccess, readentry);
	} while (retvalue == MM_HASH_TRYAGAIN);

	MM_HASH_GLOBAL_UNLOCK(table);
    }

    return retvalue;
}


int mmHashLockAddEntry(void *hashtable, mmHashAccess *vaccess, void *addentry, int nodupflag)
{
    int retvalue;
    mmHashTable *table;

    table = (mmHashTable *)hashtable;
    retvalue = mmHashTryAddEntry(table, vaccess, addentry, nodupflag);

    if (retvalue == MM_HASH_TRYAGAIN) {
	MM_HASH_GLOBAL_LOCK(table);

	do {
	    retvalue = mmHashTryAddEntry(table, vaccess, addentry, nodupflag);
	} while (retvalue == MM_HASH_TRYAGAIN);

	MM_HASH_GLOBAL_UNLOCK(table);
    }

    return retvalue;
}


void *mmHashLockFindEntry(void *hashtable, mmHashAccess *vaccess, void *findentry)
{
    int retvalue;
    void *entry;
    mmHashTable *table;

    table = (mmHashTable *)hashtable;
    retvalue = mmHashTryFindEntry(table, vaccess, findentry, &entry);

    if (retvalue == MM_HASH_TRYAGAIN) {
	MM_HASH_GLOBAL_LOCK(table);

	do {
	    retvalue = mmHashTryFindEntry(table, vaccess, findentry, &entry);
	} while (retvalue == MM_HASH_TRYAGAIN);

	MM_HASH_GLOBAL_UNLOCK(table);
    }

    return entry;
}
