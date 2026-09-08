/*                       B I N U N I F . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2026 United States Government as represented by
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

#include <stdint.h>
#include <string.h>

#include "bio.h"


#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "rt/binunif.h"
#include "raytrace.h"
#include "wdb.h"
#include "librt_private.h"


int
rt_mk_binunif(struct rt_wdb *wdbp, const char *obj_name,
	      const char *file_name, unsigned int input_type,
	      size_t max_count)
{
    size_t num_items = 0;
    size_t obj_length = 0;
    size_t item_length = 0;
    size_t allocation_length = 0;
    unsigned int input_flags = input_type & ~RT_BINUNIF_TYPE_MASK;
    unsigned int minor_type = input_type & RT_BINUNIF_TYPE_MASK;
    int network_order = input_flags & RT_BINUNIF_NETWORK_ORDER;
    struct bu_mapped_file *bu_fd = NULL;
    struct rt_binunif_internal *bip = NULL;
    struct rt_db_internal intern;
    struct bu_vls write_name = BU_VLS_INIT_ZERO;
    struct directory **headp = NULL;
    int ret;

    if (input_flags & ~RT_BINUNIF_NETWORK_ORDER) {
	bu_log("Unrecognized BINUNIF input flags (0x%x)\n", input_flags);
	return -1;
    }

    item_length = network_order ? db5_type_sizeof_n_binu(minor_type) :
	db5_type_sizeof_h_binu(minor_type);
    if (item_length == 0) {
	bu_log("Unrecognized BINUNIF minor type (%u)\n", minor_type);
	return -1;
    }

    bu_fd = bu_open_mapped_file(file_name, NULL);
    if (bu_fd == NULL) {
	bu_log("Cannot open input file (%s) for reading\n", file_name);
	return -1;
    }

    num_items = bu_fd->buflen / item_length;

    /* maybe only a partial file read */
    if (max_count > 0 && max_count < num_items) {
	num_items = max_count;
    }

    obj_length = num_items * item_length;
    RT_DB_INTERNAL_INIT(&intern);

    if (network_order && obj_length > 0) {
	struct bu_external body = BU_EXTERNAL_INIT_ZERO;
	body.ext_nbytes = obj_length;
	body.ext_buf = (uint8_t *)bu_fd->buf;
	if (rt_binunif_import5_minor_type(&intern, &body, NULL, wdbp->dbip, NULL, minor_type) != 0) {
	    bu_log("Error converting network-order input file (%s)\n", file_name);
	    bu_close_mapped_file(bu_fd);
	    rt_db_free_internal(&intern);
	    return -1;
	}
    } else {
	BU_ALLOC(bip, struct rt_binunif_internal);
	bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
	bip->type = minor_type;
	bip->count = num_items;
	allocation_length = obj_length > 0 ? obj_length : 1;
	bip->u.int8 = (char *)bu_malloc(allocation_length, "binary uniform object");
	if (obj_length > 0)
	    memcpy(bip->u.int8, bu_fd->buf, obj_length);

	intern.idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
	intern.idb_minor_type = minor_type;
	intern.idb_ptr = (void *)bip;
	intern.idb_meth = &OBJ[ID_BINUNIF];
    }

    bu_close_mapped_file(bu_fd);

    intern.idb_type = minor_type;
    intern.idb_minor_type = minor_type;

    if (wdbp->dbip->i->dbi_eof == RT_DIR_PHONY_ADDR && db_dirbuild(wdbp->dbip) != 0) {
	rt_db_free_internal(&intern);
	return -1;
    }

    bu_vls_strcpy(&write_name, obj_name);
    if (db_dircheck(wdbp->dbip, &write_name, 0, &headp) != 0) {
	bu_vls_free(&write_name);
	rt_db_free_internal(&intern);
	return -1;
    }

    ret = wdb_put_internal(wdbp, bu_vls_cstr(&write_name), &intern, 1.0);
    bu_vls_free(&write_name);
    return ret == 0 ? 0 : -1;
}

const char *
rt_binunif_type_to_string(int type)
{
    if (type < 0 || type > 15)
	return NULL;

    /**
     * this array depends on the values of the definitions of the
     * DB5_MINORTYPE_BINU_* in db5.h
     */
    static const char *binu_types[] = {
	NULL,
	NULL,
	"binary(float)",
	"binary(double)",
	"binary(u_8bit_int)",
	"binary(u_16bit_int)",
	"binary(u_32bit_int)",
	"binary(u_64bit_int)",
	NULL,
	NULL,
	NULL,
	NULL,
	"binary(8bit_int)",
	"binary(16bit_int)",
	"binary(32bit_int)",
	"binary(64bit_int)"
    };

    return binu_types[type];
}

const char *
rt_binunif_typestr(const struct directory *dp)
{
    if (!dp || dp->d_major_type != DB5_MAJORTYPE_BINARY_UNIF)
	return NULL;

    return rt_binunif_type_to_string(dp->d_minor_type);
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
