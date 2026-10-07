/*                         C A C H E . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2026 United States Government as represented by
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
#include "./librt_private.h"

/* system headers */
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "bio.h"
#include "bnetwork.h"

/* implementation headers */
#include "bu/app.h"
#include "bu/file.h"
#include "bu/cv.h"
#include "bu/hash.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/datetime.h"
#include "bu/str.h"
#include "bu/uuid.h"
#include "rt/db_attr.h"
#include "rt/db_io.h"
#include "rt/func.h"

/* Defined in cache_lz4.c */
extern int brl_LZ4_compress_default(const char* source, char* dest, int sourceSize, int maxDestSize);
extern int brl_LZ4_compressBound(int inputSize);
extern int brl_LZ4_decompress_safe(const char* source, char* dest, int compressedSize, int dstCapacity);

#define CACHE_FORMAT 4

static const char * const cache_mime_type = "brlcad/cache";
static const char * const cache_payload_hash_attribute = "rt_cache::payload_hash128";


#define CACHE_LOG(...) if (cache && cache->log) cache->log(__VA_ARGS__);
#define CACHE_DEBUG(...) if (cache && cache->debug) cache->debug(__VA_ARGS__);

struct rt_cache_entry {
    struct bu_external *ext;
};


struct rt_cache {
    char dir[MAXPATHLEN];
    int read_only;
    int (*log)(const char *format, ...);
    int (*debug)(const char *format, ...);
};
#define CACHE_INIT {{0}, 0, bu_log, NULL}


static void
cache_get_objdir(const struct rt_cache *cache, const char *filename, char *buffer, size_t len)
{
    char idx[3] = {0};
    idx[0] = filename[0];
    idx[1] = filename[1];
    bu_dir(buffer, len, cache->dir, "objects", idx, NULL);
}


static void
cache_get_objfile(const struct rt_cache *cache, const char *filename, char *buffer, size_t len)
{
    char dir[MAXPATHLEN] = {0};
    cache_get_objdir(cache, filename, dir, MAXPATHLEN);
    bu_dir(buffer, len, dir, filename, NULL);
}


/* returns truthfully if a path exists, creating it if necessary. */
static int
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
	bu_mkdir(path);
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


static void
cache_warn(const struct rt_cache *cache, const char *path, const char *msg)
{
    CACHE_LOG("  CACHE  %s\n", path);
    CACHE_LOG("WARNING: ^ %s\n", msg);
}


static int
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


