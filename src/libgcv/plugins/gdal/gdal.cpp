/*                             G D A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 Tom Browder
 * Copyright (c) 2017 United States Government as represented by the U.S. Army
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
#include <gdal.h>
#include <gdal_priv.h> // GDALDataset
#include <cpl_conv.h>
#include <ogr_spatialref.h>

#include "raytrace.h"
#include "gcv/api.h"
#include "gcv/util.h"

struct gdal_state {
    OGRSpatialReference *sp;
    GDALDataset *dataset;
    bool info;
    bool debug;
    int scalex;
    int scaley;
    int scalez;
    double adfGeoTransform[6];
    int az;
    int el;
    int pixsize;
};

HIDDEN void
gdal_state_init(struct gdal_state *gs)
{
    int i = 0;
    if(!gs) return;
    gs->sp = NULL;
    gs->dataset = NULL;
    gs->info = false;
    gs->debug = false;
    gs->scalex = 1;
    gs->scaley = 1;
    gs->scalez = 1;
    for(i = 0; i < 6; i++) gs->adfGeoTransform[i] = 0.0;
    gs->az = 35;
    gs->el = 35;
    gs->pixsize = 512 * 3;
}

std::string
_gdal_get_spaces(const int n)
{
    std::string s("");
    for (int i = 0; i < n; ++i)	s += "  ";
    return s;
}

void
_gdal_show_node_and_children(const OGRSpatialReference* sp,
	const OGR_SRSNode* parent, const char* pname, const int level)
{
    std::string spaces = _gdal_get_spaces(level);
    int nc = parent->GetChildCount();
    printf("  %s%s [%d children]:\n", spaces.c_str(), pname, nc);
    for (int j = 0; j < nc; ++j) {
	const char* cname = sp->GetAttrValue(pname, j);
	// the child may be a  parent
	const OGR_SRSNode* cnode = sp->GetAttrNode(cname);
	if (cnode) {
	    _gdal_show_node_and_children(sp, cnode, cname, level+1);
	} else {
	    bu_log("    %d: '%s'\n", j, cname);
	}
    }
}


HIDDEN int
get_dataset_info(struct gdal_state *gs)
{
    // Getting Dataset Information
    // ---------------------------
    //
    // As described in the GDAL Data Model, a GDALDataset contains a
    // list of raster bands, all pertaining to the same area, and having
    // the same resolution. It also has metadata, a coordinate system, a
    // georeferencing transform, size of raster and various other
    // information.

    // adfGeoTransform[0] /* top left x */
    // adfGeoTransform[1] /* w-e pixel resolution */
    // adfGeoTransform[2] /* rotation, 0 if image is "north up" */
    // adfGeoTransform[3] /* top left y */
    // adfGeoTransform[4] /* rotation, 0 if image is "north up" */
    // adfGeoTransform[5] /* n-s pixel resolution */

    // If we wanted to print some general information about the dataset
    // we might do the following:

    if (gs->dataset->GetGeoTransform(gs->adfGeoTransform) == CE_None) {
	if (gs->info) {
	    printf("Origin = (%.6f,%.6f)\n", gs->adfGeoTransform[0], gs->adfGeoTransform[3]);
	    printf("Pixel Size = (%.6f,%.6f)\n", gs->adfGeoTransform[1], gs->adfGeoTransform[5]);
	}
    }

    gs->scalex = static_cast<int>(floor(gs->adfGeoTransform[1]));
    gs->scaley = static_cast<int>(floor(gs->adfGeoTransform[5]));
    // use negative of scaley since we reverse the output
    gs->scaley *= -1;

    if (!gs->info && (gs->scalex != gs->scaley)) {
	bu_log("FATAL: cell scale x (%d) != cell scale y (%d)\n", gs->scalex, gs->scaley);
	return -1;
    }
    gs->scalez = 1;

    // check scalez
    if (gs->dataset->GetProjectionRef()) {
	const char* s = gs->dataset->GetProjectionRef();
	if (!gs->sp) gs->sp = new OGRSpatialReference(s);
	std::string unit(gs->sp->GetAttrValue("UNIT", 0));
	if (unit != "Meter") {
	    bu_log("FATAL:  Cell unit is '%s' instead of 'Meter'.\n", unit.c_str());
	    return -1;
	}
	else {
	    int val = 0;
	    const char *uval = gs->sp->GetAttrValue("UNIT", 1);
	    (void)bu_opt_int(NULL, 1, &uval, (void *)(&val));
	    if (val != 1) {
		bu_log("FATAL:  Cell z scale is '%d' instead of '1'.\n", val);
		return -1;
	    }
	}
    }

    if (gs->info) {
	char** flist2 = gs->dataset->GetFileList();
	CPLStringList flist(flist2, false);
	int nf = flist.size();
	if (nf) {
	    bu_log("Data set files:\n");
	    for (int i = 0; i < nf; ++i) {
		const CPLString& s = flist[i];
		bu_log("  %s\n", s.c_str());
	    }
	}
	CSLDestroy(flist2);

	char** dlist2 = gs->dataset->GetMetadata();
	CPLStringList dlist(dlist2, false);
	int nd = dlist.size();
	if (nd) {
	    bu_log("Dataset Metadata:\n");
	    for (int i = 0; i < nd; ++i) {
		const CPLString& d = dlist[i];
		bu_log("  %s\n", d.c_str());
	    }
	}

	GDALDriver* driver = gs->dataset->GetDriver();

	char** mlist2 = driver->GetMetadata();
	CPLStringList mlist(mlist2, false);
	int nm = mlist.size();
	if (nm) {
	    bu_log("Driver Metadata:\n");
	    for (int i = 0; i < nm; ++i) {
		const CPLString& m = mlist[i];
		bu_log("  %s\n", m.c_str());
	    }
	}

	bu_log("Driver: %s/%s\n", driver->GetDescription(), driver->GetMetadataItem(GDAL_DMD_LONGNAME));
	bu_log("Size is %dx%dx%d\n", gs->dataset->GetRasterXSize(), gs->dataset->GetRasterYSize(), gs->dataset->GetRasterCount());

	if (gs->dataset->GetProjectionRef()) {
	    const char* s = gs->dataset->GetProjectionRef();
	    if (!gs->sp)
		gs->sp = new OGRSpatialReference(s);

	    std::vector<std::string> nodes;
	    nodes.push_back("PROJCS");
	    nodes.push_back("GEOGCS");
	    nodes.push_back("DATUM");
	    nodes.push_back("SPHEROID");
	    nodes.push_back("PROJECTION");

	    bu_log("Projection is:\n");

	    for (unsigned i = 0; i < nodes.size(); ++i) {
		const char* name = nodes[i].c_str();
		const OGR_SRSNode* node = gs->sp->GetAttrNode(name);
		if (!node) {
		    bu_log("  %s (NULL)\n", name);
		    continue;
		}
		int level = 0;
		_gdal_show_node_and_children(gs->sp, node, name, level); // recursive
	    }

	    /*
	       const char* projcs   = sp.GetAttrNode("PROJCS");

	       const char* geogcs   = sp.GetAttrValue("GEOGCS");
	       const char* datum    = sp.GetAttrValue("DATUM");
	       const char* spheroid = sp.GetAttrValue("SPHEROID");
	       const char* project  = sp.GetAttrValue("PROJECTION");

	       printf("  %s\n", projcs);
	       printf("  %s\n", geogcs);
	       printf("  %s\n", datum);
	       printf("  %s\n", spheroid);
	       printf("  %s\n", project);

            */
	    //printf("Projection is '%s'\n", dataset->GetProjectionRef());
	}

    }
    return 0;
}


HIDDEN int
gdal_read(struct gcv_context *UNUSED(context), const struct gcv_opts *UNUSED(gcv_options),
	               const void *UNUSED(options_data), const char *UNUSED(dest_path))
{
    return 1;
}

extern "C"
{
    struct gcv_filter gcv_conv_gdal_read =
    {"GDAL Reader", GCV_FILTER_READ, BU_MIME_MODEL_AUTO, NULL, NULL, gdal_read};

    static const struct gcv_filter * const filters[] = {&gcv_conv_gdal_read, NULL};
    const struct gcv_plugin gcv_plugin_info_s = { filters };
    GCV_EXPORT const struct gcv_plugin *gcv_plugin_info(){return &gcv_plugin_info_s;}
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
