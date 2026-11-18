/*                          L O O P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file util/loop.c
 *
 * Simple program to output integers or floats or chars between
 * "start" and "finish", inclusive.  Default is an increment of +1 if
 * start < finish or -1 if start > finish.  User may specify an
 * alternate increment.  Also, user may left-pad output integers with
 * zeros.  There is no attempt to prevent "infinite" loops.
 *
 * Authors -
 * John Grosh
 * Phil Dykstra
 * Michael John Muuss
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "vmath.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "util_arg.h"
#include "bu/str.h"

enum loop_mode {
    LOOP_INTEGER = 0,
    LOOP_REAL,
    LOOP_CHARACTER
};

struct loop_args {
    int help;
    int character;
};

static int
loop_character_validate(struct bu_vls *msg, const char *arg)
{
    char value;

    return bu_arg_char_from_str(msg, &value, arg) ? 0 : -1;
}

static int
loop_nonzero_number_validate(struct bu_vls *msg, const char *arg)
{
    fastf_t value;

    if (!bu_arg_number_from_str(msg, &value, arg))
	return -1;
    if (value <= 0.0 && value >= 0.0) {
	if (msg)
	    bu_vls_printf(msg, "increment must not be zero\n");
	return -1;
    }
    return 0;
}

static int
loop_character_increment_validate(struct bu_vls *msg, const char *arg)
{
    long value;

    if (!bu_arg_long_range_from_str(msg, &value, arg, 10,
	    -UCHAR_MAX, UCHAR_MAX))
	return -1;
    if (value == 0) {
	if (msg)
	    bu_vls_printf(msg, "increment must not be zero\n");
	return -1;
    }
    return 0;
}

static const struct bu_arg_option loop_options[] = {
    BU_ARG_FLAG("h", "help", struct loop_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("c", "character", struct loop_args, character,
	"Generate a sequence of single-byte characters"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand loop_numeric_operands[] = {
    BU_ARG_OPERAND("start", BU_ARG_VALUE_NUMBER, 1, 1,
	"First numeric value", NULL),
    BU_ARG_OPERAND("finish", BU_ARG_VALUE_NUMBER, 1, 1,
	"Last numeric value", NULL),
    BU_ARG_OPERAND_VALIDATE("increment", BU_ARG_VALUE_NUMBER, 0, 1,
	loop_nonzero_number_validate, "Optional nonzero step", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand loop_character_operands[] = {
    BU_ARG_OPERAND_VALIDATE("start", BU_ARG_VALUE_STRING, 1, 1,
	loop_character_validate, "First character", NULL),
    BU_ARG_OPERAND_VALIDATE("finish", BU_ARG_VALUE_STRING, 1, 1,
	loop_character_validate, "Last character", NULL),
    BU_ARG_OPERAND_VALIDATE("increment", BU_ARG_VALUE_INTEGER, 0, 1,
	loop_character_increment_validate,
	"Optional nonzero character-code step from -255 through 255", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const loop_character_options[] = {
    "character", NULL
};

static const struct bu_arg_schema_case loop_cases[] = {
    BU_ARG_SCHEMA_CASE("characters", "Generate a character sequence",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, loop_character_options,
	loop_character_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("numbers", "Generate a numeric sequence",
	loop_numeric_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};

static const struct bu_arg_schema loop_schema = BU_ARG_SCHEMA(
    "loop", "Generate an inclusive numeric or character sequence",
    loop_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, loop_cases));

static const struct bu_arg_grammar loop_grammar = {
    &loop_schema, NULL, NULL
};

static int
loop_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&loop_schema, "loop");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "loop help");
    return status;
}

int
main(int argc, char *argv[])
{
    struct loop_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char * const *values;
    int introspection;
    int operand_index;
    enum loop_mode mode;
    int value_count;

    long int i;
    long int start,  finish;
    long int incr;

    double d;
    double dstart, dfinish, dincr;

    unsigned long int c;
    unsigned long int cstart, cfinish;
    long int cincr;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &loop_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    operand_index = bu_arg_schema_parse_complete(&loop_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return loop_help(stdout, 0);

    values = (const char * const *)(argv + operand_index + 1);
    value_count = argc - operand_index - 1;
    mode = args.character ? LOOP_CHARACTER : LOOP_INTEGER;

    /* determine if any arguments are real */
    for (i = 0; i < value_count; i++) {
	long int ival = 0;
	double dval = 0.0;

	if (mode == LOOP_CHARACTER)
	    break;
	if (util_arg_long(values[i], &ival, NULL))
	    continue;
	if (!util_arg_double(values[i], &dval, NULL))
	    return 1;
	if (!ZERO(dval - (double)ival)) {
	    mode = LOOP_REAL;
	    break;
	}
	mode = LOOP_REAL;
	break;
    }

    if (mode == LOOP_REAL) {
	if (!util_arg_double(values[0], &dstart, "start")) {
	    return 1;
	}

	if (!util_arg_double(values[1], &dfinish, "finish")) {
	    return 1;
	}

	if (value_count == 3) {
	    if (!util_arg_double(values[2], &dincr, "increment")) {
		return 1;
	    }
	} else {
	    if (dstart > dfinish)
		dincr = -1.0;
	    else
		dincr =  1.0;
	}

	if (dincr > 0.0)
	    for (d = dstart; d <= dfinish; d += dincr)
		printf("%g\n", d);
	else if (dincr < 0.0)
	    for (d = dstart; d >= dfinish; d += dincr)
		printf("%g\n", d);
	else {
	    bu_log("loop 'increment' can not be zero.\n");
	    return 1;
	}

    } else if (mode == LOOP_INTEGER) {
	/* print out integer output */
	const char *cp;
	char fmt_string[50];

	int field_width;

	int zeros      = 0;  /* leading zeros for output */
	int zeros_start = 0;
	int zeros_finish = 0;
	int zeros_increment = 0;

	/* Count leading zeros in the start value. */
	for (cp = values[0]; *cp == '0'; cp++)
	    zeros_start++;
	if (*cp == '\0')
	    zeros_start--;

	/* Count leading zeros in the finish value. */
	for (cp = values[1]; *cp == '0'; cp++)
	    zeros_finish++;
	if (*cp == '\0')
	    zeros_finish--;

	/* Count leading zeros in an explicit increment. */
	if (value_count == 3) {
	    for (cp = values[2]; *cp == '0'; cp++)
		zeros_increment++;
	    if (*cp == '\0')
		zeros_increment--;
	}

	/* determine field width and leading zeros*/
	if (zeros_start >= zeros_finish && zeros_start >= zeros_increment) {
	    field_width = strlen(values[0]);
	    zeros = zeros_start;
	} else if (zeros_finish >= zeros_start && zeros_finish >= zeros_increment) {
	    field_width = strlen(values[1]);
	    zeros = zeros_finish;
	} else {
	    field_width = strlen(values[2]);
	    zeros = zeros_increment;
	}

	/* printf format string fmt_string */
	if (zeros > 0)
	    snprintf(fmt_string, sizeof(fmt_string), "%%0%dd\n", field_width);
	else
	    bu_strlcpy(fmt_string, "%d\n", sizeof(fmt_string));
	fmt_string[sizeof(fmt_string) - 1] = '\0';

	if (!util_arg_long(values[0], &start, "start"))
	    return 1;
	if ((start < INT_MIN) || (start > INT_MAX)) {
	    bu_log("'start' out of range of signed integer.\n");
	    return 1;
	}

	if (!util_arg_long(values[1], &finish, "finish"))
	    return 1;
	if ((finish < INT_MIN) || (finish > INT_MAX)) {
	    bu_log("'finish' out of range of signed integer.\n");
	    return 1;
	}

	if (value_count == 3) {
	    if (!util_arg_long(values[2], &incr, "increment"))
		return 1;
	    if ((incr < INT_MIN) || (incr > INT_MAX)) {
		bu_log("'increment' out of range of signed integer.\n");
		return 1;
	    }
	} else {
	    if (start > finish)
		incr = -1;
	    else
		incr =  1;
	}

	if (incr > 0)
	    for (i = start; i <= finish; i += incr)
		printf(fmt_string, i);
	else if (incr < 0)
	    for (i = start; i >= finish; i += incr)
		printf(fmt_string, i);
	else {
	    bu_log("loop 'increment' can not be zero.\n");
	    return 9;
	}
    } else {
	/* print out character output */
	cstart = values[0][0];
	cfinish = values[1][0];

	/* reserve the max value so we can bounds check */
	if (cstart > ULONG_MAX-1)
	    cstart = ULONG_MAX-1;
	if (cfinish > ULONG_MAX-1)
	    cfinish = ULONG_MAX-1;

	if (value_count == 3) {
	    if (!util_arg_long(values[2], &cincr, "increment"))
		return 1;
	    if ((cincr < -UCHAR_MAX) || (cincr > UCHAR_MAX)) {
		bu_log("'increment' out of range of char.\n");
		return 1;
	    }
	} else {
	    if (cstart > cfinish)
		cincr = -1;
	    else
		cincr = 1;
	}
	if (cincr > 0)
	    for (c = cstart;  c <= cfinish; c += cincr)
		printf("%c\n", (char)c);
	else if (cincr < 0)
	    for (c = cstart; c >= cfinish; c += cincr)
		printf("%c\n", (char)c);
	else {
	    bu_log("loop 'increment' can not be zero.\n");
	    return 1;
	}
    }

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
