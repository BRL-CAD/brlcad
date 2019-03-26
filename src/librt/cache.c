/*                         C A C H E . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2019 United States Government as represented by
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
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h> /* for mkdir */
#endif
#include <lz4.h>

#include "bnetwork.h"

/* implementation headers */
#include "bu/app.h"
#include "bu/file.h"
#include "bu/cv.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/time.h"
#include "bu/str.h"
#include "bu/uuid.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "rt/func.h"


#define CACHE_FORMAT 2

static const char * const cache_mime_type = "brlcad/cache";


#define CACHE_LOG(...) if (cache && cache->log) cache->log(__VA_ARGS__);
#define CACHE_DEBUG(...) if (cache && cache->debug) cache->debug(__VA_ARGS__);


struct rt_cache {
    char dir[MAXPATHLEN];
    int read_only;
    int (*log)(const char *format, ...);
    int (*debug)(const char *format, ...);
    struct bu_hash_tbl *dbip_hash;
};
#define CACHE_INIT {{0}, 0, bu_log, NULL, NULL}


HIDDEN void
cache_get_objdir(const struct rt_cache *cache, const char *filename, char *buffer, size_t len)
{
    char idx[3] = {0};
    idx[0] = filename[0];
    idx[1] = filename[1];
    bu_dir(buffer, len, cache->dir, "objects", idx, NULL);
}


HIDDEN void
cache_get_objfile(const struct rt_cache *cache, const char *filename, char *buffer, size_t len)
{
    char dir[MAXPATHLEN] = {0};
    cache_get_objdir(cache, filename, dir, MAXPATHLEN);
    bu_dir(buffer, len, dir, filename, NULL);
}


/* returns truthfully if a path exists, creating it if necessary. */
HIDDEN int
cache_ensure_path(const char *path, int is_file)
{
    char *dir;

    if (bu_file_exists(path, NULL)) {
	int is_dir = bu_file_directory(path);
	if (is_file && !is_dir)
	    return 1;
	if (!is_file && is_dir)
	    return 1;
	/* something in the way! */
	return 0;
    }

    if (!path /* empty or root path (recursive base case) */
	|| strlen(path) == 0
	|| (strlen(path) == 1 && path[0] == BU_DIR_SEPARATOR)
	|| (strlen(path) == 1 && path[0] == '/')
	|| (strlen(path) == 3 && isalpha(path[0]) && path[1] == ':' && path[2] == BU_DIR_SEPARATOR)
	|| (strlen(path) == 3 && isalpha(path[0]) && path[1] == ':' && path[2] == '/'))
    {
	return 1;
    }
    dir = bu_path_dirname(path);
    if (!cache_ensure_path(dir, 0)) {
	bu_free(dir, "dirname");
	return 0;
    }
    bu_free(dir, "dirname");

    if (!is_file && !bu_file_directory(path)) {
#ifdef HAVE_WINDOWS_H
	CreateDirectory(path, NULL);
#else
	/* mode: 775 */
	mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
    } else if (is_file && !bu_file_exists(path, NULL)) {
	/* touch file */
	FILE *fp = fopen(path, "w");
	if (!fp) {
	    perror("fopen");
	} else {
	    fclose(fp);
	}
    }
    return bu_file_exists(path, NULL);
}


HIDDEN void
cache_warn(const struct rt_cache *cache, const char *path, const char *msg)
{
    CACHE_LOG("  CACHE  %s\n", path);
    CACHE_LOG("WARNING: ^ %s\n", msg);
}


