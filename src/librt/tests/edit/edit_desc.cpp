/*                    E D I T _ D E S C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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

#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/edit.h"


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

    if (!strstr(js, "\"selection_domain\"") ||
	!strstr(js, "\"coordinate_space\"") ||
	!strstr(js, "\"manipulator\"") ||
	!strstr(js, "\"semantic\"")) {
	bu_log("FAIL [%s]: JSON missing interaction metadata\n", name);
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


static int
check_descriptor_audit(void)
{
    int failures = 0;
    int descriptor_count = 0;
    int controls[RT_EDIT_CONTROL_UNSUPPORTED + 1] = {0};
    extern const struct rt_edit_functab EDOBJ[];

    for (int type = 0; EDOBJ[type].magic == RT_FUNCTAB_MAGIC; ++type) {
	if (!EDOBJ[type].ft_edit_desc)
	    continue;
	const struct rt_edit_prim_desc *desc = EDOBJ[type].ft_edit_desc();
	struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
	if (rt_edit_prim_desc_validate(&diagnostic, desc) != BRLCAD_OK) {
	    bu_log("FAIL [type %d]: descriptor invalid: %s\n", type,
		bu_vls_cstr(&diagnostic));
	    failures++;
	} else {
	    descriptor_count++;
	    for (int ci = 0; ci < desc->ncmd; ++ci) {
		const enum rt_edit_control_class control =
		    rt_edit_cmd_control_class(desc, &desc->cmds[ci]);
		struct bu_vls name = BU_VLS_INIT_ZERO;
		if (rt_edit_cmd_name(&name, desc, &desc->cmds[ci]) !=
			BRLCAD_OK || !bu_vls_strlen(&name)) {
		    bu_log("FAIL [%s]: command %d has no canonical name\n",
			desc->prim_type, ci);
		    failures++;
		}
		bu_vls_free(&name);
		if (control < RT_EDIT_CONTROL_GENERATED ||
		    control > RT_EDIT_CONTROL_UNSUPPORTED) {
		    bu_log("FAIL [%s]: command %d is not classified\n",
			desc->prim_type, ci);
		    failures++;
		} else {
		    controls[control]++;
		}
		if (control == RT_EDIT_CONTROL_GENERATED &&
		    !EDOBJ[type].ft_edit_get_values) {
		    bu_log("FAIL [%s]: generated command %d has no readback\n",
			desc->prim_type, desc->cmds[ci].cmd_id);
		    failures++;
		}
		const struct rt_edit_interaction_desc interaction =
		    rt_edit_cmd_interaction(desc, &desc->cmds[ci]);
		if (desc->command_interactions &&
		    interaction.coordinate_space == RT_EDIT_COORDINATE_INFER) {
		    bu_log("FAIL [%s]: command %d has inferred coordinate space\n",
			desc->prim_type, desc->cmds[ci].cmd_id);
		    failures++;
		}
	    }
	}
	bu_vls_free(&diagnostic);
    }

    for (int control = RT_EDIT_CONTROL_GENERATED;
	control <= RT_EDIT_CONTROL_UNSUPPORTED; ++control) {
	if (!controls[control]) {
	    bu_log("FAIL: no commands use control class %d\n", control);
	    failures++;
	}
    }

    bu_log("%s: classified commands generated=%d action=%d custom=%d unsupported=%d\n",
	failures ? "FAIL" : "PASS", controls[RT_EDIT_CONTROL_GENERATED],
	controls[RT_EDIT_CONTROL_ACTION], controls[RT_EDIT_CONTROL_CUSTOM],
	controls[RT_EDIT_CONTROL_UNSUPPORTED]);

    const struct rt_edit_prim_desc *ell = EDOBJ[ID_ELL].ft_edit_desc();
    if (ell && ell->ncmd > 0) {
	struct rt_edit_cmd_desc invalid_command = ell->cmds[0];
	invalid_command.name = NULL;
	struct rt_edit_prim_desc invalid_desc = *ell;
	invalid_desc.ncmd = 1;
	invalid_desc.cmds = &invalid_command;
	struct bu_vls diagnostic = BU_VLS_INIT_ZERO;
	if (rt_edit_prim_desc_validate(&diagnostic, &invalid_desc) !=
		BRLCAD_ERROR || !bu_vls_strlen(&diagnostic)) {
	    bu_log("FAIL: malformed descriptor was not rejected diagnostically\n");
	    failures++;
	}
	bu_vls_free(&diagnostic);

	invalid_desc = *ell;
	invalid_desc.control_class = RT_EDIT_CONTROL_INHERIT;
	diagnostic = BU_VLS_INIT_ZERO;
	if (rt_edit_prim_desc_validate(&diagnostic, &invalid_desc) !=
		BRLCAD_ERROR || !bu_vls_strlen(&diagnostic)) {
	    bu_log("FAIL: inherited descriptor default was not rejected\n");
	    failures++;
	}
	bu_vls_free(&diagnostic);

	const enum rt_edit_control_class invalid_control =
	    (enum rt_edit_control_class)(RT_EDIT_CONTROL_UNSUPPORTED + 1);
	invalid_desc = *ell;
	invalid_desc.ncmd = 1;
	invalid_desc.command_controls = &invalid_control;
	diagnostic = BU_VLS_INIT_ZERO;
	if (rt_edit_prim_desc_validate(&diagnostic, &invalid_desc) !=
		BRLCAD_ERROR || !bu_vls_strlen(&diagnostic)) {
	    bu_log("FAIL: invalid command control was not rejected\n");
	    failures++;
	}
	bu_vls_free(&diagnostic);

	struct rt_edit_interaction_desc invalid_interaction =
	    ell->command_interactions[0];
	invalid_interaction.coordinate_space = RT_EDIT_COORDINATE_INFER;
	invalid_desc = *ell;
	invalid_desc.ncmd = 1;
	invalid_desc.command_interactions = &invalid_interaction;
	diagnostic = BU_VLS_INIT_ZERO;
	if (rt_edit_prim_desc_validate(&diagnostic, &invalid_desc) !=
		BRLCAD_ERROR || !bu_vls_strlen(&diagnostic)) {
	    bu_log("FAIL: inferred interaction coordinates were not rejected\n");
	    failures++;
	}
	bu_vls_free(&diagnostic);

	const enum rt_edit_param_semantic invalid_semantic =
	    (enum rt_edit_param_semantic)(RT_EDIT_SEMANTIC_PROPERTY + 1);
	invalid_interaction = ell->command_interactions[0];
	invalid_interaction.parameter_semantics = &invalid_semantic;
	invalid_interaction.parameter_semantic_count = 1;
	invalid_desc = *ell;
	invalid_desc.ncmd = 1;
	invalid_desc.command_interactions = &invalid_interaction;
	diagnostic = BU_VLS_INIT_ZERO;
	if (rt_edit_prim_desc_validate(&diagnostic, &invalid_desc) !=
		BRLCAD_ERROR || !bu_vls_strlen(&diagnostic)) {
	    bu_log("FAIL: invalid parameter semantic was not rejected\n");
	    failures++;
	}
	bu_vls_free(&diagnostic);

	struct rt_edit_param_desc invalid_list = ell->cmds[0].params[0];
	invalid_list.type = RT_EDIT_PARAM_INTEGER_LIST;
	invalid_list.nenum = 0;
	invalid_command = ell->cmds[0];
	invalid_command.params = &invalid_list;
	invalid_desc = *ell;
	invalid_desc.ncmd = 1;
	invalid_desc.cmds = &invalid_command;
	diagnostic = BU_VLS_INIT_ZERO;
	if (rt_edit_prim_desc_validate(&diagnostic, &invalid_desc) !=
		BRLCAD_ERROR || !bu_vls_strlen(&diagnostic)) {
	    bu_log("FAIL: invalid list parameter was not rejected\n");
	    failures++;
	}
	bu_vls_free(&diagnostic);
    } else {
	bu_log("FAIL: ELL descriptor unavailable for validation test\n");
	failures++;
    }

    bu_log("%s: validated %d registered edit descriptors\n",
	failures ? "FAIL" : "PASS", descriptor_count);
    return failures;
}


static int
check_interactions(void)
{
    extern const struct rt_edit_functab EDOBJ[];
    const struct rt_edit_prim_desc *ell = EDOBJ[ID_ELL].ft_edit_desc();
    const struct rt_edit_prim_desc *arb = EDOBJ[ID_ARB8].ft_edit_desc();
    const struct rt_edit_prim_desc *bot = EDOBJ[ID_BOT].ft_edit_desc();
    const struct rt_edit_prim_desc *sketch = EDOBJ[ID_SKETCH].ft_edit_desc();
    const struct rt_edit_prim_desc *tor = EDOBJ[ID_TOR].ft_edit_desc();
    const struct rt_edit_interaction_desc ell_interaction =
	rt_edit_cmd_interaction(ell, &ell->cmds[0]);
    const struct rt_edit_interaction_desc arb_interaction =
	rt_edit_cmd_interaction(arb, &arb->cmds[0]);
    const struct rt_edit_interaction_desc bot_interaction =
	rt_edit_cmd_interaction(bot, &bot->cmds[6]);
    const struct rt_edit_interaction_desc sketch_interaction =
	rt_edit_cmd_interaction(sketch, &sketch->cmds[18]);
    const struct rt_edit_interaction_desc default_interaction =
	rt_edit_cmd_interaction(tor, &tor->cmds[0]);
    struct bu_vls sketch_json = BU_VLS_INIT_ZERO;
    const int json_status = rt_edit_type_to_json(&sketch_json, ID_SKETCH);

    if (ell_interaction.manipulator != RT_EDIT_MANIPULATOR_AXIS ||
	ell_interaction.coordinate_space != RT_EDIT_COORDINATE_SCALAR ||
	arb_interaction.selection_domain != RT_EDIT_SELECTION_VERTEX ||
	arb_interaction.manipulator != RT_EDIT_MANIPULATOR_INDEXED_SET ||
	bot_interaction.selection_domain != RT_EDIT_SELECTION_VERTEX ||
	sketch_interaction.coordinate_space != RT_EDIT_COORDINATE_OBJECT ||
	sketch_interaction.manipulator != RT_EDIT_MANIPULATOR_PLANE ||
	default_interaction.selection_domain != RT_EDIT_SELECTION_NONE ||
	default_interaction.coordinate_space != RT_EDIT_COORDINATE_INFER ||
	default_interaction.manipulator != RT_EDIT_MANIPULATOR_NONE ||
	rt_edit_param_semantic_get(ell, &ell->cmds[0], 0) !=
	    RT_EDIT_SEMANTIC_DISTANCE ||
	rt_edit_param_semantic_get(arb, &arb->cmds[0], 0) !=
	    RT_EDIT_SEMANTIC_INDEX ||
	rt_edit_param_semantic_get(bot, &bot->cmds[6], 0) !=
	    RT_EDIT_SEMANTIC_DELTA ||
	rt_edit_param_semantic_get(sketch, &sketch->cmds[17], 2) !=
	    RT_EDIT_SEMANTIC_ANGLE ||
	rt_edit_param_semantic_get(tor, &tor->cmds[0], 0) !=
	    RT_EDIT_SEMANTIC_INFER ||
	json_status != BRLCAD_OK ||
	!strstr(bu_vls_cstr(&sketch_json),
	    "\"selection_domain\": \"segment\"") ||
	!strstr(bu_vls_cstr(&sketch_json),
	    "\"coordinate_space\": \"parametric_2d\"") ||
	!strstr(bu_vls_cstr(&sketch_json),
	    "\"manipulator\": \"indexed_set\"") ||
	!strstr(bu_vls_cstr(&sketch_json),
	    "\"semantic\": \"angle\"") ||
	!strstr(bu_vls_cstr(&sketch_json), "\"type\": \"point2\"") ||
	!strstr(bu_vls_cstr(&sketch_json), "\"type\": \"vector2\"") ||
	!strstr(bu_vls_cstr(&sketch_json), "\"type\": \"integer_list\"") ||
	!strstr(bu_vls_cstr(&sketch_json), "\"min_count\": 1")) {
	bu_log("FAIL: representative interaction descriptors are incomplete\n");
	bu_vls_free(&sketch_json);
	return 1;
    }

    bu_vls_free(&sketch_json);
    bu_log("PASS: representative interaction descriptors are source-neutral\n");
    return 0;
}


int
rt_edit_test_edit_desc(void)
{
    int fail = 0;

    fail += check_descriptor_audit();
    fail += check_interactions();

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

    /* HALF: ECMD_HALF_SET_D = 6001 */
    fail += check_prim("HALF", ID_HALF,     "\"half\"",     6001);

    /* SPH: ECMD_SPH_SET_V = 10001 */
    fail += check_prim("SPH",  ID_SPH,      "\"sph\"",      10001);

    /* REC: ECMD_REC_SET_V = 7001 */
    fail += check_prim("REC",  ID_REC,      "\"rec\"",      7001);

    /* ARB8: ECMD_ARB_MOVE_FACE = 4013 */
    fail += check_prim("ARB8", ID_ARB8,     "\"arb8\"",     4013);

    /* ------------------------------------------------------------------
     * Verify that primitives without ft_edit_desc return BRLCAD_ERROR.
     * ------------------------------------------------------------------ */
    {
	struct bu_vls json = BU_VLS_INIT_ZERO;
	/* HF has no ft_edit_desc. */
	int ret = rt_edit_type_to_json(&json, ID_HF);
	if (ret == BRLCAD_ERROR) {
	    bu_log("PASS [HF-no-desc]: correctly returned BRLCAD_ERROR\n");
	} else {
	    bu_log("FAIL [HF-no-desc]: expected BRLCAD_ERROR, got BRLCAD_OK\n");
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
