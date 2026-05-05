/*                   T E S T _ R T _ O P T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file rt/tests/test_rt_opt.c
 *
 * Comprehensive white-box regression test for get_args() in opt.c.
 *
 * Tests every option (short and long forms) defined in the bu_opt
 * descriptor table, verifying that each one correctly mutates the
 * corresponding global variable(s).  Special-form edge cases are
 * also covered: -vvvv, -v 0xNN, -T dist/perp, -l 7,..., -+t,
 * negative -P, -Q x,y, and the bu_optind model/object boundary.
 *
 * Build: linked against all LIBRTUIF sources except main.c, plus
 * viewcheck.c (provides application_init, view_init, etc.) and
 * this file (provides the missing main.c globals + test main).
 *
 * Exit code: 0 = all tests passed, else = number of failures.
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bio.h"
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"
#include "optical.h"
#include "bn/rand.h"

/* rt-private headers */
#include "../rtuif.h"
#include "../ext.h"

/* -----------------------------------------------------------------------
 * Globals that live in main.c in normal rt builds.  We define them here
 * so that do.c, worker.c, grid.c, viewcheck.c and the rest of librtuif
 * can link successfully without pulling in main.c (which has its own
 * main()).
 * ----------------------------------------------------------------------- */

struct fb	*fbp = FB_NULL;		/* framebuffer handle */
struct icv_image *bif = NULL;		/* ICV image buffer */
struct application APP;			/* global application structure */
FILE		*outfp = NULL;		/* optional output file */
int		report_progress = 0;	/* !0 = user wants progress report */
size_t		n_malloc = 0;		/* malloc counter (for memory_summary) */
size_t		n_free = 0;
size_t		n_realloc = 0;

/* memory_summary() is called by do.c; we provide a no-op stub */
void
memory_summary(void)
{
    n_malloc  = bu_n_malloc;
    n_free    = bu_n_free;
    n_realloc = bu_n_realloc;
}

/* -----------------------------------------------------------------------
 * Global test state
 * ----------------------------------------------------------------------- */

static int g_total_tests  = 0;
static int g_failed_tests = 0;
static int g_short_only   = 0;
static int g_skip_current_checks = 0;

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/* Reset all opt.c globals to their documented defaults so each test
 * starts from a known state. */
static void
reset_globals(void)
{
    extern size_t width;
    extern size_t height;
    extern int doubles_out;
    extern fastf_t azimuth, elevation;
    extern int lightmodel;
    extern int rpt_overlap;
    extern int default_background;
    extern int output_is_binary;
    extern int query_x, query_y;
    extern int Query_one_pixel;
    extern int query_optical_debug;
    extern int query_debug;
    extern int stereo;
    extern int hypersample;
    extern unsigned int jitter;
    extern fastf_t rt_perspective;
    extern fastf_t aspect;
    extern fastf_t cell_width, cell_height;
    extern int cell_newsize;
    extern fastf_t eye_backoff;
    extern fastf_t viewsize;
    extern int incr_mode;
    extern int full_incr_mode;
    extern size_t incr_level, incr_nlevel;
    extern size_t full_incr_sample, full_incr_nsamples;
    extern ssize_t npsw;
    extern int top_down;
    extern int random_mode;
    extern int opencl_mode;
    extern double pmargs[9];
    extern char pmfile[255];
    extern int objc;
    extern char **objv;
    extern char *string_pix_start;
    extern char *string_pix_end;
    extern int pix_start, pix_end;
    extern int matflag, orientflag;
    extern int desiredframe, finalframe, curframe;
    extern char *outputfile;
    extern int benchmark;
    extern int sub_grid_mode;
    extern int sub_xmin, sub_ymin, sub_xmax, sub_ymax;
    extern int use_air;
    extern int save_overlaps;
    extern int rtg_parallel;
    extern double airdensity;
    extern double haze[3];
    extern int do_kut_plane;
    extern plane_t kut_plane;
    extern double units;
    extern int default_units, model_units;
    extern const char *densityfile;
    extern fastf_t rt_dist_tol, rt_perp_tol;
    extern char *framebuffer;
    extern int space_partition;
    extern int rt_verbosity;

    width          = 0;
    height         = 0;
    doubles_out    = 0;
    azimuth        = 0.0;
    elevation      = 0.0;
    lightmodel     = 0;
    rpt_overlap    = 1;
    default_background = 1;
    output_is_binary = 1;
    query_x = query_y = 0;
    Query_one_pixel   = 0;
    query_optical_debug = 0;
    query_debug       = 0;
    stereo            = 0;
    hypersample       = 0;
    jitter            = 0;
    rt_perspective    = (fastf_t)0.0;
    aspect            = (fastf_t)1.0;
    cell_width        = (fastf_t)0.0;
    cell_height       = (fastf_t)0.0;
    cell_newsize      = 0;
    eye_backoff       = (fastf_t)M_SQRT2;
    viewsize          = (fastf_t)0.0;
    incr_mode         = 0;
    full_incr_mode    = 0;
    incr_level = incr_nlevel = 0;
    full_incr_sample = full_incr_nsamples = 0;
    npsw              = 1;
    top_down          = 0;
    random_mode       = 0;
    opencl_mode       = 0;
    memset(pmargs, 0, sizeof(pmargs));
    memset(pmfile, 0, sizeof(pmfile));
    objc = 0; objv = NULL;
    string_pix_start = string_pix_end = NULL;
    pix_start = -1; pix_end = 0;
    matflag = orientflag = 0;
    desiredframe = 0; finalframe = -1; curframe = 0;
    outputfile    = NULL;
    benchmark     = 0;
    sub_grid_mode = 0;
    sub_xmin = sub_ymin = sub_xmax = sub_ymax = 0;
    use_air       = 0;
    save_overlaps = 0;
    rtg_parallel  = 0;
    airdensity    = 0.0;
    haze[0] = 0.8; haze[1] = 0.9; haze[2] = 0.99;
    do_kut_plane  = 0;
    VSET(kut_plane, 0.0, 0.0, 0.0); kut_plane[W] = 0.0;
    units         = 1.0;
    default_units = 1;
    model_units   = 0;
    densityfile   = NULL;
    rt_dist_tol   = (fastf_t)0.0005;
    rt_perp_tol   = (fastf_t)0.0;
    framebuffer   = NULL;
    space_partition = RT_PART_NUBSPT;
    rt_verbosity  = -1;
    /* libbu/librt debug flags */
    bu_debug      = 0;
    rt_debug      = 0;
    optical_debug = 0;
    nmg_debug     = 0;
    /* AmbientIntensity is in liboptical */
    AmbientIntensity = 0.4;
    /* bn random table */
    bn_randhalftabsize = BN_RANDHALFTABSIZE;
    /* bu_optind: reset for clean parser state */
    bu_optind = 1;
    g_skip_current_checks = 0;
}


