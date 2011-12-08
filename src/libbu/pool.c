/*                        P O O L . C
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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "raytrace.h"

#define INITIAL_POOL_CNT 512                  /* intial number of pools to create */
#define MAX_POOL_CNT 2048                     /* maximum number of pools to create */
#define INITIAL_BANK_CNT 1                    /* intial number of banks to create per pool */
#define INITIAL_BANK_ELEM_CNT 100             /* intial number elements per bank */
#define BANK_ELEM_CNT_INCREMENT_MULTIPLIER 2  /* multiple this number by the previous bank element count
					       * to determine how many elements to create in the next bank
					       */

/*
 * Description of 'struct mem_pool' type.
 *
 * pn = pool number    (starts with 1)
 * en = element number (starts with 0)
 * bn = bank number    (starts with 0)
 *
 * Note: pool number corresponds to the size in bytes of each element
 *       in the pool.
 *
 * ------------------------------------------------------------------
 * Variable Name         Type        Description
 * ------------------------------------------------------------------
 *  freelist              (void ***) list of pointers to the start of
 *                                    each pool's free list
 *                                    (pool_cnt)
 *
 *  pool_cnt              (size_t) number of allocated elements in
 *                                    arrays pool, freelist_cnt,
 *                                    freelist_alloc_cnt, banklist,
 *                                    bank_elem_cnt
 *                                    (n/a)
 *
 *  freelist[pn]          (void **) for pool 'pn', list of pointers
 *                                    to each free element in the pool
 *                                    (freelist_alloc_cnt[pn])
 *
 *  freelist[pn][en]      (void *) pointer to free element
 *                                    'en' in pool 'pn'
 *                                    (n/a)
 *
 *  freelist_cnt          (size_t *) list of counts of free elements
 *                                    in each pool
 *                                    (pool_cnt)
 *
 *  freelist_cnt[pn]      (size_t) number of free elements in pool
 *                                    'pool[pn]'
 *                                    (n/a)
 *
 *  freelist_alloc_cnt    (size_t *) list of the number of allocated
 *                                    entries in each entry of 'pool'
 *                                    (pool_cnt)
 *
 *  freelist_alloc_cnt[pn](size_t) number of allocated entries in
 *                                    pool 'pool[pn]'
 *                                    (n/a)
 *
 *  banklist              (void ***) list of pointers to the start of
 *                                    each pool's bank list
 *                                    (pool_cnt)
 *
 *  banklist[pn]          (void **) list of pointers to the start of
 *                                    each bank in pool 'pn'.
 *                                    (banklist_alloc_cnt[pn])
 *
 *  banklist[pn][bn]      (void *) pointer to bank number
 *                                    'bn' of pool 'pn'
 *                                    (n/a)
 *
 *  banklist_cnt          (size_t *) list of the number of banks in
 *                                    each pool
 *                                    (pool_cnt)
 *
 *  banklist_cnt[pn]      (size_t) number of banks in pool 'pn'
 *                                    (n/a)
 *
 *  banklist_alloc_cnt    (size_t *) list of the number of allocated
 *                                    elements in banklist for each
 *                                    pool
 *                                    (pool_cnt)
 *
 *  banklist_alloc_cnt[pn] (size_t) number of allocated elements in
 *                                    list banklist[pn]
 *                                    (n/a)
 *
 *  bank_alloc_cnt        (size_t **) list of pointers to each pool's
 *                                    list of counts of allocated
 *                                    elements in each bank of that
 *                                    pool
 *                                    (pool_cnt)
 *
 *  bank_alloc_cnt[pn]    (size_t *) list of counts of allocated
 *                                    elements in each bank of pool
 *                                    'pn'
 *                                    (banklist_alloc_cnt[pn])
 *
 *  bank_alloc_cnt[pn][bn](size_t) number of elements created in
 *                                    bank 'pn' of pool 'pn' with
 *                                    one call to calloc
 *                                    (n/a)
 *
 */

