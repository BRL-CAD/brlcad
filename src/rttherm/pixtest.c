/*                       P I X T E S T . C
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
/** @file rttherm/pixtest.c
 *
 * This is a tool for testing the spectral conversion routines and the
 * underlying libraries.  Take an RGB .pix file, convert it to
 * spectral form, then sample it back to RGB and output it as a .pix
 * file.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>


#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "spectrum.h"


extern void make_ntsc_xyz2rgb(mat_t xyz2rgb);
extern void
spect_curve_to_xyz(point_t xyz,
		   const struct bn_tabdata *tabp,
		   const struct bn_tabdata *cie_x,
		   const struct bn_tabdata *cie_y,
		   const struct bn_tabdata *cie_z);

extern struct bn_table *spectrum;
struct bn_tabdata *curve;

#if 0
/* Not many samples in visible part of spectrum */
int nsamp = 100;
double min_nm = 380;
double max_nm = 12000;
#else
int nsamp = 20;
double min_nm = 340;
double max_nm = 760;
#endif

struct bn_tabdata *cie_x;
struct bn_tabdata *cie_y;
struct bn_tabdata *cie_z;

mat_t xyz2rgb;

int
main(int ac, char **av)
{
    unsigned char rgb[4];
    float src[3];
    point_t dest;
    point_t xyz;
    size_t ret;

    if (ac > 1)
	bu_log("Usage: %s\n", av[0]);

    spectrum = bn_table_make_uniform(nsamp, min_nm, max_nm);
    BN_GET_TABDATA(curve, spectrum);

    rt_spect_make_CIE_XYZ(&cie_x, &cie_y, &cie_z, spectrum);
    make_ntsc_xyz2rgb(xyz2rgb);

    for (;;) {
	if (fread(rgb, 1, 3, stdin) != 3) break;
	if (feof(stdin)) break;

	VSET(src, rgb[0]/255., rgb[1]/255., rgb[2]/255.);

	rt_spect_reflectance_rgb(curve, src);

	spect_curve_to_xyz(xyz, curve, cie_x, cie_y, cie_z);

	MAT3X3VEC(dest, xyz2rgb, xyz);

	if (dest[0] > 1 || dest[1] > 1 || dest[2] > 1 ||
	    dest[0] < 0 || dest[1] < 0 || dest[2] < 0) {
	    VPRINT("src ", src);
	    VPRINT("dest", dest);
	}

	if (dest[0] > 1) dest[0] = 1;
	if (dest[1] > 1) dest[1] = 1;
	if (dest[2] > 1) dest[2] = 1;
	if (dest[0] < 0) dest[0] = 0;
	if (dest[1] < 0) dest[1] = 0;
	if (dest[2] < 0) dest[2] = 0;

	VSCALE(rgb, dest, 255.0);

	ret = fwrite(rgb, 1, 3, stdout);
	if (ret != 3)
	    perror("fwrite");
    }

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
