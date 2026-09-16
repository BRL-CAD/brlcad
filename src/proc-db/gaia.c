/*                         G A I A . C
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
/** @file proc-db/gaia.c
 *
 * Generate a BRL-CAD .g model of Earth from real elevation data and
 * optional satellite imagery texture maps.
 *
 * Reads a global elevation raster (ETOPO 2022 GeoTIFF or any
 * GDAL-readable DEM) and constructs a cubed-sphere globe from six
 * DSP (displacement-map) height-field primitives.  Each face is
 * reprojected via an orthographic projection centered on the face,
 * creating a tangent-plane tile positioned on the sphere.  A water
 * sphere at sea-level radius is boolean-subtracted from the terrain
 * to create distinct ocean geometry.
 *
 * Optional day and night texture maps (NASA Blue Marble / Black Marble)
 * can be applied to the terrain faces.  When both textures are provided,
 * gaia generates day, night, and dusk (twilight terminator blended)
 * models.
 *
 * Vertical exaggeration defaults to 40x so terrain features are
 * visible at globe scale (Earth's actual topographic relief is less
 * than 0.2% of the radius).  Use --exaggeration 1 for true scale.
 *
 * All coordinates are millimeters (BRL-CAD convention).
 *
 * Usage:
 *   gaia input.tif output.g [options]
 *
 * Options:
 *   --dim N               DEM grid samples per face edge    (default 900)
 *   --exaggeration F      Vertical scale factor             (default 1.0)
 *   --texture-day FILE    Daytime color texture map (GeoTIFF/JPEG/PNG)
 *   --texture-night FILE  Nighttime city lights texture map (GeoTIFF/JPEG/PNG)
 *   --texture-dim N       Texture resolution per face edge  (default = dim)
 *   --sun-lon DEG         Subsolar longitude in degrees     (default 0.0)
 *   --sun-lat DEG         Subsolar latitude in degrees      (default 0.0)
 *
 * ======================================================================
 * OBTAINING TERRAIN & TEXTURE DATASETS
 * ======================================================================
 *
 * 1. Global Elevation (NOAA NCEI ETOPO 2022, Public Domain):
 *    - 60s Global DEM (444 MB):
 *      curl -L -o ETOPO_2022_v1_60s_N90W180_surface.tif "https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/60s/60s_surface_elev_gtif/ETOPO_2022_v1_60s_N90W180_surface.tif"
 *    - 30s Global DEM (1.5 GB):
 *      curl -L -o ETOPO_2022_v1_30s_N90W180_surface.tif "https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/30s/30s_surface_elev_gtif/ETOPO_2022_v1_30s_N90W180_surface.tif"
 *
 * 2. Day Satellite Imagery (NASA Visible Earth Blue Marble, Public Domain):
 *    curl -L -o earth_day.jpg "https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73580/world.topo.bathy.200401.3x5400x2700.jpg"
 *
 * 3. Night City Lights Imagery (NASA Earth at Night / Black Marble, Public Domain):
 *    curl -L -o earth_night.tif "https://eoimages.gsfc.nasa.gov/images/imagerecords/55000/55167/earth_lights_4800.tif"
 *
 * Examples:
 *
 * LOW FIDELITY (~20 MB downsampled raster w/o textures, 6x256x256 DSPs):
 *      gdal_translate -outsize 25% 25% -r bilinear ETOPO_2022_v1_60s_N90W180_surface.tif earth_low.tif
 *      gaia earth_low.tif earth_low.g --dim 256 --exaggeration 40
 *
 * MEDIUM FIDELITY (~444 MB GeoTIFF w/ day texture, 600x600x6 DSPs):
 *      gaia ETOPO_2022_v1_60s_N90W180_surface.tif earth_medium.g --dim 600 --exaggeration 40 \
 *        --texture-day earth_day.jpg
 *
 * HIGH FIDELITY (~1.5 GB GeoTIFF w/ day+night textures, 1200x1200x6 DSPs):
 *      gaia ETOPO_2022_v1_30s_N90W180_surface.tif earth_high.g --dim 1200 --exaggeration 40 \
 *        --texture-day earth_day.jpg --texture-night earth_night.tif
 *
 * Alternatively, download 15 arc-second (~450 m) regional tiles:
 * https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/15s/15s_surface_elev_gtif/
 * and combine into a seamless GDAL Virtual Dataset (.vrt):
 *
 *      gdalbuildvrt etopo_15s.vrt ETOPO_2022_v1_15s_*.tif
 *      gaia etopo_15s.vrt earth_15s.g --dim 2400 --exaggeration 40
 * ======================================================================
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
#include "bu/path.h"
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
#define DEFAULT_EXAG 1.0

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
 * Open a raster dataset and ensure it has georeferencing metadata.
 * If the dataset lacks an affine geotransform or spatial reference (common
 * for plain JPEG or unreferenced GeoTIFF files), wrap it in an in-memory VRT
 * spanning global bounds (lon -180..180, lat 90..-90) with WGS-84 (EPSG:4326).
 */
