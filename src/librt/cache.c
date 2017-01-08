/*                         C A C H E . C
 * BRL-CAD
 *
 * Copyright (c) 2016 United States Government as represented by
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
/** @file cache.c
 *
 * Caching of prep data
 *
 */

#include "common.h"

/* interface header */
#include "./cache.h"

/* system headers */
#include <errno.h>

#define USE_NEW_COMPRESSION 1

#if USE_NEW_COMPRESSION
#include <lz4.h>
#else
#include <zlib.h>
#endif

#include "bnetwork.h"

/* implementation headers */
#include "bu/cv.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/uuid.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "rt/func.h"


static const char * const cache_mime_type = "brlcad/cache";


struct rt_cache {
    struct db_i *dbip;
};


HIDDEN void
cache_check(const struct rt_cache *cache)
{
    if (!cache)
	bu_bomb("NULL rt_cache pointer");

    RT_CK_DBI(cache->dbip);
}


HIDDEN void
cache_generate_name(char name[STATIC_ARRAY(37)], const struct soltab *stp)
{
    struct bu_external raw_external;
    struct db5_raw_internal raw_internal;
    uint8_t namespace_uuid[16];
    uint8_t uuid[16];

    RT_CK_SOLTAB(stp);

    {
	const uint8_t base_namespace_uuid[16] = {0x4a, 0x3e, 0x13, 0x3f, 0x1a, 0xfc, 0x4d, 0x6c, 0x9a, 0xdd, 0x82, 0x9b, 0x7b, 0xb6, 0xc6, 0xc1};
	char mat_buffer[SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_MAT];

	bu_cv_htond((unsigned char *)mat_buffer,
		    (unsigned char *)(stp->st_matp ? stp->st_matp : bn_mat_identity),
		    ELEMENTS_PER_MAT);

	if (5 != bu_uuid_create(namespace_uuid, sizeof(mat_buffer),
				(const uint8_t *)mat_buffer, base_namespace_uuid))
	    bu_bomb("bu_uuid_create() failed");
    }

    if (db_get_external(&raw_external, stp->st_dp, stp->st_rtip->rti_dbip))
	bu_bomb("db_get_external() failed");

    if (db5_get_raw_internal_ptr(&raw_internal, raw_external.ext_buf) == NULL)
	bu_bomb("rt_db_external5_to_internal5() failed");

    if (5 != bu_uuid_create(uuid, raw_internal.body.ext_nbytes,
			    raw_internal.body.ext_buf, namespace_uuid))
	bu_bomb("bu_uuid_create() failed");

    if (bu_uuid_encode(uuid, (uint8_t *)name))
	bu_bomb("bu_uuid_encode() failed");

    bu_free_external(&raw_external);
}


struct rt_cache *
rt_cache_open(void)
{
    struct bu_vls full_path = BU_VLS_INIT_ZERO;
    struct bu_vls tmp_path = BU_VLS_INIT_ZERO;
    const char * const file_name = "rt_cache.tmp";
    char *home_path;
    FILE *tmpfp;
    char temppath[MAXPATHLEN];
    int create;
    struct rt_cache *result;
    BU_GET(result, struct rt_cache);

    /* First, check HOME, tmp and the current working directory for
     * a pre-existing cache file */
    if ((home_path = getenv("HOME")) != (char *)NULL) {
	bu_vls_sprintf(&full_path, "%s/%s", home_path, file_name);
	if (bu_file_exists(bu_vls_addr(&full_path), NULL)) goto have_cache_file;
    }

    /* create a temp file in order to extract the tmp file working
     * directory - we have our own name, but need the path */
    tmpfp = bu_temp_file(temppath, MAXPATHLEN);
    if (tmpfp != NULL) {
	struct bu_vls dir = BU_VLS_INIT_ZERO;
	fclose(tmpfp);
	if (bu_path_component(&dir, temppath, BU_PATH_DIRNAME)) {
	    bu_vls_sprintf(&full_path, "%s/%s", bu_vls_addr(&dir), file_name);
	    bu_vls_sprintf(&tmp_path, "%s/%s", bu_vls_addr(&dir), file_name);
	    bu_vls_free(&dir);
	    if (bu_file_exists(bu_vls_addr(&full_path),NULL)) goto have_cache_file;
	}
    }

    /* As a last resort, check the current directory */
    bu_vls_sprintf(&full_path, "%s", file_name);
    if (bu_file_exists(bu_vls_addr(&full_path),NULL)) goto have_cache_file;

    /* If we got this far, we have nothing.  If we have HOME, use that, else
     * fall back on tmp, and as a last resort try current dir */
    if (home_path) {
	bu_vls_sprintf(&full_path, "%s/%s", home_path, file_name);
    } else if (bu_vls_strlen(&tmp_path) > 0) {
	bu_vls_sprintf(&full_path, "%s", bu_vls_addr(&tmp_path));
    } else {
	bu_vls_sprintf(&full_path, "%s", file_name);
    }

have_cache_file:
    create = !bu_file_exists(bu_vls_addr(&full_path), NULL);
    result->dbip = create ? db_create(bu_vls_addr(&full_path), 5) : db_open(bu_vls_addr(&full_path), DB_OPEN_READWRITE);
    bu_vls_free(&full_path);

    if (!result->dbip || (!create && db_dirbuild(result->dbip))) {
	db_close(result->dbip);
	BU_PUT(result, struct rt_cache);
	return NULL;
    }

    return result;
}


