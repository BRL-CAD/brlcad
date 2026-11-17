/*                         B W M O D . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/bwmod.c
 *
 * Modify intensities in Black and White files.
 *
 * Allows any number of add, subtract, multiply, divide, or
 * exponentiation operations to be performed on a picture.  Keeps
 * track of and reports clipping.
 *
 * Note that this works on PIX files also but there is no distinction
 * made between colors.
 *
 * WARNING, added 8 June 2015: SHIFT is done the way it is because I
 * did not see << and >> working properly when attempted with negative
 * arguments.
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"


char hyphen[] = "-";
char noname[] = "(noname)";
char *progname = noname;

char *file_name;

enum bwmod_operation {
    ADD = 1,
    MULT,
    ABS,
    POW,
    SHIFT,
    AND,
    OR,
    XOR,
    ROUND
};

#define BUFLEN	(8192*2)	/* usually 2 pages of memory, 16KB */
#define MAX_OPERATIONS 256

int numop = 0;			/* number of operations */
int op[MAX_OPERATIONS];		/* operations */
double val[MAX_OPERATIONS];	/* arguments to operations */
unsigned char ibuf[BUFLEN];	/* input buffer */

#define MAPBUFLEN 256
int mapbuf[MAPBUFLEN];		/* translation buffer/lookup table */

struct bwmod_args {
    struct bwmod_args *self;
    int help;
    int char_arith;
};

static struct bwmod_args *
bwmod_args_from_storage(void *storage)
{
    return storage ? *(struct bwmod_args **)storage : NULL;
}

static int
bwmod_append(struct bu_vls *msg, struct bwmod_args *args,
	enum bwmod_operation operation, double value)
{
    if (!args)
	return 0;
    if (numop >= MAX_OPERATIONS) {
	if (msg)
	    bu_vls_printf(msg, "too many operations (maximum %d)", MAX_OPERATIONS);
	return -1;
    }
    op[numop] = operation;
    val[numop++] = value;
    return 0;
}

static int
bwmod_number_operation(struct bu_vls *msg, const char *arg, void *storage,
	enum bwmod_operation operation, double scale)
{
    double value;

    if (!bu_arg_double_from_str(msg, &value, arg))
	return -1;
    return bwmod_append(msg, bwmod_args_from_storage(storage), operation,
	value * scale);
}

#define BWMOD_NUMBER_CALLBACK(_name, _operation, _scale) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return bwmod_number_operation(msg, arg, storage, _operation, _scale); \
    }

BWMOD_NUMBER_CALLBACK(bwmod_add, ADD, 1.0)
BWMOD_NUMBER_CALLBACK(bwmod_subtract, ADD, -1.0)
BWMOD_NUMBER_CALLBACK(bwmod_multiply, MULT, 1.0)
BWMOD_NUMBER_CALLBACK(bwmod_exponent, POW, 1.0)
BWMOD_NUMBER_CALLBACK(bwmod_shift, SHIFT, 1.0)
BWMOD_NUMBER_CALLBACK(bwmod_and, AND, 1.0)
BWMOD_NUMBER_CALLBACK(bwmod_or, OR, 1.0)
BWMOD_NUMBER_CALLBACK(bwmod_xor, XOR, 1.0)

static int
bwmod_reciprocal_operation(struct bu_vls *msg, const char *arg, void *storage,
	enum bwmod_operation operation, const char *zero_message)
{
    double value;

    if (!bu_arg_double_from_str(msg, &value, arg))
	return -1;
    if (ZERO(value)) {
	if (msg)
	    bu_vls_printf(msg, "%s", zero_message);
	return -1;
    }
    return bwmod_append(msg, bwmod_args_from_storage(storage), operation,
	1.0 / value);
}

static int
bwmod_divide(struct bu_vls *msg, const char *arg, void *storage)
{
    return bwmod_reciprocal_operation(msg, arg, storage, MULT,
	"cannot divide by zero");
}

static int
bwmod_root(struct bu_vls *msg, const char *arg, void *storage)
{
    return bwmod_reciprocal_operation(msg, arg, storage, POW,
	"root cannot be zero");
}

static int
bwmod_absolute(struct bu_vls *msg, const char *UNUSED(arg), void *storage)
{
    return bwmod_append(msg, bwmod_args_from_storage(storage), ABS, 0.0);
}

static int
bwmod_signed(struct bu_vls *UNUSED(msg), const char *UNUSED(arg), void *storage)
{
    struct bwmod_args *args = bwmod_args_from_storage(storage);
    if (args)
	args->char_arith = !args->char_arith;
    return 0;
}

static int
bwmod_round(struct bu_vls *msg, const char *UNUSED(arg), void *storage)
{
    struct bwmod_args *args = bwmod_args_from_storage(storage);

    if (!args)
	return 0;
    if (numop > MAX_OPERATIONS - 2) {
	if (msg)
	    bu_vls_printf(msg, "too many operations (maximum %d)", MAX_OPERATIONS);
	return -1;
    }
    op[numop] = ROUND;
    val[numop++] = 0.0;
    op[numop] = SHIFT;
    val[numop++] = 0.0;
    return 0;
}

