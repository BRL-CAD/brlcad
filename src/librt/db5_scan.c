/*                      D B 5 _ S C A N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
#include "bio.h"


#include "bu/parse.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"


int
db5_scan(
    struct db_i *dbip,
    void (*handler)(struct db_i *,
		    const struct db5_raw_internal *,
		    off_t addr, void *client_data),
    void *client_data)
{
    unsigned char header[8];
    struct db5_raw_internal raw;
    int got;
    size_t nrec;
    off_t addr;

    RT_CK_DBI(dbip);
    if (RT_G_DEBUG&DEBUG_DB) bu_log("db5_scan(%p, %lx)\n",
				    (void *)dbip, (long unsigned int)handler);

    raw.magic = DB5_RAW_INTERNAL_MAGIC;
    nrec = 0L;

    /* Fast-path when file is already memory-mapped */
    if (dbip->dbi_mf) {
	const unsigned char *cp = (const unsigned char *)dbip->dbi_inmem;
	off_t eof;

	BU_CK_MAPPED_FILE(dbip->dbi_mf);
	eof = (off_t)dbip->dbi_mf->buflen;

	if (db5_header_is_valid(cp) == 0) {
	    bu_log("db5_scan ERROR:  %s is lacking a proper BRL-CAD v5 database header\n", dbip->dbi_filename);
	    goto fatal;
	}
	cp += sizeof(header);
	addr = (off_t)sizeof(header);
	while (addr < eof) {
	    if ((cp = db5_get_raw_internal_ptr(&raw, cp)) == NULL) {
		goto fatal;
	    }
	    (*handler)(dbip, &raw, addr, client_data);
	    nrec++;
	    addr += (off_t)raw.object_length;
	}
	dbip->dbi_eof = addr;
	BU_ASSERT_LONG(dbip->dbi_eof, ==, (off_t)dbip->dbi_mf->buflen);
    } else {
	/* In a totally portable way, read the database with stdio */
	rewind(dbip->dbi_fp);
	if (fread(header, sizeof header, 1, dbip->dbi_fp) != 1  ||
	    db5_header_is_valid(header) == 0) {
	    bu_log("db5_scan ERROR:  %s is lacking a proper BRL-CAD v5 database header\n", dbip->dbi_filename);
	    goto fatal;
	}
	for (;;) {
	    addr = bu_ftell(dbip->dbi_fp);
	    if ((got = db5_get_raw_internal_fp(&raw, dbip->dbi_fp)) < 0) {
		if (got == -1) break;		/* EOF */
		goto fatal;
	    }
	    (*handler)(dbip, &raw, addr, client_data);
	    nrec++;
	    if (raw.buf) {
		bu_free(raw.buf, "raw v5 object");
		raw.buf = NULL;
	    }
	}
	dbip->dbi_eof = bu_ftell(dbip->dbi_fp);
	rewind(dbip->dbi_fp);
    }

    dbip->dbi_nrec = nrec;		/* # obj in db, not inc. header */
    return 0;			/* success */

fatal:
    dbip->dbi_read_only = 1;	/* Writing could corrupt it worse */
    return -1;			/* fatal error */
}


struct directory *
db_diradd5(
    struct db_i *dbip,
    const char *name,
    off_t laddr,
    unsigned char major_type,
    unsigned char minor_type,
    unsigned char name_hidden,
    size_t object_length,
    struct bu_attribute_value_set *avs)
{
    struct directory **headp;
    register struct directory *dp;
    struct bu_vls local = BU_VLS_INIT_ZERO;

    RT_CK_DBI(dbip);

    bu_vls_strcpy(&local, name);
    if (db_dircheck(dbip, &local, 0, &headp) < 0) {
	bu_vls_free(&local);
	return RT_DIR_NULL;
    }

    if (rt_uniresource.re_magic == 0)
	rt_init_resource(&rt_uniresource, 0, NULL);

    /* Duplicates the guts of db_diradd() */
    RT_GET_DIRECTORY(dp, &rt_uniresource);
    RT_CK_DIR(dp);
    BU_LIST_INIT(&dp->d_use_hd);
    RT_DIR_SET_NAMEP(dp, bu_vls_addr(&local));	/* sets d_namep */
    bu_vls_free(&local);
    dp->d_un.ptr = NULL;
    dp->d_addr = laddr;
    dp->d_major_type = major_type;
    dp->d_minor_type = minor_type;
    switch (major_type) {
	case DB5_MAJORTYPE_BRLCAD:
	    if (minor_type == ID_COMBINATION) {

		dp->d_flags = RT_DIR_COMB;
		if (!avs || avs->count == 0) break;
		/*
		 * check for the "region=" attribute.
		 */
		if (bu_avs_get(avs, "region") != NULL)
		    dp->d_flags = RT_DIR_COMB|RT_DIR_REGION;
	    } else {
		dp->d_flags = RT_DIR_SOLID;
	    }
	    break;
	case DB5_MAJORTYPE_BINARY_UNIF:
	case DB5_MAJORTYPE_BINARY_MIME:
	    /* XXX Do we want to define extra flags for this? */
	    dp->d_flags = RT_DIR_NON_GEOM;
	    break;
	case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
	    dp->d_flags = 0;
    }
    if (name_hidden)
	dp->d_flags |= RT_DIR_HIDDEN;
    dp->d_len = object_length;		/* in bytes */
    BU_LIST_INIT(&dp->d_use_hd);
    dp->d_animate = NULL;
    dp->d_nref = 0;
    dp->d_uses = 0;
    dp->d_forw = *headp;
    *headp = dp;

