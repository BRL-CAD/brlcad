/*                           W D B . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2014 United States Government as represented by
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


#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"


#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


struct rt_wdb *
wdb_fopen_v(const char *filename, int version)
{
    struct db_i *dbip;

    if ( filename == NULL ) return RT_WDB_NULL;

    if (rt_uniresource.re_magic != RESOURCE_MAGIC) {
	rt_init_resource(&rt_uniresource, 0, NULL);
    }

    if ((dbip = db_create(filename, version)) == DBI_NULL)
	return RT_WDB_NULL;

    return wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
}


struct rt_wdb *
wdb_fopen(const char *filename)
{
    if ( filename == NULL ) return RT_WDB_NULL;
    return wdb_fopen_v(filename, 5);
}


struct rt_wdb *
wdb_dbopen(struct db_i *dbip, int mode)
{
    struct rt_wdb *wdbp;

    RT_CK_DBI(dbip);

    if ((mode != RT_WDB_TYPE_DB_DISK) &&
	(mode != RT_WDB_TYPE_DB_DISK_APPEND_ONLY) &&
	(mode != RT_WDB_TYPE_DB_INMEM) &&
	(mode != RT_WDB_TYPE_DB_INMEM_APPEND_ONLY))
    {
	bu_log("wdb_dbopen(%s) mode %d unknown\n",
	       dbip->dbi_filename, mode);
	return RT_WDB_NULL;
    }

    if (rt_uniresource.re_magic != RESOURCE_MAGIC)
	rt_init_resource(&rt_uniresource, 0, NULL);

    BU_ALLOC(wdbp, struct rt_wdb);
    wdb_init(wdbp, dbip, mode);

    return wdbp;

}


int
wdb_import(struct rt_wdb *wdbp,	struct rt_db_internal *internp,	const char *name, const mat_t mat)
{
    struct directory *dp;

    dp = db_lookup(wdbp->dbip, name, LOOKUP_QUIET);
    if (dp  == RT_DIR_NULL)
	return -4;

    return rt_db_get_internal(internp, dp, wdbp->dbip, mat, &rt_uniresource);
}


int
wdb_export_external(
    struct rt_wdb *wdbp,
    struct bu_external *ep,
    const char *name,
    int flags,
    unsigned char type)
{
    struct directory *dp;
    int version;

    RT_CK_WDB(wdbp);
    BU_CK_EXTERNAL(ep);

    /* Stash name into external representation */
    version = db_version(wdbp->dbip);
    if (version < 5) {
	db_wrap_v4_external(ep, name);
    } else if (version == 5) {
	if (db_wrap_v5_external(ep, name) < 0) {
	    bu_log("wdb_export_external(%s): db_wrap_v5_external error\n",
		   name);
	    return -4;
	}
    } else {
	bu_log("wdb_export_external(%s): version %d unsupported\n",
	       name, version);
	return -4;
    }

    switch (wdbp->type) {

	case RT_WDB_TYPE_DB_DISK:
	    if (wdbp->dbip->dbi_read_only) {
		bu_log("wdb_export_external(%s): read-only database, write aborted\n", name);
		return -5;
	    }
	    /* If name already exists, that object will be updated. */
	    dp = db_lookup(wdbp->dbip, name, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		if ((dp = db_diradd(wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, flags, (void *)&type)) == RT_DIR_NULL) {
		    bu_log("wdb_export_external(%s): db_diradd error\n", name);
		    return -3;
		}
	    }
	    /* keep the caller's flags, except we don't want to
	     * pretend a disk dp is an inmem dp.  the data is
	     * read/written differently for both.
	     */
	    dp->d_flags = (dp->d_flags & ~7) | (flags & ~(RT_DIR_INMEM));
	    if (db_put_external(ep, dp, wdbp->dbip) < 0) {
		bu_log("wdb_export_external(%s): db_put_external error\n",
		       name);
		return -3;
	    }
	    break;

	case RT_WDB_TYPE_DB_DISK_APPEND_ONLY:
	    if (wdbp->dbip->dbi_read_only) {
		bu_log("wdb_export_external(%s): read-only database, write aborted\n", name);
		return -5;
	    }
	    /* If name already exists, new non-conflicting name will be generated */
	    if ((dp = db_diradd(wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, flags, (void *)&type)) == RT_DIR_NULL) {
		bu_log("wdb_export_external(%s): db_diradd error\n", name);
		return -3;
	    }
	    if (db_put_external(ep, dp, wdbp->dbip) < 0) {
		bu_log("wdb_export_external(%s): db_put_external error\n",
		       name);
		return -3;
	    }
	    break;

	case RT_WDB_TYPE_DB_INMEM_APPEND_ONLY:
	    dp = db_lookup(wdbp->dbip, name, 0);
	    if (dp != RT_DIR_NULL) {
		bu_log("wdb_export_external(%s): ERROR, that name is already in use, and APPEND_ONLY mode has been specified.\n", name);
		return -3;
	    }
	    dp = db_diradd(wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, flags, (void *)&type);
	    if (dp == RT_DIR_NULL) {
		bu_log("wdb_export_external(%s): db_diradd error\n",
		       name);
		return -3;
	    }

	    db_inmem(dp, ep, flags, wdbp->dbip);
	    /* ep->buf has been stolen, replaced with null. */
	    break;

	case RT_WDB_TYPE_DB_INMEM:
	    dp = db_lookup(wdbp->dbip, name, 0);
	    if (dp == RT_DIR_NULL) {
		dp = db_diradd(wdbp->dbip, name, RT_DIR_PHONY_ADDR, 0, flags, (void *)&type);
		if (dp == RT_DIR_NULL) {
		    bu_log("wdb_export_external(%s): db_diradd error\n", name);
		    bu_free_external(ep);
		    return -3;
		}
	    } else {
		dp->d_flags = (dp->d_flags & ~7) | flags;
	    }

	    db_inmem(dp, ep, flags, wdbp->dbip);
	    /* ep->buf has been stolen, replaced with null. */
	    break;
    }

    return 0;
}


