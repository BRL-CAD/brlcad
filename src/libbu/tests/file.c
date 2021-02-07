/*                         F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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

int
main(int ac, char *av[])
{
    int ret = 0;
    FILE *fp = NULL;
    int file_cnt = 2;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_vls fname2 = BU_VLS_INIT_ZERO;
    const char *tdir = "bu_file_test_dir";

    bu_setprogname(av[0]);

    if (ac != 1)
	bu_exit(1, "Usage: %s \n", av[0]);

#ifdef HAVE_WINDOWS_H
    ret = _mkdir(tdir);
#else
    ret = mkdir(tdir, S_IRWXU);
#endif
#if 0
    if (ret == -1) {
	bu_log("%s [FAIL] - could not create \"%s\" directory\n", av[0], tdir);
	return ret;
    }
#endif

    for (int i = 1; i < file_cnt+1; i++) {
	bu_vls_sprintf(&fname, "%s/bu_file_%d", tdir,i);
	fp = fopen(bu_vls_cstr(&fname), "wb");
	if (!fp) {
	    bu_exit(1, "%s [FAIL] Unable to create test input file %s\n", av[0], bu_vls_cstr(&fname));
	}
	fprintf(fp, "%d", i);
	fclose(fp);
    }

    /* Do some tests on the files */
    for (int i = 1; i < file_cnt+1; i++) {
	bu_vls_sprintf(&fname, "%s/bu_file_%d", tdir,i);
	if (!bu_file_exists(bu_vls_cstr(&fname), NULL)) {
	    bu_exit(1, "%s [FAIL] test input file %s does not exist\n", av[0], bu_vls_cstr(&fname));
	}
	if (!bu_file_readable(bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] test input file %s is not readable\n", av[0], bu_vls_cstr(&fname));
	}
	if (!bu_file_writable(bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] test input file %s is not readable\n", av[0], bu_vls_cstr(&fname));
	}
	if (bu_file_executable(bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] test input file %s is incorrectly reported to be executable\n", av[0], bu_vls_cstr(&fname));
	}
    }

    bu_vls_sprintf(&fname, "%s/bu_file_1", tdir);
    bu_vls_sprintf(&fname2, "%s/bu_file_2", tdir);

    if (bu_file_same(bu_vls_cstr(&fname), bu_vls_cstr(&fname2))) {
	bu_exit(1, "%s [FAIL] test input files %s and %s are incorrectly reported to be the same file\n", av[0], bu_vls_cstr(&fname), bu_vls_cstr(&fname2));
    }

    if (!bu_file_directory(tdir)) {
	bu_exit(1, "%s [FAIL] %s is not recognized as being a directory\n", av[0], tdir);
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

    /* Clean up */

    for (int i = 1; i < file_cnt+1; i++) {
	bu_vls_sprintf(&fname, "%s/bu_file_%d", tdir,i);
	if (!bu_file_delete(bu_vls_cstr(&fname))) {
	    bu_exit(1, "%s [FAIL] could not delete file %s\n", av[0], bu_vls_cstr(&fname));
	}
    }

    if (!bu_file_delete(tdir)) {
	bu_exit(1, "%s [FAIL] could not delete directory %s\n", av[0], tdir);
    }

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