    return dp;
}


struct directory *
db5_diradd(struct db_i *dbip,
	   const struct db5_raw_internal *rip,
	   off_t laddr,
	   void *client_data)
{
    struct directory **headp;
    register struct directory *dp;
    struct bu_vls local = BU_VLS_INIT_ZERO;

    RT_CK_DBI(dbip);

    if (client_data && RT_G_DEBUG&DEBUG_DB) {
	bu_log("WARNING: db5_diradd() received non-NULL client_data\n");
    }

    bu_vls_strcpy(&local, (const char *)rip->name.ext_buf);
    if (db_dircheck(dbip, &local, 0, &headp) < 0) {
	bu_vls_free(&local);
	return RT_DIR_NULL;
    }

    if (rt_uniresource.re_magic == 0)
	rt_init_resource(&rt_uniresource, 0, NULL);

    /* Duplicates the guts of db_diradd() */
    RT_GET_DIRECTORY(dp, &rt_uniresource); /* allocates a new dir */
    RT_CK_DIR(dp);
    BU_LIST_INIT(&dp->d_use_hd);
    RT_DIR_SET_NAMEP(dp, bu_vls_addr(&local));	/* sets d_namep */
    bu_vls_free(&local);
    dp->d_addr = laddr;
    dp->d_major_type = rip->major_type;
    dp->d_minor_type = rip->minor_type;
    switch (rip->major_type) {
	case DB5_MAJORTYPE_BRLCAD:
	    if (rip->minor_type == ID_COMBINATION) {
		struct bu_attribute_value_set avs;

		bu_avs_init_empty(&avs);

		dp->d_flags = RT_DIR_COMB;
		if (rip->attributes.ext_nbytes == 0) break;
		/*
		 * Crack open the attributes to
		 * check for the "region=" attribute.
		 */
		if (db5_import_attributes(&avs, &rip->attributes) < 0) {
		    bu_log("db5_diradd_handler: Bad attributes on combination '%s'\n",
			   rip->name.ext_buf);
		    break;
		}
		if (bu_avs_get(&avs, "region") != NULL)
		    dp->d_flags = RT_DIR_COMB|RT_DIR_REGION;
		bu_avs_free(&avs);
	    } else {
		dp->d_flags = RT_DIR_SOLID;
	    }
	    break;
	case DB5_MAJORTYPE_BINARY_UNIF:
	case DB5_MAJORTYPE_BINARY_MIME:
	    /* XXX Do we want to define extra flags for this? */
	    dp->d_flags = RT_DIR_NON_GEOM;
	    break;
	case DB5_MAJORTYPE_ATTRIBUTE_ONLY:
	    dp->d_flags = 0;
    }
    if (rip->h_name_hidden)
	dp->d_flags |= RT_DIR_HIDDEN;
    dp->d_len = rip->object_length;		/* in bytes */
    BU_LIST_INIT(&dp->d_use_hd);
    dp->d_animate = NULL;
    dp->d_nref = 0;
    dp->d_uses = 0;
    dp->d_forw = *headp;
    *headp = dp;

    return dp;
}


/*
 * In support of db5_scan, add a named entry to the directory.
 */
HIDDEN void
db5_diradd_handler(
    struct db_i *dbip,
    const struct db5_raw_internal *rip,
    off_t laddr,
    void *client_data)	/* unused client_data from db5_scan() */
{
    RT_CK_DBI(dbip);

    if (rip->h_dli == DB5HDR_HFLAGS_DLI_HEADER_OBJECT) return;
    if (rip->h_dli == DB5HDR_HFLAGS_DLI_FREE_STORAGE) {
	/* Record available free storage */
	rt_memfree(&(dbip->dbi_freep), rip->object_length, laddr);
	return;
    }

    /* If somehow it doesn't have a name, ignore it */
    if (rip->name.ext_buf == NULL) return;

    if (RT_G_DEBUG&DEBUG_DB) {
	bu_log("db5_diradd_handler(dbip=%p, name='%s', addr=%ld, len=%zu)\n",
	       (void *)dbip, rip->name.ext_buf, laddr, rip->object_length);
    }

    db5_diradd(dbip, rip, laddr, client_data);

    return;
}

