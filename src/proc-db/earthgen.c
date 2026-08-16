/*                      E A R T H G E N . C
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
/** @file proc-db/earthgen.c
 *
 * Generate a BRL-CAD .g model of Earth from real elevation data.
 *
 * Reads a global elevation raster (ETOPO 2022 GeoTIFF or any
 * GDAL-readable DEM) and constructs a cubed-sphere globe from six
 * DSP (displacement-map) height-field primitives.  Each face is
 * reprojected via a gnomonic projection centered on the face,
 * creating a tangent-plane tile positioned on the sphere.  A water
 * sphere at sea-level radius is boolean-subtracted from the terrain
 * to create distinct ocean geometry.
 *
 * Vertical exaggeration defaults to 40x so terrain features are
 * visible at globe scale (Earth's actual topographic relief is less
 * than 0.2% of the radius).  Use --exaggeration 1 for true scale.
 *
 * All coordinates are millimeters (BRL-CAD convention).
 *
 * Usage:
 *   earthgen input.tif output.g [--dim N] [--exaggeration F]
 *
 * Example:
 *   earthgen ETOPO_2022_v1_60s_N90W180_surface.tif earth.g
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/cv.h"
#include "bu/env.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bn.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "gdal.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "cpl_conv.h"
#include "ogr_srs_api.h"


/* WGS-84 volumetric mean Earth radius in metres. */
#define EARTH_R_M 6371000.0

/* Metres to millimetres (BRL-CAD works in mm). */
#define M2MM 1000.0

#define NFACES 6
#define U16MAX 65535

/* Default vertical exaggeration.  At 1x, Everest is 0.14% of R and
 * invisible at globe scale.  40x keeps the exaggerated range within
 * the uint16 budget at dim=900 while making features clearly visible.
 */
#define DEFAULT_EXAG 40.0

/* Default samples per cube-face edge. */
#define DEFAULT_DIM 900


/*
 * Cube-face definitions.  Each face of the cubed-sphere is a gnomonic
 * projection centered on (lat_0, lon_0).  The six faces tile the
 * entire globe.
 */
struct cube_face {
    const char *tag;
    double      lat_0;
    double      lon_0;
};

static const struct cube_face faces[NFACES] = {
    { "px",   0.0,    0.0 },   /* +X  prime meridian / equator  */
    { "nx",   0.0,  180.0 },   /* -X  antimeridian / equator    */
    { "py",   0.0,   90.0 },   /* +Y  90 E / equator            */
    { "ny",   0.0,  -90.0 },   /* -Y  90 W / equator            */
    { "pz",  90.0,    0.0 },   /* +Z  north pole                */
    { "nz", -90.0,    0.0 },   /* -Z  south pole                */
};


/**
 * Compute East / North / Up orthonormal frame at a geographic
 * position, expressed in ECEF Cartesian axes.
 *
 *   East  = d/d(lon)  tangent direction
 *   North = d/d(lat)  tangent direction
 *   Up    = radial outward normal
 *
 * The triple (E, N, U) is right-handed: E x N = U.
 */
static void
geo_frame(double lat_deg, double lon_deg,
	  vect_t east, vect_t north, vect_t up)
{
    double la = lat_deg * DEG2RAD;
    double lo = lon_deg * DEG2RAD;
    double sla = sin(la), cla = cos(la);
    double slo = sin(lo), clo = cos(lo);

    VSET(east,  -slo,        clo,        0.0);
    VSET(north, -sla * clo, -sla * slo,  cla);
    VSET(up,     cla * clo,  cla * slo,  sla);
}


/**
 * Build the DSP solid-to-model matrix for one cube face.
 *
 * The matrix maps DSP-local coordinates (cell_x, cell_y, height_z)
 * into world ECEF coordinates (mm).
 *
 * Scaling is uniform in all three axes (@a cell_mm) so that surface
 * normals transform correctly under the existing DSP code, which
 * uses dsp_stom directly (rather than the inverse transpose) for
 * normal mapping.
 *
 * The resulting coordinate mapping is:
 *   world = cell_mm * (lx * E + ly * N + lz * U)
 *         + (-hw * E  -  hw * N  +  z_base * U)
 *
 * where hw = cell_mm * (dim-1) / 2  is the half-width of the face.
 */
