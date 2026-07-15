/*                  D R A W _ T E S T _ O P T S . H
 * BRL-CAD
 *
 * Shared native option record for the image-based GED draw tests.
 */

#ifndef GED_DRAW_TEST_OPTS_H
#define GED_DRAW_TEST_OPTS_H

#include "bu/cmdschema.h"

struct ged_draw_test_args {
    int help;
    int enable_unstable;
    int continue_on_failure;
    int keep_images;
};

static const struct bu_cmd_operand ged_draw_test_operands[] = {
    BU_CMD_OPERAND("control_directory", BU_CMD_VALUE_FILE, 1, 1,
	"Directory containing control images", NULL),
    BU_CMD_OPERAND_NULL
};

#endif /* GED_DRAW_TEST_OPTS_H */