struct mem_pool {
    size_t pool_cnt;
    void *** freelist;
    size_t * freelist_cnt;
    size_t * freelist_alloc_cnt;
    void *** banklist;
    size_t * banklist_cnt;
    size_t ** bank_alloc_cnt;
};


static struct mem_pool *hd = (struct mem_pool *)NULL;

void
bu_init_poolframe(void)
{
    size_t i;

    if (hd) {
	return;
    }

    hd = (struct mem_pool *)calloc(1, sizeof(struct mem_pool));
    memset((char *)hd, 0, sizeof(struct mem_pool));

    hd->pool_cnt = INITIAL_POOL_CNT; /* initial number of pools */

    /* add 1 to cnt since pools start at array index 1 not 0 */
    hd->freelist = (void ***)calloc(hd->pool_cnt+1, sizeof(void *));
    hd->freelist_cnt = (size_t *)calloc(hd->pool_cnt+1, sizeof(size_t));
    hd->freelist_alloc_cnt = (size_t *)calloc(hd->pool_cnt+1, sizeof(size_t));
    hd->banklist = (void ***)calloc(hd->pool_cnt+1, sizeof(void *));
    hd->banklist_cnt = (size_t *)calloc(hd->pool_cnt+1, sizeof(size_t));
    hd->bank_alloc_cnt = (size_t **)calloc(hd->pool_cnt+1, sizeof(size_t *));

    /* initialize arrays */
    for (i = 1 ; i <= hd->pool_cnt ; i++) {
	hd->freelist[i] = (void **)NULL;
	hd->freelist_cnt[i] = 0;
	hd->freelist_alloc_cnt[i] = 0;
	hd->banklist[i] = (void **)NULL;
	hd->banklist_cnt[i] = 0;
	hd->bank_alloc_cnt[i] = (size_t *)NULL;
    }
}


/* free all pools and free the pool frame structures */
void
bu_free_pools_and_poolframe(void)
{
    size_t i, j;
    size_t num_elem_alloc; /* number of array elements allocated */

    if (!hd) {
	return;
    }

    for (i = 1 ; i <= hd->pool_cnt ; i++) {
	if (hd->freelist[i]) {
	    num_elem_alloc = 0;
	    for (j = 0 ; j < hd->banklist_cnt[i] ; j++) {
		num_elem_alloc += hd->bank_alloc_cnt[i][j];
	    }
	    if (num_elem_alloc != hd->freelist_cnt[i]) {
		bu_semaphore_release(BU_SEM_SYSCALL);
		bu_log("bu_free_pools_and_poolframe(): Warning: freeing %ld bytes not returned to pool %ld\n",
		       (long)((num_elem_alloc - hd->freelist_cnt[i]) * i), (long)i);
		bu_bomb("bu_free_pools_and_poolframe(): Warning: freeing memory not returned to pool\n");
	    }
	    for (j = 0 ; j < hd->banklist_cnt[i] ; j++) {
		free(hd->banklist[i][j]);
	    }
	    free(hd->freelist[i]);
	    free(hd->banklist[i]);
	    free(hd->bank_alloc_cnt[i]);
	}
    }

    free(hd->freelist);
    free(hd->freelist_cnt);
    free(hd->freelist_alloc_cnt);
    free(hd->banklist);
    free(hd->banklist_cnt);
    free(hd->bank_alloc_cnt);
    free(hd);
    hd = (struct mem_pool *)NULL;
}


/* initialize the structures for an individual pool and allocate
 * an intial amount of memory in the pool
 */
