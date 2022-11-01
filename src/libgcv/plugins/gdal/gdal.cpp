/*                             G D A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 Tom Browder
 * Copyright (c) 2017-2022 United States Government as represented by the U.S. Army
 * Research Laboratory.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
/** @file gdal.cpp
 *
 * Geospatial Data Abstraction Library plugin for libgcv.
 *
 */


#include "common.h"
#include "vmath.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

/* GDAL headers */
#include "gdal.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "ogr_spatialref.h"
#include "vrtdataset.h"

#include "bu/app.h"
#include "bu/cv.h"
#include "bu/env.h"
#include "bu/path.h"
#include "bu/units.h"
#include "raytrace.h"
#include "gcv/api.h"
#include "gcv/util.h"
#include "wdb.h"

// Function based on gdalinfo code
extern int gdal_ll(GDALDatasetH hDataset, double *lat_center, double *long_center);

struct gdal_read_options
{
    const char *proj;
    int center;
    int zone;
};

struct conversion_state {
    const struct gcv_opts *gcv_options;
    const struct gdal_read_options *ops;
    const char *input_file;
    struct rt_wdb *wdbp;

    /* GDAL state */
    GDALDatasetH hDataset;
};

HIDDEN void
gdal_state_init(struct conversion_state *gs)
{
    if(!gs) return;
    gs->gcv_options = NULL;
    gs->ops = NULL;
    gs->input_file = NULL;
    gs->wdbp = RT_WDB_NULL;

    gs->hDataset = NULL;
}

HIDDEN int
get_dataset_info(GDALDatasetH hDataset)
{
    char *gdal_info = GDALInfo(hDataset, NULL);
    if (gdal_info)
	bu_log("%s", gdal_info);
    CPLFree(gdal_info);
    return 0;
}

HIDDEN int
gdal_can_read(const char *data)
{
    GDALDatasetH hDataset;
    GDALAllRegister();

    if (!data)
	return 0;

    if (!bu_file_exists(data,NULL))
	return 0;

    hDataset = GDALOpenEx(data, GDAL_OF_READONLY | GDAL_OF_RASTER , NULL, NULL, NULL);

    if (!hDataset)
	return 0;

    GDALClose(hDataset);

    return 1;
}

HIDDEN void
gdal_elev_minmax(GDALDatasetH ds)
{
    int bmin, bmax = 0;
    double mm[2];
    GDALRasterBandH band = GDALGetRasterBand(ds, 1);
    mm[0] = GDALGetRasterMinimum(band, &bmin);
    mm[1] = GDALGetRasterMaximum(band, &bmin);
    if (!bmin || !bmax)
	GDALComputeRasterMinMax(band, TRUE, mm);
    bu_log("Elevation Minimum/Maximum: %f, %f\n", mm[0], mm[1]);
}

/* Get the UTM zone of the GDAL dataset - see
 * https://gis.stackexchange.com/questions/241696/how-to-convert-from-lat-lon-to-utm-with-gdaltransform
 * and the linked posts in the answers for more info. */
HIDDEN int
gdal_utm_zone(struct conversion_state *state)
{
    int zone = INT_MAX;
    double longitude = 0.0;
    (void)gdal_ll(state->hDataset, NULL, &longitude);
    if (longitude > -180.0 && longitude < 180.0) {
	zone = floor((longitude + 180) / 6) + 1;
	bu_log("zone: %d\n", zone);
    }
    return zone;
}


/* Get corresponding EPSG number of the UTM zone - see
 * https://gis.stackexchange.com/questions/241696/how-to-convert-from-lat-lon-to-utm-with-gdaltransform
 * and the linked posts in the answers for more info. */
HIDDEN int
gdal_utm_epsg(struct conversion_state *state, int zone)
{
    int epsg = INT_MAX;
    double latitude = 0.0;
    if (gdal_ll(state->hDataset, &latitude, NULL)) {
	epsg = (latitude > 0) ? 32600 + zone : 32700 + zone;
    }
    bu_log("epsg: %d\n", epsg);
    return epsg;
}

