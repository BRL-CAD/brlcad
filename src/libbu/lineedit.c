/*                    L I N E E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */

#include "common.h"

#include <stdlib.h>

#include "bu/lineedit.h"
#include "bu/str.h"


int
bu_cmd_completion_mode_parse(const char *name, bu_cmd_completion_mode_t *mode)
{
    bu_cmd_completion_mode_t parsed;

    if (!name || !name[0] || !mode)
	return BRLCAD_ERROR;

    if (BU_STR_EQUIV(name, "filter"))
	parsed = BU_CMD_COMPLETE_FILTER;
    else if (BU_STR_EQUIV(name, "cycle"))
	parsed = BU_CMD_COMPLETE_CYCLE;
    else if (BU_STR_EQUIV(name, "prefix"))
	parsed = BU_CMD_COMPLETE_PREFIX;
    else if (BU_STR_EQUIV(name, "legacy"))
	parsed = BU_CMD_COMPLETE_LEGACY;
    else if (BU_STR_EQUIV(name, "off"))
	parsed = BU_CMD_COMPLETE_OFF;
    else
	return BRLCAD_ERROR;

    *mode = parsed;
    return BRLCAD_OK;
}


const char *
bu_cmd_completion_mode_name(bu_cmd_completion_mode_t mode)
{
    switch (mode) {
	case BU_CMD_COMPLETE_FILTER: return "filter";
	case BU_CMD_COMPLETE_CYCLE: return "cycle";
	case BU_CMD_COMPLETE_PREFIX: return "prefix";
	case BU_CMD_COMPLETE_LEGACY: return "legacy";
	case BU_CMD_COMPLETE_OFF: return "off";
    }

    return "filter";
}


bu_cmd_completion_mode_t
bu_cmd_completion_mode_from_env(const char *frontend_env, bu_cmd_completion_mode_t fallback)
{
    bu_cmd_completion_mode_t mode;
    const char *value = NULL;

    if (frontend_env && frontend_env[0]) {
	value = getenv(frontend_env);
	if (bu_cmd_completion_mode_parse(value, &mode) == BRLCAD_OK)
	    return mode;
    }

    value = getenv(BU_CMD_COMPLETION_MODE_ENV);
    if (bu_cmd_completion_mode_parse(value, &mode) == BRLCAD_OK)
	return mode;

    return fallback;
}
