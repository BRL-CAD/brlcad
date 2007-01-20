/*                     S S A M P V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file ssampview.c
 *
 *  Program to display spectral curves on the framebuffer.
 *  Uses a Tcl script to handle the GUI.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "spectrum.h"
#include "fb.h"
#include "tcl.h"
#include "tk.h"


extern struct bn_table *spectrum; /* from liboptical */

extern void
rt_spect_curve_to_xyz(
		      point_t			xyz,
		      const struct bn_tabdata	*tabp,
		      const struct bn_tabdata	*cie_x,
		      const struct bn_tabdata	*cie_y,
		      const struct bn_tabdata	*cie_z);

extern void
rt_spect_make_NTSC_RGB(struct bn_tabdata		**rp,
		       struct bn_tabdata		**gp,
		       struct bn_tabdata		**bp,
		       const struct bn_table		*tabp);


extern void
rt_make_ntsc_xyz2rgb( mat_t	xyz2rgb );


int	width = 64;				/* Linked with TCL */
int	height = 64;				/* Linked with TCL */
int	nwave = 2;				/* Linked with TCL */

char	*datafile_basename = "mtherm";
char	spectrum_name[100];

FBIO	*fbp;

struct bn_tabdata	*data;

struct bn_tabdata	*atmosphere_orig;
struct bn_tabdata	*atmosphere;
int			use_atmosphere = 0;	/* Linked with TCL */

struct bn_tabdata	*cie_x;
struct bn_tabdata	*cie_y;
struct bn_tabdata	*cie_z;
int			use_cie_xyz = 0;	/* Linked with TCL */
mat_t			xyz2rgb;
/* mat_t			rgb2xyz; */

struct bn_tabdata	*ntsc_r;
struct bn_tabdata	*ntsc_g;
struct bn_tabdata	*ntsc_b;

unsigned char	*pixels;		/* en-route to framebuffer */

fastf_t	maxval, minval;				/* Linked with TCL */

Tcl_Interp	*interp;
Tk_Window	tkwin;

int	doit(ClientData cd, Tcl_Interp *interp, int argc, char **argv), doit1(ClientData cd, Tcl_Interp *interp, int argc, char **argv);
void	find_minmax(void);
void	rescale(BU_ARGS(int wav));
void	show_color(BU_ARGS(int off));

char			*first_command = "no_command?";

/*
 *		A S S I G N _ T A B D A T A _ T O _ T C L _ V A R
 *
 *  Assign the given "C" bn_tabdata structure to the named Tcl variable,
 *  and add the name of that variable to the Tcl result string.
 */
void
assign_tabdata_to_tcl_var(Tcl_Interp *interp, const char *name, const struct bn_tabdata *tabp)
{
	struct bu_vls	str;

	BN_CK_TABDATA(tabp);

	bu_vls_init(&str);

	bn_tabdata_to_tcl(&str, tabp);
	Tcl_SetVar( interp, (char *)name, bu_vls_addr(&str), 0 );
	Tcl_AppendResult( interp, name, " ", (char *)NULL );

	bu_vls_free(&str);
}

/*
 *  Temporary testing function
 *  Takes no args, sets three Tcl variables, ntsc_r, ntsc_g, ntsc_b
 */
