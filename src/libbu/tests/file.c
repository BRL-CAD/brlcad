/*                         F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2024 United States Government as represented by
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
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_WINDOWS_H
#  include <direct.h>
#endif
#include <errno.h>
#include "bu.h"
#include "bio.h"


static void
cleanup(const char *dir, int file_cnt)
{
    struct bu_vls fname = BU_VLS_INIT_ZERO;

    for (int i = 1; i < file_cnt+1; i++) {
	bu_vls_sprintf(&fname, "%s/bu_file_%d", dir, i);
	const char *file = bu_vls_cstr(&fname);

	if (bu_file_exists(file, NULL)) {
	    (void)bu_file_delete(file);
	}

	/* file still exists post-delete */
	if (bu_file_exists(file, NULL)) {
	    bu_exit(1, "[FAIL] test file %s was deleted but still exists\n", file);
	}
    }

    if (bu_file_exists(dir, NULL)) {
	(void)bu_file_delete(dir);
    }

    return;
}


int
main(int ac, char *av[])
{
    int ret = 0;
    FILE *fp = NULL;
    int file_cnt = 1024; /* arbitrarily "a lot" of files */
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_vls fname2 = BU_VLS_INIT_ZERO;
    const char *tdir = "bu_file_test_dir";

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(av[0]);

    if (ac != 1)
	bu_exit(1, "Usage: %s \n", av[0]);

    /* make sure output dir doesn't exist, previous interruption */
    cleanup(tdir, file_cnt);

#ifdef HAVE_WINDOWS_H
    ret = _mkdir(tdir);
#else
    ret = mkdir(tdir, S_IRWXU);
#endif

    /* make sure can create an output directory */
    if (ret != 0 && !bu_file_exists(tdir, NULL)) {
	bu_exit(ret, "%s [FAIL] - could not make \"%s\" directory\n", av[0], tdir);
    }
    if (!bu_file_directory(tdir)) {
	bu_exit(1, "%s [FAIL] %s is not recognized as a directory\n", av[0], tdir);
    }


    /* make sure can create files on open */
    for (int i = 1; i < file_cnt+1; i++) {
	bu_vls_sprintf(&fname, "%s/bu_file_%d", tdir,i);

	/* file does NOT exist pre-fopen() */
	if (bu_file_exists(bu_vls_cstr(&fname), NULL)) {
	    bu_exit(1, "%s [FAIL] test file %s already exists\n", av[0], bu_vls_cstr(&fname));
	}

	/* opening file should create it */
	fp = fopen(bu_vls_cstr(&fname), "wb");
	if (!fp) {
	    bu_exit(1, "%s [FAIL] Unable to create test file %s\n", av[0], bu_vls_cstr(&fname));
	}

	/* file exists post-fopen() */
	if (!bu_file_exists(bu_vls_cstr(&fname), NULL)) {
	    bu_exit(1, "%s [FAIL] test file %s does not exist\n", av[0], bu_vls_cstr(&fname));
	}

	/* make file permissive.
	 *
	 * NOTE: some filesystems like exfat are fixed 777, so we
	 * cannot test non-execution/non-read/non-write without taking
	 * filesystem into consideration.  should be able to make
	 * permissive, however, and test positive case accordingly.
	 */
	ret = bu_fchmod(fileno(fp), 0777);
	if (ret) {
	    perror("bu_fchmod");
	    bu_exit(1, "%s [FAIL] test file %s threw an fchmod() error\n", av[0], bu_vls_cstr(&fname));
	}

	/* write something to it */
	fprintf(fp, "%d", i);
	fclose(fp);

	/* file exists post-fclose() */
	if (!bu_file_exists(bu_vls_cstr(&fname), NULL)) {
	    bu_exit(1, "%s [FAIL] test file %s does not exist\n", av[0], bu_vls_cstr(&fname));
	}
    }

    /* Do some tests on the files */
    for (int i = 1; i < file_cnt+1; i++) {

	/* NOTE: some filesystems (e.g., exfat) are ONLY permissible
	 * (e.g., 777), so we can only trigger on not being readable,
	 * not writeable, not executable after running bu_fchmod(777).
	 */

	bu_vls_sprintf(&fname, "%s/bu_file_%d", tdir,i);
	if (!bu_file_readable(bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] test file %s not readable after bu_fchmod(777)\n", av[0], bu_vls_cstr(&fname));
	}
	if (!bu_file_writable(bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] test file %s not readable after bu_fchmod(777)\n", av[0], bu_vls_cstr(&fname));
	}
	if (!bu_file_executable(bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] test file %s not executable after bu_fchmod(777)\n", av[0], bu_vls_cstr(&fname));
	}
    }

    bu_vls_sprintf(&fname, "%s/bu_file_1", tdir);
    bu_vls_sprintf(&fname2, "%s/bu_file_2", tdir);

    if (bu_file_same(bu_vls_cstr(&fname), bu_vls_cstr(&fname2))) {
	bu_exit(1, "%s [FAIL] test input files %s and %s are incorrectly reported to be the same file\n", av[0], bu_vls_cstr(&fname), bu_vls_cstr(&fname2));
    }

    if (bu_file_directory(bu_vls_cstr(&fname))) {
	bu_exit(1, "%s [FAIL] %s is incorrectly identified as being a directory\n", av[0], bu_vls_cstr(&fname));
    }

    if (bu_file_symbolic(bu_vls_cstr(&fname))) {
	bu_exit(1, "%s [FAIL] %s is incorrectly identified as being a symbolic link\n", av[0], bu_vls_cstr(&fname));
    }

    /* file list */
    {
	const char *pattern = "bu_file_*";
	char **lfiles = NULL;
	size_t count = bu_file_list(tdir, pattern, &lfiles);
	if (count != (size_t)file_cnt) {
	    bu_exit(1, "%s [FAIL] %s should contain %d files, found %zd\n", av[0], tdir, file_cnt, count);
	}
	/* bu_file_list doesn't guarantee ordering, but make sure each lfiles entry matches the pattern */
	for (size_t c = 0; c < count; c++) {
	    if (bu_path_match(pattern, lfiles[c], 0)) {
		bu_exit(1, "%s [FAIL] file array entry %zd (%s) doesn't match the bu file test pattern \"%s\"\n", av[0], c, lfiles[c], pattern);
	    }
	}
	if (lfiles) {
	    bu_argv_free(count, lfiles);
	}
    }

    /* realpath */
    {
	char *rpath = bu_file_realpath(bu_vls_cstr(&fname), NULL);
	if (BU_STR_EQUAL(rpath, bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] path %s was not successfully expanded by bu_file_realpath (got %s)\n", av[0], bu_vls_cstr(&fname), rpath);
	}
	bu_free(rpath, "free realpath");
    }

    /* Clean up, delete all our files and dir */
    cleanup(tdir, file_cnt);

    bu_vls_free(&fname);
    bu_vls_free(&fname2);

    return ret;
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