HIDDEN int
gdal_read(struct gcv_context *context, const struct gcv_opts *gcv_options,
	const void *options_data, const char *source_path)
{
    size_t count;
    unsigned int xsize = 0;
    unsigned int ysize = 0;
    struct conversion_state *state;
    struct bu_vls name_root = BU_VLS_INIT_ZERO;
    struct bu_vls name_data = BU_VLS_INIT_ZERO;
    struct bu_vls name_dsp = BU_VLS_INIT_ZERO;
    BU_GET(state, struct conversion_state);
    gdal_state_init(state);
    state->gcv_options = gcv_options;
    state->ops = (struct gdal_read_options *)options_data;
    state->input_file = source_path;
    state->wdbp = context->dbip->dbi_wdbp;

    /* If the environment is identifying a PROJ_LIB directory use it,
     * else set it to the correct one for BRL-CAD */
    if (!getenv("PROJ_LIB")) {
	const char *pjenv = NULL;
	bu_setenv("PROJ_LIB", bu_dir(NULL, 0, BU_DIR_DATA, "proj", NULL), 1);
	pjenv = getenv("PROJ_LIB");
	bu_log("Setting PROJ_LIB to %s\n", pjenv);
    }

    GDALAllRegister();

    state->hDataset = GDALOpenEx(source_path, GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL);
    if (!state->hDataset) {
	bu_log("GDAL Reader: error opening input file %s\n", source_path);
	BU_PUT(state, struct conversion_state);
	return 0;
    }
    (void)get_dataset_info(state->hDataset);
    gdal_elev_minmax(state->hDataset);

    /* Stash the original Spatial Reference System as a PROJ.4 string
     * for later assignment
     */
    char *orig_proj4_str = NULL;
    OGRSpatialReference iSRS(GDALGetProjectionRef(state->hDataset));
    iSRS.exportToProj4(&orig_proj4_str);

    /* Use the information in the data set to deduce the EPSG number
     * corresponding to the correct UTM projection zone, define a
     * spatial reference, and generate the "Well Known Text" to use as
     * the argument to the warping function*/
    GDALDatasetH hOutDS;
    int zone = (state->ops->zone == INT_MAX) ? gdal_utm_zone(state) : state->ops->zone;
    char *dunit = NULL;
    const char *dunit_default = "m";
    struct bu_vls new_proj4_str = BU_VLS_INIT_ZERO;
    if (zone != INT_MAX) {
	int epsg = gdal_utm_epsg(state, zone);
	bu_vls_sprintf(&new_proj4_str, "+init=epsg:%d +zone=%d +proj=utm +no_defs", epsg, zone);
	OGRSpatialReference oSRS;
	oSRS.importFromProj4(bu_vls_addr(&new_proj4_str));
	char *dst_Wkt = NULL;
	oSRS.exportToWkt(&dst_Wkt);

	/* Perform the UTM data warp.  At this point the data is not
	 * yet in the form needed by the DSP primitive */
	hOutDS = GDALAutoCreateWarpedVRT(state->hDataset, NULL, dst_Wkt, GRA_CubicSpline, 0.0, NULL);
	CPLFree(dst_Wkt);

	if (hOutDS) {
	    dunit = bu_strdup(GDALGetRasterUnitType(((GDALDataset *)hOutDS)->GetRasterBand(1)));
	    bu_log("\nTransformed dataset info:\n");
	    (void)get_dataset_info(hOutDS);
	    gdal_elev_minmax(hOutDS);
	}
    } else {
	hOutDS = state->hDataset;
	dunit = bu_strdup(GDALGetRasterUnitType(((GDALDataset *)hOutDS)->GetRasterBand(1)));
    }
    if (!dunit || strlen(dunit) == 0)
	dunit = (char *)dunit_default;

    /* Do the translate step (a.l.a gdal_translate) that puts the data
     * in a form we can use */
    char *img_opts[3];
    img_opts[0] = bu_strdup("-of");
    img_opts[1] = bu_strdup("MEM");
    img_opts[2] = NULL;
    GDALTranslateOptions *gdalt_opts = GDALTranslateOptionsNew(img_opts, NULL);
    GDALDatasetH flatDS = GDALTranslate("", hOutDS, gdalt_opts, NULL);
    GDALTranslateOptionsFree(gdalt_opts);
    if (hOutDS != state->hDataset)
	GDALClose(hOutDS);
    for(int i = 0; i < 3; i++) {
	bu_free(img_opts[i], "imgopt");
    }

    /* Stash the flat Spatial Reference System as a PROJ.4 string for
     * later assignment */
    char *flat_proj4_str = NULL;
    OGRSpatialReference fSRS(GDALGetProjectionRef(flatDS));
    fSRS.exportToProj4(&flat_proj4_str);

    bu_log("\nFinalized dataset info:\n");
    (void)get_dataset_info(flatDS);
    gdal_elev_minmax(flatDS);

    /* Read the data into something a DSP can process - note that y is
     * backwards for DSPs compared to GDAL */
    unsigned short *uint16_array = NULL;
    uint16_t *scanline = NULL;
    GDALRasterBandH band = GDALGetRasterBand(flatDS, 1);
    xsize = GDALGetRasterBandXSize(band);
    ysize = GDALGetRasterBandYSize(band);
    count = xsize * ysize; /* TODO - check for overflow here... */
    uint16_array = (unsigned short *)bu_calloc(count, sizeof(unsigned short), "unsigned short array");
    for(unsigned int i = 0; i < ysize; i++) {
	if (GDALRasterIO(band, GF_Read, 0, ysize-i-1, xsize, 1, &(uint16_array[i*xsize]), xsize, 1, GDT_UInt16, 0, 0) == CPLE_None) {
	    continue;
	} else {
	    bu_log("GDAL read error for band scanline %d\n", i);
	}
    }

    /* Convert the data before writing it so the DSP get_obj_data
     * routine sees what it expects */
    int in_cookie = bu_cv_cookie("hus");
    int out_cookie = bu_cv_cookie("nus"); /* data is network unsigned short */
    if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
	size_t got = bu_cv_w_cookie(uint16_array, out_cookie, count * sizeof(unsigned short), uint16_array, in_cookie, count);
	if (got != count) {
	    bu_log("got %zu != count %zu", got, count);
	    bu_bomb("\n");
	}
    }

    /* Collect info for DSP */
    double px = 0.0; double py = 0.0;
    double cx = 0.0; double cy = 0.0;
    double fGeoT[6];
    if (GDALGetGeoTransform(flatDS, fGeoT) == CE_None) {
	cx = (fGeoT[0] + fGeoT[1] * GDALGetRasterXSize(flatDS)/2.0 + fGeoT[2] * GDALGetRasterYSize(flatDS)/2.0) - fGeoT[0];
	cy = (fGeoT[3] + fGeoT[4] * GDALGetRasterXSize(flatDS)/2.0 + fGeoT[5] * GDALGetRasterYSize(flatDS)/2.0) - fGeoT[3];
	px = fGeoT[0];
	/* y is backwards for DSPs compared to GDAL */
	py = fGeoT[3] + 2 * cy;
    }

    /* We're in business - come up with some names for the objects */
    if (!bu_path_component(&name_root, source_path, BU_PATH_BASENAME_EXTLESS)) {
	bu_vls_sprintf(&name_root, "dsp");
    }
    bu_vls_sprintf(&name_data, "%s.data", bu_vls_addr(&name_root));
    bu_vls_sprintf(&name_dsp, "%s.s", bu_vls_addr(&name_root));

    /* Got it - write the binary object to the .g file */
    mk_binunif(state->wdbp, bu_vls_addr(&name_data), (void *)uint16_array, WDB_BINUNIF_UINT16, count);

    /* Done reading - close out inputs. */
    CPLFree(scanline);
    GDALClose(flatDS);
    GDALClose(state->hDataset);

    /* TODO: if we're going to BoT (3-space mesh, will depend on the
     * transform requested) we will need different logic... */

    /* Write out the dsp.  Since we're using a data object, instead of
     * a file, do the setup by hand. */
    struct rt_dsp_internal *dsp;
    BU_ALLOC(dsp, struct rt_dsp_internal);
    dsp->magic = RT_DSP_INTERNAL_MAGIC;
    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, bu_vls_addr(&name_data));
    dsp->dsp_datasrc = RT_DSP_SRC_OBJ;
    dsp->dsp_xcnt = xsize;
    dsp->dsp_ycnt = ysize;
    dsp->dsp_smooth = 1;
    dsp->dsp_cuttype = DSP_CUT_DIR_ADAPT;
    MAT_IDN(dsp->dsp_stom);
    dsp->dsp_stom[0] = dsp->dsp_stom[5] = fGeoT[1] * bu_units_conversion(dunit);
    dsp->dsp_stom[10] = bu_units_conversion(dunit);
    bn_mat_inv(dsp->dsp_mtos, dsp->dsp_stom);

    if (state->ops->center) {
	dsp->dsp_stom[3] = -cx * bu_units_conversion(dunit);
	/* y is backwards for DSPs compared to GDAL */
	dsp->dsp_stom[7] = cy * bu_units_conversion(dunit);
    } else {
	dsp->dsp_stom[3] = px * bu_units_conversion(dunit);
	dsp->dsp_stom[7] = py * bu_units_conversion(dunit);
    }

    wdb_export(state->wdbp, bu_vls_addr(&name_dsp), (void *)dsp, ID_DSP, 1);

    /* Write out the original and current Spatial Reference Systems
     * and other dimensional information to attributes on the dsp */
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    struct directory *dp = db_lookup(state->wdbp->dbip, bu_vls_addr(&name_dsp), LOOKUP_QUIET);
    if (dp != RT_DIR_NULL && !db5_get_attributes(state->wdbp->dbip, &avs, dp)) {
	struct bu_vls tstr = BU_VLS_INIT_ZERO;
	(void)bu_avs_add(&avs, "s_srs" , orig_proj4_str);
	(void)bu_avs_add(&avs, "t_srs" , bu_vls_addr(&new_proj4_str));
	(void)bu_avs_add(&avs, "f_srs" , flat_proj4_str);
	bu_vls_sprintf(&tstr, "%f %s", fabs(2*cx), dunit);
	(void)bu_avs_add(&avs, "x_length" , bu_vls_addr(&tstr));
	bu_vls_sprintf(&tstr, "%f %s", fabs(2*cy), dunit);
	(void)bu_avs_add(&avs, "y_length" , bu_vls_addr(&tstr));
	if (state->ops->center) {
	    bu_vls_sprintf(&tstr, "%f %s", px + cx, dunit);
	    (void)bu_avs_add(&avs, "x_offset" , bu_vls_addr(&tstr));
	    bu_vls_sprintf(&tstr, "%f %s", py - cy, dunit);
	    (void)bu_avs_add(&avs, "y_offset" , bu_vls_addr(&tstr));
	}
	bu_vls_free(&tstr);
	(void)db5_update_attributes(dp, &avs, state->wdbp->dbip);
    }

    if (dunit != dunit_default)
	bu_free(dunit, "free dunit");
    if (orig_proj4_str)
	CPLFree(orig_proj4_str);
    if (flat_proj4_str)
	CPLFree(flat_proj4_str);
    bu_vls_free(&new_proj4_str);

    bu_vls_free(&name_root);
    bu_vls_free(&name_data);
    bu_vls_free(&name_dsp);

    return 1;
}

