/*                       P M A L L O C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup librt */
/** @{ */
/** @file librt/pmalloc.c
 *
 * The Princeton memory allocater.
 *
 * Original Author was Wiliam L. Sebok but later modified by John
 * Anderson for use in multi-threaded applications.  It now uses a
 * field from the BRL-CAD resource structure to store the memory
 * blocks. Also replaced mlindx() routine with a faster (but less
 * general) version that takes advantage of the fact that bucket sizes
 * increase by powers of two.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * A "smarter" malloc v1.0			William L. Sebok
 *					Sept. 24, 1984 rev. June 30, 1986
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Algorithm:
 * Assign to each area an index "n". This is currently proportional to
 * the log 2 of size of the area rounded down to the nearest integer.
 * Then all free areas of storage whose length have the same index n are
 * organized into a chain with other free areas of index n (the "bucket"
 * chain). A request for allocation of storage first searches the list of
 * free memory.  The search starts at the bucket chain of index equal to
 * that of the storage request, continuing to higher index bucket chains
 * if the first attempt fails.
 * If the search fails then new memory is allocated.  Only the amount of
 * new memory needed is allocated.  Any old free memory left after an
 * allocation is returned to the free list.
 *
 * All memory areas (free or busy) handled by rt_pmalloc are also chained
 * sequentially by increasing address (the adjacency chain).  When memory
 * is freed it is merged with adjacent free areas, if any.  If a free area
 * of memory ends at the end of memory (i.e. at the break), and if the
 * variable "endfree" is non-zero, then the break is contracted, freeing
 * the memory back to the system.
 *
 * Notes:
 * ov_length field includes sizeof(struct overhead)
 * adjacency chain includes all memory, allocated plus free.
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"


/*
 * the following items may need to be configured for a particular machine
 */

/* alignment requirement for machine (in bytes) */
#define NALIGN sizeof(double)

/* size of an integer large enough to hold a character pointer */
typedef long Size;

/*
 * CURBRK returns the value of the current system break, i.e., the system's
 * idea of the highest legal address in the data area.  It is defined as
 * a macro for the benefit of systems that have provided an easier way to
 * obtain this number (such as in an external variable)
 */

#ifndef CURBRK
#define CURBRK sbrk(0)
#else  /* CURBRK */
#	if CURBRK == curbrk
extern Size curbrk;
#	endif
#endif /* CURBRK */

/*
 * note that it is assumed that CURBRK remembers the last requested break to
 * the nearest byte (or at least the nearest word) rather than the nearest page
 * boundary.  If this is not true then the following BRK macro should be
 * replaced with one that remembers the break to within word-size accuracy.
 */

#ifndef BRK
#define BRK(x) brk(x)
#endif /* Not BRK */

/* END of machine dependent portion */

#define MAGIC_FREE 0x548a934c
#define MAGIC_BUSY 0xc139569a

struct overhead {
    struct rt_qelem ov_adj;		/* adjacency chain pointers */
    struct rt_qelem ov_buk;		/* bucket chain pointers */
    long ov_magic;
    Size ov_length;
};


/*
 * The following macros depend on the order of the elements in struct overhead
 */
#define TOADJ(p)	((struct rt_qelem *)(p))
#define FROMADJ(p)	((struct overhead *)(p))
#define FROMBUK(p)	((struct overhead *)((char *)p - sizeof(struct rt_qelem)))
#define TOBUK(p)	((struct rt_qelem *)((char *)p + sizeof(struct rt_qelem)))

extern char endfree;

static Size mlindx();
static void rt_pm_insque(), rt_pm_remque();
static void mllcerr();
static void mlfree_end();

extern void (*mlabort)();

#define debug 1
#ifdef debug
# define ASSERT(p, q) if (!(p)) {		\
	bu_semaphore_acquire(BU_SEM_SYSCALL);	\
	mllcerr(q);				\
	bu_semaphore_release(BU_SEM_SYSCALL);	\
    }
