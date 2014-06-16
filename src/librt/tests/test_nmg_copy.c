/*                   T E S T _ N M G _ C O P Y . C
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
nmg_compare_vertexuse(struct vertexuse *vu1, struct vertexuse *vu2)
{
    int result;

    result = 0;

    /* Testing index of vertexuse */
    if (vu1->index != vu2->index) {
	bu_log("Error index of vertexuse. vu1: %ld, vu2: %ld\n", vu1->index, vu2->index);
	result = -1;
    }

    return result;
}

int
nmg_compare_edgeuse(struct edgeuse *eu1, struct edgeuse *eu2)
{
    int result;

    result = 0;

    /* Testing orientation of edgeuse */
    if (eu1->orientation != eu2->orientation) {
	bu_log("Error orientation of edgeuse. eu1: %d, eu2: %d\n", eu1->orientation, eu2->orientation);
	result = -1;
    }

    /* Testing index of edgeuse */
    if (eu1->index != eu2->index) {
	bu_log("Error index of edgeuse. eu1: %ld, eu2: %ld\n", eu1->index, eu2->index);
	result = -1;
    }

    return result;
}

int
nmg_compare_loopuse(struct loopuse *lu1, struct loopuse *lu2)
{
    int result;

    result = 0;

    /* Testing orientation of loopuse */
    if (lu1->orientation != lu2->orientation) {
	bu_log("Error orientation of loopuse. lu1: %d, lu2: %d\n", lu1->orientation, lu2->orientation);
	result = -1;
    }

    /* Testing index of loopuse */
    if (lu1->index != lu2->index) {
	bu_log("Error index of loopuse. lu1: %ld, lu2: %ld\n", lu1->index, lu2->index);
	result = -1;
    }

    return result;
}

int
nmg_compare_faceuse(struct faceuse *fu1, struct faceuse *fu2)
{
    int result;

    int lulen1;
    int lulen2;
    struct loopuse *lu1;
    struct loopuse *lu2;

    result = 0;

    /* Testing orientation of faceuse */
    if (fu1->orientation != fu2->orientation) {
	bu_log("Error orientation of faceuse. fu1: %d, fu2: %d\n", fu1->orientation, fu2->orientation);
	result = -1;
    }

    /* Testing outside of faceuse */
    if (fu1->outside != fu2->outside) {
	bu_log("Error outside of faceuse. fu1: %d, fu2: %d\n", fu1->outside, fu2->outside);
	result = -1;
    }

    /* Testing lu_hd of faceuse */
    lulen1 = bu_list_len(&fu1->lu_hd);
    lulen2 = bu_list_len(&fu2->lu_hd);

    if (lulen1 != lulen2) {
	bu_log("Error lu_hd's size of faceuse. fu1: %d, fu2: %d\n", lulen1, lulen2);
	result = -1;
	return result;
    }

    for (BU_LIST_FOR2(lu1, lu2, loopuse, &fu1->lu_hd, &fu2->lu_hd)) {
	if (nmg_compare_loopuse(lu1, lu2) < 0) {
	    result = -1;
	}
    }

    /* Testing index of faceuse */
    if (fu1->index != fu2->index) {
	bu_log("Error index of faceuse. fu1: %ld, fu2: %ld\n", fu1->index, fu2->index);
	result = -1;
    }

    return result;
}

int
nmg_compare_shell_a(struct shell_a *sa1, struct shell_a *sa2)
{
    int result;

    result = 0;

    /* Testing min_pt of shell_a */
    if (!NEAR_EQUAL(sa1->min_pt, sa2->min_pt, SMALL_FASTF)) {
	bu_log("Error min_pt of shell_a.");
	result = -1;
    }

    /* Testing max_pt of shell_a */
    if (!NEAR_EQUAL(sa1->max_pt, sa2->max_pt, SMALL_FASTF)) {
	bu_log("Error max_pt of shell_a.");
	result = -1;
    }

    /* Testing index of shell_a */
    if (sa1->index != sa2->index) {
	bu_log("Error index of shell_a. sa1: %ld, sa2: %ld\n", sa1->index, sa2->index);
	result = -1;
    }

    return result;
}

