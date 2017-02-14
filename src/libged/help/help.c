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

#include "ged.h"


int
ged_help(struct ged *gedp, int argc, const char *argv[])
{
    char *dir;
    size_t i, count, files_count, dir_count;
    char **entries = NULL;
    char **files;
    char **dirs;
    size_t max_count = 2048;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 1 || !argv)
	return -1;

    count = 0;
    entries = bu_calloc(sizeof(char *), max_count, "allocate entries");
    dirs = bu_calloc(sizeof(char *), max_count, "allocate dirs");

    dir = bu_strdup(bu_brlcad_dir("doc", 0));

    dirs[0] = dir;
    dir_count = 1;
    files_count = 0;

    while (dir_count-- > 0) {
	char *curdir = dirs[dir_count];

	bu_log("Processing %s\n", curdir);
	files_count = bu_file_list(curdir, NULL, &files);

	for (i = 0; i < files_count; i++) {
	    struct bu_vls filepath = BU_VLS_INIT_ZERO;

	    /* skip the canonical dirs */
	    if (BU_STR_EQUIV(files[i], ".") || BU_STR_EQUIV(files[i], ".."))
	    {
		continue;
	    }

	    bu_vls_sprintf(&filepath, "%s%c%s", curdir, BU_DIR_SEPARATOR, files[i]);

	    if (!bu_file_directory(bu_vls_cstr(&filepath))) {
		bu_log("FILE: %s\n", bu_vls_cstr(&filepath));
		if (count > max_count-1) {
		    max_count *= 2;
		    entries = bu_realloc(entries, sizeof(char *) * max_count, "increase entries");
		}
		entries[count++] = bu_vls_strdup(&filepath);
	    } else {
		bu_log(" DIR: %s\n", bu_vls_cstr(&filepath));
		dirs[dir_count] = bu_strdup(bu_vls_cstr(&filepath));
		dir_count++;
	    }

	    bu_vls_free(&filepath);
	}

	bu_free(curdir, "free curdir");
    }

    bu_argv_free(files_count, files);
    bu_argv_free(count, entries);
    bu_free(dir, "free doc dir");

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

#if 0
    BU_LIST_MAGIC_SET(&(cmd[0].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[1].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[2].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[3].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[4].l), GED_CMD_MAGIC);
    BU_LIST_MAGIC_SET(&(cmd[5].l), GED_CMD_MAGIC);
#endif

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