static int
bwmod_truncate(struct bu_vls *msg, const char *UNUSED(arg), void *storage)
{
    return bwmod_append(msg, bwmod_args_from_storage(storage), SHIFT, 0.0);
}

static const struct bu_arg_option bwmod_options[] = {
    BU_ARG_FLAG("h", "help", struct bwmod_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("a", "add", struct bwmod_args, self, bwmod_add,
	"value", "Add a value"),
    BU_ARG_CUSTOM("s", "subtract", struct bwmod_args, self, bwmod_subtract,
	"value", "Subtract a value"),
    BU_ARG_CUSTOM("m", "multiply", struct bwmod_args, self, bwmod_multiply,
	"value", "Multiply by a value"),
    BU_ARG_CUSTOM("d", "divide", struct bwmod_args, self, bwmod_divide,
	"value", "Divide by a nonzero value"),
    BU_ARG_CUSTOM_FLAG("A", "absolute", "absolute", struct bwmod_args, self,
	bwmod_absolute, "Take the absolute value"),
    BU_ARG_CUSTOM("e", "exponent", struct bwmod_args, self, bwmod_exponent,
	"value", "Raise to a power"),
    BU_ARG_CUSTOM("r", "root", struct bwmod_args, self, bwmod_root,
	"value", "Take a nonzero root"),
    BU_ARG_CUSTOM_FLAG("c", "signed", "signed", struct bwmod_args, self,
	bwmod_signed, "Toggle signed-character arithmetic"),
    BU_ARG_CUSTOM("S", "shift", struct bwmod_args, self, bwmod_shift,
	"bits", "Shift left (positive) or right (negative)"),
    BU_ARG_CUSTOM("M", "and", struct bwmod_args, self, bwmod_and,
	"value", "Apply bitwise AND"),
    BU_ARG_CUSTOM("O", "or", struct bwmod_args, self, bwmod_or,
	"value", "Apply bitwise OR"),
    BU_ARG_CUSTOM("X", "xor", struct bwmod_args, self, bwmod_xor,
	"value", "Apply bitwise XOR"),
    BU_ARG_CUSTOM_FLAG("R", "round", "round", struct bwmod_args, self,
	bwmod_round, "Round to the nearest integer, then truncate"),
    BU_ARG_CUSTOM_FLAG("t", "truncate", "truncate", struct bwmod_args, self,
	bwmod_truncate, "Truncate to an integer"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwmod_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwmod_schema = BU_ARG_SCHEMA(
    "bwmod", "Arithmetically modify BW pixel values",
    bwmod_options, bwmod_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bwmod_grammar = {
    &bwmod_schema, NULL, NULL
};

void
checkpow(double x , double exponent)
{
    if (x > 0.0) {
	return;
    }

    if (x < 0.0) {
	double diff;
	diff = exponent - (double)((int)exponent);
	if (diff < 0.0 || diff > 0.0) {
	    fprintf(stderr,
		    "bwmod: negative number (%f) to non-integer power (%f)\n",
		    x, exponent);
	    bu_exit(-1, NULL);
	}
    }
    /* We have x == 0.0, and we accept that 0 to 0 power is 1. */
    if (exponent >= 0.0) {
	return;
    }

    fprintf(stderr, "bwmod: zero to negative power (%f)\n", exponent);
    bu_exit(-1, NULL);
}


int
get_args(int argc, char **argv, struct bwmod_args *args)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index = bu_arg_schema_parse_complete(&bwmod_schema, args,
	&msg, argc - 1, (const char **)(argv + 1));

    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 0;
    }
    bu_vls_free(&msg);
    if (args->help)
	return 1;

    if (argc - operand_index - 1 == 0) {
	if (isatty((int)fileno(stdin))) {
	    return 0;
	}
	file_name = hyphen;
    } else {
	char *ifname;
	file_name = argv[operand_index + 1];
	ifname = bu_file_realpath(file_name, NULL);
	if (freopen(ifname, "rb", stdin) == NULL) {
	    fprintf(stderr,
		    "bwmod: cannot open \"%s(canonical %s)\" for reading\n",
		    file_name, ifname);
	    bu_free(ifname, "ifname alloc from bu_file_realpath");
	    return 0;
	}
	bu_free(ifname, "ifname alloc from bu_file_realpath");
    }

    return 1; /* OK */
}


void
mk_trans_tbl(void)
{
    int j, i, tmp, intval;
    double d;

    /* create translation map */
    for (j = 0; j < MAPBUFLEN; ++j) {
	d = j;
	for (i = 0; i < numop; i++) {
	    switch (op[i]) {
	    case ADD :
		d += val[i];
		break;
	    case MULT:
		d *= val[i];
		break;
	    case POW :
		checkpow(d, val[i]);
		d = pow(d, val[i]);
		break;
	    case ABS :
		if (d < 0.0) {
		    d = - d;
		}
		break;
	    case SHIFT:
		tmp = d;
		intval = (int)val[i];
		if (intval > 0) {
		    tmp = tmp << intval;
		} else if (intval < 0) {
		    tmp = tmp >> (-intval);
		}
		d = tmp;
		break;
	    case OR  :
		tmp = d;
		tmp |= (int)val[i];
		d = tmp;
		break;
	    case AND :
		tmp = d;
		tmp &= (int)val[i];
		d = tmp;
		break;
	    case XOR :
		tmp = d;
		tmp ^= (int)val[i];
		d = tmp;
		break;
	    /* case TRUNC:
	     * tmp=((int)d/(int)val[i])*(int)val[i];
	     * d=tmp;
	     * break;
	     */
	    case ROUND:
		if (d > 0) {
		    d = (int)(d + 0.5);
		} else if (d < 0) {
		    d = (int)(d - 0.5);
		}
		break;
	    default  :
		fprintf(stderr, "%s: error in op\n", progname);
		bu_exit(-1, NULL);
		break;
	    }
	}
	if (d > 255.0) {
	    mapbuf[j] = 256;
	} else if (d < 0.0) {
	    mapbuf[j] = -1;
	} else {
	    mapbuf[j] = d + 0.5;
	}
    }
}


void
mk_char_trans_tbl(void)
{
    int j, i, intval;
    signed char d;

    /* create translation map */
    for (j = 0; j < MAPBUFLEN; ++j) {
	d = j;
	for (i = 0; i < numop; i++) {
	    switch (op[i]) {
	    case ADD :
		d += val[i];
		break;
	    case MULT:
		d *= val[i];
		break;
	    case POW :
		checkpow((double)d, val[i]);
		d = pow((double)d, val[i]);
		break;
	    case ABS :
		if (d < 0.0) {
		    d = - d;
		}
		break;
	    case SHIFT:
		intval = (int)val[i];
		if (intval > 0) {
		    d = d << intval;
		} else if (intval < 0) {
		    d = d >> (-intval);
		}
		break;
	    case AND :
		d &= (int)val[i];
		break;
	    case OR  :
		d |= (int)val[i];
		break;
	    case XOR :
		d ^= (int)val[i];
		break;
	    default  :
		fprintf(stderr, "%s: error in op\n", progname);
		bu_exit(-1, NULL);
	    /* TRUNC and ROUND do nothing because we already have
	     * integer value
	     */
	    /* case TRUNC: */
	    case ROUND:
		break;
	    }
	}
	mapbuf[j] = d & 0x0ff;
    }
}


int
main(int argc, char **argv)
{
    struct bwmod_args args = {NULL, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    unsigned char *p, *q;
    int tmp;
    int n;
    unsigned long clip_high = 0L , clip_low = 0L ;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    progname = *argv;

    introspection = bu_arg_introspect(&msg, &bwmod_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_free(&msg);

    if (!get_args(argc, argv, &args)) {
	char *help = bu_arg_schema_help(&bwmod_schema, "bwmod");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "bwmod help");
	return 1;
    }
    if (args.help) {
	char *help = bu_arg_schema_help(&bwmod_schema, "bwmod");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwmod help");
	return 0;
    }
    if (isatty((int)fileno(stdin)) || isatty((int)fileno(stdout))) {
	bu_log("bwmod requires binary input and redirected standard output\n");
	return 1;
    }

    if (args.char_arith) {
	mk_char_trans_tbl();
    } else {
	mk_trans_tbl();
    }

    while ((n = read(fileno(stdin), (void *)ibuf, (unsigned)sizeof(ibuf))) > 0) {
	/* translate */
	for (p = ibuf, q = &ibuf[n]; p < q; ++p) {
	    long i = *p;
	    if (i < 0) {
		i = 0;
	    }
	    if (i >= MAPBUFLEN) {
		*p = i = MAPBUFLEN;
	    }

	    tmp = mapbuf[i];
	    if (tmp > 255) {
		++clip_high;
		*p = 255;
	    } else if (tmp < 0) {
		++clip_low;
		*p = 0;
	    } else {
		*p = tmp;
	    }
	}
	/* output */
	if (write(fileno(stdout), (void *)ibuf, (unsigned)n) != n) {
	    fprintf(stderr, "%s: Error writing stdout\n",
		    progname);
	    bu_exit(-1, NULL);
	}
    }
    if (n < 0) {
	perror("READ ERROR");
    }

    if (clip_high != 0 || clip_low != 0) {
	fprintf(stderr, "bwmod: clipped %lu high, %lu low\n",
		(long unsigned)clip_high, (long unsigned)clip_low);
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
