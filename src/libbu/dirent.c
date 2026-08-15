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


static void
file_completion_heap_up(char **values, size_t pos)
{
    while (pos) {
	size_t parent = (pos - 1) / 2;
	char *swap;
	if (bu_strcmp(values[parent], values[pos]) >= 0)
	    break;
	swap = values[parent];
	values[parent] = values[pos];
	values[pos] = swap;
	pos = parent;
    }
}


static void
file_completion_heap_down(char **values, size_t count)
{
    size_t pos = 0;
    while (pos < count) {
	size_t left = 2 * pos + 1;
	size_t right = left + 1;
	size_t largest = pos;
	char *swap;
	if (left < count && bu_strcmp(values[left], values[largest]) > 0)
	    largest = left;
	if (right < count && bu_strcmp(values[right], values[largest]) > 0)
	    largest = right;
	if (largest == pos)
	    break;
	swap = values[pos];
	values[pos] = values[largest];
	values[largest] = swap;
	pos = largest;
    }
}


static void
file_completion_common_update(struct bu_vls *common_prefix, const char *candidate,
	size_t previous_count)
{
    size_t i = 0;
    size_t common_len;
    size_t candidate_len;

    if (!common_prefix)
	return;
    if (!previous_count) {
	bu_vls_strcpy(common_prefix, candidate);
	return;
    }
    common_len = bu_vls_strlen(common_prefix);
    candidate_len = strlen(candidate);
    while (i < common_len && i < candidate_len &&
	    bu_vls_cstr(common_prefix)[i] == candidate[i])
	i++;
    bu_vls_trunc(common_prefix, (int)i);
}


size_t
bu_file_complete_query_filtered(const char *seed, int flags,
	const char * const *extensions, size_t max_matches,
	bu_file_complete_filter_t filter, const void *filter_data,
	char ***matches, size_t *total_matches, struct bu_vls *common_prefix)
{
    const char *input = seed ? seed : "";
    const char *slash = strrchr(input, '/');
    size_t prefix_len;
    const char *base;
    size_t base_len;
    char *dir;
    char **out = NULL;
    size_t out_cnt = 0;
    size_t out_capacity = 0;
    size_t total = 0;
    DIR *dirp;
    struct dirent *dp;
#ifdef _WIN32
    const char *bslash = strrchr(input, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    prefix_len = slash ? (size_t)(slash - input + 1) : 0;
    base = input + prefix_len;
    base_len = strlen(base);
    if (prefix_len) {
	int keep_separator = (prefix_len == 1);
#ifdef _WIN32
	if (prefix_len == 3 && input[1] == ':' &&
		(input[2] == '/' || input[2] == '\\'))
	    keep_separator = 1;
#endif
	dir = (char *)bu_calloc(prefix_len + 1, 1, "completion directory");
	memcpy(dir, input, prefix_len);
	if (!keep_separator)
	    dir[prefix_len - 1] = '\0';
    } else {
	dir = bu_strdup(".");
    }


    if (total_matches)
	*total_matches = 0;
    if (common_prefix)
	bu_vls_trunc(common_prefix, 0);

    dirp = opendir(dir);
    while (dirp && (dp = readdir(dirp)) != NULL) {
	const char *name = dp->d_name;
	char *value;
	if (!name || BU_STR_EQUAL(name, ".") || BU_STR_EQUAL(name, ".."))
	    continue;
	if (name[0] == '.' && base[0] != '.' && !(flags & BU_FILE_COMPLETE_HIDDEN))
	    continue;
	if (bu_strncmp(name, base, base_len) != 0)
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
	if (filter && filter(bu_vls_cstr(&candidate), filter_data) != 0) {
	    bu_vls_free(&candidate);
	    bu_vls_free(&fspath);
	    continue;
	}
	value = bu_vls_strdup(&candidate);
	file_completion_common_update(common_prefix, value, total);
	total++;
	if (!max_matches || out_cnt < max_matches) {
	    if (out_cnt == out_capacity) {
		size_t new_capacity = out_capacity ? out_capacity * 2 : 16;
		if (max_matches && new_capacity > max_matches)
		    new_capacity = max_matches;
		out = (char **)bu_realloc(out, (new_capacity + 1) * sizeof(char *),
		    "file completion matches");
		out_capacity = new_capacity;
	    }
	    out[out_cnt] = value;
	    if (max_matches)
		file_completion_heap_up(out, out_cnt);
	    out_cnt++;
	} else if (out_cnt && bu_strcmp(value, out[0]) < 0) {
	    bu_free(out[0], "discarded file completion");
	    out[0] = value;
	    file_completion_heap_down(out, out_cnt);
	} else {
	    bu_free(value, "discarded file completion");
	}
	bu_vls_free(&candidate);
	bu_vls_free(&fspath);
    }
    if (dirp)
	(void)closedir(dirp);
    bu_free(dir, "completion directory");

    if (out_cnt)
	bu_sort(out, out_cnt, sizeof(char *), cmpdir, NULL);
    if (out)
	out[out_cnt] = NULL;
    if (matches) {
	*matches = out;
	if (!out)
	    *matches = (char **)bu_calloc(1, sizeof(char *), "empty file completions");
    } else if (out) {
	bu_argv_free(out_cnt, out);
    }
    if (total_matches)
	*total_matches = total;
    return out_cnt;
}


size_t
bu_file_complete_query(const char *seed, int flags, const char * const *extensions,
	size_t max_matches, char ***matches, size_t *total_matches,
	struct bu_vls *common_prefix)
{
    return bu_file_complete_query_filtered(seed, flags, extensions, max_matches,
	NULL, NULL, matches, total_matches, common_prefix);
}


size_t
bu_file_complete(const char *seed, int flags, const char * const *extensions, char ***matches)
{
    return bu_file_complete_query(seed, flags, extensions, 0, matches, NULL, NULL);
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
