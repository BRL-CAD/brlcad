/*                          H A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @addtogroup bu_hash */
/** @{ */
/** @file hash.c
 *
 * @brief
 *	An implimentation of hash tables.
 */

#ifndef lint
static const char libbu_hash_RCSid[] = "@(#) $Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "machine.h"
#include "bu.h"


/**		B U _ H A S H
 * the hashing function
 */
unsigned long
bu_hash(unsigned char *str, int len)
{
	unsigned long hash = 5381;
	int i, c;

	c = *str;
	for( i=0 ; i<len ; i++ ) {
		c = *str;
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
		str++;
	}

	return hash;
}

/**			B U _ C R E A T E _ H A S H _ T B L
 *@brief
 *	Create an empty hash table
 *	The input is the number of desired hash bins.
 *	This number will be rounded up to the nearest power of two.
 */
struct bu_hash_tbl *
bu_create_hash_tbl( unsigned long tbl_size )
{
	struct bu_hash_tbl *hsh_tbl;
	unsigned long power_of_two=64;
	int power=6;
	int max_power=(sizeof( unsigned long ) * 8) - 1;

	/* allocate the table structure (do not use bu_malloc() as this may be used for MEM_DEBUG) */
	hsh_tbl = (struct bu_hash_tbl *)malloc( sizeof( struct bu_hash_tbl ) );
	if( !hsh_tbl ) {
		fprintf( stderr, "Failed to allocate hash table\n" );
		return( (struct bu_hash_tbl *)NULL );
	}

	/* the number of bins in the hash table will be a power of two */
	while( power_of_two < tbl_size && power < max_power ) {
		power_of_two = power_of_two << 1;
		power++;
	}

	if( power == max_power ) {
		int i;

		hsh_tbl->mask = 1;
		for( i=1 ; i<max_power ; i++ ) {
			hsh_tbl->mask = (hsh_tbl->mask << 1) + 1;
		}
		hsh_tbl->num_lists = hsh_tbl->mask + 1;
	} else {
		hsh_tbl->num_lists = power_of_two;
		hsh_tbl->mask = power_of_two - 1;
	}

	/* allocate the bins (do not use bu_malloc() as this may be used for MEM_DEBUG) */
	hsh_tbl->lists = (struct bu_hash_entry **)calloc( hsh_tbl->num_lists, sizeof( struct bu_hash_entry *) );

	hsh_tbl->num_entries = 0;
	hsh_tbl->magic = BU_HASH_TBL_MAGIC;

	return( hsh_tbl );
}

/**			B U _ F I N D _ H A S H _ E N T R Y
 *
 * @brief
 *	Find the hash table entry corresponding to the provided key
 *
 *
 * @param[in] hsh_tbl - The hash table to look in
 * @param[in] key - the key to look for
 * @param[in] key_len - the length of the key in bytes
 *
 * Output:
 * @param[out] prev - the previous hash table entry (non-null for entries that not the first in hash bin)
 * @param[out] index - the index of the hash bin for this key
 *
 * @return
 *	the hash table entry corresponding to the provided key, or NULL if not found
 */
struct bu_hash_entry *
bu_find_hash_entry( struct bu_hash_tbl *hsh_tbl, unsigned char *key, int key_len, struct bu_hash_entry **prev, unsigned long *index )
{
	struct bu_hash_entry *hsh_entry=NULL;
	int found=0;

	BU_CK_HASH_TBL( hsh_tbl );

	/* calculate the index into the bin array */
	*index = bu_hash( key, key_len ) & hsh_tbl->mask;
	if( *index >= hsh_tbl->num_lists ) {
		fprintf( stderr, "hash function returned too large value (%ld), only have %ld lists\n",
			 *index, hsh_tbl->num_lists );
		*prev = NULL;
		return( (struct bu_hash_entry *)NULL );
	}

	/* look for the provided key in the list of entries in this bin */
	*prev = NULL;
	if( hsh_tbl->lists[*index] ) {
		*prev = NULL;
		hsh_entry = hsh_tbl->lists[*index];
		while( hsh_entry ) {
			unsigned char *c1, *c2;
			int i;

			/* compare key lengths first for performance */
			if( hsh_entry->key_len != key_len ) {
				*prev = hsh_entry;
				hsh_entry = hsh_entry->next;
				continue;
			}

			/* key lengths are the same, now compare the actual keys */
			found = 1;
			c1 = key;
			c2 = hsh_entry->key;
			for( i=0 ; i<key_len ; i++ ) {
				if( *c1 != *c2 ) {
					found = 0;
					break;
				}
				c1++;
				c2++;
			}

			/* if we have a match, get out of this loop */
			if( found ) break;

			/* step to next entry in this bin */
			*prev = hsh_entry;
			hsh_entry = hsh_entry->next;
		}
	}

	if( found ) {
		/* return the found entry */
		return( hsh_entry );
	} else {
		/* did not find the entry, return NULL */
		return( (struct bu_hash_entry *)NULL );
	}
}


