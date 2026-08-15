/*                 T E S T _ L I N E E D I T . C
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu.h"

int
main(int argc, char **argv)
{
    bu_cmd_completion_mode_t mode = BU_CMD_COMPLETE_OFF;
    struct bu_lineedit_palette palette = BU_LINEEDIT_PALETTE_INIT_ZERO;
    struct bu_cmd_completion_layout layout = BU_CMD_COMPLETION_LAYOUT_INIT_ZERO;

    bu_setprogname(argv[0]);

    if (bu_cmd_completion_mode_parse("filter", &mode) != BRLCAD_OK || mode != BU_CMD_COMPLETE_FILTER) return 1;
    if (bu_cmd_completion_mode_parse("CYCLE", &mode) != BRLCAD_OK || mode != BU_CMD_COMPLETE_CYCLE) return 2;
    if (bu_cmd_completion_mode_parse("prefix", &mode) != BRLCAD_OK || mode != BU_CMD_COMPLETE_PREFIX) return 3;
    if (bu_cmd_completion_mode_parse("legacy", &mode) != BRLCAD_OK || mode != BU_CMD_COMPLETE_LEGACY) return 4;
    if (bu_cmd_completion_mode_parse("off", &mode) != BRLCAD_OK || mode != BU_CMD_COMPLETE_OFF) return 5;
    if (bu_cmd_completion_mode_parse("bogus", &mode) != BRLCAD_ERROR) return 6;
    if (!BU_STR_EQUAL(bu_cmd_completion_mode_name(BU_CMD_COMPLETE_FILTER), "filter")) return 7;

    bu_setenv(BU_CMD_COMPLETION_MODE_ENV, "cycle", 1);
    bu_setenv("BRLCAD_TEST_COMPLETION_MODE", "prefix", 1);
    if (bu_cmd_completion_mode_from_env("BRLCAD_TEST_COMPLETION_MODE", BU_CMD_COMPLETE_OFF) != BU_CMD_COMPLETE_PREFIX) return 8;
    bu_setenv("BRLCAD_TEST_COMPLETION_MODE", "invalid", 1);
    if (bu_cmd_completion_mode_from_env("BRLCAD_TEST_COMPLETION_MODE", BU_CMD_COMPLETE_OFF) != BU_CMD_COMPLETE_CYCLE) return 9;
    bu_setenv("BRLCAD_GSH_COMPLETION_MODE", "off", 1);
    if (bu_cmd_completion_mode_from_env("BRLCAD_GSH_COMPLETION_MODE", BU_CMD_COMPLETE_FILTER) != BU_CMD_COMPLETE_OFF) return 10;
    bu_setenv("BRLCAD_QGED_COMPLETION_MODE", "prefix", 1);
    if (bu_cmd_completion_mode_from_env("BRLCAD_QGED_COMPLETION_MODE", BU_CMD_COMPLETE_FILTER) != BU_CMD_COMPLETE_PREFIX) return 11;
    bu_setenv(BU_CMD_COMPLETION_MODE_ENV, "cycle", 1);
    if (bu_cmd_completion_mode_from_env("BRLCAD_QGED_COMPLETION_MODE", BU_CMD_COMPLETE_OFF) != BU_CMD_COMPLETE_PREFIX) return 12;
    bu_setenv(BU_CMD_COMPLETION_MODE_ENV, "", 1);
    bu_setenv("BRLCAD_TEST_COMPLETION_MODE", "", 1);
    bu_setenv("BRLCAD_GSH_COMPLETION_MODE", "", 1);
    bu_setenv("BRLCAD_QGED_COMPLETION_MODE", "", 1);

    if (bu_cmd_completion_candidate_budget(80, 5) != 135 ||
	bu_cmd_completion_candidate_budget(1, 1) != 2 ||
	bu_cmd_completion_candidate_budget((size_t)-1, (size_t)-1) != 65536)
	return 45;

    {
	const char *short_candidates[] = {"beta", "alpha", "alpine"};
	if (bu_cmd_completion_layout_create(&layout, short_candidates, 3, 12, 2) != BRLCAD_OK) return 13;
	if (layout.summarized || layout.line_count != 2) return 14;
	if (!BU_STR_EQUAL(layout.lines[0], "alpha   beta") ||
	    !BU_STR_EQUAL(layout.lines[1], "alpine")) return 15;
	bu_cmd_completion_layout_clear(&layout);
    }

    {
	char names[323][8];
	const char *long_candidates[323];
	for (int i = 0; i < 3; i++) {
	    snprintf(names[i], sizeof(names[i]), "aab%d", i);
	    long_candidates[i] = names[i];
	}
	for (int i = 0; i < 20; i++) {
	    snprintf(names[3 + i], sizeof(names[3 + i]), "aac%02d", i);
	    long_candidates[3 + i] = names[3 + i];
	}
	for (int i = 0; i < 300; i++) {
	    snprintf(names[23 + i], sizeof(names[23 + i]), "d5m%03d", i);
	    long_candidates[23 + i] = names[23 + i];
	}
	if (bu_cmd_completion_layout_create(&layout, long_candidates, 323, 52, 5) != BRLCAD_OK) return 13;
	if (!layout.summarized || layout.line_count < 2 || layout.line_count > 5) return 14;
	for (size_t i = 0; i < layout.line_count; i++) {
	    if (strlen(layout.lines[i]) > 52 || strstr(layout.lines[i], "more)")) return 15;
	}
	bu_cmd_completion_layout_clear(&layout);
	if (bu_cmd_completion_layout_create(&layout, long_candidates, 323, 52, 1) != BRLCAD_OK) return 16;
	if (!layout.summarized || layout.line_count != 1) return 17;
	if (!BU_STR_EQUAL(layout.lines[0],
		"aab (3 matches)  aac (20 matches)  d5m (300 matches)")) return 18;
	bu_cmd_completion_layout_clear(&layout);
	if (bu_cmd_completion_layout_create(&layout, long_candidates, 323, 24, 1) != BRLCAD_OK) return 19;
	if (!layout.summarized || layout.line_count != 1 || strlen(layout.lines[0]) > 24) return 31;
	bu_cmd_completion_layout_clear(&layout);
    }

    {
	const char *broad[] = {
	    "alpha", "beta", "charlie", "delta", "echo", "foxtrot",
	    "golf", "hotel", "india", "juliet", "kilo", "lima"
	};
	if (bu_cmd_completion_layout_create(&layout, broad, 12, 28, 1) != BRLCAD_OK)
	    return 32;
	if (!layout.summarized || layout.line_count != 1 ||
		!strstr(layout.lines[0], "more)") || strstr(layout.lines[0], "* ("))
	    return 33;
	bu_cmd_completion_layout_clear(&layout);
    }

    {
	const size_t candidate_count = 10000;
	char (*names)[32] = (char (*)[32])bu_calloc(candidate_count, sizeof(*names),
	    "large completion layout names");
	const char **candidates = (const char **)bu_calloc(candidate_count,
	    sizeof(char *), "large completion layout candidates");
	for (size_t i = 0; i < candidate_count; i++) {
	    snprintf(names[i], sizeof(names[i]), "object_%05zu_variant", i);
	    candidates[i] = names[i];
	}
	int64_t start = bu_gettime();
	int layout_ret = bu_cmd_completion_layout_create(&layout, candidates,
	    candidate_count, 300, 100);
	int64_t elapsed = bu_gettime() - start;
	int invalid = layout_ret != BRLCAD_OK || !layout.line_count ||
	    layout.line_count > 100 || elapsed > 5000000;
	bu_cmd_completion_layout_clear(&layout);
	bu_free((void *)candidates, "large completion layout candidates");
	bu_free(names, "large completion layout names");
	/* The former unbounded frontier search took roughly 20 seconds for this
	 * case in a Debug build.  Five seconds leaves ample noisy-host margin
	 * while preventing that quadratic behavior from returning unnoticed. */
	if (invalid) return 44;
    }

    {
	const size_t candidate_count = 10000;
	char (*names)[8] = (char (*)[8])bu_calloc(candidate_count, sizeof(*names),
	    "narrow tall completion names");
	const char **candidates = (const char **)bu_calloc(candidate_count,
	    sizeof(char *), "narrow tall completion candidates");
	for (size_t i = 0; i < candidate_count; i++) {
	    snprintf(names[i], sizeof(names[i]), "%05zu", i);
	    candidates[i] = names[i];
	}
	int64_t start = bu_gettime();
	int layout_ret = bu_cmd_completion_layout_create(&layout, candidates,
	    candidate_count, 5, candidate_count);
	int64_t elapsed = bu_gettime() - start;
	int invalid = layout_ret != BRLCAD_OK || layout.summarized ||
	    layout.line_count != candidate_count || elapsed > 5000000;
	bu_cmd_completion_layout_clear(&layout);
	bu_free((void *)candidates, "narrow tall completion candidates");
	bu_free(names, "narrow tall completion names");
	if (invalid) return 47;
    }

    {
	const char branch_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZab";
	char names[sizeof(branch_chars) - 1][48];
	const char *candidates[sizeof(branch_chars) - 1];
	int saw_compact_count = 0;
	for (size_t i = 0; i < sizeof(branch_chars) - 1; i++) {
	    snprintf(names[i], sizeof(names[i]), "%c_long_unique_completion_object",
		    branch_chars[i]);
	    candidates[i] = names[i];
	}
	if (bu_cmd_completion_layout_create(&layout, candidates,
		sizeof(branch_chars) - 1, 80, 5) != BRLCAD_OK) return 34;
	if (!layout.summarized || layout.line_count != 5) return 35;
	for (size_t i = 0; i < layout.line_count; i++) {
	    if (strlen(layout.lines[i]) > 80 || strstr(layout.lines[i], "more)"))
		return 36;
	    if (strstr(layout.lines[i], "(1)"))
		saw_compact_count = 1;
	}
	if (!saw_compact_count) return 37;
	bu_cmd_completion_layout_clear(&layout);
    }

    {
	const char *wide[] = {"\xe7\x95\x8c", "a"};
	if (bu_cmd_completion_layout_create(&layout, wide, 2, 5, 1) != BRLCAD_OK)
	    return 38;
	if (layout.summarized || layout.line_count != 1 ||
		!BU_STR_EQUAL(layout.lines[0], "a  \xe7\x95\x8c"))
	    return 39;
	bu_cmd_completion_layout_clear(&layout);
    }

    {
	const char *combining[] = {"e\xcc\x81", "x"};
	if (bu_cmd_completion_layout_create(&layout, combining, 2, 4, 1) != BRLCAD_OK)
	    return 40;
	if (layout.summarized || layout.line_count != 1 ||
		!BU_STR_EQUAL(layout.lines[0], "e\xcc\x81  x"))
	    return 41;
	bu_cmd_completion_layout_clear(&layout);
    }

    {
	const char *unsafe[] = {"bad\x1b[2J\nname"};
	if (bu_cmd_completion_layout_create(&layout, unsafe, 1, 80, 1) != BRLCAD_OK)
	    return 42;
	if (layout.line_count != 1 || strchr(layout.lines[0], '\x1b') ||
		strchr(layout.lines[0], '\n') || !strstr(layout.lines[0], "\\x1b") ||
		!strstr(layout.lines[0], "\\n"))
	    return 43;
	bu_cmd_completion_layout_clear(&layout);
    }

    if (argc != 2) return 20;
    if (bu_lineedit_palette_load_file(&palette, argv[1]) != BRLCAD_OK) return 21;
    if (!(palette.roles[BU_LINEEDIT_ROLE_COMMAND].flags & BU_LINEEDIT_STYLE_COLOR)) return 22;
    if (palette.roles[BU_LINEEDIT_ROLE_COMMAND].rgb[0] != 0x12 ||
	palette.roles[BU_LINEEDIT_ROLE_COMMAND].rgb[1] != 0x34 ||
	palette.roles[BU_LINEEDIT_ROLE_COMMAND].rgb[2] != 0x56) return 23;
    if ((palette.roles[BU_LINEEDIT_ROLE_OPTION].flags &
	 (BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM)) !=
	(BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM)) return 24;
    if ((palette.roles[BU_LINEEDIT_ROLE_COMPLETION_PREVIEW].flags &
	 (BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM)) !=
	(BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET)) return 25;
    if (!BU_STR_EQUAL(bu_lineedit_role_name(BU_LINEEDIT_ROLE_COMPLETION_PREVIEW),
	"completion-preview")) return 26;

    bu_lineedit_palette_init(&palette);
    bu_setenv(BU_LINEEDIT_COLORS_ENV, argv[1], 1);
    if (bu_lineedit_palette_load_user(&palette) != BRLCAD_OK) return 27;
    if (!(palette.roles[BU_LINEEDIT_ROLE_COMMAND].flags & BU_LINEEDIT_STYLE_COLOR)) return 28;
    bu_lineedit_palette_init(&palette);
    bu_setenv(BU_LINEEDIT_COLORS_ENV, "off", 1);
    if (bu_lineedit_palette_load_user(&palette) != BRLCAD_OK) return 29;
    if (palette.roles[BU_LINEEDIT_ROLE_COMMAND].flags) return 30;
    bu_setenv(BU_LINEEDIT_COLORS_ENV, "", 1);

    return 0;
}
