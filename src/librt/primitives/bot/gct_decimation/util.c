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

#include "common.h"

#include "./util.h"

#include "bu/malloc.h"
#include "bu/parallel.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#define ADDRESSDIFF(a,b) (((char *)a)-((char *)b))


typedef struct {
    int padding;
} mmAlign;


#define MM_BTREE_FLAGS_LEFT (0)
#define MM_BTREE_FLAGS_RIGHT (1)
#define MM_BTREE_FLAGS_DIRECTION_MASK (1)
#define MM_BTREE_FLAGS_STEP (2)
typedef struct {
    void *child[2];
    void *parent;
    int flags;
} mmBTreeNode;


typedef struct {
    mmListNode listnode;
    mmBTreeNode node;
    int freecount;
    int blockindex;
} mmBlock;


static int mmInitStatus = 0;


static void *mmAlignRelayAlloc(void *(*relayalloc)(void *head, size_t bytes), void *relayvalue, size_t bytes, intptr_t align, size_t displacement)
{
    intptr_t i;
    void *v;
    mmAlign *malign;
    align--;

    if (!(v = relayalloc(relayvalue, bytes + align + sizeof(mmAlign) + displacement)))
	return 0;

    i = ((intptr_t)v + align + sizeof(mmAlign) + displacement) & ~align;
    i -= displacement;
    malign = (mmAlign *)ADDRESS((void *)i, -(intptr_t)sizeof(mmAlign));
    malign->padding = ADDRESSDIFF(i, v);
    return (void *)i;
}


/**
 * Private function to balance a branch of the tree after an insertion.
 *
 * The details of the implementation are left as an exercise to the reader.
 */