/**			B U _ S E T _ H A S H _ V A L U E
 *@brief
 *	Set the value for a hash table entry
 *
 *	Note that this is just a pointer copy, the hash table does not maintain its own copy
 *	of this value.
 */
void
bu_set_hash_value( struct bu_hash_entry *hsh_entry, unsigned char *value )
{
	BU_CK_HASH_ENTRY( hsh_entry );

	/* just copy a pointer */
	hsh_entry->value = value;
}

/**			B U _ G E T _ H A S H _ V A L U E
 *
 *	get the value pointer stored for the specified hash table entry
 */
unsigned char *
bu_get_hash_value( struct bu_hash_entry *hsh_entry )
{
	BU_CK_HASH_ENTRY( hsh_entry );

	return( hsh_entry->value );
}

/**			B U _ G E T _ H A S H _ K E Y
 *
 *	get the key pointer stored for the specified hash table entry
 */
unsigned char *
bu_get_hash_key( struct bu_hash_entry *hsh_entry )
{
	BU_CK_HASH_ENTRY( hsh_entry );

	return( hsh_entry->key );
}


/**			B U _ H A S H _ A D D _ E N T R Y
 *
 *	Add an new entry to a hash table
 *
 *
 * @param[in] hsh_tbl - the hash table to accept thye new entry
 * @param[in] key - the key (any byte string)
 * @param[in] key_len - the number of bytes in the key
 *
 * @param[out] new - a flag, non-zero indicates that a new entry was created.
 *	              zero indicates that an entry already exists with the specified key and key length
 *
 * @return
 *	a hash table entry. If "new" is non-zero, a new, empty entry is returned.
 *	if "new" is zero, the returned entry is the one matching the specified key and key_len
 */
struct bu_hash_entry *
bu_hash_add_entry( struct bu_hash_tbl *hsh_tbl, unsigned char *key, int key_len, int *new )
{
	struct bu_hash_entry *hsh_entry, *prev;
	unsigned long index;

	BU_CK_HASH_TBL( hsh_tbl );

	/* do a find for three reasons
	 * does this key already exist in the table?
	 * get the hash bin index for this key
	 * find the previous entry to link the new one to
	 */
	hsh_entry = bu_find_hash_entry( hsh_tbl, key, key_len, &prev, &index );

	if( hsh_entry ) {
		/* this key is already in the table, return the entry, with flag set to 0 */
		*new = 0;
		return( hsh_entry );
	}

	if( prev ) {
		/* already have an entry in this bin, just link to it */
		prev->next = (struct bu_hash_entry *)calloc( 1, sizeof( struct bu_hash_entry ) );
		hsh_entry = prev->next;
	} else {
		/* first entry in this bin */
		hsh_entry = (struct bu_hash_entry *)calloc( 1, sizeof( struct bu_hash_entry ) );
		hsh_tbl->lists[index] = hsh_entry;
	}

	/* fill in the structure */
	hsh_entry->next = NULL;
	hsh_entry->value = NULL;
	hsh_entry->key_len = key_len;
	hsh_entry->magic = BU_HASH_ENTRY_MAGIC;

	/* make a copy of the key */
	hsh_entry->key = (unsigned char *)malloc( key_len );
	memcpy( hsh_entry->key, key, key_len );

	/* set "new" flag, increment count of entries, and return new entry */
	*new = 1;
	hsh_tbl->num_entries++;
	return( hsh_entry );
}

/**			B U _ H A S H _ T B L _ P R
 *@brief
 *	Print the specified hash table to stderr.
 *	(Note that the keys and values are printed as pointers)
 */