int
test_nmg_clone_shell(struct shell *s1, struct shell *s2)
{
    int result;

    int fulen1;
    int fulen2;
    struct faceuse *fu1;
    struct faceuse *fu2;

    int lulen1;
    int lulen2;
    struct loopuse *lu1;
    struct loopuse *lu2;

    int eulen1;
    int eulen2;
    struct edgeuse *eu1;
    struct edgeuse *eu2;

    result = 0;

    /* Testing shell_a of shell */
    if (s1->sa_p != (struct shell_a *)NULL && s2->sa_p != (struct shell_a *)NULL) {
	if (nmg_compare_shell_a(s1->sa_p, s2->sa_p) < 0) {
	    result = -1;
	}
    } else {
	if (s1->sa_p != s2->sa_p) {
	    bu_log("Error shell_a of shell. One is NULL, but other is not NULL\n");
	    result = -1;
	}
    }

    /* Testing fu_hd of shell */
    fulen1 = bu_list_len(&s1->fu_hd);
    fulen2 = bu_list_len(&s2->fu_hd);

    if (fulen1 != fulen2) {
	bu_log("Error fu_hd's size of shell. s1: %d, s2: %d\n", fulen1, fulen2);
	result = -1;
	return result;
    }

    for (BU_LIST_FOR2(fu1, fu2, faceuse, &s1->fu_hd, &s2->fu_hd)) {
	if (nmg_compare_faceuse(fu1, fu2) < 0) {
	    result = -1;
	}
    }

    /* Testing lu_hd of shell */
    lulen1 = bu_list_len(&s1->lu_hd);
    lulen2 = bu_list_len(&s2->lu_hd);

    if (lulen1 != lulen2) {
	bu_log("Error lu_hd's size of shell. s1: %d, s2: %d\n", lulen1, lulen2);
	result = -1;
	return result;
    }

    for (BU_LIST_FOR2(lu1, lu2, loopuse, &s1->lu_hd, &s2->lu_hd)) {
	if (nmg_compare_loopuse(lu1, lu2) < 0) {
	    result = -1;
	}
    }

    /* Testing eu_hd of shell */
    eulen1 = bu_list_len(&s1->eu_hd);
    eulen2 = bu_list_len(&s2->eu_hd);

    if (eulen1 != eulen2) {
	bu_log("Error eu_hd's size of shell. s1: %d, s2: %d\n", eulen1, eulen2);
	result = -1;
	return result;
    }

    for (BU_LIST_FOR2(eu1, eu2, edgeuse, &s1->eu_hd, &s2->eu_hd)) {
	if (nmg_compare_edgeuse(eu1, eu2) < 0) {
	    result = -1;
	}
    }

    /* Testing vu_p of shell */
    if (s1->vu_p != (struct vertexuse *)NULL && s2->vu_p != (struct vertexuse *)NULL) {
	if (nmg_compare_vertexuse(s1->vu_p, s2->vu_p) < 0) {
	    result = -1;
	}
    } else {
	if (s1->vu_p != s2->vu_p) {
	    bu_log("Error vu_p of shell. One is NULL, but other is not NULL\n");
	    result = -1;
	}
    }

    /* Testing manifolds of model */
    if (s1->manifolds != (char *)NULL && s2->manifolds != (char *)NULL) {
	if (*s1->manifolds != *s2->manifolds) {
	    bu_log("Error manifolds of shell. s1: %ld, s2: %ld\n", *s1->manifolds, *s2->manifolds);
	    result = -1;
	}
    } else {
	if (s1->manifolds != s2->manifolds) {
	    bu_log("Error manifolds of shell. One is NULL, but other is not NULL\n");
	    result = -1;
	}
    }

    /* Testing index of shell */
    if (s1->index != s2->index) {
	bu_log("Error index of shell. s1: %ld, s2: %ld\n", s1->index, s2->index);
	result = -1;
    }

    /* Testing maxindex of model */
    if (s1->maxindex != s2->maxindex) {
	bu_log("Error maxindex of shell. s1: %ld, s2: %ld\n", s1->maxindex, s2->maxindex);
	result = -1;
    }

    return result;
}

int
testcase_nmg_ms()
{
    int result;
    struct shell *s1;
    struct shell *s2;

    s1 = nmg_ms();
    s2 = nmg_clone_shell(s1);

    result = test_nmg_clone_shell(s1, s2);

    nmg_ks(s1);
    nmg_ks(s2);

    return result;
}

int
testcase_nmg_msv()
{
    int result;
    struct shell *s1;
    struct shell *s2;

    s1 = nmg_msv();
    s2 = nmg_clone_shell(s1);

    result = test_nmg_clone_shell(s1, s2);

    nmg_ks(s1);
    nmg_ks(s2);

    return result;
}

int
main(int argc, char **argv)
{
    if (argc > 1) {
	bu_exit(1, "Usage: %s\n", argv[0]);
    }

    if (testcase_nmg_ms() < 0) {
	bu_exit(1, "Test Case by nmg_ms failed!\n");
    }

    if (testcase_nmg_msv() < 0) {
	bu_exit(1, "Test Case by nmg_msv failed!\n");
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
