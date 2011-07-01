/*                      S P E C T R U M . C
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
/** @file rttherm/spectrum.c
 *
 * These are all the routines that are not ready to be moved to
 * LIBRT/spectrum.c
 *
 * An application of the 'tabdata' package to spectral data.
 *
 * Inspired by -
 * Roy Hall and his book "Illumination and Color in Computer
 * Generated Imagery", Springer Verlag, New York, 1989.
 * ISBN 0-387-96774-5
 *
 * With thanks to Russ Moulton Jr, EOSoft Inc. for his "rad.c" module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "vmath.h"
#include "raytrace.h"
#include "spectrum.h"

static const double rt_NTSC_R[][2] = {
    {543, 0.0001},
    {552, 0.05},
    {562, 0.2},
    {572, 0.4},
    {577, 0.6},
    {580, 0.8},
    {583, 0.9},
    {587, 0.95},
    {590, 1.0},
    {598, 1.0},
    {601, 0.95},
    {605, 0.9},
    {610, 0.8},
    {620, 0.6},
    {630, 0.4},
    {640, 0.26},
    {650, 0.2},
    {670, 0.1},
    {690, 0.05},
    {730, 0.0001},
    {-1, -1}
};


static const double rt_NTSC_G[][2] = {
    {456, 0.0001},
    {475, 0.05},
    {480, 0.1},
    {492, 0.2},
    {507, 0.4},
    {515, 0.6},
    {522, 0.8},
    {528, 0.9},
    {531, 0.95},
    {537, 1.0},
    {545, 1.0},
    {548, 0.95},
    {551, 0.9},
    {555, 0.8},
    {562, 0.6},
    {572, 0.4},
    {582, 0.2},
    {591, 0.1},
    {603, 0.05},
    {630, 0.0001},
    {-1, -1}
};


static const double rt_NTSC_B[][2] = {
    {347, 0.0001},
    {373, 0.05},
    {385, 0.1},
    {396, 0.2},
    {409, 0.4},
    {415, 0.6},
    {423, 0.8},
    {430, 0.9},
    {433, 0.95},
    {440, 1.0},
    {450, 1.0},
    {457, 0.95},
    {460, 0.9},
    {466, 0.8},
    {470, 0.6},
    {479, 0.4},
    {492, 0.2},
    {503, 0.1},
    {515, 0.05},
    {543, 0.0001},
    {-1, -1}
};


struct bn_tabdata *rt_NTSC_r_tabdata;
struct bn_tabdata *rt_NTSC_g_tabdata;
struct bn_tabdata *rt_NTSC_b_tabdata;

/*
 * R T _ S P E C T _ M A K E _ N T S C _ R G B
 *
 * Using the "Representative set of camera taking sensitivities"
 * for a NTSC television camera, from Benson "Television Engineering
 * Handbook" page 4.58, convert an RGB value in range 0..1 to
 * a spectral curve also in range 0..1.
 *
 * These curves should be used in converting spectral samples
 * to NTSC RGB values.
 */
