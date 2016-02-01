/*                       D B 5 _ B I N . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2016 United States Government as represented by
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
/** @file db5_bin.c
 *
 * Handle bulk binary objects
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"
#include "bnetwork.h"


#include "bu/cv.h"
#include "bu/parse.h"
#include "vmath.h"
#include "rt/db5.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/nurb.h"


/* size of each element (in bytes) for the different BINUNIF types */
/* this array depends on the values of the definitions of the DB5_MINORTYPE_BINU_... in db5.h */
static const int binu_sizes[]={
    0,
    0,
    SIZEOF_NETWORK_FLOAT,
    SIZEOF_NETWORK_DOUBLE,
    1,
    2,
    4,
    8,
    0,
    0,
    0,
    0,
    1,
    2,
    4,
    8
};


/**
 * XXX these are the interface routines needed for table.c
 */
int
rt_bin_unif_export5(struct bu_external *UNUSED(ep),
		    const struct rt_db_internal *UNUSED(ip),
		    double UNUSED(local2mm),
		    const struct db_i *UNUSED(dbip),
		    struct resource *UNUSED(resp))
{
    bu_log("rt_bin_unif_export5() not implemented\n");
    return -1;
}

int
rt_bin_unif_import5(struct rt_db_internal *UNUSED(ip),
		    const struct bu_external *UNUSED(ep),
		    const mat_t UNUSED(mat),
		    const struct db_i *UNUSED(dbip),
		    struct resource *UNUSED(resp))
{
    bu_log("rt_bin_unif_import5() not implemented\n");
    return -1;
}

int
rt_bin_mime_import5(struct rt_db_internal * UNUSED(ip),
		    const struct bu_external *UNUSED(ep),
		    const mat_t UNUSED(mat),
		    const struct db_i *UNUSED(dbip),
		    struct resource *UNUSED(resp))
{
    bu_log("rt_bin_mime_import5() not implemented\n");
    return -1;
}

/**
 * Import a uniform-array binary object from the database format to
 * the internal structure.
 */
int
rt_binunif_import5_minor_type(struct rt_db_internal *ip,
			      const struct bu_external *ep,
			      const mat_t UNUSED(mat),
			      const struct db_i *dbip,
			      struct resource *resp,
			      int minor_type)
{
    struct rt_binunif_internal *bip;
    size_t i;
    unsigned char *srcp;
    unsigned long *ldestp;
    int in_cookie, out_cookie;
    size_t gotten;

    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);
    if (resp) RT_CK_RESOURCE(resp);

    /*
     * There's no particular size to expect
     *
     * BU_ASSERT_LONG(ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*4);
     */

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
    ip->idb_minor_type = minor_type;
    ip->idb_meth = &OBJ[ID_BINUNIF];
    BU_ALLOC(ip->idb_ptr, struct rt_binunif_internal);

    bip = (struct rt_binunif_internal *)ip->idb_ptr;
    bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
    bip->type = minor_type;

    /*
     * Convert from database (network) to internal (host) format
     */
    switch (bip->type) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    bip->count = ep->ext_nbytes/SIZEOF_NETWORK_FLOAT;
	    bip->u.uint8 = (unsigned char *) bu_malloc(bip->count * sizeof(float),
							"rt_binunif_internal");
	    bu_cv_ntohf((unsigned char *) bip->u.uint8,
		   ep->ext_buf, bip->count);
	    break;
	case DB5_MINORTYPE_BINU_DOUBLE:
	    bip->count = ep->ext_nbytes/SIZEOF_NETWORK_DOUBLE;
	    bip->u.uint8 = (unsigned char *) bu_malloc(bip->count * sizeof(double),
							"rt_binunif_internal");
	    bu_cv_ntohd((unsigned char *) bip->u.uint8, ep->ext_buf, bip->count);
	    break;
	case DB5_MINORTYPE_BINU_8BITINT:
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    bip->count = ep->ext_nbytes;
	    bip->u.uint8 = (unsigned char *) bu_malloc(ep->ext_nbytes,
							"rt_binunif_internal");
	    memcpy((char *) bip->u.uint8, (char *) ep->ext_buf, ep->ext_nbytes);
	    break;
	case DB5_MINORTYPE_BINU_16BITINT:
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    bip->count = ep->ext_nbytes/2;
	    bip->u.uint8 = (unsigned char *) bu_malloc(ep->ext_nbytes,
							"rt_binunif_internal");
	    in_cookie = bu_cv_cookie("nus");
	    out_cookie = bu_cv_cookie("hus");
	    if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
		gotten =
		    bu_cv_w_cookie((void *)bip->u.uint8, out_cookie,
				   ep->ext_nbytes,
				   ep->ext_buf, in_cookie, bip->count);
		if (gotten != bip->count) {
		    bu_log("%s:%d: Tried to convert %zu, did %zu",
			   __FILE__, __LINE__, bip->count, gotten);
		    bu_bomb("\n");
		}
	    } else
		memcpy((char *) bip->u.uint8,
		       (char *) ep->ext_buf,
		       ep->ext_nbytes);
	    break;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    bip->count = ep->ext_nbytes/4;
	    bip->u.uint8 = (unsigned char *) bu_malloc(ep->ext_nbytes,
							"rt_binunif_internal");
	    srcp = (unsigned char *) ep->ext_buf;
	    ldestp = (unsigned long *) bip->u.uint8;
	    for (i = 0; i < bip->count; ++i, ++ldestp, srcp += 4) {
		*ldestp = ntohl(*(uint32_t *)&srcp[0]);
	    }
	    break;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    bu_log("rt_binunif_import5_minor_type() Can't handle 64-bit integers yet\n");
	    return -1;
    }

    return 0;		/* OK */
}