static GDALDatasetH
open_raster_georef(const char *path)
{
    GDALDatasetH ds;
    double gt[6];

    ds = GDALOpenEx(path,
		    GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
		    NULL, NULL, NULL);
    if (!ds) return NULL;

    if (GDALGetGeoTransform(ds, gt) == CE_None
	&& GDALGetProjectionRef(ds)
	&& strlen(GDALGetProjectionRef(ds)) > 0) {
	return ds;
    }

    /* Wrap non-georeferenced global image into a self-contained in-memory dataset */
    const char *args[10];
    args[0] = "-a_ullr";
    args[1] = "-180";
    args[2] = "90";
    args[3] = "180";
    args[4] = "-90";
    args[5] = "-a_srs";
    args[6] = "EPSG:4326";
    args[7] = "-of";
    args[8] = "MEM";
    args[9] = NULL;

    GDALTranslateOptions *topts = GDALTranslateOptionsNew((char **)args, NULL);
    if (topts) {
	int err = 0;
	GDALDatasetH mem_ds = GDALTranslate("", ds, topts, &err);
	GDALTranslateOptionsFree(topts);
	if (mem_ds) {
	    GDALClose(ds);
	    return mem_ds;
	}
    }
    return ds;
}


/**
 * Warp the source raster to an orthographic projection centred on a cube
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
 * Warp an RGB texture dataset to an orthographic tangent plane for one
 * cube face and return a newly-allocated buffer of 3 * tex_dim * tex_dim
 * bytes in bottom-to-top (v=0 South to v=1 North) scanline order.
 */
static unsigned char *
warp_texture_face(GDALDatasetH src, double lat_0, double lon_0,
		  unsigned int tex_dim, double extent_m)
{
    GDALDatasetH warped;
    GDALRasterBandH r_band = NULL, g_band = NULL, b_band = NULL;
    int n_bands;
    unsigned char *rgb_buf;
    unsigned char *row_buf[3];
    unsigned int dy;
    size_t count = (size_t)tex_dim * (size_t)tex_dim * 3;

    if (!src) return NULL;

    warped = warp_face(src, lat_0, lon_0, (int)tex_dim, extent_m);
    if (!warped) return NULL;

    n_bands = GDALGetRasterCount(warped);
    if (n_bands <= 0) {
	GDALClose(warped);
	return NULL;
    }

    r_band = GDALGetRasterBand(warped, 1);
    g_band = (n_bands >= 2) ? GDALGetRasterBand(warped, 2) : r_band;
    b_band = (n_bands >= 3) ? GDALGetRasterBand(warped, 3) : g_band;

    rgb_buf = (unsigned char *)bu_calloc(count, sizeof(unsigned char), "texture rgb");
    row_buf[0] = (unsigned char *)bu_malloc(tex_dim, "r row");
    row_buf[1] = (unsigned char *)bu_malloc(tex_dim, "g row");
    row_buf[2] = (unsigned char *)bu_malloc(tex_dim, "b row");

    /* Read row-by-row, flipping Y axis so dy=0 is South (v=0) and dy=tex_dim-1 is North (v=1).
     * GDAL raster row=0 is North, row=tex_dim-1 is South.
     */
    for (dy = 0; dy < tex_dim; dy++) {
	int gy = (int)(tex_dim - 1 - dy);
	unsigned int dx;

	if (GDALRasterIO(r_band, GF_Read, 0, gy, (int)tex_dim, 1, row_buf[0], (int)tex_dim, 1, GDT_Byte, 0, 0) != CE_None) {
	    bu_log("Warning: Failed reading red band scanline %d\n", gy);
	}
	if (n_bands >= 2) {
	    if (GDALRasterIO(g_band, GF_Read, 0, gy, (int)tex_dim, 1, row_buf[1], (int)tex_dim, 1, GDT_Byte, 0, 0) != CE_None) {
		bu_log("Warning: Failed reading green band scanline %d\n", gy);
	    }
	} else {
	    memcpy(row_buf[1], row_buf[0], tex_dim);
	}

	if (n_bands >= 3) {
	    if (GDALRasterIO(b_band, GF_Read, 0, gy, (int)tex_dim, 1, row_buf[2], (int)tex_dim, 1, GDT_Byte, 0, 0) != CE_None) {
		bu_log("Warning: Failed reading blue band scanline %d\n", gy);
	    }
	} else {
	    memcpy(row_buf[2], row_buf[1], tex_dim);
	}

	for (dx = 0; dx < tex_dim; dx++) {
	    size_t k = ((size_t)dy * tex_dim + dx) * 3;
	    rgb_buf[k + 0] = row_buf[0][dx];
	    rgb_buf[k + 1] = row_buf[1][dx];
	    rgb_buf[k + 2] = row_buf[2][dx];
	}
    }

    bu_free(row_buf[0], "r row");
    bu_free(row_buf[1], "g row");
    bu_free(row_buf[2], "b row");
    GDALClose(warped);

    return rgb_buf;
}