/* Assertion helpers.  Each records pass/fail and prints diagnostics. */
#define CHECK(name, cond) do { \
    if (g_skip_current_checks) break; \
    g_total_tests++; \
    if (!(cond)) { \
	bu_log("FAIL [%s]: condition (%s) is false\n", (name), #cond); \
	g_failed_tests++; \
    } \
} while (0)

#define CHECK_INT(name, expected, actual) do { \
    if (g_skip_current_checks) break; \
    int _e = (int)(expected); \
    int _a = (int)(actual); \
    g_total_tests++; \
    if (_e != _a) { \
	bu_log("FAIL [%s]: expected %d, got %d\n", (name), _e, _a); \
	g_failed_tests++; \
    } \
} while (0)

#define CHECK_DBL(name, expected, actual) do { \
    if (g_skip_current_checks) break; \
    double _e = (double)(expected); \
    double _a = (double)(actual); \
    double _tol = fabs(_e) * 1e-6 + 1e-15; \
    g_total_tests++; \
    if (fabs(_e - _a) > _tol) { \
	bu_log("FAIL [%s]: expected %g, got %g\n", (name), _e, _a); \
	g_failed_tests++; \
    } \
} while (0)

#define CHECK_STR(name, expected, actual) do { \
    if (g_skip_current_checks) break; \
    const char *_e = (expected); \
    const char *_a = (actual); \
    g_total_tests++; \
    if (!_a || bu_strcmp(_e, _a) != 0) { \
	bu_log("FAIL [%s]: expected '%s', got '%s'\n", (name), _e, _a ? _a : "(null)"); \
	g_failed_tests++; \
    } \
} while (0)

/* Build a NULL-terminated const char* argv from a literal list.
 * Usage:  MAKE_ARGV(argv, "rt", "-s", "256", NULL);
 * argc is computed automatically. */
#define MAKE_ARGV(av, ...) \
    const char *av[] = { __VA_ARGS__, NULL }; \
    int av##_argc = 0; \
    { const char **_p = av; while (*_p++) av##_argc++; }

/* Convenience: call get_args and check it returns the expected code */
#define CALL_GET_ARGS(av, expected_ret) do { \
    if (g_short_only && av##_argc > 1 && bu_strncmp(av[1], "--", 2) == 0) { \
	g_skip_current_checks = 1; \
    } else { \
	int _ret = get_args(av##_argc, av); \
	g_skip_current_checks = 0; \
	g_total_tests++; \
	if (_ret != (expected_ret)) { \
	    bu_log("FAIL [get_args return]: expected %d, got %d\n", (expected_ret), _ret); \
	    g_failed_tests++; \
	} \
    } \
} while (0)


/* -----------------------------------------------------------------------
 * Individual option tests
 * ----------------------------------------------------------------------- */

/* -s / --size  (sets width == height) */
static void
test_opt_size(void)
{
    extern size_t width, height;
    {
	reset_globals();
	MAKE_ARGV(av, "rt", "-s", "256");
	CALL_GET_ARGS(av, 1);
	CHECK_INT("-s width",  256, (int)width);
	CHECK_INT("-s height", 256, (int)height);
    }
    {
	reset_globals();
	MAKE_ARGV(av, "rt", "--size", "512");
	CALL_GET_ARGS(av, 1);
	CHECK_INT("--size width",  512, (int)width);
	CHECK_INT("--size height", 512, (int)height);
    }
    /* out-of-range: no change */
    {
	reset_globals();
	MAKE_ARGV(av, "rt", "-s", "0");
	CALL_GET_ARGS(av, 1);
	CHECK_INT("-s 0 (no change) width",  0, (int)width);
    }
}


/* -w / --width */
static void
test_opt_width(void)
{
    extern size_t width;
    reset_globals();
    MAKE_ARGV(av, "rt", "-w", "800");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-w", 800, (int)width);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--width", "1024");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--width", 1024, (int)width);
}


/* -n / --height */
static void
test_opt_height(void)
{
    extern size_t height;
    reset_globals();
    MAKE_ARGV(av, "rt", "-n", "600");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-n", 600, (int)height);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--height", "768");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--height", 768, (int)height);
}


/* -o / --output */
static void
test_opt_output(void)
{
    extern char *outputfile;
    extern int doubles_out;
    reset_globals();
    MAKE_ARGV(av, "rt", "-o", "out.pix");
    CALL_GET_ARGS(av, 1);
    CHECK_STR("-o outputfile", "out.pix", outputfile);
    CHECK_INT("-o doubles_out", 0, doubles_out);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--output", "image.png");
    CALL_GET_ARGS(av2, 1);
    CHECK_STR("--output", "image.png", outputfile);
}


