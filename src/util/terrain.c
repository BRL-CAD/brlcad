/*                       T E R R A I N . C
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
/** @file util/terrain.c
 *
 * generate pseudo-terrain
 *
 * Options
 * w number of postings in X direction
 * n number of postings in Y direction
 * s number of postings in X, Y direction
 * L Noise Lacunarity
 * H Noise H value
 * O Noise Octaves
 * S Noise Scale
 * V Noise Vector scale (affine scale)
 * D Noise Delta
 * f noise function (f=fbm t=turb T=1.0-turb)
 * c toggle host-net conversion
 * o offset
 */

#include "common.h"

#include <limits.h>
#include <stdint.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/cv.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn.h"


int do_convert = 1;
size_t xdim = 512;
size_t ydim = 512;

double fbm_lacunarity = 2.1753974;		/* noise_lacunarity */
double fbm_h = 1.0;
double fbm_octaves = 7.0;
double fbm_offset = 1.0;

/* intentionally double for scan */
double fbm_vscale[3] = {0.0125, 0.0125, 0.0125};
double fbm_delta[3] = {1000.0, 1000.0, 1000.0};

int debug;

struct terrain_args {
    struct terrain_args *self;
    int help;
    int host_order;
    int verbose;
    int width;
    int height;
    fastf_t lacunarity;
    fastf_t h_value;
    fastf_t octaves;
    fastf_t offset;
    vect_t scale;
    vect_t delta;
    const char *function;
};

static struct terrain_args *
terrain_args_from_storage(void *storage)
{
    return storage ? *(struct terrain_args **)storage : NULL;
}

static int
terrain_positive_number(struct bu_vls *msg, const char *arg)
{
    fastf_t value;

    if (!bu_arg_number_from_str(msg, &value, arg))
	return -1;
    if (value <= 0.0) {
	if (msg)
	    bu_vls_printf(msg, "value must be greater than zero: %s\n", arg);
	return -1;
    }
    return 0;
}

static int
terrain_square_size(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage)
{
    struct terrain_args *args = terrain_args_from_storage(storage);
    int value;

    if (argc != 1 ||
	!bu_arg_integer_range_from_str(msg, &value, argv[0], 10, 1, INT_MAX))
	return -1;
    if (args)
	args->width = args->height = value;
    return 0;
}

static int
terrain_uniform_scale(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage)
{
    struct terrain_args *args = terrain_args_from_storage(storage);
    fastf_t value;

    if (argc != 1 || !bu_arg_number_from_str(msg, &value, argv[0]))
	return -1;
    if (value <= 0.0) {
	if (msg)
	    bu_vls_printf(msg, "scale must be greater than zero: %s\n", argv[0]);
	return -1;
    }
    if (args)
	VSETALL(args->scale, value);
    return 0;
}

