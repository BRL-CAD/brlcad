/*                      T E S T _ D I R . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#ifdef HAVE_WINDOWS_H
#  include <direct.h>
#endif

#include "bu.h"


#define PRINT(type, left, cmp, right) bu_log("bu_dir(%s) \"%s\" " CPP_STR(cmp) " \"%s\" ", enum2str[type], left, right)

/* private mapping from enum to string */
static const char *enum2str[BU_DIR_END+1] = {0};


static void
check(int test, int *count)
{
    if (test) {
	bu_log("[PASSED]\n");
    } else {
	bu_log("[FAILED]\n");
	(*count)++;
    }
}


static void
test_writable(bu_dir_t type, const char *ipath, int *count)
{
    size_t i;
    FILE *fp;
    char path[MAXPATHLEN] = {0};
    char file[MAXPATHLEN] = {0};
    const char *cpath = NULL;

    PRINT(type, ipath, is writable, "?");

    /* get a random file name for writable directory testing */
    bu_strlcat(file, "bu_dir_", MAXPATHLEN);
    srand((unsigned)(bu_gettime() % UINT_MAX));
    for (i = 0; i < 8; i++) {
	char randchar[2] = {0};
	randchar[0] = 'a' + (rand() % 26);
	bu_strlcat(file, randchar, MAXPATHLEN);
    }
    bu_strlcat(file, ".txt", MAXPATHLEN);

    /* construct full path to test file */
    bu_strlcat(path, ipath, MAXPATHLEN);
    {
	char separator[2] = {0};
	separator[0] = BU_DIR_SEPARATOR;
	bu_strlcat(path, separator, MAXPATHLEN);
    }
    bu_strlcat(path, file, MAXPATHLEN);

    /* make sure nothing in the way */
    bu_file_delete(path);
    if (bu_file_exists(path, NULL)) {
	bu_log("[ERROR]\nfile [%s] is in the way, could not be deleted\n", path);
	(*count)++;
	return;
    }

    /* see if we can write it! */
    fp = fopen(path, "w");
    fclose(fp);
    check(bu_file_exists(path, NULL), count);
    bu_file_delete(path);

    /* get and write the test file (again) */
    cpath = bu_dir(NULL, 0, type, file, NULL);
    PRINT(type, cpath, is writable, "?");
    if (!BU_STR_EMPTY(cpath)) {
	fp = fopen(cpath, "w");
	fclose(fp);
    }
    check(bu_file_exists(cpath, NULL), count);
    bu_file_delete(cpath);

    /* sanity */
    PRINT(type, path, ==, cpath);
    check(BU_STR_EQUAL(path, cpath), count);

    bu_log("tested %s\n", file);
}


