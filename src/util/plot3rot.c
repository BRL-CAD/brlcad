/*                      P L O T 3 R O T . C
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
/** @file util/plot3rot.c
 *
 * Rotate, Translate, and Scale a Unixplot file.
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h> /* atof() */
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "bio.h"   /* for isatty */
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/cv.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"
#include "bv/plot3.h"
#include "bn.h"
#include "bg/plane.h"

#define UPPER_CASE(c)	((c)-32)
#define COPY(n) {size_t ret; ret = fread(cbuf, 1, n, fp); if (ret < n) perror("fread"); ret = fwrite(cbuf, 1, n, stdout); if (ret < n) perror("fwrite");}
#define SKIP(n) {size_t ret; ret = fread(cbuf, 1, n, fp); if (ret < n) perror("fread");}
#define LEN 265

#define putsi(s) {putchar(s); putchar((s)>>8);}

double getdouble(FILE *fp);

char cbuf[48];		/* COPY and SKIP macro buffer */

mat_t rmat;			/* rotation matrix to be applied */
double scale;			/* scale factor to be applied */

int verbose;

int space_set;		/* current space has been explicitly set */
point_t space_min, space_max;	/* Current space */

int forced_space;		/* space is being forced */
point_t forced_space_min, forced_space_max;	/* forced space */

int rpp;			/* flag: compute new center values */
int Mflag;			/* flag: don't rebound space rpp */

void dofile(FILE *fp);
void copy_string(FILE *fp);
void two_coord_out(FILE *fp, fastf_t *m);
void three_coord_out(FILE *fp, fastf_t *m);
void two_dcoord_out(FILE *fp, fastf_t *m);
void three_dcoord_out(FILE *fp, fastf_t *m);

static int
parse_doubles(struct bu_vls *msg, const char *arg, double *out_values,
    size_t value_count, const char *label)
{
    double parsed[16];
    char *end = NULL;
    const char *cp = arg;

    if (!arg || !out_values || value_count > 16)
	return 0;

    for (size_t i = 0; i < value_count; i++) {
	while (isspace((unsigned char)*cp))
	    cp++;
	if (*cp == '\0')
	    goto invalid;

	errno = 0;
	parsed[i] = strtod(cp, &end);
	if (errno != 0 || end == cp || !isfinite(parsed[i]))
	    goto invalid;
	cp = end;
    }

    while (isspace((unsigned char)*cp))
	cp++;
    if (*cp != '\0')
	goto invalid;
    memcpy(out_values, parsed, value_count * sizeof(double));
    return 1;

invalid:
    if (msg)
	bu_vls_printf(msg, "%s requires exactly %zu finite numbers\n",
	    label, value_count);
    return 0;
}


/*
 * Process a space command.
 * Behavior depends on setting of several flags.
 *
 * Implicit Returns -
 * In all cases, sets space_min and space_max.
 */
int
model_rpp(const fastf_t *min, const fastf_t *max)
{

    if (space_set) {
	bu_log("plot3rot:  additional SPACE command ignored\n");
	bu_log("got: space (%g, %g, %g) (%g, %g, %g)\n",
	       V3ARGS(min), V3ARGS(max));
	bu_log("still using: space (%g, %g, %g) (%g, %g, %g)\n",
	       V3ARGS(space_min), V3ARGS(space_max));
	return 0;
    }

    if (rpp) {
	point_t rot_center;		/* center of rotation */
	mat_t xlate;
	mat_t resize;
	mat_t t1, t2;

	VADD2SCALE(rot_center, min, max, 0.5);

	/* Create the matrix which encodes this */
	MAT_IDN(xlate);
	MAT_DELTAS_VEC_NEG(xlate, rot_center);
	MAT_IDN(resize);
	resize[15] = 1/scale;
	bn_mat_mul(t1, resize, xlate);
	bn_mat_mul(t2, rmat, t1);
	MAT_COPY(rmat, t2);
	if (verbose) {
	    bn_mat_print("rmat", rmat);
	}

	if (Mflag) {
	    /* Don't rebound, just expand size of space
	     * around center point.
	     * Has advantage of the output space() not being
	     * affected by changes in rotation,
	     * which may be significant for animation scripts.
	     */
	    vect_t diag;
	    double v;

	    VSUB2(diag, max, min);
	    v = MAGNITUDE(diag)*0.5 + 0.5;
	    VSET(space_min, -v, -v, -v);
	    VSET(space_max,  v,  v,  v);
	} else {
	    /* re-bound the space() rpp with a tighter one
	     * after rotating & scaling it.
	     */
	    bg_rotate_bbox(space_min, space_max, rmat, min, max);
	}
	space_set = 1;
    } else {
	VMOVE(space_min, min);
	VMOVE(space_max, max);
	space_set = 1;
    }

    if (forced_space) {
	/* Put forced space back */
	VMOVE(space_min, forced_space_min);
	VMOVE(space_max, forced_space_max);
	space_set = 1;
    }

    if (verbose) {
	bu_log("got: space (%g, %g, %g) (%g, %g, %g)\n",
	       V3ARGS(min), V3ARGS(max));
	bu_log("put: space (%g, %g, %g) (%g, %g, %g)\n",
	       V3ARGS(space_min), V3ARGS(space_max));
    }

    return 1;
}