void
bu_init_pool(size_t pn)
{
    size_t i, j;

    /* pn is the pool number and also the number of bytes
     * in each element of the corresponding pool.
     */

    if (!hd) {
	bu_init_poolframe();
    }

    if (hd->freelist[pn]) {
	return;
    }

    hd->freelist_cnt[pn] = 0;
    hd->freelist_alloc_cnt[pn] = INITIAL_BANK_ELEM_CNT * INITIAL_BANK_CNT;
    hd->banklist_cnt[pn] = INITIAL_BANK_CNT;

    hd->freelist[pn] = (void **)calloc(hd->freelist_alloc_cnt[pn], sizeof(void *));
    hd->banklist[pn] = (void **)calloc(hd->banklist_cnt[pn], sizeof(void *));
    hd->bank_alloc_cnt[pn] = (size_t *)calloc(hd->banklist_cnt[pn], sizeof(size_t));

    /* allocate contents of each bank, pn is also the size of the elements in pool pn */
    for (i = 0 ; i < hd->banklist_cnt[pn] ; i++) {
	hd->banklist[pn][i] = (void *)calloc(INITIAL_BANK_ELEM_CNT, pn);
	hd->bank_alloc_cnt[pn][i] = INITIAL_BANK_ELEM_CNT;
	/* populate freelist with the start address of each element in bank hd->banklist[pn][i] */
	for (j = (size_t)hd->banklist[pn][i] ;
	     j < ((size_t)hd->banklist[pn][i] + (INITIAL_BANK_ELEM_CNT * pn)) ;
	     j += pn) {
	    hd->freelist[pn][hd->freelist_cnt[pn]] = (void *)j;
	    (hd->freelist_cnt[pn])++;
	}
    }
}


/* increase number of pool in poolframe */
void
bu_inc_poolframe(size_t pn)
{
    void ***tmp1;
    size_t *tmp2;
    size_t **tmp3;
    size_t pool_cnt_old, i;

    pool_cnt_old = hd->pool_cnt;
    hd->pool_cnt = pn;

    tmp1 = (void ***)realloc((char *)hd->freelist,
			     (hd->pool_cnt+1) * sizeof(void *));
    hd->freelist = tmp1;

    tmp2 = (size_t *)realloc((char *)hd->freelist_cnt,
			     (hd->pool_cnt+1) * sizeof(size_t));
    hd->freelist_cnt = tmp2;

    tmp2 = (size_t *)realloc((char *)hd->freelist_alloc_cnt,
			     (hd->pool_cnt+1) * sizeof(size_t));
    hd->freelist_alloc_cnt = tmp2;

    tmp1 = (void ***)realloc((char *)hd->banklist,
			     (hd->pool_cnt+1) * sizeof(void *));
    hd->banklist = tmp1;

    tmp2 = (size_t *)realloc((char *)hd->banklist_cnt,
			     (hd->pool_cnt+1) * sizeof(size_t));
    hd->banklist_cnt = tmp2;

    tmp3 = (size_t **)realloc((char *)hd->bank_alloc_cnt,
			      (hd->pool_cnt+1) * sizeof(size_t *));
    hd->bank_alloc_cnt = tmp3;

    /* initialize added space */
    for (i = pool_cnt_old+1 ; i <= hd->pool_cnt ; i++) {
	hd->freelist[i] = (void **)NULL;
	hd->freelist_cnt[i] = 0;
	hd->freelist_alloc_cnt[i] = 0;
	hd->banklist[i] = (void **)NULL;
	hd->banklist_cnt[i] = 0;
	hd->bank_alloc_cnt[i] = (size_t *)NULL;
    }
}


