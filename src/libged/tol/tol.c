/*                         T O L . C
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
/** @file libged/tol.c
 *
 * The tol command.
 *
 */

#include "common.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_schema *tol_schema(void);


static int
tol_parse_number(const char *arg, double *value)
{
    char *end = NULL;
    double parsed;

    if (!arg)
	return -1;
    errno = 0;
    parsed = strtod(arg, &end);
    if (errno == ERANGE || !end || *end || !isfinite(parsed) ||
	(sizeof(fastf_t) == sizeof(float) &&
	 (parsed > FLT_MAX || parsed < -FLT_MAX)))
	return -1;
    if (value)
	*value = parsed;
    return 0;
}


int
ged_tol_core(struct ged *gedp, int argc, const char *argv[])
{
    double f;
    static const char *usage = "([abs|rel|norm|dist|perp] [tolerance]) ...";
    int parse_dummy = 0;
    struct rt_wdb *wdbp;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 1 && bu_cmd_schema_parse_complete(tol_schema(), &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
	}

    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    /* print all tolerance settings */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Current tolerance settings are:\n");
	bu_vls_printf(gedp->ged_result_str, "Tessellation tolerances:\n");

	if (wdbp->wdb_ttol.abs > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tabs %g mm\n", wdbp->wdb_ttol.abs);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tabs None\n");
	}

	if (wdbp->wdb_ttol.rel > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\trel %g (%g%%)\n",
			  wdbp->wdb_ttol.rel, wdbp->wdb_ttol.rel * 100.0);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\trel None\n");
	}

	if (wdbp->wdb_ttol.norm > 0.0) {
	    int deg, min;
	    double sec;

	    sec = wdbp->wdb_ttol.norm * RAD2DEG;
	    deg = (int)(sec);
	    sec = (sec - (double)deg) * 60;
	    min = (int)(sec);
	    sec = (sec - (double)min) * 60;

	    bu_vls_printf(gedp->ged_result_str, "\tnorm %g degrees (%d deg %d min %g sec)\n",
			  wdbp->wdb_ttol.norm * RAD2DEG, deg, min, sec);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tnorm None\n");
	}

	bu_vls_printf(gedp->ged_result_str, "Calculational tolerances:\n");
	bu_vls_printf(gedp->ged_result_str,
		      "\tdistance = %g mm\n\tperpendicularity = %g (cosine of %g degrees)\n",
		      wdbp->wdb_tol.dist, wdbp->wdb_tol.perp,
		      acos(wdbp->wdb_tol.perp)*RAD2DEG);

	bu_vls_printf(gedp->ged_result_str, "BRep specific tessellation tolerances:\n");
	if (wdbp->wdb_ttol.absmax > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmax %g\n", wdbp->wdb_ttol.absmax);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmax None\n");
	}

	if (wdbp->wdb_ttol.absmin > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmin %g\n", wdbp->wdb_ttol.absmin);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tabsmin None\n");
	}

	if (wdbp->wdb_ttol.relmax > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\trelmax %g\n", wdbp->wdb_ttol.relmax);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\trelmax None\n");
	}

	if (wdbp->wdb_ttol.relmin > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\trelmin %g\n", wdbp->wdb_ttol.relmin);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\trelmin None\n");
	}

	if (wdbp->wdb_ttol.rel_lmax > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tlmax %g\n", wdbp->wdb_ttol.rel_lmax);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tlmax None\n");
	}

	if (wdbp->wdb_ttol.rel_lmin > 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "\tlmin %g\n", wdbp->wdb_ttol.rel_lmin);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "\tlmin None\n");
	}


	return BRLCAD_OK;
    }

    /* get the specified tolerance */
    if (argc == 2) {

	if (BU_STR_EQUAL(argv[1], "abs")) {
	    if (wdbp->wdb_ttol.abs > 0.0)
		bu_vls_printf(gedp->ged_result_str, "%g", wdbp->wdb_ttol.abs);
	    else {
		bu_vls_printf(gedp->ged_result_str, "None");
	    }
	    return BRLCAD_OK;
	}

	if (BU_STR_EQUAL(argv[1], "rel")) {
	    if (wdbp->wdb_ttol.rel > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "%g", wdbp->wdb_ttol.rel);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "None");
	    }
	    return BRLCAD_OK;
	}

	if (BU_STR_EQUAL(argv[1], "norm")) {
	    if (wdbp->wdb_ttol.norm > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "%g", wdbp->wdb_ttol.norm);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "None");
	    }
	    return BRLCAD_OK;
	}

	if (BU_STR_EQUAL(argv[1], "absmax")) {
	    if (wdbp->wdb_ttol.absmax > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tabsmax %g\n", wdbp->wdb_ttol.absmax);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tabsmax None\n");
	    }
	    return BRLCAD_OK;
	}

	if (BU_STR_EQUAL(argv[1], "absmin")) {
	    if (wdbp->wdb_ttol.absmin > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tabsmin %g\n", wdbp->wdb_ttol.absmin);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tabsmin None\n");
	    }
	    return BRLCAD_OK;
	}


	if (BU_STR_EQUAL(argv[1], "relmax")) {
	    if (wdbp->wdb_ttol.relmax > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\trelmax %g\n", wdbp->wdb_ttol.relmax);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\trelmax None\n");
	    }
	    return BRLCAD_OK;
	}


	if (BU_STR_EQUAL(argv[1], "relmin")) {
	    if (wdbp->wdb_ttol.relmin > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\trelmin %g\n", wdbp->wdb_ttol.relmin);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\trelmin None\n");
	    }
	    return BRLCAD_OK;
	}


	if (BU_STR_EQUAL(argv[1], "lmax")) {
	    if (wdbp->wdb_ttol.rel_lmax > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tlmax %g\n", wdbp->wdb_ttol.rel_lmax);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tlmax None\n");
	    }
	    return BRLCAD_OK;
	}


	if (BU_STR_EQUAL(argv[1], "lmin")) {
	    if (wdbp->wdb_ttol.rel_lmin > 0.0) {
		bu_vls_printf(gedp->ged_result_str, "\tlmin %g\n", wdbp->wdb_ttol.rel_lmin);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "\tlmin None\n");
	    }
	    return BRLCAD_OK;
	}

	if (BU_STR_EQUAL(argv[1], "dist")) {
	    bu_vls_printf(gedp->ged_result_str, "%g", wdbp->wdb_tol.dist);
	    return BRLCAD_OK;
	}

	if (BU_STR_EQUAL(argv[1], "perp")) {
	    bu_vls_printf(gedp->ged_result_str, "%g", wdbp->wdb_tol.perp);
	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "unrecognized tolerance type - %s", argv[1]);
	return BRLCAD_ERROR;

    }

    if ((argc-1)%2 != 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* skip the command name */
    argc--;
    argv++;

    /* iterate over the pairs of tolerance values */
    while (argc > 0) {
	int valid_tol = 0;

	/* set the specified tolerance(s) */
	if (tol_parse_number(argv[1], &f) != 0) {
	    bu_vls_printf(gedp->ged_result_str, "bad tolerance - %s", argv[1]);
	    return BRLCAD_ERROR;
	}

	/* clamp negative to zero */
	if (f < 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "negative tolerance clamped to 0.0\n");
	    f = 0.0;
	}

	if (BU_STR_EQUAL(argv[0], "abs")) {
	    /* Absolute tol */
	    if (f < wdbp->wdb_tol.dist) {
		bu_vls_printf(gedp->ged_result_str,
			"absolute tolerance cannot be less than distance tolerance, clamped to %f\n", wdbp->wdb_tol.dist);
		f = wdbp->wdb_tol.dist;
	    }
	    wdbp->wdb_ttol.abs = f;
	    valid_tol = 1;
	}
	if (BU_STR_EQUAL(argv[0], "rel")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return BRLCAD_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    wdbp->wdb_ttol.rel = f;
	    valid_tol = 1;
	}
	if (BU_STR_EQUAL(argv[0], "norm")) {
	    /* Normal tolerance, in degrees */
	    if (f > 90.0) {
		bu_vls_printf(gedp->ged_result_str,
			"Normal tolerance must be less than 90.0 degrees\n");
		return BRLCAD_ERROR;
	    }
	    /* Note that a value of 0.0 or 360.0 will disable this tol */
	    wdbp->wdb_ttol.norm = f * DEG2RAD;
	    valid_tol = 1;
	}


	if (BU_STR_EQUAL(argv[0], "absmax")) {
	    /* Absolute tol */
	    if (f < wdbp->wdb_tol.dist) {
		bu_vls_printf(gedp->ged_result_str,
			"absolute tolerance cannot be less than distance tolerance, clamped to %f\n", wdbp->wdb_tol.dist);
		f = wdbp->wdb_tol.dist;
	    }
	    wdbp->wdb_ttol.absmax = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "absmin")) {
	    /* Absolute tol */
	    if (f < wdbp->wdb_tol.dist) {
		bu_vls_printf(gedp->ged_result_str,
			"absolute tolerance cannot be less than distance tolerance, clamped to %f\n", wdbp->wdb_tol.dist);
		f = wdbp->wdb_tol.dist;
	    }
	    wdbp->wdb_ttol.absmin = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "relmax")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return BRLCAD_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    wdbp->wdb_ttol.relmax = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "relmin")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return BRLCAD_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    wdbp->wdb_ttol.relmin = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "lmax")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return BRLCAD_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    wdbp->wdb_ttol.rel_lmax = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "lmin")) {
	    if (f >= 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"relative tolerance must be between 0 and 1, not changed\n");
		return BRLCAD_ERROR;
	    }
	    /* Note that a value of 0.0 will disable relative tolerance */
	    wdbp->wdb_ttol.rel_lmin = f;
	    valid_tol = 1;
	}

	if (BU_STR_EQUAL(argv[0], "dist")) {
	    /* Calculational distance tolerance */
	    wdbp->wdb_tol.dist = f;
	    wdbp->wdb_tol.dist_sq = wdbp->wdb_tol.dist * wdbp->wdb_tol.dist;
	    valid_tol = 1;
	}
	if (BU_STR_EQUAL(argv[0], "perp")) {
	    /* Calculational perpendicularity tolerance */
	    if (f > 1.0) {
		bu_vls_printf(gedp->ged_result_str,
			"Calculational perpendicular tolerance must be from 0 to 1\n");
		return BRLCAD_ERROR;
	    }
	    wdbp->wdb_tol.perp = f;
	    wdbp->wdb_tol.para = 1.0 - f;
	    valid_tol = 1;
	}

	if (!valid_tol) {
	    bu_vls_printf(gedp->ged_result_str, "unrecognized tolerance type - %s", argv[0]);
	    return BRLCAD_ERROR;
	}

	argc-=2;
	argv+=2;
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

