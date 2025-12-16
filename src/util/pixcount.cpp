/*                    P I X C O U N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1998-2025 United States Government as represented by
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
/** @file util/pixcount.cpp
 *
 * Sort the pixels of an input stream by color value.
 */

#include "common.h"
#include "bio.h"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <iterator>

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"

static void print_usage()
{
    static const char usage[] = "Usage: pixcount [-# bytes_per_pixel] [infile.pix [outfile]]\n";
    bu_exit(1, "%s", usage);
}

int main(int argc, char **argv)
{
    int pixel_size = 3; // Bytes per pixel
    FILE *outfp = nullptr;

    bu_setprogname(argv[0]);

#ifdef HAVE_SETMODE
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    // Parse options
    int ch;
    while ((ch = bu_getopt(argc, argv, "#:h?")) != -1) {
	switch (ch) {
	    case '#':
		if (sscanf(bu_optarg, "%d", &pixel_size) != 1) {
		    bu_log("Invalid pixel size: '%s'\n", bu_optarg);
		    print_usage();
		}
		break;
	    default:
		print_usage();
	}
    }

    FILE *infp = nullptr;
    std::string inf_name, outf_name;
    switch (argc - bu_optind) {
	case 0:
	    infp = stdin;
	    outfp = stdout;
	    break;
	case 1:
	    inf_name = argv[bu_optind];
	    infp = fopen(inf_name.c_str(), "rb");
	    if (!infp) bu_exit(1, "Cannot open input file '%s'\n", inf_name.c_str());
	    outfp = stdout;
	    break;
	case 2:
	    inf_name = argv[bu_optind];
	    outf_name = argv[bu_optind + 1];
	    infp = fopen(inf_name.c_str(), "rb");
	    if (!infp) bu_exit(1, "Cannot open input file '%s'\n", inf_name.c_str());
	    outfp = fopen(outf_name.c_str(), "wb");
	    if (!outfp) bu_exit(1, "Cannot open output file '%s'\n", outf_name.c_str());
	    break;
	default:
	    print_usage();
    }

    if (infp == stdin && isatty(fileno(stdin))) {
	bu_log("FATAL: pixcount reads only from file or pipe\n");
	print_usage();
    }

    // Use std::map to store pixel color (as vector) -> count
    std::map<std::vector<unsigned char>, int> palette;

    // Read pixels
    std::vector<unsigned char> buf(pixel_size);
    while (fread(buf.data(), pixel_size, 1, infp) == 1) {
	palette[buf]++;
    }

    if (infp != stdin && infp)
	fclose(infp);

    // Output sorted by color
    for (const auto &kv : palette) {
	for (int i = 0; i < pixel_size; ++i)
	    fprintf(outfp, "%3d ", kv.first[i]);
	fprintf(outfp, " %d\n", kv.second);
    }

    if (outfp != stdout && outfp)
	fclose(outfp);

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