/* -O / --output-double */
static void
test_opt_output_double(void)
{
    extern char *outputfile;
    extern int doubles_out;
    reset_globals();
    MAKE_ARGV(av, "rt", "-O", "out.dpix");
    CALL_GET_ARGS(av, 1);
    CHECK_STR("-O outputfile", "out.dpix", outputfile);
    CHECK_INT("-O doubles_out", 1, doubles_out);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--output-double", "out2.dpix");
    CALL_GET_ARGS(av2, 1);
    CHECK_STR("--output-double", "out2.dpix", outputfile);
    CHECK_INT("--output-double doubles_out", 1, doubles_out);
}


/* -F / --framebuffer */
static void
test_opt_framebuffer(void)
{
    extern char *framebuffer;
    reset_globals();
    MAKE_ARGV(av, "rt", "-F", "/dev/fb0");
    CALL_GET_ARGS(av, 1);
    CHECK_STR("-F", "/dev/fb0", framebuffer);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--framebuffer", "myfb");
    CALL_GET_ARGS(av2, 1);
    CHECK_STR("--framebuffer", "myfb", framebuffer);
}


/* -a / --azimuth */
static void
test_opt_azimuth(void)
{
    extern fastf_t azimuth;
    extern int matflag;
    reset_globals();
    matflag = 1; /* should be cleared by -a */
    MAKE_ARGV(av, "rt", "-a", "45");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-a azimuth", 45.0, azimuth);
    CHECK_INT("-a clears matflag", 0, matflag);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--azimuth", "90");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("--azimuth", 90.0, azimuth);
}


/* -e / --elevation */
static void
test_opt_elevation(void)
{
    extern fastf_t elevation;
    extern int matflag;
    reset_globals();
    matflag = 1;
    MAKE_ARGV(av, "rt", "-e", "35");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-e elevation", 35.0, elevation);
    CHECK_INT("-e clears matflag", 0, matflag);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--elevation", "-15");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("--elevation negative", -15.0, elevation);
}


/* -p / --perspective */
static void
test_opt_perspective(void)
{
    extern fastf_t rt_perspective;
    reset_globals();
    MAKE_ARGV(av, "rt", "-p", "45");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-p", 45.0, rt_perspective);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--perspective", "0");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("--perspective 0 (ortho)", 0.0, rt_perspective);

    /* out-of-range: clipped to 0 */
    reset_globals();
    MAKE_ARGV(av3, "rt", "-p", "200");
    CALL_GET_ARGS(av3, 1);
    CHECK_DBL("-p 200 (clipped)", 0.0, rt_perspective);
}


/* -E / --eye-backoff */
static void
test_opt_eye_backoff(void)
{
    extern fastf_t eye_backoff;
    reset_globals();
    MAKE_ARGV(av, "rt", "-E", "2.5");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-E", 2.5, eye_backoff);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--eye-backoff", "1.0");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("--eye-backoff", 1.0, eye_backoff);
}


/* -V / --view-aspect */
static void
test_opt_view_aspect(void)
{
    extern fastf_t aspect;
    /* Plain fraction */
    reset_globals();
    MAKE_ARGV(av, "rt", "-V", "1.5");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-V 1.5", 1.5, aspect);

    /* Ratio form 4:3 */
    reset_globals();
    MAKE_ARGV(av2, "rt", "--view-aspect", "4:3");
    CALL_GET_ARGS(av2, 1);
    if (!g_skip_current_checks) {
	double expected = 4.0 / 3.0;
	g_total_tests++;
	if (fabs(aspect - expected) > 1e-6) {
	    bu_log("FAIL [--view-aspect 4:3]: expected %g, got %g\n", expected, aspect);
	    g_failed_tests++;
	}
    }

    /* bogus <= 0: reset to 1.0 by get_args sanity check */
    reset_globals();
    MAKE_ARGV(av3, "rt", "-V", "-1");
    CALL_GET_ARGS(av3, 1);
    CHECK_DBL("-V -1 (clamped to 1.0)", 1.0, aspect);
}


/* -M / --read-matrix */
static void
test_opt_matflag(void)
{
    extern int matflag;
    reset_globals();
    MAKE_ARGV(av, "rt", "-M");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-M matflag", 1, matflag);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--read-matrix");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--read-matrix", 1, matflag);
}


/* -g / --cell-width */
static void
test_opt_cell_width(void)
{
    extern fastf_t cell_width;
    extern int cell_newsize;
    reset_globals();
    MAKE_ARGV(av, "rt", "-g", "2.5");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-g cell_width", 2.5, cell_width);
    CHECK_INT("-g cell_newsize", 1, cell_newsize);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--cell-width", "5.0");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("--cell-width", 5.0, cell_width);
}


/* -G / --cell-height */
static void
test_opt_cell_height(void)
{
    extern fastf_t cell_height;
    extern int cell_newsize;
    reset_globals();
    MAKE_ARGV(av, "rt", "-G", "3.0");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-G cell_height", 3.0, cell_height);
    CHECK_INT("-G cell_newsize", 1, cell_newsize);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--cell-height", "4.5");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("--cell-height", 4.5, cell_height);
}


/* -j / --subgrid */
static void
test_opt_subgrid(void)
{
    extern int sub_xmin, sub_ymin, sub_xmax, sub_ymax, sub_grid_mode;
    /* standard comma-separated form */
    reset_globals();
    MAKE_ARGV(av, "rt", "-j", "10,20,300,400");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-j sub_xmin", 10,  sub_xmin);
    CHECK_INT("-j sub_ymin", 20,  sub_ymin);
    CHECK_INT("-j sub_xmax", 300, sub_xmax);
    CHECK_INT("-j sub_ymax", 400, sub_ymax);
    CHECK_INT("-j sub_grid_mode", 1, sub_grid_mode);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--subgrid", "0,0,64,64");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--subgrid sub_xmax", 64, sub_xmax);
    CHECK_INT("--subgrid sub_grid_mode", 1, sub_grid_mode);

    /* invalid (xmin >= xmax): grid mode should be 0 */
    reset_globals();
    MAKE_ARGV(av3, "rt", "-j", "100,100,50,50");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-j bad range sub_grid_mode", 0, sub_grid_mode);
}


