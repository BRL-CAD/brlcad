/*                 T E S T _ L I N E E D I T . C
 * BRL-CAD
 */

#include "common.h"
#include "bu.h"

int
main(int argc, char **argv)
{
    bu_cmd_completion_mode_t mode = BU_CMD_COMPLETE_OFF;
    struct bu_lineedit_palette palette = BU_LINEEDIT_PALETTE_INIT_ZERO;

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

    if (argc != 2) return 10;
    if (bu_lineedit_palette_load_file(&palette, argv[1]) != BRLCAD_OK) return 11;
    if (!(palette.roles[BU_LINEEDIT_ROLE_COMMAND].flags & BU_LINEEDIT_STYLE_COLOR)) return 12;
    if (palette.roles[BU_LINEEDIT_ROLE_COMMAND].rgb[0] != 0x12 ||
	palette.roles[BU_LINEEDIT_ROLE_COMMAND].rgb[1] != 0x34 ||
	palette.roles[BU_LINEEDIT_ROLE_COMMAND].rgb[2] != 0x56) return 13;
    if ((palette.roles[BU_LINEEDIT_ROLE_OPTION].flags &
	 (BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM)) !=
	(BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM)) return 14;
    if ((palette.roles[BU_LINEEDIT_ROLE_COMPLETION_PREVIEW].flags &
	 (BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET | BU_LINEEDIT_STYLE_DIM)) !=
	(BU_LINEEDIT_STYLE_COLOR | BU_LINEEDIT_STYLE_DIM_SET)) return 15;
    if (!BU_STR_EQUAL(bu_lineedit_role_name(BU_LINEEDIT_ROLE_COMPLETION_PREVIEW),
	"completion-preview")) return 16;

    bu_lineedit_palette_init(&palette);
    bu_setenv(BU_LINEEDIT_COLORS_ENV, argv[1], 1);
    if (bu_lineedit_palette_load_user(&palette) != BRLCAD_OK) return 17;
    if (!(palette.roles[BU_LINEEDIT_ROLE_COMMAND].flags & BU_LINEEDIT_STYLE_COLOR)) return 18;
    bu_lineedit_palette_init(&palette);
    bu_setenv(BU_LINEEDIT_COLORS_ENV, "off", 1);
    if (bu_lineedit_palette_load_user(&palette) != BRLCAD_OK) return 19;
    if (palette.roles[BU_LINEEDIT_ROLE_COMMAND].flags) return 20;
    bu_setenv(BU_LINEEDIT_COLORS_ENV, "", 1);

    return 0;
}
