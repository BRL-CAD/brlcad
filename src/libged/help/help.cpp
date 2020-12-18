/*                         H E L P . C
 * BRL-CAD
 *
 * Copyright (c) 2017-2020 United States Government as represented by
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

#include <ctype.h>
#include <string>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"


/**
 * get a list of all the files under a given 'dir' filepath,
 * dynamically allocated.  returns the number of files found in an
 * argv array that the caller must free via bu_argv_free().
 */
HIDDEN size_t
help_files(const char *dir, char ***files)
{
    char **dirs;
    char **entries;
    char **listing;
    size_t i, count, listing_count, dir_count;
    size_t max_count = 2048; /* pfa number based on current doc count */

    if (!dir || !files)
	return 0;

    count = 0;
    entries = *files = (char **)bu_calloc(max_count, sizeof(char *), "allocate entries");
    dirs = (char **)bu_calloc(max_count, sizeof(char *), "allocate dirs");

    dirs[0] = bu_strdup(dir);
    dir_count = 1;
    listing_count = 0;

    while (dir_count-- > 0) {
	char *curdir = dirs[dir_count];

	bu_log("Processing %s\n", curdir);
	listing_count = bu_file_list(curdir, NULL, &listing);

	for (i = 0; i < listing_count; i++) {
	    struct bu_vls filepath = BU_VLS_INIT_ZERO;

	    /* skip the canonical dirs and unreadable paths */
	    if (BU_STR_EQUIV(listing[i], ".") || BU_STR_EQUIV(listing[i], "..")) {
		continue;
	    }

	    bu_vls_sprintf(&filepath, "%s%c%s", curdir, BU_DIR_SEPARATOR, listing[i]);
	    if (!bu_file_readable(bu_vls_cstr(&filepath))) {
		continue;
	    }

	    if (!bu_file_directory(bu_vls_cstr(&filepath))) {
		if (count > max_count-1) {
		    max_count *= 2;
		    entries = *files = (char **)bu_realloc(*files, sizeof(char *) * max_count, "increase entries");
		}
		entries[count++] = bu_vls_strdup(&filepath);
	    } else {
		dirs[dir_count] = bu_strdup(bu_vls_cstr(&filepath));
		dir_count++;
	    }

	    bu_vls_free(&filepath);
	}

	bu_free(curdir, "free curdir");
	dirs[dir_count] = NULL;
    }

    bu_argv_free(listing_count, listing);

    return count;
}