/* -k / --cut-plane */
static void
test_opt_cut_plane(void)
{
    extern int do_kut_plane;
    extern plane_t kut_plane;
    const double inv_sqrt3 = 1.0 / sqrt(3.0);
    const double sqrt3 = sqrt(3.0);
    reset_globals();
    /* Normal vector (1,0,0) at dist=5 */
    MAKE_ARGV(av, "rt", "-k", "1,0,0,5");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-k do_kut_plane", 1, do_kut_plane);
    CHECK_DBL("-k kut_plane[0]", 1.0, kut_plane[0]);
    CHECK_DBL("-k kut_plane[1]", 0.0, kut_plane[1]);
    CHECK_DBL("-k kut_plane[2]", 0.0, kut_plane[2]);
    CHECK_DBL("-k kut_plane[W]", 5.0, kut_plane[W]);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--cut-plane", "0,1,0,10");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--cut-plane", 1, do_kut_plane);
    CHECK_DBL("--cut-plane Y normal", 1.0, kut_plane[1]);

    reset_globals();
    MAKE_ARGV(av3, "rt", "-k", "1,2,3,0,0,2");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-k point+normal do_kut_plane", 1, do_kut_plane);
    CHECK_DBL("-k point+normal kut_plane[0]", 0.0, kut_plane[0]);
    CHECK_DBL("-k point+normal kut_plane[1]", 0.0, kut_plane[1]);
    CHECK_DBL("-k point+normal kut_plane[2]", 1.0, kut_plane[2]);
    CHECK_DBL("-k point+normal kut_plane[W]", 3.0, kut_plane[W]);

    reset_globals();
    MAKE_ARGV(av4, "rt", "--cut-plane", "4, 0, 0, 3, 0, 0");
    CALL_GET_ARGS(av4, 1);
    CHECK_INT("--cut-plane point+normal", 1, do_kut_plane);
    CHECK_DBL("--cut-plane point+normal kut_plane[0]", 1.0, kut_plane[0]);
    CHECK_DBL("--cut-plane point+normal kut_plane[1]", 0.0, kut_plane[1]);
    CHECK_DBL("--cut-plane point+normal kut_plane[2]", 0.0, kut_plane[2]);
    CHECK_DBL("--cut-plane point+normal kut_plane[W]", 4.0, kut_plane[W]);

    reset_globals();
    MAKE_ARGV(av5, "rt", "-k", "1,1,1,1,1,1");
    CALL_GET_ARGS(av5, 1);
    CHECK_INT("-k diagonal point+normal", 1, do_kut_plane);
    CHECK_DBL("-k diagonal point+normal kut_plane[0]", inv_sqrt3, kut_plane[0]);
    CHECK_DBL("-k diagonal point+normal kut_plane[1]", inv_sqrt3, kut_plane[1]);
    CHECK_DBL("-k diagonal point+normal kut_plane[2]", inv_sqrt3, kut_plane[2]);
    CHECK_DBL("-k diagonal point+normal kut_plane[W]", sqrt3, kut_plane[W]);
}


/* -A / --ambient */
static void
test_opt_ambient(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-A", "0.7");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-A", 0.7, AmbientIntensity);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--ambient", "0.2");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("--ambient", 0.2, AmbientIntensity);
}


/* -l / --light-model */
static void
test_opt_light_model(void)
{
    extern int lightmodel;
    extern double pmargs[9];
    extern char pmfile[255];

    /* Simple model selection */
    reset_globals();
    MAKE_ARGV(av, "rt", "-l", "3");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-l 3", 3, lightmodel);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--light-model", "0");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--light-model 0", 0, lightmodel);

    /* Photon mapping model 7 with default args (no comma args -> defaults) */
    reset_globals();
    MAKE_ARGV(av3, "rt", "-l", "7");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-l 7", 7, lightmodel);
}


/* -m / --haze */
static void
test_opt_haze(void)
{
    extern double airdensity;
    extern double haze[3];

    /* Without spaces */
    reset_globals();
    MAKE_ARGV(av, "rt", "-m", "2.5e-8,0.75,0.95,0.99");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-m airdensity", 2.5e-8, airdensity);
    CHECK_DBL("-m haze[0]",    0.75,   haze[0]);
    CHECK_DBL("-m haze[1]",    0.95,   haze[1]);
    CHECK_DBL("-m haze[2]",    0.99,   haze[2]);

    /* With spaces after commas */
    reset_globals();
    MAKE_ARGV(av2, "rt", "-m", "1e-6, 0.5, 0.6, 0.7");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("-m (spaces) airdensity", 1e-6, airdensity);
    CHECK_DBL("-m (spaces) haze[0]",    0.5,  haze[0]);

    reset_globals();
    MAKE_ARGV(av3, "rt", "--haze", "1.0,0.1,0.2,0.3");
    CALL_GET_ARGS(av3, 1);
    CHECK_DBL("--haze airdensity", 1.0, airdensity);
}


/* -i / --incremental */
static void
test_opt_incremental(void)
{
    extern int incr_mode;
    reset_globals();
    MAKE_ARGV(av, "rt", "-i");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-i incr_mode", 1, incr_mode);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--incremental");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--incremental", 1, incr_mode);
}


