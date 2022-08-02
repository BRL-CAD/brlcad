/*                         U S A G E . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2022 United States Government as represented by
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
/** @file rt/usage.c
 *
 * This file prints out all of the options that pertain to most of the
 * RTUIF applications.  The usage function (or a wrapped version) is
 * registered by the application during initialization.
 */

#include "common.h"

#include <map>
#include <string>
#include <deque>
#include <algorithm>

#include "bu/log.h"

#include "./ext.h"


struct opt_info {
    std::string option;
    std::string description;
    int verbose;
};

/* category to id mapping (so they remain in insert order) */
static std::map<const std::string, size_t> cats;

/* id to option vector mapping */
static std::map<size_t, std::vector<struct opt_info> > opts;
static bool map_initialized = false;


extern "C" {

/* called recursively */
static void default_options(void);


void
option(const char *cat, const char *opt, const char *des, int verbose)
{
    if (!cat)
	cat = "";
    if (!opt)
	opt = "";
    if (!des)
	des = "";

    /* first insert initializes */
    if (!map_initialized) {
	map_initialized = true;
	default_options();
    }

    /* get the category's id */
    size_t idx;
    if (cats[cat] == 0)
	cats[cat] = cats.size();
    idx = cats[cat];

    /* see if option is already listed */
    if (opt[0] != '\0' && opt[0] == '-') {
	for (std::map<size_t, std::vector<struct opt_info> >::iterator i = opts.begin(); i != opts.end(); ++i) {
	    for (std::vector<struct opt_info>::iterator j = i->second.begin(); j != i->second.end(); ++j) {
		if (j->option[0] != opt[0] || j->option[1] != opt[1])
		    continue;

		/* first two chars match and same category, we replace */
		if (i->first == idx) {
		    j->option = opt;
		    j->description = des;
		    j->verbose = verbose;
		    return;
		} else {
		    i->second.erase(j);
		    break;
		}
	    }
	}
    }

    const std::string optstr = opt;
    const std::string desstr = des;
    struct opt_info new_option = {optstr, desstr, verbose};
    opts[idx].push_back(new_option);
}


static void
default_options(void)
{
    option("", "-o filename", "Render to specified image file (e.g., image.png or image.pix)", 0);
    option("", "-F framebuffer", "Render to a framebuffer (defaults to a window)", 0);
    option("", "-s #", "Square image size (default: 512 - implies 512x512 image)", 0);
    option("", "-w # -n #", "Image pixel dimensions as width and height", 0);
    option("", "-C #/#/#", "Set background image color to R/G/B values (default: 0/0/1)", 0);
    option("", "-W", "Set background image color to white", 0);
    option("", "-R", "Disable reporting of overlaps", 0);
    option("", "-? or -h", "Display help", 1);

    option("Raytrace", "-a # -e #", "Azimuth and elevation in degrees (default: -a 35 -e 25)", 1);
    option("Raytrace", "-p #", "Perspective angle, degrees side to side (0 <= # < 180)", 1);
    option("Raytrace", "-E #", "Set perspective eye distance from model (default: 1.414)", 1);
    option("Raytrace", "-H #", "Specify number of hypersample rays per pixel (default: 0)", 1);
    option("Raytrace", "-J #", "Specify a \"jitter\" pattern (default: 0 - no jitter)", 1);
    option("Raytrace", "-P #", "Specify number of processors to use (default: all available)", 1);
    option("Raytrace", "-T # or -T #/#", "Tolerance as distance or distance/angular", 1);

    option("Advanced", "-c \"command\"", "Runs a semicolon-separated list of commands (related: -M)", 1);
    option("Advanced", "-M", "Read matrix + commands on stdin (RT 'saveview' scripts)", 1);
    option("Advanced", "-D #", "Specify starting frame number (ending is specified via -K #)", 1);
    option("Advanced", "-K #", "Specify ending frame number (starting is specified via -D #)", 1);
    option("Advanced", "-g #", "Specify grid cell (pixel) width, in millimeters", 1);
    option("Advanced", "-G #", "Specify grid cell (pixel) height, in millimeters", 1);
    option("Advanced", "-S", "Enable stereo rendering", 1);
    option("Advanced", "-U #", "Turn on air region rendering (default: 0 - off)", 1);
    option("Advanced", "-V #", "View (pixel) aspect ratio (width/height)", 1);
    option("Advanced", "-j xmin,xmax,ymin,ymax", "Only render pixels within the specified sub-rectangle", 1);
    option("Advanced", "-k xdir,ydir,zdir,dist", "Specify a cutting plane for the entire render scene", 1);

    option("Developer", "-v [#]", "Specify or increase RT verbosity", 1);
    option("Developer", "-X #", "Specify RT debugging flags", 1);
    option("Developer", "-x #", "Specify librt debugging flags", 1);
    option("Developer", "-N #", "Specify libnmg debugging flags", 1);
    option("Developer", "-! #", "Specify libbu debugging flags", 1);
    option("Developer", "-, #", "Specify space partitioning algorithm", 1);
    option("Developer", "-B", "Disable randomness for \"benchmark\"-style repeatability", 1);
    option("Developer", "-b \"x y\"", "Only shoot one ray at pixel coordinates (quotes required)", 1);
    option("Developer", "-Q x,y", "Shoot one pixel with debugging; compute others without", 1);
#ifdef USE_OPENCL
    option("Developer", "-z #", "Turn on OpenCL ray-trace engine (default: 0 - off)", 1);
#endif

/* intentionally not listed:
 *   -u units -- because it only applies to rtarea
 *   -r -- only reserved to pair with -R (lower/upper on/off convention)
 */
}


/* default usage callback for main() */
void
usage(const char *argv0, int verbose)
{
    bu_log("\nUsage:  %s [options] model.g objects...\n", argv0);

    /* use defaults if none were loaded */
    if (!map_initialized) {
	default_options();
    }

    /* find the longest options enabled */
    size_t longest_opt = 0;
    size_t longest_desc = 0;
    int max_verbosity = 0;
    for (std::map<size_t, std::vector<struct opt_info> >::const_iterator i = opts.begin(); i != opts.end(); ++i) {
	for (std::vector<struct opt_info>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
	    if (max_verbosity < j->verbose)
		max_verbosity = j->verbose;

	    if (verbose < j->verbose)
		continue;

	    size_t olen = (size_t)j->option.length();
	    if (longest_opt < olen)
		longest_opt = olen;
	    size_t dlen = (size_t)j->description.length();
	    if (longest_desc < dlen)
		longest_desc = dlen;
	}
    }

    /* iterate over all categories, all options, and print them */
    for (std::map<size_t, std::vector<struct opt_info> >::const_iterator i = opts.begin(); i != opts.end(); ++i) {
	/* count how many will print at this verbosity level */
	size_t enabled = 0;
	for (std::vector<struct opt_info>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
	    if (verbose < j->verbose || j->verbose < 0)
		continue;
	    enabled++;
	}
	if (enabled == 0)
	    continue;

	/* lookup our category label, print it */
	std::string category = "";
	for (std::map<const std::string, size_t>::const_iterator cat = cats.begin(); cat != cats.end(); ++cat) {
	    if (cat->second == i->first) {
		category = cat->first.c_str();
		break;
	    }
	}
	if (category != "")
	    bu_log("\n%s Options:\n", category.c_str());
	else
	    bu_log("Options:\n");

	/* print our option lines */
	for (std::vector<struct opt_info>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
	    if (verbose < j->verbose || j->verbose < 0)
		continue;

	    /* long option strings are printed on lines by themselves
	     * unless all option+description lines fit within 80chars
	     */
	    if (longest_opt + longest_desc + 4 /* option padding */ <= 80) {
		bu_log("  %s%*s  %s\n", j->option.c_str(), (int)(longest_opt-(int)j->option.length()), "", j->description.c_str());
	    } else {

		/* arbitrary, 16 makes a nice 20/60 split */
		size_t limitation = 16;
		if (longest_opt < limitation)
		    limitation = longest_opt;

		if (j->option.length() > limitation) {
		    bu_log("  %s\n", j->option.c_str());
		    if (j->description.length() > 0)
			bu_log("    %*s%s\n", (int)limitation, "", j->description.c_str());
		} else {
		    bu_log("  %s%*s  %s\n", j->option.c_str(), (int)(limitation-(int)j->option.length()), "", j->description.c_str());
		}
	    }
	}
    }
    if (verbose < max_verbosity) {
	bu_log("\nType \"%s -?\" for a complete list of options.\n", argv0);
    }
    bu_log("\n");
}

} /* extern "C" */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