HIDDEN int
cache_format(const struct rt_cache *cache)
{
    const char *librt_cache = cache->dir;
    const char *cpath = NULL;

    int format = CACHE_FORMAT;
    struct bu_vls path = BU_VLS_INIT_ZERO;
    struct bu_vls fmt_str = BU_VLS_INIT_ZERO;
    size_t ret;
    FILE *fp;

    bu_vls_printf(&path, "%s%c%s", librt_cache, BU_DIR_SEPARATOR, "format");
    cpath = bu_vls_cstr(&path);
    if (!bu_file_readable(cpath)) {
	cache_warn(cache, cpath, "Cannot read format file.  Caching disabled.");
	bu_vls_free(&path);
	return -1;
    }

    fp = fopen(cpath, "r");
    if (!fp) {
	perror("fopen");
	cache_warn(cache, cpath, "Cannot open format file.  Caching disabled.");
	bu_vls_free(&path);
	return -2;
    }

    bu_vls_gets(&fmt_str, fp);
    fclose(fp);

    if (!bu_vls_strlen(&fmt_str)) {
	cache_warn(cache, cpath, "Cannot read format file.  Caching disabled.");
	return -3;
    }

    ret = bu_sscanf(bu_vls_cstr(&fmt_str), "%d", &format);
    if (ret != 1) {
	cache_warn(cache, cpath, "Cannot get version from format file.  Caching disabled.");
	return -4;
    }
    bu_vls_free(&fmt_str);
    bu_vls_free(&path);

    return format;
}


HIDDEN int
cache_generate_name(char name[STATIC_ARRAY(37)], const struct soltab *stp)
{
    struct bu_external raw_external;
    struct db5_raw_internal raw_internal;
    uint8_t namespace_uuid[16];
    uint8_t uuid[16];
    /* arbitrary namespace for a v5 uuid */
    const uint8_t base_namespace_uuid[16] = {0x4a, 0x3e, 0x13, 0x3f, 0x1a, 0xfc, 0x4d, 0x6c, 0x9a, 0xdd, 0x82, 0x9b, 0x7b, 0xb6, 0xc6, 0xc1};
    uint8_t mat_buffer[SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_MAT];
    const fastf_t *matp = stp->st_matp ? stp->st_matp : bn_mat_identity;

    RT_CK_SOLTAB(stp);

    bu_cv_htond((unsigned char *)mat_buffer, (unsigned char *)matp, ELEMENTS_PER_MAT);

    if (bu_uuid_create(namespace_uuid, sizeof(mat_buffer), mat_buffer, base_namespace_uuid) != 5)
	return 0; /*bu_bomb("bu_uuid_create() failed");*/

    if (db_get_external(&raw_external, stp->st_dp, stp->st_rtip->rti_dbip))
	return 0; /*bu_bomb("db_get_external() failed");*/

    if (db5_get_raw_internal_ptr(&raw_internal, raw_external.ext_buf) == NULL)
	return 0; /*bu_bomb("rt_db_external5_to_internal5() failed");*/

    if (bu_uuid_create(uuid, raw_internal.body.ext_nbytes, raw_internal.body.ext_buf, namespace_uuid) != 5)
	return 0; /*bu_bomb("bu_uuid_create() failed");*/

    if (bu_uuid_encode(uuid, (uint8_t *)name))
	return 0; /*bu_bomb("bu_uuid_encode() failed");*/

    bu_free_external(&raw_external);
    return 1;
}


/* returns truthfully if a cache location is exists and is usable,
 * creating and initializing the location if necessary.
 */
