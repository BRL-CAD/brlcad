/*                 T E S T _ L I N E E D I T . C
 * BRL-CAD
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
    bu_setenv(BU_CMD_COMPLETION_MODE_ENV, "", 1);
    bu_setenv("BRLCAD_TEST_COMPLETION_MODE", "", 1);

    {
	const char *short_candidates[] = {"beta", "alpha", "alpine"};
	if (bu_cmd_completion_layout_create(&layout, short_candidates, 3, 12, 2) != BRLCAD_OK) return 10;
	if (layout.summarized || layout.line_count != 2) return 11;
	if (!BU_STR_EQUAL(layout.lines[0], "alpha   beta") ||
	    !BU_STR_EQUAL(layout.lines[1], "alpine")) return 12;
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