HIDDEN void
gdal_read_create_opts(struct bu_opt_desc **odesc, void **dest_options_data)
{
    struct gdal_read_options *odata;

    BU_ALLOC(odata, struct gdal_read_options);
    *dest_options_data = odata;
    *odesc = (struct bu_opt_desc *)bu_malloc(4*sizeof(struct bu_opt_desc), "gdal option descriptions");

    odata->center = 0;
    odata->proj = NULL;
    odata->zone = INT_MAX;

    BU_OPT((*odesc)[0], "c", "center", NULL, NULL, &(odata->center), "Center the dsp terrain at global (0,0)");
    BU_OPT((*odesc)[1], "",  "utm-zone", "zone", &bu_opt_int, &(odata->zone), "When performing UTM project use the specified zone.");
    BU_OPT((*odesc)[2], "", "projection", "proj", &bu_opt_str, &(odata->proj), "Projection to apply to the terrain data before exporting to a .g file");
    BU_OPT_NULL((*odesc)[3]);
}

HIDDEN void
gdal_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}

extern "C"
{
    struct gcv_filter gcv_conv_gdal_read =
    {"GDAL Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_GDAL, gdal_can_read, gdal_read_create_opts, gdal_read_free_opts, gdal_read};

    static const struct gcv_filter * const filters[] = {&gcv_conv_gdal_read, NULL};
    const struct gcv_plugin gcv_plugin_info_s = { filters };
    COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info(){return &gcv_plugin_info_s;}
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