int
getntsccurves(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	extern struct bn_tabdata *rt_NTSC_r_tabdata;
	extern struct bn_tabdata *rt_NTSC_g_tabdata;
	extern struct bn_tabdata *rt_NTSC_b_tabdata;

	/* These are the curves as sampled to our spectrum intervals */
	assign_tabdata_to_tcl_var( interp, "ntsc_r_samp", ntsc_r );
	assign_tabdata_to_tcl_var( interp, "ntsc_g_samp", ntsc_g );
	assign_tabdata_to_tcl_var( interp, "ntsc_b_samp", ntsc_b );

#if 0
	/* These are the curves from the data tables in the library */
	assign_tabdata_to_tcl_var( interp, "ntsc_r_orig", rt_NTSC_r_tabdata );
	assign_tabdata_to_tcl_var( interp, "ntsc_g_orig", rt_NTSC_g_tabdata );
	assign_tabdata_to_tcl_var( interp, "ntsc_b_orig", rt_NTSC_b_tabdata );
#endif

	/* Sum togther the sampled curves */
	{
		struct bn_tabdata	*sum;
		BN_GET_TABDATA( sum, ntsc_r->table );
		bn_tabdata_add( sum, ntsc_r, ntsc_g );
		bn_tabdata_add( sum, sum, ntsc_b );
		assign_tabdata_to_tcl_var( interp, "ntsc_sum", sum );
		bn_tabdata_free( sum );
	}

#if 0
	/* Check out the RGB to spectrum curves */
	{
		struct bn_tabdata	*r, *g, *b, *sum;
		point_t		rgb;

		BN_GET_TABDATA( r, ntsc_r->table );
		BN_GET_TABDATA( g, ntsc_r->table );
		BN_GET_TABDATA( b, ntsc_r->table );
		BN_GET_TABDATA( sum, ntsc_r->table );

		VSET( rgb, 1, 0, 0 );
		rt_spect_reflectance_rgb( r, rgb );
		assign_tabdata_to_tcl_var( interp, "reflectance_r", r );

		VSET( rgb, 0, 1, 0 );
		rt_spect_reflectance_rgb( g, rgb );
		assign_tabdata_to_tcl_var( interp, "reflectance_g", g );

		VSET( rgb, 0, 0, 1 );
		rt_spect_reflectance_rgb( b, rgb );
		assign_tabdata_to_tcl_var( interp, "reflectance_b", b );

		bn_tabdata_add( sum, r, g );
		bn_tabdata_add( sum, sum, b );
		assign_tabdata_to_tcl_var( interp, "reflectance_sum", sum );

		bn_tabdata_free( r );
		bn_tabdata_free( g );
		bn_tabdata_free( b );
		bn_tabdata_free( sum );
	}
#endif

	/* Check out the black body curves */
	{
		struct bn_tabdata	*a, *b, *c;

		BN_GET_TABDATA( a, ntsc_r->table );
		BN_GET_TABDATA( b, ntsc_r->table );
		BN_GET_TABDATA( c, ntsc_r->table );

		rt_spect_black_body_fast( a, 5000.0 );
		assign_tabdata_to_tcl_var( interp, "a_5000", a );

		rt_spect_black_body_fast( b, 6500.0 );
		assign_tabdata_to_tcl_var( interp, "b_6500", b );

		rt_spect_black_body_fast( c, 10000.0 );
		assign_tabdata_to_tcl_var( interp, "c_10000", c );

		bn_tabdata_free( a );
		bn_tabdata_free( b );
		bn_tabdata_free( c );
	}

	return TCL_OK;
}

/*
 *
 *  With no args, returns the number of wavelengths.
 *  With an integer arg, returns the i-th wavelength.
 *
 *  spectrum pointer should be an arg, not implicit.
 */
int
getspectrum(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	int	wl;

	BN_CK_TABLE(spectrum);

	if( argc <= 1 )  {
		sprintf( interp->result, "%d", spectrum->nx );
		return TCL_OK;
	}
	if( argc != 2 )  {
		interp->result = "Usage: getspectrum [wl]";
		return TCL_ERROR;
	}
	wl = atoi(argv[2]);

	if( wl < 0 || wl > spectrum->nx )  {
		sprintf( interp->result, "getspectrum: wavelength %d out of range 0..%d",
			wl, spectrum->nx);
		return TCL_ERROR;
	}
	sprintf( interp->result, "%g", spectrum->x[wl] );
	return TCL_OK;
}

int
getspectval(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	struct bn_tabdata	*sp;
	int	x, y, wl;
	char	*cp;
	fastf_t	val;

	if( argc != 4 )  {
		interp->result = "Usage: getspect x y wl";
		return TCL_ERROR;
	}
	x = atoi(argv[1]);
	y = atoi(argv[2]);
	wl = atoi(argv[3]);

	BN_CK_TABLE(spectrum);

	if( x < 0 || x >= width )  {
		interp->result = "x out of range";
		return TCL_ERROR;
	}
	if( y < 0 || y >= height )  {
		interp->result = "y out of range";
		return TCL_ERROR;
	}
	if( wl < 0 || wl >= spectrum->nx )  {
		interp->result = "wavelength index out of range";
		return TCL_ERROR;
	}

	if( !data )  {
		interp->result = "pixel data table not loaded yet";
		return TCL_ERROR;
	}

	cp = (char *)data;
	cp = cp + (y * width + x) * BN_SIZEOF_TABDATA(spectrum);
	sp = (struct bn_tabdata *)cp;
	BN_CK_TABDATA(sp);
	val = sp->y[wl];
	if( use_atmosphere )
		val *= atmosphere->y[wl];
	sprintf( interp->result, "%g", val );
	return TCL_OK;
}