HIDDEN int
cache_init(struct rt_cache *cache)
{
    const char *dir = cache->dir;
    char path[MAXPATHLEN] = {0};

    if (!bu_file_exists(dir, NULL)) {
	cache_warn(cache, dir, "Directory does not exist.  Initializing.");
	if (!cache_ensure_path(dir, 0)) {
	    cache_warn(cache, dir, "Cannot create cache directory.  Caching disabled.");
	    return 0;
	}
    }

    /* verify usability */

    if (!bu_file_directory(dir)) {
	cache_warn(cache, dir, "Location must be a directory.  Caching disabled.");
	return 0;
    }
    if (!bu_file_readable(dir)) {
	cache_warn(cache, dir, "Location must be readable.  Caching disabled.");
	return 0;
    }
    if (!cache->read_only && !bu_file_writable(dir)) {
	cache_warn(cache, dir, "Location is READ-ONLY.");
	cache->read_only = 1;
    }

    /* make sure there's a format file */

    snprintf(path, MAXPATHLEN, "%s%c%s", dir, BU_DIR_SEPARATOR, "format");
    if (!bu_file_exists(path, NULL)) {
	if (!cache_ensure_path(path, 1)) {
	    cache_warn(cache, path, "Cannot create format file.  Caching disabled.");
	    return 0;
	}
	if (bu_file_writable(path)) {
	    FILE *fp = fopen(path, "w");
	    int ret;
	    if (!fp) {
		perror("fopen");
		cache_warn(cache, path, "Cannot initialize format file.  Caching disabled.");
		return 0;
	    }
	    ret = fprintf(fp, "%d\n", CACHE_FORMAT);
	    fclose(fp);
	    if (ret <= 0) {
		cache_warn(cache, path, "Cannot set version in format file.  Caching disabled.");
		return 0;
	    }
	}
    }
    if (bu_file_directory(path)) {
	cache_warn(cache, path, "Directory blocking format file creation.  Caching disabled.");
	return 0;
    }
    if (!cache->read_only && !bu_file_writable(path)) {
	cache_warn(cache, path, "Location is READ-ONLY.");
	cache->read_only = 1;
    }

    /* v1+ cache is a directory of directories of .g files using
     * 2-letter directory names containing 1-object per directory and
     * file, e.g.:
     * [CACHE_DIR]/.rt/objects/A8/A8D460B2-194F-5FA7-8FED-286A6C994B89
     */
    snprintf(path, MAXPATHLEN, "%s%c%s", dir, BU_DIR_SEPARATOR, "objects");
    if (!bu_file_exists(path, NULL)) {
	if (!cache_ensure_path(path, 0)) {
	    cache_warn(cache, path, "Cannot create objects directory.  Caching disabled.");
	    return 0;
	}
    }
    if (!bu_file_directory(path)) {
	cache_warn(cache, path, "Cannot initialize objects directory.  Caching disabled.");
	return 0;
    }
    if (!cache->read_only && !bu_file_writable(path)) {
	cache_warn(cache, path, "Location is READ-ONLY.");
	cache->read_only = 1;
    }

    /* initialize database instance pointer storage */
    cache->dbip_hash = bu_hash_create(1024);

    return 1;
}


HIDDEN void
compress_external(const struct rt_cache *cache, struct bu_external *external)
{
    int ret;
    int compressed = 0;
    uint8_t *buffer;
    int compressed_size;

    BU_CK_EXTERNAL(external);

    BU_ASSERT(external->ext_nbytes < INT_MAX);
    compressed_size = LZ4_compressBound((int)external->ext_nbytes);

    /* buffer is uncompsize + maxcompsize + compressed_data */
    buffer = (uint8_t *)bu_malloc((size_t)compressed_size + SIZEOF_NETWORK_LONG, "buffer");

    *(uint32_t *)buffer = htonl((uint32_t)external->ext_nbytes);

    ret = LZ4_compress_default((const char *)external->ext_buf, (char *)(buffer + SIZEOF_NETWORK_LONG), external->ext_nbytes, compressed_size);
    if (ret != 0)
	compressed = 1;

    if (!compressed) {
	CACHE_DEBUG("++++++ [%lu.%lu] Compression failed (ret %d, %zu bytes @ %p to %d bytes max)\n", bu_process_id(), bu_parallel_id(), ret, external->ext_nbytes, (void *) external->ext_buf, compressed_size);
	return;
    }

    *(uint32_t *)buffer = htonl((uint32_t)external->ext_nbytes);

    bu_free(external->ext_buf, "ext_buf");
    external->ext_nbytes = compressed_size + SIZEOF_NETWORK_LONG;
    external->ext_buf = buffer;
}


HIDDEN void
uncompress_external(const struct rt_cache *cache, const struct bu_external *external, struct bu_external *dest)
{
    int ret;
    int uncompressed = 0;
    uint8_t *buffer;

    BU_CK_EXTERNAL(external);

    BU_EXTERNAL_INIT(dest);

    dest->ext_nbytes = ntohl(*(uint32_t *)external->ext_buf);
    buffer = (uint8_t *)bu_malloc(dest->ext_nbytes, "buffer");

    BU_ASSERT(dest->ext_nbytes < INT_MAX);
    ret = LZ4_decompress_fast((const char *)(external->ext_buf + SIZEOF_NETWORK_LONG), (char *)buffer, (int)dest->ext_nbytes);
    if (ret > 0)
	uncompressed = 1;

    if (!uncompressed) {
	CACHE_DEBUG("++++++ [%lu.%lu] decompression failed (ret %d, %zu bytes @ %p to %zu bytes max)\n", bu_process_id(), bu_parallel_id(), ret, external->ext_nbytes, (void *) external->ext_buf, dest->ext_nbytes);
	return;
    }

    dest->ext_buf = buffer;
}


