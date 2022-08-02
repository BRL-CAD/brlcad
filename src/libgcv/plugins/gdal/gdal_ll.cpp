/* Pull the latitude and longitude out of the dataset.  Based on gdalinfo code, so
 * this function (gdal_ll) is:
 *
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2015, Faza Mahamood
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "common.h"

/* GDAL headers */
#include "gdal.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "ogr_spatialref.h"
#include "vrtdataset.h"

#include "bu/log.h"

int
gdal_ll(GDALDatasetH hDataset, double *lat_center, double *long_center)
{
    double longitude, latitude;
    double adfGeoTransform[6];
    const char *pzp = NULL;
    OGRCoordinateTransformationH htfm = NULL;
    double gx = GDALGetRasterXSize(hDataset)/2.0;
    double gy = GDALGetRasterYSize(hDataset)/2.0;
    if(GDALGetGeoTransform(hDataset, adfGeoTransform) == CE_None ) {
	longitude = adfGeoTransform[0] + adfGeoTransform[1] * gx  + adfGeoTransform[2] * gy;
	latitude = adfGeoTransform[3] + adfGeoTransform[4] * gx  + adfGeoTransform[5] * gy;
	pzp = GDALGetProjectionRef(hDataset);
    } else {
	return 0;
    }

    if(pzp != NULL && strlen(pzp) > 0) {
	OGRSpatialReferenceH hpj, hll = NULL;
	hpj = OSRNewSpatialReference( pzp );
	if(hpj != NULL) hll = OSRCloneGeogCS(hpj);
	if(hll != NULL)	{
	    CPLPushErrorHandler(CPLQuietErrorHandler);
	    /* The bit we need... */
	    htfm = OCTNewCoordinateTransformation(hpj, hll);
	    CPLPopErrorHandler();
	    OSRDestroySpatialReference(hll);
	}
	if (hpj != NULL)
	    OSRDestroySpatialReference(hpj);
    }

    if(htfm != NULL) {
	if (OCTTransform(htfm,1,&longitude,&latitude,NULL)) {
	    if (lat_center)
		*lat_center  = latitude;
	    if (long_center)
		*long_center = longitude;
	} else {
	    return 0;
	}
    } else {
	if (lat_center)
	    *lat_center  = latitude;
	if (long_center)
	    *long_center = longitude;
    }
    bu_log("lat: %f, long: %f\n", latitude, longitude);

    return 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
