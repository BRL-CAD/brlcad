/*                   D B _ I N S T A N C E . H
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
/** @file db_instance.h
 *
 */

#ifndef RT_DB_INSTANCE_H
#define RT_DB_INSTANCE_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "bu/magic.h"
#include "bu/file.h"
#include "bu/mapped_file.h"
#include "bu/ptbl.h"
#include "rt/mem.h"
#include "rt/op.h"
#include "rt/directory.h"
#include "rt/anim.h"

__BEGIN_DECLS

struct rt_wdb; /* forward declaration */

/**
 * One of these structures is used to describe each separate instance
 * of a BRL-CAD model database ".g" file.
 *
 * dbi_filepath is a C-style argv array of places to search when
 * opening related files (such as data files for EBM solids or
 * texture-maps).  The array and strings are all dynamically
 * allocated.
 *
 * Note that the current working units are specified as a conversion
 * factor to/from millimeters (they are the 'base' in local2base and
 * base2local) because database dimensional values are always stored
 * as millimeters (mm).  The units conversion factor only affects the
 * display and conversion of input values.  This helps prevent error
 * accumulation and improves numerical stability when calculations are
 * made.
 *
 */
struct db_i {
    uint32_t dbi_magic;         /**< @brief magic number */

    /* THESE ELEMENTS ARE AVAILABLE FOR APPLICATIONS TO READ */

    char * dbi_filename;                /**< @brief file name */
    int dbi_read_only;                  /**< @brief !0 => read only file */
    double dbi_local2base;              /**< @brief local2mm */
    double dbi_base2local;              /**< @brief unit conversion factors */
    char * dbi_title;                   /**< @brief title from IDENT rec */
    char ** dbi_filepath;               /**< @brief search path for aux file opens (convenience var) */

    /* Callback called when database objects are changed.  The int indicates
     * the change type (0 = mod, 1 = add, 2 = rm). ctx is a user
     * supplied context and is passed back as the last argument to db_change.
     */
    void (*dbi_changed)(struct directory *, int, void *);

    /* Callback called when references are updated. Args are:
     *
     * 1. parent dp,
     * 2. child dp referenced by parent dp
     * 3. the child name (should be available even if the child dp is null, which can happen with references to
     * non-existent objects)
     * 4. the boolean operation used to include the child
     * 5. the matrix above the child (NULL == IDN matrix)
     * 6. dbip ctx (generally application provided context for use in the callback)
     *
     * NOTE:  the contents of the child name and matrix should be copied by the
     * caller if they want to make use of them - they are not references to
     * stable storage.
     *
     * NOTE:  the parent may be a non-comb object, if extrudes or other primitives
     * that reference other primitives are present in the .g - the caller should
     * be aware of that when processing the results.
     *
     * librt only stores the reference count in d_nref, but applications may
     * need more explicit awareness of the parent/child relationships.  Since
     * db_update_nref must be called in any case for librt to function
     * properly, this callback lets the parent application benefit from the
     * work db_update_nref is already doing to figure out these relationships.
     * */
    void (*dbi_update_nref)(struct directory *, struct directory *, const char *, db_op_t, matp_t, void *);

    /* Application context supplied to callback functions */
    void *ctx;

    /* THESE ELEMENTS ARE FOR LIBRT ONLY, AND MAY CHANGE */

    struct directory * dbi_Head[RT_DBNHASH]; /** @brief PRIVATE: object hash table */
    FILE * dbi_fp;                      /**< @brief PRIVATE: standard file pointer */
    b_off_t dbi_eof;                      /**< @brief PRIVATE: End+1 pos after db_scan() */
    size_t dbi_nrec;                    /**< @brief PRIVATE: # records after db_scan() */
    int dbi_uses;                       /**< @brief PRIVATE: # of uses of this struct */
    struct mem_map * dbi_freep;         /**< @brief PRIVATE: map of free granules */
    void *dbi_inmem;                    /**< @brief PRIVATE: ptr to in-memory copy */
    struct animate * dbi_anroot;        /**< @brief PRIVATE: heads list of anim at root lvl */
    struct bu_mapped_file * dbi_mf;     /**< @brief PRIVATE: Only in read-only mode */
    struct bu_ptbl dbi_clients;         /**< @brief PRIVATE: List of rtip's using this db_i */
    int dbi_version;                    /**< @brief PRIVATE: use db_version() */
    struct rt_wdb * dbi_wdbp;           /**< @brief PRIVATE: ptr back to containing rt_wdb */
};
#define DBI_NULL ((struct db_i *)0)
#define RT_CHECK_DBI(_p) BU_CKMAG(_p, DBI_MAGIC, "struct db_i")
#define RT_CK_DBI(_p) RT_CHECK_DBI(_p)


__END_DECLS

#endif /* RT_DB_INSTANCE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