HIDDEN struct db_i *
cache_read_dbip(const struct rt_cache *cache, const char *name)
{
    struct db_i *dbip = NULL;
    char path[MAXPATHLEN] = {0};

    if (!cache || !name)
	return NULL;

    bu_semaphore_acquire(RT_SEM_CACHE);

    dbip = (struct db_i *)bu_hash_get(cache->dbip_hash, (const uint8_t *)name, strlen(name));
    if (dbip) {
	bu_semaphore_release(RT_SEM_CACHE);
	return dbip;
    }

    cache_get_objfile(cache, name, path, MAXPATHLEN);
    if (!bu_file_exists(path, NULL)) {
	bu_semaphore_release(RT_SEM_CACHE);
	return NULL;
    }

    dbip = db_open(path, DB_OPEN_READONLY);
    if (!dbip) {
	bu_semaphore_release(RT_SEM_CACHE);
	return NULL;
    }

    if (db_dirbuild(dbip)) {
	/* failed to build directory */
	db_close(dbip);
	bu_semaphore_release(RT_SEM_CACHE);
	return NULL;
    }

    bu_hash_set(cache->dbip_hash, (const uint8_t *)name, strlen(name), dbip);

    bu_semaphore_release(RT_SEM_CACHE);
    return dbip;
}


HIDDEN struct db_i *
cache_create_dbip(const struct rt_cache *cache, const char *name)
{
    struct db_i *dbip = NULL;
    char path[MAXPATHLEN] = {0};

    if (!cache || !name)
	return NULL;

    cache_get_objfile(cache, name, path, MAXPATHLEN);

    /* something in the way? clobber time. */
    if (bu_file_exists(path, NULL)) {
	bu_file_delete(path);
    }

    if (!cache_ensure_path(path, 1)) {
	cache_warn(cache, path, "Cache object failure.  Continuing.");
	return NULL;
    }

    dbip = db_create(path, 5);
    if (!dbip)
	return NULL;

    return dbip;
}


HIDDEN int
cache_try_load(const struct rt_cache *cache, const char *name, const struct rt_db_internal *internal, struct soltab *stp)
{
    struct bu_external data_external = BU_EXTERNAL_INIT_ZERO;
    size_t version = (size_t)-1;
    struct bu_external raw_external;
    struct db5_raw_internal raw_internal;
    struct directory *dp;
    struct db_i *dbip;

    RT_CK_DB_INTERNAL(internal);
    RT_CK_SOLTAB(stp);

    CACHE_DEBUG("++++ [%lu.%lu] Trying to LOAD %s\n", bu_process_id(), bu_parallel_id(), name);

    dbip = cache_read_dbip(cache, name);
    if (!dbip)
	return 0; /* no storage */

    dp = db_lookup(dbip, name, 0);
    if (!dp)
	return 0; /* not in cache */

    if (dp->d_major_type != DB5_MAJORTYPE_BINARY_MIME || dp->d_minor_type != 0)
	bu_bomb("invalid object type");

    if (db_get_external(&raw_external, dp, dbip))
	return 0;

    if (db5_get_raw_internal_ptr(&raw_internal, raw_external.ext_buf) == NULL)
	return 0;

    {
	struct bu_attribute_value_set attributes;
	const char *version_str;
	const char *endptr;

	if (db5_import_attributes(&attributes, &raw_internal.attributes) < 0)
	    return 0;

	if (bu_strcmp(cache_mime_type, bu_avs_get(&attributes, "mime_type")))
	    return 0;

	version_str = bu_avs_get(&attributes, "rt_cache::version");
	if (!version_str)
	    return 0; /* unversioned?? */

	errno = 0;
	version = strtol(version_str, (char **)&endptr, 10);

	if ((version == 0 && errno) || endptr == version_str || *endptr)
	    return 0; /* invalid version */

	bu_avs_free(&attributes);
    }

    uncompress_external(cache, &raw_internal.body, &data_external);
    bu_free_external(&raw_external);

    if (rt_obj_prep_serialize(stp, internal, &data_external, &version)) {
	/* failed to deserialize */
	bu_free_external(&data_external);
	return 0;
    }

    bu_free_external(&data_external);
    return 1; /* success! */
}