static void
build_stom(double lat_deg, double lon_deg, unsigned int dim,
	   double cell_x_mm, double cell_y_mm, double cell_z_mm,
	   double z_base_mm, mat_t stom)
{
    vect_t e, n, u;
    double hw_x, hw_y;

    geo_frame(lat_deg, lon_deg, e, n, u);
    hw_x = cell_x_mm * (dim - 1) * 0.5;
    hw_y = cell_y_mm * (dim - 1) * 0.5;

    MAT_IDN(stom);

    /* column 0: x -> East */
    stom[0] = cell_x_mm * e[X];
    stom[4] = cell_x_mm * e[Y];
    stom[8] = cell_x_mm * e[Z];

    /* column 1: y -> North */
    stom[1] = cell_y_mm * n[X];
    stom[5] = cell_y_mm * n[Y];
    stom[9] = cell_y_mm * n[Z];

    /* column 2: z -> Up (radial) */
    stom[2]  = cell_z_mm * u[X];
    stom[6]  = cell_z_mm * u[Y];
    stom[10] = cell_z_mm * u[Z];

    /* column 3: translation (face origin on the sphere) */
    stom[3]  = -hw_x * e[X] - hw_y * n[X] + z_base_mm * u[X];
    stom[7]  = -hw_x * e[Y] - hw_y * n[Y] + z_base_mm * u[Y];
    stom[11] = -hw_x * e[Z] - hw_y * n[Z] + z_base_mm * u[Z];
}


/**
 * Warp the source raster to a gnomonic projection centred on a cube
 * face.  The output covers +/- extent_m on the tangent plane,
 * resampled to dim x dim pixels.
 *
 * Returns an in-memory GDAL dataset (caller must GDALClose), or NULL.
 */
static GDALDatasetH
warp_face(GDALDatasetH src, double lat_0, double lon_0,
	  int dim, double extent_m)
{
    char proj[256];
    char s_xmin[32], s_ymin[32], s_xmax[32], s_ymax[32];
    char s_w[16], s_h[16];
    const char *args[15];
    GDALWarpAppOptions *opts;
    GDALDatasetH src_arr[1];
    GDALDatasetH result;
    int err = 0;

    snprintf(proj, sizeof(proj),
	     "+proj=ortho +lat_0=%.6f +lon_0=%.6f "
	     "+x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs",
	     lat_0, lon_0);

    snprintf(s_xmin, sizeof(s_xmin), "%.1f", -extent_m);
    snprintf(s_ymin, sizeof(s_ymin), "%.1f", -extent_m);
    snprintf(s_xmax, sizeof(s_xmax), "%.1f",  extent_m);
    snprintf(s_ymax, sizeof(s_ymax), "%.1f",  extent_m);
    snprintf(s_w, sizeof(s_w), "%d", dim);
    snprintf(s_h, sizeof(s_h), "%d", dim);

    args[0]  = "-t_srs";   args[1]  = proj;
    args[2]  = "-te";
    args[3]  = s_xmin;     args[4]  = s_ymin;
    args[5]  = s_xmax;     args[6]  = s_ymax;
    args[7]  = "-ts";      args[8]  = s_w;     args[9] = s_h;
    args[10] = "-r";       args[11] = "bilinear";
    args[12] = "-of";      args[13] = "MEM";
    args[14] = NULL;

    opts = GDALWarpAppOptionsNew((char **)args, NULL);
    if (!opts) return NULL;

    src_arr[0] = src;
    result = GDALWarp("", NULL, 1, src_arr, opts, &err);
    GDALWarpAppOptionsFree(opts);

    return result;
}


/**
 * Process one cube face: warp source data, read elevation, apply
 * vertical exaggeration, quantize to uint16, and write BINUNIF +
 * DSP objects to the database.
 *
 * @param src          Open GDAL source dataset (global DEM)
 * @param wdbp         Open BRL-CAD database handle
 * @param face         Cube face definition
 * @param dim          Samples per face edge
 * @param raw_min_m    Global raw elevation minimum (metres)
 * @param exag         Vertical exaggeration factor
 * @param cell_m       Uniform cell spacing (metres, same for all axes)
 * @param radius_mm    Earth radius in mm
 *
 * @return 0 on success, -1 on failure.
 */