#else
# define ASSERT(p, q)
#endif

#ifndef NULL
#define NULL 0
#endif

/*
 * return to the system memory freed adjacent to the break
 * default is Off
 */
char endfree = 0;

/* sizes of buckets currently proportional to log 2() */
static Size mlsizes[RT_PM_NBUCKETS] = {
    0, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768,
    65536, 131072, 262144, 524288, 1048576, 2097152, 4194304
};


void (*mlabort)() = {0};	/* ptr to optional user-provided error handler */

char *
rt_pmalloc(nbytes, pmem)
    long nbytes;
    struct rt_pm_res *pmem;
{
    register struct overhead *p, *q;
    register struct rt_qelem *bucket;
    register Size surplus;

    nbytes = ((nbytes + (NALIGN-1)) & ~(NALIGN-1))
	+ sizeof(struct overhead);

    for (
	bucket = &pmem->buckets[mlindx(nbytes)];
	bucket < &pmem->buckets[RT_PM_NBUCKETS];
	bucket++
	) {
	register struct rt_qelem *b;
	for (b = bucket->q_forw; b != bucket; b = b->q_forw) {
	    p = FROMBUK(b);
	    ASSERT(p->ov_magic == MAGIC_FREE,
		   "\nrt_pmalloc: Entry not marked FREE found on Free List!\n")
		if (p->ov_length >= nbytes) {
		    rt_pm_remque(b);
		    surplus = p->ov_length - nbytes;
		    goto foundit;
		}
	}
    }

    /* obtain additional memory from system */
    {
	register Size i;
	long ret;

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	p = (struct overhead *)CURBRK;
	i = ((Size)p)&(NALIGN-1);
	if (i != 0)
	    p = (struct overhead *)((char *)p + NALIGN - i);
	ret = (long)BRK(((char *)p) + nbytes);
	bu_semaphore_release(BU_SEM_SYSCALL);

	if (ret)
	    bu_bomb("rt_pmalloc: brk() failure. Insufficient memory available!\n");

	p->ov_length = nbytes;
	surplus = 0;

	/* add to end of adjacency chain */
	rt_pm_insque(TOADJ(p), pmem->adjhead.q_back);
    }

foundit:
    /* mark surplus memory free */
    if (surplus > sizeof(struct overhead)) {
	/* if big enough, split it up */
	q = (struct overhead *)((char *)p + nbytes);

	q->ov_length = surplus;
	p->ov_length = nbytes;
	q->ov_magic = MAGIC_FREE;

	/* add surplus into adjacency chain */
	rt_pm_insque(TOADJ(q), TOADJ(p));

	/* add surplus into bucket chain */
	rt_pm_insque(TOBUK(q), &pmem->buckets[mlindx(surplus)]);
    }

    p->ov_magic = MAGIC_BUSY;
    return (char*)p + sizeof(struct overhead);
}


/*
 * select the proper size bucket
 */
/* this version of mlindx() will only work with the original RT_PM_NBUCKETS
 * and mlsizes[]
 */
static Size
mlindx(n)
    register Size n;
{
    register Size index=0, shifter;

    if (n >= mlsizes[RT_PM_NBUCKETS-1])
	index = RT_PM_NBUCKETS-1;
    else if (n < mlsizes[1])
	index = 0;
    else {
	shifter = n >> 6;
	while (shifter) {
	    index++;
	    shifter = shifter >> 1;
	}
    }

    return index;
}


static void
mllcerr(p)
    char *p;
{
    register char *q;
    q = p;
    while (*q++);	/* find end of string */
    (void)write(2, p, q-p-1);
    if (mlabort)
	(*mlabort)();
    else
	abort();
}


