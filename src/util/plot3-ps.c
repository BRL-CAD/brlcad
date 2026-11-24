/*                      P L O T 3 - P S . C
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
/** @file util/plot3-ps.c
 *
 * Display plot3(5) as PostScript.
 * Based on plot3-X.c and bw-ps.c
 *
 */

#include "common.h"

#include <math.h>
#include <time.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/cv.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"


#define TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define TIEEE	3	/* IEEE 64-bit floating */
#define TCHAR	4	/* unsigned chars */
#define TSTRING	5	/* linefeed terminated string */

struct uplot {
    int targ;	/* type of args */
    int narg;	/* number or args */
    const char *desc;	/* description */
};
struct uplot uerror = { 0, 0, 0 };
struct uplot letters[] = {
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


int verbose;
double cx, cy, cz;		/* current x, y, z, point */
double arg[6];			/* parsed plot command arguments */
double sp[6];			/* space command */
char strarg[512];		/* string buffer */

#define DEFAULT_SIZE 6.75	/* default output size in inches */

struct plot3_ps_dimensions {
    double width;
    double height;
};

struct plot3_ps_args {
    int help;
    int encapsulated;
    int center;
    struct plot3_ps_dimensions dimensions;
};

static int
plot3_ps_size_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    double size;

    if (bu_arg_positive_number_validate(msg, arg) < 0 ||
	!bu_arg_double_from_str(msg, &size, arg))
	return -1;
    if (storage) {
	struct plot3_ps_dimensions *dimensions =
	    (struct plot3_ps_dimensions *)storage;
	dimensions->width = size;
	dimensions->height = size;
    }
    return 0;
}