static int
process_face(GDALDatasetH src, struct rt_wdb *wdbp,
	     const struct cube_face *face, unsigned int dim,
	     double raw_min_m, double exag,
	     double cell_m, double cell_z_m, double radius_mm)
{
    char name_data[64], name_solid[64], name_pyr[64], name_comb[64];
    GDALDatasetH warped;
    GDALRasterBandH band;
    int has_nodata = 0;
    double nodata_val = 0.0;
    double *elev;
    unsigned short *grid;
    size_t count;
    unsigned int row, col;

    double cell_mm   = cell_m * M2MM;
    double cell_z_mm = cell_z_m * M2MM;
    
    /* Z_base is chosen deep enough to intersect the water sphere
     * cleanly and to bound the valid region from the origin.
     * For an inscribed cube wedge, the minimum Z is R/sqrt(3).
     */
    double R_m = radius_mm / M2MM;
    double z_base_m  = R_m / sqrt(3.0);
    double z_base_mm = z_base_m * M2MM;

    struct rt_dsp_internal *dsp;

    snprintf(name_data,  sizeof(name_data),  "face_%s.data", face->tag);
    snprintf(name_solid, sizeof(name_solid), "face_%s.s",    face->tag);
    snprintf(name_pyr,   sizeof(name_pyr),   "pyr_%s.s",     face->tag);
    snprintf(name_comb,  sizeof(name_comb),  "face_%s.c",    face->tag);

    bu_log("earthgen: face %s  center (%.1f, %.1f) ...\n",
	   face->tag, face->lat_0, face->lon_0);

    /* ---- Warp source data to orthographic for this face. ---- */
    /* extent_m is R / sqrt(2) which is computed in main() as cell_m * (dim-1) / 2 */
    double extent_m = cell_m * (dim - 1) * 0.5;
    warped = warp_face(src, face->lat_0, face->lon_0,
		       (int)dim, extent_m);
    if (!warped) {
	bu_log("earthgen: warp failed for face %s\n", face->tag);
	return -1;
    }

    count = (size_t)dim * (size_t)dim;
    elev = (double *)bu_calloc(count, sizeof(double), "elevation");
    grid = (unsigned short *)bu_calloc(count, sizeof(unsigned short),
				       "dsp grid");

    band = GDALGetRasterBand(warped, 1);
    nodata_val = GDALGetRasterNoDataValue(band, &has_nodata);

    /* Read elevation row-by-row, flipping Y axis.
     * GDAL: y=0 is top (north).  DSP: y=0 is bottom (south). */
    for (row = 0; row < dim; row++) {
	if (GDALRasterIO(band, GF_Read,
			 0, (int)(dim - 1 - row),
			 (int)dim, 1,
			 &elev[(size_t)row * dim],
			 (int)dim, 1,
			 GDT_Float64, 0, 0) != CE_None) {
	    bu_log("earthgen: read error face %s row %u\n",
		   face->tag, row);
	}
    }
    GDALClose(warped);

    /* ---- Compute radial displacement and quantize. ---- */
    for (row = 0; row < dim; row++) {
        double y = -extent_m + row * cell_m;
        for (col = 0; col < dim; col++) {
            double x = -extent_m + col * cell_m;
            size_t k = (size_t)row * dim + col;
            double h = elev[k];
            double z_val, z_dsp;
            long v;
            double r_sq = x*x + y*y;

            if (has_nodata && NEAR_EQUAL(h, nodata_val, 1.0))
                h = 0.0;

            if (r_sq >= R_m * R_m) {
                z_val = z_base_m;
            } else {
                double R_elev = R_m + h * exag;
                double under_sqrt = R_elev * R_elev - r_sq;
                if (under_sqrt < 0.0) under_sqrt = 0.0;
                z_val = sqrt(under_sqrt);
            }

            if (z_val < z_base_m) z_val = z_base_m;
            
            z_dsp = z_val - z_base_m;
            v = (long)(z_dsp / cell_z_m + 0.5);

            if (v < 0)      v = 0;
            if (v > U16MAX) v = U16MAX;
            grid[k] = (unsigned short)v;
        }
    }
    bu_free(elev, "elevation");

    /* Convert to network (big-endian) byte order. */
    {
	int in_c  = bu_cv_cookie("hus");
	int out_c = bu_cv_cookie("nus");
	if (bu_cv_optimize(in_c) != bu_cv_optimize(out_c)) {
	    bu_cv_w_cookie(grid, out_c,
			   count * sizeof(unsigned short),
			   grid, in_c, count);
	}
    }

    /* Write the height data as a BINUNIF object in the .g. */
    mk_binunif(wdbp, name_data, (void *)grid, WDB_BINUNIF_UINT16, count);
    bu_free(grid, "dsp grid");

    /* ---- Create the DSP primitive. ---- */
    BU_ALLOC(dsp, struct rt_dsp_internal);
    dsp->magic       = RT_DSP_INTERNAL_MAGIC;
    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, name_data);
    dsp->dsp_datasrc = RT_DSP_SRC_OBJ;
    dsp->dsp_xcnt    = dim;
    dsp->dsp_ycnt    = dim;
    dsp->dsp_smooth  = 1;
    dsp->dsp_cuttype = DSP_CUT_DIR_ADAPT;

    build_stom(face->lat_0, face->lon_0, dim,
	       cell_mm, cell_mm, cell_z_mm, z_base_mm, dsp->dsp_stom);
    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);

    wdb_export(wdbp, name_solid, (void *)dsp, ID_DSP, 1);

    /* ---- Create the Wedge Pyramid (arb5). ----
     * The wedge bounds exactly the 1/6th volume for this face.
     * Apex is at origin. Base is a square far outside the globe.
     */
    {
        point_t pts[5];
        vect_t e, n, u;
        double D = 2.0 * radius_mm; /* Far outside */
        
        geo_frame(face->lat_0, face->lon_0, e, n, u);
        
        /* Base vertices (CCW from outside looking in to origin) */
        VSET(pts[0], D * (u[X] + e[X] + n[X]), D * (u[Y] + e[Y] + n[Y]), D * (u[Z] + e[Z] + n[Z]));
        VSET(pts[1], D * (u[X] - e[X] + n[X]), D * (u[Y] - e[Y] + n[Y]), D * (u[Z] - e[Z] + n[Z]));
        VSET(pts[2], D * (u[X] - e[X] - n[X]), D * (u[Y] - e[Y] - n[Y]), D * (u[Z] - e[Z] - n[Z]));
        VSET(pts[3], D * (u[X] + e[X] - n[X]), D * (u[Y] + e[Y] - n[Y]), D * (u[Z] + e[Z] - n[Z]));
        /* Apex */
        VSET(pts[4], 0.0, 0.0, 0.0);
        
        mk_arb5(wdbp, name_pyr, (const fastf_t *)pts);
    }

    /* ---- Boolean intersection: DSP + Pyramid ---- */
    {
        struct wmember comb_hd;
        BU_LIST_INIT(&comb_hd.l);
        (void)mk_addmember(name_solid, &comb_hd.l, NULL, WMOP_UNION);
        (void)mk_addmember(name_pyr,   &comb_hd.l, NULL, WMOP_INTERSECT);
        mk_lcomb(wdbp, name_comb, &comb_hd, 0, NULL, NULL, NULL, 0);
    }

    bu_log("earthgen: face %s  %ux%u  cell %.1f km\n",
	   face->tag, dim, dim, cell_m / 1000.0);
    return 0;
}


