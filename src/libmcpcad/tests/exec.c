/*                         E X E C . C
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
/** @file exec.c
 *
 * Unit tests for libmcpcad command execution.
 *
 * Contract under test (see include/mcpcad.h):
 *
 *  - NULL/empty cmd  -> MCPCAD_ERR_NOCMD (checked before gedp)
 *  - NULL gedp       -> MCPCAD_ERR_NODB, diagnostic in result
 *  - otherwise the verbatim ged_exec() status is returned and the
 *    command output (or GED error text) lands in result
 *  - result may be NULL when the caller doesn't want output
 *  - executing a cmd does not consume it (re-executable)
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/vls.h"
#include "ged.h"
#include "mcpcad.h"


/* parse + exec + free in one step; returns exec status, copies output */
static int
run(struct ged *gedp, const char *line, struct bu_vls *result)
{
    struct mcpcad_cmd *c = mcpcad_cmd_parse(line);
    int status;

    if (!c) {
	printf("INTERNAL: \"%s\" failed to parse - test bug\n", line);
	return -999;
    }
    status = mcpcad_cmd_exec(gedp, c, result);
    mcpcad_cmd_free(c);
    return status;
}


static int
expect_status(struct ged *gedp, const char *line, int expected,
	      struct bu_vls *result)
{
    int status = run(gedp, line, result);
    if (status != expected) {
	printf("FAIL: \"%s\": status %d, expected %d\n",
	       line, status, expected);
	return 1;
    }
    return 0;
}


static int
expect_contains(const struct bu_vls *result, const char *needle,
		const char *what)
{
    if (!strstr(bu_vls_cstr(result), needle)) {
	printf("FAIL: %s: result \"%s\" does not contain \"%s\"\n",
	       what, bu_vls_cstr(result), needle);
	return 1;
    }
    return 0;
}


int
main(int argc, char *argv[])
{
    int failures = 0;
    char dbpath[MAXPATHLEN] = {0};
    struct ged *gedp = NULL;
    struct bu_vls result = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    if (argc > 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    /* --- pipeline-level failures: no database involved --- */

    /* NULL cmd: rejected before gedp is even considered */
    if (mcpcad_cmd_exec(NULL, NULL, &result) != MCPCAD_ERR_NOCMD) {
	printf("FAIL: NULL cmd: expected MCPCAD_ERR_NOCMD\n");
	failures++;
    }

    /* hand-built empty cmd: defensive NOCMD even with plausible struct */
    {
	struct mcpcad_cmd empty = {0, NULL, NULL};
	if (mcpcad_cmd_exec(NULL, &empty, &result) != MCPCAD_ERR_NOCMD) {
	    printf("FAIL: empty cmd: expected MCPCAD_ERR_NOCMD\n");
	    failures++;
	}
    }

    /* valid cmd but no database: NODB with a diagnostic for the reply
     * layer to ship back to the client */
    failures += expect_status(NULL, "ls", MCPCAD_ERR_NODB, &result);
    failures += expect_contains(&result, "database", "NODB diagnostic");

    /* --- live database --- */

    bu_dir(dbpath, MAXPATHLEN, BU_DIR_TEMP, "mcpcad_test_exec.g", NULL);
    bu_file_delete(dbpath);  /* stale copy from a previous run */
    gedp = ged_open("db", dbpath, 0);
    if (!gedp) {
	printf("INTERNAL: could not create scratch db at %s\n", dbpath);
	bu_vls_free(&result);
	return 99;
    }

    /* empty db: ls succeeds and reports nothing */
    failures += expect_status(gedp, "ls", 0, &result);
    if (bu_vls_strlen(&result) != 0) {
	printf("FAIL: ls on empty db: expected empty result, got \"%s\"\n",
	       bu_vls_cstr(&result));
	failures++;
    }

    /* create geometry two ways: 'make' (defaults) and the user-story
     * canonical 'in' (explicit params), then see both via ls */
    failures += expect_status(gedp, "make sph1 sph", 0, &result);
    failures += expect_status(gedp, "in sph2 sph 0 0 0 5", 0, &result);
    failures += expect_status(gedp, "ls", 0, &result);
    failures += expect_contains(&result, "sph1", "ls after make");
    failures += expect_contains(&result, "sph2", "ls after in");

    /* result string captures command output (title round-trip,
     * quoted arg exercising the parser+exec seam) */
    failures += expect_status(gedp, "title \"mcpcad scratch\"", 0, &result);
    failures += expect_status(gedp, "title", 0, &result);
    failures += expect_contains(&result, "mcpcad scratch", "title get");

    /* GED-level failure: propagated, explained, survived */
    {
	int status = run(gedp, "frobnicate now", &result);
	if (status <= 0) {
	    printf("FAIL: unknown command: expected positive status, got %d\n",
		   status);
	    failures++;
	}
	if (bu_vls_strlen(&result) == 0) {
	    printf("FAIL: unknown command: expected diagnostic in result\n");
	    failures++;
	}
    }

    /* NULL result is allowed - output discarded, no crash */
    failures += expect_status(gedp, "ls", 0, NULL);

    /* a parsed cmd is not consumed by execution */
    {
	struct mcpcad_cmd *c = mcpcad_cmd_parse("ls");
	if (!c) {
	    printf("INTERNAL: reuse parse failed\n");
	    failures++;
	} else {
	    if (mcpcad_cmd_exec(gedp, c, &result) != 0 ||
		mcpcad_cmd_exec(gedp, c, &result) != 0) {
		printf("FAIL: re-executing a parsed cmd failed\n");
		failures++;
	    }
	    mcpcad_cmd_free(c);
	}
    }

    /* the database survived all of the above: still lists both spheres */
    failures += expect_status(gedp, "ls", 0, &result);
    failures += expect_contains(&result, "sph1", "db integrity");

    ged_close(gedp);
    bu_file_delete(dbpath);
    bu_vls_free(&result);

    printf("exec tests: %d failures\n", failures);
    return failures > 255 ? 255 : failures;
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
