/*                         H A S H . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/** @file hash.h
 *
 */
#ifndef BU_HASH_H
#define BU_HASH_H

#include "common.h"

#include "bu/defines.h"

/** @addtogroup hash */
/** @{ */
/** @file libbu/hash.c
 *
 * @brief
 * An implementation of hash tables.
 */

/**
 * A hash entry
 */
struct bu_hash_entry {
    uint32_t magic;
    unsigned char *key;
    unsigned char *value;
    int key_len;
    struct bu_hash_entry *next;
};
typedef struct bu_hash_entry bu_hash_entry_t;
#define BU_HASH_ENTRY_NULL ((struct bu_hash_entry *)0)

/**
 * asserts the integrity of a non-head node bu_hash_entry struct.
 */
#define BU_CK_HASH_ENTRY(_ep) BU_CKMAG(_ep, BU_HASH_ENTRY_MAGIC, "bu_hash_entry")

/**
 * initializes a bu_hash_entry struct without allocating any memory.
 */
#define BU_HASH_ENTRY_INIT(_h) { \
	(_h)->magic = BU_HASH_ENTRY_MAGIC; \
	(_h)->key = (_h)->value = NULL; \
	(_h)->key_len = 0; \
	(_h)->next = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_hash_entry struct.  does not allocate memory.
 */
#define BU_HASH_ENTRY_INIT_ZERO { BU_HASH_ENTRY_MAGIC, NULL, NULL, 0, NULL }

/**
 * returns truthfully whether a bu_hash_entry has been initialized.
 */
#define BU_HASH_ENTRY_IS_INITIALIZED(_h) (((struct bu_hash_entry *)(_h) != BU_HASH_ENTRY_NULL) && LIKELY((_h)->magic == BU_HASH_ENTRY_MAGIC))


/**
 * A table of hash entries
 */
struct bu_hash_tbl {
    uint32_t magic;
    unsigned long mask;
    unsigned long num_lists;
    unsigned long num_entries;
    struct bu_hash_entry **lists;
};
typedef struct bu_hash_tbl bu_hash_tbl_t;
#define BU_HASH_TBL_NULL ((struct bu_hash_tbl *)0)

/**
 * asserts the integrity of a non-head node bu_hash_tbl struct.
 */
#define BU_CK_HASH_TBL(_hp) BU_CKMAG(_hp, BU_HASH_TBL_MAGIC, "bu_hash_tbl")

/**
 * initializes a bu_hash_tbl struct without allocating any memory.
 */
#define BU_HASH_TBL_INIT(_h) { \
	(_h)->magic = BU_HASH_TBL_MAGIC; \
	(_h)->mask = (_h)->num_lists = (_h)->num_entries = 0; \
	(_h)->lists = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_hash_tbl struct.  does not allocate memory.
 */
#define BU_HASH_TBL_INIT_ZERO { BU_HASH_TBL_MAGIC, 0, 0, 0, NULL }

/**
 * returns truthfully whether a bu_hash_tbl has been initialized.
 */
#define BU_HASH_TBL_IS_INITIALIZED(_h) (((struct bu_hash_tbl *)(_h) != BU_HASH_TBL_NULL) && LIKELY((_h)->magic == BU_HASH_TBL_MAGIC))


/**
 * A hash table entry record
 */
struct bu_hash_record {
    uint32_t magic;
    const struct bu_hash_tbl *tbl;
    unsigned long index;
    struct bu_hash_entry *hsh_entry;
};
typedef struct bu_hash_record bu_hash_record_t;
#define BU_HASH_RECORD_NULL ((struct bu_hash_record *)0)

/**
 * asserts the integrity of a non-head node bu_hash_record struct.
 */
#define BU_CK_HASH_RECORD(_rp) BU_CKMAG(_rp, BU_HASH_RECORD_MAGIC, "bu_hash_record")

/**
 * initializes a bu_hash_record struct without allocating any memory.
 */
#define BU_HASH_RECORD_INIT(_h) { \
	(_h)->magic = BU_HASH_RECORD_MAGIC; \
	(_h)->tbl = NULL; \
	(_h)->index = 0; \
	(_h)->hsh_entry = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_hash_record struct.  does not allocate memory.
 */
#define BU_HASH_RECORD_INIT_ZERO { BU_HASH_RECORD_MAGIC, NULL, 0, NULL }

/**
 * returns truthfully whether a bu_hash_record has been initialized.
 */
#define BU_HASH_RECORD_IS_INITIALIZED(_h) (((struct bu_hash_record *)(_h) != BU_HASH_RECORD_NULL) && LIKELY((_h)->magic == BU_HASH_RECORD_MAGIC))


/**
 * the hashing function
 */
BU_EXPORT extern unsigned long bu_hash(const unsigned char *str,
				       int len);