int
getshort(FILE *fp)
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


struct plot3rot_args {
    struct plot3rot_args *self;
    int help;
    int verbose;
};

enum plot3rot_transform {
    PLOT3ROT_AZIMUTH,
    PLOT3ROT_ELEVATION,
    PLOT3ROT_X_ROTATE,
    PLOT3ROT_Y_ROTATE,
    PLOT3ROT_Z_ROTATE,
    PLOT3ROT_X_TRANSLATE,
    PLOT3ROT_Y_TRANSLATE,
    PLOT3ROT_Z_TRANSLATE,
    PLOT3ROT_SCALE
};

static int
plot3rot_transform_parse(struct bu_vls *msg, const char *arg, void *storage,
	enum plot3rot_transform transform)
{
    mat_t tmp, previous;
    double value;

    if (!bu_arg_double_from_str(msg, &value, arg))
	return -1;
    if (transform == PLOT3ROT_SCALE && ZERO(value)) {
	if (msg)
	    bu_vls_strcat(msg, "scale factor must be nonzero\n");
	return -1;
    }
    if (!storage)
	return 0;

    switch (transform) {
	case PLOT3ROT_AZIMUTH:
	    bn_mat_angles(tmp, 0.0, 0.0, -value);
	    rpp++;
	    break;
	case PLOT3ROT_ELEVATION:
	    bn_mat_angles(tmp, 0.0, -value, 0.0);
	    rpp++;
	    break;
	case PLOT3ROT_X_ROTATE:
	    bn_mat_angles(tmp, value, 0.0, 0.0);
	    break;
	case PLOT3ROT_Y_ROTATE:
	    bn_mat_angles(tmp, 0.0, value, 0.0);
	    break;
	case PLOT3ROT_Z_ROTATE:
	    bn_mat_angles(tmp, 0.0, 0.0, value);
	    break;
	case PLOT3ROT_X_TRANSLATE:
	    MAT_IDN(tmp);
	    tmp[MDX] = value;
	    break;
	case PLOT3ROT_Y_TRANSLATE:
	    MAT_IDN(tmp);
	    tmp[MDY] = value;
	    break;
	case PLOT3ROT_Z_TRANSLATE:
	    MAT_IDN(tmp);
	    tmp[MDZ] = value;
	    break;
	case PLOT3ROT_SCALE:
	    scale *= value;
	    if (rpp)
		return 0;
	    MAT_IDN(tmp);
	    tmp[15] = 1.0 / scale;
	    scale = 1.0;
	    break;
    }

    MAT_COPY(previous, rmat);
    bn_mat_mul(rmat, tmp, previous);
    return 0;
}

#define PLOT3ROT_TRANSFORM_PARSER(_name, _transform) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return plot3rot_transform_parse(msg, arg, storage, _transform); \
    }

PLOT3ROT_TRANSFORM_PARSER(plot3rot_azimuth, PLOT3ROT_AZIMUTH)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_elevation, PLOT3ROT_ELEVATION)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_x_rotate, PLOT3ROT_X_ROTATE)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_y_rotate, PLOT3ROT_Y_ROTATE)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_z_rotate, PLOT3ROT_Z_ROTATE)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_x_translate, PLOT3ROT_X_TRANSLATE)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_y_translate, PLOT3ROT_Y_TRANSLATE)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_z_translate, PLOT3ROT_Z_TRANSLATE)
PLOT3ROT_TRANSFORM_PARSER(plot3rot_scale, PLOT3ROT_SCALE)