int
wdb_put_internal(
    struct rt_wdb *wdbp,
    const char *name,
    struct rt_db_internal *ip,
    double local2mm)
{
    struct bu_external ext;
    int ret;
    int flags;

    RT_CK_WDB(wdbp);
    RT_CK_DB_INTERNAL(ip);

    if (db_version(wdbp->dbip) < 5) {
	BU_EXTERNAL_INIT(&ext);

	ret = -1;
	if (ip->idb_meth && ip->idb_meth->ft_export4) {
	    ret = ip->idb_meth->ft_export4(&ext, ip, local2mm, wdbp->dbip, &rt_uniresource);
	}
	if (ret < 0) {
	    bu_log("rt_db_put_internal(%s):  solid export failure\n",
		   name);
	    ret = -1;
	    goto out;
	}
	db_wrap_v4_external(&ext, name);
    } else {
	if (rt_db_cvt_to_external5(&ext, name, ip, local2mm, wdbp->dbip, &rt_uniresource, ip->idb_major_type) < 0) {
	    bu_log("wdb_export(%s): solid export failure\n",
		   name);
	    ret = -2;
	    goto out;
	}
    }
    BU_CK_EXTERNAL(&ext);

    flags = db_flags_internal(ip);
    ret = wdb_export_external(wdbp, &ext, name, flags, ip->idb_type);
out:
    bu_free_external(&ext);
    rt_db_free_internal(ip);
    return ret;
}


int
wdb_export(
    struct rt_wdb *wdbp,
    const char *name,
    void *gp,
    int id,
    double local2mm)
{
    struct rt_db_internal intern;

    RT_CK_WDB(wdbp);

    if ((id <= 0 || id > ID_MAX_SOLID) && id != ID_COMBINATION) {
	bu_log("wdb_export(%s): id=%d bad\n",
	       name, id);
	return -1;
    }

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = id;
    intern.idb_ptr = gp;
    intern.idb_meth = &OBJ[id];

    return wdb_put_internal(wdbp, name, &intern, local2mm);
}


