/*                          D I R . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "bu.h"


int
main(int argc, char *argv[])
{
    bu_dir_t type;
    int ret;
    int failures = 0;
    const char *cpath;
    const char *initp;
    const char *currp;
    char path[MAXPATHLEN] = {0};

    if (BU_STR_EQUAL(bu_path_basename(argv[0], path), "bu_test"))
	argc--, argv++;

    if (argc != 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    /* basic test, get the current directory */
    memset(path, 0, MAXPATHLEN);
    currp = bu_dir(NULL, 0, BU_DIR_CURR, NULL);
    if (!getcwd(path, MAXPATHLEN)) {
	failures++;
    } else {
	if (!BU_STR_EQUAL(path, currp)) {
	    failures++;
	}
    }

    /* each directory lookup should return something */
    for (type = BU_DIR_CURR; type != BU_DIR_UNKNOWN; type++) {
	memset(path, 0, MAXPATHLEN);

	if (type == BU_DIR_EXT || type == BU_DIR_LIBEXT)
	    continue;

	bu_dir(path, MAXPATHLEN, type, NULL);
	if (BU_STR_EMPTY(path))
	    failures++;
    }

    /* test absolute composition */
    if (bu_file_exists("/", NULL)) {
	cpath = bu_dir(NULL, 0, "/", NULL);
	if (!BU_STR_EQUAL(cpath, "/"))
	    failures++;
    } else if (bu_file_exists("C:\\", NULL)) {
	cpath = bu_dir(NULL, 0, "C:\\", NULL);
	if (!BU_STR_EQUAL(cpath, "C:\\"))
	    failures++;
    }

    /* slightly more complex bindir composition */
    if (bu_file_exists("/bin/sh", NULL)) {
	cpath = bu_dir(NULL, 0, "/", "bin", "sh", NULL);
	if (!BU_STR_EQUAL(cpath, "/bin/sh"))
	    failures++;
	cpath = bu_dir(NULL, 0, "/", "bin", "sh", BU_DIR_EXT, NULL);
	if (!BU_STR_EQUAL(cpath, "/bin/sh"))
	    failures++;
    } else if (bu_file_exists("C:\\Windows\\System32\\cmd.exe", NULL)) {
	cpath = bu_dir(NULL, 0, "C:\\", "Windows", "System32", "cmd.exe", NULL);
	if (!BU_STR_EQUAL(cpath, "C:\\Windows\\System32\\cmd.exe"))
	    failures++;
	cpath = bu_dir(NULL, 0, "C:\\", "Windows", "System32", "cmd", BU_DIR_EXT, NULL);
	if (!BU_STR_EQUAL(cpath, "C:\\Windows\\System32\\cmd.exe"))
	    failures++;
    }

    /* current dir <> init dir */
    initp = bu_dir(NULL, 0, BU_DIR_INIT, NULL);
    if (!BU_STR_EQUAL(currp, initp))
	failures++;
    cpath = bu_dir(NULL, 0, currp, "..", NULL);
    ret = chdir(cpath);
    if (ret != 0) {
	failures++;
    }
    currp = bu_dir(NULL, 0, BU_DIR_CURR, NULL);
    if (BU_STR_EQUAL(currp, initp))
	failures++;
    ret = chdir(initp);
    if (ret != 0) {
	failures++;
    }
    currp = bu_dir(NULL, 0, BU_DIR_CURR, NULL);
    if (!BU_STR_EQUAL(currp, initp))
	failures++;

    /* bindir */
    /* TODO: need something to test, but nada installed yet */

    /* libdir and library extension expansion */
    cpath = bu_dir(NULL, 0, BU_DIR_LIB, NULL);
    {
	size_t i;
	const char *extensions[4] = {".so", ".dylib", ".dll", NULL};
	for (i = 0; i < 4; i++) {
	    snprintf(path, MAXPATHLEN, "%s/libbu%s", cpath, extensions[i]);
	    if (bu_file_exists(path, NULL)) {
		char lpath[MAXPATHLEN] = {0};
		bu_dir(lpath, MAXPATHLEN, BU_DIR_LIB, "libbu", BU_DIR_LIBEXT, NULL);
		if (!BU_STR_EQUAL(path, lpath))
		    failures++;
		memset(lpath, 0, MAXPATHLEN);
		bu_dir(lpath, MAXPATHLEN, cpath, "libbu", BU_DIR_LIBEXT, NULL);
		if (!BU_STR_EQUAL(path, lpath))
		    failures++;
	    }
	}
    }

    /* libexecdir */
    /* TODO: need something to test, but nada installed yet */

    /* includedir */
    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_INCLUDE, "conf", NULL);
    if (BU_STR_EMPTY(path))
	failures++;
    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_INCLUDE, "conf", "MAJOR", NULL);
    if (BU_STR_EMPTY(path))
	failures++;

    /* datadir */
    /* TODO: need something to test, but nada installed yet */

    /* docdir */
    /* TODO: need something to test, but nada installed yet */

    /* mandir */
    /* TODO: need something to test, but nada installed yet */

    /* tempdir */

    /* homedir */

    /* cachedir */

    /* configdir */

    /* input edge cases */
    if (bu_dir(NULL, 0, NULL) != NULL)
	failures++;
    if (bu_dir(NULL, MAXPATHLEN, NULL) != NULL)
	failures++;
    memset(path, 0, MAXPATHLEN);
    if (bu_dir(path, 0, NULL) != NULL)
	failures++;
    memset(path, 0, MAXPATHLEN);
    if (!BU_STR_EMPTY(bu_dir(path, MAXPATHLEN, NULL)))
	failures++;
    memset(path, 0, MAXPATHLEN);
    if (!BU_STR_EMPTY(bu_dir(path, 1, ".", NULL)))
	failures++;
    memset(path, 0, MAXPATHLEN);
    if (BU_STR_EMPTY(bu_dir(path, 2, ".", NULL)))
	failures++;
    memset(path, 0, MAXPATHLEN);
    if (!BU_STR_EMPTY(bu_dir(path, 1, "/", NULL)))
	failures++;
    memset(path, 0, MAXPATHLEN);
    if (BU_STR_EMPTY(bu_dir(path, 2, "/", NULL)))
	failures++;

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