void
spect_make_NTSC_RGB(struct bn_tabdata **rp,
		    struct bn_tabdata **gp,
		    struct bn_tabdata **bp,
		    const struct bn_table *tabp)
{
    BN_CK_TABLE(tabp);

    /* Convert array of number pairs into bn_tabdata & bn_table */
    rt_NTSC_r_tabdata = bn_tabdata_from_array(&rt_NTSC_R[0][0]);
    rt_NTSC_g_tabdata = bn_tabdata_from_array(&rt_NTSC_G[0][0]);
    rt_NTSC_b_tabdata = bn_tabdata_from_array(&rt_NTSC_B[0][0]);

    bu_log("ntsc_R: area=%g\n", bn_tabdata_area2(rt_NTSC_r_tabdata));
    bn_print_table_and_tabdata("/dev/tty", rt_NTSC_r_tabdata);
    bu_log("ntsc_G: area=%g\n", bn_tabdata_area2(rt_NTSC_g_tabdata));
    bn_print_table_and_tabdata("/dev/tty", rt_NTSC_g_tabdata);
    bu_log("ntsc_B: area=%g\n", bn_tabdata_area2(rt_NTSC_b_tabdata));
    bn_print_table_and_tabdata("/dev/tty", rt_NTSC_b_tabdata);

    /* Resample original NTSC curves to match given bn_table sampling */
#if 0
    /* just to test the routine */
    *rp = bn_tabdata_resample_avg(tabp, rt_NTSC_r_tabdata);
    *gp = bn_tabdata_resample_avg(tabp, rt_NTSC_g_tabdata);
    *bp = bn_tabdata_resample_avg(tabp, rt_NTSC_b_tabdata);
#else
    /* use this one for real */
    *rp = bn_tabdata_resample_max(tabp, rt_NTSC_r_tabdata);
    *gp = bn_tabdata_resample_max(tabp, rt_NTSC_g_tabdata);
    *bp = bn_tabdata_resample_max(tabp, rt_NTSC_b_tabdata);
#endif
}


/*
 * These are the NTSC primaries with D6500 white point for use as
 * the default initialization as given in sect 5.1.1 Color
 * Correction for Display.
 * From Roy Hall, page 228.
 * Gives the XYZ coordinates of the NTSC primaries and D6500 white.
 * Note:  X+Y+Z=1 for primaries (cf. equations of pg.54)
 */
static const point_t rgb_NTSC[4] = {
    {0.670,     0.330,      0.000},     /* red */
    {0.210,     0.710,      0.080},     /* green */
    {0.140,     0.080,      0.780},     /* blue */
    {0.313,     0.329,      0.358}};    /* white */

/* ****************************************************************
 * clr__cspace_to_xyz (cspace, t_mat)
 * CLR_XYZ       cspace[4]   (in)  - the color space definition,
 *                                      3 primaries and white
 * double        t_mat[3][3] (mod) - the color transformation
 *
 * Builds the transformation from a set of primaries to the CIEXYZ
 * color space.  This is the basis for the generation of the color
 * transformations in the CLR_ routine set.  The method used is
 * that detailed in Sect 3.2 Colorimetry and the RGB monitor.
 * Returns RGB to XYZ matrix.
 * From Roy Hall, pg 239-240.
 *
 * The RGB white point of (1, 1, 1) times this matrix gives the
 * (Y=1 normalized) XYZ white point of (0.951368, 1, 1.08815)
 * From Roy Hall, pg 54.
 * MAT3X3VEC(xyz, rgb2xyz, rgb);
 *
 * Returns -
 * 0 if there is a singularity.
 * !0 if OK
 */