static int
cache_generate_name(char name[STATIC_ARRAY(37)], struct soltab *stp,
	const struct rt_db_internal *internal, size_t *prep_version)
{
    struct bu_external raw_external = BU_EXTERNAL_INIT_ZERO;
    struct db5_raw_internal raw_internal;
    uint8_t matrix_namespace_uuid[16];
    uint8_t namespace_uuid[16];
    uint8_t uuid[16];
    /* arbitrary namespace for a v5 uuid */
    const uint8_t base_namespace_uuid[16] = {0x4a, 0x3e, 0x13, 0x3f, 0x1a, 0xfc, 0x4d, 0x6c, 0x9a, 0xdd, 0x82, 0x9b, 0x7b, 0xb6, 0xc6, 0xc1};
    uint8_t mat_buffer[SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_MAT];
    const fastf_t *matp = stp->st_matp ? stp->st_matp : bn_mat_identity;
    uint64_t network_prep_version;

    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(internal);
    if (!prep_version)
	return 0;

    *prep_version = (size_t)-1;
    if (rt_obj_prep_serialize(stp, internal, NULL, prep_version) ||
	    *prep_version == (size_t)-1)
	return 0;

    bu_cv_htond((unsigned char *)mat_buffer, (unsigned char *)matp, ELEMENTS_PER_MAT);

    if (bu_uuid_create(matrix_namespace_uuid, sizeof(mat_buffer), mat_buffer,
	    base_namespace_uuid) != 5)
	return 0; /*bu_bomb("bu_uuid_create() failed");*/

    /* A prep-cache object is identified by geometry, transform, serialized
     * representation, and all primitive-specific prep options.  Encoding the
     * queried variant in the UUID namespace lets incompatible variants coexist
     * instead of repeatedly rejecting and then failing to replace one shared
     * filename. */
    network_prep_version = htonll((uint64_t)*prep_version);
    if (bu_uuid_create(namespace_uuid, sizeof(network_prep_version),
	    (const uint8_t *)&network_prep_version, matrix_namespace_uuid) != 5)
	return 0; /*bu_bomb("bu_uuid_create() failed");*/

    if (db_get_external(&raw_external, stp->st_dp, stp->st_rtip->rti_dbip)) {
	bu_free_external(&raw_external);
	return 0; /*bu_bomb("db_get_external() failed");*/
    }

    if (db5_get_raw_internal_ptr(&raw_internal, raw_external.ext_buf) == NULL)
	goto failure; /*bu_bomb("rt_db_external5_to_internal5() failed");*/

    if (bu_uuid_create(uuid, raw_internal.body.ext_nbytes, raw_internal.body.ext_buf, namespace_uuid) != 5)
	goto failure; /*bu_bomb("bu_uuid_create() failed");*/

    if (bu_uuid_encode(uuid, (uint8_t *)name))
	goto failure; /*bu_bomb("bu_uuid_encode() failed");*/

    bu_free_external(&raw_external);
    return 1;

failure:
    bu_free_external(&raw_external);
    return 0;
}


/* returns truthfully if a cache location is exists and is usable,
 * creating and initializing the location if necessary.
 */
