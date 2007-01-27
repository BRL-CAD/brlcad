/*                       T E R R A I N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file terrain.c
 *
 */

/*	T E R R A I N . C --- generate pseudo-terrain
 *
 *	Options
 *	w	number of postings in X direction
 *	n	number of postings in Y direction
 *	s	number of postings in X,Y direction
 *	L	Noise Lacunarity
 *	H	Noise H value
 *	O	Noise Octaves
 *	S	Noise Scale
 *	V	Noise Vector scale (affine scale)
 *	D	Noise Delta
 *	f	noise function (f=fbm t=turb T=1.0-turb)
 *	c	toggle host-net conversion
 *	o	offset
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


/* declarations to support use of bu_getopt() system call */
char *options = "w:n:s:L:H:O:S:V:D:f:co:v";
extern char *bu_optarg;
extern int bu_optind, opterr, bu_getopt(int, char *const *, const char *);

int do_convert = 1;
char *progname = "(noname)";
unsigned xdim = 256;
unsigned ydim = 256;

double fbm_lacunarity = 2.1753974;		/* noise_lacunarity */
double fbm_h = 1.0;
double fbm_octaves = 7.0;
double fbm_size = 1.0;
vect_t fbm_vscale = {0.0125, 0.0125, 0.0125};
vect_t fbm_delta = {1000.0, 1000.0, 1000.0};
double fbm_offset = 1.0;
int quiet = 0;

int debug;

/* transform a point in integer X,Y,Z space to appropriate noise space */
static void
xform(point_t t, point_t pt)
{
	t[X] = fbm_delta[X] + pt[X] * fbm_vscale[X];
	t[Y] = fbm_delta[Y] + pt[Y] * fbm_vscale[Y];
	t[Z] = fbm_delta[Z] + pt[Z] * fbm_vscale[Z];
}

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void
usage(char *s)
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [ flags ] > outfile]\nFlags:\n%s\n",
		       progname,
"\t-w #\t\tnumber of postings in X direction\n\
\t-n #\t\tnumber of postings in Y direction\n\
\t-s #\t\tnumber of postings in X,Y direction\n\
\t-L #\t\tNoise Lacunarity\n\
\t-H #\t\tNoise H value\n\
\t-O #\t\tNoise Octaves\n\
\t-S #\t\tNoise Scale\n\
\t-V #,#,#\tNoise Vector scale (affine scale)\n\
\t-D #,#,#\tNoise Delta\n\
\t-f func\t\tNoise function:\n\
\t\t\t\tf:fbm t:turb T:1.0-turb m:multi r:ridged");

	exit(1);
}


/***********************************************************************
 *
 *	func_fbm
 *
 *	Fractional Brownian motion noise
 */
void
func_fbm(unsigned short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;

	if (debug) bu_log("fbm\n");

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
			pt[X] = x;

			xform(t, pt);
			v = bn_noise_fbm(t, fbm_h,fbm_lacunarity, fbm_octaves);
			if (v > 1.0 || v < -1.0)
				if (debug) bu_log("clamping noise value %g \n", v);
			v = v * 0.5 + 0.5;
			CLAMP(v, 0.0, 1.0);

			buf[y*xdim + x] = 1.0 + 65534.0 * v;
		}
	}
}
/***********************************************************************
 *
 *	func_turb
 *
 *	Turbulence noise
 */
