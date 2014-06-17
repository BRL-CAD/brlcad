/*                   T E S T _ N M G _ M K . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "raytrace.h"

int
test_nmg_ms(void)
{
    int result;
    struct shell *s;

    result = 0;
    s = nmg_ms();

    /* test s */
    if (s->index != 0) {
	bu_log("Error index of shell in nmg_ms %ld\n", s->index);
	result = -1;
    }

    if (s->maxindex != 1) {
	bu_log("Error maxindex of shell in nmg_ms %ld\n", s->maxindex);
	result = -1;
    }

    if (s->magic != NMG_SHELL_MAGIC) {
	bu_log("Error magic of shell in nmg_ms, it should be NMG_SHELL_MAGIC\n");
	result = -1;
    }

    if (s->manifolds != NULL) {
	bu_log("Error manifolds of shell in nmg_ms, it should be NULL\n");
	result = -1;
    }

    nmg_ks(s);
    return result;
}

int
test_nmg_msv(void)
{
    int result;
    struct shell *s;

    result = 0;
    s = nmg_msv();

    /* test s */
    if (s->maxindex != 3) {
	bu_log("Error maxindex of shell in nmg_msv %ld\n", s->maxindex);
	result = -1;
    }

    /* test s */
    if (s == NULL) {
	bu_log("Error r_hd of shell in nmg_msv");
	nmg_ks(s);
	result = -1;
	return result;
    }

    if (s->index != 0) {
	bu_log("Error index of shell in nmg_msv %ld\n", s->index);
	result = -1;
    }

    if (s->magic != NMG_SHELL_MAGIC) {
	bu_log("Error magic of shell in nmg_msv, it should be NMG_SHELL_MAGIC\n");
	result = -1;
    }

    nmg_ks(s);
    return result;
}

int
main(int argc, char **argv)
{
    if (argc > 1) {
	bu_exit(1, "Usage: %s\n", argv[0]);
    }

    if (test_nmg_ms() < 0) {
	bu_exit(1, "Test for nmg_ms failed!\n");
    }

    if (test_nmg_msv() < 0) {
	bu_exit(1, "Test for nmg_msv failed!\n");
    }

    bu_log("All unit tests succeeded.\n");
    return 0;
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