/**
 * Blend daytime and nighttime RGB textures to produce a dusk / twilight
 * terminator texture for one face.
 *
 * @param day_rgb     Daytime RGB buffer (tex_dim x tex_dim x 3)
 * @param night_rgb   Nighttime RGB buffer (tex_dim x tex_dim x 3)
 * @param face        Cube face parameters
 * @param tex_dim     Texture raster dimension
 * @param extent_m    Tangent plane half-width (metres)
 * @param sun_dir     Unit vector pointing towards the sun (in ECEF)
 *
 * @return Newly allocated RGB buffer (tex_dim x tex_dim x 3).
 */
static unsigned char *
blend_dusk_texture(const unsigned char *day_rgb,
		   const unsigned char *night_rgb,
		   const struct cube_face *face,
		   unsigned int tex_dim, double extent_m,
		   const vect_t sun_dir)
{
    vect_t e, n, u;
    unsigned char *dusk_rgb;
    unsigned int dx, dy;
    size_t count = (size_t)tex_dim * (size_t)tex_dim * 3;
    double cell_m = 2.0 * extent_m / (tex_dim - 1);
    /* Twilight transition half-width: sin(9 deg) approx 0.1564 */
    const double twilight_w = 0.156434465;

    if (!day_rgb || !night_rgb) return NULL;

    dusk_rgb = (unsigned char *)bu_calloc(count, sizeof(unsigned char), "dusk rgb");
    geo_frame(face->lat_0, face->lon_0, e, n, u);

    for (dy = 0; dy < tex_dim; dy++) {
	double y = -extent_m + dy * cell_m;
	for (dx = 0; dx < tex_dim; dx++) {
	    double x = -extent_m + dx * cell_m;
	    size_t k = ((size_t)dy * tex_dim + dx) * 3;
	    double r_sq = x*x + y*y;
	    double z, norm_len;
	    vect_t norm;
	    double solar_cos, t, blend_val;
	    int c;

	    if (r_sq < EARTH_R_M * EARTH_R_M) {
		z = sqrt(EARTH_R_M * EARTH_R_M - r_sq);
	    } else {
		z = 0.0;
	    }

	    /* 3D point on the sphere in ECEF */
	    VSET(norm,
		 x * e[X] + y * n[X] + z * u[X],
		 x * e[Y] + y * n[Y] + z * u[Y],
		 x * e[Z] + y * n[Z] + z * u[Z]);
	    norm_len = MAGNITUDE(norm);
	    if (norm_len > 0.0) {
		VSCALE(norm, norm, 1.0 / norm_len);
	    }

	    solar_cos = VDOT(norm, sun_dir);

	    /* Smoothstep blend: t = 1.0 (day), t = 0.0 (night) */
	    if (solar_cos >= twilight_w) {
		t = 1.0;
	    } else if (solar_cos <= -twilight_w) {
		t = 0.0;
	    } else {
		double u_val = (solar_cos + twilight_w) / (2.0 * twilight_w);
		t = u_val * u_val * (3.0 - 2.0 * u_val);
	    }

	    for (c = 0; c < 3; c++) {
		double d = (double)day_rgb[k + c];
		double n_val = (double)night_rgb[k + c];
		blend_val = t * d + (1.0 - t) * n_val;
		if (blend_val < 0.0)   blend_val = 0.0;
		if (blend_val > 255.0) blend_val = 255.0;
		dusk_rgb[k + c] = (unsigned char)(blend_val + 0.5);
	    }
	}
    }

    return dusk_rgb;
}


