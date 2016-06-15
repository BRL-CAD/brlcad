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
test_nmg_mm(void)
{
    int result;
    struct model *m;

    result = 0;
    m = nmg_mm();

    /* test m */
    if (m->index != 0) {
	bu_log("Error index of model in nmg_mm %ld\n", m->index);
	result = -1;
    }

    if (m->maxindex != 1) {
	bu_log("Error maxindex of model in nmg_mm %ld\n", m->maxindex);
	result = -1;
    }

    if (m->magic != NMG_MODEL_MAGIC) {
	bu_log("Error magic of model in nmg_mm, it should be NMG_MODEL_MAGIC\n");
	result = -1;
    }

    if (m->manifolds != NULL) {
	bu_log("Error manifolds of model in nmg_mm, it should be NULL\n");
	result = -1;
    }

    nmg_km(m);
    return result;
}

int
test_nmg_mmr(void)
{
    int result;
    struct model *m;
    struct nmgregion *r;

    result = 0;
    m = nmg_mmr();
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);

    /* test m */
    if (m->maxindex != 2) {
	bu_log("Error maxindex of model in nmg_mmr %ld\n", m->maxindex);
	result = -1;
    }

    /* test r */
    if (r == NULL) {
	bu_log("Error r_hd of model in nmg_mmr");
	nmg_km(m);
	result = -1;
	return result;
    }

    if (r->index != 1) {
	bu_log("Error index of region in nmg_mmr %ld\n", r->index);
	result = -1;
    }

    if (r->l.magic != NMG_REGION_MAGIC) {
	bu_log("Error magic of region in nmg_mmr, it should be NMG_REGION_MAGIC\n");
	result = -1;
    }

    nmg_km(m);
    return result;
}

int
test_nmg_mrsv(void)
{
    int result;
    struct model *m;
    struct nmgregion *r;

    result = 0;
    m = nmg_mmr();
    r = nmg_mrsv(m);

    /* test m */
    if (m->maxindex != 6) {
	bu_log("Error maxindex of model in nmg_mrsv %ld\n", m->maxindex);
	result = -1;
    }

    /* test r */
    if (r == NULL) {
	bu_log("Error r_hd of model in nmg_mrsv");
	nmg_km(m);
	result = -1;
	return result;
    }

    if (r->index != 2) {
	bu_log("Error index of region in nmg_mrsv %ld\n", r->index);
	result = -1;
    }

    if (r->l.magic != NMG_REGION_MAGIC) {
	bu_log("Error magic of region in nmg_mrsv, it should be NMG_REGION_MAGIC\n");
	result = -1;
    }

    nmg_km(m);
    return result;
}

int
test_nmg_msv(void)
{
    int result;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;

    result = 0;
    m = nmg_mmr();
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    s = nmg_msv(r);

    /* test m */
    if (m->maxindex != 5) {
	bu_log("Error maxindex of model in nmg_msv %ld\n", m->maxindex);
	result = -1;
    }

    /* test s */
    if (s == NULL) {
	bu_log("Error r_hd of model in nmg_msv");
	nmg_km(m);
	result = -1;
	return result;
    }

    if (s->index != 2) {
	bu_log("Error index of shell in nmg_msv %ld\n", s->index);
	result = -1;
    }

    if (s->l.magic != NMG_SHELL_MAGIC) {
	bu_log("Error magic of shell in nmg_msv, it should be NMG_SHELL_MAGIC\n");
	result = -1;
    }

    nmg_km(m);
    return result;
}

int
main(int argc, char **argv)
{
    if (argc > 1) {
	bu_exit(1, "Usage: %s\n", argv[0]);
    }

    if (test_nmg_mm() < 0) {
	bu_exit(1, "Test for nmg_mm failed!\n");
    }

    if (test_nmg_mmr() < 0) {
	bu_exit(1, "Test for nmg_mmr failed!\n");
    }

    if (test_nmg_mrsv() < 0) {
	bu_exit(1, "Test for nmg_mrsv failed!\n");
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