static void mmBTreeInsertBalance(void *item, intptr_t offset, void **root)
{
    void *parent, *relative, *ancestor, *vlink;
    mmBTreeNode *node, *pnode, *rnode, *anode, *lnode, *tnode;

    node = (mmBTreeNode *)ADDRESS(item, offset);
    parent = node->parent;

    if (!(parent)) {
	node->flags |= MM_BTREE_FLAGS_STEP;
	*root = item;
	return;
    }

    pnode = (mmBTreeNode *)ADDRESS(parent, offset);

    if (pnode->flags & MM_BTREE_FLAGS_STEP)
	return;

    ancestor = pnode->parent;
    anode = (mmBTreeNode *)ADDRESS(ancestor, offset);

    relative = anode->child[(pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK) ^ 1];

    if ((relative) && !((rnode = (mmBTreeNode *)ADDRESS(relative, offset))->flags & MM_BTREE_FLAGS_STEP)) {
	anode->flags &= ~MM_BTREE_FLAGS_STEP;
	pnode->flags |= MM_BTREE_FLAGS_STEP;
	rnode->flags |= MM_BTREE_FLAGS_STEP;
	mmBTreeInsertBalance(ancestor, offset, root);
	return;
    }

    if ((node->flags & MM_BTREE_FLAGS_DIRECTION_MASK) != (pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK)) {
	if ((node->flags & MM_BTREE_FLAGS_DIRECTION_MASK) == MM_BTREE_FLAGS_RIGHT) {
	    node->flags = (anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK) | MM_BTREE_FLAGS_STEP;
	    vlink = anode->parent;

	    anode->parent = item;
	    anode->flags = MM_BTREE_FLAGS_RIGHT;
	    anode->child[0] = node->child[1];

	    if (anode->child[0]) {
		tnode = (mmBTreeNode *)ADDRESS(anode->child[0], offset);
		tnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
		tnode->parent = ancestor;
	    }

	    pnode->parent = item;
	    pnode->child[1] = node->child[0];

	    if (pnode->child[1]) {
		tnode = (mmBTreeNode *)ADDRESS(pnode->child[1], offset);
		tnode->flags |= MM_BTREE_FLAGS_RIGHT;
		tnode->parent = parent;
	    }

	    if (relative)
		((mmBTreeNode *)ADDRESS(relative, offset))->flags |= MM_BTREE_FLAGS_STEP;

	    node->child[0] = parent;
	    node->child[1] = ancestor;
	    node->parent = vlink;

	    if (vlink) {
		lnode = (mmBTreeNode *)ADDRESS(vlink, offset);
		lnode->child[node->flags & MM_BTREE_FLAGS_DIRECTION_MASK] = item;
		return;
	    }

	    *root = item;
	    return;
	} else {
	    node->flags = (anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK) | MM_BTREE_FLAGS_STEP;
	    vlink = anode->parent;

	    anode->parent = item;
	    anode->flags = 0;
	    anode->child[1] = node->child[0];

	    if (anode->child[1]) {
		tnode = (mmBTreeNode *)ADDRESS(anode->child[1], offset);
		tnode->flags |= MM_BTREE_FLAGS_RIGHT;
		tnode->parent = ancestor;
	    }

	    pnode->parent = item;
	    pnode->child[0] = node->child[1];

	    if (pnode->child[0]) {
		tnode = (mmBTreeNode *)ADDRESS(pnode->child[0], offset);
		tnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
		tnode->parent = parent;
	    }

	    if (relative)
		((mmBTreeNode *)ADDRESS(relative, offset))->flags |= MM_BTREE_FLAGS_STEP;

	    node->child[0] = ancestor;
	    node->child[1] = parent;
	    node->parent = vlink;

	    if (vlink) {
		lnode = (mmBTreeNode *)ADDRESS(vlink, offset);
		lnode->child[node->flags & MM_BTREE_FLAGS_DIRECTION_MASK] = item;
		return;
	    }

	    *root = item;
	    return;
	}
    }

    if ((node->flags & MM_BTREE_FLAGS_DIRECTION_MASK) == MM_BTREE_FLAGS_RIGHT) {
	pnode->flags = (anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK) | MM_BTREE_FLAGS_STEP;
	vlink = anode->parent;

	anode->parent = parent;
	anode->flags = 0;
	anode->child[1] = pnode->child[0];

	if (anode->child[1]) {
	    tnode = (mmBTreeNode *)ADDRESS(anode->child[1], offset);
	    tnode->flags |= MM_BTREE_FLAGS_RIGHT;
	    tnode->parent = ancestor;
	}

	pnode->child[0] = ancestor;
	pnode->child[1] = item;
	pnode->parent = vlink;

	if (vlink) {
	    lnode = (mmBTreeNode *)ADDRESS(vlink, offset);
	    lnode->child[pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK] = parent;
	    return;
	}

	*root = parent;
	return;
    } else {
	pnode->flags = (anode->flags & MM_BTREE_FLAGS_DIRECTION_MASK) | MM_BTREE_FLAGS_STEP;
	vlink = anode->parent;

	anode->parent = parent;
	anode->flags = MM_BTREE_FLAGS_RIGHT;
	anode->child[0] = pnode->child[1];

	if (anode->child[0]) {
	    tnode = (mmBTreeNode *)ADDRESS(anode->child[0], offset);
	    tnode->flags &= ~MM_BTREE_FLAGS_RIGHT;
	    tnode->parent = ancestor;
	}

	pnode->child[0] = item;
	pnode->child[1] = ancestor;
	pnode->parent = vlink;

	if (vlink) {
	    lnode = (mmBTreeNode *)ADDRESS(vlink, offset);
	    lnode->child[pnode->flags & MM_BTREE_FLAGS_DIRECTION_MASK] = parent;
	    return;
	}

	*root = parent;
	return;
    }
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
static void mmBTreeInsert(void *item, void *parent, int itemflag, intptr_t offset, void **root)
{
    mmBTreeNode *node, *pnode;

    node = (mmBTreeNode *)ADDRESS(item, offset);
    node->parent = parent;
    node->child[0] = 0;
    node->child[1] = 0;
    node->flags = itemflag;

    if (parent) {
	pnode = (mmBTreeNode *)ADDRESS(parent, offset);
	pnode->child[itemflag] = item;
    }

    mmBTreeInsertBalance(item, offset, root);
}


static void mmBlockTreeInsert(mmBlock *block, void **treeroot)
{
    mmBlock *root = *(mmBlock **)treeroot;

    if (!(root)) {
	mmBTreeInsert(block, 0, 0, offsetof(mmBlock, node), treeroot);
	return;
    }

    for (;;) {
	if (block < root) {
	    if (root->node.child[0]) {
		root = (mmBlock *)root->node.child[0];
		continue;
	    }

	    mmBTreeInsert(block, root, MM_BTREE_FLAGS_LEFT, offsetof(mmBlock, node), treeroot);
	    break;
	} else {
	    if (root->node.child[1]) {
		root = (mmBlock *)root->node.child[1];
		continue;
	    }

	    mmBTreeInsert(block, root, MM_BTREE_FLAGS_RIGHT, offsetof(mmBlock, node), treeroot);
	    break;
	}
    }
}


static mmBlock *mmBlockResolveChunk(void *p, mmBlock *root)
{
    mmBlock *best = 0;

    for (; root;) {
	if (p < (void *)root)
	    root = (mmBlock *)root->node.child[0];
	else {
	    best = root;
	    root = (mmBlock *)root->node.child[1];
	}
    }

    return best;
}


static void mmAlignRelayFree(void (*relayfree)(void *head, void *v, size_t bytes), void *relayvalue, void *v, size_t bytes)
{
    mmAlign *malign;
    malign = (mmAlign *)ADDRESS(v, -(intptr_t)sizeof(mmAlign));
    relayfree(relayvalue, ADDRESS(v, -(intptr_t)malign->padding), bytes);
}


static void *mmNodeRelayAlloc(void *head, size_t bytes)
{
#ifdef MM_NUMA

    if (mmcontext.numaflag)
	return numa_alloc_onnode(bytes, (int)((intptr_t)head));

#else
    (void)head;
    (void)bytes;
#endif
    return bu_malloc(bytes, "mmNodeRelayAloc");
}

static void mmNodeRelayFree(void *UNUSED(head), void *v, size_t bytes)
{
#ifdef MM_NUMA

    if (mmcontext.numaflag) {
	numa_free(v, bytes);
	return;
    }

#else
    (void)bytes;
#endif
    bu_free(v, "mmNodeRelayFree()");
}


static void *mmAlloc(void *UNUSED(unused), size_t bytes)
{
    return bu_calloc(1, bytes, "mmAlloc()");
}


static void mmFree(void *UNUSED(unused), void *v, size_t UNUSED(bytes))
{
    bu_free(v, "mmFree()");
}


/****/


mmContext mmcontext;


#ifndef MM_ATOMIC_SUPPORT
#include <limits.h>
#include <stdlib.h>
void mmBlockProcessList(mmBlockHead *head, void *userpointer, int (*processchunk)(void *chunk, void *userpointer))
{
    const int INTPTR_BITS = sizeof(intptr_t) * CHAR_BIT;
    const int INTPTR_BITSHIFT = ccLog2Int(INTPTR_BITS);

    int i, blockcount, blockrefsize, chunkperblock;
    intptr_t **bitsref;
    intptr_t *blockmask;
    intptr_t blockindex, chunkindex;
    size_t chunksize;
    void *p, *chunk;
    mmBlock *block;
    mmListNode *list;

    mtSpinLock(&head->spinlock);

    blockcount = 0;

    for (block = (mmBlock *)head->blocklist; block; block = (mmBlock *)block->listnode.next)
	block->blockindex = blockcount++;

    chunksize = head->chunksize;
    chunkperblock = head->chunkperblock;
    blockrefsize = ((chunkperblock + INTPTR_BITS - 1) >> INTPTR_BITSHIFT) * sizeof(intptr_t);
    bitsref = (intptr_t **)bu_malloc(blockcount * (sizeof(intptr_t *) + blockrefsize), "bitsref");
    memset(bitsref, 0, blockcount * (sizeof(intptr_t *) + blockrefsize));

    p = ADDRESS(bitsref, blockcount * sizeof(intptr_t *));

    for (i = 0; i < blockcount; i++) {
	bitsref[i] = (intptr_t *)p;
	p = ADDRESS(p, blockrefsize);
    }

    for (list = (mmListNode *)head->freelist; list; list = (mmListNode *)list->next) {
	block = mmBlockResolveChunk(list, (mmBlock *)head->treeroot);
	chunkindex = ADDRESSDIFF(list, ADDRESS(block, sizeof(mmBlock))) / chunksize;
	bitsref[block->blockindex][chunkindex >> INTPTR_BITSHIFT] |= (intptr_t)1 << (chunkindex & (INTPTR_BITS - 1));
    }

    blockindex = 0;

    for (block = (mmBlock *)head->blocklist; block; block = (mmBlock *)block->listnode.next) {
	blockmask = bitsref[blockindex];
	chunk = ADDRESS(block, sizeof(mmBlock));

	for (chunkindex = 0; chunkindex < chunkperblock; chunkindex++, chunk = ADDRESS(chunk, chunksize)) {
	    if (blockmask[chunkindex >> INTPTR_BITSHIFT] & ((intptr_t)1 << (chunkindex & (INTPTR_BITS - 1))))
		continue;

	    if (processchunk(chunk, userpointer))
		goto end;
	}

	blockindex++;
    }

end:
    bu_free(bitsref, "bitsref");
    mtSpinUnlock(&head->spinlock);
}
#endif


void mmInit()
{
    int64_t sysmemory;

    if (mmInitStatus)
	return;

    mmcontext.numaflag = 0;
#ifdef MM_NUMA
    mmNumaInit();
#endif

    if (!(mmcontext.numaflag)) {
	mmcontext.nodecount = 1;
#if defined(_SC_PAGESIZE) && (defined(MM_LINUX) || defined(MM_UNIX))
	mmcontext.pagesize = sysconf(_SC_PAGESIZE);
#elif defined(MM_WIN32)
	{
	    SYSTEM_INFO sysinfo;
	    GetSystemInfo(&sysinfo);
	    mmcontext.pagesize = sysinfo.dwPageSize;
	}
#else
	mmcontext.pagesize = BU_PAGE_SIZE;
#endif
#if defined(MM_UNIX) && defined(_SC_PHYS_PAGES)
	sysmemory = sysconf(_SC_PHYS_PAGES);

	if (sysmemory > 0)
	    sysmemory *= mmcontext.pagesize;

#endif
	mmcontext.sysmemory = sysmemory;
	mmcontext.nodesize[0] = mmcontext.sysmemory;
    }

    mmInitStatus = 1;
}


/**
 * Allocate a chunk of memory with the alignment specified.
 *
 * The alignment parameter must be a power of two. The allocated memory must be
 * freed with the mmAlignFree() function.
 */
void *mmAlignAlloc(size_t bytes, intptr_t align)
{
    intptr_t i;
    void *v;
    mmAlign *malign;
    align--;

    v = bu_malloc(bytes + align + sizeof(mmAlign), "mmAlignAlloc()");

    i = ((intptr_t)v + align + sizeof(mmAlign)) & ~align;
    malign = (mmAlign *)ADDRESS((void *)i, -(intptr_t)sizeof(mmAlign));
    malign->padding = ADDRESSDIFF(i, v);
    return (void *)i;
}

/**
 * Free a chunk of memory that was allocated by mmAlignAlloc().
 */
void mmAlignFree(void *v)
{
    mmAlign *malign;
    malign = (mmAlign *)ADDRESS(v, -(intptr_t)sizeof(mmAlign));
    bu_free(ADDRESS(v, -malign->padding), "mmAlignFree()");
}

/**
 * Allocates a chunk of memory from the block.
 *
 * The size of the memory chunk returned was defined during the initialisation
 * of the specified memory block head.
 */
void *mmBlockAlloc(mmBlockHead *head)
{
    int a;
    mmBlock *block;
    void *chunk;
    mtSpinLock(&head->spinlock);

    if (!(head->freelist)) {
	if (head->alignment)
	    block = (mmBlock *)mmAlignRelayAlloc(head->relayalloc, head->relayvalue, head->allocsize, head->alignment, sizeof(mmBlock));
	else
	    block = (mmBlock *)head->relayalloc(head->relayvalue, head->allocsize);

	if (!(block))
	    bu_bomb("mmBlockAlloc(): failed");

	block->freecount = head->chunkperblock;
	mmListAdd(&head->blocklist, block, offsetof(mmBlock, listnode));
	chunk = ADDRESS(block, sizeof(mmBlock));

	for (a = 0; a < head->chunkperblock; a++, chunk = ADDRESS(chunk, head->chunksize))
	    mmListAdd(&head->freelist, chunk, 0);

	mmBlockTreeInsert(block, &head->treeroot);
	head->chunkfreecount += head->chunkperblock;
    }

    chunk = head->freelist;
    block = (mmBlock *)mmBlockResolveChunk(chunk, (mmBlock *)head->treeroot);
    mmListRemove(chunk, 0);
    block->freecount--;
    head->chunkfreecount--;
    mtSpinUnlock(&head->spinlock);
    return chunk;
}


/**
 * Release a chunk of memory previously allocated by mmBlockAlloc().
 *
 * Unlike mmBlockFree(), this function will collect but never free unused
 * memory, improving performance if the memory pages are to be soon reused.
 */
void mmBlockRelease(mmBlockHead *head, void *v)
{
    mmBlock *block;
    void *chunk;
    chunk = v;
    mtSpinLock(&head->spinlock);
    block = (mmBlock *)mmBlockResolveChunk(chunk, (mmBlock *)head->treeroot);
    block->freecount++;
    head->chunkfreecount++;
    mmListAdd(&head->freelist, chunk, 0);
    mtSpinUnlock(&head->spinlock);
}


/**
 * Free all memory allocated by a block head.
 */
void mmBlockFreeAll(mmBlockHead *head)
{
    mmBlock *block, *blocknext;
    mtSpinLock(&head->spinlock);

    for (block = (mmBlock *)head->blocklist; block; block = blocknext) {
	blocknext = (mmBlock *)block->listnode.next;

	if (head->alignment)
	    mmAlignRelayFree(head->relayfree, head->relayvalue, block, head->allocsize);
	else
	    head->relayfree(head->relayvalue, block, head->allocsize);
    }

    head->blocklist = 0;
    head->freelist = 0;
    head->treeroot = 0;
    mtSpinUnlock(&head->spinlock);
    mtSpinDestroy(&head->spinlock);
}

void mmBlockNodeInit(mmBlockHead *head, int nodeindex, size_t chunksize, int chunkperblock, int keepfreecount, int alignment)
{
    mmBlockInit(head, chunksize, chunkperblock, keepfreecount, alignment);
    head->relayalloc = mmNodeRelayAlloc;
    head->relayfree = mmNodeRelayFree;
    head->relayvalue = (void *)((intptr_t)nodeindex);
}


/**
 * Initialize the memory block head specified.
 *
 * The parameters chunksize and chunkperblock define the size of each memory
 * chunk and the number of chunks per block.
 */
void mmBlockInit(mmBlockHead *head, size_t chunksize, int chunkperblock, int keepfreecount, int alignment)
{
    memset(head, 0, sizeof(mmBlockHead));
    head->alignment = 0;

    head->chunksize = chunksize;

    if (head->chunksize < sizeof(mmListNode))
	head->chunksize = sizeof(mmListNode);

    if (alignment >= 0x10) {
	head->alignment = alignment - 1;
	head->chunksize = (chunksize + head->alignment) & ~head->alignment;
    }

    head->chunkfreecount = 0;
    head->relayalloc = mmAlloc;
    head->relayfree = mmFree;
    head->relayvalue = 0;
    head->chunkperblock = chunkperblock;
    head->allocsize = sizeof(mmBlock) + head->chunksize * head->chunkperblock;
    head->keepfreecount = keepfreecount + chunkperblock;
    mtSpinInit(&head->spinlock);
}


void *mmNodeAlloc(int nodeindex, size_t size)
{
#ifdef MM_NUMA

    if (mmcontext.numaflag)
	return numa_alloc_onnode(size, nodeindex);

#endif
    return bu_malloc(size, "mmNodeAlloc");
    (void)nodeindex;
}

void mmNodeFree(int UNUSED(nodeindex), void *v, size_t size)
{
#ifdef MM_NUMA

    if (mmcontext.numaflag) {
	numa_free(v, size);
	return;
    }

#else
    (void)size;
#endif
    bu_free(v, "mmNodeFree()");
}


/**
 * Add the item to a linked list.
 *
 * The head of the linked list should be defined as a void pointer. The list
 * parameter can be a pointer to it, or a pointer to the mmListNode.next
 * variable of the preceding item. The offset parameter should be the offset
 * of the mmListNode structure within the structure of the item. It can be
 * easily obtained with the offsetof(,) macro.
 */
void mmListAdd(void **list, void *item, intptr_t offset)
{
    mmListNode *node, *next;
    node = (mmListNode *)ADDRESS(item, offset);
    node->prev = list;
    node->next = *list;

    if (*list) {
	next = (mmListNode *)ADDRESS(*list, offset);
	next->prev = &(node->next);
    }

    *list = item;
}


/**
 * Remove the item from a linked list.
 *
 * The offset parameter should be the offset of the mmListNode  structure
 * within the structure of the item. It can be easily obtained with the
 * offsetof(,) macro.
 */
void mmListRemove(void *item, intptr_t offset)
{
    mmListNode *node, *next;
    node = (mmListNode *)ADDRESS(item, offset);
    *(node->prev) = (void *)node->next;

    if (node->next) {
	next = (mmListNode *)ADDRESS(node->next, offset);
	next->prev = node->prev;
    }
}

#if 0 /*if defined(MM_LINUX) */
#define _GNU_SOURCE
#include <sched.h>
void mmThreadBindToCpu(int cpuindex)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuindex, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}
#else
void mmThreadBindToCpu(int cpuindex)
{
    (void)cpuindex;
}
#endif


