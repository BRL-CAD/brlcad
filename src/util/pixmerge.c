/*                      P I X M E R G E . C
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
/** @file util/pixmerge.c
 *
 * Given two streams of data, typically pix(5) or bw(5) images,
 * generate an output stream of the same size, where the value of the
 * output is determined by a formula involving the first (foreground)
 * stream and a constant, or the value of the second (background)
 * stream.
 *
 * This routine operates on a pixel-by-pixel basis, and thus is
 * independent of the resolution of the image.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"


#define EL_WIDTH 32 /* Max width of one element */

static int width = 3;

static const char *f1_name;
static const char *f2_name;
static FILE *f1;
static FILE *f2;

#define LT 1
#define EQ 2
#define GT 4
#define NE 8
static int wanted = 0;			/* LT|EQ|GT conditions to pick fb */

static long fg_cnt = 0;
static long bg_cnt = 0;

static int seen_const = 0;

static unsigned char pconst[32] = {0};

#define CHUNK 1024
static char *b1;			/* fg input buffer */
static char *b2;			/* bg input buffer */
static char *b3;			/* output buffer */

static int
parse_const_arg(struct bu_vls *msg, const char *arg, int component_count,
	unsigned char *out_values)
{
    unsigned char parsed[EL_WIDTH] = {0};
    int parsed_count = 0;
    const char *cp = arg;

    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_strcat(msg, "constant requires at least one component\n");
	return 0;
    }

    while (*cp) {
	char *end = NULL;
	long int value;

	if (parsed_count >= EL_WIDTH - 1) {
	    if (msg)
		bu_vls_printf(msg, "constant accepts at most %d components\n",
		    EL_WIDTH - 1);
	    return 0;
	}
	errno = 0;
	value = strtol(cp, &end, 10);
	if (errno != 0 || end == cp || value < 0 || value > UCHAR_MAX) {
	    if (msg)
		bu_vls_printf(msg, "invalid constant component: %s\n", cp);
	    return 0;
	}
	parsed[parsed_count++] = (unsigned char)value;
	if (*end == '\0')
	    break;
	if (*end != '/' || !end[1]) {
	    if (msg)
		bu_vls_printf(msg, "invalid constant: %s\n", arg);
	    return 0;
	}
	cp = end + 1;
    }

    if (component_count > 0 && parsed_count != component_count) {
	if (msg)
	    bu_vls_printf(msg, "constant requires %d component(s)\n",
		component_count);
	return 0;
    }
    if (out_values)
	memcpy(out_values, parsed, (size_t)parsed_count);
    return 1;
}

struct pixmerge_args {
    struct pixmerge_args *self;
    int help;
    int greater;
    int less;
    int equal;
    int not_equal;
    int width;
    int seen_const;
    unsigned char constant[EL_WIDTH];
};

static struct pixmerge_args *
pixmerge_args_from_storage(void *storage)
{
    return storage ? *(struct pixmerge_args **)storage : NULL;
}

static int
pixmerge_constant(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixmerge_args *args = pixmerge_args_from_storage(storage);
    unsigned char constant[EL_WIDTH] = {0};
    int component_count = args ? args->width : 0;

    if (!parse_const_arg(msg, arg, component_count, constant))
	return -1;
    if (args) {
	memcpy(args->constant, constant, (size_t)component_count);
	args->seen_const = 1;
    }
    return 0;
}

