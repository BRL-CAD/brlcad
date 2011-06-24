/*                     S S A M P V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file rttherm/ssampview.c
 *
 * Program to display spectral curves on the framebuffer.
 * Uses a Tcl script to handle the GUI.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

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
spect_curve_to_xyz(point_t xyz,
		   const struct bn_tabdata *tabp,
		   const struct bn_tabdata *cie_x,
		   const struct bn_tabdata *cie_y,
		   const struct bn_tabdata *cie_z);

extern void
spect_make_NTSC_RGB(struct bn_tabdata **rp,
		    struct bn_tabdata **gp,
		    struct bn_tabdata **bp,
		    const struct bn_table *tabp);


extern void
make_ntsc_xyz2rgb(mat_t xyz2rgb);


int width = 64;				/* Linked with TCL */
int height = 64;				/* Linked with TCL */
int nwave = 2;				/* Linked with TCL */

char *datafile_basename = "mtherm";
char spectrum_name[100];

FBIO *fbp;

struct bn_tabdata *data;

struct bn_tabdata *atmosphere_orig;
struct bn_tabdata *atmosphere;
int use_atmosphere = 0;	/* Linked with TCL */

struct bn_tabdata *cie_x;
struct bn_tabdata *cie_y;
struct bn_tabdata *cie_z;
int use_cie_xyz = 0;	/* Linked with TCL */
mat_t xyz2rgb;
/* mat_t rgb2xyz; */

struct bn_tabdata *ntsc_r;
struct bn_tabdata *ntsc_g;
struct bn_tabdata *ntsc_b;

unsigned char *pixels;		/* en-route to framebuffer */

fastf_t maxval, minval;				/* Linked with TCL */

Tk_Window tkwin;

int doit(ClientData cd, Tcl_Interp *interp, int argc, char **argv), doit1(ClientData cd, Tcl_Interp *interp, int argc, char **argv);
void find_minmax(void);
void rescale(int wav);
void show_color(int off);

char *first_command = "no_command?";

/*
 * A S S I G N _ T A B D A T A _ T O _ T C L _ V A R
 *
 * Assign the given "C" bn_tabdata structure to the named Tcl variable,
 * and add the name of that variable to the Tcl result string.
 */
void
assign_tabdata_to_tcl_var(Tcl_Interp *interp, const char *name, const struct bn_tabdata *tabp)
{
    struct bu_vls str;

    BN_CK_TABDATA(tabp);

    bu_vls_init(&str);

    bn_tabdata_to_tcl(&str, tabp);
    Tcl_SetVar(interp, (char *)name, bu_vls_addr(&str), 0);
    Tcl_AppendResult(interp, name, " ", (char *)NULL);

    bu_vls_free(&str);
}


/*
 * Temporary testing function
 * Takes no args, sets three Tcl variables, ntsc_r, ntsc_g, ntsc_b
 */
int
getntsccurves(ClientData UNUSED(cd), Tcl_Interp *interp, int UNUSED(argc), char **UNUSED(argv))
{
    /* These are the curves as sampled to our spectrum intervals */
    assign_tabdata_to_tcl_var(interp, "ntsc_r_samp", ntsc_r);
    assign_tabdata_to_tcl_var(interp, "ntsc_g_samp", ntsc_g);
    assign_tabdata_to_tcl_var(interp, "ntsc_b_samp", ntsc_b);

    /* Sum togther the sampled curves */
    {
	struct bn_tabdata *sum;
	BN_GET_TABDATA(sum, ntsc_r->table);
	bn_tabdata_add(sum, ntsc_r, ntsc_g);
	bn_tabdata_add(sum, sum, ntsc_b);
	assign_tabdata_to_tcl_var(interp, "ntsc_sum", sum);
	bn_tabdata_free(sum);
    }

    /* Check out the black body curves */
    {
	struct bn_tabdata *a, *b, *c;

	BN_GET_TABDATA(a, ntsc_r->table);
	BN_GET_TABDATA(b, ntsc_r->table);
	BN_GET_TABDATA(c, ntsc_r->table);

	rt_spect_black_body_fast(a, 5000.0);
	assign_tabdata_to_tcl_var(interp, "a_5000", a);

	rt_spect_black_body_fast(b, 6500.0);
	assign_tabdata_to_tcl_var(interp, "b_6500", b);

	rt_spect_black_body_fast(c, 10000.0);
	assign_tabdata_to_tcl_var(interp, "c_10000", c);

	bn_tabdata_free(a);
	bn_tabdata_free(b);
	bn_tabdata_free(c);
    }

    return TCL_OK;
}