static const struct bu_cmd_value_keyword tol_schema_keywords[] = {
    {"abs", NULL, "Absolute tessellation tolerance in mm"},
    {"rel", NULL, "Relative tessellation tolerance"},
    {"norm", NULL, "Normal tessellation tolerance in degrees"},
    {"absmax", NULL, "Maximum BRep absolute tessellation tolerance"},
    {"absmin", NULL, "Minimum BRep absolute tessellation tolerance"},
    {"relmax", NULL, "Maximum BRep relative tessellation tolerance"},
    {"relmin", NULL, "Minimum BRep relative tessellation tolerance"},
    {"lmax", NULL, "Maximum BRep relative edge-length tolerance"},
    {"lmin", NULL, "Minimum BRep relative edge-length tolerance"},
    {"dist", NULL, "Boolean calculation distance tolerance in mm"},
    {"perp", NULL, "Boolean calculation perpendicularity tolerance"},
    {NULL, NULL, NULL}
};


static const struct bu_cmd_value_keyword *
tol_schema_keyword(const char *s)
{
    if (!s)
	return NULL;
    for (size_t i = 0; tol_schema_keywords[i].canonical; i++)
	if (BU_STR_EQUAL(s, tol_schema_keywords[i].canonical))
	    return &tol_schema_keywords[i];
    return NULL;
}