static const struct bu_arg_option plot3_ps_options[] = {
    BU_ARG_FLAG("h", "help", struct plot3_ps_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("e", "encapsulated", struct plot3_ps_args, encapsulated,
	"Produce Encapsulated PostScript output"),
    BU_ARG_FLAG("c", "center", struct plot3_ps_args, center,
	"Center output on an 8.5 by 11 inch page"),
    BU_ARG_CUSTOM("s", "size", struct plot3_ps_args, dimensions,
	plot3_ps_size_from_str, "inches", "Set both output dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 0),
    BU_ARG_VALUE_VALIDATE("w", "width", struct plot3_ps_args,
	dimensions.width, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"inches", "Set the output width"),
    BU_ARG_ALIAS("W", NULL, "width", 0),
    BU_ARG_VALUE_VALIDATE("n", "height", struct plot3_ps_args,
	dimensions.height, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"inches", "Set the output height"),
    BU_ARG_ALIAS("N", NULL, "height", 0),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand plot3_ps_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PLOT3 input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema plot3_ps_schema = BU_ARG_SCHEMA(
    "plot3-ps", "Convert a PLOT3 stream to PostScript",
    plot3_ps_options, plot3_ps_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar plot3_ps_grammar = {
    &plot3_ps_schema, NULL, NULL
};

int encapsulated = 0;	/* encapsulated PostScript */
int center = 0;		/* center output on 8.5 x 11 page */
int width = 4096;		/* Our integer plotting space */
int height = 4096;
double outwidth = DEFAULT_SIZE;		/* output plot size in inches */
double outheight = DEFAULT_SIZE;
int xpoints;		/* output plot size in points */
int ypoints;
int page_dirty = 0;		/* to skip extra erases */

static const char Stdin[] = "[stdin]";
static const char *file_name;
static FILE *infp;

int
getshort(void)
{
    long v, w;

    v = getc(infp);
    v |= (getc(infp)<<8);	/* order is important! */

    /* worry about sign extension - sigh */
    if (v <= 0x7FFF) return v;
    w = -1;
    w &= ~0x7FFF;
    return w | v;
}


void
getstring(void)
{
    int c;
    char *cp;

    cp = strarg;
    while ((c = getc(infp)) != '\n' && c != EOF)
	*cp++ = c;
    *cp = 0;
}


double
getieee(void)
{
    unsigned char in[8];
    double d;
    size_t ret;

    ret = fread(in, 8, 1, infp);
    if (ret < 1)
	perror("fread");

    bu_cv_ntohd((unsigned char *)&d, in, 1);
    return d;
}


void
draw(double x_1, double y_1, double UNUSED(z_1), double x_2, double y_2, double z_2)
/* from point */
/* to point */
{
    int sx1, sy1, sx2, sy2;

    sx1 = (x_1 - sp[0]) / (sp[3] - sp[0]) * width;
    sy1 = (y_1 - sp[1]) / (sp[4] - sp[1]) * height;
    sx2 = (x_2 - sp[0]) / (sp[3] - sp[0]) * width;
    sy2 = (y_2 - sp[1]) / (sp[4] - sp[1]) * height;

    printf("newpath %d %d moveto %d %d lineto stroke\n", sx1, sy1, sx2, sy2);

    cx = x_2;
    cy = y_2;
    cz = z_2;

    page_dirty = 1;
}


void
label(double x, double y, char *str)
{
    int sx, sy;
    static int lastx = -1;
    static int lasty = -1;

    sx = (x - sp[0]) / (sp[3] - sp[0]) * width;
    sy = (y - sp[1]) / (sp[4] - sp[1]) * height;

    /* HACK for "continued text", i.e. more text
     * without a move command in between.  We
     * really need a better solution. - XXX
     */
    if (ZERO(lastx - x) && ZERO(lasty - y)) { /* lastx == x && lasty == y */
	printf("DFntM (%s) show\n", str);

    } else {
	printf("DFntM (%s) %d %d moveto show\n",
	       str, sx, sy);
	lastx = x;
	lasty = y;
    }

    page_dirty = 1;
}


void
scaleinfo(FILE *fp, int w, int h)

/* in points */
{
    /*
     * About this PostScript scaling issue...
     * A "unit" in postscript with no scaling is 1/72 of an inch
     * (i.e. one point).  Thus below, 8.5 x 11 inches is converted
     * to POINTS (*72).  The width and height are already given
     * in points.
     * All of our UnixPlot commands are scaled to a first quadrant
     * space from 0-4096 x 0-4096.  We thus calculate a scale
     * command to bring this space down to our width and height.
     */
    if (!encapsulated && center) {
	int xtrans, ytrans;
	xtrans = (8.5*72 - w)/2.0;
	ytrans = (11*72 - h)/2.0;
	fprintf(fp, "%d %d translate\n", xtrans, ytrans);
    }
    fprintf(fp, "%f %f scale\n", w/4096.0, h/4096.0);
}


char boilerplate1[] = "\
4 setlinewidth\n\
\n\
% Sizes, made functions to avoid scaling if not needed\n\
/FntH /Courier findfont 80 scalefont def\n\
/DFntL { /FntL /Courier findfont 73.4 scalefont def } def\n\
/DFntM { /FntM /Courier findfont 50.2 scalefont def } def\n\
/DFntS { /FntS /Courier findfont 44 scalefont def } def\n\
";

char boilerplate2[] = "\
\n\
% line styles\n\
/NV { [] 0 setdash } def		% normal vectors\n\
/DV { [8] 0 setdash } def		% dotted vectors\n\
/DDV { [8 8 32 8] 0 setdash } def	% dot-dash vectors\n\
/SDV { [32 8] 0 setdash } def		% short-dash vectors\n\
/LDV { [64 8] 0 setdash } def		% long-dash vectors\n\
\n\
FntH setfont\n\
NV\n\
% Begin Plot Data\n\
";

void
prolog(FILE *fp, const char *name, int w, int h)


/* in points */
{
    time_t ltime;

    ltime = time(0);

    if (encapsulated) {
	fputs("%!PS-Adobe-2.0 EPSF-1.2\n", fp);
	fputs("%%Creator: BRL-CAD plot3-ps\n", fp);
	fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime));
	fprintf(fp, "%%%%Title: %s\n", name);
	fputs("%%Pages: 0\n", fp);
    } else {
	fputs("%!PS-Adobe-1.0\n", fp);
	fputs("%begin(plot)\n", fp);
	fputs("%%DocumentFonts:  Courier\n", fp);
	fprintf(fp, "%%%%Title: %s\n", name);
	fputs("%%Creator: BRL-CAD plot3-ps\n", fp);
	fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime));
    }
    fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", w, h);
    fputs("%%EndComments\n\n", fp);

    scaleinfo(fp, xpoints, ypoints);
    fputs(boilerplate1, fp);
    fputs(boilerplate2, fp);
}


void
postlog(FILE *fp)
{
    fputs("\n", fp);
    if (!encapsulated)
	fputs("%end(plot)\n", fp);
    /*
     * I believe the Adobe spec says that even Encapsulated
     * PostScript files can end with a showpage.
     */
    fputs("showpage\n", fp);
}


void
getargs(struct uplot *up)
{
    int i;

    for (i = 0; i < up->narg; i++) {
	switch (up->targ) {
	    case TSHORT:
		arg[i] = getshort();
		break;
	    case TIEEE:
		arg[i] = getieee();
		break;
	    case TSTRING:
		getstring();
		break;
	    case TCHAR:
		arg[i] = getc(infp);
		break;
	    case TNONE:
	    default:
		arg[i] = 0;	/* ? */
		break;
	}
    }
}