static const struct bu_arg_value_keyword terrain_function_values[] = {
    {"fbm", BU_ARG_ALIASES("f"), "Fractional Brownian motion"},
    {"turbulence", BU_ARG_ALIASES("t"), "Turbulence"},
    {"inverted-turbulence", BU_ARG_ALIASES("T"), "One minus turbulence"},
    {"multifractal", BU_ARG_ALIASES("m"), "Multifractal noise"},
    {"ridged-multifractal", BU_ARG_ALIASES("r"), "Ridged multifractal noise"},
    {"lunar", BU_ARG_ALIASES("L"), "Lunar-like terrain"},
    {"lee", BU_ARG_ALIASES("l"), "Detail concentrated on peaks and valleys"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option terrain_options[] = {
    BU_ARG_FLAG("h", "help", struct terrain_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_INTEGER_RANGE("w", "width", struct terrain_args, width,
	1, 1, 1, INT_MAX, "postings", "Set X postings (default: 512)"),
    BU_ARG_INTEGER_RANGE("n", "height", struct terrain_args, height,
	1, 1, 1, INT_MAX, "postings", "Set Y postings (default: 512)"),
    BU_ARG_OPTION_SHAPED("s", "size", "size", struct terrain_args, self,
	BU_ARG_VALUE_INTEGER, "postings", "Set equal X and Y postings",
	BU_ARG_REQUIRED, NULL, terrain_square_size),
    BU_ARG_VALUE_VALIDATE("L", "lacunarity", struct terrain_args, lacunarity,
	BU_ARG_VALUE_NUMBER, terrain_positive_number, "factor",
	"Set positive scale between successive noise octaves (default: 2.1753974)"),
    BU_ARG_VALUE_VALIDATE("H", "fractal-increment", struct terrain_args, h_value,
	BU_ARG_VALUE_NUMBER, terrain_positive_number, "value",
	"Set positive fractal increment controlling detail across octaves (default: 1)"),
    BU_ARG_VALUE_VALIDATE("O", "octaves", struct terrain_args, octaves,
	BU_ARG_VALUE_NUMBER, terrain_positive_number, "count",
	"Set positive number of noise octaves (default: 7)"),
    BU_ARG_OPTION_SHAPED("S", "scale", "scale", struct terrain_args, self,
	BU_ARG_VALUE_NUMBER, "factor", "Set all XYZ noise scales to a positive factor",
	BU_ARG_REQUIRED, NULL, terrain_uniform_scale),
    BU_ARG_VECTOR3("V", "vector-scale", struct terrain_args, scale,
	"x,y,z", "Set independent finite XYZ noise scales (default: 0.0125,0.0125,0.0125)"),
    BU_ARG_VECTOR3("D", "delta", struct terrain_args, delta,
	"x,y,z", "Set finite XYZ noise-space translation (default: 1000,1000,1000)"),
    BU_ARG_VALUE("o", "offset", struct terrain_args, offset,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "value",
	"Set multifractal and ridged offset (default: 1)"),
    BU_ARG_KEYWORD_VALUES("f", "function", struct terrain_args, function,
	"name", "Select the noise function (default: fbm)",
	terrain_function_values),
    BU_ARG_FLAG("c", "host-order", struct terrain_args, host_order,
	"Write host byte order instead of portable network byte order"),
    BU_ARG_FLAG("v", "verbose", struct terrain_args, verbose,
	"Print noise-function and clamping diagnostics"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema terrain_schema = BU_ARG_SCHEMA(
    "terrain", "Generate a procedural terrain height field",
    terrain_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar terrain_grammar = {
    &terrain_schema, NULL, NULL
};

static int
terrain_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&terrain_schema, "terrain");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "terrain help");
    return status;
}

/* transform a point in integer X, Y, Z space to appropriate noise space */
static void
xform(point_t t, point_t pt)
{
    t[X] = fbm_delta[X] + pt[X] * fbm_vscale[X];
    t[Y] = fbm_delta[Y] + pt[Y] * fbm_vscale[Y];
    t[Z] = fbm_delta[Z] + pt[Z] * fbm_vscale[Z];
}


/***********************************************************************
 * Fractional Brownian motion noise
 */
void
func_fbm(unsigned short *buf)
{
    point_t pt;
    size_t x, y;
    vect_t t;
    double v;

    if (debug) bu_log("fbm\n");

    pt[Z] = 0.0;
    for (y = 0; y < ydim; y++) {
	pt[Y] = y;
	for (x = 0; x < xdim; x++) {
	    pt[X] = x;

	    xform(t, pt);
	    v = bn_noise_fbm(t, fbm_h, fbm_lacunarity, fbm_octaves);
	    if (v > 1.0 || v < -1.0)
		if (debug) bu_log("clamping noise value %g \n", v);
	    v = v * 0.5 + 0.5;
	    CLAMP(v, 0.0, 1.0);

	    buf[y*xdim + x] = 1.0 + 65534.0 * v;
	}
    }
}


/***********************************************************************
 * Turbulence noise
 */
void
func_turb(unsigned short *buf)
{
    point_t pt;
    size_t x, y;
    vect_t t;
    double v;

    if (debug) bu_log("turb\n");

    pt[Z] = 0.0;
    for (y = 0; y < ydim; y++) {
	pt[Y] = y;
	for (x = 0; x < xdim; x++) {
	    pt[X] = x;

	    xform(t, pt);

	    v = bn_noise_turb(t, fbm_h,
			      fbm_lacunarity, fbm_octaves);

	    if (v > 1.0 || v < 0.0)
		if (debug) bu_log("clamping noise value %g \n", v);
	    CLAMP(v, 0.0, 1.0);
	    buf[y*xdim + x] = 1.0 + 65534.0 * v;
	}
    }
}


/***********************************************************************
 * Upside-down turbulence noise
 */
void
func_turb_up(unsigned short *buf)
{
    point_t pt;
    size_t x, y;
    vect_t t;
    double v;

    if (debug) bu_log("1.0 - turb\n");

    pt[Z] = 0.0;
    for (y = 0; y < ydim; y++) {
	pt[Y] = y;
	for (x = 0; x < xdim; x++) {
	    pt[X] = x;

	    xform(t, pt);

	    v = bn_noise_turb(t, fbm_h,
			      fbm_lacunarity, fbm_octaves);
	    CLAMP(v, 0.0, 1.0);
	    v = 1.0 - v;

	    if (v > 1.0 || v < 0.0)
		if (debug) bu_log("clamping noise value %g \n", v);
	    buf[y*xdim + x] = 1 + 65535.0 * v;
	}
    }
}


/***********************************************************************
 * Multi-fractal
 */
void
func_multi(unsigned short *buf)
{
    point_t pt;
    size_t x, y;
    vect_t t;
    double v;
    double min_V, max_V;

    if (debug) bu_log("multi\n");

    min_V = 10.0;
    max_V = -10.0;

    pt[Z] = 0.0;
    for (y = 0; y < ydim; y++) {
	pt[Y] = y;
	for (x = 0; x < xdim; x++) {
	    pt[X] = x;

	    xform(t, pt);

	    v = bn_noise_mf(t, fbm_h,
			    fbm_lacunarity, fbm_octaves,
			    fbm_offset);

	    v -= .3;
	    v *= 0.8;
	    if (v < min_V) min_V = v;
	    if (v > max_V) max_V = v;

	    if (v > 1.0 || v < 0.0) {
		if (debug) bu_log("clamping noise value %g \n", v);
		CLAMP(v, 0.0, 1.0);
	    }
	    buf[y*xdim + x] = 1 + 65534000.0 * v;
	}
    }
    if (debug) bu_log("min_V: %g   max_V: %g\n", min_V, max_V);

}


/***********************************************************************
 * Ridged multi-fractal
 */
void
func_ridged(unsigned short *buf)
{
    point_t pt;
    size_t x, y;
    vect_t t;
    double v;
    double lo, hi;

    if (debug) bu_log("ridged\n");

    lo = 10.0;
    hi = -10.0;

    pt[Z] = 0.0;
    for (y = 0; y < ydim; y++) {
	pt[Y] = y;
	for (x = 0; x < xdim; x++) {
	    pt[X] = x;

	    xform(t, pt);

	    v = bn_noise_ridged(t, fbm_h,
				fbm_lacunarity, fbm_octaves,
				fbm_offset);
	    if (v < lo) lo = v;
	    if (v > hi) hi = v;
	    v *= 0.5;

	    if (v > 1.0 || v < 0.0)
		if (debug) bu_log("clamping noise value %g \n", v);
	    CLAMP(v, 0.0, 1.0);
	    buf[y*xdim + x] = 1.0 + 65534.0 * v;
	}
    }
}


#define PSCALE(_p, _s) _p[0] *= _s; _p[1] *= _s; _p[2] *= _s
#define PCOPY(_d, _s) _d[0] = _s[0]; _d[1] = _s[1]; _d[2] = _s[2]
double
fiord(point_t point, double h, double lacunarity, double octaves)
{
    int i = 0;
    point_t pt;
    double weight, sig, freq, result;

    if (debug) bu_log("fiord\n");

    PCOPY(pt, point);
    freq = weight = .5;
    result = 0.0;

    do {
	sig = fabs(bn_noise_perlin(pt)) * pow(freq, -h);
	result += sig * weight;
	weight = result;
	freq *= lacunarity;
	PSCALE(pt, lacunarity);
    } while (++i < octaves);

    return result;
}


double
ice(point_t point, double h, double lacunarity, double octaves)
{
    int i = 0;
    point_t pt;
    double weight, sig, freq, result;
    static double lo = 10.0;
    static double hi = -10.0;

    if (debug) bu_log("ice\n");

    PCOPY(pt, point);
    freq =  1.0;
    weight = 1.0;
    result = 0.0;

    do {
	sig = fabs(bn_noise_perlin(pt)) * pow(freq, -h);

	if (sig < lo) {
	    lo = sig;
	    if (debug) bu_log("new low %g\n", lo);
	}
	if (sig > hi) {
	    hi = sig;
	    if (debug) bu_log("new high %g\n", hi);
	}

	result += sig * weight;
	weight -= result;
	freq *= lacunarity;
	PSCALE(pt, lacunarity);
    } while (++i < octaves);

    return 1.0 - result;
}


double
lunar2(point_t point, double h, double lacunarity, double octaves)
{
    int i = 0;
    point_t pt;
    double sig, freq, result;
    static double lo = 10.0;
    static double hi = -10.0;

    if (debug) bu_log("lunar2\n");

    PCOPY(pt, point);
    freq =  1.0;
    result = 0.0;

    do {
	sig = fabs(bn_noise_perlin(pt));
	sig *= sig;

	if (sig < lo) {
	    lo = sig;
	    if (debug) bu_log("new low %g\n", lo);
	}
	if (sig > hi) {
	    hi = sig;
	    if (debug) bu_log("new high %g\n", hi);
	}

	result += sig * pow(freq, -h);
	freq *= lacunarity;
	PSCALE(pt, lacunarity);
    } while (++i < octaves);

    return 1.0 - result;
}


/***********************************************************************
 * This one has detail on the peaks
 */
double
land(point_t point, double h, double lacunarity, double octaves)
{
    int i = 0;
    point_t pt;
    double weight, sig, freq, result;

    if (debug) bu_log("land\n");

    PCOPY(pt, point);
    freq =  1.0;
    weight = 1.0;
    result = 0.0;

    do {
	sig = fabs(bn_noise_perlin(pt));
	sig *= weight;

	result += sig * pow(freq, -h);
	weight = 0.5 - result;
	CLAMP(weight, 0.0, 1.0);
	freq *= lacunarity;
	PSCALE(pt, lacunarity);
    } while (++i < octaves);


    return 1.0 - result;
}


/***********************************************************************
 * This one has detail on the peaks and in the valleys, but not on the
 * slopes.
 */
double
lee(point_t point, double h, double lacunarity, double octaves)
{
    int i = 0;
    point_t pt;
    double weight, sig, freq, result;

    if (debug) bu_log("lee\n");

    PCOPY(pt, point);
    freq =  1.0;
    weight = 1.0;
    result = 0.0;

    do {
	sig = fabs(bn_noise_perlin(pt));
	sig *=  1.5 * weight;


	result += sig * pow(freq, -h);
	weight = .6 - result;
	freq *= lacunarity;
	PSCALE(pt, lacunarity);
    } while (++i < octaves);


    return 1.2 - result;
}


/***********************************************************************
 * Ridged multi-fractal
 */
void
func_lee(unsigned short *buf)
{
    point_t pt;
    size_t x, y;
    vect_t t;
    double v;
    double lo, hi;

    if (debug) bu_log("func_lee\n");

    lo = 10.0;
    hi = -10.0;

    pt[Z] = 0.0;
    for (y = 0; y < ydim; y++) {
	pt[Y] = y;
	for (x = 0; x < xdim; x++) {
	    pt[X] = x;

	    xform(t, pt);

	    v = lee(t, fbm_h, fbm_lacunarity, fbm_octaves);
	    if (v < lo) lo = v;
	    if (v > hi) hi = v;
	    v *= 0.5;

	    if (v > 1.0 || v < 0.0)
		if (debug) bu_log("clamping noise value %g \n", v);
	    CLAMP(v, 0.0, 1.0);
	    buf[y*xdim + x] = 1.0 + 65534.0 * v;
	}
    }
    if (debug) bu_log("min: %g max: %g\n", lo, hi);
}


/***********************************************************************
 * Ridged multi-fractal
 */
void
func_lunar(unsigned short *buf)
{
    point_t pt;
    size_t x, y;
    vect_t t;
    double v;
    double lo, hi;

    if (debug) bu_log("lee\n");

    lo = 10.0;
    hi = -10.0;

    pt[Z] = 0.0;
    for (y = 0; y < ydim; y++) {
	pt[Y] = y;
	for (x = 0; x < xdim; x++) {
	    pt[X] = x;

	    xform(t, pt);

	    /*
	      1 fiord
	      2 lunar2
	      3 ice
	      4 land
	      5 lee

	    */
	    v = fiord(t, fbm_h, fbm_lacunarity, fbm_octaves);
	    if (v < lo) lo = v;
	    if (v > hi) hi = v;

	    if (v > 1.0 || v < 0.0)
		if (debug) bu_log("clamping noise value %g \n", v);
	    CLAMP(v, 0.0, 1.0);
	    buf[y*xdim + x] = 1.0 + 65534.0 * v;
	}
    }
    if (debug) bu_log("min: %g max: %g\n", lo, hi);
}


typedef void (*terrain_func_t)(unsigned short *);

static terrain_func_t
terrain_function(const char *name)
{
    if (BU_STR_EQUAL(name, "fbm"))
	return &func_fbm;
    if (BU_STR_EQUAL(name, "turbulence"))
	return &func_turb;
    if (BU_STR_EQUAL(name, "inverted-turbulence"))
	return &func_turb_up;
    if (BU_STR_EQUAL(name, "multifractal"))
	return &func_multi;
    if (BU_STR_EQUAL(name, "ridged-multifractal"))
	return &func_ridged;
    if (BU_STR_EQUAL(name, "lunar"))
	return &func_lunar;
    if (BU_STR_EQUAL(name, "lee"))
	return &func_lee;
    return NULL;
}


int
main(int argc, char *argv[])
{
    struct terrain_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    unsigned short *buf;
    int in_cookie, out_cookie;
    int introspection;
    size_t count;
    size_t ret;
    terrain_func_t terrain_func;

    bu_setprogname(argv[0]);
    args.self = &args;
    args.width = 512;
    args.height = 512;
    args.lacunarity = 2.1753974;
    args.h_value = 1.0;
    args.octaves = 7.0;
    args.offset = 1.0;
    VSETALL(args.scale, 0.0125);
    VSETALL(args.delta, 1000.0);
    args.function = "fbm";

    introspection = bu_arg_introspect(&msg, &terrain_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    if (bu_arg_schema_parse_complete(&terrain_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return terrain_help(stdout, 0);

    xdim = (size_t)args.width;
    ydim = (size_t)args.height;
    fbm_lacunarity = args.lacunarity;
    fbm_h = args.h_value;
    fbm_octaves = args.octaves;
    fbm_offset = args.offset;
    VMOVE(fbm_vscale, args.scale);
    VMOVE(fbm_delta, args.delta);
    do_convert = !args.host_order;
    debug = args.verbose;
    terrain_func = terrain_function(args.function);
    if (!terrain_func) {
	bu_flog(stderr, "terrain: unsupported noise function '%s'\n",
		args.function);
	return 1;
    }

    setmode(fileno(stdout), O_BINARY);
    if (isatty(fileno(stdout))) {
	bu_flog(stderr, "terrain: standard output must be redirected\n");
	return terrain_help(stderr, 1);
    }

    if (xdim > SIZE_MAX / ydim || xdim * ydim > SIZE_MAX / sizeof(*buf))
	bu_exit(1, "%s: dimensions %zu x %zu exceed the addressable output size\n",
		bu_getprogname(), xdim, ydim);
    count = xdim * ydim;
    buf = (unsigned short *)bu_malloc(sizeof(*buf) * count, "buf");

    terrain_func(buf);

    if (do_convert) {
	/* make sure the output is going as network unsigned shorts */

	in_cookie = bu_cv_cookie("hus");

	out_cookie = bu_cv_cookie("nus");

	if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
	    if (debug) bu_log("converting to network order\n");
	    bu_cv_w_cookie(buf, out_cookie, count*sizeof(*buf),
			   buf, in_cookie, count);
	}
    }

    ret = fwrite(buf, sizeof(*buf), count, stdout);
    if (ret < (size_t)count)
	perror("fwrite");
    bu_free(buf, "buf");

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