void
rt_binunif_dump(struct rt_binunif_internal *bip) {
    RT_CK_BINUNIF(bip);
    bu_log("rt_bin_unif_internal <%p>...\n", (void *)bip);
    bu_log("  type = x%x = %d", bip->type, bip->type);
    bu_log("  count = %ld  first = 0x%02x", bip->count,
	   bip->u.uint8[0] & 0x0ff);
    bu_log("- - - - -\n");
}


/**
 * Create the "body" portion of external form
 */
int
rt_binunif_export5(struct bu_external		*ep,
		    const struct rt_db_internal	*ip,
		    double			UNUSED(local2mm), /* we ignore */
		    const struct db_i		*dbip,
		    struct resource		*resp)
{
    struct rt_binunif_internal	*bip;
    size_t			i;
    unsigned char		*destp;
    unsigned long		*lsrcp;
    int				in_cookie, out_cookie;
    size_t			gotten;

    if (dbip) RT_CK_DBI(dbip);
    if (resp) RT_CK_RESOURCE(resp);

    RT_CK_DB_INTERNAL(ip);
    bip = (struct rt_binunif_internal *)ip->idb_ptr;
    RT_CK_BINUNIF(bip);

    BU_EXTERNAL_INIT(ep);

    /*
     * Convert from internal (host) to database (network) format
     */
    switch (bip->type) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    ep->ext_nbytes = bip->count * SIZEOF_NETWORK_FLOAT;
	    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes,
					      "binunif external");
	    bu_cv_htonf(ep->ext_buf, (unsigned char *) bip->u.uint8, bip->count);
	    break;
	case DB5_MINORTYPE_BINU_DOUBLE:
	    ep->ext_nbytes = bip->count * SIZEOF_NETWORK_DOUBLE;
	    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes,
					      "binunif external");
	    bu_cv_htond(ep->ext_buf, (unsigned char *)bip->u.uint8, bip->count);
	    break;
	case DB5_MINORTYPE_BINU_8BITINT:
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    ep->ext_nbytes = bip->count;
	    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes,
					      "binunif external");
	    memcpy((char *)ep->ext_buf, (char *)bip->u.uint8, bip->count);
	    break;
	case DB5_MINORTYPE_BINU_16BITINT:
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    ep->ext_nbytes = bip->count * 2;
	    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "binunif external");
	    in_cookie = bu_cv_cookie("hus");
	    out_cookie = bu_cv_cookie("nus");
	    if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
		gotten =
		    bu_cv_w_cookie(ep->ext_buf, out_cookie,
				   ep->ext_nbytes,
				   (void *) bip->u.uint8, in_cookie,
				   bip->count);

		if (gotten != bip->count) {
		    bu_log("%s:%d: Tried to convert %zu, did %zu",
			   __FILE__, __LINE__, bip->count, gotten);
		    bu_bomb("\n");
		}
	    } else {
		memcpy((char *) ep->ext_buf,
		       (char *) bip->u.uint8,
		       ep->ext_nbytes);
	    }
	    break;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    ep->ext_nbytes = bip->count * 4;
	    ep->ext_buf = (uint8_t *)bu_malloc(ep->ext_nbytes, "binunif external");

	    lsrcp = (unsigned long *) bip->u.uint8;
	    destp = (unsigned char *) ep->ext_buf;
	    for (i = 0; i < bip->count; ++i, ++destp, ++lsrcp) {
		*(uint32_t *)&destp[0] = htonl(*lsrcp);
	    }
	    break;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    bu_log("rt_binunif_export5() Can't handle 64-bit integers yet\n");
	    return -1;
    }

    return 0;
}

