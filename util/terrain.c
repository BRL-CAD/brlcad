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
 */
#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

/* declarations to support use of getopt() system call */
char *options = "w:n:s:L:H:O:S:V:D:f:c";
extern char *optarg;
extern int optind, opterr, getopt();

int do_convert = 1;
char *progname = "(noname)";
unsigned xdim = 256;
unsigned ydim = 256;

double fbm_lacunarity = 2.1753974;		/* noise_lacunarity */
double fbm_h = 1.0;
double fbm_octaves = 4.0;
double fbm_size = 1.0;
vect_t fbm_vscale = {0.0125, 0.0125, 0.0125};
vect_t fbm_delta = {1000.0, 1000.0, 1000.0};

#define FUNC_FBM 0
#define FUNC_TURB 1
#define FUNC_TURB_UP 2
int func = FUNC_FBM;

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
usage(s)
char *s;
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [ flags ] > outfile]\nFlags:\n%s\n",
		       progname, 
"\t-w #		number of postings in X direction\n\
\t-n #		number of postings in Y direction\n\
\t-s #		number of postings in X,Y direction\n\
\t-L #		Noise Lacunarity\n\
\t-H #		Noise H value\n\
\t-O #		Noise Octaves\n\
\t-S #		Noise Scale\n\
\t-V #,#,#	Noise Vector scale (affine scale)\n\
\t-D #,#,#	Noise Delta\n\
\t-f func		noise function (fbm=\"f\" turb=\"t\" 1.0-turb=\"T\")");

	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int
parse_args(ac, av)
int ac;
char *av[];
{
	int  c;
	char *strrchr();
	double v;

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'c': do_convert = !do_convert; break;
		case 'w': if ((c=atoi(optarg)) > 0) xdim = c;
			break;
		case 'n': if ((c=atoi(optarg)) > 0) ydim = c;
			break;
		case 's': if ((c=atoi(optarg)) > 0) xdim = ydim = c;
			break;
		case 'L': if ((v=atof(optarg)) >  0.0) fbm_lacunarity = v;
			break;
		case 'H': if ((v=atof(optarg)) >  0.0) fbm_h = v;
			break;
		case 'O': if ((v=atof(optarg)) >  0.0) fbm_octaves = v;
			break;

		case 'S': if ((v=atof(optarg)) >  0.0) { VSETALL(fbm_vscale, v); }
			break;

		case 'V': sscanf(optarg, "%lg,%lg,%lg",
			       &fbm_vscale[0], &fbm_vscale[1], &fbm_vscale[2]);
			break;
		case 'D': sscanf(optarg, "%lg,%lg,%lg",
			       &fbm_delta[0], &fbm_delta[1], &fbm_delta[2]);
			break;
		case 'f':
			switch (*optarg) {
			case 'f': func = FUNC_FBM;
				break;
			case 't': func = FUNC_TURB;
				break;
			case 'T': func = FUNC_TURB_UP;
				break;
			default:
				fprintf(stderr, 
					"Unknown noise function: \"%s\"\n",
					*optarg);
				exit(-1);
				break;
			}
			break;
		case '?'	:
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}

	return(optind);
}

/***********************************************************************
 *
 *	func_fbm
 *
 *	Fractional Brownian motion noise
 */
func_fbm(unsigned short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
			pt[X] = x;

			xform(t, pt);
			v = bn_noise_fbm(t, fbm_h,fbm_lacunarity, fbm_octaves);
			if (v > 1.0 || v < -1.0)
				bu_log("clamping noise value %g \n", v);
			v = v * 0.5 + 0.5;
			CLAMP(v, 0.0, 1.0);
			buf[y*xdim + x] = 1.0 + 65535.0 * v;
		}
	}
}
/***********************************************************************
 *
 *	func_turb
 *
 *	Turbulence noise
 */
func_turb(unsigned short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;

	pt[Z] = 0.0;
	for (y=0 ; y < ydim ; y++) {
		pt[Y] = y;
		for (x=0  ; x < xdim ; x++) {
			pt[X] = x;

			xform(t, pt);

			v = bn_noise_turb(t, fbm_h, 
					  fbm_lacunarity, fbm_octaves);

			if (v > 1.0 || v < 0.0)
				bu_log("clamping noise value %g \n", v);
			CLAMP(v, 0.0, 1.0);
			buf[y*xdim + x] = 1.0 + 65535.0 * v;
		}
	}
}

/***********************************************************************
 *
 *	func_turb_up
 *
 *	Upside-down turbulence noise
 */
func_turb_up(short *buf)
{
	point_t pt;
	int x, y;
	vect_t t;
	double v;

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
				bu_log("clamping noise value %g \n", v);
			buf[y*xdim + x] = 1 + 65535.0 * v;
		}
	}
}

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments, then
 *	produce the noise field selected.  Write out binary in network order.
 */
int
main(ac,av)
int ac;
char *av[];
{
	int arg_count;
	FILE *inp;
	int status;
	int x, y;
	unsigned short *buf;
	int in_cookie, out_cookie;
	int count;
	 

	arg_count = parse_args(ac, av);
	
	if (arg_count+1 < ac) usage("Excess arguments on cmd line\n");

	if (isatty(fileno(stdout))) usage("Redirect standard output\n");

	if (arg_count < ac)
		fprintf(stderr, "Excess command line arguments ignored\n");

	count = xdim*ydim;
	buf = malloc(sizeof(*buf) * count);
	if (! buf) {
		fprintf(stderr, "malloc error\n");
		exit(-1);
	}

	switch (func) {
	case FUNC_FBM:
		func_fbm(buf);
		break;
	case FUNC_TURB:
		func_turb(buf);
		break;
	case FUNC_TURB_UP:
		func_turb_up(buf);
		break;
	default:
		fprintf(stderr, "bad function?\n");
		break;
	}

	if (do_convert) {
	/* make sure the output is going as network unsigned shorts */
		in_cookie = bu_cv_cookie("hus");
		out_cookie = bu_cv_cookie("nus");
		if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie) ) {
			bu_log("converting\n");
			bu_cv_w_cookie(buf, out_cookie, count*sizeof(*buf), 
				       buf, in_cookie, count);
		}
	}
	fwrite(buf, sizeof(*buf), count, stdout);
	return 0;
}