void
rt_cache_close(struct rt_cache *cache)
{
    cache_check(cache);

    db_close(cache->dbip);
    BU_PUT(cache, struct rt_cache);
}


HIDDEN void
compress_external(struct bu_external *external)
{
    int ret;
    int compressed = 0;
    uint8_t *buffer;
    size_t compressed_size;
    BU_CK_EXTERNAL(external);

#if USE_NEW_COMPRESSION
    compressed_size = LZ4_compressBound(external->ext_nbytes);
#else
    compressed_size = compressBound(external->ext_nbytes);
#endif
    /* buffer is uncompsize + maxcompsize + compressed_data */
    buffer = (uint8_t *)bu_malloc(compressed_size + SIZEOF_NETWORK_LONG, "buffer");

    *(uint32_t *)buffer = htonl(external->ext_nbytes);

#if USE_NEW_COMPRESSION
    ret = LZ4_compress_fast((const char *)external->ext_buf, (char *)(buffer + SIZEOF_NETWORK_LONG), external->ext_nbytes, compressed_size, 1);
    if (ret != 0)
	compressed = 1;
#else
    ret = compress(buffer + SIZEOF_NETWORK_LONG, &compressed_size, external->ext_buf, external->ext_nbytes);
    if (ret == Z_OK)
	compressed = 1;
#endif

    if (!compressed) {
	bu_log("compression failed (ret %d, %zu bytes @ %p to %zu bytes max)\n", ret, external->ext_nbytes, (void *) external->ext_buf, compressed_size);
	return;
    }

    *(uint32_t *)buffer = htonl(external->ext_nbytes);

    bu_free(external->ext_buf, "ext_buf");
    external->ext_nbytes = compressed_size + SIZEOF_NETWORK_LONG;
    external->ext_buf = buffer;
}


HIDDEN void
uncompress_external(const struct bu_external *external,
		    struct bu_external *dest)
{
    int ret;
    int uncompressed = 0;
    uint8_t *buffer;

    BU_CK_EXTERNAL(external);

    BU_EXTERNAL_INIT(dest);

    dest->ext_nbytes = ntohl(*(uint32_t *)external->ext_buf);
    buffer = (uint8_t *)bu_malloc(dest->ext_nbytes, "buffer");

#if USE_NEW_COMPRESSION
    ret = LZ4_decompress_fast((const char *)(external->ext_buf + SIZEOF_NETWORK_LONG), (char *)buffer, dest->ext_nbytes);
/*((const char *)external->ext_buf + SIZEOF_NETWORK_LONG, (char *)buffer, external->ext_nbytes - SIZEOF_NETWORK_LONG, dest->ext_nbytes); */
    if (ret > 0)
	uncompressed = 1;
#else
    ret = uncompress(buffer, &dest->ext_nbytes, external->ext_buf + SIZEOF_NETWORK_LONG, external->ext_nbytes - SIZEOF_NETWORK_LONG);
    if (Z_OK == ret)
	uncompressed = 1;
#endif

    if (!uncompressed) {
	bu_log("decompression failed (ret %d, %zu bytes @ %p to %zu bytes max)\n", ret, external->ext_nbytes, (void *) external->ext_buf, dest->ext_nbytes);
	return;
    }

    dest->ext_buf = buffer;
}


