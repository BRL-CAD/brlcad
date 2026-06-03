/*                        P A R S E . C
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
/** @file parse.c
 *
 * Unit tests for libmcpcad command string parsing.
 *
 * Contract under test (see include/mcpcad.h):
 *
 *  - whitespace tokenization with double-quote and backslash support
 *    (delegated to bu_argv_from_string)
 *  - argv is NULL-terminated and points into storage owned by the cmd
 *  - the caller's input string is never modified
 *  - rejected inputs (NULL return): NULL/empty/whitespace-only,
 *    length >= MCPCAD_MAXLINE, control chars other than tab,
 *    argc >= MCPCAD_MAXARGS
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "mcpcad.h"


#define REJECT -1

struct parse_case {
    const char *input;     /* command string fed to the parser */
    int expect_argc;       /* REJECT means parse must return NULL */
    const char *e0;        /* expected argv[0] (NULL = don't check) */
    const char *e1;        /* expected argv[1] (NULL = don't check) */
    const char *elast;     /* expected argv[argc-1] (NULL = don't check) */
};

static const struct parse_case cases[] = {
    /* --- basic tokenization --- */
    {"in sph1 sph 0 0 0 5",        7, "in",    "sph1", "5"},
    {"ls",                         1, "ls",    NULL,   "ls"},
    {"  ls   -l  ",                2, "ls",    "-l",   "-l"},
    {"kill\tsph1",                 2, "kill",  "sph1", "sph1"},

    /* --- quoting (bu_argv_from_string semantics) --- */
    {"title \"my new db\"",        2, "title", "my new db", "my new db"},
    {"\"a b\" c",                  2, "a b",   "c",    "c"},
    {"comb c.s u \"left wing\"",   4, "comb",  "c.s",  "left wing"},

    /* --- rejections: nothing to parse --- */
    {"",                      REJECT, NULL, NULL, NULL},
    {"   ",                   REJECT, NULL, NULL, NULL},
    {"\t\t",                  REJECT, NULL, NULL, NULL},

    /* --- rejections: control characters (newline = framing layer's
     *     delimiter; must never survive into a single command) --- */
    {"ls\nkill sph1",         REJECT, NULL, NULL, NULL},
    {"ls\r",                  REJECT, NULL, NULL, NULL},
    {"ls \x01 -l",            REJECT, NULL, NULL, NULL},

    /* table terminator */
    {NULL, 0, NULL, NULL, NULL}
};


static int
check_argv(const struct mcpcad_cmd *c, int idx, const char *expected,
	   const char *input)
{
    if (!expected)
	return 0;
    if (!c->argv[idx] || !BU_STR_EQUAL(c->argv[idx], expected)) {
	printf("FAIL: \"%s\": argv[%d] = \"%s\", expected \"%s\"\n",
	       input, idx, c->argv[idx] ? c->argv[idx] : "(null)", expected);
	return 1;
    }
    return 0;
}


static int
run_case(const struct parse_case *tc)
{
    int failures = 0;

    /* the parser must never modify the caller's string - hand it a
     * private copy we can compare afterwards */
    char *input = bu_strdup(tc->input);

    struct mcpcad_cmd *c = mcpcad_cmd_parse(input);

    if (tc->expect_argc == REJECT) {
	if (c) {
	    printf("FAIL: \"%s\": expected rejection, got argc=%d\n",
		   tc->input, c->argc);
	    failures++;
	}
    } else if (!c) {
	printf("FAIL: \"%s\": expected argc=%d, got NULL (rejected)\n",
	       tc->input, tc->expect_argc);
	failures++;
    } else {
	if (c->argc != tc->expect_argc) {
	    printf("FAIL: \"%s\": argc = %d, expected %d\n",
		   tc->input, c->argc, tc->expect_argc);
	    failures++;
	}
	failures += check_argv(c, 0, tc->e0, tc->input);
	failures += check_argv(c, 1, tc->e1, tc->input);
	if (c->argc == tc->expect_argc)
	    failures += check_argv(c, c->argc - 1, tc->elast, tc->input);

	/* NULL termination contract: argv[argc] == NULL */
	if (c->argv[c->argc] != NULL) {
	    printf("FAIL: \"%s\": argv[argc] is not NULL\n", tc->input);
	    failures++;
	}
    }

    /* const contract: input must be untouched no matter the outcome */
    if (!BU_STR_EQUAL(input, tc->input)) {
	printf("FAIL: \"%s\": caller's input string was modified\n",
	       tc->input);
	failures++;
    }

    mcpcad_cmd_free(c);
    bu_free(input, "test input copy");
    return failures;
}


