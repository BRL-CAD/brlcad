/*                          W D B . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file wdb.h
 *
 */

#ifndef RT_WDB_H
#define RT_WDB_H

#include "common.h"
#include "vmath.h"
#include "bu/magic.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bu/bu_tcl.h" /* for bu_observer and Tcl_Interp */
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
    struct rt_tess_tol  wdb_ttol;
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
    struct bu_observer  wdb_observers;
    Tcl_Interp *        wdb_interp;

};


#define RT_CHECK_WDB(_p) BU_CKMAG(_p, RT_WDB_MAGIC, "rt_wdb")
#define RT_CK_WDB(_p) RT_CHECK_WDB(_p)
#define RT_WDB_INIT_ZERO { {RT_WDB_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, 0, NULL, RT_DBTS_INIT_ZERO, RT_TESS_TOL_INIT_ZERO, BN_TOL_INIT_ZERO, NULL, BU_VLS_INIT_ZERO, 0, 0, 0, 0, 0, 0, BU_VLS_INIT_ZERO, BU_OBSERVER_INIT_ZERO, NULL }
#define RT_WDB_NULL             ((struct rt_wdb *)NULL)
#define RT_WDB_TYPE_DB_DISK                     2
#define RT_WDB_TYPE_DB_DISK_APPEND_ONLY         3
#define RT_WDB_TYPE_DB_INMEM                    4
#define RT_WDB_TYPE_DB_INMEM_APPEND_ONLY        5


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
