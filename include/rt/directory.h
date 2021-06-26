/*                     D I R E C T O R Y . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @file directory.h
 *
 */

#ifndef RT_DIRECTORY_H
#define RT_DIRECTORY_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "rt/anim.h"

__BEGIN_DECLS

/**
 * One of these structures is allocated in memory to represent each
 * named object in the database.
 *
 * Note that a d_addr of RT_DIR_PHONY_ADDR ((b_off_t)-1) means that
 * database storage has not been allocated yet.
 *
 * Note that there is special handling for RT_DIR_INMEM "in memory"
 * overrides.
 *
 * Construction should be done only by using RT_GET_DIRECTORY()
 * Destruction should be done only by using db_dirdelete().
 *
 * Special note: In order to reduce the overhead of acquiring heap
 * memory (e.g., via bu_strdup()) to stash the name in d_namep, we
 * carry along enough storage for small names right in the structure
 * itself (d_shortname).  Thus, d_namep should never be assigned to
 * directly, it should always be accessed using RT_DIR_SET_NAMEP() and
 * RT_DIR_FREE_NAMEP().
 *
 * The in-memory name of an object should only be changed using
 * db_rename(), so that it can be requeued on the correct linked list,
 * based on new hash.  This should be followed by rt_db_put_internal()
 * on the object to modify the on-disk name.
 */
struct directory {
    uint32_t d_magic;   /**< @brief Magic number */
    char * d_namep;             /**< @brief pointer to name string */
    union {
	b_off_t file_offset;      /**< @brief disk address in obj file */
	void *ptr;              /**< @brief ptr to in-memory-only obj */
    } d_un;
    struct directory * d_forw;  /**< @brief link to next dir entry */
    struct animate * d_animate; /**< @brief link to animation */
    long d_uses;                /**< @brief number of uses, from instancing */
    size_t d_len;               /**< @brief number of of db granules used */
    long d_nref;                /**< @brief number of times ref'ed by COMBs */
    int d_flags;                /**< @brief flags */
    unsigned char d_major_type; /**< @brief object major type */
    unsigned char d_minor_type; /**< @brief object minor type */
    struct bu_list d_use_hd;    /**< @brief heads list of uses (struct soltab l2) */
    char d_shortname[16];       /**< @brief Stash short names locally */
    void *u_data;		/**< @brief void pointer hook for user data. user is responsible for freeing. */
};
#define RT_DIR_NULL     ((struct directory *)0)
#define RT_CK_DIR(_dp) BU_CKMAG(_dp, RT_DIR_MAGIC, "(librt)directory")

#define d_addr  d_un.file_offset
#define RT_DIR_PHONY_ADDR       ((b_off_t)-1)     /**< @brief Special marker for d_addr field */

/* flags for db_diradd() and friends */
#define RT_DIR_SOLID    0x1   /**< @brief this name is a solid */
#define RT_DIR_COMB     0x2   /**< @brief combination */
#define RT_DIR_REGION   0x4   /**< @brief region */
#define RT_DIR_HIDDEN   0x8   /**< @brief object name is hidden */
#define RT_DIR_NON_GEOM 0x10  /**< @brief object is not geometry (e.g. binary object) */
#define RT_DIR_USED     0x80  /**< @brief One bit, used similar to d_nref */
#define RT_DIR_INMEM    0x100 /**< @brief object is in memory (only) */

/**< @brief Args to db_lookup() */
#define LOOKUP_NOISY    1
#define LOOKUP_QUIET    0

#define FOR_ALL_DIRECTORY_START(_dp, _dbip) { int _i; \
    for (_i = RT_DBNHASH-1; _i >= 0; _i--) { \
    for ((_dp) = (_dbip)->dbi_Head[_i]; (_dp); (_dp) = (_dp)->d_forw) {

#define FOR_ALL_DIRECTORY_END   }}}

#define RT_DIR_SET_NAMEP(_dp, _name) { \
	if (strlen(_name) < sizeof((_dp)->d_shortname)) {\
	    bu_strlcpy((_dp)->d_shortname, (_name), sizeof((_dp)->d_shortname)); \
	    (_dp)->d_namep = (_dp)->d_shortname; \
	} else { \
	    (_dp)->d_namep = bu_strdup(_name); /* Calls bu_malloc() */ \
	} }


/**
 * Use this macro to free the d_namep member, which is sometimes not
 * dynamic.
 */
#define RT_DIR_FREE_NAMEP(_dp) { \
	if ((_dp)->d_namep != (_dp)->d_shortname)  \
	    bu_free((_dp)->d_namep, "d_namep"); \
	(_dp)->d_namep = NULL; }


/**
 * allocate and link in a new directory entry to the resource
 * structure's freelist
 */
#define RT_GET_DIRECTORY(_p, _res) { \
	while (((_p) = (_res)->re_directory_hd) == NULL) \
	    db_alloc_directory_block(_res); \
	(_res)->re_directory_hd = (_p)->d_forw; \
	(_p)->d_forw = NULL; }


/**
 * convert an argv list of names to a directory pointer array.
 *
 * If db_lookup fails for any individual argv, an empty directory
 * structure is created and assigned the name and RT_DIR_PHONY_ADDR
 *
 * The returned directory ** structure is NULL terminated.
 */
RT_EXPORT extern struct directory **db_argv_to_dpv(const struct db_i *dbip,
						   const char **argv);


/**
 * convert a directory pointer array to an argv char pointer array.
 */
RT_EXPORT extern char **db_dpv_to_argv(struct directory **dpv);



__END_DECLS

#endif /* RT_DIRECTORY_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
