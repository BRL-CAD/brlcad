/*                         A V S . H
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

#ifndef BU_AVS_H
#define BU_AVS_H

#include "common.h"

#include <stddef.h> /* for size_t */

#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"

/*----------------------------------------------------------------------*/
/** @addtogroup avs */
/** @ingroup container */
/** @{ */
/** @file libbu/avs.c
 *
 * Routines to manage attribute/value sets.
 */

/** for attr and avs use.
 */
typedef enum {
  BU_ATTR_CREATED,
  BU_ATTR_MODIFIED
} bu_attr_time_t;

/**
 * These strings may or may not be individually allocated, it depends
 * on usage.
 */
/* FIXME: can this be made to include a union (or a struct pointer) to
 * allow for a binary attr?  if so, some (if not all) attr functions
 * will need to be modified; maybe add an artificial const string
 * value to indicate the binary attr which will probably never be
 * allowed to be changed other than programmatically (don't list,
 * i.e., keep them hidden? no, we will at least want to show a date
 * and time for the time stamp) */
struct bu_attribute_value_pair {
    const char *name;	    /**< attribute name           */
    const char *value;      /**< attribute value          */
#if defined(USE_BINARY_ATTRIBUTES)
    /* trying a solution to include binary attributes */
    unsigned int binvaluelen;
    const unsigned char *binvalue;
#endif
};


/**
 * A variable-sized attribute-value-pair array.
 *
 * avp points to an array of [max] slots.  The interface routines will
 * realloc to extend as needed.
 *
 * In general, each of the names and values is a local copy made with
 * bu_strdup(), and each string needs to be freed individually.
 * However, if a name or value pointer is between readonly_min and
 * readonly_max, then it is part of a big malloc block that is being
 * freed by the caller, and should not be individually freed.
 */
struct bu_attribute_value_set {
    uint32_t magic;
    size_t count;                        /**< # valid entries in avp */
    size_t max;                          /**< # allocated slots in avp */
    genptr_t readonly_min;
    genptr_t readonly_max;
    struct bu_attribute_value_pair *avp; /**< array[max] */
};
typedef struct bu_attribute_value_set bu_avs_t;
#define BU_AVS_NULL ((struct bu_attribute_value_set *)0)

/**
 * assert the integrity of a non-head node bu_attribute_value_set struct.
 */
#define BU_CK_AVS(_ap) BU_CKMAG(_ap, BU_AVS_MAGIC, "bu_attribute_value_set")

/**
 * initialize a bu_attribute_value_set struct without allocating any memory.
 */
#define BU_AVS_INIT(_ap) { \
	(_ap)->magic = BU_AVS_MAGIC; \
	(_ap)->count = (_ap)->max = 0; \
	(_ap)->readonly_min = (_ap)->readonly_max = (_ap)->avp = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_attribute_value_set struct.  does not allocate memory.
 */
#define BU_AVS_INIT_ZERO { BU_AVS_MAGIC, 0, 0, NULL, NULL, NULL }

/**
 * returns truthfully whether a bu_attribute_value_set has been initialized via
 * BU_AVS_INIT() or BU_AVS_INIT_ZERO.
 */
#define BU_AVS_IS_INITIALIZED(_ap) (((struct bu_attribute_value_set *)(_ap) != BU_AVS_NULL) && LIKELY((_ap)->magic == BU_AVS_MAGIC))


/**
 * For loop iterator for avs structures.
 *
 * Provide an attribute value pair struct pointer and an attribute
 * value set, and this will iterate over all entries.  iteration order
 * is not defined but should iterate over each AVS entry once.
 *
 * Example Use:
 @code
 void
 print_avs(struct bu_attribute_value_set *avs) {
   struct bu_attribute_value_pair *avpp;

   for (BU_AVS_FOR(avpp, avs)) {
     bu_log("key=%s, value=%s\n", avpp->name, avpp->value);
   }
 }
 @endcode
 *
 */
#define BU_AVS_FOR(_pp, _avp) \
    (_pp) = ((const void *)(_avp) != (const void *)NULL) ? ((_avp)->count > 0 ? &(_avp)->avp[(_avp)->count-1] : NULL) : NULL; ((const void *)(_pp) != (const void *)NULL) && ((const void *)(_avp) != (const void *)NULL) && (_avp)->avp && (_pp) >= (_avp)->avp; (_pp)--

/**
 * Some (but not all) attribute name and value string pointers are
 * taken from an on-disk format bu_external block, while others have
 * been bu_strdup()ed and need to be freed.  This macro indicates
 * whether the pointer needs to be freed or not.
 */
#define AVS_IS_FREEABLE(_avsp, _p)	\
    ((_avsp)->readonly_max == NULL \
     || (const_genptr_t)(_p) < (_avsp)->readonly_min \
     || (const_genptr_t)(_p) > (_avsp)->readonly_max)

/** @} */

/**
 * Initialize avs with storage for len entries.
 */
BU_EXPORT extern void bu_avs_init(struct bu_attribute_value_set *avp,
				  size_t len,
				  const char *str);

/**
 * Initialize an empty avs.
 */
BU_EXPORT extern void bu_avs_init_empty(struct bu_attribute_value_set *avp);

/**
 * Allocate storage for a new attribute/value set, with at least 'len'
 * slots pre-allocated.
 */
BU_EXPORT extern struct bu_attribute_value_set *bu_avs_new(size_t len,
							   const char *str);

/**
 * If the given attribute exists it will receive the new value,
 * otherwise the set will be extended to have a new attribute/value
 * pair.
 *
 * Returns -
 * 0 some error occurred
 * 1 existing attribute updated with new value
 * 2 set extended with new attribute/value pair
 */
BU_EXPORT extern int bu_avs_add(struct bu_attribute_value_set *avp,
				const char *attribute,
				const char *value);

/**
 * Add a bu_vls string as an attribute to a given attribute set,
 * updating the value if it already exists.
 */
BU_EXPORT extern int bu_avs_add_vls(struct bu_attribute_value_set *avp,
				    const char *attribute,
				    const struct bu_vls *value_vls);

/**
 * Add a name/value pair even if the name already exists in the set.
 */
BU_EXPORT extern void bu_avs_add_nonunique(struct bu_attribute_value_set *avsp,
					   const char *attribute,
					   const char *value);
/**
 * Take all the attributes from 'src' and merge them into 'dest' by
 * replacing an attribute if it already exists in the set.
 */
BU_EXPORT extern void bu_avs_merge(struct bu_attribute_value_set *dest,
				   const struct bu_attribute_value_set *src);

/**
 * Get the value of a given attribute from an attribute set.  The
 * behavior is not currently well-defined for AVS containing
 * non-unique attributes, but presently returns the first encountered.
 * Returns NULL if the requested attribute is not present in the set.
 */
BU_EXPORT extern const char *bu_avs_get(const struct bu_attribute_value_set *avp,
					const char *attribute);

/**
 * Remove all occurrences of an attribute from the provided attribute
 * set.
 *
 * @return
 *	-1	attribute not found in set
 * @return
 *	 0	OK
 */
BU_EXPORT extern int bu_avs_remove(struct bu_attribute_value_set *avp,
				   const char *attribute);

/**
 * Release all attributes in the provided attribute set.
 */
BU_EXPORT extern void bu_avs_free(struct bu_attribute_value_set *avp);

/**
 * Print all attributes in an attribute set in "name = value" form,
 * using the provided title.
 */
BU_EXPORT extern void bu_avs_print(const struct bu_attribute_value_set *avp,
				   const char *title);

/** @} */

#endif  /* BU_AVS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