int mmCpuGetNode(int cpuindex)
{
    return mmcontext.cpunode[cpuindex];
}
/* *****************************************************************************
 *
 * Hash
 *
 * *****************************************************************************
 */

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
/* *****************************************************************************
 *
 * Binsort
 *
 * *****************************************************************************
 */

typedef float mmbsf;
#define mmbsffloor(x) floor(x)
#define mmbsfceil(x) ceil(x)

typedef struct {
    int itemcount;
    int flags;
    mmbsf min, max;
    void *p;
    void **last;
} mmBinSortBucket;

#define MM_BINSORT_BUCKET_FLAGS_SUBGROUP (0x1)


typedef struct {
    mmbsf groupbase, groupmax, bucketrange;
    int bucketmax;
    mmBinSortBucket bucket[1];
} mmBinSortGroup;


typedef struct {
    int numanodeindex;
    size_t memsize;
    size_t itemlistoffset;
    int rootbucketcount; /* Count of buckets for root */
    int groupbucketcount; /* Count of buckets per group */
    int groupthreshold; /* Count of items required in a bucket  */
    int collapsethreshold;
    int maxdepth;

    /* Callback to user code to obtain the value of an item */
    double(*itemvalue)(void *item);

    /* Memory management */
    mmBlockHead bucketblock;
    mmBlockHead groupblock;

    /* Top-level group, *MUST* be at end of struct due to bucket[] zero-length array */
    mmBinSortGroup root;
} mmBinSortHead;


