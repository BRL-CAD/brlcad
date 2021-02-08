/*                          W D B . H
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
/** @file rt/wdb.h
 *
 */

#ifndef RT_WDB_H
#define RT_WDB_H

#include "common.h"

#include "vmath.h"
#include "bu/magic.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bu/observer.h"
#include "bn/tol.h"
#include "rt/db_instance.h"
#include "rt/tree.h"
#include "rt/tol.h"

__BEGIN_DECLS

struct resource;  /* forward declaration */

/**
 * This data structure is at the core of the "LIBWDB" support for
 * allowing application programs to read and write BRL-CAD databases.
 * Many different access styles are supported.
 */

struct rt_wdb {
    struct bu_list      l;
    int                 type;
    struct db_i *       dbip;
    struct db_tree_state        wdb_initial_tree_state;
    struct bg_tess_tol  wdb_ttol;
    struct bn_tol       wdb_tol;
    struct resource*    wdb_resp;

    /* variables for name prefixing */
    struct bu_vls       wdb_prestr;
    int                 wdb_ncharadd;
    int                 wdb_num_dups;

    /* default region ident codes for this particular database. */
    int                 wdb_item_default;/**< @brief  GIFT region ID */
    int                 wdb_air_default;
    int                 wdb_mat_default;/**< @brief  GIFT material code */
    int                 wdb_los_default;/**< @brief  Line-of-sight estimate */

    /* These members are marked for removal */
    struct bu_vls       wdb_name;       /**< @brief  database object name */
    struct bu_observer_list  wdb_observers;
    void *              wdb_interp; /**< @brief Tcl_Interp */
};


#define RT_CHECK_WDB(_p) BU_CKMAG(_p, RT_WDB_MAGIC, "rt_wdb")
#define RT_CK_WDB(_p) RT_CHECK_WDB(_p)
#define RT_WDB_INIT_ZERO { {RT_WDB_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, 0, NULL, RT_DBTS_INIT_ZERO, BG_TESS_TOL_INIT_ZERO, BN_TOL_INIT_ZERO, NULL, BU_VLS_INIT_ZERO, 0, 0, 0, 0, 0, 0, BU_VLS_INIT_ZERO, BU_OBSERVER_LIST_INIT_ZERO, NULL }
#define RT_WDB_NULL             ((struct rt_wdb *)NULL)
#define RT_WDB_TYPE_DB_DISK                     2
#define RT_WDB_TYPE_DB_DISK_APPEND_ONLY         3
#define RT_WDB_TYPE_DB_INMEM                    4
#define RT_WDB_TYPE_DB_INMEM_APPEND_ONLY        5


/**
 *
 * Routines to allow libwdb to use librt's import/export interface,
 * rather than having to know about the database formats directly.
 *
 */
RT_EXPORT extern struct rt_wdb *wdb_fopen(const char *filename);


/**
 * Create a libwdb output stream destined for a disk file.  This will
 * destroy any existing file by this name, and start fresh.  The file
 * is then opened in the normal "update" mode and an in-memory
 * directory is built along the way, allowing retrievals and object
 * replacements as needed.
 *
 * Users can change the database title by calling: ???
 */
RT_EXPORT extern struct rt_wdb *wdb_fopen_v(const char *filename,
					    int version);


/**
 * Create a libwdb output stream destined for an existing BRL-CAD
 * database, already opened via a db_open() call.
 *
 * RT_WDB_TYPE_DB_DISK Add to on-disk database
 * RT_WDB_TYPE_DB_DISK_APPEND_ONLY Add to on-disk database, don't clobber existing names, use prefix
 * RT_WDB_TYPE_DB_INMEM Add to in-memory database only
 * RT_WDB_TYPE_DB_INMEM_APPEND_ONLY Ditto, but give errors if name in use.
 */
RT_EXPORT extern struct rt_wdb *wdb_dbopen(struct db_i *dbip,
					   int mode);


/**
 * Returns -
 *  0 and modified *internp;
 * -1 ft_import failure (from rt_db_get_internal)
 * -2 db_get_external failure (from rt_db_get_internal)
 * -3 Attempt to import from write-only (stream) file.
 * -4 Name not found in database TOC.
 *
 * NON-PARALLEL because of rt_uniresource
 */
RT_EXPORT extern int wdb_import(struct rt_wdb *wdbp,
				struct rt_db_internal *internp,
				const char *name,
				const mat_t mat);


/**
 * The caller must free "ep".
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_export_external(struct rt_wdb *wdbp,
					 struct bu_external *ep,
					 const char *name,
					 int flags,
					 unsigned char minor_type);

/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.
 *
 * The internal representation is always freed.  This is the analog of
 * rt_db_put_internal() for rt_wdb objects.
 *
 * Use this routine in preference to wdb_export() whenever the caller
 * already has an rt_db_internal structure handy.
 *
 * NON-PARALLEL because of rt_uniresource
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_put_internal(struct rt_wdb *wdbp,
				      const char *name,
				      struct rt_db_internal *ip,
				      double local2mm);


/**
 * Export an in-memory representation of an object, as described in
 * the file h/rtgeom.h, into the indicated database.
 *
 * The internal representation (gp) is always freed.
 *
 * WARNING: The caller must be careful not to double-free gp,
 * particularly if it's been extracted from an rt_db_internal, e.g. by
 * passing intern.idb_ptr for gp.
 *
 * If the caller has an rt_db_internal structure handy already, they
 * should call wdb_put_internal() directly -- this is a convenience
 * routine intended primarily for internal use in LIBWDB.
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_export(struct rt_wdb *wdbp,
				const char *name,
				void *gp,
				int id,
				double local2mm);
RT_EXPORT extern void wdb_init(struct rt_wdb *wdbp,
			       struct db_i   *dbip,
			       int           mode);


/**
 * Release from associated database "file", destroy dynamic data
 * structure.
 */
RT_EXPORT extern void wdb_close(struct rt_wdb *wdbp);

/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling.
 *
 * Returns -
 * BRLCAD_OK
 * BRLCAD_ERROR
 */
RT_EXPORT extern int wdb_import_from_path(struct bu_vls *logstr,
					  struct rt_db_internal *ip,
					  const char *path,
					  struct rt_wdb *wdb);


/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling. Additionally,
 * copies ts.ts_mat to matp.
 *
 * Returns -
 * BRLCAD_OK
 * BRLCAD_ERROR
 */
RT_EXPORT extern int wdb_import_from_path2(struct bu_vls *logstr,
					   struct rt_db_internal *ip,
					   const char *path,
					   struct rt_wdb *wdb,
					   matp_t matp);


__END_DECLS

#endif /* RT_WDB_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
