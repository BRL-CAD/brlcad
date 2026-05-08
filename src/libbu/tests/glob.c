/*                         G L O B . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file tests/glob.c
 *
 * Unit tests for bu_glob() / bu_glob_ctx_create() / bu_glob_ctx_destroy().
 *
 * The tests exercise bu_glob against a small in-memory tree so they do
 * not rely on filesystem layout.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/glob.h"
#include "bu/log.h"


/* -----------------------------------------------------------------------
 * Minimal in-memory tree backend
 * ----------------------------------------------------------------------- */

/* Each node is either a leaf (is_dir==0) or a container (is_dir==1). */
struct _test_node {
    const char *name;
    int is_dir;
};

/*
 * Flat list of all nodes; "containers" with a matching prefix are
 * treated as parent directories.
 *
 * The tree looks like:
 *   root/
 *     foo.s
 *     bar.s
 *     baz.r
 *     sub1/
 *       part_a.s
 *       part_b.s
 *     sub2/
 *       thing.r
 */
static const struct _test_node _tree[] = {
    { "",      1 }, /* root */
    { "foo.s", 0 },
    { "bar.s", 0 },
    { "baz.r", 0 },
    { "sub1",  1 },
    { "sub1/part_a.s", 0 },
    { "sub1/part_b.s", 0 },
    { "sub2",  1 },
    { "sub2/thing.r",  0 },
    { NULL, 0 }
};

/* Directory handle for the test backend. */
struct _test_dir_handle {
    const char *prefix;   /* path prefix this dir represents, e.g. "" or "sub1" */
    int idx;              /* current iteration index into _tree */
};


static void *
_test_opendir(const char *path, void *UNUSED(data))
{
    struct _test_dir_handle *h;
    BU_ALLOC(h, struct _test_dir_handle);
    h->prefix = (path && *path) ? path : "";
    h->idx = 0;
    return (void *)h;
}


static int
_test_readdir(struct bu_dirent *de, void *handle)
{
    struct _test_dir_handle *h = (struct _test_dir_handle *)handle;
    size_t plen;
    const char *pref;

    pref = h->prefix;
    plen = strlen(pref);

    for (; _tree[h->idx].name; h->idx++) {
        const char *nname = _tree[h->idx].name;
        const char *basename;

        /* skip the root placeholder */
        if (*nname == '\0') { continue; }

        if (plen == 0) {
            /* root: include only top-level names (no '/' in name) */
            if (strchr(nname, '/') != NULL) { continue; }
            bu_vls_sprintf(de->name, "%s", nname);
            h->idx++;
            return 0;
        } else {
            /* sub-dir: include names that start with "prefix/" */
            char prefix_slash[512];
            snprintf(prefix_slash, sizeof(prefix_slash), "%s/", pref);
            if (bu_strncmp(nname, prefix_slash, plen + 1) != 0) { continue; }
            /* only one level deeper (no further '/' after the prefix) */
            basename = nname + plen + 1;
            if (strchr(basename, '/') != NULL) { continue; }
            bu_vls_sprintf(de->name, "%s", basename);
            h->idx++;
            return 0;
        }
    }
    return 1; /* end of directory */
}


static void
_test_closedir(void *handle)
{
    bu_free(handle, "_test_dir_handle");
}


static int
_test_lstat(const char *path, struct bu_stat *sb, void *UNUSED(data))
{
    const struct _test_node *n;
    for (n = _tree; n->name; n++) {
        if (bu_strcmp(n->name, path) == 0) {
            sb->is_dir = n->is_dir;
            return 0;
        }
    }
    return -1; /* not found */
}


/* -----------------------------------------------------------------------
 * Test helpers
 * -----------------------------------------------------------------------*/

static struct bu_glob_context *
_make_test_ctx(void)
{
    struct bu_glob_context *gp = bu_glob_ctx_create();
    gp->gl_opendir  = _test_opendir;
    gp->gl_readdir  = _test_readdir;
    gp->gl_closedir = _test_closedir;
    gp->gl_lstat    = _test_lstat;
    gp->gl_stat     = _test_lstat; /* same semantics for test backend */
    return gp;
}


/* Return non-zero if 'name' is found in gp->gl_pathv. */
static int
_in_results(struct bu_glob_context *gp, const char *name)
{
    int i;
    for (i = 0; i < gp->gl_pathc; i++)
        if (gp->gl_pathv[i] && bu_strcmp(bu_vls_cstr(gp->gl_pathv[i]), name) == 0)
            return 1;
    return 0;
}


/* -----------------------------------------------------------------------
 * Tests
 * -----------------------------------------------------------------------*/