/* -t / --top-down */
static void
test_opt_topdown(void)
{
    extern int top_down;
    reset_globals();
    MAKE_ARGV(av, "rt", "-t");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-t", 1, top_down);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--top-down");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--top-down", 1, top_down);
}


/* -S / --stereo */
static void
test_opt_stereo(void)
{
    extern int stereo;
    reset_globals();
    MAKE_ARGV(av, "rt", "-S");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-S", 1, stereo);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--stereo");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--stereo", 1, stereo);
}


/* -H / --hypersample */
static void
test_opt_hypersample(void)
{
    extern int hypersample;
    extern unsigned int jitter;
    reset_globals();
    MAKE_ARGV(av, "rt", "-H", "4");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-H hypersample", 4, hypersample);
    CHECK_INT("-H enables jitter", 1, (int)jitter);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--hypersample", "8");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--hypersample", 8, hypersample);
}


/* -J / --jitter */
static void
test_opt_jitter(void)
{
    extern unsigned int jitter;
    reset_globals();
    MAKE_ARGV(av, "rt", "-J", "3");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-J 3", 3, (int)jitter);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--jitter", "1");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--jitter 1", 1, (int)jitter);

    /* hex form */
    reset_globals();
    MAKE_ARGV(av3, "rt", "-J", "0x2");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-J 0x2", 2, (int)jitter);
}


/* -u / --units */
static void
test_opt_units(void)
{
    extern int model_units, default_units;
    reset_globals();
    MAKE_ARGV(av, "rt", "-u", "model");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-u model -> model_units", 1, model_units);
    CHECK_INT("-u model -> default_units", 0, default_units);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--units", "mm");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--units mm -> default_units", 0, default_units);
    CHECK_INT("--units mm -> model_units", 0, model_units);
}


/* -U / --use-air */
static void
test_opt_use_air(void)
{
    extern int use_air;
    reset_globals();
    MAKE_ARGV(av, "rt", "-U", "1");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-U 1", 1, use_air);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--use-air", "0");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--use-air 0", 0, use_air);
}


/* -T / --tolerance */
static void
test_opt_tolerance(void)
{
    extern fastf_t rt_dist_tol, rt_perp_tol;

    /* Single dist only */
    reset_globals();
    MAKE_ARGV(av, "rt", "-T", "0.01");
    CALL_GET_ARGS(av, 1);
    CHECK_DBL("-T dist only", 0.01, rt_dist_tol);

    /* dist/perp separated by slash */
    reset_globals();
    MAKE_ARGV(av2, "rt", "-T", "0.001/1e-5");
    CALL_GET_ARGS(av2, 1);
    CHECK_DBL("-T dist/perp dist", 0.001, rt_dist_tol);
    CHECK_DBL("-T dist/perp perp", 1e-5,  rt_perp_tol);

    /* dist,perp separated by comma */
    reset_globals();
    MAKE_ARGV(av3, "rt", "--tolerance", "0.005,1e-6");
    CALL_GET_ARGS(av3, 1);
    CHECK_DBL("--tolerance dist,perp dist", 0.005, rt_dist_tol);
    CHECK_DBL("--tolerance dist,perp perp", 1e-6,  rt_perp_tol);

    /* perp out of range (>= 1): should not be set */
    reset_globals();
    MAKE_ARGV(av4, "rt", "-T", "0.002/2.0");
    CALL_GET_ARGS(av4, 1);
    CHECK_DBL("-T perp >= 1 (not set)", 0.0, rt_perp_tol);
}


/* -z / --opencl */
static void
test_opt_opencl(void)
{
    extern int opencl_mode;
    reset_globals();
    MAKE_ARGV(av, "rt", "-z", "1");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-z", 1, opencl_mode);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--opencl", "0");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--opencl 0", 0, opencl_mode);
}


/* -c / --command (passes to rt_do_cmd; just verify it doesn't crash
 * and returns success) */
static void
test_opt_command(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-c", "set background=0/0/0");
    CALL_GET_ARGS(av, 1);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--command", "set background=0/0/0");
    CALL_GET_ARGS(av2, 1);
}


/* -d / --density-file */
static void
test_opt_density_file(void)
{
    extern const char *densityfile;
    reset_globals();
    MAKE_ARGV(av, "rt", "-d", "density.tbl");
    CALL_GET_ARGS(av, 1);
    CHECK_STR("-d densityfile", "density.tbl", densityfile);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--density-file", "other.tbl");
    CALL_GET_ARGS(av2, 1);
    CHECK_STR("--density-file", "other.tbl", densityfile);
}


/* -D / --start-frame */
static void
test_opt_start_frame(void)
{
    extern int desiredframe;
    reset_globals();
    MAKE_ARGV(av, "rt", "-D", "5");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-D", 5, desiredframe);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--start-frame", "10");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--start-frame", 10, desiredframe);
}


/* -K / --end-frame */
static void
test_opt_end_frame(void)
{
    extern int finalframe;
    reset_globals();
    MAKE_ARGV(av, "rt", "-K", "100");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-K", 100, finalframe);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--end-frame", "200");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--end-frame", 200, finalframe);
}


/* -b / --single-pixel */
static void
test_opt_single_pixel(void)
{
    extern char *string_pix_start;
    extern ssize_t npsw;
    reset_globals();
    MAKE_ARGV(av, "rt", "-b", "100 200");
    CALL_GET_ARGS(av, 1);
    CHECK_STR("-b string_pix_start", "100 200", string_pix_start);
    CHECK_INT("-b npsw=1", 1, (int)npsw);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--single-pixel", "50 75");
    CALL_GET_ARGS(av2, 1);
    CHECK_STR("--single-pixel", "50 75", string_pix_start);
}


