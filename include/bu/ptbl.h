/*                         P T B L . H
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

/** @defgroup container Data Containers */
/**   @defgroup ptbl Pointer Tables */

/** @file ptbl.h
 *
 */
#ifndef BU_PTBL_H
#define BU_PTBL_H

#include "common.h"

#include <stddef.h> /* for size_t */
#include <sys/types.h> /* for off_t */

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/list.h"

/* ptbl.c */
/** @addtogroup ptbl */
/** @{ */
/** @file libbu/ptbl.c
 *
 * Support for generalized "pointer tables"
 *
 * Support for generalized "pointer tables", kept compactly in a
 * dynamic array.
 *
 * The table is currently un-ordered, and is merely an array of
 * pointers.  The support routines BU_*PTBL* and bu_ptbl* manipulate the
 * array for you.  Pointers to be operated on (inserted, deleted, searched
 * for) are passed as a "pointer to long".
 *
 */


/**
 * Support for generalized "pointer tables".
 */
struct bu_ptbl {
    struct bu_list l; /**< linked list for caller's use */
    off_t end;        /**< index into buffer of first available location */
    size_t blen;      /**< # of (long *)'s worth of storage at *buffer */
    long **buffer;    /**< data storage area */
};
typedef struct bu_ptbl bu_ptbl_t;
#define BU_PTBL_NULL ((struct bu_ptbl *)0)

/**
 * assert the integrity of a bu_ptbl struct pointer.
 */
#define BU_CK_PTBL(_p) BU_CKMAG(_p, BU_PTBL_MAGIC, "bu_ptbl")

/**
 * initialize a bu_ptbl struct without allocating any memory.  this
 * macro is not suitable for initializing a list head node.
 */
#define BU_PTBL_INIT(_p) { \
	BU_LIST_INIT_MAGIC(&(_p)->l, BU_PTBL_MAGIC); \
	(_p)->end = 0; \
	(_p)->blen = 0; \
	(_p)->buffer = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_ptbl struct.  does not allocate memory.  not suitable for
 * initializing a list head node.
 */
#define BU_PTBL_INIT_ZERO { {BU_PTBL_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, 0, 0, NULL }

/**
 * returns truthfully whether a bu_ptbl has been initialized via
 * BU_PTBL_INIT() or BU_PTBL_INIT_ZERO.
 */
#define BU_PTBL_IS_INITIALIZED(_p) (((struct bu_ptbl *)(_p) != BU_PTBL_NULL) && LIKELY((_p)->l.magic == BU_PTBL_MAGIC))


/*
 * For those routines that have to "peek" into the ptbl a little bit.
 */
#define BU_PTBL_BASEADDR(ptbl)	((ptbl)->buffer)
#define BU_PTBL_LASTADDR(ptbl)	((ptbl)->buffer + (ptbl)->end - 1)
#define BU_PTBL_END(ptbl)	((ptbl)->end)
#define BU_PTBL_LEN(ptbl)	((size_t)(ptbl)->end)
#define BU_PTBL_GET(ptbl, i)	((ptbl)->buffer[(i)])
#define BU_PTBL_SET(ptbl, i, val)	((ptbl)->buffer[(i)] = (long*)(val))
#define BU_PTBL_TEST(ptbl)	((ptbl)->l.magic == BU_PTBL_MAGIC)
#define BU_PTBL_CLEAR_I(_ptbl, _i) ((_ptbl)->buffer[(_i)] = (long *)0)

/**
 * A handy way to visit all the elements of the table is:
 *
 * struct edgeuse **eup;
 * for (eup = (struct edgeuse **)BU_PTBL_LASTADDR(&eutab); eup >= (struct edgeuse **)BU_PTBL_BASEADDR(&eutab); eup--) {
 *     NMG_CK_EDGEUSE(*eup);
 * }
 * --- OR ---
 * for (BU_PTBL_FOR(eup, (struct edgeuse **), &eutab)) {
 *     NMG_CK_EDGEUSE(*eup);
 * }
 */
