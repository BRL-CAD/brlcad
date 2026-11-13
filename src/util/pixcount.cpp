/*                    P I X C O U N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
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
#include <climits>
#include <vector>
#include <map>
#include <string>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"

struct pixcount_args {
    int help;
    int pixel_size;
};

static const struct bu_arg_option pixcount_options[] = {
    BU_ARG_FLAG("h", "help", struct pixcount_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_INTEGER_RANGE("#", "bytes-per-pixel", struct pixcount_args,
	pixel_size, 1, 1, 1, INT_MAX, "bytes", "Bytes in each pixel"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixcount_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input image (default: standard input)", NULL),
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 0, 1,
	"Output tally (default: standard output)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixcount_schema = BU_ARG_SCHEMA(
    "pixcount", "Count occurrences of each unique pixel value",
    pixcount_options, pixcount_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixcount_grammar = {
    &pixcount_schema, NULL, NULL
};

int main(int argc, char **argv)
{
    struct pixcount_args args = {0, 3};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    FILE *outfp = nullptr;

    bu_setprogname(argv[0]);

#ifdef HAVE_SETMODE
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    int introspection = bu_arg_introspect(&msg, &pixcount_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixcount_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixcount_schema, "pixcount");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixcount help");
	return 0;
    }

    argc -= operand_index + 1;
    argv += operand_index + 1;

    FILE *infp = nullptr;
    std::string inf_name, outf_name;
    switch (argc) {
	case 0:
	    infp = stdin;
	    outfp = stdout;
	    break;
	case 1:
	    inf_name = argv[0];
	    infp = fopen(inf_name.c_str(), "rb");
	    if (!infp) bu_exit(1, "Cannot open input file '%s'\n", inf_name.c_str());
	    outfp = stdout;
	    break;
	case 2:
	    inf_name = argv[0];
	    outf_name = argv[1];
	    infp = fopen(inf_name.c_str(), "rb");
	    if (!infp) bu_exit(1, "Cannot open input file '%s'\n", inf_name.c_str());
	    outfp = fopen(outf_name.c_str(), "wb");
	    if (!outfp) bu_exit(1, "Cannot open output file '%s'\n", outf_name.c_str());
	    break;
    }

    if (infp == stdin && isatty(fileno(stdin))) {
	bu_log("FATAL: pixcount reads only from file or pipe\n");
	return 1;
    }

    // Use std::map to store pixel color (as vector) -> count
    std::map<std::vector<unsigned char>, int> palette;

    // Read pixels
    std::vector<unsigned char> buf(args.pixel_size);
    while (fread(buf.data(), args.pixel_size, 1, infp) == 1) {
	palette[buf]++;
    }

    if (infp != stdin && infp)
	fclose(infp);

    // Output sorted by color
    for (const auto &kv : palette) {
	for (int i = 0; i < args.pixel_size; ++i)
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