/* -Q / --query-pixel  (also tests x,y parsing) */
static void
test_opt_query_pixel(void)
{
    extern int Query_one_pixel, query_x, query_y;

    reset_globals();
    MAKE_ARGV(av, "rt", "-Q", "40,80");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-Q Query_one_pixel toggled", 1, Query_one_pixel);
    CHECK_INT("-Q query_x", 40, query_x);
    CHECK_INT("-Q query_y", 80, query_y);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--query-pixel", "10,20");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--query-pixel x", 10, query_x);
    CHECK_INT("--query-pixel y", 20, query_y);
}


/* -I / --objects-file (create a temp file, then test) */
static void
test_opt_objects_file(void)
{
    extern int objc;
    extern char **objv;
    char tmpfile[512] = {0};
    FILE *fp;

    /* Create a temporary object list file.
     * bu_temp_file() fills tmpfile with the actual path it creates. */
    fp = bu_temp_file(tmpfile, sizeof(tmpfile));
    if (!fp) {
	bu_log("WARN: test_opt_objects_file: cannot create temp file, skipping\n");
	return;
    }
    fprintf(fp, "obj_a\nobj_b\nobj_c\n");
    fclose(fp);

    reset_globals();
    {
	const char *av[] = { "rt", "-I", tmpfile, NULL };
	int av_argc = 3;
	int ret = get_args(av_argc, av);
	g_total_tests++;
	if (ret != 1) {
	    bu_log("FAIL [-I return]: expected 1, got %d\n", ret);
	    g_failed_tests++;
	}
    }
    CHECK_INT("-I objc", 3, objc);
    if (objv) {
	CHECK_STR("-I objv[0]", "obj_a", objv[0]);
	CHECK_STR("-I objv[1]", "obj_b", objv[1]);
	CHECK_STR("-I objv[2]", "obj_c", objv[2]);
	/* clean up */
	{
	    int ii;
	    for (ii = 0; ii < objc; ii++)
		bu_free(objv[ii], "test objv");
	    bu_free(objv, "test objv array");
	    objv = NULL; objc = 0;
	}
    }

    /* long form */
    if (!g_short_only) {
	reset_globals();
	{
	    const char *av2[] = { "rt", "--objects-file", tmpfile, NULL };
	    int av2_argc = 3;
	    int ret2 = get_args(av2_argc, av2);
	    g_total_tests++;
	    if (ret2 != 1) {
		bu_log("FAIL [--objects-file return]: expected 1, got %d\n", ret2);
		g_failed_tests++;
	    }
	}
	CHECK_INT("--objects-file objc", 3, objc);
	if (objv) {
	    int ii;
	    for (ii = 0; ii < objc; ii++) bu_free(objv[ii], "test objv");
	    bu_free(objv, "test objv array");
	    objv = NULL; objc = 0;
	}
    }

    bu_file_delete(tmpfile);
}


/* -+ / --text-output  t */
static void
test_opt_plus(void)
{
    extern int output_is_binary;

    reset_globals();
    MAKE_ARGV(av, "rt", "-+", "t");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-+ t output_is_binary", 0, output_is_binary);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--text-output", "t");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--text-output t", 0, output_is_binary);

    /* -+t (combined form) */
    reset_globals();
    MAKE_ARGV(av3, "rt", "-+t");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-+t combined", 0, output_is_binary);
}


/* -q / --rand-table-size */
static void
test_opt_rand_table(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-q", "1024");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-q rand table size", 1024, bn_randhalftabsize);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--rand-table-size", "2048");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--rand-table-size", 2048, bn_randhalftabsize);
}


/* -v / --verbose  (all forms) */
static void
test_opt_verbose(void)
{
    extern int rt_verbosity;

    /* Plain -v from default -1: increase_level(-1 cast to unsigned) is
     * already all-ones, so rt_verbosity stays -1 (0xffffffff). */
    reset_globals();
    rt_verbosity = 0; /* set to 0 so we can verify an increase */
    MAKE_ARGV(av, "rt", "-v");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-v from 0 -> 0xff", 0xff, rt_verbosity);

    /* Two flags -v -v: should go to 0xffff */
    reset_globals();
    rt_verbosity = 0;
    MAKE_ARGV(av2, "rt", "-v", "-v");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("-v -v -> 0xffff", 0xffff, rt_verbosity);

    /* Clustered -vvv: next arg is "vv" (2 more v's) -> 3 increments -> 0xffffff */
    reset_globals();
    rt_verbosity = 0;
    MAKE_ARGV(av3, "rt", "-vvv");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-vvv -> 0xffffff", 0xffffff, rt_verbosity);

    /* Hex override: -v 0x12 sets verbosity to 0x12 */
    reset_globals();
    MAKE_ARGV(av4, "rt", "-v", "0x12");
    CALL_GET_ARGS(av4, 1);
    CHECK_INT("-v 0x12", 0x12, rt_verbosity);

    /* Long form */
    reset_globals();
    rt_verbosity = 0;
    MAKE_ARGV(av5, "rt", "--verbose");
    CALL_GET_ARGS(av5, 1);
    CHECK_INT("--verbose from 0", 0xff, rt_verbosity);
}


/* -x / --rt-debug */
static void
test_opt_rt_debug(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-x", "0x10");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-x rt_debug", 0x10, (int)rt_debug);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--rt-debug", "0xff");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--rt-debug", 0xff, (int)rt_debug);

    reset_globals();
    MAKE_ARGV(av3, "rt", "-x", "0");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-x 0", 0, (int)rt_debug);
}


/* -X / --optical-debug */
static void
test_opt_optical_debug(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-X", "0x20");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-X", 0x20, (int)optical_debug);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--optical-debug", "0x1");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--optical-debug", 0x1, (int)optical_debug);
}


/* -N / --nmg-debug */
static void
test_opt_nmg_debug(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-N", "0x8");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-N", 0x8, (int)nmg_debug);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--nmg-debug", "0x0");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--nmg-debug 0", 0, (int)nmg_debug);
}