/* boundary cases need constructed inputs - build them at runtime */
static int
run_boundaries(void)
{
    int failures = 0;
    struct mcpcad_cmd *c;

    /* one arg of MCPCAD_MAXLINE-1 chars (+NUL = exactly MAXLINE bytes)
     * is the longest accepted input */
    {
	char *buf = (char *)bu_calloc(MCPCAD_MAXLINE + 2, 1, "maxline buf");
	memset(buf, 'a', MCPCAD_MAXLINE - 1);
	c = mcpcad_cmd_parse(buf);
	if (!c || c->argc != 1) {
	    printf("FAIL: %d-char input: expected argc=1, got %s\n",
		   MCPCAD_MAXLINE - 1, c ? "wrong argc" : "NULL");
	    failures++;
	}
	mcpcad_cmd_free(c);

	/* one more char tips it over the limit */
	buf[MCPCAD_MAXLINE - 1] = 'a';
	c = mcpcad_cmd_parse(buf);
	if (c) {
	    printf("FAIL: %d-char input: expected rejection\n",
		   MCPCAD_MAXLINE);
	    failures++;
	    mcpcad_cmd_free(c);
	}
	bu_free(buf, "maxline buf");
    }

    /* MCPCAD_MAXARGS-1 args is the most accepted; MCPCAD_MAXARGS is
     * rejected (hitting the cap may mean truncation - never execute a
     * possibly-partial command) */
    {
	char *buf = (char *)bu_calloc(2 * MCPCAD_MAXARGS + 2, 1, "maxargs buf");
	int i;

	for (i = 0; i < MCPCAD_MAXARGS - 1; i++) {
	    buf[2 * i] = 'x';
	    buf[2 * i + 1] = ' ';
	}
	c = mcpcad_cmd_parse(buf);
	if (!c || c->argc != MCPCAD_MAXARGS - 1) {
	    printf("FAIL: %d-arg input: expected argc=%d, got %s\n",
		   MCPCAD_MAXARGS - 1, MCPCAD_MAXARGS - 1,
		   c ? "wrong argc" : "NULL");
	    failures++;
	}
	mcpcad_cmd_free(c);

	/* one more arg hits the cap */
	buf[2 * (MCPCAD_MAXARGS - 1)] = 'x';
	c = mcpcad_cmd_parse(buf);
	if (c) {
	    printf("FAIL: %d-arg input: expected rejection, got argc=%d\n",
		   MCPCAD_MAXARGS, c->argc);
	    failures++;
	    mcpcad_cmd_free(c);
	}
	bu_free(buf, "maxargs buf");
    }

    return failures;
}


int
main(int argc, char *argv[])
{
    int failures = 0;
    int ncases = 0;
    const struct parse_case *tc;

    bu_setprogname(argv[0]);

    if (argc > 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    /* NULL input must be rejected, and freeing NULL is a no-op */
    if (mcpcad_cmd_parse(NULL)) {
	printf("FAIL: mcpcad_cmd_parse(NULL) returned non-NULL\n");
	failures++;
    }
    mcpcad_cmd_free(NULL);
    ncases++;

    for (tc = cases; tc->input; tc++) {
	failures += run_case(tc);
	ncases++;
    }

    failures += run_boundaries();
    ncases += 4;

    printf("%d cases, %d failures\n", ncases, failures);

    /* exit codes are truncated to 8 bits - don't let 256 look like 0 */
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
