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

#include <string.h>

#include "bio.h"

#include "bu.h"

static void
shrink_path(struct bu_vls *tp, const char *lp)
{
    if (!tp || !lp)
	return;
    bu_vls_sprintf(tp, "%s", lp);
#ifdef HAVE_WINDOWS_H
    char sp[MAXPATHLEN];
    char *llp = bu_strdup(lp);
    DWORD r = GetShortPathNameA(llp, sp, MAXPATHLEN);
    bu_log("r: %d  sp:%s llp: %s\n", r, sp, llp);
    if (r != 0 && r < MAXPATHLEN) {
	// Unless short path call failed, use sp
	bu_vls_sprintf(tp, "%s", sp);
    }
    bu_free(llp, "llp");
    bu_log("%s\n", bu_vls_cstr(tp));
#endif
}

static int
editor_tests(void)
{
    const char *e = NULL;
    struct bu_ptbl eopts = BU_PTBL_INIT_ZERO;

    // Unset the EDITOR variable
    bu_setenv("EDITOR", "", 1);

    // First, check that we can find *something* - will select GUI
    // editor first, but should fall back on console editors if
    // GUI not found.
    e = bu_editor(&eopts, 0, 0, NULL);
    if (!e) {
	bu_log("Failed to identify default editor.\n");
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("Default editor: %s\n", e);

    // Some environments may not have a graphical editor, but we should
    // *always* be able to find a console editor
    e = bu_editor(&eopts, 1, 0, NULL);
    if (!e) {
	bu_log("Failed to identify default console editor.\n");
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("Default console editor: %s\n", e);

    // Exercise the code path for GUI only, even though NULL is technically OK.
    e = bu_editor(&eopts, 2, 0, NULL);
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
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("bu_test executable: %s\n", btest_path);

    // Set the EDITOR variable and exercise the EDITOR code path
    bu_setenv("EDITOR", btest_path, 1);

    // bu_editor outputs may be processed to produce more compact paths on on
    // some platforms - we'll need to replicate that with our test paths to be
    // able to do correct comparisons.
    struct bu_vls check_path = BU_VLS_INIT_ZERO;
    shrink_path(&check_path, getenv("EDITOR"));

    e = bu_editor(&eopts, 0, 0, NULL);
    if (!e) {
	bu_log("EDITOR value %s did not produce an editor path\n", getenv("EDITOR"));
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    if (!(BU_STR_EQUAL(e, bu_vls_cstr(&check_path)))) {
	bu_log("Failed to return EDITOR value %s with bu_editor\n", bu_vls_cstr(&check_path));
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("EDITOR value returned with bu_editor: %s\n", e);


    // We want to make sure the editor option setting is working, but to do so we
    // need to trigger a case we know should produce an option - most cases will not.
    // To make this work, we need a valid path on the filesystem with the "emacs"
    // name, and since there's no guarantee the user has emacs installed we make
    // a file in the same location as bu_test to use as a target.  We then use EDITOR
    // to look for that file specifically.
    struct bu_vls bp = BU_VLS_INIT_ZERO;
    if (bu_path_component(&bp , btest_path, BU_PATH_DIRNAME)) {
	char epath[MAXPATHLEN] = {'\0'};
	bu_dir(epath, MAXPATHLEN, bu_vls_cstr(&bp), "emacs", NULL);
	FILE *fp = fopen(epath, "w");
	fprintf(fp, "BRL-CAD");
	fclose(fp);
	bu_setenv("EDITOR", epath, 1);
	e = bu_editor(&eopts, 1, 0, NULL);
	bu_file_delete(epath);
	if (!e) {
	    bu_log("EDITOR value %s did not produce an editor path\n", getenv("EDITOR"));
	    bu_vls_free(&check_path);
	    bu_ptbl_free(&eopts);
	    bu_vls_free(&bp);
	    return -1;
	}
	if (!BU_PTBL_LEN(&eopts)) {
	    bu_log("Expected editor option, but no option returned\n");
	    bu_vls_free(&check_path);
	    bu_ptbl_free(&eopts);
	    bu_vls_free(&bp);
	    return -1;
	}
	const char *emacs_opt = (const char *)BU_PTBL_GET(&eopts, 0);
	if (!(BU_STR_EQUAL(emacs_opt, "-nw"))) {
	    bu_log("Failed to return EDITOR value %s with bu_editor\n", bu_vls_cstr(&check_path));
	    bu_vls_free(&check_path);
	    bu_ptbl_free(&eopts);
	    bu_vls_free(&bp);
	    return -1;
	}
	bu_log("Console mode lookup for %s returned with editor option: %s\n", e, emacs_opt);
    }
    bu_vls_free(&bp);


    // Unset the EDITOR env var and prepare a list of "editors" to provide.
    bu_setenv("EDITOR", "", 1);
    char *de = bu_strdup(bu_editor(&eopts, 0, 0, NULL));
    const char *elist[4] = {NULL};
    elist[0] = "non-existent-editor1";
    elist[1] = btest_path;
    elist[2] = de;

    // Make sure the list returns the btest_path entry
    e = bu_editor(&eopts, 0, 3, elist);
    shrink_path(&check_path, elist[1]);
    if (!BU_STR_EQUAL(e, bu_vls_cstr(&check_path))) {
	bu_log("Failed to return list entry %s\n", elist[1]);
	bu_free(de, "default editor");
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("Second list entry returned: %s\n", elist[1]);

    // Make sure a non-NULL terminated list doesn't continue on to search defaults
    elist[0] = "non-existent-editor1";
    elist[1] = "non-existent-editor2";
    elist[2] = "non-existent-editor3";
    e = bu_editor(&eopts, 0, 4, elist);
    if (e) {
	bu_log("Failed to stop after checking user supplied entries\n");
	bu_free(de, "default editor");
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("As requested, list check skipped libbu internal testing after all entries failed.\n");

    // Check "fall back on internal libbu search" mode
    e = bu_editor(&eopts, 0, 3, elist);
    if (!BU_STR_EQUAL(e, de)) {
	bu_log("After unsuccessful list, failed to fall back to libbu's internal list\n");
	bu_free(de, "default editor");
	bu_vls_free(&check_path);
	bu_ptbl_free(&eopts);
	return -1;
    }
    bu_log("As requested, fallback internal libbu check succeeded after list check failed: %s\n", e);

    bu_free(de, "default editor");
    bu_vls_free(&check_path);
    bu_ptbl_free(&eopts);
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
