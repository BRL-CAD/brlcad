/*                      D C A C H E . C P P
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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
/** @addtogroup ged_db*/
/** @{ */
/** @file libged/dcache.cpp
 *
 * Manage cached drawing data.
 */

#include "common.h"

#include <algorithm>
#include <map>
#include <thread>
#include <fstream>
#include <sstream>

extern "C" {
#include "lmdb.h"
}

#include "./alphanum.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bu/path.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bv/lod.h"
#include "raytrace.h"
#include "ged/defines.h"
#include "ged/view.h"
#include "./ged_private.h"

// Subdirectory in BRL-CAD cache to Dbi state data
#define DBI_CACHEDIR ".Dbi"

// Maximum database size.
#define CACHE_MAX_DB_SIZE 4294967296

// Define what format of the cache is current - if it doesn't match, we need
// to wipe and redo.
#define CACHE_CURRENT_FORMAT 3


struct bu_cache *
dbi_cache_open(const char *name)
{
    struct bu_cache *c = NULL;

    // Hash the input filename to generate a key for uniqueness - the base name
    // of the specified .g file may not be unique on the filesystem, but by
    // definition the full path to the file is.
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s", bu_path_normalize(name));
    unsigned long long hash = bu_data_hash(bu_vls_cstr(&fname), bu_vls_strlen(&fname)*sizeof(char));

    // Get the basename and add the hash
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    bu_path_component(&bname, bu_path_normalize(name), BU_PATH_BASENAME_EXTLESS);
    bu_vls_sprintf(&fname, "%s_%llu", bu_vls_cstr(&bname), hash);
    bu_vls_free(&bname);
    char crelpath[MAXPATHLEN];
    bu_dir(crelpath, MAXPATHLEN, DBI_CACHEDIR, bu_vls_cstr(&fname), NULL);
    bu_vls_free(&fname);

    // See if we have a pre-existing format in place - if we do, and it is too old,
    // clear the old cache
    char dir[MAXPATHLEN];
    bu_dir(dir, MAXPATHLEN, BU_DIR_CACHE, DBI_CACHEDIR, "format", NULL);
    if (bu_file_exists(dir, NULL)) {
	std::ifstream format_file(dir);
	size_t disk_format_version = 0;
	format_file >> disk_format_version;
	format_file.close();
	if (disk_format_version != CACHE_CURRENT_FORMAT) {
	    bu_log("Old GED drawing info cache (%zd) found - clearing\n", disk_format_version);
	    bu_cache_erase(bu_vls_cstr(&fname));
	}
    }

    // Open the cache, creating it if it doesn't already exist
    c = bu_cache_open(crelpath, 1, 0);

    if (!c)
	return NULL;

    // Make sure our identification of the cache on-disk format is current.
    if (c) {
	FILE *fp = fopen(dir, "w");
	if (fp) {
	    fprintf(fp, "%d\n", CACHE_CURRENT_FORMAT);
	    fclose(fp);
	}
    }

    // Success - return the context
    return c;
}

size_t
cache_get(struct bu_cache *c, void **data, unsigned long long hash, const char *component)
{
    if (!c || !data || hash == 0 || !component)
	return 0;

    // Construct lookup key
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);

    return bu_cache_get(data, keystr.c_str(), c);
}

void
cache_write(struct bu_cache *c, unsigned long long hash, const char *component, std::stringstream &s)
{
    if (!c || hash == 0 || !component)
	return;

    // Prepare inputs for writing
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);
    std::string buffer = s.str();
    bu_cache_write(buffer.data(), buffer.length()*sizeof(char), keystr.c_str(), c);
}

void
cache_del(struct bu_cache *c, unsigned long long hash, const char *component)
{
    if (!c || hash == 0 || !component)
	return;

    // Construct lookup key
    std::string keystr = std::to_string(hash) + std::string(":") + std::string(component);

    bu_cache_clear(keystr.c_str(), c);
}


/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