static int
cache_init(struct rt_cache *cache)
{
    const char *dir = cache->dir;
    char path[MAXPATHLEN] = {0};
    char dirsep[2] = {BU_DIR_SEPARATOR, '\0'};

    if (!bu_file_exists(dir, NULL)) {
	//cache_warn(cache, dir, "Directory does not exist.  Initializing.");
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

    /* make sure there's a cache/dir/format file */

    bu_strlcpy(path, dir, MAXPATHLEN);
    bu_strlcat(path, dirsep, MAXPATHLEN);
    bu_strlcat(path, "format", MAXPATHLEN);

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

    bu_strlcpy(path, dir, MAXPATHLEN);
    bu_strlcat(path, dirsep, MAXPATHLEN);
    bu_strlcat(path, "objects", MAXPATHLEN);

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

    return 1;
}


static int
cache_decode_length(size_t *value, const uint8_t **cursor,
	const uint8_t *limit, unsigned int width_code)
{
    uint64_t decoded = 0;
    size_t width;

    if (!value || !cursor || !*cursor || !limit || *cursor > limit ||
	    width_code > DB5HDR_WIDTHCODE_64BIT)
	return 0;

    width = (size_t)1 << width_code;
    if ((size_t)(limit - *cursor) < width)
	return 0;

    for (size_t i = 0; i < width; ++i)
	decoded = (decoded << 8) | (*cursor)[i];
    if (sizeof(size_t) < sizeof(decoded) && decoded > (uint64_t)SIZE_MAX)
	return 0;

    *cursor += width;
    *value = (size_t)decoded;
    return 1;
}


/* Validate every length before handing a cache object to the historical DB5
 * pointer parser, whose interface does not include the available buffer size. */
static int
cache_db5_external_valid(const struct bu_external *external)
{
    const uint8_t *cursor;
    const uint8_t *limit;
    const uint8_t * const buffer = external ? external->ext_buf : NULL;
    const size_t bytes = external ? external->ext_nbytes : 0;
    const size_t header_bytes = sizeof(struct db5_ondisk_header);
    const uint8_t hflags = buffer && bytes > 1 ? buffer[1] : 0;
    const uint8_t aflags = buffer && bytes > 2 ? buffer[2] : 0;
    const uint8_t bflags = buffer && bytes > 3 ? buffer[3] : 0;
    size_t object_chunks = 0;
    size_t field_bytes = 0;

    if (!buffer || bytes < header_bytes + 2 || (bytes & 7) ||
	    buffer[0] != DB5HDR_MAGIC1 || buffer[bytes - 1] != DB5HDR_MAGIC2)
	return 0;
    if ((hflags & DB5HDR_HFLAGS_DLI_MASK) !=
	    DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT ||
	    !(hflags & DB5HDR_HFLAGS_NAME_PRESENT) ||
	    !(aflags & DB5HDR_AFLAGS_PRESENT) ||
	    !(bflags & DB5HDR_BFLAGS_PRESENT) ||
	    (aflags & DB5HDR_AFLAGS_ZZZ_MASK) != DB5_ZZZ_UNCOMPRESSED ||
	    (bflags & DB5HDR_BFLAGS_ZZZ_MASK) != DB5_ZZZ_UNCOMPRESSED ||
	    buffer[4] != DB5_MAJORTYPE_BINARY_MIME || buffer[5] != 0)
	return 0;

    cursor = buffer + header_bytes;
    limit = buffer + bytes - 1;
    if (!cache_decode_length(&object_chunks, &cursor, limit,
	    (hflags & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >>
		DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT) ||
	    object_chunks != bytes / 8)
	return 0;

    if (!cache_decode_length(&field_bytes, &cursor, limit,
	    (hflags & DB5HDR_HFLAGS_NAME_WIDTH_MASK) >>
		DB5HDR_HFLAGS_NAME_WIDTH_SHIFT) ||
	    field_bytes > (size_t)(limit - cursor) || !field_bytes ||
	    cursor[field_bytes - 1] != '\0')
	return 0;
    cursor += field_bytes;

    if (!cache_decode_length(&field_bytes, &cursor, limit,
	    (aflags & DB5HDR_AFLAGS_WIDTH_MASK) >>
		DB5HDR_AFLAGS_WIDTH_SHIFT) ||
	    field_bytes > (size_t)(limit - cursor) || field_bytes < 4)
	return 0;
    cursor += field_bytes;

    if (!cache_decode_length(&field_bytes, &cursor, limit,
	    (bflags & DB5HDR_BFLAGS_WIDTH_MASK) >>
		DB5HDR_BFLAGS_WIDTH_SHIFT) ||
	    field_bytes > (size_t)(limit - cursor) ||
	    field_bytes <= SIZEOF_NETWORK_LONG)
	return 0;
    cursor += field_bytes;

    if ((size_t)(limit - cursor) > 7)
	return 0;
    while (cursor < limit) {
	if (*cursor++)
	    return 0;
    }
    return 1;
}


static int
cache_attributes_valid(const struct bu_external *attributes)
{
    const uint8_t *cursor;
    const uint8_t *limit;

    if (!attributes || !attributes->ext_buf || attributes->ext_nbytes < 4)
	return 0;
    cursor = attributes->ext_buf;
    limit = cursor + attributes->ext_nbytes;
    while (cursor < limit && *cursor) {
	const uint8_t *terminator = (const uint8_t *)memchr(cursor, '\0',
	    (size_t)(limit - cursor));
	if (!terminator)
	    return 0;
	cursor = terminator + 1;
	if (cursor >= limit)
	    return 0;
	terminator = (const uint8_t *)memchr(cursor, '\0',
	    (size_t)(limit - cursor));
	if (!terminator)
	    return 0;
	cursor = terminator + 1;
    }

    return cursor < limit && !*cursor && cursor + 1 == limit;
}


static void
cache_payload_hash(char encoded[STATIC_ARRAY(33)],
	const struct bu_external *external)
{
    const bu_h128_t hash = bu_data_hash128(external->ext_buf,
	external->ext_nbytes);
    snprintf(encoded, 33, "%016llx%016llx",
	(unsigned long long)hash.w[0], (unsigned long long)hash.w[1]);
}


int
_rt_cache_compress_external(struct bu_external *external)
{
    int ret;
    uint32_t network_size;
    uint8_t *buffer;
    int maximum_compressed_size;

    BU_CK_EXTERNAL(external);

    if (!external->ext_buf || !external->ext_nbytes ||
	    external->ext_nbytes >= INT_MAX)
	return 0;
    maximum_compressed_size =
	brl_LZ4_compressBound((int)external->ext_nbytes);
    if (maximum_compressed_size <= 0)
	return 0;

    /* buffer is uncompsize + maxcompsize + compressed_data */
    buffer = (uint8_t *)bu_malloc((size_t)maximum_compressed_size +
	SIZEOF_NETWORK_LONG, "buffer");

    network_size = htonl((uint32_t)external->ext_nbytes);
    memcpy(buffer, &network_size, sizeof(network_size));

    ret = brl_LZ4_compress_default((const char *)external->ext_buf,
	(char *)(buffer + SIZEOF_NETWORK_LONG), (int)external->ext_nbytes,
	maximum_compressed_size);
    if (ret <= 0) {
	bu_free(buffer, "buffer");
	return 0;
    }

    bu_free(external->ext_buf, "ext_buf");
    /* LZ4 returns the bytes it actually wrote.  The former maximum-bound
     * assignment retained and wrote the unused tail of this buffer, largely
     * defeating cache compression. */
    external->ext_nbytes = (size_t)ret + SIZEOF_NETWORK_LONG;
    external->ext_buf = buffer;
    return 1;
}


static int
compress_external(const struct rt_cache *cache, struct bu_external *external)
{
    const size_t source_bytes = external ? external->ext_nbytes : 0;
    if (!_rt_cache_compress_external(external)) {
	CACHE_DEBUG("++++++ [%lu.%lu] Compression failed (%zu bytes @ %p)\n",
	    bu_pid(), bu_parallel_id(), source_bytes,
	    external ? (void *)external->ext_buf : NULL);
        return 0;
    }
    return 1;
}


static int
uncompress_external(const struct rt_cache *cache, const struct bu_external *external, struct bu_external *dest)
{
    int ret;
    size_t compressed_bytes;
    uint32_t network_size;
    uint8_t *buffer;

    BU_CK_EXTERNAL(external);

    BU_EXTERNAL_INIT(dest);

    if (!external->ext_buf || external->ext_nbytes <= SIZEOF_NETWORK_LONG)
	return 0;

    compressed_bytes = external->ext_nbytes - SIZEOF_NETWORK_LONG;
    if (compressed_bytes >= INT_MAX)
	return 0;

    memcpy(&network_size, external->ext_buf, sizeof(network_size));
    dest->ext_nbytes = ntohl(network_size);
    if (!dest->ext_nbytes || dest->ext_nbytes >= INT_MAX) {
	dest->ext_nbytes = 0;
	return 0;
    }
    buffer = (uint8_t *)bu_malloc(dest->ext_nbytes, "buffer");

    BU_ASSERT(dest->ext_nbytes < INT_MAX);
    ret = brl_LZ4_decompress_safe(
	(const char *)(external->ext_buf + SIZEOF_NETWORK_LONG),
	(char *)buffer, (int)compressed_bytes, (int)dest->ext_nbytes);
    if (ret != (int)dest->ext_nbytes) {
	CACHE_DEBUG("++++++ [%lu.%lu] decompression failed (ret %d, %zu bytes @ %p to %zu bytes max)\n", bu_pid(), bu_parallel_id(), ret, external->ext_nbytes, (void *) external->ext_buf, dest->ext_nbytes);
	bu_free(buffer, "buffer");
	dest->ext_nbytes = 0;
	return 0;
    }

    dest->ext_buf = buffer;
    return 1;
}


static struct rt_cache_entry *
cache_read_entry(const struct rt_cache *cache, const char *name)
{
    char path[MAXPATHLEN] = {0};

    if (!cache || !name)
	return NULL;

    cache_get_objfile(cache, name, path, MAXPATHLEN);
    if (!bu_file_exists(path, NULL))
	return NULL;

    /* Size and read through the same handle.  Querying the pathname first
     * races a concurrent atomic replacement: the size can describe the old
     * entry while fopen observes the new one.  Windows may also deny an open
     * for a short interval while MoveFileEx replaces an existing entry, so
     * retry only after the final pathname was already observed.  A normal
     * cold-cache miss above pays no delay. */
#ifdef HAVE_WINDOWS_H
    const int maximum_attempts = 4;
#else
    const int maximum_attempts = 1;
#endif
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
	FILE *ficache = fopen(path, "rb");
	long int fbytes = 0;
	if (ficache && !fseek(ficache, 0, SEEK_END)) {
	    fbytes = ftell(ficache);
	    if (fbytes > 0 && !fseek(ficache, 0, SEEK_SET)) {
		struct rt_cache_entry *e = NULL;
		BU_GET(e, struct rt_cache_entry);
		BU_GET(e->ext, struct bu_external);
		BU_EXTERNAL_INIT(e->ext);
		e->ext->ext_nbytes = (size_t)fbytes;
		e->ext->ext_buf = (uint8_t *)bu_malloc(
		    e->ext->ext_nbytes, "prep cache entry");
		if (fread(e->ext->ext_buf, 1, e->ext->ext_nbytes,
			ficache) == e->ext->ext_nbytes) {
		    fclose(ficache);
		    return e;
		}
		bu_free_external(e->ext);
		BU_PUT(e->ext, struct bu_external);
		BU_PUT(e, struct rt_cache_entry);
	    }
	}
	if (ficache)
	    fclose(ficache);
#ifdef HAVE_WINDOWS_H
	if (attempt + 1 < maximum_attempts)
	    (void)bu_snooze((int64_t)1000 << attempt);
#endif
    }
    return NULL;
}


static void
cache_free_entry(struct rt_cache_entry *entry)
{
    if (!entry)
	return;
    bu_free_external(entry->ext);
    BU_PUT(entry->ext, struct bu_external);
    BU_PUT(entry, struct rt_cache_entry);
}


static int
cache_create_dir(const struct rt_cache *cache, const char *name)
{
    char path[MAXPATHLEN] = {0};

    if (!cache || !name)
	return 0;

    cache_get_objfile(cache, name, path, MAXPATHLEN);

    /* something in the way? clobber time. */
    if (bu_file_exists(path, NULL)) {
	bu_file_delete(path);
    }

    if (!cache_ensure_path(path, 1)) {
	cache_warn(cache, path, "Cache object failure.  Continuing.");
	return 0;
    }

    return 1;
}


static int
cache_try_load(const struct rt_cache *cache, const char *name,
	const struct rt_db_internal *internal, struct soltab *stp,
	size_t expected_version)
{
    size_t version = (size_t)-1;
    struct db5_raw_internal raw_internal;
    struct bu_external data_external = BU_EXTERNAL_INIT_ZERO;
    struct rt_cache_entry *e;

    RT_CK_DB_INTERNAL(internal);
    RT_CK_SOLTAB(stp);

    CACHE_DEBUG("++++ [%lu.%lu] Trying to LOAD %s\n", bu_pid(), bu_parallel_id(), name);

    e = cache_read_entry(cache, name);
    if (!e) {
	return 0; /* no storage */
    }

    if (!cache_db5_external_valid(e->ext)) {
	cache_free_entry(e);
	return 0;
    }
    if (db5_get_raw_internal_ptr(&raw_internal, e->ext->ext_buf) == NULL) {
	cache_free_entry(e);
	return 0;
    }
    if (raw_internal.name.ext_nbytes != strlen(name) + 1 ||
	    memcmp(raw_internal.name.ext_buf, name,
		strlen(name) + 1)) {
	cache_free_entry(e);
	return 0;
    }

    {
	struct bu_attribute_value_set attributes;
	char payload_hash[33] = {0};
	const char *payload_hash_str;
	const char *version_str;
	const char *endptr;
	unsigned long long parsed_version;

	if (!cache_attributes_valid(&raw_internal.attributes)) {
	    cache_free_entry(e);
	    return 0;
	}
	if (db5_import_attributes(&attributes, &raw_internal.attributes) < 0) {
	    cache_free_entry(e);
	    return 0;
	}

	if (!bu_avs_get(&attributes, "mime_type") ||
		bu_strcmp(cache_mime_type,
		    bu_avs_get(&attributes, "mime_type"))) {
	    bu_avs_free(&attributes);
	    cache_free_entry(e);
	    return 0;
	}

	version_str = bu_avs_get(&attributes, "rt_cache::version");
	if (!version_str) {
	    bu_avs_free(&attributes);
	    cache_free_entry(e);
	    return 0; /* unversioned?? */
	}

	errno = 0;
	parsed_version = strtoull(version_str, (char **)&endptr, 10);

	if (errno || endptr == version_str || *endptr ||
		parsed_version > (unsigned long long)SIZE_MAX) {
	    bu_avs_free(&attributes);
	    cache_free_entry(e);
	    return 0; /* invalid version */
	}
	version = (size_t)parsed_version;
	if (version != expected_version) {
	    bu_avs_free(&attributes);
	    cache_free_entry(e);
	    return 0;
	}

	payload_hash_str = bu_avs_get(&attributes,
	    cache_payload_hash_attribute);
	cache_payload_hash(payload_hash, &raw_internal.body);
	if (!payload_hash_str || strlen(payload_hash_str) != 32 ||
		bu_strcmp(payload_hash_str, payload_hash)) {
	    bu_avs_free(&attributes);
	    cache_free_entry(e);
	    return 0;
	}

	bu_avs_free(&attributes);
    }

    if (!uncompress_external(cache, &raw_internal.body, &data_external)) {
	cache_free_entry(e);
	return 0;
    }
    cache_free_entry(e);

    if (rt_obj_prep_serialize(stp, internal, &data_external, &version)) {
	/* failed to deserialize */
	bu_free_external(&data_external);
	return 0;
    }

    bu_free_external(&data_external);
    return 1; /* success! */
}


static int
cache_try_store(struct rt_cache *cache, const char *name,
	const struct rt_db_internal *internal, struct soltab *stp,
	size_t expected_version)
{
    FILE *focache = NULL;
    struct bu_external attributes_external = BU_EXTERNAL_INIT_ZERO;
    struct bu_external data_external = BU_EXTERNAL_INIT_ZERO;
    struct bu_external db_external = BU_EXTERNAL_INIT_ZERO;
    size_t version = (size_t)-1;
    char path[MAXPATHLEN] = {0};
    char tmpname[MAXPATHLEN] = {0};
    char tmppath[MAXPATHLEN] = {0};
    int ret = 0;

    RT_CK_DB_INTERNAL(internal);
    RT_CK_SOLTAB(stp);

    CACHE_DEBUG("++++ [%lu.%lu] Trying to STORE %s\n", bu_pid(), bu_parallel_id(), name);

    if (rt_obj_prep_serialize(stp, internal, &data_external, &version) || version == (size_t)-1) {
	CACHE_DEBUG("++++++ [%lu.%lu] Failed to serialize %s\n", bu_pid(), bu_parallel_id(), name);
	bu_free_external(&data_external);
	return 0; /* can't serialize */
    }
    if (version != expected_version) {
	CACHE_DEBUG("++++++ [%lu.%lu] Prep variant changed while serializing %s\n", bu_pid(), bu_parallel_id(), name);
	bu_free_external(&data_external);
	return 0;
    }

    if (!compress_external(cache, &data_external)) {
	bu_free_external(&data_external);
	return 0;
    }

    {
	struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
	char payload_hash[33] = {0};
	struct bu_vls version_vls = BU_VLS_INIT_ZERO;

	cache_payload_hash(payload_hash, &data_external);
	bu_vls_sprintf(&version_vls, "%zu", version);
	bu_avs_add(&attributes, "mime_type", cache_mime_type);
	bu_avs_add(&attributes, "rt_cache::version", bu_vls_addr(&version_vls));
	bu_avs_add(&attributes, cache_payload_hash_attribute, payload_hash);
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
	bu_free_external(&attributes_external);
	bu_free_external(&data_external);
	return 0;
    }
    cache_get_objdir(cache, name, tmppath, MAXPATHLEN);
    if (bu_file_exists(tmppath, NULL) && (!bu_file_writable(tmppath) || !bu_file_executable(tmppath))) {
	char objdir[MAXPATHLEN] = {0};
	bu_path_basename(tmppath, objdir);
	cache_warn(cache, objdir, "Subdirectory is not writable.  Caching disabled.");
	bu_free_external(&attributes_external);
	bu_free_external(&data_external);
	return 0;
    }

    /* get a temporary name unlikely to exist */
    snprintf(tmpname, MAXPATHLEN, "%s.%d.%d.%lld", name, bu_pid(), bu_parallel_id(), (long long int)bu_gettime());

    cache_get_objfile(cache, tmpname, tmppath, MAXPATHLEN);
    bu_file_delete(tmppath); /* okay if it doesn't exist */

    if (!cache_create_dir(cache, tmpname)) {
	CACHE_DEBUG("++++++ [%lu.%lu] Failed to create cache dir %s\n", bu_pid(), bu_parallel_id(), tmpname);
	bu_free_external(&attributes_external);
	bu_free_external(&data_external);
	return 0; /* no storage */
    }

    db5_export_object3(&db_external, 0, name, 0, &attributes_external,
		       &data_external, DB5_MAJORTYPE_BINARY_MIME, 0,
		       DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);

    focache = fopen(tmppath, "wb");
    if (!focache) {
	CACHE_DEBUG("++++++ [%lu.%lu] Failed to put cache temp %s\n", bu_pid(), bu_parallel_id(), tmpname);
	bu_file_delete(tmppath);
	bu_free_external(&db_external);
	bu_free_external(&attributes_external);
	bu_free_external(&data_external);
	return 0; /* can't stash */
    }

    if (fwrite((void *)db_external.ext_buf, 1, db_external.ext_nbytes, focache) != db_external.ext_nbytes) {
	fclose(focache);
	bu_file_delete(tmppath);
	bu_free_external(&db_external);
	bu_free_external(&attributes_external);
	bu_free_external(&data_external);
	CACHE_DEBUG("++++++ [%lu.%lu] Failed to put cache temp %s\n", bu_pid(), bu_parallel_id(), tmpname);
	return 0; /* can't stash */
    }
    fclose(focache);

    CACHE_DEBUG("++++++ [%lu.%lu] Successfully wrote cache temp %s\n", bu_pid(), bu_parallel_id(), tmpname);

    bu_free_external(&db_external);
    bu_free_external(&attributes_external);
    bu_free_external(&data_external);

    /* get the real / final cache object file name */
    cache_get_objfile(cache, name, path, MAXPATHLEN);

    /* Atomically publish this complete entry.  Replacing an existing object is
     * intentional: a same-name entry may have just failed validation, and
     * concurrent writers for one content-derived name produce equivalent
     * entries. */
#ifdef HAVE_WINDOWS_H
    /* invert zero-is-failure return */
    ret = !MoveFileEx(tmppath, path, MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING);
#else
    ret = rename(tmppath, path);
#endif
    if (ret) {
	bu_file_delete(tmppath);
	return 0; /* someone probably beat us to it */
    }

    struct rt_cache_entry *entry = cache_read_entry(cache, name);
    if (entry != NULL) {
	cache_free_entry(entry);
	return 1;
    }
    return 0;
}


int
rt_cache_prep(struct rt_cache *cache, struct soltab *stp, struct rt_db_internal *internal)
{
    int ret = 0; /* success */
    char name[37] = {0};
    size_t prep_version = (size_t)-1;

    RT_CK_SOLTAB(stp);
    RT_CK_DB_INTERNAL(internal);

    if (!cache || !cache_generate_name(name, stp, internal, &prep_version))
	return rt_obj_prep(stp, internal, stp->st_rtip);

    if (cache_try_load(cache, name, internal, stp, prep_version))
	return ret; /* found in cache */

    /* not in cache yet */

    ret = rt_obj_prep(stp, internal, stp->st_rtip);
    if (ret == 0 && !cache->read_only)
	cache_try_store(cache, name, internal, stp, prep_version);

    return ret;
}


void
rt_cache_close(struct rt_cache *cache)
{
    if (!cache)
	return;

    CACHE_DEBUG("++ [%lu.%lu] Closing cache at %s\n", bu_pid(), bu_parallel_id(), cache->dir);

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

	bu_strlcpy(cache->dir, dir, MAXPATHLEN);
	/* dir = bu_file_realpath(dir, cache->dir); */
    } else {
	/* LIBRT_CACHE is either set-and-empty or unset.  Default is on. */
	dir = bu_dir(cache->dir, MAXPATHLEN, BU_DIR_CACHE, ".rt", NULL);
    }

    if (!cache_init(cache)) {
	return NULL;
    }

    CACHE_DEBUG("++ [%lu.%lu] Opening cache at %s\n", bu_pid(), bu_parallel_id(), dir);

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
    } else if (format < CACHE_FORMAT) {
	struct bu_vls path = BU_VLS_INIT_ZERO;
	char **matches = NULL;
	size_t count;
	size_t i;

	/* Older formats either used a different object container or lack current
	 * integrity metadata.  They cannot be mixed with this reader. */

	bu_vls_printf(&path, "%s%c%s", dir, BU_DIR_SEPARATOR, "objects");

	count = bu_file_list(bu_vls_cstr(&path), "[A-Z0-9][A-Z0-9]", &matches);

	CACHE_DEBUG("++++ [%lu.%lu] Found V%d cache, deleting %zu object dirs in %s", bu_pid(), bu_parallel_id(), format, count, bu_vls_cstr(&path));

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

		CACHE_DEBUG("++++ [%lu.%lu] Deleting %s", bu_pid(), bu_parallel_id(), bu_vls_cstr(&subpath));
	    }
	    bu_argv_free(subcount, submatches);

	    bu_vls_trunc(&subpath, 0);
	    bu_vls_printf(&subpath, "%s%cobjects%c%s", dir, BU_DIR_SEPARATOR, BU_DIR_SEPARATOR, matches[i]);
	    bu_file_delete(bu_vls_cstr(&subpath));

	    CACHE_DEBUG("++++ [%lu.%lu] Deleting %s", bu_pid(), bu_parallel_id(), bu_vls_cstr(&subpath));

	    bu_vls_free(&subpath);
	}
	bu_argv_free(count, matches);

	bu_file_delete(bu_vls_cstr(&path));

	CACHE_DEBUG("++++ [%lu.%lu] Deleting %s", bu_pid(), bu_parallel_id(), bu_vls_cstr(&path));

	bu_vls_trunc(&path, 0);
	bu_vls_printf(&path, "%s%c%s", dir, BU_DIR_SEPARATOR, "format");
	(void)bu_file_delete(bu_vls_addr(&path));
	bu_vls_free(&path);

	/* Recreate both the directory structure and the format marker. */
	if (!cache_init(cache))
	    return NULL;
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