static int
test_glob_star_s(void)
{
    int errors = 0;
    struct bu_glob_context *gp = _make_test_ctx();

    if (bu_glob("*.s", 0, gp) != 0) { bu_log("bu_glob returned error\n"); errors++; }

    /* Should match foo.s and bar.s but NOT baz.r */
    if (!_in_results(gp, "foo.s")) { bu_log("FAIL: *.s did not match foo.s\n"); errors++; }
    if (!_in_results(gp, "bar.s")) { bu_log("FAIL: *.s did not match bar.s\n"); errors++; }
    if ( _in_results(gp, "baz.r")) { bu_log("FAIL: *.s wrongly matched baz.r\n"); errors++; }
    if ( _in_results(gp, "sub1"))  { bu_log("FAIL: *.s wrongly matched sub1\n"); errors++; }

    bu_glob_ctx_destroy(gp);
    return errors;
}


static int
test_glob_literal(void)
{
    int errors = 0;
    struct bu_glob_context *gp = _make_test_ctx();

    if (bu_glob("foo.s", 0, gp) != 0) { bu_log("bu_glob returned error\n"); errors++; }
    if (gp->gl_pathc != 1) { bu_log("FAIL: literal match count != 1 (got %d)\n", gp->gl_pathc); errors++; }
    if (!_in_results(gp, "foo.s")) { bu_log("FAIL: literal did not match foo.s\n"); errors++; }

    bu_glob_ctx_destroy(gp);
    return errors;
}


static int
test_glob_no_match(void)
{
    int errors = 0;
    struct bu_glob_context *gp = _make_test_ctx();

    bu_glob("*.g", 0, gp);
    if (gp->gl_pathc != 0) { bu_log("FAIL: *.g should have 0 matches, got %d\n", gp->gl_pathc); errors++; }

    bu_glob_ctx_destroy(gp);
    return errors;
}


static int
test_glob_subdir(void)
{
    int errors = 0;
    struct bu_glob_context *gp = _make_test_ctx();

    /* sub1/part*.s should match sub1/part_a.s and sub1/part_b.s */
    if (bu_glob("sub1/part*.s", 0, gp) != 0) { bu_log("bu_glob returned error\n"); errors++; }
    if (gp->gl_pathc != 2) { bu_log("FAIL: sub1/part*.s count=%d, want 2\n", gp->gl_pathc); errors++; }
    if (!_in_results(gp, "sub1/part_a.s")) { bu_log("FAIL: missing sub1/part_a.s\n"); errors++; }
    if (!_in_results(gp, "sub1/part_b.s")) { bu_log("FAIL: missing sub1/part_b.s\n"); errors++; }

    bu_glob_ctx_destroy(gp);
    return errors;
}


static int
test_glob_wildcard_dir(void)
{
    int errors = 0;
    struct bu_glob_context *gp = _make_test_ctx();

    /* sub* /part*.s should match sub1/part_a.s and sub1/part_b.s */
    if (bu_glob("sub*/*.s", 0, gp) != 0) { bu_log("bu_glob returned error\n"); errors++; }
    if (gp->gl_pathc != 2) { bu_log("FAIL: sub*/*.s count=%d, want 2\n", gp->gl_pathc); errors++; }

    bu_glob_ctx_destroy(gp);
    return errors;
}


static int
test_glob_append(void)
{
    int errors = 0;
    struct bu_glob_context *gp = _make_test_ctx();

    bu_glob("*.s", 0, gp);
    int first_count = gp->gl_pathc;

    /* Append *.r matches to existing results */
    bu_glob("*.r", BU_GLOB_APPEND, gp);
    if (gp->gl_pathc <= first_count) { bu_log("FAIL: APPEND did not add entries\n"); errors++; }
    if (!_in_results(gp, "baz.r")) { bu_log("FAIL: *.r did not match baz.r\n"); errors++; }

    bu_glob_ctx_destroy(gp);
    return errors;
}


static int
test_glob_question(void)
{
    int errors = 0;
    struct bu_glob_context *gp = _make_test_ctx();

    /* sub? should match sub1 and sub2 */
    bu_glob("sub?", 0, gp);
    if (gp->gl_pathc != 2) { bu_log("FAIL: sub? count=%d, want 2\n", gp->gl_pathc); errors++; }
    if (!_in_results(gp, "sub1")) { bu_log("FAIL: sub? missing sub1\n"); errors++; }
    if (!_in_results(gp, "sub2")) { bu_log("FAIL: sub? missing sub2\n"); errors++; }

    bu_glob_ctx_destroy(gp);
    return errors;
}


int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);

    int total_errors = 0;

    total_errors += test_glob_star_s();
    total_errors += test_glob_literal();
    total_errors += test_glob_no_match();
    total_errors += test_glob_subdir();
    total_errors += test_glob_wildcard_dir();
    total_errors += test_glob_append();
    total_errors += test_glob_question();

    return total_errors;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