HIDDEN size_t
help_tokenize(size_t count, const char **files)
{
    size_t bytes;
    size_t zeros;
    struct bu_mapped_file *data;

#define USE_ARRAY 0
#define MAX_WORDS 820000
#if USE_ARRAY
    struct bu_vls words[MAX_WORDS] = {BU_VLS_INIT_ZERO};
#else
    struct bu_hash_tbl *hash;
#endif
    size_t cnt[MAX_WORDS] = {0};
    size_t words = 0;

#if !USE_ARRAY
    hash = bu_hash_create(2048);
#endif

    while (count-- > 0) {
	struct bu_vls word = BU_VLS_INIT_ZERO;

#if USE_ARRAY
	/* lotta words? leave an empty spot */
	if (words+1 > MAX_WORDS-1) {
	    continue;
	}
#endif

	bytes = zeros = 0;
	data = bu_open_mapped_file(files[count], NULL);
	if (!data)
	    continue;

	/* binary files have a propensity for nul bytes */
	for (bytes = 0; bytes < data->buflen; bytes++) {
	    if (((const char *)data->buf)[bytes] == '\0') {
		zeros++;
		if (zeros > 1) {
		    break;
		}
	    }
	}

	/* skip binary */
	if (zeros) {
	    bu_close_mapped_file(data);
	    continue;
	}

	/* tokenize one file */
	/* && words+1 <= MAX_WORDS-1 */
	for (bytes = 0; bytes < data->buflen; bytes++) {
	    const uint8_t *wordbytes;
	    size_t wordbyteslen;
	    int c = ((const char *)data->buf)[bytes];
	    int *cntptr;

	    if (isalnum(c)) {
		bu_vls_putc(&word, tolower(c));
	    } else if (bu_vls_strlen(&word) > 0) {
		wordbytes = (const uint8_t *)bu_vls_cstr(&word);
		wordbyteslen = bu_vls_strlen(&word);
		/* bu_log("WORD: %s\n", (const char *)wordbytes); */

#if USE_ARRAY
		size_t i;
		for (i = 0; i < words; i++) {
		    if (BU_STR_EQUIV(bu_vls_cstr(&words[i]), bu_vls_cstr(&word))) {
			bu_vls_trunc(&word, 0);
			cnt[i]++;
			break;
		    }
		}
		if (i == words) {
		    /* found a new word */
		    /* bu_log("%zu WORD: %s\n", words, bu_vls_cstr(&word)); */

		    bu_vls_init(&words[words]);
		    bu_vls_strcpy(&words[words], bu_vls_cstr(&word));
		    bu_vls_trunc(&word, 0);
		    cnt[words]++;
		    words++;
		}
#else
		cntptr = (int *)bu_hash_get(hash, wordbytes, wordbyteslen);
		if (cntptr) {
/*		    bu_log("found existing %s\n", (char *)wordbytes); */
		    (*cntptr)++;
		} else {
		    int ret = bu_hash_set(hash, wordbytes, wordbyteslen, &cnt[words]);
/*		    bu_log("adding %s\n", (char *)wordbytes); */
		    if (ret != 1)
 			bu_bomb("totally expected a new entry\n");
		    cnt[words]++;
		    words++;

		}
#endif
		bu_vls_trunc(&word, 0);
  	    }
	}

	/* bu_log("FILE: %s (%zu bytes, %zu words)\n", files[count], data->buflen, words); */

	bu_close_mapped_file(data);
    }

    /* bu_log("FOUND:\n"); */

#if USE_ARRAY
    {
	size_t i;
	for (i = 0; i < words; i++) {
	    /* bu_log("%s => %zu\n", (const char *)bu_vls_cstr(&words[i]), cnt[i]); */
	}
    }
#else
    {
	std::string foo = "bar";
	struct bu_hash_entry *e = bu_hash_next(hash, NULL);
	while (e) {
	    uint8_t *found;
	    size_t foundlen;
	    bu_hash_key(e, &found, &foundlen);
	    /* bu_log("%s => %zu\n", (char *)found, *(size_t *)bu_hash_value(e, NULL)); */
	    e = bu_hash_next(hash, e);
	}
    }
    bu_hash_destroy(hash);
#endif

    return words;
}


int
ged_help_core(struct ged *gedp, int argc, const char *argv[])
{
    char *dir = NULL;
    char **entries = NULL;
    size_t count = 0;
    size_t words = 0;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 1 || !argv)
	return -1;

    /* get our doc dir */
    dir = bu_strdup(bu_dir(NULL, 0, BU_DIR_DOC, NULL));

    /* get recursive list of documentation files */
    count = help_files(dir, &entries);
    bu_log("Found %zu files\n", count);

    /* condense the file into searchable tokens */
    words = help_tokenize(count, (const char **)entries);

    if (words == 0) {
	return GED_ERROR;
    }

    bu_free(dir, "free doc dir");
    bu_argv_free(count, entries);

    return 0;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl help_cmd_impl     = { "help",    ged_help_core, GED_CMD_DEFAULT };
    const struct ged_cmd help_cmd = { &help_cmd_impl };

    struct ged_cmd_impl apropos_cmd_impl  = { "apropos", ged_help_core, GED_CMD_DEFAULT };
    const struct ged_cmd apropos_cmd = { &apropos_cmd_impl };

    struct ged_cmd_impl info_cmd_impl     = { "info",    ged_help_core, GED_CMD_DEFAULT };
    const struct ged_cmd info_cmd = { &info_cmd_impl };

    struct ged_cmd_impl man_cmd_impl      = { "man",     ged_help_core, GED_CMD_DEFAULT };
    const struct ged_cmd man_cmd = { &man_cmd_impl };

    struct ged_cmd_impl question_cmd_impl = { "?",       ged_help_core, GED_CMD_DEFAULT };
    const struct ged_cmd question_cmd = { &question_cmd_impl };

    const struct ged_cmd *help_cmds[] = { &help_cmd,  &apropos_cmd,  &info_cmd,  &man_cmd,  &question_cmd, NULL };

    static const struct ged_plugin pinfo = { GED_API,  help_cmds, 5 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

#ifdef STANDALONE
int main(int ac, char *av[])
{
    struct ged ged = {0};

    bu_setprogname(av[0]);

    GED_INIT(&ged, NULL);
    ged_exec(&ged, ac, (const char **)av);

    return 0;
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
