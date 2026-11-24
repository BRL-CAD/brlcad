/*                      P I X M A T T E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2026 United States Government as represented by
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
/** @file util/pixmatte.c
 *
 * Given four streams of data elements,
 * where element is of arbitrary width,
 * typically pix(5) or bw(5) images,
 * output a stream of the same number of data elements.
 * The value of the output stream is determined element-by-element,
 * by comparing the
 * first (foreground) input stream with the
 * the second (background, or matte) input stream.
 * If the formula holds true, the element from the
 * true-output stream is written,
 * otherwise, the element from the false-output stream is written.
 * Each of these streams comes from a file, or is given as a constant.
 * A particular file may be used to feed more than one stream,
 * and the name '-' specifies stdin.
 * For example, the foreground file may also be the true-output file.
 *
 * This routine operates on an element-by-element basis, and thus
 * is independent of the resolution of the image.
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#define NFILES 4 /* Two in, two out */
#define EL_WIDTH 32 /* Max width of one element */
#define CHUNK (32*1024)	/* # elements per I/O */

static int width = 3;

static const char *file_name[NFILES];
static FILE *fp[NFILES];	/* NULL => use f_const */
static unsigned char f_const[NFILES][EL_WIDTH];
static char *buf[NFILES];	/* I/O buffers, size width*CHUNK */
static char *obuf;		/* output buffer */

#define LT 1
#define EQ 2
#define GT 4
#define NE 8
#define APPROX 16
static int wanted = 0;		/* LT|EQ|GT conditions */

static long true_cnt = 0;
static long false_cnt = 0;

static int twoconstants;
static int limit = 2;

static int
parse_constant_bytes(const char *arg, unsigned char *out_values, int component_count)
{
    int i;
    const char *component = arg;
    char *end = NULL;
    long int value;

    for (i = 0; i < component_count; i++) {
	errno = 0;
	value = strtol(component, &end, 10);
	if (errno != 0 || end == component || value < 0 || value > 255)
	    return 0;
	out_values[i] = (unsigned char)value;

	if (i == component_count - 1) {
	    if (*end != '\0')
		return 0;
	} else {
	    if (*end != '/')
		return 0;
	    component = end + 1;
	}
    }

    return 1;
}


static int
open_file(int i, const char *name)
{
    if (name[0] == '=') {
	/* Parse constant */
	if (!parse_constant_bytes(name + 1, &f_const[i][0], width)) {
	    bu_log("pixmatte: invalid constant '%s'\n", name);
	    return -1;
	}

	file_name[i] = name+1;	/* skip '=' */
	fp[i] = NULL;
	buf[i] = NULL;
	return 0;		/* OK */
    }

    file_name[i] = name;
    if (BU_STR_EQUAL(name, "-")) {
	fp[i] = stdin;
	if (isatty(fileno(stdin)))
	    return -1;	/* FAIL */
	/* XXX No checking for multiple uses of stdin */
    } else if ((fp[i] = fopen(name, "rb")) == NULL) {
	perror(name);
	bu_log("pixmatte: cannot open \"%s\" for reading\n", name);
	return -1;		/* FAIL */
    }

    /* Obtain buffer */
    if ((buf[i] = (char *)malloc(width*CHUNK)) == (char *)0) {
	bu_exit (3, "pixmatte: input buffer malloc failure\n");
    }

    return 0;			/* OK */
}


struct pixmatte_args {
    int help;
    int greater_than;
    int less_than;
    int equal;
    int not_equal;
    int approximate;
    int element_width;
};

