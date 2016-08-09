/*                          H A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

/* TODO - once deprecation is complete, bu_hash structs become private */
#if 0
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
    unsigned long mask;
    unsigned long num_lists;
    unsigned long num_entries;
    struct bu_hash_entry **lists;
};
#define BU_CK_HASH_TBL(_hp) BU_CKMAG(_hp, BU_HASH_TBL_MAGIC, "bu_hash_tbl")
#endif

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

HIDDEN int _nhash_keycmp(const uint8_t *k1, const uint8_t *k2, size_t key_len)
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

    /* If we don't have a key, error */
    if (!key || key_len == 0) return -1;

    /* If we don't have a value, "already set" in the sense that a lookup with this
     * key will already return the expected value (null) so we don't need to add it. */
    if (!val) return 0;

    /* Use key hash to get bin, add entry to bin list */
    idx = _bu_hash(key, key_len) & hsh_tbl->mask;

    /* If we have existing entries, go hunting.  Otherwise, just add */
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
	if (found) break;
	end_entry = hsh_entry;
	hsh_entry = hsh_entry->next;
    }

    /* If and only if we ended up with a null hsh_entry, create a new one */
    if (!hsh_entry) {
	hsh_entry  = (struct bu_hash_entry *)calloc(1, sizeof(struct bu_hash_entry));
	hsh_entry->next = NULL;
	hsh_entry->key_len = key_len;
	hsh_entry->magic = BU_HASH_ENTRY_MAGIC;
	/* make a copy of the key */
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

    /* Whether the structure is old or new, it gets the value */
    hsh_entry->value = val;

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

    /* If we don't have a key, no-op */
    if (!key || key_len == 0) return;

    /* Use key hash to get bin, add entry to bin list */
    idx = _bu_hash(key, key_len) & hsh_tbl->mask;

    /* Nothing in bin, we're done */
    if (!hsh_tbl->lists[idx]) return;

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
}