static int
plot3rot_model_space(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (storage) {
	rpp++;
	Mflag = 1;
    }
    return 0;
}

static int
plot3rot_mged_view(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (storage) {
	mat_t tmp, previous;
	bn_mat_angles(tmp, -90.0, 0.0, -90.0);
	MAT_COPY(previous, rmat);
	bn_mat_mul(rmat, tmp, previous);
    }
    return 0;
}

static int
plot3rot_matrix(struct bu_vls *msg, const char *arg, void *storage)
{
    double values[16];

    if (!parse_doubles(msg, arg, values, 16, "matrix"))
	return -1;
    if (storage) {
	for (size_t i = 0; i < 16; i++)
	    rmat[i] = values[i];
    }
    return 0;
}

static int
plot3rot_space(struct bu_vls *msg, const char *arg, void *storage)
{
    double values[6];

    if (!parse_doubles(msg, arg, values, 6, "space"))
	return -1;
    if (storage) {
	VSET(forced_space_min, values[0], values[1], values[2]);
	VSET(forced_space_max, values[3], values[4], values[5]);
	pdv_3space(stdout, forced_space_min, forced_space_max);
	forced_space = 1;
    }
    return 0;
}

static const struct bu_arg_option plot3rot_options[] = {
    BU_ARG_FLAG("h", "help", struct plot3rot_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("M", "model-space", "model-space",
	struct plot3rot_args, self, plot3rot_model_space,
	"Auto-center and fit a rotation-invariant model space"),
    BU_ARG_CUSTOM_FLAG("g", "mged-view", "mged-view",
	struct plot3rot_args, self, plot3rot_mged_view,
	"Convert MGED front-view coordinates to display coordinates"),
    BU_ARG_CUSTOM("a", "azimuth", struct plot3rot_args, self,
	plot3rot_azimuth, "degrees", "Rotate by view azimuth and auto-center"),
    BU_ARG_CUSTOM("e", "elevation", struct plot3rot_args, self,
	plot3rot_elevation, "degrees", "Rotate by view elevation and auto-center"),
    BU_ARG_CUSTOM("x", "x-rotate", struct plot3rot_args, self,
	plot3rot_x_rotate, "degrees", "Rotate about the X axis"),
    BU_ARG_CUSTOM("y", "y-rotate", struct plot3rot_args, self,
	plot3rot_y_rotate, "degrees", "Rotate about the Y axis"),
    BU_ARG_CUSTOM("z", "z-rotate", struct plot3rot_args, self,
	plot3rot_z_rotate, "degrees", "Rotate about the Z axis"),
    BU_ARG_CUSTOM("X", "x-translate", struct plot3rot_args, self,
	plot3rot_x_translate, "distance", "Translate along the X axis"),
    BU_ARG_CUSTOM("Y", "y-translate", struct plot3rot_args, self,
	plot3rot_y_translate, "distance", "Translate along the Y axis"),
    BU_ARG_CUSTOM("Z", "z-translate", struct plot3rot_args, self,
	plot3rot_z_translate, "distance", "Translate along the Z axis"),
    BU_ARG_CUSTOM("s", "scale", struct plot3rot_args, self,
	plot3rot_scale, "factor", "Apply a nonzero scale factor"),
    BU_ARG_CUSTOM("m", "matrix", struct plot3rot_args, self,
	plot3rot_matrix, "matrix", "Replace the accumulated transform with a quoted 4x4 matrix"),
    BU_ARG_CUSTOM("S", "space", struct plot3rot_args, self,
	plot3rot_space, "bounds", "Emit and force quoted min-x min-y min-z max-x max-y max-z bounds"),
    BU_ARG_COUNTING_FLAG("v", "verbose", struct plot3rot_args, verbose,
	BU_ARG_VALUE_INTEGER, "Print transformation and space diagnostics"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand plot3rot_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, BU_ARG_COUNT_UNLIMITED,
	"Plot input file or - for standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema plot3rot_schema = BU_ARG_SCHEMA(
    "plot3rot", "Rotate, translate, and scale Plot3 streams",
    plot3rot_options, plot3rot_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar plot3rot_grammar = {
    &plot3rot_schema, NULL, NULL
};

static int
plot3rot_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&plot3rot_schema, "plot3rot");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "plot3rot help");
    return status;
}