/*
 * The vax has wondrous instructions for inserting and removing items into
 * doubly linked queues.  On the vax the assembler output of the C compiler is
 * massaged by an sed script to turn these function calls into invocations of
 * the rt_pm_insque and rt_pm_remque machine instructions.
 * In BRL's version, all machines use these functions.  No assembler.
 */

static void
rt_pm_insque(item, queu)
    register struct rt_qelem *item, *queu;
/* insert "item" after "queu" */
{
    register struct rt_qelem *pueu;
    pueu = queu->q_forw;
    item->q_forw = pueu;
    item->q_back = queu;
    queu->q_forw = item;
    pueu->q_back = item;
}


static void
rt_pm_remque(item)
    register struct rt_qelem *item;
/* remove "item" */
{
    register struct rt_qelem *queu, *pueu;
    pueu = item->q_forw;
    queu = item->q_back;
    queu->q_forw = pueu;
    pueu->q_back = queu;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * rt_pfree					William L. Sebok
 * A "smarter" malloc v1.0		Sept. 24, 1984 rev. June 30, 1986
 *
 * 	rt_pfree takes a previously rt_pmalloc-allocated area at mem and frees it.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
rt_pfree(mem, pmem)
    register char *mem;
    register struct rt_pm_res *pmem;
{
    register struct overhead *p, *q;

    if (mem == NULL)
	return;

    p = (struct overhead *)(mem - sizeof(struct overhead));

    /* not advised but allowed */
    if (p->ov_magic == MAGIC_FREE)
	return;

    if (p->ov_magic != MAGIC_BUSY)
	mllcerr("rt_pfree: attempt to free memory not allocated with rt_pmalloc!\n");

    /* try to merge with previous free area */
    q = FROMADJ((TOADJ(p))->q_back);

    if (q != FROMADJ(&pmem->adjhead)) {
	ASSERT(q < p,
	       "\nrt_pfree: While trying to merge a free area with a lower adjacent free area, \n\
 addresses were found out of order!\n")
	    /* If lower segment can be merged */
	    if (q->ov_magic == MAGIC_FREE
		&& (char *)q + q->ov_length == (char *)p
		) {
		/* remove lower address area from bucket chain */
		rt_pm_remque(TOBUK(q));

		/* remove upper address area from adjacency chain */
		rt_pm_remque(TOADJ(p));

		q->ov_length += p->ov_length;
		p->ov_magic = 0;	/* decommission */
		p = q;
	    }
    }

    /* try to merge with next higher free area */
    q = FROMADJ((TOADJ(p))->q_forw);

    if (q != FROMADJ(&pmem->adjhead)) {
	/* upper segment can be merged */
	ASSERT(q > p,
	       "\nrt_pfree: While trying to merge a free area with a higher adjacent free area, \n\
 addresses were found out of order!\n")
	    if (q->ov_magic == MAGIC_FREE
		&&	(char *)p + p->ov_length == (char *)q
		) {
		/* remove upper from bucket chain */
		rt_pm_remque(TOBUK(q));

		/* remove upper from adjacency chain */
		rt_pm_remque(TOADJ(q));

		p->ov_length += q->ov_length;
		q->ov_magic = 0;	/* decommission */
	    }
    }

    p->ov_magic = MAGIC_FREE;

    /* place in bucket chain */
    rt_pm_insque(TOBUK(p), &pmem->buckets[mlindx(p->ov_length)]);

    if (endfree)
	mlfree_end(pmem);

    return;
}


static void
mlfree_end(pmem)
    register struct rt_pm_res *pmem;
{
    register struct overhead *p;

    p = FROMADJ(pmem->adjhead.q_back);
    if (p->ov_magic != MAGIC_FREE)
	return;

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    if ((char*)p + p->ov_length == (char *)CURBRK) {
	/* area is free and at end of memory */

	p->ov_magic = 0;	/* decommission (just in case) */

	/* remove from end of adjacency chain */
	rt_pm_remque(TOADJ(p));

	/* remove from bucket chain */
	rt_pm_remque(TOBUK(p));

	/* release memory to system */
	(void)BRK((char *)p);
    }
    bu_semaphore_release(BU_SEM_SYSCALL);

    return;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * rt_prealloc				William L. Sebok
 * A "smarter" malloc v1.0		Sept. 24, 1984 rev. June 30, 1986
 *
 *	rt_prealloc takes previously rt_pmalloc-allocated area at mem, and tries
 *	 to change its size to nbytes bytes, moving it and copying its
 *	 contents if necessary.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

char *
rt_prealloc(mem, nbytes, pmem)
    register char *mem; unsigned nbytes;
    register struct rt_pm_res *pmem;
{
    register char *newmem;
    register struct overhead *p, *q;
    Size surplus, length;
    char oendfree;

    if (mem == NULL)
	return rt_pmalloc((long)nbytes, pmem);

    /* if beyond current arena it has to be bad */
    if (mem > (char*)FROMADJ(pmem->adjhead.q_back) + sizeof(struct overhead))
	return NULL;

    p = (struct overhead *)(mem - sizeof(struct overhead));

    if (p->ov_magic != MAGIC_BUSY && p->ov_magic != MAGIC_FREE)
	return NULL;	/* already gone */

    length = p->ov_length;

    nbytes = (nbytes + (NALIGN-1)) & (~(NALIGN-1));

    if (p->ov_magic == MAGIC_BUSY) {
	oendfree = endfree;	endfree = 0;
	rt_pfree(mem, pmem);	/* free it but don't let it contract break */
	endfree = oendfree;
    }

    if (p->ov_magic == MAGIC_FREE
	&& (surplus = length - nbytes - sizeof(struct overhead)) >= 0
	) {
	/* shrink area in place */
	if (surplus > sizeof(struct overhead)) {
	    q = (struct overhead *)((char *)p + nbytes
				    + sizeof(struct overhead));
	    q->ov_length = surplus;
	    q->ov_magic = MAGIC_FREE;
	    rt_pm_insque(TOADJ(q), TOADJ(p));
	    rt_pm_insque(TOBUK(q), &pmem->buckets[mlindx(surplus)]);
	    p->ov_length -= surplus;
	}
	/* declare it to be busy */
	rt_pm_remque(TOBUK(p));
	p->ov_magic = MAGIC_BUSY;

	if (endfree)
	    mlfree_end(pmem);
	return mem;
    }

    /* if at break, grow in place */

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    if (p->ov_magic == MAGIC_FREE && ((char *)p + p->ov_length) == (char *)CURBRK) {
	nbytes += sizeof(struct overhead);

	BRK((char *)p + nbytes);
	bu_semaphore_release(BU_SEM_SYSCALL);

	p->ov_length = nbytes;
	/* declare it to be busy */
	rt_pm_remque(TOBUK(p));
	p->ov_magic = MAGIC_BUSY;
	return mem;
    } else
	bu_semaphore_release(BU_SEM_SYSCALL);

    newmem = rt_pmalloc((long)nbytes, pmem);

    if (newmem != mem && newmem != NULL) {
	register Size n;
	n = length - sizeof(struct overhead);
	nbytes = (nbytes < n) ? nbytes: n;
	(void)memmove(newmem, mem, nbytes);
    }

    if (endfree)
	mlfree_end(pmem);

    return newmem;
}


/*
 * P C A L L O C
 *
 * Malloc() a block of memory, and clear it.
 */
char *
rt_pcalloc(num, size, pmem)
    register unsigned num, size;
    register struct rt_pm_res *pmem;
{
    register char *p;

    size *= num;
    if ((p = rt_pmalloc((long)size, pmem))) {
	memset(p, 0, size);
    }
    return p;
}


void
rt_cfree(p, num, size, pmem)
    char *p;
    unsigned num;
    unsigned size;
    struct rt_pm_res *pmem;
{
    rt_pfree(p, pmem);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