static int MM_NOINLINE mmBinSortBucketIndex(mmBinSortGroup *group, mmbsf value)
{
    int bucketindex = mmbsffloor((value - group->groupbase) / group->bucketrange);

    if (bucketindex < 0)
	bucketindex = 0;

    if (bucketindex > group->bucketmax)
	bucketindex = group->bucketmax;

    return bucketindex;
}


static mmBinSortGroup *mmBinSortSpawnGroup(mmBinSortHead *bsort, void *itembase, mmbsf base, mmbsf range)
{
    int bucketindex;
    mmbsf value;
    void *item, *itemnext;
    mmBinSortGroup *group;
    mmBinSortBucket *bucket;

    group = (mmBinSortGroup *)mmBlockAlloc(&bsort->groupblock);
    group->groupbase = base;
    group->groupmax = base + range;
    group->bucketrange = range / (mmbsf)bsort->groupbucketcount;
    group->bucketmax = bsort->groupbucketcount - 1;

    bucket = group->bucket;

    for (bucketindex = 0; bucketindex < bsort->groupbucketcount; bucketindex++) {
	bucket->flags = 0;
	bucket->itemcount = 0;
	bucket->min =  FLT_MAX;
	bucket->max = -FLT_MAX;
	bucket->p = 0;
	bucket->last = &bucket->p;
	bucket++;
    }

    for (item = itembase; item; item = itemnext) {
	itemnext = ((mmListNode *)ADDRESS(item, bsort->itemlistoffset))->next;

	value = bsort->itemvalue(item);
	bucketindex = mmBinSortBucketIndex(group, value);

	if (value < bucket->min)
	    bucket->min = value;

	if (value > bucket->max)
	    bucket->max = value;

	bucket = &group->bucket[bucketindex];
	bucket->itemcount++;
	mmListAdd(bucket->last, item, bsort->itemlistoffset);
	bucket->last = &((mmListNode *)ADDRESS(item, bsort->itemlistoffset))->next;
    }

    return group;
}