int
rt_clr__cspace_to_xyz (const point_t cspace[4],
		       mat_t rgb2xyz)
{
    int ii, jj, kk, tmp_i, ind[3];
    fastf_t mult, white[3], scale[3];
    mat_t t_mat;

    /* Might want to enforce X+Y+Z=1 for 4 inputs.  Roy does, on pg 229. */

    /* normalize the white point to Y=1 */
#define WHITE 3
    if (cspace[WHITE][Y] <= 0.0) return 0;
    white[0] = cspace[WHITE][X] / cspace[WHITE][Y];
    white[1] = 1.0;
    white[2] = cspace[WHITE][Z] / cspace[WHITE][Y];

#define tmat(a, b) t_mat[(a)*4+(b)]
    MAT_IDN(t_mat);
    for (ii=0; ii<=2; ii++) {
	tmat(0, ii) = cspace[ii][X];
	tmat(1, ii) = cspace[ii][Y];
	tmat(2, ii) = cspace[ii][Z];
	ind[ii] = ii;
    }

    /* gaussian elimination with partial pivoting */
    for (ii=0; ii<2; ii++) {
	for (jj=ii+1; jj<=2; jj++) {
	    if (fabs(tmat(ind[jj], ii)) > fabs(tmat(ind[ii], ii))) {
		tmp_i=ind[jj];
		ind[jj]=ind[ii];
		ind[ii]=tmp_i;
	    }
	}
	if (ZERO(tmat(ind[ii], ii))) return 0;

	for (jj=ii+1; jj<=2; jj++) {
	    mult = tmat(ind[jj], ii) / tmat(ind[ii], ii);
	    for (kk=ii+1; kk<=2; kk++)
		tmat(ind[jj], kk) -= tmat(ind[ii], kk) * mult;
	    white[ind[jj]] -= white[ind[ii]] * mult;
	}
    }
    if (ZERO(tmat(ind[2], 2))) return 0;

    /* back substitution to solve for scale */
    scale[ind[2]] = white[ind[2]] / tmat(ind[2], 2);
    scale[ind[1]] = (white[ind[1]] - (tmat(ind[1], 2) *
				      scale[ind[2]])) / tmat(ind[1], 1);
    scale[ind[0]] = (white[ind[0]] - (tmat(ind[0], 1) *
				      scale[ind[1]]) - (tmat(ind[0], 2) *
							scale[ind[2]])) / tmat(ind[0], 0);

    /* build matrix.  Embed 3x3 in BRL-CAD 4x4 */
    for (ii=0; ii<=2; ii++) {
	rgb2xyz[0*4+ii] = cspace[ii][X] * scale[ii];
	rgb2xyz[1*4+ii] = cspace[ii][Y] * scale[ii];
	rgb2xyz[2*4+ii] = cspace[ii][Z] * scale[ii];
	rgb2xyz[3*4+ii] = 0;
    }
    rgb2xyz[12] = rgb2xyz[13] = rgb2xyz[14];
    rgb2xyz[15] = 1;

    return 1;
}


/*
 * M A K E _ N T S C _ X Y Z 2 R G B
 *
 * Create the map from
 * CIE XYZ perceptual space into
 * an idealized RGB space assuming NTSC primaries with D6500 white.
 * Only high-quality television-studio monitors are like this, but...
 */
void
make_ntsc_xyz2rgb(fastf_t *xyz2rgb)
{
    mat_t rgb2xyz;
    point_t tst, new;

    if (rt_clr__cspace_to_xyz(rgb_NTSC, rgb2xyz) == 0)
	bu_exit(EXIT_FAILURE, "make_ntsc_xyz2rgb() can't initialize color space\n");
    bn_mat_inv(xyz2rgb, rgb2xyz);

#if 1
    /* Verify that it really works, I'm a skeptic */
    VSET(tst, 1, 1, 1);
    MAT3X3VEC(new, rgb2xyz, tst);
    VPRINT("white_rgb (i)", tst);
    VPRINT("white_xyz (o)", new);

    VSET(tst, 0.313,     0.329,      0.358);
    MAT3X3VEC(new, xyz2rgb, tst);
    VPRINT("white_xyz (i)", tst);
    VPRINT("white_rgb (o)", new);

    VSET(tst, 1, 0, 0);
    MAT3X3VEC(new, rgb2xyz, tst);
    VPRINT("red_rgb (i)", tst);
    VPRINT("red_xyz (o)", new);

    VSET(tst, 0.670,     0.330,      0.000);
    MAT3X3VEC(new, xyz2rgb, tst);
    VPRINT("red_xyz (i)", tst);
    VPRINT("red_rgb (o)", new);

    VSET(tst, 0, 1, 0);
    MAT3X3VEC(new, rgb2xyz, tst);
    VPRINT("grn_rgb (i)", tst);
    VPRINT("grn_xyz (o)", new);

    VSET(tst, 0.210,     0.710,      0.080);
    MAT3X3VEC(new, xyz2rgb, tst);
    VPRINT("grn_xyz (i)", tst);
    VPRINT("grn_rgb (o)", new);

    VSET(tst, 0, 0, 1);
    MAT3X3VEC(new, rgb2xyz, tst);
    VPRINT("blu_rgb (i)", tst);
    VPRINT("blu_xyz (o)", new);

    VSET(tst, 0.140,     0.080,      0.780);
    MAT3X3VEC(new, xyz2rgb, tst);
    VPRINT("blu_xyz (i)", tst);
    VPRINT("blu_rgb (o)", new);
#endif
}