struct bu_hash_entry *
bu_hash_next(struct bu_hash_tbl *hsh_tbl, struct bu_hash_entry *e)
{
    unsigned long idx, l;
    BU_CK_HASH_TBL(hsh_tbl);

    if (hsh_tbl->num_entries == 0) {
	/* this table is empty */
	return (struct bu_hash_entry *)NULL;
    }

    /* If we don't have an entry, find loop through all the bins in this hash
     * table to find the first non-null entry
     */
    if (!e) {
	for (l = 0; l < hsh_tbl->num_lists; l++) {
	    if (hsh_tbl->lists[l]) return hsh_tbl->lists[l];
	}
	/* if we've got nothing (empty bins) we're "done" iterating - return
	 * NULL */
	return (struct bu_hash_entry *)NULL;
    }

    if (e && e->next) return e->next;

    /* If we've got the last entry in a bin, we need to find the next bin.
     * Use the key hash to get the "current" bin, and proceed from there. */
    idx = _bu_hash(e->key, e->key_len) & hsh_tbl->mask;
    idx++;
    for (l = idx; l < hsh_tbl->num_lists; l++) {
	if (hsh_tbl->lists[l]) return hsh_tbl->lists[l];
	idx++;
    }
    /* if we've got nothing by this point we've reached the end */
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


/* Old implementation code - remove once deprecation is complete */

#if 1
unsigned long
bu_hash(const uint8_t *key, size_t len)
{
    return _bu_hash(key, len);
}

struct bu_hash_entry *
_bu_hash_tbl_find(const struct bu_hash_tbl *hsh_tbl, const uint8_t *key, int key_len, struct bu_hash_entry **prev, unsigned long *idx)
{
    struct bu_hash_entry *hsh_entry=NULL;
    int found=0;

    BU_CK_HASH_TBL(hsh_tbl);

    /* calculate the index into the bin array */
    *idx = _bu_hash(key, key_len) & hsh_tbl->mask;
    if (*idx >= hsh_tbl->num_lists) {
	fprintf(stderr, "hash function returned too large value (%ld), only have %ld lists\n",
		*idx, hsh_tbl->num_lists);
	*prev = NULL;
	return (struct bu_hash_entry *)NULL;
    }

    /* look for the provided key in the list of entries in this bin */
    *prev = NULL;
    if (hsh_tbl->lists[*idx]) {
	*prev = NULL;
	hsh_entry = hsh_tbl->lists[*idx];
	while (hsh_entry) {
	    const uint8_t *c1, *c2;
	    int i;

	    /* compare key lengths first for performance */
	    if (hsh_entry->key_len != key_len) {
		*prev = hsh_entry;
		hsh_entry = hsh_entry->next;
		continue;
	    }

	    /* key lengths are the same, now compare the actual keys */
	    found = 1;
	    c1 = key;
	    c2 = hsh_entry->key;
	    for (i=0; i<key_len; i++) {
		if (*c1 != *c2) {
		    found = 0;
		    break;
		}
		c1++;
		c2++;
	    }

	    /* if we have a match, get out of this loop */
	    if (found) break;

	    /* step to next entry in this bin */
	    *prev = hsh_entry;
	    hsh_entry = hsh_entry->next;
	}
    }

    if (found) {
	/* return the found entry */
	return hsh_entry;
    } else {
	/* did not find the entry, return NULL */
	return (struct bu_hash_entry *)NULL;
    }
}
struct bu_hash_entry *
bu_hash_tbl_find(const struct bu_hash_tbl *hsh_tbl, const uint8_t *key, int key_len, struct bu_hash_entry **prev, unsigned long *idx)
{
    return _bu_hash_tbl_find(hsh_tbl, key, key_len, prev, idx);
}

void
bu_set_hash_value(struct bu_hash_entry *hsh_entry, void *value)
{
    BU_CK_HASH_ENTRY(hsh_entry);

    /* just copy a pointer */
    hsh_entry->value = value;
}


void *
bu_get_hash_value(const struct bu_hash_entry *hsh_entry)
{
    BU_CK_HASH_ENTRY(hsh_entry);

    return hsh_entry->value;
}


uint8_t *
bu_get_hash_key(const struct bu_hash_entry *hsh_entry)
{
    BU_CK_HASH_ENTRY(hsh_entry);

    return hsh_entry->key;
}


struct bu_hash_entry *
bu_hash_tbl_add(struct bu_hash_tbl *hsh_tbl, const uint8_t *key, int key_len, int *new_entry)
{
    struct bu_hash_entry *hsh_entry, *prev;
    unsigned long idx;

    BU_CK_HASH_TBL(hsh_tbl);

    /* do a find for three reasons.
     * does this key already exist in the table?
     * get the hash bin index for this key.
     * find the previous entry to link the new one to.
     */
    hsh_entry = _bu_hash_tbl_find(hsh_tbl, key, key_len, &prev, &idx);

    if (hsh_entry) {
	/* this key is already in the table, return the entry, with
	 * flag set to 0
	 */
	*new_entry = 0;
	return hsh_entry;
    }

    if (prev) {
	/* already have an entry in this bin, just link to it */
	prev->next = (struct bu_hash_entry *)calloc(1, sizeof(struct bu_hash_entry));
	hsh_entry = prev->next;
    } else {
	/* first entry in this bin */
	hsh_entry = (struct bu_hash_entry *)calloc(1, sizeof(struct bu_hash_entry));
	hsh_tbl->lists[idx] = hsh_entry;
    }

    /* fill in the structure */
    hsh_entry->next = NULL;
    hsh_entry->value = NULL;
    hsh_entry->key_len = key_len;
    hsh_entry->magic = BU_HASH_ENTRY_MAGIC;

    /* make a copy of the key */
    hsh_entry->key = (uint8_t *)malloc((size_t)key_len);
    memcpy(hsh_entry->key, key, (size_t)key_len);

    /* set "new" flag, increment count of entries, and return new
     * entry.
     */
    *new_entry = 1;
    hsh_tbl->num_entries++;
    return hsh_entry;
}


void
bu_hash_tbl_print(const struct bu_hash_tbl *hsh_tbl, const char *str)
{
    unsigned long idx;
    struct bu_hash_entry *hsh_entry;

    BU_CK_HASH_TBL(hsh_tbl);

    fprintf(stderr, "%s\n", str);
    fprintf(stderr, "bu_hash_table (%ld entries):\n", hsh_tbl->num_entries);

    /* visit all the entries in this table */
    for (idx=0; idx<hsh_tbl->num_lists; idx++) {
	hsh_entry = hsh_tbl->lists[idx];
	while (hsh_entry) {
	    BU_CK_HASH_ENTRY(hsh_entry);
	    fprintf(stderr, "\tindex=%ld, key=%p, value=%p\n", idx, (void *)hsh_entry->key, hsh_entry->value);
	    hsh_entry = hsh_entry->next;
	}
    }
}


void
bu_hash_tbl_free(struct bu_hash_tbl *hsh_tbl)
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

    /* free the actual hash table structure */
    free(hsh_tbl);
}