/**
 * Make human-readable formatted presentation of this object.  First
 * line describes type of object.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_binunif_describe(struct bu_vls *str,
		     const struct rt_db_internal *ip,
		     int UNUSED(verbose),
		     double UNUSED(mm2local))
{
    register struct rt_binunif_internal	*bip;
    char					buf[256];
    unsigned short				wid;

    bip = (struct rt_binunif_internal *) ip->idb_ptr;
    RT_CK_BINUNIF(bip);
    bu_vls_strcat(str, "uniform-array binary object (BINUNIF)\n");
    wid = (bip->type & DB5_MINORTYPE_BINU_WID_MASK) >> 4;
    switch (wid) {
	case 0:
	    sprintf(buf, "%llu ", (unsigned long long)bip->count); break;
	case 1:
	    sprintf(buf, "%llu pairs of ", (unsigned long long)(bip->count / 2)); break;
	case 2:
	    sprintf(buf, "%llu triples of ", (unsigned long long)(bip->count / 3)); break;
	case 3:
	    sprintf(buf, "%llu quadruples of ", (unsigned long long)(bip->count / 4)); break;
    }
    bu_vls_strcat(str, buf);
    switch (bip->type & DB5_MINORTYPE_BINU_ATM_MASK) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    bu_vls_strcat(str, "floats\n"); break;
	case DB5_MINORTYPE_BINU_DOUBLE:
	    bu_vls_strcat(str, "doubles\n"); break;
	case DB5_MINORTYPE_BINU_8BITINT:
	    bu_vls_strcat(str, "8-bit ints\n"); break;
	case DB5_MINORTYPE_BINU_16BITINT:
	    bu_vls_strcat(str, "16-bit ints\n"); break;
	case DB5_MINORTYPE_BINU_32BITINT:
	    bu_vls_strcat(str, "32-bit ints\n"); break;
	case DB5_MINORTYPE_BINU_64BITINT:
	    bu_vls_strcat(str, "64-bit ints\n"); break;
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    bu_vls_strcat(str, "unsigned 8-bit ints\n"); break;
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    bu_vls_strcat(str, "unsigned 16-bit ints\n"); break;
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    bu_vls_strcat(str, "unsigned 32-bit ints\n"); break;
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    bu_vls_strcat(str, "unsigned 64-bit ints\n"); break;
	default:
	    bu_log("%s:%d: This shouldn't happen", __FILE__, __LINE__);
	    return 1;
    }

    return 0;
}

void
rt_binunif_free(struct rt_binunif_internal *bip) {
    RT_CK_BINUNIF(bip);
    bu_free((void *) bip->u.uint8, "binunif free uint8");
    bu_free(bip, "binunif free");
    bip = NULL; /* sanity */
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * thing.
 */
void
rt_binunif_ifree(struct rt_db_internal *ip)
{
    struct rt_binunif_internal	*bip;

    RT_CK_DB_INTERNAL(ip);
    bip = (struct rt_binunif_internal *)ip->idb_ptr;
    RT_CK_BINUNIF(bip);
    bu_free((void *) bip->u.uint8, "binunif ifree");
    bu_free(ip->idb_ptr, "binunif ifree");
    ip->idb_ptr = ((void *)0);
}