static void
tol_schema_keyword_candidates(struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;

    for (size_t i = 0; tol_schema_keywords[i].canonical; i++)
	if (!prefix || !prefix[0] ||
	    !bu_strncmp(tol_schema_keywords[i].canonical, prefix, strlen(prefix)))
	    count++;
    if (!count)
	return;

    result->completion_candidates = (const char **)bu_calloc(count + 1, sizeof(char *),
	"tol keyword completion candidates");
    for (size_t i = 0, ci = 0; tol_schema_keywords[i].canonical; i++)
	if (!prefix || !prefix[0] ||
	    !bu_strncmp(tol_schema_keywords[i].canonical, prefix, strlen(prefix)))
	    result->completion_candidates[ci++] = bu_strdup(tol_schema_keywords[i].canonical);
    result->completion_count = count;
}


static int
tol_schema_result(struct bu_cmd_validate_result *result, bu_cmd_validate_state_t state,
	size_t token, bu_cmd_value_t type, const char *hint)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    return 0;
}


static int
tol_schema_value_valid(const char *keyword, const char *arg)
{
    double value;

    if (tol_parse_number(arg, &value) != 0)
	return 0;
    if ((BU_STR_EQUAL(keyword, "rel") || BU_STR_EQUAL(keyword, "relmax") ||
	 BU_STR_EQUAL(keyword, "relmin") || BU_STR_EQUAL(keyword, "lmax") ||
	 BU_STR_EQUAL(keyword, "lmin")) && value >= 1.0)
	return 0;
    if (BU_STR_EQUAL(keyword, "norm") && value > 90.0)
	return 0;
    if (BU_STR_EQUAL(keyword, "perp") && value > 1.0)
	return 0;
    return 1;
}


