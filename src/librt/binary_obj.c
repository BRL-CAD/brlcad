/*                    B I N A R Y _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2007 United States Government as represented by
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
/** @addtogroup db5  */
/** @{ */
/** @file binary_obj.c
 *
 *  Routines for writing binary objects to a BRL-CAD database
 *  Assumes that some of the structure of such databases are known
 *  by the calling routines.
 *
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	John R. Anderson
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/** @} */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <sys/stat.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <limits.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

#ifndef __LONG_MAX__
#  define __LONG_MAX__ 2147483647L
#endif


int
rt_mk_binunif(struct rt_wdb *wdbp, const char *obj_name, const char *file_name, unsigned int minor_type, long max_count)
{
	struct stat st;
	unsigned int major_type=DB5_MAJORTYPE_BINARY_UNIF;
#if defined(_WIN32) && !defined(__CYGWIN__)
	__int64 num_items=-1;
	__int64 obj_length=-1;
#else
	long long num_items=-1;
	long long obj_length=-1;
#endif
	int item_length=0;
	struct bu_mapped_file *bu_fd;
	struct rt_binunif_internal *bip;
	struct rt_db_internal intern;
	struct bu_external body;
	struct bu_external bin_ext;
	struct directory *dp;

	if( (item_length=db5_type_sizeof_h_binu( minor_type ) ) <= 0 ) {
		bu_log( "Unrecognized minor type!!!\n" );
		return -1;
	}

	if( stat( file_name, &st ) ) {
		bu_log( "Cannot stat input file(%s)", file_name );
		return -1;
	}

	if( (bu_fd=bu_open_mapped_file( file_name, NULL)) == NULL ) {
		bu_log( "Cannot open input file(%s) for reading",
			      file_name );
		return -1;
	}

	/* create the rt_binunif internal form */
	GETSTRUCT( bip, rt_binunif_internal );
	bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
	bip->type = minor_type;

	num_items = (long)(st.st_size / item_length);

	/* maybe only a partial file read */
	if (max_count > 0 && max_count < num_items) {
	    num_items = max_count;
	}

	obj_length = num_items * item_length;

	if (obj_length > __LONG_MAX__) {
	    bu_log("Unable to create binary objects larger than %ld bytes\n", __LONG_MAX__);
	    return -1;
	}

	/* just copy the bytes */
	bip->count = num_items;
	bip->u.int8 = (char *)bu_malloc( obj_length, "binary uniform object" );
	memcpy( bip->u.int8, bu_fd->buf, obj_length );

	bu_close_mapped_file( bu_fd );

	/* create the rt_internal form */
	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_major_type = major_type;
	intern.idb_minor_type = minor_type;
	intern.idb_ptr = (genptr_t)bip;
	intern.idb_meth = &rt_functab[ID_BINUNIF];

	/* create body portion of external form */
	if( intern.idb_meth->ft_export5( &body, &intern, 1.0, wdbp->dbip, wdbp->wdb_resp, intern.idb_minor_type ) ) {

		bu_log( "Error while attemptimg to export %s\n", obj_name );
		rt_db_free_internal( &intern, wdbp->wdb_resp );
		return -1;
	}

	/* create entire external form */
	db5_export_object3( &bin_ext, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
			    obj_name, 0, NULL, &body,
			    intern.idb_major_type, intern.idb_minor_type,
			    DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED );

	rt_db_free_internal( &intern, wdbp->wdb_resp );
	bu_free_external( &body );

	/* add this object to the directory */
	if( (dp=db_diradd5( wdbp->dbip, obj_name, -1, major_type,
			    minor_type, 0, 0, NULL )) == DIR_NULL ) {
		bu_log( "Error while attemptimg to add new name (%s) to the database",
			obj_name );
		bu_free_external( &bin_ext );
		return -1;
	}

	/* and write it to the database */
	if( db_put_external5( &bin_ext, dp, wdbp->dbip ) ) {
		bu_log( "Error while adding new binary object (%s) to the database",
			obj_name );
		bu_free_external( &bin_ext );
		return -1;
	}

	bu_free_external( &bin_ext );

	return 0;
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
