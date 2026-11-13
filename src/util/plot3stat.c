/*                     P L O T 3 S T A T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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
/** @file util/plot3stat.c
 *
 * Print statistics about a Plot3(5) format file.
 *
 */

#include "common.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/cv.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"


#define TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define TIEEE	3	/* IEEE 64-bit floating */
#define TCHAR	4	/* unsigned chars */
#define TSTRING	5	/* linefeed terminated string */
#define COORD	0x10	/* these values are coordinates */

struct uplot {
    int targ;	/* type of args */
    int narg;	/* number or args */
    const char *desc;	/* description */
};
static const struct uplot uerror = { 0, 0, 0 };
static const struct uplot letters[] = {
    /*A*/	{ 0, 0, 0 },
    /*B*/	{ 0, 0, 0 },
    /*C*/	{ TCHAR, 3, "color" },
    /*D*/	{ 0, 0, 0 },
    /*E*/	{ 0, 0, 0 },
    /*F*/	{ TNONE, 0, "flush" },
    /*G*/	{ 0, 0, 0 },
    /*H*/	{ 0, 0, 0 },
    /*I*/	{ 0, 0, 0 },
    /*J*/	{ 0, 0, 0 },
    /*K*/	{ 0, 0, 0 },
    /*L*/	{ TSHORT, 6, "3line" },
    /*M*/	{ TSHORT, 3, "3move" },
    /*N*/	{ TSHORT, 3, "3cont" },
    /*O*/	{ TIEEE, 3, "d_3move" },
    /*P*/	{ TSHORT, 3, "3point" },
    /*Q*/	{ TIEEE, 3, "d_3cont" },
    /*R*/	{ 0, 0, 0 },
    /*S*/	{ TSHORT, 6, "3space" },
    /*T*/	{ 0, 0, 0 },
    /*U*/	{ 0, 0, 0 },
    /*V*/	{ TIEEE, 6, "d_3line" },
    /*W*/	{ TIEEE, 6, "d_3space" },
    /*X*/	{ TIEEE, 3, "d_3point" },
    /*Y*/	{ 0, 0, 0 },
    /*Z*/	{ 0, 0, 0 },
    /*[*/	{ 0, 0, 0 },
    /*\*/	{ 0, 0, 0 },
    /*]*/	{ 0, 0, 0 },
    /*^*/	{ 0, 0, 0 },
    /*_*/	{ 0, 0, 0 },
    /*`*/	{ 0, 0, 0 },
    /*a*/	{ TSHORT, 6, "arc" },
    /*b*/	{ 0, 0, 0 },
    /*c*/	{ TSHORT, 3, "circle" },
    /*d*/	{ 0, 0, 0 },
    /*e*/	{ TNONE, 0, "erase" },
    /*f*/	{ TSTRING, 1, "linmod" },
    /*g*/	{ 0, 0, 0 },
    /*h*/	{ 0, 0, 0 },
    /*i*/	{ TIEEE, 3, "d_circle" },
    /*j*/	{ 0, 0, 0 },
    /*k*/	{ 0, 0, 0 },
    /*l*/	{ TSHORT, 4, "line" },
    /*m*/	{ TSHORT, 2, "move" },
    /*n*/	{ TSHORT, 2, "cont" },
    /*o*/	{ TIEEE, 2, "d_move" },
    /*p*/	{ TSHORT, 2, "point" },
    /*q*/	{ TIEEE, 2, "d_cont" },
    /*r*/	{ TIEEE, 6, "d_arc" },
    /*s*/	{ TSHORT, 4, "space" },
    /*t*/	{ TSTRING, 1, "label" },
    /*u*/	{ 0, 0, 0 },
    /*v*/	{ TIEEE, 4, "d_line" },
    /*w*/	{ TIEEE, 4, "d_space" },
    /*x*/	{ TIEEE, 2, "d_point" },
    /*y*/	{ 0, 0, 0 },
    /*z*/	{ 0, 0, 0 }
};


static FILE *fp;

static void outchar(int n);
static void outstring(int n);
static void outshort(int n);
static void outfloat(int n);

struct plot3stat_args {
    int help;
    int verbose;
};

static const struct bu_arg_option plot3stat_options[] = {
    BU_ARG_FLAG("h", "help", struct plot3stat_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct plot3stat_args, verbose,
	"Print each command name while reading"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand plot3stat_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PLOT3 input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema plot3stat_schema = BU_ARG_SCHEMA(
    "plot3stat", "Report command counts for a PLOT3 stream",
    plot3stat_options, plot3stat_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar plot3stat_grammar = {
    &plot3stat_schema, NULL, NULL
};

static int
getshort(void)
{
    long v, w;

    v = getc(fp);
    v |= (getc(fp)<<8);	/* order is important! */

    /* worry about sign extension - sigh */
    if (v <= 0x7FFF) return v;
    w = -1;
    w &= ~0x7FFF;
    return w | v;
}


int
main(int argc, char **argv)
{
    int c;
    const struct uplot *up;
    int i;
    long counts['z'-'A'+1] = {0};
    struct plot3stat_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &plot3stat_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&plot3stat_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&plot3stat_schema, "plot3stat");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "plot3stat help");
	return 0;
    }

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc - operand_index - 1 == 1) {
	const char *path = argv[operand_index + 1];
	fp = fopen(path, "rb");
	if (!fp)
	    bu_exit(1, "plot3stat: cannot open '%s' for reading\n", path);
    } else {
	fp = stdin;
	if (isatty(fileno(stdin)))
	    bu_exit(1, "plot3stat requires an input file or redirected input\n");
    }

    while ((c = getc(fp)) != EOF) {

	/* look it up */
	if (c < 'A' || c > 'z') {
	    fprintf(stderr, "Bad command '%c' (0x%02x)\n", c, c);
	    continue;
	} else {
	    up = &letters[ c - 'A' ];
	}

	if (up->targ == TBAD) {
	    fprintf(stderr, "Bad command '%c' (0x%02x)\n", c, c);
	    continue;
	}

	counts[ c - 'A' ]++;

	if (up->narg > 0) {
	    switch (up->targ) {
		case TNONE:
		    break;
		case TSHORT:
		    outshort(up->narg);
		    break;
		case TIEEE:
		    outfloat(up->narg);
		    break;
		case TSTRING:
		    outstring(up->narg);
		    break;
		case TCHAR:
		    outchar(up->narg);
		    break;
	    }
	}

	if (args.verbose)
	    printf(" %s", up->desc);
    }

    /* write command usage summary */
    for (i = 0; i < 'z'-'A'+1; i++) {
	if (counts[i] != 0) {
	    fprintf(stderr, "%s %ld\n", letters[i].desc, counts[i]);
	}
    }
    return 0;
}


static void
outchar(int n)
{
    int i;

    for (i = 0; i < n; i++) {
	(void)getc(fp);
	/*printf("%3d", c);*/
    }
}


static void
outstring(int UNUSED(n))
{
    int c;

    while ((c = getc(fp)) != '\n' && c != EOF)
	;
    /*putchar(c);*/
}


static void
outshort(int n)
{
    int i;

    for (i = 0; i < n; i++) {
	(void)getshort();
    }
}


static void
outfloat(int n)
{
    int i;
    unsigned char in[8*16];
    double out[16];
    size_t ret;

    ret = fread(in, 8, n, fp);
    if (ret < (size_t)n)
	perror("fread");

    bu_cv_ntohd((unsigned char *)out, in, n);

    for (i = 0; i < n; i++) {
	/*printf("%g", out[i]);*/
	;
    }
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