/*
 *			G E T S P E C T X Y
 *
 *  Given the x,y coordinates of a pixel in the multi-spectral image,
 *  return the spectral data found there in Tcl string form.
 */
int
getspectxy(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	struct bn_tabdata	*sp;
	int	x, y;
	char	*cp;
	struct bu_vls	str;

	if( argc != 3 )  {
		interp->result = "Usage: getspectxy x y";
		return TCL_ERROR;
	}
	x = atoi(argv[1]);
	y = atoi(argv[2]);

	BN_CK_TABLE(spectrum);

	if( x < 0 || x >= width || y < 0 || y >= height )  {
		interp->result = "x or y out of range";
		return TCL_ERROR;
	}

	if( !data )  {
		interp->result = "pixel data table not loaded yet";
		return TCL_ERROR;
	}
	cp = (char *)data;
	cp = cp + (y * width + x) * BN_SIZEOF_TABDATA(spectrum);
	sp = (struct bn_tabdata *)cp;
	BN_CK_TABDATA(sp);

	bu_vls_init(&str);
	bn_tabdata_to_tcl( &str, sp );
	Tcl_SetResult( interp, bu_vls_addr(&str), TCL_VOLATILE);
	bu_vls_free(&str);

	return TCL_OK;
}

/*
 *  TCL interface to LIBFB.
 *  Points at lower left corner of selected pixel.
 */
