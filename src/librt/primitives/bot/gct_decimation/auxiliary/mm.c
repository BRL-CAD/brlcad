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

#include "mm.h"

#include "bu/malloc.h"
#include "bu/parallel.h"

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
#include "cc.h"
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
	sysmemory = -1;
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
