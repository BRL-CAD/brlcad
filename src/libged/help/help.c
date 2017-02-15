/*                         H E L P . C
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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


HIDDEN void
help_tokenize(size_t count, const char **files)
{
    size_t bytes;
    size_t zeros;
    struct bu_mapped_file *data;

#define MAX_WORDS 81920
    struct bu_vls words[MAX_WORDS] = {{0}};
    size_t cnt[MAX_WORDS] = {0};
    size_t indexed = 0;

    while (count-- > 0) {
	struct bu_vls word = BU_VLS_INIT_ZERO;

	/* lotta words? leave an empty spot */
	if (indexed+1 > MAX_WORDS-1) {
	    continue;
	}

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
	bu_vls_trunc(&word, 0);
	for (bytes = 0; bytes < data->buflen && indexed+1 <= MAX_WORDS-1; bytes++) {
	    int c = ((const char *)data->buf)[bytes];
	    if (isalnum(c)) {
		bu_vls_putc(&word, c);
	    } else if (bu_vls_strlen(&word) > 0) {
		size_t i;
		for (i = 0; i < indexed; i++) {
		    if (BU_STR_EQUIV(bu_vls_cstr(&words[i]), bu_vls_cstr(&word))) {
			bu_vls_trunc(&word, 0);
			cnt[i]++;
			break;
		    }
		}
		if (i == indexed) {
		    /* found a new word */
		    /* bu_log("%zu WORD: %s\n", indexed, bu_vls_cstr(&word)); */

		    bu_vls_init(&words[indexed]);
		    bu_vls_strcpy(&words[indexed], bu_vls_cstr(&word));
		    bu_vls_trunc(&word, 0);
		    cnt[indexed]++;
		    indexed++;
		}
	    }
	}

	bu_log("FILE: %s (%zu bytes, %zu words)\n", files[count], data->buflen, indexed);

	bu_close_mapped_file(data);
    }
}


int
ged_help(struct ged *gedp, int argc, const char *argv[])
{
    char *dir;
    char **entries;
    size_t count;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 1 || !argv)
	return -1;

    /* get our doc dir */
    dir = bu_strdup(bu_brlcad_dir("doc", 0));

    /* get recursive list of documentation files */
    count = help_files(dir, &entries);
    bu_log("Found %zu files\n", count);

    /* condense the file into searchable tokens */
    help_tokenize(count, (const char **)entries);

    bu_free(dir, "free doc dir");
    bu_argv_free(count, entries);

    return 0;
}


HIDDEN int
help_load(struct ged *gedp)
{
    int ret = 0;
    HIDDEN struct bu_list *help_cmd(void);
    struct bu_list *hp = help_cmd();
    struct ged_cmd *cmd;

    for (BU_LIST_FOR(cmd, ged_cmd, hp)) {
	ret += gedp->add(gedp, cmd);
    }

    BU_PUT(hp, struct bu_list);

    return ret;
}


HIDDEN void
help_unload(struct ged *gedp)
{
    gedp->del(gedp, "help");
    gedp->del(gedp, "apropos");
    gedp->del(gedp, "info");
    gedp->del(gedp, "man");
    gedp->del(gedp, "?");
}


HIDDEN struct bu_list *
help_cmd(void)
{
    struct bu_list *hp;

    static struct ged_cmd cmd[6] = {
	{
	    BU_LIST_INIT_ZERO, "help",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "apropos",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "info",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "man",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, "?",
	    "the BRL-CAD help system",
	    "help",
	    &help_load, &help_unload, &ged_help
	}, {
	    BU_LIST_INIT_ZERO, NULL, {0}, NULL, NULL, NULL, NULL
	}
    };

    BU_GET(hp, struct bu_list);
    BU_LIST_INIT(hp);

    BU_LIST_PUSH(hp, &(cmd[0].l));
    BU_LIST_PUSH(hp, &(cmd[1].l));
    BU_LIST_PUSH(hp, &(cmd[2].l));
    BU_LIST_PUSH(hp, &(cmd[3].l));
    BU_LIST_PUSH(hp, &(cmd[4].l));

    return hp;
}


#ifdef STANDALONE
int main(int ac, char *av[])
{
    struct ged ged = {0};

    GED_INIT(&ged, NULL);

    help_load(&ged);
    ged_help(&ged, ac, (const char **)av);
    help_unload(&ged);

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