int
rt_retrieve_binunif(struct rt_db_internal *intern,
		    struct db_i	*dbip,
		    char *name)
{
    register struct directory	*dp;
    struct rt_binunif_internal	*bip;
    struct bu_external		ext;
    struct db5_raw_internal		raw;
    char				*tmp;

    /*
     *Find the guy we're told to write
     */
    if ((dp = db_lookup(dbip, name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return -1;

    RT_DB_INTERNAL_INIT(intern);
    if (rt_db_get_internal5(intern, dp, dbip, NULL, &rt_uniresource)
	 != ID_BINUNIF     || db_get_external(&ext, dp, dbip) < 0)
	return -1;

    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	bu_log("%s:%d\n", __FILE__, __LINE__);
	bu_free_external(&ext);
	return -1;
    }
    if (db5_type_descrip_from_codes(&tmp, raw.major_type, raw.minor_type))
	tmp = 0;

    if (RT_G_DEBUG & DEBUG_VOL)
	bu_log("get_body() sees type (%d, %d)='%s'\n",
	       raw.major_type, raw.minor_type, tmp);

    if (raw.major_type != DB5_MAJORTYPE_BINARY_UNIF)
	return -1;

    bip = (struct rt_binunif_internal *)intern->idb_ptr;
    RT_CK_BINUNIF(bip);
    if (RT_G_DEBUG & DEBUG_HF)
	rt_binunif_dump(bip);

    if (RT_G_DEBUG & DEBUG_VOL)
	bu_log("cmd_export_body() thinks bip->count=%zu\n",
	       bip->count);

    switch (bip->type) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... float");
	    break;
	case DB5_MINORTYPE_BINU_DOUBLE:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... double");
	    break;
	case DB5_MINORTYPE_BINU_8BITINT:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... 8bitint");
	    break;
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... 8bituint");
	    break;
	case DB5_MINORTYPE_BINU_16BITINT:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... 16bituint");
	    break;
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... 16bitint");
	    break;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... 32bitint");
	    break;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("bip->type switch... 64bitint");
	    break;
	default:
	    /* XXX	This shouldn't happen!!    */
	    bu_log("bip->type switch... default");
	    break;
    }

    bu_free_external(&ext);

    return 0;
}

void
rt_binunif_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_binunif_internal *bip;

    intern->idb_type = DB5_MINORTYPE_BINU_8BITINT;
    intern->idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
    BU_ASSERT(&OBJ[ID_BINUNIF] == ftp);

    intern->idb_meth = ftp;
    BU_ALLOC(bip, struct rt_binunif_internal);

    intern->idb_ptr = (void *) bip;
    bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
    bip->type = DB5_MINORTYPE_BINU_8BITINT;
    bip->count = 0;
    bip->u.int8 = NULL;
}