void
func_turb(unsigned short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;

	if (debug) bu_log("turb\n");

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
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
 *
 *	func_turb_up
 *
 *	Upside-down turbulence noise
 */
void
func_turb_up(short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;

	if (debug) bu_log("1.0 - turb\n");

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
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
 *
 *	func_multi
 *
 *	Multi-fractal
 */
void
func_multi(short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;
	double min_V, max_V;

	if (debug) bu_log("multi\n");

	min_V = 10.0;
	max_V =  -10.0;

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
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
 *
 *	func_ridged
 *
 *	Ridged multi-fractal
 */
void
func_ridged(unsigned short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;
	double lo, hi;

	if (debug) bu_log("ridged\n");

	lo = 10.0;
	hi = -10.0;

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
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
fiord(point_t point, double h, double lacunarity, double octaves, double offset)
{
	int i = 0;
	point_t pt;
	double weight, signal, freq, result;

	if (debug) bu_log("fiord\n");

	PCOPY(pt, point);
	freq = weight = .5;
	result = 0.0;

	do {
		signal = fabs(bn_noise_perlin(pt)) * pow(freq, -h);
		result += signal * weight;
		weight = result;
		freq *= lacunarity;
		PSCALE(pt, lacunarity);
	} while (++i < octaves);

	return result;
}

double
ice(point_t point, double h, double lacunarity, double octaves, double offset)
{
	int i = 0;
	point_t pt;
	double weight, signal, freq, result;
	static double lo = 10.0;
	static double hi = -10.0;

	if (debug) bu_log("ice\n");

	PCOPY(pt, point);
	freq =  1.0;
	weight = 1.0;
	result = 0.0;

	do {
		signal = fabs(bn_noise_perlin(pt)) * pow(freq, -h);

		if (signal < lo) {
			lo = signal;
			if (debug) bu_log("new low %g\n", lo);
		}
		if (signal > hi) {
			hi = signal;
			if (debug) bu_log("new high %g\n", hi);
		}

		result += signal * weight;
		weight -= result;
		freq *= lacunarity;
		PSCALE(pt, lacunarity);
	} while (++i < octaves);

	return 1.0 - result;
}

double
lunar2(point_t point, double h, double lacunarity, double octaves, double offset)
{
	int i = 0;
	point_t pt;
	double weight, signal, freq, result;
	static double lo = 10.0;
	static double hi = -10.0;

	if (debug) bu_log("lunar2\n");

	PCOPY(pt, point);
	freq =  1.0;
	weight = 1.0;
	result = 0.0;

	do {
		signal = fabs(bn_noise_perlin(pt));
		signal *= signal;

		if (signal < lo) {
			lo = signal;
			if (debug) bu_log("new low %g\n", lo);
		}
		if (signal > hi) {
			hi = signal;
			if (debug) bu_log("new high %g\n", hi);
		}

		result += signal * pow(freq, -h);
		weight -= signal;
		freq *= lacunarity;
		PSCALE(pt, lacunarity);
	} while (++i < octaves);

	return 1.0 - result;
}
/***********************************************************************
 * This one's got detail on the peaks
 */
double
land(point_t point, double h, double lacunarity, double octaves, double offset)
{
	int i = 0;
	point_t pt;
	double weight, signal, freq, result;

	if (debug) bu_log("land\n");

	PCOPY(pt, point);
	freq =  1.0;
	weight = 1.0;
	result = 0.0;

	do {
		signal = fabs(bn_noise_perlin(pt));
		signal *= weight;

		result += signal * pow(freq, -h);
		weight = 0.5 - result;
		CLAMP(weight, 0.0, 1.0);
		freq *= lacunarity;
		PSCALE(pt, lacunarity);
	} while (++i < octaves);


	return 1.0 - result;
}
/***********************************************************************
 * This one's got detail on the peaks and in the valleys, but not on the
 * slopes
 *
 */
double
lee(point_t point, double h, double lacunarity, double octaves, double offset)
{
	int i = 0;
	point_t pt;
	double weight, signal, freq, result;

	if (debug) bu_log("lee\n");

	PCOPY(pt, point);
	freq =  1.0;
	weight = 1.0;
	result = 0.0;

	do {
		signal = fabs(bn_noise_perlin(pt));
		signal *=  1.5 * weight;


		result += signal * pow(freq, -h);
		weight = .6 - result;
		freq *= lacunarity;
		PSCALE(pt, lacunarity);
	} while (++i < octaves);


	return 1.2 - result;
}

/***********************************************************************
 *
 *	func_lee
 *
 *	Ridged multi-fractal
 */
void
func_lee(unsigned short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;
	double lo, hi;

	if (debug) bu_log("func_lee\n");

	lo = 10.0;
	hi = -10.0;

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
			pt[X] = x;

			xform(t, pt);

			v = lee(t, fbm_h,
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
	if (debug) bu_log("min: %g max: %g\n", lo, hi);
}


/***********************************************************************
 *
 *	func_lee
 *
 *	Ridged multi-fractal
 */
void
func_lunar(unsigned short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;
	double lo, hi;

	if (debug) bu_log("lee\n");

	lo = 10.0;
	hi = -10.0;

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
			pt[X] = x;

			xform(t, pt);

			/*
1 fiord
2 lunar2
3 ice
4 land
5 lee

			*/
			v = fiord(t, fbm_h,
					  fbm_lacunarity, fbm_octaves,
					    fbm_offset);
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


/* function to call to generate the terrain.  Default noise pattern is fBm */
void (*terrain_func)() = func_fbm;

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int
parse_args(int ac, char **av)
{
	int  c;
	char *strrchr(const char *, int);
	double v;

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;

	/* Turn off bu_getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=bu_getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'v': debug = !debug; break;
		case 'c': do_convert = !do_convert; break;
		case 'w': if ((c=atoi(bu_optarg)) > 0) xdim = c;
			break;
		case 'n': if ((c=atoi(bu_optarg)) > 0) ydim = c;
			break;
		case 'q' : quiet = !quiet; break;
		case 's': if ((c=atoi(bu_optarg)) > 0) xdim = ydim = c;
			break;
		case 'L': if ((v=atof(bu_optarg)) >  0.0) fbm_lacunarity = v;
			break;
		case 'H': if ((v=atof(bu_optarg)) >  0.0) fbm_h = v;
			break;
		case 'O': if ((v=atof(bu_optarg)) >  0.0) fbm_octaves = v;
			break;

		case 'S': if ((v=atof(bu_optarg)) >  0.0) { VSETALL(fbm_vscale, v); }
			break;

		case 'V': sscanf(bu_optarg, "%lg,%lg,%lg",
			       &fbm_vscale[0], &fbm_vscale[1], &fbm_vscale[2]);
			break;
		case 'D': sscanf(bu_optarg, "%lg,%lg,%lg",
			       &fbm_delta[0], &fbm_delta[1], &fbm_delta[2]);
			break;
		case 'o': fbm_offset = atof(bu_optarg);
			break;
		case 'f':
			switch (*bu_optarg) {
			case 'L': terrain_func = func_lunar;
				break;
			case 'l': terrain_func = func_lee;
				break;
			case 'f': terrain_func = func_fbm;
				break;
			case 't': terrain_func = func_turb;
				break;
			case 'T': terrain_func = func_turb_up;
				break;
			case 'm': terrain_func = func_multi;
				break;
			case 'r': terrain_func = func_ridged;
				break;
			default:
				fprintf(stderr,
					"Unknown noise terrain_function: \"%s\"\n",
					bu_optarg);
				exit(-1);
				break;
			}
			break;
		case '?'	:
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}

	return(bu_optind);
}

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments, then
 *	produce the noise field selected.  Write out binary in network order.
 */
int
main(int ac, char **av)
{
	int arg_count;
	unsigned short *buf;
	int in_cookie, out_cookie;
	int count;

	arg_count = parse_args(ac, av);

	if (arg_count+1 < ac) usage("Excess arguments on cmd line\n");

	if (isatty(fileno(stdout))) usage("Redirect standard output\n");

	if (arg_count < ac)
		fprintf(stderr, "Excess command line arguments ignored\n");

	count = xdim*ydim;
	buf = bu_malloc(sizeof(*buf) * count, "buf");

	if (! terrain_func) {
		if (debug) bu_log("terrain func not specified\n");
		return 10;
	}

	terrain_func(buf);

	if (do_convert) {
	/* make sure the output is going as network unsigned shorts */

		in_cookie = bu_cv_cookie("hus");

		out_cookie = bu_cv_cookie("nus");

		if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie) ) {
			if (debug) bu_log("converting to network order\n");
			bu_cv_w_cookie(buf, out_cookie, count*sizeof(*buf),
				       buf, in_cookie, count);
		}
	}

	fwrite(buf, sizeof(*buf), count, stdout);
	return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