struct bu_hash_entry *
_bu_hash_tbl_first(const struct bu_hash_tbl *hsh_tbl, struct bu_hash_record *rec)
{
    BU_CK_HASH_TBL(hsh_tbl);

    /* initialize the record structure */
    rec->magic = BU_HASH_RECORD_MAGIC;
    rec->tbl = hsh_tbl;
    rec->index = (unsigned long)-1;
    rec->hsh_entry = (struct bu_hash_entry *)NULL;

    if (hsh_tbl->num_entries == 0) {
	/* this table is empty */
	return (struct bu_hash_entry *)NULL;
    }

    /* loop through all the bins in this hash table, looking for a
     * non-null entry
     */
    for (rec->index=0; rec->index < hsh_tbl->num_lists; rec->index++) {
	rec->hsh_entry = hsh_tbl->lists[rec->index];
	if (rec->hsh_entry) {
	    return rec->hsh_entry;
	}
    }

    /* no entry found, return NULL */
    return (struct bu_hash_entry *)NULL;
}

struct bu_hash_entry *
bu_hash_tbl_first(const struct bu_hash_tbl *hsh_tbl, struct bu_hash_record *rec)
{
    return _bu_hash_tbl_first(hsh_tbl, rec);
}

struct bu_hash_entry *
_bu_hash_tbl_next(struct bu_hash_record *rec)
{
    const struct bu_hash_tbl *hsh_tbl;

    BU_CK_HASH_RECORD(rec);
    hsh_tbl = rec->tbl;
    BU_CK_HASH_TBL(hsh_tbl);

    /* if the entry in the record structure has a non-null "next",
     * return it, and update the record structure
     */
    if (rec->hsh_entry) {
	rec->hsh_entry = rec->hsh_entry->next;
	if (rec->hsh_entry) {
	    return rec->hsh_entry;
	}
    }

    /* must move to a new bin to find another entry */
    for (rec->index++; rec->index < hsh_tbl->num_lists; rec->index++) {
	rec->hsh_entry = hsh_tbl->lists[rec->index];
	if (rec->hsh_entry) {
	    return rec->hsh_entry;
	}
    }

    /* no more entries, return NULL */
    return (struct bu_hash_entry *)NULL;
}

struct bu_hash_entry *
bu_hash_tbl_next(struct bu_hash_record *rec)
{
    return _bu_hash_tbl_next(rec);
}

struct bu_hash_entry *
bu_hash_tbl_traverse(struct bu_hash_tbl *hsh_tbl, int (*func)(struct bu_hash_entry *, void *), void *func_arg)
{
    int ret;
    struct bu_hash_record rec;
    struct bu_hash_entry *entry;

    entry = _bu_hash_tbl_first(hsh_tbl, &rec);
    while (entry) {
	ret = func(entry, func_arg);
	if (ret) {
	    return entry;
	}
	entry = _bu_hash_tbl_next(&rec);
    }
    return NULL;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