static void mmBinSortCollapseGroup(mmBinSortHead *bsort, mmBinSortBucket *parentbucket)
{
    int bucketindex, itemcount;
    mmBinSortGroup *group;
    mmBinSortBucket *bucket;
    void *item, *itemnext;

    group = (mmBinSortGroup *)parentbucket->p;
    parentbucket->p = 0;
    parentbucket->last = &parentbucket->p;

    itemcount = 0;

    for (bucketindex = 0, bucket = group->bucket; bucketindex < bsort->groupbucketcount; bucketindex++, bucket++) {
	if (bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP)
	    mmBinSortCollapseGroup(bsort, bucket);

	for (item = bucket->p; item; item = itemnext) {
	    itemnext = ((mmListNode *)ADDRESS(item, bsort->itemlistoffset))->next;
	    mmListAdd(parentbucket->last, item, bsort->itemlistoffset);
	    parentbucket->last = &((mmListNode *)ADDRESS(item, bsort->itemlistoffset))->next;
	    itemcount++;
	}
    }

    parentbucket->flags &= ~MM_BINSORT_BUCKET_FLAGS_SUBGROUP;
    parentbucket->itemcount = itemcount;
}


static void *mmBinSortGroupFirstItem(mmBinSortHead *bsort, mmBinSortGroup *group, mmbsf failmax)
{
    int bucketindex, topbucket;
    void *item;
    mmBinSortBucket *bucket;
    mmBinSortGroup *subgroup;

    if (failmax > group->groupmax)
	topbucket = group->bucketmax;
    else {
	topbucket = mmbsfceil((failmax - group->groupbase) / group->bucketrange);

	if ((topbucket < 0) || (topbucket > group->bucketmax))
	    topbucket = group->bucketmax;
    }

    bucket = group->bucket;

    for (bucketindex = 0; bucketindex <= topbucket; bucketindex++, bucket++) {
	if (bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP) {
	    subgroup = (mmBinSortGroup *)bucket->p;
	    item = mmBinSortGroupFirstItem(bsort, subgroup, failmax);

	    if (item)
		return item;
	} else if (bucket->p)
	    return bucket->p;
    }

    return 0;
}


