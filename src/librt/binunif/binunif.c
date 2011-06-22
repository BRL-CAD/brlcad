/*                       B I N U N I F . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2011 United States Government as represented by
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
/** @addtogroup db5 */
/** @{ */
/** @file binunif.c
 *
 * Routines for writing binary objects to a BRL-CAD database
 * Assumes that some of the structure of such databases are known
 * by the calling routines.
 *
 * Return codes of 0 are OK, -1 signal an error.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


int
rt_mk_binunif(struct rt_wdb *wdbp, const char *obj_name, const char *file_name, unsigned int minor_type, size_t max_count)
{
    int ret;
    struct stat st;
    size_t num_items = 0;
    size_t obj_length = 0;
    size_t item_length = 0;
    unsigned int major_type = DB5_MAJORTYPE_BINARY_UNIF;
    struct directory *dp = NULL;
    struct bu_mapped_file *bu_fd = NULL;
    struct rt_binunif_internal *bip = NULL;
    struct bu_external body;
    struct bu_external bin_ext;
    struct rt_db_internal intern;

    item_length = db5_type_sizeof_h_binu(minor_type);
    if (item_length == 0) {
	bu_log("Unrecognized minor type (%d)!\n", minor_type);
	return -1;
    }

    if (stat(file_name, &st)) {
	bu_log("Cannot stat input file (%s)", file_name);
	return -1;
    }

    bu_fd = bu_open_mapped_file(file_name, NULL);
    if (bu_fd == NULL) {
	bu_log("Cannot open input file (%s) for reading", file_name);
	return -1;
    }

    /* create the rt_binunif internal form */
    BU_GETSTRUCT(bip, rt_binunif_internal);
    bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
    bip->type = minor_type;

    if (item_length != 0) {
	num_items = (size_t)(st.st_size / item_length);
    } else {
	num_items = 0;
    }

    /* maybe only a partial file read */
    if (max_count > 0 && max_count < num_items) {
	num_items = max_count;
    }

    obj_length = num_items * item_length;
    if (obj_length < 1) {
	obj_length = 1;
    }

    /* just copy the bytes */
    bip->count = (long)num_items;
    bip->u.int8 = (char *)bu_malloc(obj_length, "binary uniform object");
    memcpy(bip->u.int8, bu_fd->buf, obj_length);

    bu_close_mapped_file(bu_fd);

    /* create the rt_internal form */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = major_type;
    intern.idb_minor_type = minor_type;
    intern.idb_ptr = (genptr_t)bip;
    intern.idb_meth = &rt_functab[ID_BINUNIF];

    /* create body portion of external form */
    ret = -1;
    if (intern.idb_meth->ft_export5) {
	ret = intern.idb_meth->ft_export5(&body, &intern, 1.0, wdbp->dbip, wdbp->wdb_resp);
    }
    if (ret != 0) {
	bu_log("Error while attemptimg to export %s\n", obj_name);
	rt_db_free_internal(&intern);
	return -1;
    }

    /* create entire external form */
    db5_export_object3(&bin_ext, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		       obj_name, 0, NULL, &body,
		       intern.idb_major_type, intern.idb_minor_type,
		       DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);

    rt_db_free_internal(&intern);
    bu_free_external(&body);

    /* make sure the database directory is initialized */
    if (wdbp->dbip->dbi_eof == RT_DIR_PHONY_ADDR) {
	ret = db_dirbuild(wdbp->dbip);
	if (ret) {
	    return -1;
	}
    }

    /* add this (phony until written) object to the directory */
    if ((dp=db_diradd5(wdbp->dbip, obj_name, RT_DIR_PHONY_ADDR, major_type,
		       minor_type, 0, 0, NULL)) == RT_DIR_NULL) {
	bu_log("Error while attemptimg to add new name (%s) to the database",
	       obj_name);
	bu_free_external(&bin_ext);
	return -1;
    }

    /* and write it to the database */
    if (db_put_external5(&bin_ext, dp, wdbp->dbip)) {
	bu_log("Error while adding new binary object (%s) to the database",
	       obj_name);
	bu_free_external(&bin_ext);
	return -1;
    }

    bu_free_external(&bin_ext);

    return 0;
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