/*
 * R T _ S P E C T _ C U R V E _ T O _ X Y Z
 *
 * Convenience routine.
 * Serves same function as Roy Hall's CLR_spect_to_xyz(), pg 233.
 * The normalization xyz_scale = 1.0 / bn_tabdata_area2(cie_y);
 * has been folded into spect_make_CIE_XYZ();
 */
void
spect_curve_to_xyz(point_t xyz,
		   const struct bn_tabdata *tabp,
		   const struct bn_tabdata *cie_x,
		   const struct bn_tabdata *cie_y,
		   const struct bn_tabdata *cie_z)
{
    fastf_t tab_area;

    BN_CK_TABDATA(tabp);

#if 0
    tab_area = bn_tabdata_area2(tabp);
    bu_log(" tab_area = %g\n", tab_area);
    if (fabs(tab_area) < VDIVIDE_TOL) {
	bu_log("spect_curve_to_xyz(): Area = 0 (no luminance) in this part of the spectrum\n");
	VSETALL(xyz, 0);
	return;
    }
    tab_area = 1 / tab_area;
#else
    /* This is what Roy says to do, but I'm not certain */
    tab_area = 1;
#endif

    xyz[X] = bn_tabdata_mul_area2(tabp, cie_x) * tab_area;
    xyz[Y] = bn_tabdata_mul_area2(tabp, cie_y) * tab_area;
    xyz[Z] = bn_tabdata_mul_area2(tabp, cie_z) * tab_area;
}


/*
 * R T _ S P E C T _ R G B _ T O _ C U R V E
 *
 * Using the "Representative set of camera taking sensitivities"
 * for a NTSC television camera, from Benson "Television Engineering
 * Handbook" page 4.58, convert an RGB value in range 0..1 to
 * a spectral curve also in range 0..1.
 *
 * XXX This is completely wrong, don't do this.
 */
void
spect_rgb_to_curve(struct bn_tabdata *tabp, const fastf_t *rgb, const struct bn_tabdata *ntsc_r, const struct bn_tabdata *ntsc_g, const struct bn_tabdata *ntsc_b)
{
    bn_tabdata_blend3(tabp,
		      rgb[0], ntsc_r,
		      rgb[1], ntsc_g,
		      rgb[2], ntsc_b);
}


/*
 * R T _ S P E C T _ X Y Z _ T O _ C U R V E
 *
 * Values of the curve will be normalized to 0..1 range;
 * caller must scale into meaningful units.
 *
 * Convenience routine.
 XXX This routine is probably wrong.  Or at least, it needs different curves.
 XXX Converting rgb to a curve, directly, should be easy.
*/
void
spect_xyz_to_curve(struct bn_tabdata *tabp, const fastf_t *xyz, const struct bn_tabdata *cie_x, const struct bn_tabdata *cie_y, const struct bn_tabdata *cie_z)
{
    bn_tabdata_blend3(tabp,
		      xyz[X], cie_x,
		      xyz[Y], cie_y,
		      xyz[Z], cie_z);
}


/*
 * R T _ T A B L E _ M A K E _ V I S I B L E _ A N D _ U N I F O R M
 *
 * A quick hack to make sure there are enough samples in the visible band.
 */
struct bn_table *
bn_table_make_visible_and_uniform(int num, double first, double last, int vis_nsamp)
{
    struct bn_table *new;
    struct bn_table *uniform;
    struct bn_table *vis;

    if (vis_nsamp < 10) vis_nsamp = 10;
    uniform = bn_table_make_uniform(num, first, last);
    vis = bn_table_make_uniform(vis_nsamp, 340.0, 700.0);

    new = bn_table_merge2(uniform, vis);
    bn_ck_table(new);

    bn_table_free(uniform);
    bn_table_free(vis);

    return new;
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