HIDDEN int
cache_try_load(const struct rt_cache *cache,
	       const struct directory *cache_dir, struct soltab *stp,
	       const struct rt_db_internal *internal)
{
    struct bu_external data_external = BU_EXTERNAL_INIT_ZERO;
    size_t version = (size_t)-1;

    cache_check(cache);
    RT_CK_DIR(cache_dir);
    RT_CK_SOLTAB(stp);

    if (stp->st_specific)
	bu_bomb("already prepped");

    if (cache_dir->d_major_type != DB5_MAJORTYPE_BINARY_MIME
	|| cache_dir->d_minor_type != 0)
	bu_bomb("invalid object type");

    {
	struct bu_external raw_external;
	struct db5_raw_internal raw_internal;

	if (db_get_external(&raw_external, cache_dir, cache->dbip))
	    bu_bomb("db_get_external() failed");

	if (db5_get_raw_internal_ptr(&raw_internal, raw_external.ext_buf) == NULL)
	    bu_bomb("rt_db_external5_to_internal5() failed");

	{
	    struct bu_attribute_value_set attributes;
	    const char *version_str;

	    if (0 > db5_import_attributes(&attributes, &raw_internal.attributes))
		bu_bomb("db5_import_attributes() failed");

	    if (bu_strcmp(cache_mime_type, bu_avs_get(&attributes, "mime_type")))
		bu_bomb("invalid MIME type");

	    if (!(version_str = bu_avs_get(&attributes, "rt_cache::version")))
		bu_bomb("missing version");

	    {
		const char *endptr;

		errno = 0;
		version = strtol(version_str, (char **)&endptr, 10);

		if ((version == 0 && errno) || endptr == version_str || *endptr)
		    bu_bomb("invalid version");
	    }

	    bu_avs_free(&attributes);
	}

	uncompress_external(&raw_internal.body, &data_external);
	bu_free_external(&raw_external);
    }

    if (rt_obj_prep_serialize(stp, internal, &data_external, &version)) {
	bu_free_external(&data_external);
	return 0;
    }

    bu_free_external(&data_external);
    return 1;
}


HIDDEN void
cache_try_store(struct rt_cache *cache, struct directory *cache_dir,
		struct soltab *stp, const struct rt_db_internal *internal)
{
    struct bu_external attributes_external;
    struct bu_external data_external = BU_EXTERNAL_INIT_ZERO;
    size_t version = (size_t)-1;

    cache_check(cache);
    RT_CK_DIR(cache_dir);
    RT_CK_SOLTAB(stp);

    if (!stp->st_specific)
	bu_bomb("not prepped");

    cache_dir->d_major_type = DB5_MAJORTYPE_BINARY_MIME;
    cache_dir->d_minor_type = 0;

    if (rt_obj_prep_serialize(stp, internal, &data_external, &version)
	|| version == (size_t)-1)
	bu_bomb("rt_obj_prep_serialize() failed");

    compress_external(&data_external);

    {
	struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
	struct bu_vls version_vls = BU_VLS_INIT_ZERO;

	bu_vls_sprintf(&version_vls, "%zu", version);
	bu_avs_add(&attributes, "mime_type", cache_mime_type);
	bu_avs_add(&attributes, "rt_cache::version", bu_vls_addr(&version_vls));
	db5_export_attributes(&attributes_external, &attributes);
	bu_vls_free(&version_vls);
	bu_avs_free(&attributes);
    }

    {
	struct bu_external db_external;
	db5_export_object3(&db_external, 0, cache_dir->d_namep, 0, &attributes_external,
			   &data_external, cache_dir->d_major_type, cache_dir->d_minor_type,
			   DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);

	if (db_put_external(&db_external, cache_dir, cache->dbip))
	    bu_bomb("db_put_external() failed");
    }

    bu_free_external(&attributes_external);
    bu_free_external(&data_external);
}


int
rt_cache_prep(struct rt_cache *cache, struct soltab *stp,
	      struct rt_db_internal *internal)
{
    char name[37];
    struct directory *dir;

    cache_check(cache);
    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(internal);

    cache_generate_name(name, stp);
    dir = db_lookup(cache->dbip, name, 0);

    if (dir) {
	if (!cache_try_load(cache, dir, stp, internal))
	    bu_bomb("cache_try_load() failed");
    } else {
	char type = 0;

	if (rt_obj_prep(stp, internal, stp->st_rtip))
	    return 1;

	if (OBJ[stp->st_id].ft_prep_serialize) {
	    if (!(dir = db_diradd(cache->dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_NON_GEOM,
				  (void *)&type)))
		bu_bomb("db_diradd() failed");

	    cache_try_store(cache, dir, stp, internal);
	}
    }

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