HIDDEN int
cache_try_store(struct rt_cache *cache, const char *name, const struct rt_db_internal *internal, struct soltab *stp)
{
    struct bu_external attributes_external;
    struct bu_external data_external = BU_EXTERNAL_INIT_ZERO;
    size_t version = (size_t)-1;
    struct directory *dp;
    struct db_i *dbip;
    char type = 0;
    char path[MAXPATHLEN] = {0};
    char tmpname[MAXPATHLEN] = {0};
    char tmppath[MAXPATHLEN] = {0};
    int ret = 0;

    RT_CK_DB_INTERNAL(internal);
    RT_CK_SOLTAB(stp);

    CACHE_DEBUG("++++ [%lu.%lu] Trying to STORE %s\n", bu_process_id(), bu_parallel_id(), name);

    if (rt_obj_prep_serialize(stp, internal, &data_external, &version) || version == (size_t)-1) {
	CACHE_DEBUG("++++++ [%lu.%lu] Failed to serialize %s\n", bu_process_id(), bu_parallel_id(), name);
	return 0; /* can't serialize */
    }

    compress_external(cache, &data_external);

    {
	struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
	struct bu_vls version_vls = BU_VLS_INIT_ZERO;

	bu_vls_sprintf(&version_vls, "%zu", version);
	bu_avs_add(&attributes, "mime_type", cache_mime_type);
	bu_avs_add(&attributes, "rt_cache::version", bu_vls_addr(&version_vls));
	if (stp->st_dp && stp->st_dp->d_namep) {
	    bu_avs_add(&attributes, "rt_cache::source_obj", stp->st_dp->d_namep);
	}
	if (stp->st_rtip && stp->st_rtip->rti_dbip->dbi_filename) {
	    bu_avs_add(&attributes, "rt_cache::source_g", stp->st_rtip->rti_dbip->dbi_filename);
	}
	db5_export_attributes(&attributes_external, &attributes);
	bu_vls_free(&version_vls);
	bu_avs_free(&attributes);
    }

    /* [FIXME: redundant] make sure we can write to the cache dir */
    if (!bu_file_writable(cache->dir) || !bu_file_executable(cache->dir)) {
	cache_warn(cache, cache->dir, "Directory is not writable.  Caching disabled.");
	return 0;
    }
    cache_get_objdir(cache, name, tmppath, MAXPATHLEN);
    if (bu_file_exists(tmppath, NULL) && (!bu_file_writable(tmppath) || !bu_file_executable(tmppath))) {
	char objdir[MAXPATHLEN] = {0};
	bu_path_basename(tmppath, objdir);
	cache_warn(cache, objdir, "Subdirectory is not writable.  Caching disabled.");
	return 0;
    }

    /* get a temporary name unlikely to exist */
    snprintf(tmpname, MAXPATHLEN, "%s.%d.%d.%lld", name, bu_process_id(), bu_parallel_id(), (long long int)bu_gettime());

    cache_get_objfile(cache, tmpname, tmppath, MAXPATHLEN);
    bu_file_delete(tmppath); /* okay if it doesn't exist */

    dbip = cache_create_dbip(cache, tmpname);
    if (!dbip) {
	bu_file_delete(tmppath);
	CACHE_DEBUG("++++++ [%lu.%lu] Failed to create cache temp %s\n", bu_process_id(), bu_parallel_id(), tmpname);
	return 0; /* no storage */
    }

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_NON_GEOM, (void *)&type);
    if (!dp) {
	db_close(dbip);
	bu_file_delete(tmppath);
	CACHE_DEBUG("++++++ [%lu.%lu] Failed to add to cache temp %s\n", bu_process_id(), bu_parallel_id(), tmpname);
	return 0; /* bad db */
    }
    RT_CK_DIR(dp);

    dp->d_major_type = DB5_MAJORTYPE_BINARY_MIME;
    dp->d_minor_type = 0;

    {
	struct bu_external db_external;
	db5_export_object3(&db_external, 0, dp->d_namep, 0, &attributes_external,
			   &data_external, dp->d_major_type, dp->d_minor_type,
			   DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);

	if (db_put_external(&db_external, dp, dbip)) {
	    bu_free_external(&db_external);
	    db_close(dbip);
	    bu_file_delete(tmppath);
	    CACHE_DEBUG("++++++ [%lu.%lu] Failed to put cache temp %s\n", bu_process_id(), bu_parallel_id(), tmpname);
	    return 0; /* can't stash */
	}
	bu_free_external(&db_external);
    }
    db_sync(dbip);
    db_close(dbip);
    CACHE_DEBUG("++++++ [%lu.%lu] Successfully wrote cache temp %s\n", bu_process_id(), bu_parallel_id(), tmpname);

    bu_free_external(&attributes_external);
    bu_free_external(&data_external);

    /* get the real / final cache object file name */
    cache_get_objfile(cache, name, path, MAXPATHLEN);

    /* anyone beat us to creating the cache? */
    if (bu_file_exists(path, NULL)) {
	CACHE_DEBUG("++++++ [%lu.%lu] Someone else finished %s first, no longer need %s\n", bu_process_id(), bu_parallel_id(), name, tmpname);
	bu_file_delete(tmppath);

	dbip = cache_read_dbip(cache, name);
	if (dbip) {
	    CACHE_DEBUG("++++++ [%lu.%lu] Successfully read %s\n", bu_process_id(), bu_parallel_id(), name);
	    return 1;
	}
	CACHE_DEBUG("++++++ [%lu.%lu] BUT we can't read %s !!!  Giving up.\n", bu_process_id(), bu_parallel_id(), name);
	return 0;
    }

    /* atomically flip it into place */