/* -! / --bu-debug */
static void
test_opt_bu_debug(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-!", "0x4");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-! bu_debug", 0x4, (int)bu_debug);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--bu-debug", "0x0");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--bu-debug 0", 0, (int)bu_debug);
}


/* -P / --cpus */
static void
test_opt_cpus(void)
{
    extern ssize_t npsw;
    reset_globals();
    MAKE_ARGV(av, "rt", "-P", "4");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-P 4", 4, (int)npsw);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--cpus", "1");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--cpus 1", 1, (int)npsw);

    /* Negative value: "all but N" */
    reset_globals();
    MAKE_ARGV(av3, "rt", "-P", "-2");
    CALL_GET_ARGS(av3, 1);
    CHECK_INT("-P -2 (all but 2)", -2, (int)npsw);
}


/* -B / --benchmark */
static void
test_opt_benchmark(void)
{
    extern int benchmark;
    reset_globals();
    MAKE_ARGV(av, "rt", "-B");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-B benchmark", 1, benchmark);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--benchmark");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--benchmark", 1, benchmark);
}


/* -r / --report-overlaps */
static void
test_opt_report_overlaps(void)
{
    extern int rpt_overlap;
    /* default is 1; -r is a flag that re-affirms it */
    reset_globals();
    rpt_overlap = 0;
    MAKE_ARGV(av, "rt", "-r");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-r sets rpt_overlap=1", 1, rpt_overlap);

    reset_globals();
    rpt_overlap = 0;
    MAKE_ARGV(av2, "rt", "--report-overlaps");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--report-overlaps", 1, rpt_overlap);
}


/* -R / --no-overlap-report */
static void
test_opt_no_overlap(void)
{
    extern int rpt_overlap;
    reset_globals();
    MAKE_ARGV(av, "rt", "-R");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-R rpt_overlap=0", 0, rpt_overlap);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--no-overlap-report");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--no-overlap-report", 0, rpt_overlap);
}


/* -W / --white-background */
static void
test_opt_white_bg(void)
{
    extern int default_background;
    reset_globals();
    MAKE_ARGV(av, "rt", "-W");
    CALL_GET_ARGS(av, 1);
    CHECK_INT("-W default_background=0", 0, default_background);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--white-background");
    CALL_GET_ARGS(av2, 1);
    CHECK_INT("--white-background", 0, default_background);
}


/* -C / --background (passes color to rt_do_cmd; verify it parses ok) */
static void
test_opt_bgcolor(void)
{
    extern int default_background;
    /* We just check it doesn't crash and returns success */
    reset_globals();
    MAKE_ARGV(av, "rt", "-C", "255/0/0");
    CALL_GET_ARGS(av, 1);

    reset_globals();
    MAKE_ARGV(av2, "rt", "--background", "0/128/255");
    CALL_GET_ARGS(av2, 1);
}


/* -h / -? : help flag - should return 0 */
static void
test_opt_help(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "-h");
    CALL_GET_ARGS(av, 0);   /* expect 0 = help */

    reset_globals();
    MAKE_ARGV(av2, "rt", "-?");
    CALL_GET_ARGS(av2, 0);

    reset_globals();
    MAKE_ARGV(av3, "rt", "--help");
    CALL_GET_ARGS(av3, 0);
}


/* Unknown option: should return -1 */
static void
test_opt_unknown(void)
{
    reset_globals();
    MAKE_ARGV(av, "rt", "--this-option-does-not-exist");
    CALL_GET_ARGS(av, -1);
}


/* -,  (disabled): should call bu_exit — we cannot test this directly
 * from a test harness without fork/subprocess, so we just document it. */


/* bu_optind boundary: model.g should be correctly identified */
static void
test_optind_boundary(void)
{
    extern int matflag;
    /* Options + model.g + object: bu_optind should point at argv[3] */
    reset_globals();
    {
	const char *av[] = { "rt", "-s", "64", "model.g", "all", NULL };
	int av_argc = 5;
	int ret = get_args(av_argc, av);
	g_total_tests++;
	if (ret != 1) {
	    bu_log("FAIL [bu_optind return]: expected 1, got %d\n", ret);
	    g_failed_tests++;
	}
	g_total_tests++;
	if (bu_optind != 3) {
	    bu_log("FAIL [bu_optind after -s 64 model.g all]: expected 3, got %d\n", bu_optind);
	    g_failed_tests++;
	}
    }

    /* No options: bu_optind == 1 */
    reset_globals();
    {
	const char *av2[] = { "rt", "model.g", NULL };
	int av2_argc = 2;
	get_args(av2_argc, av2);
	g_total_tests++;
	if (bu_optind != 1) {
	    bu_log("FAIL [bu_optind no-opts]: expected 1, got %d\n", bu_optind);
	    g_failed_tests++;
	}
    }

    /* All options, no model: bu_optind == argc */
    reset_globals();
    {
	const char *av3[] = { "rt", "-t", "-i", NULL };
	int av3_argc = 3;
	get_args(av3_argc, av3);
	g_total_tests++;
	if (bu_optind != 3) {
	    bu_log("FAIL [bu_optind all-opts]: expected 3, got %d\n", bu_optind);
	    g_failed_tests++;
	}
    }

    /* Long option + model */
    if (!g_short_only) {
	reset_globals();
	{
	    const char *av4[] = { "rt", "--size", "128", "scene.g", "all.r", NULL };
	    int av4_argc = 5;
	    get_args(av4_argc, av4);
	    g_total_tests++;
	    if (bu_optind != 3) {
		bu_log("FAIL [bu_optind --size 128 scene.g]: expected 3, got %d\n", bu_optind);
		g_failed_tests++;
	    }
	}
    }
}