int
db_dirbuild(struct db_i *dbip)
{
    int version;

    if (!dbip) {
	return -1;
    }

    RT_CK_DBI(dbip);

    if (!dbip->dbi_fp) {
	return -1;
    }

    /* First, determine what version database this is */
    version = db_version(dbip);

    if (version == 5) {
	struct directory *dp;
	struct bu_external ext;
	struct db5_raw_internal raw;
	struct bu_attribute_value_set avs;
	const char *cp;

	bu_avs_init_empty(&avs);

	/* File is v5 format */
	if (db5_scan(dbip, db5_diradd_handler, NULL) < 0) {
	    bu_log("db_dirbuild(%s): db5_scan() failed\n", dbip->dbi_filename);
	    return -1;
	}

	/* Need to retrieve _GLOBAL object and obtain title and units */
	if ((dp = db_lookup(dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_log("db_dirbuild(%s): improper database, no %s object\n",
		   dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME);
	    dbip->dbi_title = bu_strdup(DB5_GLOBAL_OBJECT_NAME);
	    /* Missing _GLOBAL object so create it and set default title and units */
	    db5_update_ident(dbip, "Untitled BRL-CAD Database", 1.0);
	    return 0;	/* not a fatal error, user may have deleted it */
	}
	BU_EXTERNAL_INIT(&ext);
	if (db_get_external(&ext, dp, dbip) < 0 ||
	    db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	    bu_log("db_dirbuild(%s): improper database, unable to read %s object\n",
		   dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME);
	    return -1;
	}
	if (raw.major_type != DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	    bu_log("db_dirbuild(%s): improper database, %s exists but is not an attribute-only object\n",
		   dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME);
	    dbip->dbi_title = bu_strdup(DB5_GLOBAL_OBJECT_NAME);
	    return 0;	/* not a fatal error, need to let user proceed to fix it */
	}

	/* Parse out the attributes */

	if (db5_import_attributes(&avs, &raw.attributes) < 0) {
	    bu_log("db_dirbuild(%s): improper database, corrupted attribute-only %s object\n",
		   dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME);
	    bu_free_external(&ext);
	    return -1;	/* this is fatal */
	}
	BU_CK_AVS(&avs);

	/* 1/3: title */
	if ((cp = bu_avs_get(&avs, "title")) != NULL) {
	    dbip->dbi_title = bu_strdup(cp);
	} else {
	    dbip->dbi_title = bu_strdup("Untitled BRL-CAD database");
	}

	/* 2/3: units */
	if ((cp = bu_avs_get(&avs, "units")) != NULL) {
	    double dd;
	    if (sscanf(cp, "%lf", &dd) != 1 || NEAR_ZERO(dd, VUNITIZE_TOL)) {
		bu_log("db_dirbuild(%s): improper database, %s object attribute 'units'=%s is invalid\n",
		       dbip->dbi_filename, DB5_GLOBAL_OBJECT_NAME, cp);
		/* Not fatal, just stick with default value from db_open() */
	    } else {
		dbip->dbi_local2base = dd;
		dbip->dbi_base2local = 1/dd;
	    }
	}

	/* 3/3: color table */
	if ((cp = bu_avs_get(&avs, "regionid_colortable")) != NULL) {
	    /* Import the region-id coloring table */
	    db5_import_color_table((char *)cp);
	}
	bu_avs_free(&avs);
	bu_free_external(&ext);	/* not until after done with avs! */

	return 0;		/* ok */

    } else if (version == 4) {
	/* things used to be pretty simple with v4 */

	if (db_scan(dbip, (int (*)(struct db_i *, const char *, off_t, size_t, int, void *))db_diradd, 1, NULL) < 0) {
	    return -1;
	}

	return 0;		/* ok */
    }

    bu_log("ERROR: Cannot build object directory.\n\tFile '%s' does not seem to be in BRL-CAD geometry database format.\n",
	   dbip->dbi_filename);

    return -1;
}


int
db_version(struct db_i *dbip)
{
    unsigned char header[8];

    if (!dbip) {
	return -1;
    }

    RT_CK_DBI(dbip);

    /* already calculated during db_open? */
    if (dbip->dbi_version != 0)
	return abs(dbip->dbi_version);

    if (!dbip->dbi_fp) {
	return -1;
    }

    rewind(dbip->dbi_fp);
    if (fread(header, sizeof(header), 1, dbip->dbi_fp) != 1) {
	bu_log("ERROR: file (%s) too short to be a BRL-CAD database\n", dbip->dbi_filename ? dbip->dbi_filename : "unknown");
	return -1;
    }

    if (db5_header_is_valid(header)) {
	return 5;
    } else if (header[0] == 'I') {
	return 4;
    }

    return -1;
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
