/*                          H A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu/magic.h"
#include "bu/hash.h"
#include "bu/parallel.h"

struct bu_hash_entry {
    uint32_t magic;
    uint8_t *key;
    void *value;
    size_t key_len;
    struct bu_hash_entry *next;
};
#define BU_CK_HASH_ENTRY(_ep) BU_CKMAG(_ep, BU_HASH_ENTRY_MAGIC, "bu_hash_entry")

struct bu_hash_tbl {
    uint32_t magic;
    int semaphore;
    unsigned long mask;
    unsigned long num_lists;
    unsigned long num_entries;
    struct bu_hash_entry **lists;
};
#define BU_CK_HASH_TBL(_hp) BU_CKMAG(_hp, BU_HASH_TBL_MAGIC, "bu_hash_tbl")


HIDDEN unsigned long
_bu_hash(const uint8_t *key, size_t len)
{
    unsigned long hash = 5381;
    size_t i;

    if (!key) return hash;

    for (i = 0; i < len; i++) {
	hash = ((hash << 5) + hash) + key[i]; /* hash * 33 + c */
    }

    return hash;
}


HIDDEN int
_nhash_keycmp(const uint8_t *k1, const uint8_t *k2, size_t key_len)
{
    const uint8_t *c1 = k1;
    const uint8_t *c2 = k2;
    size_t i = 0;
    int match = 1;
    for (i = 0; i < key_len; i++) {
	if (*c1 != *c2) {
	    match = 0;
	    break;
	}
	c1++;
	c2++;
    }
    return match;
}


struct bu_hash_tbl *
bu_hash_create(unsigned long tbl_size)
{
    struct bu_hash_tbl *hsh_tbl;
    unsigned long power_of_two=64;
    int power=6;
    int max_power=(sizeof(unsigned long) * 8) - 1;

    /* allocate the table structure (do not use bu_malloc() as this
     * may be used for MEM_DEBUG).
     */
    hsh_tbl = (struct bu_hash_tbl *)malloc(sizeof(struct bu_hash_tbl));
    if (UNLIKELY(!hsh_tbl)) {
	fprintf(stderr, "Failed to allocate hash table\n");
	return (struct bu_hash_tbl *)NULL;
    }

    /* the number of bins in the hash table will be a power of two */
    while (power_of_two < tbl_size && power < max_power) {
	power_of_two = power_of_two << 1;
	power++;
    }

    if (power == max_power) {
	int i;

	hsh_tbl->mask = 1;
	for (i=1; i<max_power; i++) {
	    hsh_tbl->mask = (hsh_tbl->mask << 1) + 1;
	}
	hsh_tbl->num_lists = hsh_tbl->mask + 1;
    } else {
	hsh_tbl->num_lists = power_of_two;
	hsh_tbl->mask = power_of_two - 1;
    }

    /* allocate the bins (do not use bu_malloc() as this may be used
     * for MEM_DEBUG)
     */
    hsh_tbl->lists = (struct bu_hash_entry **)calloc(hsh_tbl->num_lists, sizeof(struct bu_hash_entry *));

    hsh_tbl->semaphore = bu_semaphore_register("SEM_HASH");

    hsh_tbl->num_entries = 0;
    hsh_tbl->magic = BU_HASH_TBL_MAGIC;

    return hsh_tbl;
}


void
bu_hash_destroy(struct bu_hash_tbl *hsh_tbl)
{
    unsigned long idx;
    struct bu_hash_entry *hsh_entry, *tmp;

    BU_CK_HASH_TBL(hsh_tbl);

    /* loop through all the bins in this hash table */
    for (idx=0; idx<hsh_tbl->num_lists; idx++) {
	/* traverse all the entries in the list for this bin */
	hsh_entry = hsh_tbl->lists[idx];
	while (hsh_entry) {
	    BU_CK_HASH_ENTRY(hsh_entry);
	    tmp = hsh_entry->next;

	    /* free the copy of the key, and this entry */
	    free(hsh_entry->key);
	    free(hsh_entry);

	    /* step to next entry in linked list */
	    hsh_entry = tmp;
	}
    }

    /* free the array of bins */
    free(hsh_tbl->lists);
    hsh_tbl->lists = NULL;

    /* free the actual hash table structure */
    free(hsh_tbl);
}