void
bu_hash_tbl_pr( struct bu_hash_tbl *hsh_tbl, char *str )
{
	unsigned long index;
	struct bu_hash_entry *hsh_entry;

	BU_CK_HASH_TBL( hsh_tbl );

	fprintf( stderr, "%s\n", str );
	fprintf( stderr, "bu_hash_table (%ld entries):\n", hsh_tbl->num_entries );

	/* visit all the entries in this table */
	for( index=0 ; index<hsh_tbl->num_lists ; index++ ) {
		hsh_entry = hsh_tbl->lists[index];
		while( hsh_entry ) {
			BU_CK_HASH_ENTRY( hsh_entry );
			fprintf( stderr, "\tindex=%ld, key=x%lx, value=x%lx\n", index, (unsigned long int)hsh_entry->key, (unsigned long int)hsh_entry->value );
			hsh_entry = hsh_entry->next;
		}
	}
}

/**			B U _ H A S H _ T B L _ F R E E
 *@brief
 *	Free all the memory associated with the specified hash table.
 *	Note that the keys are freed (they are copies), but the "values" are not freed.
 *	(The values are merely pointers)
 */
void
bu_hash_tbl_free( struct bu_hash_tbl *hsh_tbl )
{
	unsigned long index;
	struct bu_hash_entry *hsh_entry, *tmp;

	BU_CK_HASH_TBL( hsh_tbl );

	/* loop through all the bins in this hash table */
	for( index=0 ; index<hsh_tbl->num_lists ; index++ ) {
		/* traverse all the entries in the list for this bin */
		hsh_entry = hsh_tbl->lists[index];
		while( hsh_entry ) {
			BU_CK_HASH_ENTRY( hsh_entry );
			tmp = hsh_entry->next;

			/* free the copy of the key, and this entry */
			free( hsh_entry->key );
			free( hsh_entry );

			/* step to next entry in libnked list */
			hsh_entry = tmp;
		}
	}

	/* free the array of bins */
	free( hsh_tbl->lists );

	/* free the actual hash table structure */
	free( hsh_tbl );
}


/**			B U _ H A S H _ T B L _ F I R S T
 *@brief
 *	get the "first" entry in a hash table
 *
 *
 * @param[in] hsh_tbl - the hash table of interest
 * @param[in] rec - an empty "bu_hash_record" structure for use by this routine and "bu_hash_tbl_next"
 *
 * @return
 *	the first non-null entry in the hash table, or NULL if there are no entries
 *	(Note that the order of enties is not likely to have any significance)
 */
struct bu_hash_entry *
bu_hash_tbl_first( struct bu_hash_tbl *hsh_tbl, struct bu_hash_record *rec )
{
	BU_CK_HASH_TBL( hsh_tbl );

	/* initialize the record structure */
	rec->magic = BU_HASH_RECORD_MAGIC;
	rec->tbl = hsh_tbl;
	rec->index = -1;
	rec->hsh_entry = (struct bu_hash_entry *)NULL;

	if( hsh_tbl->num_entries == 0 ) {
		/* this table is empty */
		return( (struct bu_hash_entry *)NULL );
	}

	/* loop through all the bins in this hash table, looking for a non-null entry */
	for( rec->index=0 ; rec->index < hsh_tbl->num_lists ; rec->index++ ) {
		rec->hsh_entry = hsh_tbl->lists[rec->index];
		if( rec->hsh_entry ) {
			return( rec->hsh_entry );
		}
	}

	/* no entry found, return NULL */
	return( (struct bu_hash_entry *)NULL );
}

/**			B U _ H A S H _ T B L _ N E X T
 *
 *	get the "next" entry in a hash table
 *
 * input:
 *	rec - the "bu_hash_record" structure that was passed to "bu_hash_tbl_first"
 *
 * return:
 *	the "next" non-null hash entry in this hash table
 */
struct bu_hash_entry *
bu_hash_tbl_next( struct bu_hash_record *rec )
{
	struct bu_hash_tbl *hsh_tbl;

	BU_CK_HASH_RECORD( rec );
	hsh_tbl = rec->tbl;
	BU_CK_HASH_TBL( hsh_tbl );

	/* if the entry in the record structure has a non-null "next",
	 * return it, and update the record structure
	 */
	if( rec->hsh_entry ) {
		rec->hsh_entry = rec->hsh_entry->next;
		if( rec->hsh_entry ) {
			return( rec->hsh_entry );
		}
	}

	/* must move to a new bin to find another entry */
	for( rec->index++ ; rec->index < hsh_tbl->num_lists ; rec->index++ ) {
		rec->hsh_entry = hsh_tbl->lists[rec->index];
		if( rec->hsh_entry ) {
			return( rec->hsh_entry );
		}
	}

	/* no more entries, return NULL */
	return( (struct bu_hash_entry *)NULL );
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