int
main(int argc, char **argv)
{
    int c;
    struct uplot *up;
    struct plot3_ps_args args = {
	0, 0, 0, {DEFAULT_SIZE, DEFAULT_SIZE}
    };
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &plot3_ps_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&plot3_ps_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&plot3_ps_schema, "plot3-ps");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "plot3-ps help");
	return 0;
    }

    encapsulated = args.encapsulated;
    center = args.center;
    outwidth = args.dimensions.width;
    outheight = args.dimensions.height;

    if (argc - operand_index - 1 == 1) {
	file_name = argv[operand_index + 1];
	infp = fopen(file_name, "rb");
	if (!infp)
	    bu_exit(1, "plot3-ps: cannot open '%s' for reading\n", file_name);
    } else {
	file_name = Stdin;
	infp = stdin;
	if (isatty(fileno(stdin)))
	    bu_exit(1, "plot3-ps requires an input file or redirected input\n");
    }

    setmode(fileno(infp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* Default space */
    sp[0] = sp[1] = sp[2] = 0.0;	/* minimum */
    sp[3] = sp[4] = sp[5] = 4096.0;	/* max */

    if (encapsulated) {
	xpoints = width;
	ypoints = height;
    } else {
	xpoints = outwidth * 72 + 0.5;
	ypoints = outheight * 72 + 0.5;
    }
    prolog(stdout, file_name, xpoints, ypoints);

    while ((c = getc(infp)) != EOF) {
	/* look it up */
	if (c < 'A' || c > 'z') {
	    up = &uerror;
	} else {
	    up = &letters[ c - 'A' ];
	}

	if (up->targ == TBAD) {
	    fprintf(stderr, "Bad command '%c' (0x%02x)\n", c, c);
	    continue;
	}

	if (up->narg > 0)
	    getargs(up);

	switch (c) {
	    case 's':
	    case 'w':
		sp[0] = arg[0];
		sp[1] = arg[1];
		sp[2] = 0;
		sp[3] = arg[2];
		sp[4] = arg[3];
		sp[5] = 0;
		break;
	    case 'S':
	    case 'W':
		sp[0] = arg[0];
		sp[1] = arg[1];
		sp[2] = arg[2];
		sp[3] = arg[3];
		sp[4] = arg[4];
		sp[5] = arg[5];
		break;
	    case 'm':
	    case 'o':
		cx = arg[0];
		cy = arg[1];
		cz = 0;
		break;
	    case 'M':
	    case 'O':
		cx = arg[0];
		cy = arg[1];
		cz = arg[2];
		break;
	    case 'n':
	    case 'q':
		draw(cx, cy, cz, arg[0], arg[1], 0.0);
		break;
	    case 'N':
	    case 'Q':
		draw(cx, cy, cz, arg[0], arg[1], arg[2]);
		break;
	    case 'l':
	    case 'v':
		draw(arg[0], arg[1], 0.0, arg[2], arg[3], 0.0);
		break;
	    case 'L':
	    case 'V':
		draw(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
		break;
	    case 'p':
	    case 'x':
		draw(arg[0], arg[1], 0.0, arg[0], arg[1], 0.0);
		break;
	    case 'P':
	    case 'X':
		draw(arg[0], arg[1], arg[2], arg[0], arg[1], arg[2]);
		break;
	    case 't':
		label(cx, cy, strarg);
		break;
	    case 'F':
		/* Flush */
		break;
	    case 'e':
		/* New page */
		if (page_dirty) {
		    printf("showpage\n");
		    /* Recenter and rescale! */
		    scaleinfo(stdout, xpoints, ypoints);
		    page_dirty = 0;
		}
		break;
	    case 'f':
		/* linmod */
		if (BU_STR_EQUAL(strarg, "solid")) {
		    printf("NV ");
		} else if (BU_STR_EQUAL(strarg, "dotted")) {
		    printf("DV ");
		} else if (BU_STR_EQUAL(strarg, "longdashed")) {
		    printf("LDV ");
		} else if (BU_STR_EQUAL(strarg, "shortdashed")) {
		    printf("SDV ");
		} else if (BU_STR_EQUAL(strarg, "dotdashed")) {
		    printf("DDV ");
		} else {
		    fprintf(stderr, "linmod %s unknown\n", strarg);
		}
		break;
	}

	if (verbose)
	    fprintf(stderr, "%s\n", up->desc);
    }

    postlog(stdout);

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
