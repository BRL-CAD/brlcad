/*                         R A W - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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

#include "RegionList.h"


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
    int        ret = 0;
    RegionList regions;

    if (argc < 3) {
	std::cout << "Usage: " << argv[0] << " <RAW file> <BRL-CAD file>" << std::endl;
	ret = 1;
    }
    else {
	std::ifstream is(argv[1]);

	if (!is.is_open()) {
	    std::cout << "Error reading RAW file" << std::endl;
	    ret = 1;
	}
	else {
	    struct rt_wdb* wdbp = wdb_fopen(argv[2]);
	    std::string title = "Converted from ";
	    title += argv[1];

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
		    int a = bot.addPoint(p);

		    getPoint(triangleLine[3], triangleLine[4], triangleLine[5], p);
		    int b = bot.addPoint(p);

		    getPoint(triangleLine[6], triangleLine[7], triangleLine[8], p);
		    int c = bot.addPoint(p);

		    bot.addTriangle(a, b, c);

		    triangleLine = readLine(is);
		}

		nameLine = triangleLine;
	    }

	    regions.create(wdbp);
	    wdb_close(wdbp);
	}
    }

    regions.printStat();

    return ret;
}
