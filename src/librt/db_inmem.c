/*                     D B _ I N M E M . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2025 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_inmem.c
 *
 * Routines in support of maintaining geometry in-memory-only.  The
 * general process for adding geometry to an inmem database is to
 * either:
 *
 * 1) call wdb_export_external() providing an external representation
 * of the geometry object and a flag marking it as in-memory
 * (preferred), or
 *
 * 2) call db_diradd() and mark the directory entry as in-memory via a
 * call to db_inmem() providing an external representation.
 *
 */

#include "common.h"

#include "bio.h"

#include "vmath.h"
#include "rt/db4.h"
#include "raytrace.h"

#include "./librt_private.h"

#define DEFAULT_DB_TITLE "Untitled BRL-CAD Database"

struct db_i *
db_open_inmem(void)
{
    register struct db_i *dbip = DBI_NULL;
    register int i;

    BU_ALLOC(dbip, struct db_i);
    dbip->dbi_eof = (b_off_t)-1L;
    dbip->dbi_fp = NULL;
    dbip->dbi_mf = NULL;
    dbip->i = NULL;

    /* XXX it "should" be safe and recommended to set this to 1 as it
     * merely toggles whether the data can be written to _disk_.  the
     * various uses, however, haven't been exhaustively checked so
     * leave it off for now.
     */
    dbip->dbi_read_only = 0;

    /* Initialize fields */
    for (i = 0; i < RT_DBNHASH; i++) {
	dbip->dbi_Head[i] = RT_DIR_NULL;
    }

    dbip->dbi_local2base = 1.0;		/* mm */
    dbip->dbi_base2local = 1.0;
    dbip->dbi_title = bu_strdup(DEFAULT_DB_TITLE);
    dbip->dbi_uses = 1;
    dbip->dbi_filename = NULL;
    dbip->dbi_filepath = NULL;
    dbip->dbi_version = 5;

    /* XXX might want/need to stash an ident record so it's valid.
     * see db_fwrite_ident();
     */

    bu_ptbl_init(&dbip->dbi_clients, 128, "dbi_clients[]");
    bu_ptbl_init(&dbip->dbi_changed_clbks , 8, "dbi_changed_clbks]");
    bu_ptbl_init(&dbip->dbi_update_nref_clbks, 8, "dbi_update_nref_clbks");

    dbip->dbi_use_comb_instance_ids = 0;
    dbip->dbi_magic = DBI_MAGIC;		/* Now it's valid */

    /* These wdb modes aren't valid for an in-mem db */
    dbip->dbi_wdbp = NULL;
    dbip->dbi_wdbp_a = NULL;

    /* Set up the valid in-mem wdb modes */
    BU_ALLOC(dbip->dbi_wdbp_inmem, struct rt_wdb);
    wdb_init(dbip->dbi_wdbp_inmem, dbip, RT_WDB_TYPE_DB_INMEM);

    BU_ALLOC(dbip->dbi_wdbp_inmem_a, struct rt_wdb);
    wdb_init(dbip->dbi_wdbp_inmem_a, dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);

    return dbip;
}

struct db_i *
db_create_inmem(void) {
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct bu_external obj;
    struct bu_attribute_value_set avs;
    struct bu_vls units = BU_VLS_INIT_ZERO;
    struct bu_external attr;
    int flags;

    dbip = db_open_inmem();

    RT_CK_DBI(dbip);

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    RT_CK_WDB(wdbp);

    /* Second, create the attribute-only _GLOBAL object */
    bu_vls_printf(&units, "%.25e", dbip->dbi_local2base);

    bu_avs_init(&avs, 4, "db_create_inmem");
    bu_avs_add(&avs, "title", dbip->dbi_title);
    bu_avs_add(&avs, "units", bu_vls_addr(&units));

    db5_export_attributes(&attr, &avs);
    db5_export_object3(&obj, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		       DB5_GLOBAL_OBJECT_NAME, DB5HDR_HFLAGS_HIDDEN_OBJECT, &attr, NULL,
		       DB5_MAJORTYPE_ATTRIBUTE_ONLY, 0,
		       DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
    flags = RT_DIR_HIDDEN | RT_DIR_NON_GEOM | RT_DIR_INMEM;
    wdb_export_external(wdbp, &obj, DB5_GLOBAL_OBJECT_NAME, flags, 0);

    bu_free_external(&obj);
    bu_free_external(&attr);
    bu_avs_free(&avs);
    bu_vls_free(&units);

    return dbip;
}

void
db_inmem(struct directory *dp, struct bu_external *ext, int flags, struct db_i *dbip)
{
    BU_CK_EXTERNAL(ext);
    RT_CK_DIR(dp);

    if (dp->d_flags & RT_DIR_INMEM)
	bu_free(dp->d_un.ptr, "db_inmem() ext ptr");
    dp->d_un.ptr = ext->ext_buf;
    if (db_version(dbip) < 5) {
	/* DB_MINREC granule size */
	dp->d_len = ext->ext_nbytes / 128;
    } else {
	dp->d_len = ext->ext_nbytes;
    }
    dp->d_flags = flags | RT_DIR_INMEM;

    /* Empty out the external structure, but leave it w/valid magic */
    ext->ext_buf = (uint8_t *)NULL;
    ext->ext_nbytes = 0;
}


/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