void
wdb_init(struct rt_wdb *wdbp, struct db_i *dbip, int mode)
{
    BU_LIST_INIT_MAGIC(&wdbp->l, RT_WDB_MAGIC);

    wdbp->type = mode;
    wdbp->dbip = dbip;
    wdbp->dbip->dbi_wdbp = wdbp;

    /* Provide the same default tolerance that librt/prep.c does */
    wdbp->wdb_tol.magic = BN_TOL_MAGIC;
    wdbp->wdb_tol.dist = 0.0005;
    wdbp->wdb_tol.dist_sq = wdbp->wdb_tol.dist * wdbp->wdb_tol.dist;
    wdbp->wdb_tol.perp = 1e-6;
    wdbp->wdb_tol.para = 1 - wdbp->wdb_tol.perp;

    wdbp->wdb_ttol.magic = RT_TESS_TOL_MAGIC;
    wdbp->wdb_ttol.abs = 0.0;
    wdbp->wdb_ttol.rel = 0.01;
    wdbp->wdb_ttol.norm = 0;

    bu_vls_init(&wdbp->wdb_name);
    bu_vls_init(&wdbp->wdb_prestr);

    /* initialize tree state */
    wdbp->wdb_initial_tree_state = rt_initial_tree_state;  /* struct copy */
    wdbp->wdb_initial_tree_state.ts_ttol = &wdbp->wdb_ttol;
    wdbp->wdb_initial_tree_state.ts_tol = &wdbp->wdb_tol;

    /* default region ident codes */
    wdbp->wdb_item_default = 1000;
    wdbp->wdb_air_default = 0;
    wdbp->wdb_mat_default = 1;
    wdbp->wdb_los_default = 100;

    /* resource structure */
    wdbp->wdb_resp = &rt_uniresource;
}


void
wdb_close(struct rt_wdb *wdbp)
{

    RT_CK_WDB(wdbp);

    BU_LIST_DEQUEUE(&wdbp->l);
    BU_LIST_MAGIC_SET(&wdbp->l, 0); /* sanity */

    /* XXX Flush any unwritten "struct matter" records here */

    if (wdbp->dbip) {
	db_close(wdbp->dbip);
	wdbp->dbip = NULL;
    }

    /* release allocated member memory */
    bu_vls_free(&wdbp->wdb_name);
    bu_vls_free(&wdbp->wdb_prestr);

    /* sanity */
    wdbp->type = 0;
    wdbp->wdb_resp = NULL;
    wdbp->wdb_interp = NULL;

    /* release memory */
    bu_free((void *)wdbp, "struct rt_wdb");
    wdbp = NULL;
}


int
wdb_import_from_path2(struct bu_vls *logstr, struct rt_db_internal *ip, const char *path, struct rt_wdb *wdb, matp_t matp)
{
    struct db_i *dbip;
    int status;

    /* Can't run RT_CK_DB_INTERNAL(ip), it hasn't been filled in yet */
    RT_CK_WDB(wdb);
    dbip = wdb->dbip;
    RT_CK_DBI(dbip);

    if (strchr(path, '/')) {
	/* This is a path */
	struct db_tree_state ts;
	struct db_full_path old_path;
	struct db_full_path new_path;
	struct directory *dp_curr;
	int ret;

	db_init_db_tree_state(&ts, dbip, &rt_uniresource);
	db_full_path_init(&old_path);
	db_full_path_init(&new_path);

	if (db_string_to_path(&new_path, dbip, path) < 0) {
	    bu_vls_printf(logstr, "wdb_import_from_path: '%s' contains unknown object names\n", path);
	    return BRLCAD_ERROR;
	}

	dp_curr = DB_FULL_PATH_CUR_DIR(&new_path);
	ret = db_follow_path(&ts, &old_path, &new_path, LOOKUP_NOISY, 0);
	db_free_full_path(&old_path);
	db_free_full_path(&new_path);

	MAT_COPY(matp, ts.ts_mat);

	if (!dp_curr || ret < 0) {
	    bu_vls_printf(logstr, "wdb_import_from_path: '%s' is a bad path\n", path);
	    return BRLCAD_ERROR;
	}

	status = wdb_import(wdb, ip, dp_curr->d_namep, ts.ts_mat);
	if (status == -4) {
	    bu_vls_printf(logstr, "%s not found in path %s\n", dp_curr->d_namep, path);
	    return BRLCAD_ERROR;
	}
	if (status < 0) {
	    bu_vls_printf(logstr, "wdb_import failure: %s", dp_curr->d_namep);
	    return BRLCAD_ERROR;
	}
    } else {
	MAT_IDN(matp);

	status = wdb_import(wdb, ip, path, (matp_t)NULL);
	if (status == -4) {
	    bu_vls_printf(logstr, "%s: not found\n", path);
	    return BRLCAD_ERROR;
	}
	if (status < 0) {
	    bu_vls_printf(logstr, "wdb_import failure: %s", path);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}


int
wdb_import_from_path(struct bu_vls *logstr, struct rt_db_internal *ip, const char *path, struct rt_wdb *wdb)
{
    mat_t mat;

    return wdb_import_from_path2(logstr, ip, path, wdb, mat);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