int
main(int ac, char *av[])
{
    struct rt_wdb *wdbp;
    GDALDatasetH src;
    GDALRasterBandH band;
    double mm[2];
    int bmin_ok = 0, bmax_ok = 0;
    double raw_min, raw_max, raw_range;
    unsigned int dim = DEFAULT_DIM;
    double exag = DEFAULT_EXAG;
    double radius_mm;
    double cell_m, cell_z_m;
    int i;

    const char *input_path;
    const char *output_path;

    struct wmember terr_hd, water_hd, all_hd;
    unsigned char land_rgb[3]  = { 110, 130, 80 };
    unsigned char water_rgb[3] = {  30,  80, 160 };
    point_t center;

    bu_setprogname(av[0]);

    if (ac < 3) {
	bu_exit(1,
		"Usage: %s input.tif output.g [--dim N] [--exaggeration F]\n\n"
		"  input.tif        Global DEM raster (e.g. ETOPO 2022 GeoTIFF)\n"
		"  output.g         Output BRL-CAD database\n"
		"  --dim N          Samples per cube-face edge  (default %d)\n"
		"  --exaggeration F Vertical scale factor        (default %.0f)\n",
		av[0], DEFAULT_DIM, DEFAULT_EXAG);
    }
    input_path  = av[1];
    output_path = av[2];

    for (i = 3; i + 1 < ac; i += 2) {
	if (BU_STR_EQUAL(av[i], "--dim"))
	    dim = (unsigned int)atoi(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "--exaggeration"))
	    exag = atof(av[i + 1]);
    }

    if (dim < 64)    dim = 64;
    if (dim > 4096)  dim = 4096;
    if (exag < 1.0)  exag = 1.0;
    if (exag > 200.0) exag = 200.0;

    radius_mm = EARTH_R_M * M2MM;

    /* Ensure GDAL's PROJ engine can find its data files. */
    if (!getenv("PROJ_LIB"))
	bu_setenv("PROJ_LIB",
		  bu_dir(NULL, 0, BU_DIR_DATA, "proj", NULL), 1);

    GDALAllRegister();

    /* ---- Open the source elevation raster. ---- */
    src = GDALOpenEx(input_path,
		     GDAL_OF_READONLY | GDAL_OF_RASTER
		     | GDAL_OF_VERBOSE_ERROR,
		     NULL, NULL, NULL);
    if (!src)
	bu_exit(2, "earthgen: cannot open '%s'\n", input_path);

    /* Query the global elevation range from band statistics. */
    band = GDALGetRasterBand(src, 1);
    mm[0] = GDALGetRasterMinimum(band, &bmin_ok);
    mm[1] = GDALGetRasterMaximum(band, &bmax_ok);
    if (!bmin_ok || !bmax_ok)
	GDALComputeRasterMinMax(band, TRUE, mm);

    raw_min   = mm[0];
    raw_max   = mm[1];
    raw_range = raw_max - raw_min;
    if (raw_range < 1.0) raw_range = 1.0;

    bu_log("earthgen: elevation %.1f .. %.1f m   (x%.0f)\n",
	   raw_min, raw_max, exag);

    /* Uniform cell spacing: the orthographic face spans +/- R/sqrt(2) on the
     * tangent plane.
     */
    double extent_m = EARTH_R_M * M_SQRT1_2;
    cell_m = 2.0 * extent_m / (dim - 1);

    /* Z quantization: we map the range from R/sqrt(3) to the max
     * exaggerated mountain peak into uint16.
     */
    double z_base_m = EARTH_R_M / sqrt(3.0);
    double z_max_m = EARTH_R_M + raw_max * exag;
    double span_z_m = z_max_m - z_base_m;
    
    cell_z_m = span_z_m / (double)U16MAX;

    bu_log("earthgen: dim=%u  cell_xy=%.1f km  cell_z=%.1f m  exag=%.0fx\n",
	   dim, cell_m / 1000.0, cell_z_m, exag);

    /* ---- Open the output .g database. ---- */
    wdbp = wdb_fopen(output_path);
    if (!wdbp) {
	GDALClose(src);
	bu_exit(3, "earthgen: cannot create '%s'\n", output_path);
    }
    mk_id_units(wdbp, "Earth Terrain Model", "mm");

    /* ---- Process each cube face. ---- */
    for (i = 0; i < NFACES; i++) {
	if (process_face(src, wdbp, &faces[i], dim,
			 raw_min, exag,
			 cell_m, cell_z_m, radius_mm) != 0) {
	    bu_log("earthgen: WARNING - face %s failed\n",
		   faces[i].tag);
	}
    }
    GDALClose(src);

    /* ---- Terrain region: union of all six DSP faces. ---- */
    BU_LIST_INIT(&terr_hd.l);
    for (i = 0; i < NFACES; i++) {
	char nm[64];
	snprintf(nm, sizeof(nm), "face_%s.c", faces[i].tag);
	(void)mk_addmember(nm, &terr_hd.l, NULL, WMOP_UNION);
    }
    mk_lcomb(wdbp, "terrain.r", &terr_hd, 1,
	     "plastic", "di=0.8 sp=0.1", land_rgb, 0);

    /* ---- Water sphere at sea-level radius. ----
     * Subtract all terrain faces so that water occupies only the
     * volume inside the sphere that is outside the DSP solids.
     * This correctly shows ocean surfaces over below-sea-level
     * terrain and fills the gaps between the flat faces.
     */
    VSET(center, 0.0, 0.0, 0.0);
    mk_sph(wdbp, "water.s", center, radius_mm);

    BU_LIST_INIT(&water_hd.l);
    (void)mk_addmember("water.s", &water_hd.l, NULL, WMOP_UNION);
    for (i = 0; i < NFACES; i++) {
	char nm[64];
	snprintf(nm, sizeof(nm), "face_%s.c", faces[i].tag);
	(void)mk_addmember(nm, &water_hd.l, NULL, WMOP_SUBTRACT);
    }
    mk_lcomb(wdbp, "water.r", &water_hd, 1,
	     "plastic", "di=0.7 sp=0.3", water_rgb, 0);

    /* ---- Top-level assembly. ---- */
    BU_LIST_INIT(&all_hd.l);
    (void)mk_addmember("terrain.r", &all_hd.l, NULL, WMOP_UNION);
    (void)mk_addmember("water.r",   &all_hd.l, NULL, WMOP_UNION);
    mk_lcomb(wdbp, "earth.all", &all_hd, 0, NULL, NULL, NULL, 0);

    bu_log("earthgen: wrote %s  (top-level: 'earth.all')\n",
	   output_path);

    db_close(wdbp->dbip);
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
