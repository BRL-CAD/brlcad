/*                         P U T M A T . C
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
/** @file libged/putmat.c
 *
 * The putmat command.
 *
 */

#include "common.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_schema *putmat_schema(void);


static int
_getmat(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    struct bu_vls name1 = BU_VLS_INIT_ZERO;
    struct bu_vls name2 = BU_VLS_INIT_ZERO;
    static const char *usage = "a/b";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    {
	const char *begin;
	const char *first_fs;
	const char *last_fs;
	const char *end;

	/* skip leading slashes */
	begin = argv[1];
	while (*begin == '/')
	    ++begin;

	if (*begin == '\0' ||
	    !(first_fs = strchr(begin, '/')) ||
	    !(last_fs = strrchr(begin, '/')) ||
	    first_fs != last_fs) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad path specification '%s'", argv[0], argv[1]);
	    return BRLCAD_ERROR;
	}

	/* Note: At this point first_fs == last_fs */

	end = strrchr(begin, '\0');
	if (last_fs == end-1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad path specification '%s'", argv[0], argv[1]);
	    return BRLCAD_ERROR;
	}
	bu_vls_strncpy(&name1, begin, (size_t)(last_fs-begin));
	bu_vls_strncpy(&name2, last_fs+1, (size_t)(end-last_fs));
    }

    if ((dp = db_lookup(gedp->dbip, bu_vls_addr(&name1), LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not found in database.\n", argv[0], bu_vls_addr(&name1));
	bu_vls_free(&name1);
	bu_vls_free(&name2);
	return BRLCAD_ERROR;
    }

    if (!(dp->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not a combination\n", argv[0], bu_vls_addr(&name1));
	bu_vls_free(&name1);
	bu_vls_free(&name2);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (matp_t)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	bu_vls_free(&name1);
	bu_vls_free(&name2);
	return BRLCAD_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    if (!comb->tree) {
	bu_vls_printf(gedp->ged_result_str, "%s: empty combination", dp->d_namep);
	goto fail;
    }

    /* Search for first mention of arc */
    if ((tp = db_find_named_leaf(comb->tree, bu_vls_addr(&name2))) == TREE_NULL) {
	bu_vls_printf(gedp->ged_result_str,
		      "Unable to find instance of '%s' in combination '%s', error",
		      bu_vls_addr(&name2), bu_vls_addr(&name1));
	goto fail;
    }

    if (!tp->tr_l.tl_mat) {
	bu_vls_printf(gedp->ged_result_str, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1");
	rt_db_free_internal(&intern);

	return BRLCAD_OK;
    } else {
	register int i;

	for (i = 0; i < 16; i++)
	    bu_vls_printf(gedp->ged_result_str, "%lf ", tp->tr_l.tl_mat[i]);

	rt_db_free_internal(&intern);

	return BRLCAD_OK;
    }

fail:
    bu_vls_free(&name1);
    bu_vls_free(&name2);
    rt_db_free_internal(&intern);
    return BRLCAD_ERROR;
}


/*
 * Replace the matrix on an arc in the database from the command line,
 * when NOT in an edit state.  Used mostly to facilitate writing shell
 * scripts.  There are two valid syntaxes, each of which is
 * implemented as an appropriate call to f_arced.  Commands of the
 * form:
 *
 * putmat a/b m0 m1 ... m15
 *
 * are converted to:
 *
 * arced a/b matrix rarc m0 m1 ... m15,
 *
 * while commands of the form:
 *
 * putmat a/b I
 *
 * are converted to:
 *
 * arced a/b matrix rarc 1 0 0 0   0 1 0 0   0 0 1 0   0 0 0 1
 *
 */
int
ged_putmat_core(struct ged *gedp, int argc, const char *argv[])
{
    int result = BRLCAD_OK;	/* Return code */
    char *newargv[20+2];
    struct bu_vls *avp;
    int got;
    static const char *usage = "a/b I|m0 m1 ... m15";
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(putmat_schema(),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 2)
	return _getmat(gedp, argc, argv);

    switch (argc) {
	case 18:
	    avp = bu_vls_vlsinit();
	    bu_vls_from_argv(avp, 16, (const char **)argv + 2);
	    break;
	case 3:
	    if ((argv[2][0] == 'I') && (argv[2][1] == '\0')) {
		avp = bu_vls_vlsinit();
		bu_vls_printf(avp, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 ");
		break;
	    }
	    /* Sometimes the matrix is sent through as one long string.
	     * Copy it so we can crack it, below.
	     */
	    avp = bu_vls_vlsinit();
	    bu_vls_strcat(avp, argv[2]);
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s: error in matrix specification\n", argv[0]);
	    return BRLCAD_ERROR;
    }
    newargv[0] = "arced";
    newargv[1] = (char *)argv[1];
    newargv[2] = "matrix";
    newargv[3] = "rarc";

    got = bu_argv_from_string(&newargv[4], 16, bu_vls_addr(avp));
    if (got != 16) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s:%d: bad matrix, only got %d elements\n",
		      argv[0], __FILE__, __LINE__, got);
	result = BRLCAD_ERROR;
    }

    if (result != BRLCAD_ERROR)
	result = ged_exec_arced(gedp, 20, (const char **)newargv);

    bu_vls_vlsfree(avp);
    return result;
}


#include "../include/plugin.h"

static int
putmat_arc_validate(struct bu_vls *msg, const char *arg)
{
    const char *begin;
    const char *separator;

    if (!arg || !arg[0])
	goto invalid;
    begin = arg;
    while (*begin == '/')
	begin++;
    separator = strchr(begin, '/');
    if (!begin[0] || !separator || separator == begin || !separator[1] ||
	strchr(separator + 1, '/'))
	goto invalid;

    return 0;

invalid:
    if (msg)
	bu_vls_printf(msg, "arc must have exactly one non-empty parent/member separator");
    return -1;
}

static int
putmat_number_valid(const char *arg)
{
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(arg, &end);
    return arg && arg[0] && errno != ERANGE && end && !*end && isfinite(value) &&
	!(sizeof(fastf_t) == sizeof(float) && (value > FLT_MAX || value < -FLT_MAX));
}

static int
putmat_packed_matrix_valid(const char *arg)
{
    char *copy;
    char *values[18] = {NULL};
    size_t count;
    int valid = 1;

    if (!arg || !arg[0])
	return 0;
    copy = bu_strdup(arg);
    count = bu_argv_from_string(values, 17, copy);
    if (count != 16) {
	valid = 0;
	goto done;
    }
    for (size_t i = 0; i < count; i++) {
	if (!putmat_number_valid(values[i])) {
	    valid = 0;
	    break;
	}
    }

done:
    bu_free(copy, "putmat packed matrix copy");
    return valid;
}

static int
putmat_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, unsigned int expected,
	bu_cmd_value_t type, const char *hint, const char *provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = expected;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}

static void
putmat_identity_candidate(struct bu_cmd_validate_result *result, const char *prefix)
{
    if (!result || (prefix && prefix[0] && strncmp("I", prefix, strlen(prefix))))
	return;
    result->completion_candidates = (const char **)bu_calloc(2, sizeof(char *),
	"putmat identity completion candidate");
    result->completion_candidates[0] = bu_strdup("I");
    result->completion_count = 1;
}

static int
putmat_schema_validate(const struct bu_cmd_schema *cmd, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *cmd;
    size_t matrix_count;
    size_t token;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc == 0 || cursor_arg == 0)
	return ret;

    matrix_count = argc - 1;
    token = cursor_arg < argc ? cursor_arg : argc;
    if (matrix_count == 0) {
	putmat_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	    BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_MATRIX,
	    "optional replacement matrix", "ged.matrix");
	putmat_identity_candidate(result, "");
	return 0;
    }

    if (matrix_count == 1) {
	const char *matrix = argv[1];
	if (BU_STR_EQUAL(matrix, "I") || putmat_packed_matrix_valid(matrix))
	    return putmat_validation_result(result, BU_CMD_VALIDATE_VALID, token,
		BU_CMD_EXPECT_NONE, BU_CMD_VALUE_MATRIX, "replacement matrix", "ged.matrix");
	if (putmat_number_valid(matrix))
	    return putmat_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
		BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_NUMBER,
		"matrix requires 16 finite numeric elements", NULL);
	if (cursor_arg < argc && !strncmp("I", matrix, strlen(matrix))) {
	    putmat_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
		BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_MATRIX,
		"identity marker I or 16-number matrix expected", "ged.matrix");
	    putmat_identity_candidate(result, matrix);
	    return 0;
	}
	return putmat_validation_result(result, BU_CMD_VALIDATE_INVALID, token,
	    BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_MATRIX,
	    "identity marker I or 16-number matrix expected", "ged.matrix");
    }

    for (size_t i = 1; i < argc; i++) {
	if (!putmat_number_valid(argv[i]))
	    return putmat_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_NUMBER,
		"matrix elements must be finite numbers", NULL);
    }
    if (matrix_count < 16)
	return putmat_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, token,
	    BU_CMD_EXPECT_OPERAND, BU_CMD_VALUE_NUMBER,
	    "matrix requires 16 finite numeric elements", NULL);
    return putmat_validation_result(result, BU_CMD_VALIDATE_VALID, token,
	BU_CMD_EXPECT_NONE, BU_CMD_VALUE_MATRIX, "replacement matrix", "ged.matrix");
}

static const struct bu_cmd_operand putmat_schema_operands[] = {
    BU_CMD_OPERAND_VALIDATE("arc", BU_CMD_VALUE_DB_PATH, 1, 1,
	putmat_arc_validate, "Combination parent/child arc", "ged.db_path"),
    BU_CMD_OPERAND("matrix", BU_CMD_VALUE_RAW, 0, 16,
	"Optional I, packed 16-number matrix, or 16 finite number tokens", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema putmat_cmd_schema = {
    "putmat", "Get or replace a combination arc matrix", NULL,
    putmat_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    {putmat_schema_validate}
};

static const struct bu_cmd_schema *
putmat_schema(void)
{
    return &putmat_cmd_schema;
}

#define GED_PUTMAT_COMMANDS(X, XID) \
    X(putmat, ged_putmat_core, GED_CMD_DEFAULT, &putmat_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PUTMAT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_putmat", 1, GED_PUTMAT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