int
main(int argc, char *argv[])
{
    bu_dir_t type;
    size_t i;
    int ret;
    int failures = 0;
    const char *cpath;
    char initp[MAXPATHLEN] = {0};
    char currp[MAXPATHLEN] = {0};
    char workp[MAXPATHLEN] = {0};
    char lpath[MAXPATHLEN] = {0};
    char path[MAXPATHLEN] = {0};

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    /* explicit init to proof against enum reordering */
    enum2str[BU_DIR_CURR] = CPP_STR(BU_DIR_CURR);
    enum2str[BU_DIR_INIT] = CPP_STR(BU_DIR_INIT);
    enum2str[BU_DIR_BIN] = CPP_STR(BU_DIR_BIN);
    enum2str[BU_DIR_LIB] = CPP_STR(BU_DIR_LIB);
    enum2str[BU_DIR_LIBEXEC] = CPP_STR(BU_DIR_LIBEXEC);
    enum2str[BU_DIR_INCLUDE] = CPP_STR(BU_DIR_INCLUDE);
    enum2str[BU_DIR_DATA] = CPP_STR(BU_DIR_DATA);
    enum2str[BU_DIR_DOC] = CPP_STR(BU_DIR_DOC);
    enum2str[BU_DIR_MAN] = CPP_STR(BU_DIR_MAN);
    enum2str[BU_DIR_TEMP] = CPP_STR(BU_DIR_TEMP);
    enum2str[BU_DIR_HOME] = CPP_STR(BU_DIR_HOME);
    enum2str[BU_DIR_CACHE] = CPP_STR(BU_DIR_CACHE);
    enum2str[BU_DIR_CONFIG] = CPP_STR(BU_DIR_CONFIG);
    enum2str[BU_DIR_EXT] = CPP_STR(BU_DIR_EXT);
    enum2str[BU_DIR_LIBEXT] = CPP_STR(BU_DIR_LIBEXT);
    enum2str[BU_DIR_END] = CPP_STR(BU_DIR_END);

    if (argc != 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    /* basic test, get the current directory */
#ifdef HAVE_GETCWD
    memset(path, 0, MAXPATHLEN);
    bu_dir(currp, MAXPATHLEN, BU_DIR_CURR, NULL);
    cpath = getcwd(path, MAXPATHLEN);
    if (cpath == NULL) {
	perror("ERROR, getcwd");
	failures++;
    }
    PRINT(BU_DIR_CURR, path, ==, currp);
    check(BU_STR_EQUAL(path, currp), &failures);
#endif

    /* each directory lookup should return something */
    for (type = BU_DIR_CURR; type != BU_DIR_END; type = (bu_dir_t)(type + 1)) {
	int ok = 0;

	memset(path, 0, MAXPATHLEN);

	if (type == BU_DIR_EXT || type == BU_DIR_LIBEXT)
	    continue;

	bu_dir(path, MAXPATHLEN, type, NULL);
	if (type == BU_DIR_BIN) {
	    ok = BU_STR_EMPTY(path) || bu_file_directory(path) || bu_file_exists(path, NULL);
	    PRINT(type, path, resolves or is empty in an uninstalled tree, "");
	} else {
	    ok = !BU_STR_EMPTY(path);
	    PRINT(type, path, !=, "");
	}
	check(ok, &failures);
    }

    /* test absolute composition */
    if (bu_file_exists("/", NULL)) {
	cpath = bu_dir(NULL, 0, "/", NULL);
	PRINT(BU_DIR_END, cpath, ==, "/");
	check(BU_STR_EQUAL(cpath, "/"), &failures);
    } else if (bu_file_exists("C:\\", NULL)) {
	cpath = bu_dir(NULL, 0, "C:\\", NULL);
	PRINT(BU_DIR_END, cpath, ==, "C:\\");
	check(BU_STR_EQUAL(cpath, "C:\\"), &failures);
    }

    /* slightly more complex bindir composition */
    if (bu_file_exists("/bin/sh", NULL)) {
	cpath = bu_dir(NULL, 0, "/", "bin", "sh", NULL);
	PRINT(BU_DIR_END, cpath, ==, "/bin/sh");
	check(BU_STR_EQUAL(cpath, "/bin/sh"), &failures);

	cpath = bu_dir(NULL, 0, "/", "bin", "sh", BU_DIR_EXT, NULL);
	PRINT(BU_DIR_EXT, cpath, ==, "/bin/sh");
	check(BU_STR_EQUAL(cpath, "/bin/sh"), &failures);
    } else if (bu_file_exists("C:\\Windows\\System32\\cmd.exe", NULL)) {
	cpath = bu_dir(NULL, 0, "C:\\", "Windows", "System32", "cmd.exe", NULL);
	PRINT(BU_DIR_END, cpath, ==, "C:\\Windows\\System32\\cmd.exe");
	check(BU_STR_EQUAL(cpath, "C:\\Windows\\System32\\cmd.exe"), &failures);

	cpath = bu_dir(NULL, 0, "C:\\", "Windows", "System32", "cmd", BU_DIR_EXT, NULL);
	PRINT(BU_DIR_EXT, cpath, ==, "C:\\Windows\\System32\\cmd.exe");
	check(BU_STR_EQUAL(cpath, "C:\\Windows\\System32\\cmd.exe"), &failures);
    }

    /* current dir <> init dir */
    bu_dir(initp, MAXPATHLEN, BU_DIR_INIT, NULL);
    PRINT(BU_DIR_INIT, currp, !empty && ==, initp);
    check(!BU_STR_EMPTY(currp) && BU_STR_EQUAL(currp, initp), &failures);
    cpath = bu_dir(NULL, 0, currp, "..", NULL);
    ret = chdir(cpath);
    if (ret != 0) {
	perror("ERROR, chdir");
	failures++;
    }
    bu_dir(workp, MAXPATHLEN, BU_DIR_CURR, NULL);
    PRINT(BU_DIR_CURR, workp, !empty && !=, initp);
    check(!BU_STR_EMPTY(workp) && !BU_STR_EQUAL(workp, initp), &failures);
    ret = chdir(initp);
    if (ret != 0) {
	perror("ERROR, chdir");
	failures++;
    }
    bu_dir(currp, MAXPATHLEN, BU_DIR_CURR, NULL);
    PRINT(BU_DIR_CURR, currp, !empty && ==, initp);
    check(!BU_STR_EMPTY(currp) && BU_STR_EQUAL(currp, initp), &failures);

    /* bindir */
    /* TODO: need something to test, but nada installed yet */

    /* libdir and library extension expansion */
    cpath = bu_dir(NULL, 0, BU_DIR_LIB, NULL);
    {
	const char *extensions[4] = {".so", ".dylib", ".dll", NULL};
	char libdir[MAXPATHLEN] = {0};

	bu_strlcpy(libdir, cpath, MAXPATHLEN);
	for (i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
	    snprintf(path, MAXPATHLEN, "%s/libbu%s", libdir, extensions[i]);
	    if (bu_file_exists(path, NULL)) {
		bu_dir(lpath, MAXPATHLEN, BU_DIR_LIB, "libbu", BU_DIR_LIBEXT, NULL);
		PRINT(BU_DIR_LIBEXT, path, ==, lpath);
		check(BU_STR_EQUAL(path, lpath), &failures);

		memset(lpath, 0, MAXPATHLEN);
		bu_dir(lpath, MAXPATHLEN, libdir, "libbu", BU_DIR_LIBEXT, NULL);
		PRINT(BU_DIR_LIBEXT, path, ==, lpath);
		check(BU_STR_EQUAL(path, lpath), &failures);
	    }
	}
    }

    /* libexecdir */
    /* TODO: need something to test, but nada installed yet */

    /* includedir */
    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_INCLUDE, "conf", NULL);
    PRINT(BU_DIR_INCLUDE, path, !=, "");
    check(!BU_STR_EMPTY(path), &failures);

    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_INCLUDE, "conf", "MAJOR", NULL);
    PRINT(BU_DIR_INCLUDE, path, !=, "");
    check(!BU_STR_EMPTY(path), &failures);

    /* datadir */
    /* TODO: need something to test, but nada installed yet */

    /* docdir */
    /* TODO: need something to test, but nada installed yet */

    /* mandir */
    /* TODO: need something to test, but nada installed yet */

    /* tempdir, should be writable */
    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_TEMP, NULL);
    if (!BU_STR_EMPTY(path)) {
	test_writable(BU_DIR_TEMP, path, &failures);
    } else {
	bu_log("ERROR: TEMP directory could not be found\n");
	failures++;
    }

    /* homedir */
    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_HOME, NULL);
    if (!BU_STR_EMPTY(path)) {
	test_writable(BU_DIR_HOME, path, &failures);
    } else {
	bu_log("ERROR: HOME directory could not be found\n");
	failures++;
    }

    /* cachedir */
    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_CACHE, NULL);
    if (!BU_STR_EMPTY(path)) {
	test_writable(BU_DIR_CACHE, path, &failures);
    } else {
	bu_log("ERROR: CACHE directory could not be found\n");
	failures++;
    }

    /* configdir */
    memset(path, 0, MAXPATHLEN);
    bu_dir(path, MAXPATHLEN, BU_DIR_CONFIG, NULL);
    if (!BU_STR_EMPTY(path)) {
	test_writable(BU_DIR_CONFIG, path, &failures);
    } else {
	bu_log("ERROR: CONFIG directory could not be found\n");
	failures++;
    }

    /* input edge cases */
    cpath = bu_dir(NULL, 0, NULL);
    PRINT(BU_DIR_END, cpath, ==, "");
    check(cpath != NULL && BU_STR_EMPTY(cpath), &failures);

    cpath = bu_dir(NULL, MAXPATHLEN, NULL);
    PRINT(BU_DIR_END, cpath, ==, "");
    check(cpath != NULL && BU_STR_EMPTY(cpath), &failures);

    memset(path, 0, MAXPATHLEN);
    cpath = bu_dir(path, 0, NULL);
    PRINT(BU_DIR_END, cpath, ==, "");
    check(cpath != NULL && BU_STR_EMPTY(cpath), &failures);

    memset(path, 0, MAXPATHLEN);
    cpath = bu_dir(path, 1, ".", NULL);
    PRINT(BU_DIR_END, cpath, ==, "");
    check(BU_STR_EMPTY(cpath), &failures);

    memset(path, 0, MAXPATHLEN);
    cpath = bu_dir(path, 2, ".", NULL);
    PRINT(BU_DIR_END, cpath, !=, "");
    check(!BU_STR_EMPTY(cpath), &failures);

    memset(path, 0, MAXPATHLEN);
    cpath = bu_dir(path, 1, "/", NULL);
    PRINT(BU_DIR_END, cpath, ==, "");
    check(BU_STR_EMPTY(cpath), &failures);

    memset(path, 0, MAXPATHLEN);
    cpath = bu_dir(path, 2, "/", NULL);
    PRINT(BU_DIR_END, cpath, !=, "");
    check(!BU_STR_EMPTY(cpath), &failures);

    return failures;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