/**
 * Process one cube face: warp elevation data, write DSP solid and
 * bounding pyramid, and generate textured face regions if textures are
 * provided.
 *
 * @return 0 on success, -1 on failure.
 */
static int
process_face(GDALDatasetH src, struct rt_wdb *wdbp,
	     const struct cube_face *face, unsigned int dim,
	     double exag,
	     double cell_m, double cell_z_m, double radius_mm,
	     GDALDatasetH src_day, GDALDatasetH src_night,
	     unsigned int tex_dim, const vect_t sun_dir)
{
    char name_data[64], name_solid[64], name_pyr[64], name_comb[64];
    GDALDatasetH warped;
    GDALRasterBandH band;
    int has_nodata = 0;
    double nodata_val = 0.0;
    double *elev;
    unsigned short *grid;
    size_t count;
    unsigned int dx, dy;

    double cell_mm   = cell_m * M2MM;
    double cell_z_mm = cell_z_m * M2MM;

    /* Z_base is chosen deep enough to intersect the water sphere
     * cleanly and to bound the valid region from the origin.
     */
    double R_m = radius_mm / M2MM;
    double z_base_m  = 3000.0; /* Deep inside the Earth */
    double z_base_mm = z_base_m * M2MM;

    struct rt_dsp_internal *dsp;
    unsigned char *day_rgb = NULL;
    unsigned char *night_rgb = NULL;

    snprintf(name_data,  sizeof(name_data),  "face_%s.data", face->tag);
    snprintf(name_solid, sizeof(name_solid), "face_%s.s",    face->tag);
    snprintf(name_pyr,   sizeof(name_pyr),   "pyr_%s.s",     face->tag);
    snprintf(name_comb,  sizeof(name_comb),  "face_%s.c",    face->tag);

    bu_log("gaia: face %s  center (%.1f, %.1f) ...\n",
	   face->tag, face->lat_0, face->lon_0);

    /* ---- Warp source data to orthographic for this face. ---- */
    double extent_m = cell_m * (dim - 1) * 0.5;
    float min_h = 1e9, max_h = -1e9;
    warped = warp_face(src, face->lat_0, face->lon_0,
		       (int)dim, extent_m);
    if (!warped) {
	bu_log("gaia: warp failed for face %s\n", face->tag);
	return -1;
    }

    count = (size_t)dim * (size_t)dim;
    elev = (double *)bu_calloc(count, sizeof(double), "elevation");
    grid = (unsigned short *)bu_calloc(count, sizeof(unsigned short),
				       "dsp grid");

    band = GDALGetRasterBand(warped, 1);
    nodata_val = GDALGetRasterNoDataValue(band, &has_nodata);

    /* Read elevation row-by-row, flipping Y axis so dy=0 is South and dy=dim-1 is North.
     * GDAL: row=0 is North, row=dim-1 is South. */
    for (dy = 0; dy < dim; dy++) {
	if (GDALRasterIO(band, GF_Read,
			 0, (int)(dim - 1 - dy),
			 (int)dim, 1,
			 &elev[(size_t)dy * dim],
			 (int)dim, 1,
			 GDT_Float64, 0, 0) != CE_None) {
	    bu_log("gaia: read error face %s row %u\n",
		   face->tag, dy);
	}
    }
    GDALClose(warped);

    /* ---- Compute radial displacement and quantize. ---- */
    for (dy = 0; dy < dim; dy++) {
        double y = -extent_m + dy * cell_m;
        for (dx = 0; dx < dim; dx++) {
            double x = -extent_m + dx * cell_m;
            size_t k = (size_t)dy * dim + dx;
            double h = elev[k];
            double z_val, z_dsp;
            long v;
            double r_sq = x*x + y*y;

            if (h < min_h) min_h = (float)h;
            if (h > max_h) max_h = (float)h;

            if (has_nodata && NEAR_EQUAL(h, nodata_val, 1.0))
                h = -100.0;

            if (h <= 0.0) {
                h = -100.0;
            }

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
    printf("Face %s: elevation min=%f, max=%f\n", face->tag, min_h, max_h);
    bu_free(elev, "elevation");

    /* Write the height data as a BINUNIF object in the .g */
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
        double D = 2.0 * radius_mm; // Far outside
        double overlap = 1.0; // Exact partition

        geo_frame(face->lat_0, face->lon_0, e, n, u);

        /* Base vertices (CCW from outside looking in to origin) */
        VSET(pts[0], D * (u[X] + overlap*e[X] + overlap*n[X]), D * (u[Y] + overlap*e[Y] + overlap*n[Y]), D * (u[Z] + overlap*e[Z] + overlap*n[Z]));
        VSET(pts[1], D * (u[X] - overlap*e[X] + overlap*n[X]), D * (u[Y] - overlap*e[Y] + overlap*n[Y]), D * (u[Z] - overlap*e[Z] + overlap*n[Z]));
        VSET(pts[2], D * (u[X] - overlap*e[X] - overlap*n[X]), D * (u[Y] - overlap*e[Y] - overlap*n[Y]), D * (u[Z] - overlap*e[Z] - overlap*n[Z]));
        VSET(pts[3], D * (u[X] + overlap*e[X] - overlap*n[X]), D * (u[Y] + overlap*e[Y] - overlap*n[Y]), D * (u[Z] + overlap*e[Z] - overlap*n[Z]));
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

    /* ---- Warp and assign textures if requested ---- */
    if (src_day) {
	char name_bin[64], name_r[64], shader_args[256];
	struct wmember hd;

	day_rgb = warp_texture_face(src_day, face->lat_0, face->lon_0,
				    tex_dim, extent_m);
	if (day_rgb) {
	    snprintf(name_bin, sizeof(name_bin), "face_%s_day.bin", face->tag);
	    mk_binunif(wdbp, name_bin, (void *)day_rgb, WDB_BINUNIF_UINT8,
		       (long)tex_dim * tex_dim * 3);

	    snprintf(name_r, sizeof(name_r), "face_%s_day.r", face->tag);
	    snprintf(shader_args, sizeof(shader_args),
		     "texture obj %s w %u n %u; phong di=0.8 sp=0.1",
		     name_bin, tex_dim, tex_dim);

	    BU_LIST_INIT(&hd.l);
	    (void)mk_addmember(name_comb, &hd.l, NULL, WMOP_UNION);
	    mk_lcomb(wdbp, name_r, &hd, 1, "stack", shader_args, NULL, 0);
	}
    }

    if (src_night) {
	char name_bin[64], name_r[64], shader_args[256];
	struct wmember hd;

	night_rgb = warp_texture_face(src_night, face->lat_0, face->lon_0,
				      tex_dim, extent_m);
	if (night_rgb) {
	    snprintf(name_bin, sizeof(name_bin), "face_%s_night.bin", face->tag);
	    mk_binunif(wdbp, name_bin, (void *)night_rgb, WDB_BINUNIF_UINT8,
		       (long)tex_dim * tex_dim * 3);

	    snprintf(name_r, sizeof(name_r), "face_%s_night.r", face->tag);
	    snprintf(shader_args, sizeof(shader_args),
		     "texture obj %s w %u n %u; phong di=0.8 sp=0.1",
		     name_bin, tex_dim, tex_dim);

	    BU_LIST_INIT(&hd.l);
	    (void)mk_addmember(name_comb, &hd.l, NULL, WMOP_UNION);
	    mk_lcomb(wdbp, name_r, &hd, 1, "stack", shader_args, NULL, 0);
	}
    }

    if (day_rgb && night_rgb) {
	char name_bin[64], name_r[64], shader_args[256];
	struct wmember hd;
	unsigned char *dusk_rgb;

	dusk_rgb = blend_dusk_texture(day_rgb, night_rgb, face,
				      tex_dim, extent_m, sun_dir);
	if (dusk_rgb) {
	    snprintf(name_bin, sizeof(name_bin), "face_%s_dusk.bin", face->tag);
	    mk_binunif(wdbp, name_bin, (void *)dusk_rgb, WDB_BINUNIF_UINT8,
		       (long)tex_dim * tex_dim * 3);

	    snprintf(name_r, sizeof(name_r), "face_%s_dusk.r", face->tag);
	    snprintf(shader_args, sizeof(shader_args),
		     "texture obj %s w %u n %u; phong di=0.8 sp=0.1",
		     name_bin, tex_dim, tex_dim);

	    BU_LIST_INIT(&hd.l);
	    (void)mk_addmember(name_comb, &hd.l, NULL, WMOP_UNION);
	    mk_lcomb(wdbp, name_r, &hd, 1, "stack", shader_args, NULL, 0);

	    bu_free(dusk_rgb, "dusk rgb");
	}
    }

    if (day_rgb)   bu_free(day_rgb, "texture rgb");
    if (night_rgb) bu_free(night_rgb, "texture rgb");

    bu_log("gaia: face %s  %ux%u  cell %.1f km\n",
	   face->tag, dim, dim, cell_m / 1000.0);
    return 0;
}


int
main(int ac, char *av[])
{
    struct rt_wdb *wdbp;
    GDALDatasetH src = NULL;
    GDALDatasetH src_day = NULL;
    GDALDatasetH src_night = NULL;
    GDALRasterBandH band;
    double mm[2];
    int bmin_ok = 0, bmax_ok = 0;
    double raw_min, raw_max, raw_range;
    unsigned int dim = DEFAULT_DIM;
    unsigned int tex_dim = 0;
    double exag = DEFAULT_EXAG;
    double sun_lon = 0.0;
    double sun_lat = 0.0;
    double radius_mm;
    double cell_m, cell_z_m;
    int i;

    const char *input_path = NULL;
    const char *output_path = NULL;
    const char *texture_day_path = NULL;
    const char *texture_night_path = NULL;
    char title[1024];

    unsigned char land_rgb[3]  = { 110, 130, 80 };
    unsigned char water_rgb[3] = {  30,  80, 160 };
    point_t center;
    vect_t sun_dir;

    bu_setprogname(av[0]);

    if (ac < 3) {
	bu_exit(1,
		"Usage: %s input.tif output.g [options]\n\n"
		"  input.tif             Global DEM raster (e.g. ETOPO 2022 GeoTIFF)\n"
		"  output.g              Output BRL-CAD database\n"
		"  --dim N               Samples per cube-face edge        (default %d)\n"
		"  --exaggeration F      Vertical scale factor              (default %.0f)\n"
		"  --texture-day FILE    Daytime color texture map (GeoTIFF/JPEG/PNG)\n"
		"  --texture-night FILE  Nighttime city lights texture map (GeoTIFF/JPEG/PNG)\n"
		"  --texture-dim N       Texture resolution per face edge   (default = dim)\n"
		"  --sun-lon DEG         Subsolar longitude in degrees     (default 0.0)\n"
		"  --sun-lat DEG         Subsolar latitude in degrees      (default 0.0)\n",
		av[0], DEFAULT_DIM, DEFAULT_EXAG);
    }
    input_path  = av[1];
    output_path = av[2];

    for (i = 3; i < ac; i++) {
	if (BU_STR_EQUAL(av[i], "--dim") && i + 1 < ac) {
	    dim = (unsigned int)atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--exaggeration") && i + 1 < ac) {
	    exag = atof(av[++i]);
	} else if ((BU_STR_EQUAL(av[i], "--texture-day") ||
		    BU_STR_EQUAL(av[i], "--day") ||
		    BU_STR_EQUAL(av[i], "--texture")) && i + 1 < ac) {
	    texture_day_path = av[++i];
	} else if ((BU_STR_EQUAL(av[i], "--texture-night") ||
		    BU_STR_EQUAL(av[i], "--night")) && i + 1 < ac) {
	    texture_night_path = av[++i];
	} else if (BU_STR_EQUAL(av[i], "--texture-dim") && i + 1 < ac) {
	    tex_dim = (unsigned int)atoi(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--sun-lon") && i + 1 < ac) {
	    sun_lon = atof(av[++i]);
	} else if (BU_STR_EQUAL(av[i], "--sun-lat") && i + 1 < ac) {
	    sun_lat = atof(av[++i]);
	}
    }

    if (dim < 64)    dim = 64;
    if (dim > 4096)  dim = 4096;
    if (exag < 1.0)  exag = 1.0;
    if (exag > 200.0) exag = 200.0;

    if (tex_dim == 0)    tex_dim = dim;
    if (tex_dim < 64)    tex_dim = 64;
    if (tex_dim > 8192)  tex_dim = 8192;

    /* Compute sun direction vector in ECEF */
    {
	double lon_rad = sun_lon * DEG2RAD;
	double lat_rad = sun_lat * DEG2RAD;
	VSET(sun_dir,
	     cos(lat_rad) * cos(lon_rad),
	     cos(lat_rad) * sin(lon_rad),
	     sin(lat_rad));
	VUNITIZE(sun_dir);
    }

    radius_mm = EARTH_R_M * M2MM;

    /* Ensure GDAL's PROJ engine can find its data files. */
    if (!getenv("PROJ_LIB"))
	bu_setenv("PROJ_LIB",
		  bu_dir(NULL, 0, BU_DIR_DATA, "proj", NULL), 1);

    GDALAllRegister();

    /* ---- Open the source elevation raster. ---- */
    src = open_raster_georef(input_path);
    if (!src)
	bu_exit(2, "gaia: cannot open '%s'\n", input_path);

    /* Open optional texture datasets */
    if (texture_day_path) {
	src_day = open_raster_georef(texture_day_path);
	if (!src_day)
	    bu_exit(2, "gaia: cannot open daytime texture '%s'\n", texture_day_path);
    }

    if (texture_night_path) {
	src_night = open_raster_georef(texture_night_path);
	if (!src_night)
	    bu_exit(2, "gaia: cannot open nighttime texture '%s'\n", texture_night_path);
    }

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

    bu_log("gaia: elevation %.1f .. %.1f m   (x%.0f)\n",
	   raw_min, raw_max, exag);

    /* Uniform cell spacing: the orthographic face spans +/- extent_m on the
     * tangent plane.
     */
    double extent_m = EARTH_R_M;
    cell_m = 2.0 * extent_m / (dim - 1);

    /* Z quantization: we map the range from base to max mountain peak into uint16. */
    double z_base_m = 3000.0; /* Deep inside the Earth */
    double z_max_m = EARTH_R_M + raw_max * exag;
    double span_z_m = z_max_m - z_base_m;

    cell_z_m = span_z_m / (double)U16MAX;

    bu_log("gaia: dim=%u  cell_xy=%.1f km  cell_z=%.1f m  exag=%.0fx\n",
	   dim, cell_m / 1000.0, cell_z_m, exag);
    if (src_day || src_night) {
	bu_log("gaia: textures: day=%s night=%s  tex_dim=%u\n",
	       src_day ? "yes" : "no",
	       src_night ? "yes" : "no",
	       tex_dim);
    }

    /* ---- Open the output .g database. ---- */
    wdbp = wdb_fopen(output_path);
    if (!wdbp) {
	GDALClose(src);
	if (src_day) GDALClose(src_day);
	if (src_night) GDALClose(src_night);
	bu_exit(3, "gaia: cannot create '%s'\n", output_path);
    }
    snprintf(title, sizeof(title),
	     "GAIA: Earth Terrain Model, derived from %s to 6x%ux%u at %gx",
	     bu_path_basename(input_path, NULL), dim, dim, exag);
    mk_id_units(wdbp, title, "mm");

    /* ---- Process each cube face. ---- */
    for (i = 0; i < NFACES; i++) {
	if (process_face(src, wdbp, &faces[i], dim,
			 exag,
			 cell_m, cell_z_m, radius_mm,
			 src_day, src_night,
			 tex_dim, sun_dir) != 0) {
	    bu_log("gaia: WARNING - face %s failed\n",
		   faces[i].tag);
	}
    }
    GDALClose(src);
    if (src_day) GDALClose(src_day);
    if (src_night) GDALClose(src_night);

    /* ---- Water sphere at sea-level radius. ----
     * Subtract all terrain faces so that water occupies only the
     * volume inside the sphere that is outside the DSP solids.
     */
    VSET(center, 0.0, 0.0, 0.0);
    mk_sph(wdbp, "water.s", center, radius_mm);

    {
	struct wmember water_hd;
	BU_LIST_INIT(&water_hd.l);
	(void)mk_addmember("water.s", &water_hd.l, NULL, WMOP_UNION);
	for (i = 0; i < NFACES; i++) {
	    char nm[64];
	    snprintf(nm, sizeof(nm), "face_%s.c", faces[i].tag);
	    (void)mk_addmember(nm, &water_hd.l, NULL, WMOP_SUBTRACT);
	}
	mk_lcomb(wdbp, "water.r", &water_hd, 1,
		 "plastic", "di=0.7 sp=0.3", water_rgb, 0);
    }

    /* ---- Build textured / untextured combinations ---- */
    if (texture_day_path) {
	struct wmember terr_day_hd;
	struct wmember earth_day_hd;
	BU_LIST_INIT(&terr_day_hd.l);
	for (i = 0; i < NFACES; i++) {
	    char nm[64];
	    snprintf(nm, sizeof(nm), "face_%s_day.r", faces[i].tag);
	    (void)mk_addmember(nm, &terr_day_hd.l, NULL, WMOP_UNION);
	}
	mk_lcomb(wdbp, "terrain_day.c", &terr_day_hd, 0, NULL, NULL, NULL, 0);

	BU_LIST_INIT(&earth_day_hd.l);
	(void)mk_addmember("terrain_day.c", &earth_day_hd.l, NULL, WMOP_UNION);
	(void)mk_addmember("water.r",       &earth_day_hd.l, NULL, WMOP_UNION);
	mk_lcomb(wdbp, "earth_day.all", &earth_day_hd, 0, NULL, NULL, NULL, 0);
    }

    if (texture_night_path) {
	struct wmember terr_night_hd;
	struct wmember earth_night_hd;
	BU_LIST_INIT(&terr_night_hd.l);
	for (i = 0; i < NFACES; i++) {
	    char nm[64];
	    snprintf(nm, sizeof(nm), "face_%s_night.r", faces[i].tag);
	    (void)mk_addmember(nm, &terr_night_hd.l, NULL, WMOP_UNION);
	}
	mk_lcomb(wdbp, "terrain_night.c", &terr_night_hd, 0, NULL, NULL, NULL, 0);

	BU_LIST_INIT(&earth_night_hd.l);
	(void)mk_addmember("terrain_night.c", &earth_night_hd.l, NULL, WMOP_UNION);
	(void)mk_addmember("water.r",         &earth_night_hd.l, NULL, WMOP_UNION);
	mk_lcomb(wdbp, "earth_night.all", &earth_night_hd, 0, NULL, NULL, NULL, 0);
    }

    if (texture_day_path && texture_night_path) {
	struct wmember terr_dusk_hd;
	struct wmember earth_dusk_hd;
	BU_LIST_INIT(&terr_dusk_hd.l);
	for (i = 0; i < NFACES; i++) {
	    char nm[64];
	    snprintf(nm, sizeof(nm), "face_%s_dusk.r", faces[i].tag);
	    (void)mk_addmember(nm, &terr_dusk_hd.l, NULL, WMOP_UNION);
	}
	mk_lcomb(wdbp, "terrain_dusk.c", &terr_dusk_hd, 0, NULL, NULL, NULL, 0);

	BU_LIST_INIT(&earth_dusk_hd.l);
	(void)mk_addmember("terrain_dusk.c", &earth_dusk_hd.l, NULL, WMOP_UNION);
	(void)mk_addmember("water.r",        &earth_dusk_hd.l, NULL, WMOP_UNION);
	mk_lcomb(wdbp, "earth_dusk.all", &earth_dusk_hd, 0, NULL, NULL, NULL, 0);
    }

    if (!texture_day_path && !texture_night_path) {
	struct wmember terr_hd;
	BU_LIST_INIT(&terr_hd.l);
	for (i = 0; i < NFACES; i++) {
	    char nm[64];
	    snprintf(nm, sizeof(nm), "face_%s.c", faces[i].tag);
	    (void)mk_addmember(nm, &terr_hd.l, NULL, WMOP_UNION);
	}
	mk_lcomb(wdbp, "terrain.r", &terr_hd, 1,
		 "plastic", "di=0.8 sp=0.1", land_rgb, 0);
    }

    /* ---- Top-level assembly: earth.all ---- */
    {
	struct wmember all_hd;
	BU_LIST_INIT(&all_hd.l);
	if (texture_day_path && texture_night_path) {
	    (void)mk_addmember("terrain_dusk.c", &all_hd.l, NULL, WMOP_UNION);
	} else if (texture_day_path) {
	    (void)mk_addmember("terrain_day.c",  &all_hd.l, NULL, WMOP_UNION);
	} else if (texture_night_path) {
	    (void)mk_addmember("terrain_night.c", &all_hd.l, NULL, WMOP_UNION);
	} else {
	    (void)mk_addmember("terrain.r",      &all_hd.l, NULL, WMOP_UNION);
	}
	(void)mk_addmember("water.r", &all_hd.l, NULL, WMOP_UNION);
	mk_lcomb(wdbp, "earth.all", &all_hd, 0, NULL, NULL, NULL, 0);
    }

    bu_log("gaia: wrote %s  (top-level: 'earth.all')\n",
	   output_path);
    if (texture_day_path && texture_night_path) {
	bu_log("gaia: generated assemblies: 'earth_day.all', 'earth_night.all', 'earth_dusk.all', 'earth.all'\n");
    } else if (texture_day_path) {
	bu_log("gaia: generated assemblies: 'earth_day.all', 'earth.all'\n");
    } else if (texture_night_path) {
	bu_log("gaia: generated assemblies: 'earth_night.all', 'earth.all'\n");
    }

    wdb_close(wdbp);
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