#define BU_PTBL_FOR(ip, cast, ptbl)	\
    ip = cast BU_PTBL_LASTADDR(ptbl); ip >= cast BU_PTBL_BASEADDR(ptbl); ip--

/**
 * This collection of routines implements a "pointer table" data
 * structure providing a convenient mechanism for managing a collection
 * of pointers to objects.  This is useful where the size of the array
 * is not known in advance and may change with time.  It's convenient
 * to be able to write code that can say "remember this object", and
 * then later on iterate through the collection of remembered objects.
 *
 * When combined with the concept of placing "magic numbers" as the
 * first field of each data structure, the pointers to the objects
 * become automatically typed.
 */

/**
 * Initialize struct & get storage for table.
 * Recommend 8 or 64 for initial len.
 */
BU_EXPORT extern void bu_ptbl_init(struct bu_ptbl *b,
				   size_t len,
				   const char *str);

/**
 * Reset the table to have no elements, but retain any existing
 * storage.
 */
BU_EXPORT extern void bu_ptbl_reset(struct bu_ptbl *b);

/**
 * Append/Insert a (long *) item to/into the table.
 */
BU_EXPORT extern int bu_ptbl_ins(struct bu_ptbl *b,
				 long *p);

/**
 * locate a (long *) in an existing table
 *
 *
 * @return index of first matching element in array, if found
 * @return -1 if not found
 *
 * We do this a great deal, so make it go as fast as possible.  this
 * is the biggest argument I can make for changing to an ordered list.
 * Someday....
 */
BU_EXPORT extern int bu_ptbl_locate(const struct bu_ptbl *b,
				    const long *p);

/**
 * Set all occurrences of "p" in the table to zero.  This is different
 * than deleting them.
 */
BU_EXPORT extern void bu_ptbl_zero(struct bu_ptbl *b,
				   const long *p);

/**
 * Append item to table, if not already present.  Unique insert.
 *
 * @return index of first matching element in array, if found.  (table unchanged)
 * @return -1 if table extended to hold new element
 *
 * We do this a great deal, so make it go as fast as possible.  this
 * is the biggest argument I can make for changing to an ordered list.
 * Someday....
 */
BU_EXPORT extern int bu_ptbl_ins_unique(struct bu_ptbl *b, long *p);

/**
 * Remove all occurrences of an item from a table
 *
 * @return Number of copies of 'p' that were removed from the table.
 * @return 0 if none found.
 *
 * we go backwards down the table looking for occurrences of p to
 * delete.  We do it backwards to reduce the amount of data moved when
 * there is more than one occurrence of p in the table.  A pittance
 * savings, unless you're doing a lot of it.
 */
BU_EXPORT extern int bu_ptbl_rm(struct bu_ptbl *b,
				const long *p);

/**
 * Catenate one table onto end of another.  There is no checking for
 * duplication.
 */
BU_EXPORT extern void bu_ptbl_cat(struct bu_ptbl *dest,
				  const struct bu_ptbl *src);

/**
 * Catenate one table onto end of another, ensuring that no entry is
 * duplicated.  Duplications between multiple items in 'src' are not
 * caught.  The search is a nasty n**2 one.  The tables are expected
 * to be short.
 */
BU_EXPORT extern void bu_ptbl_cat_uniq(struct bu_ptbl *dest,
				       const struct bu_ptbl *src);

/**
 * Deallocate dynamic buffer associated with a table, and render this
 * table unusable without a subsequent bu_ptbl_init().
 */
BU_EXPORT extern void bu_ptbl_free(struct bu_ptbl *b);

/**
 * Print a bu_ptbl array for inspection.
 */
BU_EXPORT extern void bu_pr_ptbl(const char *title,
				 const struct bu_ptbl *tbl,
				 int verbose);

/**
 * truncate a bu_ptbl
 */
BU_EXPORT extern void bu_ptbl_trunc(struct bu_ptbl *tbl,
				    int end);

/** @} */

#endif  /* BU_PTBL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
