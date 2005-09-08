
/* 
 * bltHash.h --
 *
 * Copyright 2001 Silicon Metrics Corporation.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Silicon Metrics disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 *
 * Bob Jenkins, 1996.  hash.c.  Public Domain.
 * Bob Jenkins, 1997.  lookup8.c.  Public Domain.
 *
 * Copyright (c) 1991-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#ifndef BLT_HASH_H
#define BLT_HASH_H 1

#ifndef BLT_INT_H
#ifndef SIZEOF_LONG
#define SIZEOF_LONG 4
#endif
#ifndef SIZEOF_LONG_LONG
#define SIZEOF_LONG_LONG 8
#endif
#ifndef SIZEOF_INT
#define SIZEOF_INT 4
#endif
#ifndef SIZEOF_VOID_P
#define SIZEOF_VOID_P 4
#endif
#ifndef HAVE_INTTYPES_H
#if 0
#define HAVE_INTTYPES_H 1
#endif
#endif
#endif /* !BLT_INT_H */

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if (SIZEOF_VOID_P == 8)
#if (SIZEOF_LONG == 8)
typedef signed long int64_t;
typedef unsigned long uint64_t;
#else
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif /* SIZEOF_LONG == 8 */
#else
#ifndef __CYGWIN__
typedef signed int int32_t;
#endif /* __CYGWIN__ */
typedef unsigned int uint32_t;
#endif /* SIZEOF_VOID_P == 8 */
#endif /* HAVE_INTTYPES_H */

#if (SIZEOF_VOID_P == 8) 
typedef uint64_t Blt_Hash;
#else
typedef uint32_t Blt_Hash;
#endif /* SIZEOF_VOID_P == 8 */

#include "bltPool.h"

/*
 * Acceptable key types for hash tables:
 */
#define BLT_STRING_KEYS		0
#define BLT_ONE_WORD_KEYS	((size_t)-1)

/*
 * Forward declaration of Blt_HashTable.  Needed by some C++ compilers
 * to prevent errors when the forward reference to Blt_HashTable is
 * encountered in the Blt_HashEntry structure.
 */

#ifdef __cplusplus
struct Blt_HashTable;
#endif

typedef union {			/* Key has one of these forms: */
    void *oneWordValue;		/* One-word value for key. */
    unsigned long words[1];	/* Multiple integer words for key.
				 * The actual size will be as large
				 * as necessary for this table's
				 * keys. */
    char string[4];		/* String for key.  The actual size
				 * will be as large as needed to hold
				 * the key. */
} Blt_HashKey;

/*
 * Structure definition for an entry in a hash table.  No-one outside
 * Blt should access any of these fields directly;  use the macros
 * defined below.
 */
typedef struct Blt_HashEntry {
    struct Blt_HashEntry *nextPtr;	/* Pointer to next entry in this
					 * hash bucket, or NULL for end of
					 * chain. */
    Blt_Hash hval;

    ClientData clientData;		/* Application stores something here
					 * with Blt_SetHashValue. */
    Blt_HashKey key;			/* MUST BE LAST FIELD IN RECORD!! */
} Blt_HashEntry;

/*
 * Structure definition for a hash table.  Must be in blt.h so clients
 * can allocate space for these structures, but clients should never
 * access any fields in this structure.
 */
#define BLT_SMALL_HASH_TABLE 4
typedef struct Blt_HashTable {
    Blt_HashEntry **buckets;		/* Pointer to bucket array.  Each
					 * element points to first entry in
					 * bucket's hash chain, or NULL. */
    Blt_HashEntry *staticBuckets[BLT_SMALL_HASH_TABLE];
					/* Bucket array used for small tables
					 * (to avoid mallocs and frees). */
    size_t numBuckets;		        /* Total number of buckets allocated
					 * at **buckets. */
    size_t numEntries;	                /* Total number of entries present
					 * in table. */
    size_t rebuildSize;	                /* Enlarge table when numEntries gets
					 * to be this large. */
    Blt_Hash mask;		        /* Mask value used in hashing
				         * function. */	
    unsigned int downShift;	        /* Shift count used in hashing
					 * function.  Designed to use high-
					 * order bits of randomized keys. */
    size_t keyType;			/* Type of keys used in this table. 
					 * It's either BLT_STRING_KEYS,
					 * BLT_ONE_WORD_KEYS, or an integer
					 * giving the number of ints that
                                         * is the size of the key.
					 */
    Blt_HashEntry *(*findProc) _ANSI_ARGS_((struct Blt_HashTable *tablePtr,
	    CONST void *key));
    Blt_HashEntry *(*createProc) _ANSI_ARGS_((struct Blt_HashTable *tablePtr,
	    CONST void *key, int *newPtr));

    Blt_Pool hPool;		/* Pointer to the pool allocator used
				 * for entries in this hash table. If
				 * NULL, the standard Tcl_Alloc,
				 * Tcl_Free routines will be used
				 * instead.
				 */
} Blt_HashTable;

/*
 * Structure definition for information used to keep track of searches
 * through hash tables:
 */

typedef struct {
    Blt_HashTable *tablePtr;		/* Table being searched. */
    unsigned long nextIndex;	        /* Index of next bucket to be
					 * enumerated after present one. */
    Blt_HashEntry *nextEntryPtr;	/* Next entry to be enumerated in the
					 * the current bucket. */
} Blt_HashSearch;

/*
 * Macros for clients to use to access fields of hash entries:
 */

#define Blt_GetHashValue(h) ((h)->clientData)
#define Blt_SetHashValue(h, value) ((h)->clientData = (ClientData)(value))
#define Blt_GetHashKey(tablePtr, h) \
    ((void *) (((tablePtr)->keyType == BLT_ONE_WORD_KEYS) ? \
     (void *)(h)->key.oneWordValue : (h)->key.string))

/*
 * Macros to use for clients to use to invoke find and create procedures
 * for hash tables:
 */
#define Blt_FindHashEntry(tablePtr, key) \
	(*((tablePtr)->findProc))(tablePtr, key)
#define Blt_CreateHashEntry(tablePtr, key, newPtr) \
	(*((tablePtr)->createProc))(tablePtr, key, newPtr)

EXTERN void Blt_InitHashTable _ANSI_ARGS_((Blt_HashTable *tablePtr, 
	size_t keyType));

EXTERN void Blt_InitHashTableWithPool _ANSI_ARGS_((Blt_HashTable *tablePtr, 
	size_t keyType));

EXTERN void Blt_DeleteHashTable _ANSI_ARGS_((Blt_HashTable *tablePtr));

EXTERN void Blt_DeleteHashEntry _ANSI_ARGS_((Blt_HashTable *tablePtr,
	Blt_HashEntry *entryPtr));

EXTERN Blt_HashEntry *Blt_FirstHashEntry _ANSI_ARGS_((Blt_HashTable *tablePtr,
	Blt_HashSearch *searchPtr));

EXTERN Blt_HashEntry *Blt_NextHashEntry _ANSI_ARGS_((Blt_HashSearch *srchPtr));

EXTERN char *Blt_HashStats _ANSI_ARGS_((Blt_HashTable *tablePtr));

#endif /* BLT_HASH_H */
