/*                     D B _ I N M E M . C
 * BRL-CAD
 *
 * Copyright (c) 2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup db */

/*@{*/
/** @file db_inmem.c
 *
 * Routines in support of maintaining geometry in-memory-only.  The
 * general process for adding geometry to an inmem database is to
 * either: 
 *
 *   1) call wdb_export_external() providing an external
 *   representation of the geometry object and a flag marking it as
 *   in-memory (preferred), or
 *
 *   2) call db_diradd() and mark the directory entry as in-memory via
 *   a call to db_inmem() providing an external representation.
 *
 * Functions -
 *      db_open_inmem	open a database marking it as in-memory-only
 *      db_create_inmem	create an in-memory-only database instance
 *	db_inmem	convert existing dir entry to in-memory-only
 *
 *  Authors -
 *      Christopher Sean Morrison
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#include "common.h"

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"


#define DEFAULT_DB_TITLE "Untitled BRL-CAD Database"

/**
 * d b _ o p e n _ i n m e m
 *
 * "open" an in-memory-only database instance.  this initializes a
 * dbip for use, creating an inmem dbi_wdbp as the means to add
 * geometry to the directory (use wdb_export_external()).
 */
struct db_i *
db_open_inmem(void)
{
    register struct db_i *dbip = DBI_NULL;
    register int i;

    BU_GETSTRUCT( dbip, db_i );
    dbip->dbi_eof = -1L;
    dbip->dbi_fd = -1;
    dbip->dbi_fp = NULL;
    dbip->dbi_mf = NULL;
    dbip->dbi_read_only = 1;

    /* Initialize fields */
    for( i=0; i<RT_DBNHASH; i++ ) {
	dbip->dbi_Head[i] = DIR_NULL;
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

    bu_ptbl_init( &dbip->dbi_clients, 128, "dbi_clients[]" );
    dbip->dbi_magic = DBI_MAGIC;		/* Now it's valid */

    /* mark the wdb structure as in-memory. */
    dbip->dbi_wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);

    return dbip;
}


/**
 * d b _ c r e a t e _ i n m e m
 *
 * creates an in-memory-only database.  this is very similar to
 * db_open_inmem() with the exception that the this routine adds a
 * default _GLOBAL object.
 */
struct db_i *
db_create_inmem(void) {
    struct db_i *dbip;
    struct bu_external obj;
    struct bu_attribute_value_set avs;
    struct bu_vls units;
    struct bu_external attr;
    int flags;

    dbip = db_open_inmem();

    RT_CK_DBI(dbip);
    RT_CK_WDB(dbip->dbi_wdbp);

#if 0
    /* create the header record? */
    db5_export_object3( &out, DB5HDR_HFLAGS_DLI_HEADER_OBJECT,
			NULL, 0, NULL, NULL,
			DB5_MAJORTYPE_RESERVED, 0,
			DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED );
#endif

    /* Second, create the attribute-only _GLOBAL object */
    bu_vls_init( &units );
    bu_vls_printf( &units, "%.25e", dbip->dbi_local2base );
    
    bu_avs_init( &avs, 4, "db_create_inmem" );
    bu_avs_add( &avs, "title", dbip->dbi_title );
    bu_avs_add( &avs, "units", bu_vls_addr(&units) );

    db5_export_attributes( &attr, &avs );
    db5_export_object3(&obj, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		       DB5_GLOBAL_OBJECT_NAME, DB5HDR_HFLAGS_HIDDEN_OBJECT, &attr, NULL,
		       DB5_MAJORTYPE_ATTRIBUTE_ONLY, 0,
		       DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED );
    flags = DIR_HIDDEN | DIR_NON_GEOM | RT_DIR_INMEM;
    wdb_export_external(dbip->dbi_wdbp, &obj, DB5_GLOBAL_OBJECT_NAME, flags, 0);

    bu_free_external( &obj );
    bu_free_external( &attr );
    bu_avs_free( &avs );
    bu_vls_free( &units );

    return dbip;
}


/**
 *			D B _ I N M E M
 *
 *  Transmogrify an existing directory entry to be an in-memory-only
 *  one, stealing the external representation from 'ext'.
 */
void
db_inmem(struct directory *dp, struct bu_external *ext, int flags, struct db_i *dbip)
{
    BU_CK_EXTERNAL(ext);
    RT_CK_DIR(dp);

    if( dp->d_flags & RT_DIR_INMEM )
	bu_free( dp->d_un.ptr, "db_inmem() ext ptr" );
    dp->d_un.ptr = ext->ext_buf;
    if( dbip->dbi_version < 5 ) {
	dp->d_len = ext->ext_nbytes / 128;	/* DB_MINREC granule size */
    } else {
	dp->d_len = ext->ext_nbytes;
    }
    dp->d_flags = flags | RT_DIR_INMEM;

    /* Empty out the external structure, but leave it w/valid magic */
    ext->ext_buf = (genptr_t)NULL;
    ext->ext_nbytes = 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
