/*                  D R A W _ T E S T _ O P T S . H
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