#ifdef HAVE_WINDOWS_H
    ret = MoveFileEx(tmppath, path, MOVEFILE_WRITE_THROUGH|MOVEFILE_REPLACE_EXISTING);
#else
    ret = rename(tmppath, path);
#endif
    if (!ret) {
	bu_file_delete(tmppath);
	return 0; /* someone probably beat us to it */
    }

    if (cache_read_dbip(cache, name) != NULL) {
	return 1;
    }
    return 0;
}


int
rt_cache_prep(struct rt_cache *cache, struct soltab *stp, struct rt_db_internal *internal)
{
    int ret = 0; /* success */
    char name[37] = {0};

    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(internal);

    if (!cache || !cache_generate_name(name, stp))
	return rt_obj_prep(stp, internal, stp->st_rtip);

    if (cache_try_load(cache, name, internal, stp))
	return ret; /* found in cache */

    /* not in cache yet */

    ret = rt_obj_prep(stp, internal, stp->st_rtip);
    if (ret == 0 && !cache->read_only)
	cache_try_store(cache, name, internal, stp);

    return ret;
}


void
rt_cache_close(struct rt_cache *cache)
{
    struct bu_hash_entry *entry;

    if (!cache)
	return;

    CACHE_DEBUG("++ [%lu.%lu] Closing cache at %s\n", bu_process_id(), bu_parallel_id(), cache->dir);

    entry = bu_hash_next(cache->dbip_hash, NULL);
    while (entry) {
	struct db_i *dbip = (struct db_i *)bu_hash_value(entry, NULL);
	db_close(dbip);
	entry = bu_hash_next(cache->dbip_hash, entry);
    }
    bu_hash_destroy(cache->dbip_hash);

    cache->debug = NULL;
    cache->log = NULL;
    cache->read_only = -1;
    memset(cache->dir, 0, MAXPATHLEN);

    BU_PUT(cache, struct rt_cache);
}