static const struct bu_arg_option pixmerge_options[] = {
    BU_ARG_FLAG("h", "help", struct pixmerge_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("g", "greater", struct pixmerge_args, greater,
	"Select foreground bytes greater than background bytes"),
    BU_ARG_FLAG("l", "less", struct pixmerge_args, less,
	"Select foreground bytes less than background bytes"),
    BU_ARG_FLAG("e", "equal", struct pixmerge_args, equal,
	"Select foreground bytes equal to background bytes"),
    BU_ARG_FLAG("n", "not-equal", struct pixmerge_args, not_equal,
	"Select foreground bytes unequal to background bytes"),
    BU_ARG_INTEGER_RANGE("w", "width", struct pixmerge_args, width,
	1, 1, 1, EL_WIDTH - 1, "bytes",
	"Set element width in bytes (default: 3; use 1 for BW data)"),
    BU_ARG_CUSTOM("C", "constant", struct pixmerge_args, self,
	pixmerge_constant, "values",
	"Compare against exactly width slash-separated byte values (0-255) instead of a background stream"),
    BU_ARG_ALIAS("c", NULL, "constant", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixmerge_operands[] = {
    BU_ARG_OPERAND("foreground", BU_ARG_VALUE_FILE, 1, 1,
	"Foreground image or - for standard input", NULL),
    BU_ARG_OPERAND("background", BU_ARG_VALUE_FILE, 0, 1,
	"Background image or - for standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static int
pixmerge_validate(const struct bu_arg_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_arg_validate_result *result)
{
    size_t operand_count;
    size_t required_count;
    int has_formula;
    int has_constant;

    if (!result || cursor_arg < argc || result->state == BU_ARG_VALIDATE_INVALID)
	return 0;

    operand_count = bu_arg_schema_operand_count(schema, argc, argv);
    has_formula =
	bu_arg_schema_option_present(schema, argc, argv, "greater") ||
	bu_arg_schema_option_present(schema, argc, argv, "less") ||
	bu_arg_schema_option_present(schema, argc, argv, "equal") ||
	bu_arg_schema_option_present(schema, argc, argv, "not-equal");
    has_constant = bu_arg_schema_option_present(schema, argc, argv, "constant");
    required_count = has_formula && !has_constant ? 2 : 1;

    if (operand_count == required_count)
	return 0;

    if (operand_count < required_count) {
	bu_arg_validate_result_set(result, BU_ARG_VALIDATE_INCOMPLETE, argc,
	    BU_ARG_EXPECT_OPERAND, BU_ARG_VALUE_FILE,
	    "supply a background image when comparing two streams", NULL);
	return 0;
    }

    bu_arg_validate_result_set(result, BU_ARG_VALIDATE_INVALID, argc - 1,
	BU_ARG_EXPECT_NONE, BU_ARG_VALUE_FILE,
	"a background image is not accepted with this option form", NULL);
    return 0;
}

static const struct bu_arg_schema pixmerge_schema = BU_ARG_SCHEMA(
    "pixmerge", "Merge image streams using per-element comparisons",
    pixmerge_options, pixmerge_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, pixmerge_validate, NULL, NULL));

static const struct bu_arg_grammar pixmerge_grammar = {
    &pixmerge_schema, NULL, NULL
};

static int
pixmerge_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&pixmerge_schema, "pixmerge");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "pixmerge help");
    return status;
}

static int
pixmerge_open_input(const char *name, FILE **stream)
{
    if (BU_STR_EQUAL(name, "-")) {
	*stream = stdin;
	return 1;
    }
    *stream = fopen(name, "rb");
    if (!*stream) {
	perror(name);
	bu_flog(stderr, "pixmerge: cannot open \"%s\" for reading\n", name);
	return 0;
    }
    return 1;
}


int
main(int argc, char **argv)
{
    struct pixmerge_args args = {NULL, 0, 0, 0, 0, 0, 3, 0, {0}};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;
    int operand_index;
    size_t ret;

    args.self = &args;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &pixmerge_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&pixmerge_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return pixmerge_help(stdout, 0);
    if (isatty(fileno(stdout)))
	return pixmerge_help(stderr, 1);

    width = args.width;
    seen_const = args.seen_const;
    memcpy(pconst, args.constant, sizeof(pconst));
    if (args.greater)
	wanted |= GT;
    if (args.less)
	wanted |= LT;
    if (args.equal)
	wanted |= EQ;
    if (args.not_equal)
	wanted |= NE;
    if (!wanted) {
	wanted = GT;
	seen_const = 1;
    }

    f1_name = argv[operand_index + 1];
    if (!pixmerge_open_input(f1_name, &f1))
	return 1;
    if (!seen_const) {
	f2_name = argv[operand_index + 2];
	if (!pixmerge_open_input(f2_name, &f2))
	    return 1;
    }

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    fprintf(stderr, "pixmerge: Selecting foreground when fg ");
    if (wanted & LT) fputc('<', stderr);
    if (wanted & EQ) fputc('=', stderr);
    if (wanted & GT) fputc('>', stderr);
    if (wanted & NE) fprintf(stderr, "!=");
    if (seen_const) {
	int i;

	fputc(' ', stderr);
	for (i = 0; i < width; i++) {
	    fprintf(stderr, "%d", pconst[i]);
	    if (i < width-1)
		fputc('/', stderr);
	}
	fputc('\n', stderr);
    } else {
	fprintf(stderr, " bg\n");
    }

    if ((b1 = (char *)bu_calloc(width,CHUNK,"b1")) == (char *)0 ||
	(b2 = (char *)bu_calloc(width,CHUNK,"b2")) == (char *)0 ||
	(b3 = (char *)bu_calloc(width,CHUNK,"b3")) == (char *)0) {
	fprintf(stderr, "pixmerge:  bu_calloc failure\n");
	bu_exit (3, NULL);
    }

/* If "<>" is detected (in "wanted"), we change to "!=" because it's the same.
 */
    if ( (wanted & LT) && (wanted & GT)) {
    	wanted |= NE;
    	wanted ^= LT;
    	wanted ^= GT;
    }
/* If NE is detected, then there is no point also having either LT or GT.
 */
    else if ( (wanted & NE) ){
    	if (wanted & LT) wanted ^=LT;
    	else
    	if (wanted & GT) wanted ^=GT;
    }

    while (1) {
	unsigned char *cb1, *cb2;	/* current input buf ptrs */
	unsigned char *cb3; 	/* current output buf ptr */
	unsigned char *ebuf;		/* end ptr in b1 */
	int r2, len;

	len = fread(b1, width, CHUNK, f1);
	if (!seen_const) {
	    r2 = fread(b2, width, CHUNK, f2);
	    if (r2 < len)
		len = r2;
	}
	if (len <= 0)
	    break;

	cb1 = (unsigned char *)b1;
	cb2 = (unsigned char *)b2;
	cb3 = (unsigned char *)b3;
	ebuf = cb1 + width*len;
	for (; cb1 < ebuf; cb1 += width, cb2 += width) {
	    /*
	     * Stated condition must hold for all input bytes
	     * to select the foreground for output
	     */
	    unsigned char *ap = NULL;
	    unsigned char *bp = NULL;
	    unsigned char *ep = NULL;		/* end ptr */

	    ap = cb1;
	    if (seen_const)
		bp = pconst;
	    else
		bp = cb2;

	    if (wanted & NE) {
/* If both NE and EQ are detected, all elements yield true result.
 */
	    	if (wanted & EQ) goto success;
		for (ep = cb1+width; ap < ep; ap++, bp++) {
		    if (*ap == *bp)
			goto fail;
		}
		goto success;
	    }

	    for (ep = cb1+width; ap < ep; ap++, bp++) {
		if (*ap > *bp) {
		    if (!(GT & wanted)) goto fail;
		} else if (*ap == *bp) {
		    if (!(EQ & wanted)) goto fail;
		} else {
		    if (!(LT & wanted)) goto fail;
		}
	    }
	success:
	    {
		int i;
		ap = cb1;
		for (i=0; i<width; i++)
		    *cb3++ = *ap++;
	    }
	    fg_cnt++;
	    continue;
	fail:
	    {
		int i;
		bp = cb2;
		for (i=0; i<width; i++)
		    *cb3++ = *bp++;
	    }
	    bg_cnt++;
	}
	ret = fwrite(b3, width, len, stdout);
	if (ret < (size_t)len)
	    perror("fwrite");
    }
    fprintf(stderr, "pixmerge: %ld foreground, %ld background\n",
	    fg_cnt, bg_cnt);

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
