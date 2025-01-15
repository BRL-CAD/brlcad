/*                            E N V . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2025 United States Government as represented by
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
#include <string.h>

#include "bu.h"

static int
editor_tests(void)
{
    const char *e = NULL;
    const char *eopt = NULL;

    // Unset the EDITOR variable
    bu_setenv("EDITOR", "", 1);

    // First, check that we can find *something* - will select GUI
    // editor first, but should fall back on console editors if
    // GUI not found.
    e = bu_editor(&eopt, 0, 0, NULL);
    if (!e) {
	bu_log("Failed to identify default editor.\n");
	return -1;
    }
    bu_log("Default editor: %s\n", e);

    // Some environments may not have a graphical editor, but we should
    // *always* be able to find a console editor
    e = bu_editor(&eopt, 1, 0, NULL);
    if (!e) {
	bu_log("Failed to identify default console editor.\n");
	return -1;
    }
    bu_log("Default console editor: %s\n", e);

    // Exercise the code path for GUI only, even though NULL is technically OK.
    e = bu_editor(&eopt, 2, 0, NULL);
    if (e)
	bu_log("Default GUI editor: %s\n", e);

    // The only executable we can guarantee at this point is bu_test
    // itself, and its location may differ depending on which platform
    // we built on.  Find it.
    char btest_path[MAXPATHLEN] = {'\0'};
    const char *btest_exec = bu_getprogname();
    bu_strlcpy(btest_path, btest_exec, MAXPATHLEN);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, btest_exec, BU_DIR_EXT, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, btest_exec, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, btest_exec, BU_DIR_EXT, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, "..", "src", "libbu", "tests", btest_exec, NULL);
    if (!bu_file_exists(btest_path, NULL))
	bu_dir(btest_path, MAXPATHLEN, BU_DIR_BIN, "..", "src", "libbu", "tests", btest_exec, BU_DIR_EXT, NULL);
    if (!bu_file_exists(btest_path, NULL)) {
	bu_log("Failed to locate bu_test executable\n");
	return -1;
    }
    bu_log("bu_test executable: %s\n", btest_path);

    // Set the EDITOR variable and exercise the EDITOR code path
    bu_setenv("EDITOR", btest_path, 1);
    e = bu_editor(&eopt, 0, 0, NULL);
    if (!e || !(BU_STR_EQUAL(e, getenv("EDITOR")))) {
	bu_log("Failed to return EDITOR value %s with bu_editor\n", getenv("EDITOR"));
	return -1;
    }
    bu_log("EDITOR value returned with bu_editor: %s\n", btest_path);

    // Unset the EDITOR env var and prepare a list of "editors" to provide.
    bu_setenv("EDITOR", "", 1);
    char *de = bu_strdup(bu_editor(&eopt, 0, 0, NULL));
    const char *elist[4] = {NULL};
    elist[0] = "non-existent-editor1";
    elist[1] = btest_path;
    elist[2] = de;

    // Make sure the list returns the btest_path entry
    e = bu_editor(&eopt, 0, 3, elist);
    if (!BU_STR_EQUAL(e, elist[1])) {
	bu_log("Failed to return list entry %s\n", elist[1]);
	return -1;
    }
    bu_log("Second list entry returned: %s\n", elist[1]);

    // Make sure a non-NULL terminated list doesn't continue on to search defaults
    elist[0] = "non-existent-editor1";
    elist[1] = "non-existent-editor2";
    elist[2] = "non-existent-editor3";
    e = bu_editor(&eopt, 0, 4, elist);
    if (e) {
	bu_log("Failed to stop after checking user supplied entries\n");
	return -1;
    }
    bu_log("As requested, list check skipped libbu internal testing after all entries failed.\n");

    // Check "fall back on internal libbu search" mode
    e = bu_editor(&eopt, 0, 3, elist);
    if (!BU_STR_EQUAL(e, de)) {
	bu_log("After unsuccessful list, failed to fall back to libbu's internal list\n");
	return -1;
    }
    bu_log("As requested, fallback internal libbu check succeeded after list check failed: %s\n", e);

    bu_free(de, "default editor");
    return 0;
}

int
main(int ac, char *av[])
{
    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    if (ac > 1 && BU_STR_EQUAL(av[1], "-e"))
	return editor_tests();

    ssize_t all_mem = bu_mem(BU_MEM_ALL, NULL);
    if (all_mem < 0)
	return -1;
    ssize_t avail_mem = bu_mem(BU_MEM_AVAIL, NULL);
    if (avail_mem < 0)
	return -2;
    ssize_t page_mem = bu_mem(BU_MEM_PAGE_SIZE, NULL);
    if (page_mem < 0)
	return -3;

    /* make sure passing works too */
    size_t all_mem2 = 0;
    size_t avail_mem2 = 0;
    size_t page_mem2 = 0;

    (void)bu_mem(BU_MEM_ALL, &all_mem2);
    if (all_mem2 != (size_t)all_mem)
	return -4;
    (void)bu_mem(BU_MEM_AVAIL, &avail_mem2);
    if (avail_mem2 != (size_t)avail_mem)
	return -5;
    (void)bu_mem(BU_MEM_PAGE_SIZE, &page_mem2);
    if (page_mem2 != (size_t)page_mem)
	return -6;

    char all_buf[6] = {'\0'};
    char avail_buf[6] = {'\0'};
    char p_buf[6] = {'\0'};

    bu_humanize_number(all_buf, 5, all_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
    bu_humanize_number(avail_buf, 5, avail_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);
    bu_humanize_number(p_buf, 5, page_mem, "", BU_HN_AUTOSCALE, BU_HN_B | BU_HN_NOSPACE | BU_HN_DECIMAL);

    bu_log("MEM report: all: %s(%zd) avail: %s(%zd) page_size: %s(%zd)\n",
	   all_buf, all_mem,
	   avail_buf, avail_mem,
	   p_buf, page_mem);

    return 0;
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