static const struct bu_arg_option pixmatte_options[] = {
    BU_ARG_FLAG("h", "help", struct pixmatte_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("g", "greater-than", struct pixmatte_args, greater_than,
	"Select true output when input 1 is greater than input 2"),
    BU_ARG_FLAG("l", "less-than", struct pixmatte_args, less_than,
	"Select true output when input 1 is less than input 2"),
    BU_ARG_FLAG("e", "equal", struct pixmatte_args, equal,
	"Select true output when input 1 equals input 2"),
    BU_ARG_FLAG("n", "not-equal", struct pixmatte_args, not_equal,
	"Select true output when input 1 does not equal input 2"),
    BU_ARG_FLAG("a", "approximate", struct pixmatte_args, approximate,
	"Allow a per-byte difference of one in comparisons"),
    BU_ARG_INTEGER_RANGE("w", "element-width", struct pixmatte_args,
	element_width, 1, 1, 1, EL_WIDTH - 1, "bytes",
	"Set the number of bytes in each stream element"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixmatte_operands[] = {
    BU_ARG_OPERAND("input-1", BU_ARG_VALUE_RAW, 1, 1,
	"First input file, constant, or standard input", NULL),
    BU_ARG_OPERAND("input-2", BU_ARG_VALUE_RAW, 1, 1,
	"Second input file, constant, or standard input", NULL),
    BU_ARG_OPERAND("true-output", BU_ARG_VALUE_RAW, 1, 1,
	"Stream selected when the comparison is true", NULL),
    BU_ARG_OPERAND("false-output", BU_ARG_VALUE_RAW, 1, 1,
	"Stream selected when the comparison is false", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixmatte_schema = BU_ARG_SCHEMA(
    "pixmatte", "Merge streams using per-element comparisons",
    pixmatte_options, pixmatte_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixmatte_grammar = {
    &pixmatte_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    int chunkcount = 0;
    int i;
    struct pixmatte_args args = {0, 0, 0, 0, 0, 0, 3};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixmatte_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixmatte_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixmatte_schema, "pixmatte");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixmatte help");
	return 0;
    }

    if (!args.greater_than && !args.less_than && !args.equal && !args.not_equal)
	bu_exit(1, "pixmatte: specify at least one comparison option\n");

    width = args.element_width;
    if (args.greater_than)
	wanted |= GT;
    if (args.less_than)
	wanted |= LT;
    if (args.equal)
	wanted |= EQ;
    if (args.not_equal)
	wanted |= NE;
    if (args.approximate)
	wanted |= APPROX;

    for (i = 0; i < NFILES; i++) {
	if (open_file(i, argv[operand_index + i + 1]) < 0)
	    bu_exit(1, "pixmatte: unable to open input stream\n");
    }

    /* Avoid an unbounded stream when neither comparison input has EOF. */
    twoconstants = (fp[0] == NULL && fp[1] == NULL);

    if (isatty(fileno(stdout)))
	bu_exit(1, "pixmatte: cannot write image data to a terminal\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    bu_log("pixmatte:\tif (%s ", file_name[0]);

    if (wanted & APPROX) bu_log("~~");
    if (wanted & LT) bu_log("<");
    if (wanted & EQ) bu_log("=");
    if (wanted & GT) bu_log(">");
    if (wanted & NE) bu_log("!=");

    bu_log(" %s)\n", file_name[1]);
    bu_log("pixmatte:\t\tthen output %s\n", file_name[2]);
    bu_log("pixmatte:\t\telse output %s\n", file_name[3]);

    if ((obuf = (char *)malloc(width*CHUNK)) == (char *)0) {
	bu_exit (3, "pixmatte: obuf malloc failure\n");
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

    if (wanted & APPROX) {
	if ( (wanted & GT) && (wanted & EQ) )
	    limit = -1;
    	    /* remember that limit defaulted to 2,
	     * (for the case of APPROX/GT)
	     */
    	else if (wanted & LT) {
    	    if (wanted & EQ)
    		limit = 1;
    	     else
    		limit = -2;
	     }
    /* Remember that some cases are "don't care" w/r to "limit".
     */
    }


    while (1) {
	unsigned char *cb0, *cb1;	/* current input buf ptrs */
	unsigned char *cb2, *cb3;
	unsigned char *obp; 	/* current output buf ptr */
	unsigned char *ebuf;		/* end ptr in buf[0] */
	size_t len;
	int i;

    	chunkcount++;
	if ( chunkcount == 9 && twoconstants )
	    bu_exit (1, NULL);

	len = CHUNK;
	for (i=0; i<NFILES; i++) {
	    size_t got;

	    if (fp[i] == NULL) continue;
	    got = fread(buf[i], width, CHUNK, fp[i]);
	    if (got < len) len = got;
	}
	if (len <= 0)
	    break;

	cb0 = (unsigned char *)buf[0];
	cb1 = (unsigned char *)buf[1];
	cb2 = (unsigned char *)buf[2];
	cb3 = (unsigned char *)buf[3];
	obp = (unsigned char *)obuf;
	ebuf = cb0 + width*len;
	for (; cb0 < ebuf;
	     cb0 += width, cb1 += width, cb2 += width, cb3 += width) {
	    /*
	     * Stated condition must hold for all input bytes
	     * to select the foreground for output
	     */
	    unsigned char *ap, *bp;
	    unsigned char *ep;		/* end ptr */

	    if (buf[0] != NULL)
		ap = cb0;
	    else
		ap = &f_const[0][0];

	    if (buf[1] != NULL)
		bp = cb1;
	    else
		bp = &f_const[1][0];

	    if (wanted & NE) {
/* If both NE and EQ are detected, all elements yield true result,
 * and don't care about APPROX usage.
 */
	    	if (wanted & EQ) goto success;
		if (!(wanted & APPROX)) {
		    for (ep = ap+width; ap < ep;) {
			if (*ap++ == *bp++) goto fail;
		    }
		    goto success;
		}
	    }
	    if (wanted & APPROX) {
		if (wanted & NE) {
		    /* Want not even approx equal */
		    for (ep = ap+width; ap < ep;) {
			if ((i= *ap++ - *bp++) >= -1 &&
			    i <= 1)
			    goto fail;
		    }
		    goto success;
		}
	    	/* Cannot get here with LT and GT both in use, because
		 * if both LT and GT were detected, they would have been
	    	 * turned off and replaced with NE.
	    	 */
		if (wanted & GT) {
		    /* APPROX/GT/EQ (limit = -1):
                     *   Want approx GT/EQ; in addition to ">=",
                     *   we allow 1 on the OTHER side of equality.
		     * APPROX/GT (limit = 2):
		     *   Want approx GT; same as ">", except we
                     *   stay more than 1 away from equality.
		     */
		    for (ep = ap+width; ap < ep;) {
			if ((*ap++ - *bp++) < limit)
			    goto fail;
		    }
		    goto success;
		}
		if (wanted & LT) {
		    /* APPROX/LT/EQ (limit = 1):
		     *   Want approx LT/EQ; in addition to "<=",
                     *   we allow 1 on the OTHER side of equality.
		     * APPROX/LT (limit = -2):
		     *   Want approx LT; same as "<", except we
	             *   stay more than 1 away from equality.
		     */
		    for (ep = ap+width; ap < ep;) {
			if ((*ap++ - *bp++) > limit)
			    goto fail;
		    }
		    goto success;
		}
		if (wanted & EQ) {
		    /* Want approx equal */
		    for (ep = ap+width; ap < ep;) {
			if ((i= *ap++ - *bp++) < -1 ||
			    i > 1)
			    goto fail;
		    }
		    goto success;
		}
	    }

/* (wanted & APPROX) is false if we arrive here.
 */
	    for (ep = ap+width; ap < ep; ap++, bp++) {
		if (*ap > *bp) {
		    if (!(GT & wanted))
			goto fail;
		} else if (*ap == *bp) {
		    if (!(EQ & wanted))
			goto fail;
		} else {
		    if (!(LT & wanted))
			goto fail;
		}
	    }

	success:
	    if (buf[2] != NULL)
		ap = cb2;
	    else
		ap = &f_const[2][0];

	    for (i=0; i<width; i++)
		*obp++ = *ap++;

	    true_cnt++;
	    continue;
	fail:
	    if (buf[3] != NULL)
		bp = cb3;
	    else
		bp = &f_const[3][0];

	    for (i=0; i<width; i++)
		*obp++ = *bp++;

	    false_cnt++;
	}
	if (fwrite(obuf, width, len, stdout) != len) {
	    perror("fwrite");
	    bu_exit (1, "pixmatte: write error\n");
	}
    }

    bu_log("pixmatte: %ld element comparisons true, %ld false (width=%d)\n",
	   true_cnt, false_cnt, width);

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