/* Multiple options in one call (integration) */
static void
test_multiple_opts(void)
{
    extern size_t width, height;
    extern fastf_t azimuth, elevation;
    extern int benchmark;
    extern int top_down;

    if (g_short_only)
	return;

    reset_globals();
    {
	const char *av[] = {
	    "rt",
	    "--size", "256",
	    "--azimuth", "45",
	    "--elevation", "30",
	    "--top-down",
	    "--benchmark",
	    NULL
	};
	int av_argc = 9;
	int ret = get_args(av_argc, av);
	g_total_tests++;
	if (ret != 1) {
	    bu_log("FAIL [multi-opt return]: expected 1, got %d\n", ret);
	    g_failed_tests++;
	}
    }
    CHECK_INT("multi width",  256, (int)width);
    CHECK_INT("multi height", 256, (int)height);
    CHECK_DBL("multi azimuth", 45.0, azimuth);
    CHECK_DBL("multi elevation", 30.0, elevation);
    CHECK_INT("multi top_down", 1, top_down);
    CHECK_INT("multi benchmark", 1, benchmark);
}


/* -----------------------------------------------------------------------
 * Test table
 * ----------------------------------------------------------------------- */

struct rt_opt_test_entry {
    const char *name;
    void (*func)(void);
};

static struct rt_opt_test_entry all_tests[] = {
    { "size",            test_opt_size            },
    { "width",           test_opt_width           },
    { "height",          test_opt_height          },
    { "output",          test_opt_output          },
    { "output_double",   test_opt_output_double   },
    { "framebuffer",     test_opt_framebuffer     },
    { "azimuth",         test_opt_azimuth         },
    { "elevation",       test_opt_elevation       },
    { "perspective",     test_opt_perspective     },
    { "eye_backoff",     test_opt_eye_backoff     },
    { "view_aspect",     test_opt_view_aspect     },
    { "matflag",         test_opt_matflag         },
    { "cell_width",      test_opt_cell_width      },
    { "cell_height",     test_opt_cell_height     },
    { "subgrid",         test_opt_subgrid         },
    { "cut_plane",       test_opt_cut_plane       },
    { "ambient",         test_opt_ambient         },
    { "light_model",     test_opt_light_model     },
    { "haze",            test_opt_haze            },
    { "incremental",     test_opt_incremental     },
    { "topdown",         test_opt_topdown         },
    { "stereo",          test_opt_stereo          },
    { "hypersample",     test_opt_hypersample     },
    { "jitter",          test_opt_jitter          },
    { "units",           test_opt_units           },
    { "use_air",         test_opt_use_air         },
    { "tolerance",       test_opt_tolerance       },
    { "opencl",          test_opt_opencl          },
    { "command",         test_opt_command         },
    { "density_file",    test_opt_density_file    },
    { "start_frame",     test_opt_start_frame     },
    { "end_frame",       test_opt_end_frame       },
    { "single_pixel",    test_opt_single_pixel    },
    { "query_pixel",     test_opt_query_pixel     },
    { "objects_file",    test_opt_objects_file    },
    { "plus_text",       test_opt_plus            },
    { "rand_table",      test_opt_rand_table      },
    { "verbose",         test_opt_verbose         },
    { "rt_debug",        test_opt_rt_debug        },
    { "optical_debug",   test_opt_optical_debug   },
    { "nmg_debug",       test_opt_nmg_debug       },
    { "bu_debug",        test_opt_bu_debug        },
    { "cpus",            test_opt_cpus            },
    { "benchmark",       test_opt_benchmark       },
    { "report_overlaps", test_opt_report_overlaps },
    { "no_overlap",      test_opt_no_overlap      },
    { "white_bg",        test_opt_white_bg        },
    { "bgcolor",         test_opt_bgcolor         },
    { "help",            test_opt_help            },
    { "unknown_opt",     test_opt_unknown         },
    { "optind_boundary", test_optind_boundary     },
    { "multiple_opts",   test_multiple_opts       },
    { NULL, NULL }
};


/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
    int i;
    const char *filter = NULL;
    int found = 0;

    bu_setprogname(argv[0]);

    for (i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--short-only")) {
	    g_short_only = 1;
	    continue;
	}
	if (BU_STR_EQUAL(argv[i], "-h") || BU_STR_EQUAL(argv[i], "--help")) {
	    int j;
	    bu_log("Usage: %s [--short-only] [test_name]\n", argv[0]);
	    bu_log("  --short-only   run only short-option checks (skip long-option invocations)\n");
	    bu_log("  test_name      run only one named test from the list below\n");
	    bu_log("Available tests:\n");
	    for (j = 0; all_tests[j].name; j++)
		bu_log("  %s\n", all_tests[j].name);
	    return 0;
	}
	if (!filter) {
	    filter = argv[i];
	    continue;
	}
	bu_log("ERROR: too many arguments\n");
	return 1;
    }

    if (g_short_only)
	bu_log("Mode: short-only\n");

    for (i = 0; all_tests[i].name != NULL; i++) {
	if (filter && !BU_STR_EQUAL(all_tests[i].name, filter))
	    continue;
	found = 1;
	bu_log("  running: %s\n", all_tests[i].name);
	all_tests[i].func();
    }

    if (filter && !found) {
	bu_log("ERROR: unknown test '%s'\n", filter);
	bu_log("Available tests:\n");
	for (i = 0; all_tests[i].name; i++)
	    bu_log("  %s\n", all_tests[i].name);
	return 1;
    }

    if (g_failed_tests == 0) {
	bu_log("ALL %d TESTS PASSED\n", g_total_tests);
    } else {
	bu_log("%d/%d TESTS FAILED\n", g_failed_tests, g_total_tests);
    }
    return g_failed_tests;
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