/*
 *
 * With no args, returns the number of wavelengths.
 * With an integer arg, returns the i-th wavelength.
 *
 * spectrum pointer should be an arg, not implicit.
 */
int
getspectrum(ClientData UNUSED(cd), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls vls;
    size_t wl;

    BN_CK_TABLE(spectrum);

    bu_vls_init(&vls);
    Tcl_ResetResult(interp);

    if (argc <= 1) {
	bu_vls_printf(&vls, "%zu", spectrum->nx);
	Tcl_SetResult(interp, bu_vls_addr(&vls), TCL_VOLATILE);
	bu_vls_free(&vls);
	return TCL_OK;
    }
    if (argc != 2) {
	Tcl_AppendResult(interp, "Usage: getspectrum [wl]", (char *)NULL);
	return TCL_ERROR;
    }
    wl = atoi(argv[2]);

    if (wl > spectrum->nx) {
	bu_vls_printf(&vls, "getspectrum: wavelength %zu out of range 0..%zu", wl, spectrum->nx);
	Tcl_SetResult(interp, bu_vls_addr(&vls), TCL_VOLATILE);
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    bu_vls_printf(&vls, "%g", spectrum->x[wl]);
    Tcl_SetResult(interp, bu_vls_addr(&vls), TCL_VOLATILE);
    bu_vls_free(&vls);
    return TCL_OK;
}


int
getspectval(ClientData UNUSED(cd), Tcl_Interp *interp, int argc, char **argv)
{
    struct bn_tabdata *sp;
    int x, y;
    long lval;
    size_t wl;
    char *cp;
    fastf_t val;
    struct bu_vls vls;

    Tcl_ResetResult(interp);

    if (argc != 4) {
	Tcl_AppendResult(interp, "Usage: getspect x y wl", (char *)NULL);
	return TCL_ERROR;
    }
    x = atoi(argv[1]);
    y = atoi(argv[2]);
    lval = atol(argv[3]);

    BN_CK_TABLE(spectrum);

    if (x < 0 || x >= width) {
	Tcl_AppendResult(interp, "x out of range", (char *)NULL);
	return TCL_ERROR;
    }
    if (y < 0 || y >= height) {
	Tcl_AppendResult(interp, "y out of range", (char *)NULL);
	return TCL_ERROR;
    }

    wl = (size_t)lval;
    if (lval < 0 || wl >= spectrum->nx) {
	Tcl_AppendResult(interp, "wavelength index out of range", (char *)NULL);
	return TCL_ERROR;
    }

    if (!data) {
	Tcl_AppendResult(interp, "pixel data table not loaded yet", (char *)NULL);
	return TCL_ERROR;
    }

    cp = (char *)data;
    cp = cp + (y * width + x) * BN_SIZEOF_TABDATA(spectrum);
    sp = (struct bn_tabdata *)cp;
    BN_CK_TABDATA(sp);
    val = sp->y[wl];
    if (use_atmosphere)
	val *= atmosphere->y[wl];

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%g", val);
    Tcl_SetResult(interp, bu_vls_addr(&vls), TCL_VOLATILE);
    bu_vls_free(&vls);
    return TCL_OK;
}


/*
 * G E T S P E C T X Y
 *
 * Given the x, y coordinates of a pixel in the multi-spectral image,
 * return the spectral data found there in Tcl string form.
 */
int
getspectxy(ClientData UNUSED(cd), Tcl_Interp *interp, int argc, char **argv)
{
    struct bn_tabdata *sp;
    int x, y;
    char *cp;
    struct bu_vls str;

    Tcl_ResetResult(interp);

    if (argc != 3) {
	Tcl_AppendResult(interp, "Usage: getspectxy x y", (char *)NULL);
	return TCL_ERROR;
    }
    x = atoi(argv[1]);
    y = atoi(argv[2]);

    BN_CK_TABLE(spectrum);

    if (x < 0 || x >= width || y < 0 || y >= height) {
	Tcl_AppendResult(interp, "x or y out of range", (char *)NULL);
	return TCL_ERROR;
    }

    if (!data) {
	Tcl_AppendResult(interp, "pixel data table not loaded yet", (char *)NULL);
	return TCL_ERROR;
    }
    cp = (char *)data;
    cp = cp + (y * width + x) * BN_SIZEOF_TABDATA(spectrum);
    sp = (struct bn_tabdata *)cp;
    BN_CK_TABDATA(sp);

    bu_vls_init(&str);
    bn_tabdata_to_tcl(&str, sp);
    Tcl_SetResult(interp, bu_vls_addr(&str), TCL_VOLATILE);
    bu_vls_free(&str);

    return TCL_OK;
}


/*
 * TCL interface to LIBFB.
 * Points at lower left corner of selected pixel.
 */
int
tcl_fb_cursor(ClientData UNUSED(cd), Tcl_Interp *interp, int argc, char **argv)
{
    FBIO *ifp;
    long mode, x, y;

    Tcl_ResetResult(interp);

    if (argc != 5) {
	Tcl_AppendResult(interp, "Usage: fb_cursor fbp mode x y", (char *)NULL);
	return TCL_ERROR;
    }
    ifp = (FBIO *)atol(argv[1]);
    mode = atol(argv[2]);
    x = atol(argv[3]);
    y = atol(argv[4]);

    ifp = fbp;	/* XXX hack, ignore tcl arg. */

    FB_CK_FBIO(ifp);
    if (fb_cursor(ifp, mode, x, y) < 0) {
	Tcl_AppendResult(interp, "fb_cursor got error from library", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 * Return value of one pixel as RGB tripple, in decimal
 */
int
tcl_fb_readpixel(ClientData UNUSED(cd), Tcl_Interp *interp, int argc, char **argv)
{
    FBIO *ifp;
    long x, y;
    unsigned char pixel[4];
    struct bu_vls vls;

    Tcl_ResetResult(interp);

    if (argc != 4) {
	Tcl_AppendResult(interp, "Usage: fb_readpixel fbp x y", (char *)NULL);
	return TCL_ERROR;
    }
    ifp = (FBIO *)atol(argv[1]);
    x = atol(argv[2]);
    y = atol(argv[3]);

    ifp = fbp;	/* XXX hack, ignore tcl arg. */

    FB_CK_FBIO(ifp);
    if (fb_read(ifp, x, y, pixel, 1) < 0) {
	Tcl_AppendResult(interp, "fb_readpixel got error from library", (char *)NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d %d %d", pixel[RED], pixel[GRN], pixel[BLU]);
    Tcl_SetResult(interp, bu_vls_addr(&vls), TCL_VOLATILE);
    bu_vls_free(&vls);
    return TCL_OK;
}


int
tcl_appinit(Tcl_Interp *interp)
{
    const char *ssampview_tcl = NULL;

    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /* Run tk.tcl script */
    if (Tk_Init(interp) == TCL_ERROR) return TCL_ERROR;

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

    Tcl_LinkVar(interp, "minval", (char *)&minval, TCL_LINK_DOUBLE);
    Tcl_LinkVar(interp, "maxval", (char *)&maxval, TCL_LINK_DOUBLE);

    Tcl_LinkVar(interp, "width", (char *)&width, TCL_LINK_INT);
    Tcl_LinkVar(interp, "height", (char *)&height, TCL_LINK_INT);
    Tcl_LinkVar(interp, "nwave", (char *)&nwave, TCL_LINK_INT);
    Tcl_LinkVar(interp, "use_atmosphere", (char *)&use_atmosphere, TCL_LINK_INT);
    Tcl_LinkVar(interp, "use_cie_xyz", (char *)&use_cie_xyz, TCL_LINK_INT);

    /* Tell Tcl script what to do first */
    Tcl_SetVar(interp, "first_command", first_command, 0);

    /* Specify startup file to invoke when run interactively */
    /* Source the TCL part of this lashup */
    ssampview_tcl = bu_brlcad_root("bin/ssampview.tcl", 0);
    if (ssampview_tcl)
	Tcl_SetVar(interp, "tcl_rcFileName", ssampview_tcl, TCL_GLOBAL_ONLY);
    else
	Tcl_SetVar(interp, "tcl_rcFileName", "./ssampview.tcl", TCL_GLOBAL_ONLY);

    return TCL_OK;
}


/* Check identity of XYZ->RGB->spectrum->XYZ->RGB */
void
check(double x, double y, double z)
{
    point_t xyz;
    point_t rgb;
    point_t xyz2, rgb2;
    struct bn_tabdata *tabp;
    fastf_t tab_area;

    BN_GET_TABDATA(tabp, spectrum);
    xyz[X] = x;
    xyz[Y] = y;
    xyz[Z] = z;
    VPRINT("\nstarting xyz", xyz);

    MAT3X3VEC(rgb, xyz2rgb, xyz);
    VPRINT("rgb", rgb);
    {
	float rrggbb[3];
	VMOVE(rrggbb, rgb);

	rt_spect_reflectance_rgb(tabp, rrggbb);
    }

    bn_print_table_and_tabdata("/dev/tty", tabp);
    tab_area = bn_tabdata_area2(tabp);
    bu_log(" tab_area = %g\n", tab_area);

    spect_curve_to_xyz(xyz2, tabp, cie_x, cie_y, cie_z);

    VPRINT("xyz2", xyz2);
    MAT3X3VEC(rgb2, xyz2rgb, xyz2);
    VPRINT("rgb2", rgb2);

    bn_tabdata_free(tabp);
    bu_exit(2, NULL);
}


void
conduct_tests(void)
{
    struct bn_tabdata *flat;
    vect_t xyz;

    /* Code for testing library routines */
    spectrum = bn_table_make_uniform(20, 340.0, 760.0);
    rt_spect_make_CIE_XYZ(&cie_x, &cie_y, &cie_z, spectrum);
    bu_log("X:\n");bn_print_table_and_tabdata("/dev/tty", cie_x);
    bu_log("Y:\n");bn_print_table_and_tabdata("/dev/tty", cie_y);
    bu_log("Z:\n");bn_print_table_and_tabdata("/dev/tty", cie_z);

    spect_make_NTSC_RGB(&ntsc_r, &ntsc_g, &ntsc_b, spectrum);
    bu_log("R:\n");bn_print_table_and_tabdata("/dev/tty", ntsc_r);
    bu_log("G:\n");bn_print_table_and_tabdata("/dev/tty", ntsc_g);
    bu_log("B:\n");bn_print_table_and_tabdata("/dev/tty", ntsc_b);
    {
	struct bu_vls str;
	bu_vls_init(&str);
	bn_tabdata_to_tcl(&str, ntsc_r);
	bu_log("ntsc_r tcl:  %s\n", bu_vls_addr(&str));
	bu_vls_free(&str);
    }

/* "A flat spectral curve is represente by equal XYZ values".  Hall pg 52 */
    flat = bn_tabdata_get_constval(42.0, spectrum);
    bu_log("flat:\n");bn_print_table_and_tabdata("/dev/tty", flat);
    spect_curve_to_xyz(xyz, flat, cie_x, cie_y, cie_z);
    VPRINT("flat xyz?", xyz);

    return;

    /* Check identity of XYZ->RGB->spectrum->XYZ->RGB */
    check(0.313,     0.329,      0.358);	/* D6500 white */
    check(0.670,     0.330,      0.000);	/* NTSC red primary */
    check(0.210,     0.710,      0.080);	/* NTSC green primary */
    check(0.140,     0.080,      0.780);	/* NTSC blue primary */
    check(.5, .5, .5);
    check(1, 0, 0);
    check(0, 1, 0);
    check(0, 0, 1);
    check(1, 1, 1);
    check(1, 1, 0);
    check(1, 0, 1);
    check(0, 1, 1);
}


static char usage[] = "\
Usage: ssampview [-t] [-s squarefilesize] [-w file_width] [-n file_height]\n\
		file.ssamp\n";


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "ts:w:n:")) != -1) {
	switch (c) {
	    case 't':
		fprintf(stderr, "ssampview: conducting library tests\n");
		conduct_tests();
		first_command = "do_testing";
		Tk_Main(1, argv, tcl_appinit);
		/* NOTREACHED */
		bu_exit(0, NULL);
		/* NOTREACHED */
		break;
	    case 's':
		/* square file size */
		height = width = atoi(bu_optarg);
		break;
	    case 'w':
		width = atoi(bu_optarg);
		break;
	    case 'n':
		height = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) return 0;
    return 1;	/* OK */
}


/*
 * M A I N
 */
int
main(int argc, char **argv)
{

    bu_debug = BU_DEBUG_COREDUMP;
    rt_g.debug = 1;

    make_ntsc_xyz2rgb(xyz2rgb);

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if (argc > 1 && BU_STR_EQUAL(argv[1], "-t")) {
    }

    datafile_basename = argv[bu_optind];

    first_command = "doit1 42";

    if ((fbp = fb_open(NULL, width, height)) == FBIO_NULL) {
	bu_exit(EXIT_FAILURE, "Unable to open fb\n");
    }
    fb_view(fbp, width/2, height/2, fb_getwidth(fbp)/width, fb_getheight(fbp)/height);

    /* Read spectrum definition */
    snprintf(spectrum_name, 100, "%s.spect", datafile_basename);
    spectrum = (struct bn_table *)bn_table_read(spectrum_name);
    if (spectrum == NULL) {
	bu_exit(EXIT_FAILURE, "Unable to read spectrum\n");
    }
    BN_CK_TABLE(spectrum);
    bu_log("spectrum has %zu samples\n", spectrum->nx);
    nwave = spectrum->nx;	/* shared with Tcl */

    /* Read atmosphere curve -- input is in microns, not nm */
    atmosphere_orig = bn_read_table_and_tabdata("../rttherm/std_day_1km.dat");
    bn_table_scale((struct bn_table *)(atmosphere_orig->table), 1000.0);
    atmosphere = bn_tabdata_resample_max(spectrum, atmosphere_orig);

    /* Allocate and read 2-D spectrum array */
    data = bn_tabdata_binary_read(datafile_basename, width*height, spectrum);
    if (!data) bu_exit(EXIT_FAILURE, "bn_tabdata_binary_read() of datafile_basename failed\n");

    /* Allocate framebuffer image buffer */
    pixels = (unsigned char *)bu_malloc(width * height * 3, "pixels[]");

    find_minmax();
    bu_log("min = %g, max=%g Watts\n", minval, maxval);

    Tk_Main(1, argv, tcl_appinit);
    /* NOTREACHED */

    return 0;
}


int
doit(ClientData UNUSED(cd), Tcl_Interp *interp, int UNUSED(argc), char **UNUSED(argv))
{
    size_t wl;
    char cmd[96];

    for (wl = 0; wl < spectrum->nx; wl++) {
	sprintf(cmd, "doit1 %lu", (unsigned long)wl);
	Tcl_Eval(interp, cmd);
    }
    return TCL_OK;
}


int
doit1(ClientData UNUSED(cd), Tcl_Interp *interp, int argc, char **argv)
{
    size_t wl;
    char buf[32];

    Tcl_ResetResult(interp);

    if (argc != 2) {
	Tcl_AppendResult(interp, "Usage: doit1 wavel#", (char *)NULL);
	return TCL_ERROR;
    }
    wl = atoi(argv[1]);
    if (wl >= spectrum->nx) {
	Tcl_AppendResult(interp, "Wavelength number out of range", (char *)NULL);
	return TCL_ERROR;
    }

    if (!data) {
	Tcl_AppendResult(interp, "pixel data table not loaded yet", (char *)NULL);
	return TCL_ERROR;
    }

    bu_log("doit1 %zu: %g um to %g um\n",
	   wl,
	   spectrum->x[wl] * 0.001,
	   spectrum->x[wl+1] * 0.001);
    if (use_cie_xyz)
	show_color(wl);
    else
	rescale(wl);
    (void)fb_writerect(fbp, 0, 0, width, height, pixels);
    fb_poll(fbp);

    /* export C variables to TCL, one-way */
    /* These are being traced by Tk, this will cause update */
    sprintf(buf, "%lu", (unsigned long)wl);
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
    char *cp;
    int todo;
    fastf_t max, min;
    size_t nbytes;
    size_t j;

    cp = (char *)data;
    nbytes = BN_SIZEOF_TABDATA(spectrum);

    max = -INFINITY;
    min =  INFINITY;

    for (todo = width * height; todo > 0; todo--, cp += nbytes) {
	struct bn_tabdata *sp;
	sp = (struct bn_tabdata *)cp;
	BN_CK_TABDATA(sp);
	for (j = 0; j < spectrum->nx; j++) {
	    fastf_t v;

	    if ((v = sp->y[j]) > max) max = v;
	    if (v < min) min = v;
	}
    }
    maxval = max;
    minval = min;
}


/*
 * R E S C A L E
 *
 * Create monochrome image from the spectral data, at wavelength 'wav',
 * given current min & max values.
 */
void
rescale(int wav)
{
    char *cp;
    unsigned char *pp;
    int todo;
    int nbytes;
    fastf_t scale;
    fastf_t atmos_scale;

    cp = (char *)data;
    nbytes = BN_SIZEOF_TABDATA(spectrum);

    pp = pixels;

    scale = 255 / (maxval - minval);

    if (use_atmosphere)
	atmos_scale = atmosphere->y[wav];
    else
	atmos_scale = 1;

    for (todo = width * height; todo > 0; todo--, cp += nbytes, pp += 3) {
	struct bn_tabdata *sp;
	int val;

	sp = (struct bn_tabdata *)cp;
	BN_CK_TABDATA(sp);

	val = (sp->y[wav] * atmos_scale - minval) * scale;
	if (val > 255) val = 255;
	else if (val < 0) val = 0;
	pp[0] = pp[1] = pp[2] = val;
    }
}


/*
 * S H O W _ C O L O R
 *
 * Create color image from spectral curve,
 * given current min & max values, and frequency offset (in nm).
 * Go via CIE XYZ space.
 */
void
show_color(int off)
{
    char *cp;
    unsigned char *pp;
    int todo;
    int nbytes;
    fastf_t scale;
    struct bn_tabdata *new;

    cp = (char *)data;
    nbytes = BN_SIZEOF_TABDATA(spectrum);

    pp = pixels;

    scale = 255 / (maxval - minval);

    /* Build CIE curves */
    if (cie_x->magic == 0)
	rt_spect_make_CIE_XYZ(&cie_x, &cie_y, &cie_z, spectrum);

    BN_GET_TABDATA(new, spectrum);

    for (todo = width * height; todo > 0; todo--, cp += nbytes, pp += 3) {
	struct bn_tabdata *sp;
	point_t xyz;
	point_t rgb;
	int val;

	sp = (struct bn_tabdata *)cp;
	BN_CK_TABDATA(sp);

	if (use_atmosphere) {
	    bn_tabdata_mul(new, sp, atmosphere);
	    bn_tabdata_freq_shift(new, new, spectrum->x[off] - 380.0);
	} else {
	    bn_tabdata_freq_shift(new, sp, spectrum->x[off] - 380.0);
	}

	spect_curve_to_xyz(xyz, new, cie_x, cie_y, cie_z);

	MAT3X3VEC(rgb, xyz2rgb, xyz);

	val = (rgb[RED] - minval) * scale;
	if (val > 255) val = 255;
	else if (val < 0) val = 0;
	pp[RED] = val;

	val = (rgb[GRN] - minval) * scale;
	if (val > 255) val = 255;
	else if (val < 0) val = 0;
	pp[GRN] = val;

	val = (rgb[BLU] - minval) * scale;
	if (val > 255) val = 255;
	else if (val < 0) val = 0;
	pp[BLU] = val;
    }

    bn_tabdata_free(new);
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