int
rt_binunif_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    register struct rt_binunif_internal *bip=(struct rt_binunif_internal *)intern->idb_ptr;
    struct bu_external	ext;
    size_t		i;
    unsigned char	*c;

    RT_CHECK_BINUNIF(bip);

    if (attr == (char *)NULL) {
	/* export the object to get machine independent form */
	if (rt_binunif_export5(&ext, intern, 1.0, NULL, NULL)) {
	    bu_vls_strcpy(logstr, "Failed to export binary object!!\n");
	    return BRLCAD_ERROR;
	} else {
	    bu_vls_strcpy(logstr, "binunif");
	    bu_vls_printf(logstr, " T %d D {", bip->type);
	    c = ext.ext_buf;
	    for (i = 0; i < ext.ext_nbytes; i++, c++) {
		if (i%40 == 0) bu_vls_strcat(logstr, "\n");
		bu_vls_printf(logstr, "%2.2x", *c);
	    }
	    bu_vls_strcat(logstr, "}");
	    bu_free_external(&ext);
	}

    } else {
	if (BU_STR_EQUAL(attr, "T")) {
	    bu_vls_printf(logstr, "%d", bip->type);
	} else if (BU_STR_EQUAL(attr, "D")) {
	    /* export the object to get machine independent form */
	    if (rt_binunif_export5(&ext, intern, 1.0, NULL, NULL)) {
		bu_vls_strcpy(logstr, "Failed to export binary object!!\n");
		return BRLCAD_ERROR;
	    } else {
		c = ext.ext_buf;
		for (i = 0; i < ext.ext_nbytes; i++, c++) {
		    if (i != 0 && i%40 == 0) bu_vls_strcat(logstr, "\n");
		    bu_vls_printf(logstr, "%2.2x", *c);
		}
		bu_free_external(&ext);
	    }
	} else {
	    bu_vls_printf(logstr, "Binary object has no attribute '%s'", attr);
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}

int
rt_binunif_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, char **argv)
{
    struct rt_binunif_internal *bip;
    size_t i;

    RT_CK_DB_INTERNAL(intern);
    bip = (struct rt_binunif_internal *)intern->idb_ptr;
    RT_CHECK_BINUNIF(bip);

    while (argc >= 2) {
	if (BU_STR_EQUAL(argv[0], "T")) {
	    int new_type=-1;
	    char *c;
	    int type_is_digit=1;

	    c = argv[1];
	    while (*c != '\0') {
		if (!isdigit((int)*c)) {
		    type_is_digit = 0;
		    break;
		}
		c++;
	    }

	    if (type_is_digit) {
		new_type = atoi(argv[1]);
	    } else {
		if (argv[1][1] != '\0') {
		    bu_vls_printf(logstr,
				  "Illegal type: %s, must be 'f', 'd', 'c', 'i', 'l', 'C', 'S', 'I', or 'L'",
				  argv[1]);
		    return BRLCAD_ERROR;
		}
		switch (argv[1][0]) {
		    case 'f':
			new_type = 2;
			break;
		    case 'd':
			new_type = 3;
			break;
		    case 'c':
			new_type = 4;
			break;
		    case 's':
			new_type = 5;
			break;
		    case 'i':
			new_type = 6;
			break;
		    case 'l':
			new_type = 7;
			break;
		    case 'C':
			new_type = 12;
			break;
		    case 'S':
			new_type = 13;
			break;
		    case 'I':
			new_type = 14;
			break;
		    case 'L':
			new_type = 15;
			break;
		}
	    }
	    if (new_type < 0 ||
		 new_type > DB5_MINORTYPE_BINU_64BITINT ||
		 binu_types[new_type] == NULL) {
		/* Illegal value for type */
		bu_vls_printf(logstr, "Illegal value for binary type: %s", argv[1]);
		return BRLCAD_ERROR;
	    } else {
		if (bip->u.uint8) {
		    size_t new_count;
		    size_t old_byte_count, new_byte_count;
		    size_t remainder_count;

		    old_byte_count = bip->count * binu_sizes[bip->type];
		    new_count = old_byte_count / binu_sizes[new_type];
		    remainder_count = old_byte_count % binu_sizes[new_type];
		    if (remainder_count) {
			new_count++;
			new_byte_count = new_count * binu_sizes[new_type];
		    } else {
			new_byte_count = old_byte_count;
		    }

		    if (new_byte_count != old_byte_count) {
			bip->u.uint8 = (unsigned char *)bu_realloc(bip->u.uint8,
								  new_byte_count,
								  "new bytes for binunif");
			/* zero out the new bytes */
			for (i = old_byte_count; i < new_byte_count; i++) {
			    bip->u.uint8[i] = 0;
			}
		    }
		    bip->count = new_count;
		}
		bip->type = new_type;
		intern->idb_type = new_type;
	    }
	} else if (BU_STR_EQUAL(argv[0], "D")) {
	    int list_len;
	    const char **obj_array;
	    unsigned char *buf, *d;
	    const char *s;
	    int hexlen;
	    unsigned int h;

	    /* split initial list */
	    if (bu_argv_from_tcl_list(argv[1], &list_len, (const char ***)&obj_array) != 0) {
		return -1;
	    }

	    hexlen = 0;
	    for (i = 0; i < (size_t)list_len; i++) {
		hexlen += strlen(obj_array[i]);
	    }

	    if (hexlen % 2) {
		bu_vls_printf(logstr, "Hex form of binary data must have an even number of hex digits");
		bu_free((char *)obj_array, "obj array");
		return BRLCAD_ERROR;
	    }

	    buf = (unsigned char *)bu_malloc(hexlen / 2, "tcladjust binary data");
	    d = buf;
	    for (i = 0; i < (size_t)list_len; i++) {
		s = obj_array[i];
		while (*s) {
		    sscanf(s, "%2x", &h);
		    *d++ = h;
		    s += 2;
		}
	    }

	    if (bip->u.uint8) {
		bu_free(bip->u.uint8, "binary data");
	    }
	    bip->u.uint8 = buf;
	    bip->count = hexlen / 2 / binu_sizes[bip->type];

	    bu_free((char *)obj_array, "obj array");
	}

	argc -= 2;
	argv += 2;
    }

    return BRLCAD_OK;
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
