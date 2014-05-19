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
nmg_compare_shell(struct shell *s1, struct shell *s2)
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

    /* Testing index of shell */
    if (s1->index != s2->index) {
	bu_log("Error index of shell. s1: %ld, s2: %ld\n", s1->index, s2->index);
	result = -1;
    }

    return result;
}

int 
nmg_compare_region_a(struct nmgregion_a *ra1, struct nmgregion_a *ra2)
{
    int result;

    result = 0;

    /* Testing min_pt of nmgregion_a */
    if (!NEAR_EQUAL(ra1->min_pt, ra2->min_pt, SMALL_FASTF)) {
	bu_log("Error min_pt of nmgregion_a.");
	result = -1;
    }

    /* Testing max_pt of nmgregion_a */
    if (!NEAR_EQUAL(ra1->max_pt, ra2->max_pt, SMALL_FASTF)) {
	bu_log("Error max_pt of nmgregion_a.");
	result = -1;
    }

    /* Testing index of nmgregion_a */
    if (ra1->index != ra2->index) {
	bu_log("Error index of nmgregion_a. ra1: %ld, ra2: %ld\n", ra1->index, ra2->index);
	result = -1;
    }

    return result;
}

int
nmg_compare_region(struct nmgregion *r1, struct nmgregion *r2)
{
    int result;

    int slen1;
    int slen2;
    struct shell *s1;
    struct shell *s2;

    result = 0;

    /* Testing ra_p of nmgregion */
    if (r1->ra_p != (struct nmgregion_a *)NULL && r2->ra_p != (struct nmgregion_a *)NULL) {
	if (nmg_compare_region_a(r1->ra_p, r2->ra_p) < 0) {
	    result = -1;
	}
    } else {
	if (r1->ra_p != r2->ra_p) {
	    bu_log("Error nmgregion_a of region. One is NULL, but other is not NULL\n");
	    result = -1;
	}
    }

    /* Testing s_hd of nmgregion */
    slen1 = bu_list_len(&r1->s_hd);
    slen2 = bu_list_len(&r2->s_hd);

    if (slen1 != slen2) {
	bu_log("Error s_hd's size of region. r1: %d, r2: %d\n", slen1, slen2);
	result = -1;
	return result;
    }

    for (BU_LIST_FOR2(s1, s2, shell, &r1->s_hd, &r2->s_hd)) {
	if (nmg_compare_shell(s1, s2) < 0) {
	    result = -1;
	}
    }

    /* Testing index of nmgregion */
    if (r1->index != r2->index) {
	bu_log("Error index of region. r1: %ld, r2: %ld\n", r1->index, r2->index);
	result = -1;
    }

    return result;
}

int
test_nmg_clone_model(struct model *m1, struct model *m2)
{
    int result;

    int rlen1;
    int rlen2;
    struct nmgregion *r1;
    struct nmgregion *r2;

    result = 0;

    /* Testing r_hd of model */
    rlen1 = bu_list_len(&m1->r_hd);
    rlen2 = bu_list_len(&m2->r_hd);

    if (rlen1 != rlen2) {
	bu_log("Error r_hd's size of model. m1: %d, m2: %d\n", rlen1, rlen2);
	result = -1;
	return result;
    }

    for (BU_LIST_FOR2(r1, r2, nmgregion, &m1->r_hd, &m2->r_hd)) {
	if (nmg_compare_region(r1, r2) < 0) {
	    result = -1;
	}
    }

    /* Testing manifolds of model */
    if (m1->manifolds != (char *)NULL && m2->manifolds != (char *)NULL) {
	if (*m1->manifolds != *m2->manifolds) {
	    bu_log("Error manifolds of model. m1: %ld, m2: %ld\n", *m1->manifolds, *m2->manifolds);
	    result = -1;
	}
    } else {
	if (m1->manifolds != m2->manifolds) {
	    bu_log("Error manifolds of model. One is NULL, but other is not NULL\n");
	    result = -1;
	}
    }

    /* Testing index of model */
    if (m1->index != m2->index) {
	bu_log("Error index of model. m1: %ld, m2: %ld\n", m1->index, m2->index);
	result = -1;
    }

    /* Testing maxindex of model */
    if (m1->maxindex != m2->maxindex) {
	bu_log("Error maxindex of model. m1: %ld, m2: %ld\n", m1->maxindex, m2->maxindex);
	result = -1;
    }

    return result;
}

int
testcase_nmg_mm()
{
    int result;
    struct model *m1;
    struct model *m2;

    m1 = nmg_mm();
    m2 = nmg_clone_model(m1);

    result = test_nmg_clone_model(m1, m2);

    nmg_km(m1);
    nmg_km(m2);

    return result;
}

int
testcase_nmg_mmr()
{
    int result;
    struct model *m1;
    struct model *m2;

    m1 = nmg_mmr();
    m2 = nmg_clone_model(m1);

    result = test_nmg_clone_model(m1, m2);

    nmg_km(m1);
    nmg_km(m2);

    return result;
}

int
testcase_nmg_mrsv()
{
    int result;
    struct model *m1;
    struct model *m2;

    m1 = nmg_mmr();
    nmg_mrsv(m1);
    m2 = nmg_clone_model(m1);

    result = test_nmg_clone_model(m1, m2);

    nmg_km(m1);
    nmg_km(m2);

    return result;
}

int
main(int argc, char **argv)
{
    if (argc > 1) {
	bu_exit(1, "Usage: %s\n", argv[0]);
    }

    if (testcase_nmg_mm() < 0) {
	bu_exit(1, "Test Case by nmg_mm failed!\n");
    }

    if (testcase_nmg_mmr() < 0) {
	bu_exit(1, "Test Case by nmg_mmr failed!\n");
    }

    if (testcase_nmg_mrsv() < 0) {
	bu_exit(1, "Test Case by nmg_mrsv failed!\n");
    }

    bu_log("All unit tests successed.\n");
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