struct rt_cache *
rt_cache_open(void)
{
    const char *dir = NULL;
    int format;
    struct rt_cache *result;
    struct rt_cache CACHE = CACHE_INIT;
    struct rt_cache *cache = &CACHE;

    dir = getenv("LIBRT_CACHE");
    if (!BU_STR_EMPTY(dir)) {

	/* default unset is on, so do nothing if explicitly off */
	if (bu_str_false(dir))
	    return NULL;

	dir = bu_file_realpath(dir, cache->dir);
    } else {
	/* LIBRT_CACHE is either set-and-empty or unset.  Default is on. */
	dir = bu_dir(cache->dir, MAXPATHLEN, BU_DIR_CACHE, ".rt", NULL);
    }

    if (!cache_init(cache)) {
	return NULL;
    }

    CACHE_DEBUG("++ [%lu.%lu] Opening cache at %s\n", bu_process_id(), bu_parallel_id(), dir);

    format = cache_format(cache);
    if (format < 0) {
	return NULL;
    } else if (format == 0) {
	struct bu_vls path = BU_VLS_INIT_ZERO;
	int ret;

	/* v0 cache is just a single file, delete so we can start fresh */
	bu_vls_printf(&path, "%s%c%s", dir, BU_DIR_SEPARATOR, "rt.db");
	(void)bu_file_delete(bu_vls_cstr(&path));

	bu_vls_trunc(&path, 0);
	bu_vls_printf(&path, "%s%c%s", dir, BU_DIR_SEPARATOR, "format");
	ret = bu_file_delete(bu_vls_cstr(&path));

	bu_vls_free(&path);

	/* reinit */
	if (ret) {
	    cache_init(cache);
	    format = cache_format(cache);
	}
    } else if (format == 1) {
	struct bu_vls path = BU_VLS_INIT_ZERO;
	char **matches = NULL;
	size_t count;
	size_t i;

	/* V1 cache may have corruption, delete it */

	bu_vls_printf(&path, "%s%c%s", dir, BU_DIR_SEPARATOR, "objects");

	count = bu_file_list(bu_vls_cstr(&path), "[A-Z0-9][A-Z0-9]", &matches);

	CACHE_DEBUG("++++ [%lu.%lu] Found V1 cache, deleting %zu objects in %s", bu_process_id(), bu_parallel_id(), count, bu_vls_cstr(&path));

	for (i = 0; i < count; i++) {
	    struct bu_vls subpath = BU_VLS_INIT_ZERO;
	    const char *pattern =  /* 8-4-4-4-12 */
		"[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]"
		"-"
		"[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]"
		"-"
		"[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]"
		"-"
		"[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]"
		"-"
		"[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]";
	    char **submatches = NULL;
	    size_t subcount;
	    size_t j;

	    bu_vls_printf(&subpath, "%s%c%s", bu_vls_cstr(&path), BU_DIR_SEPARATOR, matches[i]);

	    subcount = bu_file_list(bu_vls_cstr(&subpath), pattern, &submatches);

	    for (j = 0; j < subcount; j++) {
		bu_vls_trunc(&subpath, 0);
		bu_vls_printf(&subpath, "%s%cobjects%c%s%c%s", dir, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, matches[i], BU_DIR_SEPARATOR, submatches[j]);
		bu_file_delete(bu_vls_cstr(&subpath));

		CACHE_DEBUG("++++ [%lu.%lu] Deleting %s", bu_process_id(), bu_parallel_id(), bu_vls_cstr(&subpath));
	    }
	    bu_argv_free(subcount, submatches);

	    bu_vls_trunc(&subpath, 0);
	    bu_vls_printf(&subpath, "%s%cobjects%c%s", dir, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, matches[i]);
	    bu_file_delete(bu_vls_cstr(&subpath));

	    CACHE_DEBUG("++++ [%lu.%lu] Deleting %s", bu_process_id(), bu_parallel_id(), bu_vls_cstr(&subpath));

	    bu_vls_free(&subpath);
	}
	bu_argv_free(count, matches);

	bu_file_delete(bu_vls_cstr(&path));

	CACHE_DEBUG("++++ [%lu.%lu] Deleting %s", bu_process_id(), bu_parallel_id(), bu_vls_cstr(&path));

	bu_vls_trunc(&path, 0);
	bu_vls_printf(&path, "%s%c%s", dir, BU_DIR_SEPARATOR, "format");
	(void)bu_file_delete(bu_vls_addr(&path));
	bu_vls_free(&path);

	/* reinit */
	format = cache_format(cache);
    } else if (format > CACHE_FORMAT) {
	CACHE_LOG("NOTE: Cache at %s was created with a newer version of %s\n", dir, PACKAGE_NAME);
	CACHE_LOG("      Delete or move folder to enable caching with this version.\n");
    }

    if (format != CACHE_FORMAT) {
	return NULL;
    }

    BU_GET(result, struct rt_cache);
    *result = CACHE; /* struct copy */

    return result;
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