void
bu_inc_pool(size_t pn)
{
    void **tmp2;
    size_t *tmp3;
    size_t new_bank_elem_cnt;
    size_t num_of_new_banks = 2;
    size_t old_banklist_cnt;
    size_t i, j;

    if (!hd) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_inc_pool(): poolframe is not initialized\n");
    }
    if (!hd->freelist[pn]) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_inc_pool(): pool is not initialized\n");
    }

    new_bank_elem_cnt = hd->bank_alloc_cnt[pn][hd->banklist_cnt[pn]-1] *
	BANK_ELEM_CNT_INCREMENT_MULTIPLIER;

    /* freelist will always be large enough to hold all the pointers to allocated
     * elements in all the banks in the current pool. the reason is so than when
     * all elements are freed we do not need to worry if freelist is large enough
     * to hold all the pointers to the freed elements.
     */
    hd->freelist_alloc_cnt[pn] += (new_bank_elem_cnt * num_of_new_banks);

    tmp2 = (void **)realloc((char *)hd->freelist[pn],
			    hd->freelist_alloc_cnt[pn] * sizeof(void *));
    hd->freelist[pn] = tmp2;

    old_banklist_cnt = hd->banklist_cnt[pn];
    hd->banklist_cnt[pn] += num_of_new_banks;

    tmp2 = (void **)realloc((char *)hd->banklist[pn],
			    hd->banklist_cnt[pn] * sizeof(void *));
    hd->banklist[pn] = tmp2;

    tmp3 = (size_t *)realloc((char *)hd->bank_alloc_cnt[pn],
			     hd->banklist_cnt[pn] * sizeof(size_t));
    hd->bank_alloc_cnt[pn] = tmp3;

    for (i = old_banklist_cnt ; i < hd->banklist_cnt[pn] ; i++) {
	hd->banklist[pn][i] = (void *)calloc(new_bank_elem_cnt, pn);
	hd->bank_alloc_cnt[pn][i] = new_bank_elem_cnt;
	/* populate freelist with the start address of each element in bank hd->banklist[pn][i] */
	for (j = (size_t)hd->banklist[pn][i] ;
	     j < ((size_t)hd->banklist[pn][i] + (new_bank_elem_cnt * pn)) ;
	     j += pn) {
	    hd->freelist[pn][hd->freelist_cnt[pn]] = (void *)j;
	    (hd->freelist_cnt[pn])++;
	}
    }
}


void *
bu_get_elem_from_pool(size_t pn)
{
    void *ret;

    bu_semaphore_acquire(BU_SEM_SYSCALL);

    if (!pn) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_get_elem_from_pool(): requesting an element of size 0\n");
    }

    if (pn > MAX_POOL_CNT) {
	ret = (void *)calloc(1, pn);
	bu_semaphore_release(BU_SEM_SYSCALL);
	return ret;
    }

    if (!hd) {
	bu_init_poolframe();
    }
    if (pn > hd->pool_cnt) {
	bu_inc_poolframe(pn);
    }
    if (!hd->freelist[pn]) {
	bu_init_pool(pn);
    }
    if (!hd->freelist_cnt[pn]) {
	bu_inc_pool(pn);
    }

    ret = hd->freelist[pn][hd->freelist_cnt[pn]-1];
    (hd->freelist_cnt[pn])--;
    memset((char *)ret, 0, pn);

    bu_semaphore_release(BU_SEM_SYSCALL);
    return ret;
}


void
bu_free_elem_pool(void *ptr, size_t elem_byte_size)
{
    size_t pn;

    bu_semaphore_acquire(BU_SEM_SYSCALL);

    pn = elem_byte_size;

    if (!ptr) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_free_elem_pool(): was called with a null pointer\n");
    }

    if (pn > MAX_POOL_CNT) {
	free(ptr);
	bu_semaphore_release(BU_SEM_SYSCALL);
	return;
    }

    if (!pn) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_free_elem_pool(): was called for pool 0\n");
    }
    if (!hd) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_free_elem_pool(): poolframe is not initialized\n");
    }
    if (!hd->freelist[pn]) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_free_elem_pool(): pool is not initialized\n");
    }
    if (hd->freelist_cnt[pn] >= hd->freelist_alloc_cnt[pn]) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	bu_bomb("bu_free_elem_pool(): corrupt memory pool, possible an element was double freed\n");
    }

    memset((char *)ptr, 0, pn);
    hd->freelist[pn][hd->freelist_cnt[pn]] = ptr;
    (hd->freelist_cnt[pn])++;

    bu_semaphore_release(BU_SEM_SYSCALL);
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
