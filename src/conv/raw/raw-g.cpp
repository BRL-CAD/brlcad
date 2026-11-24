/*                         R A W - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file raw-g.cpp
 *
 * RAW geometry file to BRL-CAD converter:
 * main function
 *
 *  Origin -
 *        IABG mbH (Germany)
 */

#include "common.h"

#include <cassert>
#include <cstdio>
#include <iostream>

#ifndef HAVE_DECL_FSEEKO
#include "bio.h" /* for b_off_t */
extern "C" int fseeko(FILE *, b_off_t, int);
extern "C" b_off_t ftello(FILE *);
#endif
#include <fstream>

#include "RegionList.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"

struct raw_g_args {
    int help;
};

static const struct bu_arg_option raw_g_options[] = {
    BU_ARG_FLAG("h", "help", struct raw_g_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand raw_g_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 1, 1,
	"Input RAW triangle file", NULL),
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 1, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema raw_g_schema = BU_ARG_SCHEMA(
    "raw-g", "Convert RAW triangle geometry to a BRL-CAD database",
    raw_g_options, raw_g_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar raw_g_grammar = {
    &raw_g_schema, NULL, NULL
};

static std::vector<std::string> readLine(std::istream& is)
{
    std::vector<std::string> ret;
    std::string              temp;

    for (;;) {
	if (!is)
	    break;

	char a;
	is.get(a);

	if (a == '\n') {
	    if (temp.size() > 0) {
		ret.push_back(temp);
		temp.clear();
	    }

	    break;
	}

	if ((a == ' ') || (a == '\t') || (a == '\r')) {
	    if (temp.size() > 0) {
		ret.push_back(temp);
		temp.clear();
	    }
	}
	else
	    temp += a;
    }

    return ret;
}


static inline void
getPoint(const std::string &x, const std::string &y, const std::string &z, point_t& point) {
    point[X] = toValue(x.c_str());
    point[Y] = toValue(y.c_str());
    point[Z] = toValue(z.c_str());
}

int main(int   argc,
	 char* argv[])
{
    struct raw_g_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = 0;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &raw_g_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&raw_g_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&raw_g_schema, "raw-g");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "raw-g help");
	return 0;
    }
    const char *raw_file = argv[operand_index + 1];
    const char *database = argv[operand_index + 2];

    RegionList regions;

    std::ifstream is(raw_file);

    if (!is.is_open()) {
	std::cout << "Error reading RAW file" << std::endl;
	ret = 1;

    } else {
	    struct rt_wdb* wdbp = wdb_fopen(database);
	    std::string title = "Converted from ";
	    title += raw_file;

	    mk_id(wdbp, title.c_str());

	    std::vector<std::string> nameLine = readLine(is);

	    while (is && !is.eof()) {
		if (nameLine.size() == 0) {
		    nameLine = readLine(is);
		    continue;
		}

		std::cout << "Read: " << nameLine[0].c_str() << '\n';
		assert(nameLine[0].size() > 0);

		Bot& bot = regions.addRegion(nameLine[0]);

		if (nameLine.size() > 1) {
		    size_t thicknessIndex = nameLine[1].find("thickf=");

		    if (thicknessIndex != std::string::npos) {
			std::string thickf = nameLine[1].substr(thicknessIndex + 7);

			if (thickf.size() > 0) {
			    fastf_t val = toValue(thickf.c_str());
			    bot.setThickness(val);
			} else {
			    std::cout << "Missing thickness in " << nameLine[0].c_str() << '\n';
			}
		    }
		}

		std::vector<std::string> triangleLine = readLine(is);

		while (is && (triangleLine.size() == 9)) {
		    point_t p;

		    getPoint(triangleLine[0], triangleLine[1], triangleLine[2], p);
		    size_t a = bot.addPoint(p);

		    getPoint(triangleLine[3], triangleLine[4], triangleLine[5], p);
		    size_t b = bot.addPoint(p);

		    getPoint(triangleLine[6], triangleLine[7], triangleLine[8], p);
		    size_t c = bot.addPoint(p);

		    bot.addTriangle(a, b, c);

		    triangleLine = readLine(is);
		}

		nameLine = triangleLine;
	    }

	    regions.create(wdbp);
	    db_close(wdbp->dbip);
    }

    regions.printStat();

    return ret;
}
