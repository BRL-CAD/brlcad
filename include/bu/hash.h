/*                         H A S H . H
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

#ifndef BU_HASH_H
#define BU_HASH_H

#include "common.h"

#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_hash
 * @brief
 * An implementation of hash tables. TODO - need much better discussion here. Key points:
 *
 * 1. Keys are copied to the table and do not need to be maintained by the user
 * 2. All keys, regardless of original type, are handled as byte arrays.  Need
 *    examples of using strings and pointers as has keys
 * 3. Void pointers sorted as values are *not* copies - the application must keep
 *    the data pointed to by the pointers intact and not rely on the table.
 * 4. Performance is not currently a focus of bu_hash - it is not currently suitable
 *    for applications where high performance is critical.
 */
/** @{ */
/** @file bu/hash.h */

/* Use typedefs to hide the details of the hash entry and table structures */
typedef struct bu_hash_entry bu_hash_entry;
typedef struct bu_hash_tbl bu_hash_tbl;


/**
 * Create and initialize a hash table.  The input is the number of desired hash
 * bins.  This number will be rounded up to the nearest power of two, or a
 * minimal size if tbl_size is smaller than the internal minimum bin count.
 */
BU_EXPORT extern bu_hash_tbl *bu_hash_create(unsigned long tbl_size);


/**
 * Free all the memory associated with the specified hash table.
 *
 * Note that the keys are freed (they are copies), but the "values"
 * are not freed.  (The values are merely pointers)
 */
BU_EXPORT extern void bu_hash_destroy(bu_hash_tbl *t);


/**
 * Get the value stored in the hash table entry corresponding to the provided key
 *
 * @param[in] t - The hash table to look in
 * @param[in] key - the key to look for
 * @param[in] key_len - the length of the key in bytes
 *
 * @return
 * the void pointer stored in the hash table entry corresponding to key, or
 * NULL if no entry corresponding to key exists.
 */
BU_EXPORT extern void *bu_hash_get(const bu_hash_tbl *t, const uint8_t *key, size_t key_len);


/**
 * Set the value stored in the hash table entry corresponding to the provided
 * key to val, or if no entry corresponding to the provided key exists create a
 * new entry associating the key and value.  The key value is stored in the
 * hash entry, but the table does not maintain its own copy of val - ensuring
 * the value pointer remains valid is the responsibility of the caller.
 *
 * Null or zero length keys are not supported.  The table will also not store
 * a key/value combination where the value is NULL, but for random access with
 * bu_hash_get get this won't matter because the return value for a key not
 * in the table is NULL - i.e. the return will be the same as if the key/value
 * pair had actually been added.  The only use case where this property is observable
 * is when a user iterates over the whole contents of a hash table - in that
 * situation a key, NULL entry might be expected, but will not be present.
 *
 * @param[in] t - The hash table to look in
 * @param[in] key - the key to look for
 * @param[in] key_len - the length of the key in bytes
 * @param[in] val - the value to be associated with key
 *
 * @return
 * 1 if a new entry is created, 0 if an existing value was updated, -1 on error.
 */
BU_EXPORT extern int bu_hash_set(bu_hash_tbl *t, const uint8_t *key, size_t key_len, void *val);


/**
 * Remove the hash table entry associated with key from the table.
 *
 * @param[in] t - The hash table to look in
 * @param[in] key - the key to look for
 * @param[in] key_len - the length of the key in bytes
 */
BU_EXPORT extern void bu_hash_rm(bu_hash_tbl *t, const uint8_t *key, size_t key_len);


/**
 * Supports iteration of all the contents in a hash table.
 *
 * @param[in] t - The hash table to look in
 * @param[in] p - the previous entry in the iteration
 *
 * This example prints all values in a hash table:
 * @code
 * void print_vals(struct bu_hash_tbl *t) {
 *   struct bu_hash_entry *e = bu_hash_next(t, NULL);
 *   while (e) {
 *     bu_log("Value: %p\n", bu_hash_value(e, NULL));
 *     e = bu_hash_next(t, e);
 *   }
 * }
 * @endcode
 *
 * @return
 * Either first entry (if p is NULL) or next entry (if p is NON-null).  Returns
 * NULL when p is last entry in table.
 */
BU_EXPORT extern bu_hash_entry *bu_hash_next(bu_hash_tbl *t, bu_hash_entry *p);


/**
 * Supports iteration of all the contents in a hash table.
 *
 * @param[in] e - The hash table to look in
 *
 * @param[out] key - the entry's key
 * @param[out] key_len - the length of the entry's key
 *
 * @return
 * Returns 0 on success, 1 on failure.
 */
BU_EXPORT extern int bu_hash_key(bu_hash_entry *e, uint8_t **key, size_t *key_len);


/* returns value of bu_hash_entry if nval is NULL.
 * returns nval if nval was assigned to p's value.
 * returns NULL on error */

/**
 * Extracts or updates the void pointer value for a bu_hash_entry. If nval is
 * not NULL the entry will be updated.
 *
 * @param[in] e - The hash table to look in
 * @param[in] nval - The new void pointer to assign to the entry's value slot, or NULL if no assignment is to be made.
 *
 * @return Returns the hash entry's value pointer, or NULL on error.
 */
BU_EXPORT extern void *bu_hash_value(bu_hash_entry *e, void *nval);

/** @} */

__END_DECLS

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
