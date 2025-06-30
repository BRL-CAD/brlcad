/*                   D B _ I N S T A N C E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
#include "bn/tol.h"
#include "rt/mem.h"
#include "rt/op.h"
#include "rt/directory.h"
#include "rt/anim.h"
#include "rt/tol.h"

__BEGIN_DECLS

struct db_i;   /* forward declaration */
struct rt_wdb; /* forward declaration */

/* Callback called when database objects are changed.  The int indicates
 * the change type (0 = mod, 1 = add, 2 = rm). ctx is a user
 * supplied context and is passed back as the last argument to db_change.
 */
typedef void (*dbi_changed_t)(struct db_i *, struct directory *, int, void *);


/* Callback called when references are updated. Args are:
 *
 * 1. parent dp,
 * 2. child dp referenced by parent dp
 * 3. the child name (should be available even if the child dp is null, which can happen with references to
 * non-existent objects)
 * 4. the boolean operation used to include the child
 * 5. the matrix above the child (NULL == IDN matrix)
 * 6. dbi_u_data (generally application context set for use in these callbacks)
 *
 * There are two particular sets of callback args that have special significance:
 *
 * NULL, NULL, NULL, DB_OP_UNION, NULL == the beginning of a db_update_nref cycle
 * NULL, NULL, NULL, DB_OP_SUBTRACT, NULL == the end of a db_update_nref cycle
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
typedef void (*dbi_update_nref_t)(struct db_i *, struct directory *, struct directory *, const char *, db_op_t, matp_t, void *);

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
 * TODO - make a db_i_internal struct, and move all the LIBRT ONLY
 * elements into it.  That will also give us a place to do fancier
 * database state management using things like C++ containers without
 * bothering the public API.
 */
struct db_i_internal;
struct db_i {
    uint32_t dbi_magic;         /**< @brief magic number */

    /* THESE ELEMENTS ARE AVAILABLE FOR APPLICATIONS TO READ */

    char * dbi_filename;                /**< @brief file name */
    int dbi_read_only;                  /**< @brief !0 => read only file */
    double dbi_local2base;              /**< @brief local2mm */
    double dbi_base2local;              /**< @brief unit conversion factors */
    char * dbi_title;                   /**< @brief title from IDENT rec */
    char ** dbi_filepath;               /**< @brief search path for aux file opens (convenience var) */

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
    int dbi_version;                    /**< @brief PRIVATE: use db_version(), is negative for flipped v4 */
    struct rt_wdb * dbi_wdbp;           /**< @brief PRIVATE: disk rt_wdb */
    struct rt_wdb * dbi_wdbp_a;         /**< @brief PRIVATE: disk append-only rt_wdb */
    struct rt_wdb * dbi_wdbp_inmem;     /**< @brief PRIVATE: inmem rt_wdb */
    struct rt_wdb * dbi_wdbp_inmem_a;   /**< @brief PRIVATE: inmem append-only rt_wdb */
    struct bu_ptbl dbi_changed_clbks;     /**< @brief PRIVATE: dbi_changed_t callbacks registered with dbi */
    struct bu_ptbl dbi_update_nref_clbks; /**< @brief PRIVATE: dbi_update_nref_t callbacks registered with dbi */
    int dbi_use_comb_instance_ids;            /**< @brief PRIVATE: flag to enable/disable comb instance tracking in full paths */

    struct db_i_internal *i;
};
#define DBI_NULL ((struct db_i *)0)
#define RT_CHECK_DBI(_p) BU_CKMAG(_p, DBI_MAGIC, "struct db_i")
#define RT_CK_DBI(_p) RT_CHECK_DBI(_p)

/* Functions for registering and unregistering callbacks with a dbip */
extern RT_EXPORT int db_add_changed_clbk(struct db_i *dbip, dbi_changed_t c, void *u_data);
extern RT_EXPORT int db_rm_changed_clbk(struct db_i *dbip, dbi_changed_t c, void *u_data);

extern RT_EXPORT int db_add_update_nref_clbk(struct db_i *dbip, dbi_update_nref_t c, void *u_data);
extern RT_EXPORT int db_rm_update_nref_clbk(struct db_i *dbip, dbi_update_nref_t c, void *u_data);

/**
 * for db_open(), open the specified file as read-only
 */
#define DB_OPEN_READONLY "r"

/**
 * for db_open(), open the specified file as read-write
 */
#define DB_OPEN_READWRITE "rw"

/**
 * Open the named database.
 *
 * The 'name' parameter specifies the file or filepath to a .g
 * geometry database file for reading and/or writing.
 *
 * The 'mode' parameter specifies whether to open read-only or in
 * read-write mode, specified via the DB_OPEN_READONLY and
 * DB_OPEN_READWRITE symbols respectively.
 *
 * As a convenience, the returned db_t structure's dbi_filepath field
 * is a C-style argv array of dirs to search when attempting to open
 * related files (such as data files for EBM solids or texture-maps).
 * The default values are "." and the directory containing the ".g"
 * file.  They may be overridden by setting the environment variable
 * BRLCAD_FILE_PATH.
 *
 * Returns:
 * DBI_NULL error
 * db_i * success
 */
RT_EXPORT extern struct db_i *
db_open(const char *name, const char *mode);

/**
 * "open" an in-memory-only database instance.  this initializes a
 * dbip for use, creating an inmem dbi_wdbp as the means to add
 * geometry to the directory (use wdb_export_external()).
 */
RT_EXPORT extern struct db_i * db_open_inmem(void);


/**
 * Create a new database containing just a header record, regardless
 * of whether the database previously existed or not, and open it for
 * reading and writing.
 *
 * This routine also calls db_dirbuild(), so the caller doesn't need
 * to.
 *
 * Returns:
 * DBI_NULL on error
 * db_i * on success
 */
RT_EXPORT extern struct db_i *
db_create(const char *name, int version);

/**
 * creates an in-memory-only database.  this is very similar to
 * db_open_inmem() with the exception that the this routine adds a
 * default _GLOBAL object.
 */
RT_EXPORT extern struct db_i * db_create_inmem(void);


/**
 * Close a database, releasing dynamic memory. Will also release the db_i
 * struct memory itself, not just the internal struct contents.  However, the
 * actual freeing of memory Waits until last user is done - db_close is a no-op
 * if dbi_uses is greater than 1.
 */
RT_EXPORT extern void db_close(struct db_i *dbip);


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
