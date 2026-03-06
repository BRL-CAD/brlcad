/*                    E D I T _ D E S C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file edit_desc.cpp
 *
 * Unit tests for rt_edit_type_to_json() / rt_edit_prim_desc_to_json().
 *
 * For each primitive that implements ft_edit_desc() this test:
 *   1. Calls rt_edit_type_to_json() and verifies BRLCAD_OK is returned.
 *   2. Checks the JSON string is non-empty.
 *   3. Checks that a few required top-level keys are present in the output.
 *   4. Checks that at least one expected cmd_id value appears as a number
 *      in the JSON string (verbatim substring search — no JSON parser
 *      needed for these structural checks).
 */

#include "common.h"

#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"


/* Check that the JSON output for prim_type_id:
 *   - returns BRLCAD_OK
 *   - contains the expected prim_type string
 *   - contains the expected first cmd_id value as a JSON integer
 *   - contains "commands"
 */
static int
check_prim(const char *name, int prim_type_id,
	   const char *expected_prim_type, int expected_first_cmd_id)
{
    struct bu_vls json = BU_VLS_INIT_ZERO;

    int ret = rt_edit_type_to_json(&json, prim_type_id);
    if (ret != BRLCAD_OK) {
	bu_log("FAIL [%s]: rt_edit_type_to_json returned error\n", name);
	bu_vls_free(&json);
	return 1;
    }

    const char *js = bu_vls_cstr(&json);
    if (!js || js[0] == '\0') {
	bu_log("FAIL [%s]: rt_edit_type_to_json produced empty string\n", name);
	bu_vls_free(&json);
	return 1;
    }

    /* Check prim_type value */
    if (!strstr(js, expected_prim_type)) {
	bu_log("FAIL [%s]: JSON missing prim_type '%s'\n", name, expected_prim_type);
	bu_vls_free(&json);
	return 1;
    }

    /* Check "commands" key */
    if (!strstr(js, "\"commands\"")) {
	bu_log("FAIL [%s]: JSON missing \"commands\" key\n", name);
	bu_vls_free(&json);
	return 1;
    }

    /* Check expected first cmd_id as a numeric substring.
     * The serialiser emits  "cmd_id": N  so search for that. */
    char needle[64];
    snprintf(needle, sizeof(needle), "\"cmd_id\": %d", expected_first_cmd_id);
    if (!strstr(js, needle)) {
	bu_log("FAIL [%s]: JSON missing '%s'\n", name, needle);
	bu_log("  JSON was: %s\n", js);
	bu_vls_free(&json);
	return 1;
    }

    bu_log("PASS [%s]: prim_type='%s', first cmd_id=%d, %zu bytes\n",
	   name, expected_prim_type, expected_first_cmd_id, bu_vls_strlen(&json));

    bu_vls_free(&json);
    return 0;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return BRLCAD_ERROR;

    int fail = 0;

    /* ------------------------------------------------------------------
     * Primitives with ft_edit_desc() implementations.
     * Arguments: display name, prim_type_id, expected prim_type string,
     *            first cmd_id to search for.
     * ------------------------------------------------------------------ */

    /* TOR: ECMD_TOR_R1 = 1021 */
    fail += check_prim("TOR",  ID_TOR,      "\"tor\"",      1021);

    /* TGC: ECMD_TGC_SCALE_H = 2027 */
    fail += check_prim("TGC",  ID_TGC,      "\"tgc\"",      2027);

    /* ELL: ECMD_ELL_SCALE_A = 3039 */
    fail += check_prim("ELL",  ID_ELL,      "\"ell\"",      3039);

    /* EPA: ECMD_EPA_H = 19050 */
    fail += check_prim("EPA",  ID_EPA,      "\"epa\"",      19050);

    /* EHY: ECMD_EHY_H = 20053 */
    fail += check_prim("EHY",  ID_EHY,      "\"ehy\"",      20053);

    /* ETO: ECMD_ETO_R = 21057 */
    fail += check_prim("ETO",  ID_ETO,      "\"eto\"",      21057);

    /* HYP: ECMD_HYP_H = 38127 */
    fail += check_prim("HYP",  ID_HYP,      "\"hyp\"",      38127);

    /* RPC: ECMD_RPC_B = 17043 */
    fail += check_prim("RPC",  ID_RPC,      "\"rpc\"",      17043);

    /* RHC: ECMD_RHC_B = 18046 */
    fail += check_prim("RHC",  ID_RHC,      "\"rhc\"",      18046);

    /* PART: ECMD_PART_H = 16088 */
    fail += check_prim("PART", ID_PARTICLE, "\"part\"",     16088);

    /* SUPERELL: ECMD_SUPERELL_SCALE_A = 35113 */
    fail += check_prim("SUPERELL", ID_SUPERELL, "\"superell\"", 35113);

    /* CLINE: ECMD_CLINE_SCALE_H = 29077 */
    fail += check_prim("CLINE", ID_CLINE,  "\"cline\"",     29077);

    /* DSP: ECMD_DSP_FNAME = 25056 */
    fail += check_prim("DSP",  ID_DSP,      "\"dsp\"",      25056);

    /* EBM: ECMD_EBM_FNAME = 12053 */
    fail += check_prim("EBM",  ID_EBM,      "\"ebm\"",      12053);

    /* VOL: ECMD_VOL_FNAME = 13052  (first in vol_cmds) */
    fail += check_prim("VOL",  ID_VOL,      "\"vol\"",      13052);

    /* PIPE: ECMD_PIPE_PT_OD = 15065 */
    fail += check_prim("PIPE", ID_PIPE,     "\"pipe\"",     15065);

    /* COMB: ECMD_COMB_ADD_MEMBER = 12001 */
    fail += check_prim("COMB", ID_COMBINATION, "\"comb\"",  12001);

    /* ------------------------------------------------------------------
     * Verify that primitives without ft_edit_desc return BRLCAD_ERROR.
     * ------------------------------------------------------------------ */
    {
	struct bu_vls json = BU_VLS_INIT_ZERO;
	/* ARB8 (ID=4) has no ft_edit_desc */
	int ret = rt_edit_type_to_json(&json, 4 /* ID_ARB8 */);
	if (ret == BRLCAD_ERROR) {
	    bu_log("PASS [ARB8-no-desc]: correctly returned BRLCAD_ERROR\n");
	} else {
	    bu_log("FAIL [ARB8-no-desc]: expected BRLCAD_ERROR, got BRLCAD_OK\n");
	    fail++;
	}
	bu_vls_free(&json);
    }

    if (fail)
	bu_exit(1, "edit_desc: %d test(s) FAILED\n", fail);

    bu_log("edit_desc: all tests PASSED\n");
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