int
main(int argc, char **argv)
{
    struct plot3rot_args args = {NULL, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    FILE *fp=NULL;
    int introspection;
    int operand_index;

    bu_setprogname(argv[0]);
    args.self = &args;
    MAT_IDN(rmat);
    scale = 1.0;

    introspection = bu_arg_introspect(&msg, &plot3rot_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    operand_index = bu_arg_schema_parse_complete(&plot3rot_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return plot3rot_help(stdout, 0);
    if (isatty(fileno(stdout)) ||
	(isatty(fileno(stdin)) && operand_index >= argc - 1))
	return plot3rot_help(stderr, 1);

    verbose = args.verbose;

    if (verbose) {
	bn_mat_print("rmat", rmat);
    }

    if (operand_index < argc - 1) {
	int arg_index = operand_index + 1;
	while (arg_index < argc) {
	    if (fp != NULL && fp != stdin)
		fclose(fp);
	    if (BU_STR_EQUAL(argv[arg_index], "-"))
		fp = stdin;
	    else if ((fp = fopen(argv[arg_index], "rb")) == NULL) {
		bu_log("plot3rot: can't open \"%s\"\n", argv[arg_index]);
		arg_index++;
		continue;
	    }
	    dofile(fp);
	    arg_index++;
	}
    } else {
	dofile(stdin);
    }
    return 0;
}


void
dofile(FILE *fp)
{
    int c;

    while ((c = getc(fp)) != EOF) {
	switch (c) {
	    /* One of a kind functions */
	    case 'e':
	    case 'F':
		putchar(c);
		break;
	    case 'f':
	    case 't':
		putchar(c);
		copy_string(fp);
		break;
	    case 'C':
		putchar(c);
		COPY(3);
		break;
	    case 'c':	/* map x, y? */
		putchar(c);
		COPY(6);
		break;
	    case 'a':	/* map points? */
		putchar(c);
		COPY(12);
		break;
	    case 'p':
	    case 'm':
	    case 'n':
		/* Two coordinates in, three out. Change command. */
		putchar(UPPER_CASE(c));
		two_coord_out(fp, rmat);
		break;
	    case 'l':
		putchar('L');
		two_coord_out(fp, rmat);
		two_coord_out(fp, rmat);
		break;
	    case 'P':
	    case 'M':
	    case 'N':
		putchar(c);
		three_coord_out(fp, rmat);
		break;
	    case 'L':
		putchar(c);
		three_coord_out(fp, rmat);
		three_coord_out(fp, rmat);
		break;
	    case 's':
		{
		    /* 2-D integer SPACE command.
		     * This is the only AT&T compatible space
		     * command;  be certain to only output
		     * with pl_space(), to ensure that output
		     * file is AT&T style if input was.
		     */
		    long minx, miny, maxx, maxy;
		    point_t min, max;

		    minx = getshort(fp);
		    miny = getshort(fp);
		    maxx = getshort(fp);
		    maxy = getshort(fp);

		    VSET(min, minx, miny, -1);
		    VSET(max, maxx, maxy, -1);
		    model_rpp(min, max);

		    minx = lrint(floor(space_min[X]));
		    miny = lrint(floor(space_min[Y]));
		    maxx = lrint(ceil(space_max[X]));
		    maxy = lrint(ceil(space_max[Y]));
		    if (minx < -32768) minx = -32768;
		    if (miny < -32768) miny = -32768;
		    if (maxx > 32767) maxx = 32767;
		    if (maxy > 32767) maxy = 32767;

		    pl_space(stdout, minx, miny, maxx, maxy);
		}
		break;
	    case 'S':
		{
		    /* BRL-extended 3-D integer SPACE command */
		    point_t min, max;

		    min[X] = getshort(fp);
		    min[Y] = getshort(fp);
		    min[Z] = getshort(fp);
		    max[X] = getshort(fp);
		    max[Y] = getshort(fp);
		    max[Z] = getshort(fp);

		    model_rpp(min, max);

		    pdv_3space(stdout, space_min, space_max);
		}
		break;
		/* 2D and 3D IEEE */
	    case 'w':
		{
		    /* BRL 2-D floating point SPACE command */
		    point_t min, max;

		    min[X] = getdouble(fp);
		    min[Y] = getdouble(fp);
		    min[Z] = -1.0;
		    max[X] = getdouble(fp);
		    max[Y] = getdouble(fp);
		    max[Z] = 1.0;

		    model_rpp(min, max);

		    pdv_3space(stdout, space_min, space_max);
		}
		break;
	    case 'W':
		{
		    /* BRL 3-D floating point SPACE command */
		    point_t min, max;

		    min[X] = getdouble(fp);
		    min[Y] = getdouble(fp);
		    min[Z] = getdouble(fp);
		    max[X] = getdouble(fp);
		    max[Y] = getdouble(fp);
		    max[Z] = getdouble(fp);

		    model_rpp(min, max);

		    pdv_3space(stdout, space_min, space_max);
		}
		break;
	    case 'i':
		putchar(c);
		COPY(3*8);
		break;
	    case 'r':
		putchar(c);
		COPY(6*8);
		break;
	    case 'x':
	    case 'o':
	    case 'q':
		/* Two coordinates in, three out.  Change command. */
		putchar(UPPER_CASE(c));
		two_dcoord_out(fp, rmat);
		break;
	    case 'v':
		/* Two coordinates in, three out.  Change command. */
		putchar('V');
		two_dcoord_out(fp, rmat);
		two_dcoord_out(fp, rmat);
		break;
	    case 'X':
	    case 'O':
	    case 'Q':
		putchar(c);
		three_dcoord_out(fp, rmat);
		break;
	    case 'V':
		putchar(c);
		three_dcoord_out(fp, rmat);
		three_dcoord_out(fp, rmat);
		break;
	    default:
		bu_log("plot3rot: unrecognized command '%c' (0x%x)\n",
		       (isprint(c)) ? c : '?',
		       c);
		bu_log("plot3rot: ftell = %jd\n", (intmax_t)bu_ftell(fp));
		putchar(c);
		break;
	}
    }
}


void
copy_string(FILE *fp)
{
    int c = 1;

    while (c != '\n' && c != EOF) {
	int tc = getc(fp);
	if (tc < 0) {
	    bu_log("getc error: plot3rot.c:%d\n", __LINE__);
	    break;
	}
	c = putchar(tc);
    }
}


/******* Coordinate Transforms / Output *******/

void
two_coord_out(FILE *fp, fastf_t *m)
{
    point_t p1;
    short p2[3];

    p1[0] = getshort(fp);	/* get X */
    p1[1] = getshort(fp);	/* and Y */
    p1[2] = 0;		/* no Z */

    MAT4X3PNT(p2, m, p1);

    putsi(p2[0]);		/* put X */
    putsi(p2[1]);		/* and Y */
    putsi(p2[2]);		/* and Z */
}


void
three_coord_out(FILE *fp, fastf_t *m)
{
    point_t p1;
    short p2[3];

    p1[0] = getshort(fp);	/* get X */
    p1[1] = getshort(fp);	/* and Y */
    p1[2] = getshort(fp);	/* and Z */

    MAT4X3PNT(p2, m, p1);

    putsi(p2[0]);		/* put X */
    putsi(p2[1]);		/* and Y */
    putsi(p2[2]);		/* and Z */
}


void
two_dcoord_out(FILE *fp, fastf_t *m)
{
    unsigned char buf[3*8];
    double p1[3];
    double p2[3];
    size_t ret;

    ret = fread(buf, 1, 2*8, fp);
    if (ret < 2*8)
	perror("fread");

    bu_cv_ntohd((unsigned char *)p1, buf, 2);
    p1[2] = 0;		/* no Z */

    MAT4X3PNT(p2, m, p1);

    bu_cv_htond(buf, (unsigned char *)p2, 3);
    ret = fwrite(buf, 1, 3*8, stdout);
    if (ret < 3*8)
	perror("fwrite");
}


void
three_dcoord_out(FILE *fp, fastf_t *m)
{
    unsigned char buf[3*8];
    double p1[3];
    double p2[3];
    size_t ret;

    ret = fread(buf, 1, 3*8, fp);
    if (ret < 3*8)
	perror("fread");

    bu_cv_ntohd((unsigned char *)p1, buf, 3);

    MAT4X3PNT(p2, m, p1);

    bu_cv_htond(buf, (unsigned char *)p2, 3);
    ret = fwrite(buf, 1, 3*8, stdout);
    if (ret < 3*8)
	perror("fwrite");
}


double
getdouble(FILE *fp)
{
    double d;
    unsigned char buf[8];
    size_t ret;

    ret = fread(buf, 8, 1, fp);
    if (ret < 1)
	perror("fread");

    bu_cv_ntohd((unsigned char *)&d, buf, 1);
    return d;
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