/**
 * Create an empty hash table
 *
 * The input is the number of desired hash bins.  This number will be
 * rounded up to the nearest power of two.
 */
BU_EXPORT extern struct bu_hash_tbl *bu_hash_tbl_create(unsigned long tbl_size);

/**
 * Find the hash table entry corresponding to the provided key
 *
 * @param[in] hsh_tbl - The hash table to look in
 * @param[in] key - the key to look for
 * @param[in] key_len - the length of the key in bytes
 *
 * Output:
 * @param[out] prev - the previous hash table entry (non-null for entries that not the first in hash bin)
 * @param[out] idx - the index of the hash bin for this key
 *
 * @return
 * the hash table entry corresponding to the provided key, or NULL if
 * not found.
 */
BU_EXPORT extern struct bu_hash_entry *bu_hash_tbl_find(const struct bu_hash_tbl *hsh_tbl,
							const unsigned char *key,
							int key_len,
							struct bu_hash_entry **prev,
							unsigned long *idx);

/**
 * Set the value for a hash table entry
 *
 * Note that this is just a pointer copy, the hash table does not
 * maintain its own copy of this value.
 */
BU_EXPORT extern void bu_set_hash_value(struct bu_hash_entry *hsh_entry,
					unsigned char *value);

/**
 * get the value pointer stored for the specified hash table entry
 */
BU_EXPORT extern unsigned char *bu_get_hash_value(const struct bu_hash_entry *hsh_entry);

/**
 * get the key pointer stored for the specified hash table entry
 */
BU_EXPORT extern unsigned char *bu_get_hash_key(const struct bu_hash_entry *hsh_entry);

/**
 * Add an new entry to a hash table
 *
 * @param[in] hsh_tbl - the hash table to accept the new entry
 * @param[in] key - the key (any byte string)
 * @param[in] key_len - the number of bytes in the key
 *
 * @param[out] new_entry - a flag, non-zero indicates that a new entry
 * was created.  zero indicates that an entry already exists with the
 * specified key and key length
 *
 * @return
 * a hash table entry. If "new" is non-zero, a new, empty entry is
 * returned.  if "new" is zero, the returned entry is the one matching
 * the specified key and key_len.
 */
BU_EXPORT extern struct bu_hash_entry *bu_hash_tbl_add(struct bu_hash_tbl *hsh_tbl,
						       const unsigned char *key,
						       int key_len,
						       int *new_entry);

/**
 * Print the specified hash table to stderr.
 *
 * (Note that the keys and values are printed as pointers)
 */
BU_EXPORT extern void bu_hash_tbl_print(const struct bu_hash_tbl *hsh_tbl,
					const char *str);

/**
 * Free all the memory associated with the specified hash table.
 *
 * Note that the keys are freed (they are copies), but the "values"
 * are not freed.  (The values are merely pointers)
 */
BU_EXPORT extern void bu_hash_tbl_free(struct bu_hash_tbl *hsh_tbl);

/**
 * get the "first" entry in a hash table
 *
 * @param[in] hsh_tbl - the hash table of interest
 * @param[in] rec - an empty "bu_hash_record" structure for use by this routine and "bu_hash_tbl_next"
 *
 * @return
 * the first non-null entry in the hash table, or NULL if there are no
 * entries (Note that the order of entries is not likely to have any
 * significance)
 */
BU_EXPORT extern struct bu_hash_entry *bu_hash_tbl_first(const struct bu_hash_tbl *hsh_tbl,
							 struct bu_hash_record *rec);

/**
 * get the "next" entry in a hash table
 *
 * input:
 * rec - the "bu_hash_record" structure that was passed to
 * "bu_hash_tbl_first"
 *
 * return:
 * the "next" non-null hash entry in this hash table
 */
BU_EXPORT extern struct bu_hash_entry *bu_hash_tbl_next(struct bu_hash_record *rec);

/**
 * Pass each table entry to a supplied function, along with an
 * additional argument (which may be NULL).
 *
 * Returns when func returns !0 or every entry has been visited.
 *
 * Example, freeing all memory associated with a table whose values
 * are dynamically allocated ints:
 @code
 static int
 free_entry(struct bu_hash_entry *entry, void *UNUSED(arg))
 {
     bu_free(bu_get_hash_value(entry), "table value");
     return 0;
 }

 bu_hash_table_traverse(table, free_entry, NULL);
 bu_hash_table_free(table);
 @endcode
 *
 * @return
 * If func returns !0 for an entry, that entry is returned.
 * Otherwise NULL is returned.
 */
BU_EXPORT extern struct bu_hash_entry *bu_hash_tbl_traverse(struct bu_hash_tbl *hsh_tbl, int (*func)(struct bu_hash_entry *, void *), void *func_arg);


/** @} */

#endif  /* BU_HASH_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