int
tcl_fb_cursor(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	FBIO	*ifp;
	long	mode, x, y;

	if( argc != 5 )  {
		interp->result = "Usage: fb_cursor fbp mode x y";
		return TCL_ERROR;
	}
	ifp = (FBIO *)atol(argv[1]);
	mode = atol(argv[2]);
	x = atol(argv[3]);
	y = atol(argv[4]);

	ifp = fbp;	/* XXX hack, ignore tcl arg. */

	FB_CK_FBIO(ifp);
	if( fb_cursor( ifp, mode, x, y ) < 0 )  {
		interp->result = "fb_cursor got error from library";
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
 *  Return value of one pixel as RGB tripple, in decimal
 */
int
tcl_fb_readpixel(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	FBIO	*ifp;
	long	mode, x, y;
	unsigned char	pixel[4];

	if( argc != 4 )  {
		interp->result = "Usage: fb_readpixel fbp x y";
		return TCL_ERROR;
	}
	ifp = (FBIO *)atol(argv[1]);
	x = atol(argv[2]);
	y = atol(argv[3]);

	ifp = fbp;	/* XXX hack, ignore tcl arg. */

	FB_CK_FBIO(ifp);
	if( fb_read( ifp, x, y, pixel, 1 ) < 0 )  {
		interp->result = "fb_readpixel got error from library";
		return TCL_ERROR;
	}
	sprintf(interp->result, "%d %d %d", pixel[RED], pixel[GRN], pixel[BLU] );
	return TCL_OK;
}

int
tcl_appinit(Tcl_Interp *inter)
{
	interp = inter;	/* set global var */
	if( Tcl_Init(interp) == TCL_ERROR )  {
		return TCL_ERROR;
	}

	/* Run tk.tcl script */
	if( Tk_Init(interp) == TCL_ERROR )  return TCL_ERROR;

	/* Add commands offered by the libraries */
	bu_tcl_setup(interp);
	rt_tcl_setup(interp);

	/* Add commands offered by this program */
	Tcl_CreateCommand(interp, "fb_cursor", (Tcl_CmdProc *)tcl_fb_cursor, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "fb_readpixel", (Tcl_CmdProc *)tcl_fb_readpixel, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateCommand(interp, "doit", (Tcl_CmdProc *)doit, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "doit1", (Tcl_CmdProc *)doit1, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateCommand(interp, "getspectval", (Tcl_CmdProc *)getspectval, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "getspectrum", (Tcl_CmdProc *)getspectrum, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "getspectxy", (Tcl_CmdProc *)getspectxy, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateCommand(interp, "getntsccurves", (Tcl_CmdProc *)getntsccurves, (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_LinkVar( interp, "minval", (char *)&minval, TCL_LINK_DOUBLE );
	Tcl_LinkVar( interp, "maxval", (char *)&maxval, TCL_LINK_DOUBLE );

	Tcl_LinkVar( interp, "width", (char *)&width, TCL_LINK_INT );
	Tcl_LinkVar( interp, "height", (char *)&height, TCL_LINK_INT );
	Tcl_LinkVar( interp, "nwave", (char *)&nwave, TCL_LINK_INT );
	Tcl_LinkVar( interp, "use_atmosphere", (char *)&use_atmosphere, TCL_LINK_INT );
	Tcl_LinkVar( interp, "use_cie_xyz", (char *)&use_cie_xyz, TCL_LINK_INT );

	/* Tell Tcl script what to do first */
	Tcl_SetVar( interp, "first_command", first_command, 0 );

	/* Specify startup file to invoke when run interactively */
	/* Source the TCL part of this lashup */
	/* Tcl7 way:  tcl_RcFileName = "./ssampview.tcl"; */
	Tcl_SetVar(interp, "tcl_rcFileName", "~/brlcad/rttherm/ssampview.tcl", TCL_GLOBAL_ONLY);

	return TCL_OK;
}

void check( double x, double y, double z);

/* Check identity of XYZ->RGB->spectrum->XYZ->RGB */
void
check(double x, double y, double z)
{
	point_t	xyz;
	point_t	rgb;
	point_t	xyz2, rgb2;
	struct bn_tabdata	*tabp;
	FAST fastf_t	tab_area;

	BN_GET_TABDATA( tabp, spectrum );
	xyz[X] = x;
	xyz[Y] = y;
	xyz[Z] = z;
	VPRINT( "\nstarting xyz", xyz );

#if 0
	/* XXX No way to do this yet!! */
	rt_spect_xyz_to_curve( tabp, xyz, cie_x, cie_y, cie_z );
#else
	MAT3X3VEC( rgb, xyz2rgb, xyz );
	VPRINT( "rgb", rgb );
	{
	    float rrggbb[3];
	    VMOVE( rrggbb, rgb);

	    rt_spect_reflectance_rgb( tabp, rrggbb );
	}
#endif
	bn_print_table_and_tabdata( "/dev/tty", tabp );
	tab_area = bn_tabdata_area2( tabp );
	bu_log(" tab_area = %g\n", tab_area);

	rt_spect_curve_to_xyz( xyz2, tabp, cie_x, cie_y, cie_z );

	VPRINT( "xyz2", xyz2 );
	MAT3X3VEC( rgb2, xyz2rgb, xyz2 );
	VPRINT( "rgb2", rgb2 );

	bn_tabdata_free( tabp );
exit(2);
}

void
conduct_tests(void)
{
	struct bn_tabdata	*flat;
	vect_t			xyz;

	/* Code for testing library routines */
	spectrum = bn_table_make_uniform( 20, 340.0, 760.0 );
	rt_spect_make_CIE_XYZ( &cie_x, &cie_y, &cie_z, spectrum );
bu_log("X:\n");bn_print_table_and_tabdata( "/dev/tty", cie_x );
bu_log("Y:\n");bn_print_table_and_tabdata( "/dev/tty", cie_y );
bu_log("Z:\n");bn_print_table_and_tabdata( "/dev/tty", cie_z );

	rt_spect_make_NTSC_RGB( &ntsc_r, &ntsc_g, &ntsc_b, spectrum );
bu_log("R:\n");bn_print_table_and_tabdata( "/dev/tty", ntsc_r );
bu_log("G:\n");bn_print_table_and_tabdata( "/dev/tty", ntsc_g );
bu_log("B:\n");bn_print_table_and_tabdata( "/dev/tty", ntsc_b );
	{
		struct bu_vls str;
		bu_vls_init(&str);
		bn_tabdata_to_tcl( &str, ntsc_r );
		bu_log("ntsc_r tcl:  %s\n", bu_vls_addr(&str) );
		bu_vls_free(&str);
	}

/* "A flat spectral curve is represente by equal XYZ values".  Hall pg 52 */
	flat = bn_tabdata_get_constval( 42.0, spectrum );
	bu_log("flat:\n");bn_print_table_and_tabdata( "/dev/tty", flat );
	rt_spect_curve_to_xyz(xyz, flat, cie_x, cie_y, cie_z );
	VPRINT("flat xyz?", xyz);

return;

	/* Check identity of XYZ->RGB->spectrum->XYZ->RGB */
	check( 0.313,     0.329,      0.358);	/* D6500 white */
	check( 0.670,     0.330,      0.000);	/* NTSC red primary */
	check( 0.210,     0.710,      0.080);	/* NTSC green primary */
	check( 0.140,     0.080,      0.780);	/* NTSC blue primary */
	check( .5, .5, .5 );
	check( 1, 0, 0 );
	check( 0, 1, 0 );
	check( 0, 0, 1 );
	check( 1, 1, 1 );
	check( 1, 1, 0 );
	check( 1, 0, 1 );
	check( 0, 1, 1 );
}

static char usage[] = "\
Usage: ssampview [-t] [-s squarefilesize] [-w file_width] [-n file_height]\n\
		file.ssamp\n";


int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "ts:w:n:" )) != EOF )  {
		switch( c )  {
		case 't':
			fprintf(stderr, "ssampview: conducting library tests\n");
			conduct_tests();
			first_command = "do_testing";
			Tk_Main( 1, argv, tcl_appinit );
			/* NOTREACHED */
			exit(0);
			/* NOTREACHED */
			break;
		case 's':
			/* square file size */
			height = width = atoi(optarg);
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'n':
			height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  return 0;
	return 1;	/* OK */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{

	bu_debug = BU_DEBUG_COREDUMP;
	rt_g.debug = 1;

	rt_make_ntsc_xyz2rgb( xyz2rgb );

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( argc > 1 && strcmp(argv[1], "-t") == 0 )  {
	}

	datafile_basename = argv[optind];

	first_command = "doit1 42";

	if( (fbp = fb_open( NULL, width, height )) == FBIO_NULL )  {
		rt_bomb("Unable to open fb\n");
	}
	fb_view( fbp, width/2, height/2, fb_getwidth(fbp)/width, fb_getheight(fbp)/height );

	/* Read spectrum definition */
	sprintf( spectrum_name, "%s.spect", datafile_basename );
	spectrum = (struct bn_table *)bn_table_read( spectrum_name );
	if( spectrum == NULL )  {
		rt_bomb("Unable to read spectrum\n");
	}
	BN_CK_TABLE(spectrum);
	bu_log("spectrum has %d samples\n", spectrum->nx);
	nwave = spectrum->nx;	/* shared with Tcl */

	/* Read atmosphere curve -- input is in microns, not nm */
	atmosphere_orig = bn_read_table_and_tabdata( "../rttherm/std_day_1km.dat" );
	bn_table_scale( (struct bn_table *)(atmosphere_orig->table), 1000.0 );
	atmosphere = bn_tabdata_resample_max( spectrum, atmosphere_orig );

	/* Allocate and read 2-D spectrum array */
	data = bn_tabdata_binary_read( datafile_basename, width*height, spectrum );
	if( !data )  bu_bomb("bn_tabdata_binary_read() of datafile_basename failed\n");

	/* Allocate framebuffer image buffer */
	pixels = (unsigned char *)bu_malloc( width * height * 3, "pixels[]" );

	find_minmax();
	rt_log("min = %g, max=%g Watts\n", minval, maxval );

	Tk_Main( 1, argv, tcl_appinit );
	/* NOTREACHED */

	return 0;
}

int
doit(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	int	wl;
	char	cmd[96];

	for( wl = 0; wl < spectrum->nx; wl++ )  {
		sprintf( cmd, "doit1 %d", wl );
		Tcl_Eval( interp, cmd );
	}
	return TCL_OK;
}

int
doit1(ClientData cd, Tcl_Interp *interp, int argc, char **argv)
{
	int	wl;
	char	buf[32];
	int	got;

	if( argc != 2 )  {
		interp->result = "Usage: doit1 wavel#";
		return TCL_ERROR;
	}
	wl = atoi(argv[1]);
	if( wl < 0 || wl >= spectrum->nx )  {
		interp->result = "Wavelength number out of range";
		return TCL_ERROR;
	}

	if( !data )  {
		interp->result = "pixel data table not loaded yet";
		return TCL_ERROR;
	}

	rt_log("doit1 %d: %g um to %g um\n",
		wl,
		spectrum->x[wl] * 0.001,
		spectrum->x[wl+1] * 0.001 );
	if( use_cie_xyz )
		show_color(wl);
	else
		rescale(wl);
	(void)fb_writerect( fbp, 0, 0, width, height, pixels );
	fb_poll(fbp);

	/* export C variables to TCL, one-way */
	/* These are being traced by Tk, this will cause update */
	sprintf(buf, "%d", wl);
	Tcl_SetVar(interp, "x", buf, TCL_GLOBAL_ONLY);
	sprintf(buf, "%g", spectrum->x[wl] * 0.001);
	Tcl_SetVar(interp, "lambda", buf, TCL_GLOBAL_ONLY);

	return TCL_OK;
}

/*
 */
void
find_minmax(void)
{
	char			*cp;
	int			todo;
	register fastf_t	max, min;
	int		nbytes;
	int		j;

	cp = (char *)data;
	nbytes = BN_SIZEOF_TABDATA(spectrum);

	max = -INFINITY;
	min =  INFINITY;

	for( todo = width * height; todo > 0; todo--, cp += nbytes )  {
		struct bn_tabdata	*sp;
		sp = (struct bn_tabdata *)cp;
		BN_CK_TABDATA(sp);
		for( j = 0; j < spectrum->nx; j++ )  {
			register fastf_t	v;

			if( (v = sp->y[j]) > max )  max = v;
			if( v < min )  min = v;
		}
	}
	maxval = max;
	minval = min;
}

/*
 *			R E S C A L E
 *
 *  Create monochrome image from the spectral data, at wavelength 'wav',
 *  given current min & max values.
 */
void
rescale(int wav)
{
	char		*cp;
	unsigned char	*pp;
	int		todo;
	int		nbytes;
	fastf_t		scale;
	fastf_t		atmos_scale;

	cp = (char *)data;
	nbytes = BN_SIZEOF_TABDATA(spectrum);

	pp = pixels;

	scale = 255 / (maxval - minval);

	if( use_atmosphere )
		atmos_scale = atmosphere->y[wav];
	else
		atmos_scale = 1;

	for( todo = width * height; todo > 0; todo--, cp += nbytes, pp += 3 )  {
		struct bn_tabdata	*sp;
		register int		val;

		sp = (struct bn_tabdata *)cp;
		BN_CK_TABDATA(sp);

		val = (sp->y[wav] * atmos_scale - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[0] = pp[1] = pp[2] = val;
	}
}


/*
 *			S H O W _ C O L O R
 *
 *  Create color image from spectral curve,
 *  given current min & max values, and frequency offset (in nm).
 *  Go via CIE XYZ space.
 */
void
show_color(int off)
{
	char		*cp;
	unsigned char	*pp;
	int		todo;
	int		nbytes;
	fastf_t		scale;
	struct bn_tabdata *new;

	cp = (char *)data;
	nbytes = BN_SIZEOF_TABDATA(spectrum);

	pp = pixels;

	scale = 255 / (maxval - minval);

	/* Build CIE curves */
	if( cie_x->magic == 0 )
		rt_spect_make_CIE_XYZ( &cie_x, &cie_y, &cie_z, spectrum );

	BN_GET_TABDATA(new, spectrum);

	for( todo = width * height; todo > 0; todo--, cp += nbytes, pp += 3 )  {
		struct bn_tabdata	*sp;
		point_t			xyz;
		point_t			rgb;
		register int		val;

		sp = (struct bn_tabdata *)cp;
		BN_CK_TABDATA(sp);

		if( use_atmosphere )  {
			bn_tabdata_mul( new, sp, atmosphere );
			bn_tabdata_freq_shift( new, new, spectrum->x[off] - 380.0 );
		} else {
			bn_tabdata_freq_shift( new, sp, spectrum->x[off] - 380.0 );
		}

#if 0
		if( todo == (width/2)*(height/2) )  {
			struct bu_vls str;
			bu_vls_init(&str);

			bu_vls_printf(&str, "popup_plot_tabdata centerpoint {");
			bn_tabdata_to_tcl(&str, sp);
			bu_vls_printf(&str, "}" );
			Tcl_Eval( interp, bu_vls_addr(&str) );

			bu_vls_trunc(&str,0);
			bu_vls_printf(&str, "popup_plot_tabdata centerpoint_shifted {");
			bn_tabdata_to_tcl(&str, new);
			bu_vls_printf(&str, "}" );
			Tcl_Eval( interp, bu_vls_addr(&str) );
			bu_vls_free(&str);
		}
#endif

		rt_spect_curve_to_xyz( xyz, new, cie_x, cie_y, cie_z );

		MAT3X3VEC( rgb, xyz2rgb, xyz );

		val = (rgb[RED] - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[RED] = val;

		val = (rgb[GRN] - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[GRN] = val;

		val = (rgb[BLU] - minval) * scale;
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		pp[BLU] = val;
	}

	bn_tabdata_free( new );
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