void *
bu_hash_get(const struct bu_hash_tbl *hsh_tbl, const uint8_t *key, size_t key_len)
{
    struct bu_hash_entry *hsh_entry=NULL;
    int found=0;
    unsigned long idx;

    BU_CK_HASH_TBL(hsh_tbl);

    /* calculate the index into the bin array */
    idx = _bu_hash(key, key_len) & hsh_tbl->mask;
    if (idx >= hsh_tbl->num_lists) {
	fprintf(stderr, "hash function returned too large value (%ld), only have %ld lists\n",
		idx, hsh_tbl->num_lists);
	return NULL;
    }

    /* look for the provided key in the list of entries in this bin */
    if (hsh_tbl->lists[idx]) {
	hsh_entry = hsh_tbl->lists[idx];
	while (hsh_entry) {
	    /* compare key lengths first for performance */
	    if ((size_t)hsh_entry->key_len != key_len) {
		hsh_entry = hsh_entry->next;
		continue;
	    }

	    /* key lengths are the same, now compare the actual keys */
	    found = _nhash_keycmp(key, hsh_entry->key, key_len);

	    /* if we have a match, get out of this loop */
	    if (found) break;

	    /* step to next entry in this bin */
	    hsh_entry = hsh_entry->next;
	}
    }

    if (found) {
	/* return the found entry */
	return hsh_entry->value;
    } else {
	/* did not find the entry, return NULL */
	return NULL;
    }
}

int
bu_hash_set(struct bu_hash_tbl *hsh_tbl, const uint8_t *key, size_t key_len, void *val)
{
    struct bu_hash_entry *hsh_entry, *end_entry;
    unsigned long idx;
    int ret = 0;
    int found = 0;

    BU_CK_HASH_TBL(hsh_tbl);

    /* must have a key */
    if (!key || key_len == 0)
	return -1;

    bu_semaphore_acquire(hsh_tbl->semaphore);

    /* Use key hash to get bin, add entry to bin list */
    idx = _bu_hash(key, key_len) & hsh_tbl->mask;

    /* If we have existing entries, see if we need to update. */
    hsh_entry = hsh_tbl->lists[idx];
    end_entry = NULL;
    while (hsh_entry && !found) {
	/* compare key lengths first for performance */
	if ((size_t)hsh_entry->key_len != key_len) {
	    end_entry = hsh_entry;
	    hsh_entry = hsh_entry->next;
	    continue;
	}

	/* key lengths are the same, now compare the actual keys */
	found = _nhash_keycmp(key, hsh_entry->key, key_len);
	if (found)
	    break;

	end_entry = hsh_entry;
	hsh_entry = hsh_entry->next;
    }

    /* If and only if we ended up with a NULL entry, create a new one */
    if (!hsh_entry) {
	/* FIXME: should use BU_GET/PUT for small memory allocations */
	hsh_entry  = (struct bu_hash_entry *)calloc(1, sizeof(struct bu_hash_entry));
	hsh_entry->next = NULL;
	hsh_entry->key_len = key_len;
	hsh_entry->magic = BU_HASH_ENTRY_MAGIC;
	/* make a copy of the key */
	/* FIXME: should use BU_GET/PUT for small memory allocations */
	hsh_entry->key = (uint8_t *)malloc((size_t)key_len);
	memcpy(hsh_entry->key, key, (size_t)key_len);
	if (!end_entry) {
	    hsh_tbl->lists[idx] = hsh_entry;
	} else {
	    end_entry->next = hsh_entry;
	}
	/* increment count of entries */
	hsh_tbl->num_entries++;
	ret = 1;
    }

    /* finally do as asked, set the value */
    hsh_entry->value = val;

    bu_semaphore_release(hsh_tbl->semaphore);

    return ret;
}