/****/


void *mmBinSortInit(size_t itemlistoffset, int rootbucketcount, int groupbucketcount, double rootmin, double rootmax, int groupthreshold, double(*itemvaluecallback)(void *item), int maxdepth, int numanodeindex)
{
    int bucketindex;
    size_t memsize;
    mmBinSortHead *bsort;
    mmBinSortGroup *group;
    mmBinSortBucket *bucket;

    if (!(mmcontext.numaflag))
	numanodeindex = -1;

    memsize = sizeof(mmBinSortHead) + (rootbucketcount * sizeof(mmBinSortBucket));

    if (numanodeindex >= 0) {
	bsort = (mmBinSortHead *)mmNodeAlloc(numanodeindex, memsize);
	mmBlockNodeInit(&bsort->bucketblock, numanodeindex, sizeof(mmBinSortBucket), 1024, 1024, 0x40);
	mmBlockNodeInit(&bsort->groupblock, numanodeindex, sizeof(mmBinSortGroup) + (groupbucketcount * sizeof(mmBinSortBucket)), 16, 16, 0x40);
    } else {
	bsort = (mmBinSortHead *)bu_malloc(memsize, "bsort");
	mmBlockInit(&bsort->bucketblock, sizeof(mmBinSortBucket), 1024, 1024, 0x40);
	mmBlockInit(&bsort->groupblock, sizeof(mmBinSortGroup) + (groupbucketcount * sizeof(mmBinSortBucket)), 16, 16, 0x40);
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
    group->bucketrange = (rootmax - rootmin) / (double)rootbucketcount;
    group->bucketmax = rootbucketcount - 1;
    bucket = group->bucket;

    for (bucketindex = 0; bucketindex < rootbucketcount; bucketindex++) {
	bucket->flags = 0;
	bucket->itemcount = 0;
	bucket->min =  FLT_MAX;
	bucket->max = -FLT_MAX;
	bucket->p = 0;
	bucket++;
    }

    return bsort;
}


void mmBinSortFree(void *binsort)
{
    mmBinSortHead *bsort;
    bsort = (mmBinSortHead *)binsort;
    mmBlockFreeAll(&bsort->bucketblock);
    mmBlockFreeAll(&bsort->groupblock);

    if (bsort->numanodeindex >= 0)
	mmNodeFree(bsort->numanodeindex, bsort, bsort->memsize);
    else
	bu_free(bsort, "bsort");
}


void mmBinSortAdd(void *binsort, void *item, double itemvalue)
{
    int bucketindex, depth;
    mmbsf value;
    mmBinSortHead *bsort;
    mmBinSortGroup *group, *subgroup;
    mmBinSortBucket *bucket;

    bsort = (mmBinSortHead *)binsort;
    value = itemvalue;
    group = &bsort->root;

    for (depth = 0;; group = (mmBinSortGroup *)bucket->p, depth++) {
	bucketindex = mmBinSortBucketIndex(group, value);
	bucket = &group->bucket[bucketindex];
	bucket->itemcount++;

	if (bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP)
	    continue;

	if ((bucket->itemcount >= bsort->groupthreshold) && (depth < bsort->maxdepth)) {
	    /* Build a new sub group, sort all entries into it */
	    subgroup = mmBinSortSpawnGroup(bsort, bucket->p, group->groupbase + ((mmbsf)bucketindex * group->bucketrange), group->bucketrange);
	    bucket->flags |= MM_BINSORT_BUCKET_FLAGS_SUBGROUP;
	    bucket->p = subgroup;
	    continue;
	}

	if (value < bucket->min)
	    bucket->min = value;

	if (value > bucket->max)
	    bucket->max = value;

#ifdef MM_BINSORT_INSERTION_SORT
	void *itemfind, *itemnext;
	void **insert;
	insert = &bucket->p;

	for (itemfind = bucket->p; itemfind; itemfind = itemnext) {
	    itemnext = ((mmListNode *)ADDRESS(itemfind, bsort->itemlistoffset))->next;

	    if (itemvalue <= bsort->itemvalue(itemfind))
		break;

	    insert = &((mmListNode *)ADDRESS(itemfind, bsort->itemlistoffset))->next;
	}

	mmListAdd(insert, item, bsort->itemlistoffset);
#else
	mmListAdd(&bucket->p, item, bsort->itemlistoffset);
#endif

	break;
    }
}


void mmBinSortRemove(void *binsort, void *item, double itemvalue)
{
    int bucketindex;
    mmbsf value;
    mmBinSortHead *bsort;
    mmBinSortGroup *group;
    mmBinSortBucket *bucket;

    bsort = (mmBinSortHead *)binsort;
    value = itemvalue;
    group = &bsort->root;

    for (;; group = (mmBinSortGroup *)bucket->p) {
	bucketindex = mmBinSortBucketIndex(group, value);
	bucket = &group->bucket[bucketindex];
	bucket->itemcount--;

	if (bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP) {
	    if (bucket->itemcount >= bsort->collapsethreshold)
		continue;

	    mmBinSortCollapseGroup(bsort, bucket);
	}

	break;
    }

    mmListRemove(item, bsort->itemlistoffset);
}


void mmBinSortUpdate(void *binsort, void *item, double olditemvalue, double newitemvalue)
{
    mmBinSortRemove(binsort, item, olditemvalue);
    mmBinSortAdd(binsort, item, newitemvalue);
}


void *mmBinSortGetFirstItem(void *binsort, double failmax)
{
    int bucketindex, topbucket;
    mmBinSortHead *bsort;
    mmBinSortGroup *group;
    mmBinSortBucket *bucket;
    void *item;

    bsort = (mmBinSortHead *)binsort;

    if (failmax > bsort->root.groupmax)
	topbucket = bsort->root.bucketmax;
    else {
	topbucket = mmbsfceil(((mmbsf)failmax - bsort->root.groupbase) / bsort->root.bucketrange);

	if ((topbucket < 0) || (topbucket > bsort->root.bucketmax))
	    topbucket = bsort->root.bucketmax;
    }

    bucket = bsort->root.bucket;

    for (bucketindex = 0; bucketindex <= topbucket; bucketindex++, bucket++) {
	if (bucket->flags & MM_BINSORT_BUCKET_FLAGS_SUBGROUP) {
	    group = (mmBinSortGroup *)bucket->p;
	    item = mmBinSortGroupFirstItem(bsort, group, (mmbsf)failmax);

	    if (item)
		return item;
	} else if (bucket->p)
	    return bucket->p;
    }

    return 0;
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
