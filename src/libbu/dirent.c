/*                         D I R E N T . C
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

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "bu/file.h"
#include "bu/path.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "uce-dirent.h"


static int
cmpdir(const void *a, const void *b, void *UNUSED(context))
{
    return (bu_strcmp(*(const char **)a, *(const char **)b));
}


size_t
bu_file_list(const char *path, const char *pattern, char ***files)
{
    size_t i = 0;
    size_t filecount = 0;
    DIR *dir = NULL;
    struct dirent *dp = NULL;

    /* calculate file count */
    dir = opendir(path);
    while (dir && (dp = readdir(dir)) != NULL) {
	if (!pattern
	    || (strlen(pattern) == 0)
	    || (bu_path_match(pattern, dp->d_name, 0) == 0))
	{
	    filecount++;
	}
    }
    if (dir)
	(void)closedir(dir);

    /* bail now if there's no files array pointer to fill in */
    if (!files) {
	return filecount;
    }

    /* allocate enough space plus room for a null entry too */
    *files = (char **)bu_calloc(filecount+1, sizeof(char *), "files alloc");

    dir = opendir(path);
    while (dir && (dp = readdir(dir)) != NULL) {
	if (!pattern
	    || (strlen(pattern) == 0)
	    || (bu_path_match(pattern, dp->d_name, 0) == 0))
	{
	    (*files)[i++] = bu_strdup(dp->d_name);
	}
    }
    if (dir)
	(void)closedir(dir);

    bu_sort(*files, filecount, sizeof(char *), cmpdir, NULL);

    return filecount;
}


size_t
bu_file_complete(const char *seed, int flags, const char * const *extensions, char ***matches)
{
    const char *input = seed ? seed : "";
    const char *slash = strrchr(input, '/');
#ifdef _WIN32
    const char *bslash = strrchr(input, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    size_t prefix_len = slash ? (size_t)(slash - input + 1) : 0;
    const char *base = input + prefix_len;
    char *dir = NULL;
    if (prefix_len) {
	dir = (char *)bu_calloc(prefix_len + 1, 1, "completion directory");
	memcpy(dir, input, prefix_len);
	if (prefix_len > 1)
	    dir[prefix_len - 1] = '\0';
    } else {
	dir = bu_strdup(".");
    }

    char **entries = NULL;
    size_t entry_cnt = bu_file_list(dir, "*", &entries);
    char **out = (char **)bu_calloc(entry_cnt + 1, sizeof(char *), "file completion matches");
    size_t out_cnt = 0;
    for (size_t i = 0; i < entry_cnt; i++) {
	const char *name = entries[i];
	if (!name || BU_STR_EQUAL(name, ".") || BU_STR_EQUAL(name, ".."))
	    continue;
	if (name[0] == '.' && base[0] != '.' && !(flags & BU_FILE_COMPLETE_HIDDEN))
	    continue;
	if (bu_strncmp(name, base, strlen(base)) != 0)
	    continue;

	struct bu_vls fspath = BU_VLS_INIT_ZERO;
	if (prefix_len) {
	    bu_vls_strncpy(&fspath, input, prefix_len);
	    bu_vls_strcat(&fspath, name);
	}
	else
	    bu_vls_printf(&fspath, "%s/%s", dir, name);
	int is_dir = bu_file_directory(bu_vls_cstr(&fspath));
	if ((flags & BU_FILE_COMPLETE_DIRS_ONLY) && !is_dir) {
	    bu_vls_free(&fspath);
	    continue;
	}
	if (!is_dir && extensions) {
	    int ext_match = 0;
	    for (size_t ei = 0; extensions[ei]; ei++) {
		const char *ext = extensions[ei];
		size_t nlen = strlen(name);
		size_t elen = strlen(ext);
		if (elen && ext[0] != '.') {
		    if (nlen > elen && name[nlen - elen - 1] == '.' && BU_STR_EQUAL(name + nlen - elen, ext)) ext_match = 1;
		} else if (nlen >= elen && BU_STR_EQUAL(name + nlen - elen, ext)) {
		    ext_match = 1;
		}
	    }
	    if (!ext_match) {
		bu_vls_free(&fspath);
		continue;
	    }
	}
	struct bu_vls candidate = BU_VLS_INIT_ZERO;
	if (prefix_len) bu_vls_strncpy(&candidate, input, prefix_len);
	bu_vls_strcat(&candidate, name);
	if (is_dir && (flags & BU_FILE_COMPLETE_APPEND_SLASH))
	    bu_vls_putc(&candidate, slash ? *slash : '/');
	out[out_cnt++] = bu_vls_strdup(&candidate);
	bu_vls_free(&candidate);
	bu_vls_free(&fspath);
    }
    if (entries) bu_argv_free(entry_cnt, entries);
    bu_free(dir, "completion directory");
    if (matches)
	*matches = out;
    else
	bu_argv_free(out_cnt, out);
    return out_cnt;
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