void
bu_hash_rm(struct bu_hash_tbl *hsh_tbl, const uint8_t *key, size_t key_len)
{
    struct bu_hash_entry *hsh_entry = NULL;
    struct bu_hash_entry *prev_entry = NULL;
    unsigned long idx;
    int found = 0;

    BU_CK_HASH_TBL(hsh_tbl);

    bu_semaphore_acquire(hsh_tbl->semaphore);

    /* If we don't have a key, no-op */
    if (!key || key_len == 0) {
	bu_semaphore_release(hsh_tbl->semaphore);
	return;
    }

    /* Use key hash to get bin, add entry to bin list */
    idx = _bu_hash(key, key_len) & hsh_tbl->mask;

    /* Nothing in bin, we're done */
    if (!hsh_tbl->lists[idx]) {
	bu_semaphore_release(hsh_tbl->semaphore);
	return;
    }

    hsh_entry = hsh_tbl->lists[idx];
    prev_entry = NULL;
    while (!found) {
	/* compare key lengths first for performance */
	if ((size_t)hsh_entry->key_len != key_len) {
	    prev_entry = hsh_entry;
	    hsh_entry = hsh_entry->next;
	    continue;
	}

	/* key lengths are the same, now compare the actual keys */
	found = _nhash_keycmp(key, hsh_entry->key, key_len);
	if (found) {
	    if (prev_entry) {
		prev_entry->next = hsh_entry->next;
	    } else {
		hsh_tbl->lists[idx] = hsh_entry->next;
	    }
	    free(hsh_entry->key);
	    free(hsh_entry);
	}
    }

    bu_semaphore_release(hsh_tbl->semaphore);
}


struct bu_hash_entry *
bu_hash_next(struct bu_hash_tbl *hsh_tbl, struct bu_hash_entry *e)
{
    struct bu_hash_entry *ec = e;
    unsigned long idx, l;

    BU_CK_HASH_TBL(hsh_tbl);

    bu_semaphore_acquire(hsh_tbl->semaphore);

    if (hsh_tbl->num_entries == 0) {
	/* this table is empty */
	bu_semaphore_release(hsh_tbl->semaphore);
	return (struct bu_hash_entry *)NULL;
    }

    if (e && e->next && !bu_hash_get(hsh_tbl, e->key, e->key_len)) {
	/* e has a next, but it's no longer valid (??)*/
	ec = NULL;
    }

    /* If we don't have an entry, find loop through all the bins in this hash
     * table to find the first non-null entry
     */
    if (!ec) {
	for (l = 0; l < hsh_tbl->num_lists; l++) {
	    if (hsh_tbl->lists[l]) {
		bu_semaphore_release(hsh_tbl->semaphore);
		return hsh_tbl->lists[l];
	    }
	}
	/* if we've got nothing (empty bins) we're "done" iterating - return
	 * NULL */
	bu_semaphore_release(hsh_tbl->semaphore);
	return (struct bu_hash_entry *)NULL;
    }

    if (e && e->next) {
	bu_semaphore_release(hsh_tbl->semaphore);
	return e->next;
    }

    /* If we've got the last entry in a bin, we need to find the next bin.
     * Use the key hash to get the "current" bin, and proceed from there. */
    idx = _bu_hash(e->key, e->key_len) & hsh_tbl->mask;
    idx++;
    for (l = idx; l < hsh_tbl->num_lists; l++) {
	if (hsh_tbl->lists[l]) {
	    bu_semaphore_release(hsh_tbl->semaphore);
	    return hsh_tbl->lists[l];
	}
	idx++;
    }
    /* if we've got nothing by this point we've reached the end */
    bu_semaphore_release(hsh_tbl->semaphore);
    return (struct bu_hash_entry *)NULL;
}


int
bu_hash_key(struct bu_hash_entry *e, uint8_t **key, size_t *key_len)
{
    if (!e || (!key && !key_len)) return 1;

    if (key)     (*key)     = e->key;
    if (key_len) (*key_len) = e->key_len;

    return 0;
}


void *
bu_hash_value(struct bu_hash_entry *e, void *val)
{
    if (!e) return NULL;

    if (!val) return e->value;

    e->value = val;

    return e->value;
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