static int
tol_schema_validate(const struct bu_cmd_schema *UNUSED(schema), size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    size_t token = cursor_arg < argc ? cursor_arg : argc;

    for (size_t i = 0; i < argc; i++) {
	if ((i % 2) == 0) {
	    if (!tol_schema_keyword(argv[i])) {
		if (i == cursor_arg) {
		    tol_schema_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
			BU_CMD_VALUE_KEYWORD, "tolerance name expected");
		    tol_schema_keyword_candidates(result, argv[i]);
		    return 0;
		}
		return tol_schema_result(result, BU_CMD_VALIDATE_INVALID, i,
		    BU_CMD_VALUE_KEYWORD, "unrecognized tolerance type");
	    }
	} else if (!tol_schema_value_valid(argv[i - 1], argv[i])) {
	    return tol_schema_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_NUMBER, "invalid tolerance value");
	}
    }

    if (cursor_arg < argc) {
	if ((cursor_arg % 2) == 0) {
	    tol_schema_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_VALUE_KEYWORD, "tolerance name");
	    tol_schema_keyword_candidates(result, argv[cursor_arg]);
	    return 0;
	}
	return tol_schema_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_VALUE_NUMBER, "tolerance value");
    }

    if (argc == 0 || (argc % 2) == 0) {
	tol_schema_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_VALUE_KEYWORD, "tolerance name");
	tol_schema_keyword_candidates(result, "");
	return 0;
    }
    if (argc == 1)
	return tol_schema_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_VALUE_NUMBER, "optional tolerance value");
    return tol_schema_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
	BU_CMD_VALUE_NUMBER, "tolerance value expected");
}


static const struct bu_cmd_operand tol_schema_operands[] = {
    BU_CMD_OPERAND("tolerances", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Tolerance names and optional values", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema tol_cmd_schema = {
    "tol", "Query or set geometric tolerances", NULL, tol_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {tol_schema_validate}
};

static const struct bu_cmd_schema *
tol_schema(void)
{
    return &tol_cmd_schema;
}

#define GED_TOL_COMMANDS(X, XID) \
    X(tol, ged_tol_core, GED_CMD_DEFAULT, &tol_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_TOL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_tol", 1, GED_TOL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
