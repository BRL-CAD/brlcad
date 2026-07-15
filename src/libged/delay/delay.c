/*                         D E L A Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/delay.c
 *
 * The delay command.
 *
 */

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "bsocket.h"

#include "../ged_private.h"

static int
delay_seconds_validate(struct bu_vls *msg, const char *arg)
{
    char *end = NULL;
    long value = 0;

    errno = 0;
    value = strtol(arg, &end, 0);
    if (errno != ERANGE && end && !*end && value >= 0 && value <= INT_MAX)
	return 0;
    if (msg)
	bu_vls_printf(msg, "seconds must be a nonnegative integer: %s\n", arg);
    return -1;
}


static int
delay_useconds_validate(struct bu_vls *msg, const char *arg)
{
    char *end = NULL;
    long value = 0;

    errno = 0;
    value = strtol(arg, &end, 0);
    if (errno != ERANGE && end && !*end && value >= 0 && value < 1000000)
	return 0;
    if (msg)
	bu_vls_printf(msg, "microseconds must be an integer from 0 through 999999: %s\n", arg);
    return -1;
}


static int
delay_value(const char *arg, bu_cmd_value_validate_t validate, int *value)
{
    char *end = NULL;
    long parsed = 0;

    if (!arg || !value || !validate || validate(NULL, arg) != 0)
	return -1;
    errno = 0;
    parsed = strtol(arg, &end, 0);
    if (errno == ERANGE || !end || *end || parsed > INT_MAX || parsed < INT_MIN)
	return -1;
    *value = (int)parsed;
    return 0;
}


static const struct bu_cmd_operand delay_operands[] = {
    BU_CMD_OPERAND_VALIDATE("sec", BU_CMD_VALUE_INTEGER, 1, 1, delay_seconds_validate,
	"Nonnegative seconds to delay", NULL),
    BU_CMD_OPERAND_VALIDATE("usec", BU_CMD_VALUE_INTEGER, 1, 1, delay_useconds_validate,
	"Microseconds to delay (0 through 999999)", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema delay_cmd_schema = {
    "delay", "Pause command processing", NULL, delay_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    {NULL}
};


int
ged_delay_core(struct ged *gedp, int argc, const char *argv[])
{
    struct timeval tv;
    static const char *usage = "sec usec";
    int seconds = 0;
    int useconds = 0;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (delay_value(argv[1], delay_seconds_validate, &seconds) != 0 ||
	delay_value(argv[2], delay_useconds_validate, &useconds) != 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    tv.tv_sec = seconds;
    tv.tv_usec = useconds;
    select(0, NULL, NULL, NULL, &tv);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_DELAY_COMMANDS(X, XID) \
    X(delay, ged_delay_core, GED_CMD_DEFAULT, &delay_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_DELAY_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_delay", 1, GED_DELAY_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
