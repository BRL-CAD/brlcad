/*                         K - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file k-g.cpp
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * main function
 *
 * Usage: k-g input.k output.g
 */

#include "common.h"

#include <iostream>

#include "rt/db_io.h"

#include "k_parser.h"
#include "region_list.h"


static void AddArb(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8,KData& kData, std::string arbNumber, Geometry& geometry) {
    point_t point1;
    point_t point2;
    point_t point3;
    point_t point4;
    point_t point5;
    point_t point6;
    point_t point7;
    point_t point8;

    const double factor = 10.0;

    point1[X] = kData.nodes[n1].x * factor;
    point1[Y] = kData.nodes[n1].y * factor;
    point1[Z] = kData.nodes[n1].z * factor;

    point2[X] = kData.nodes[n2].x * factor;
    point2[Y] = kData.nodes[n2].y * factor;
    point2[Z] = kData.nodes[n2].z * factor;

    point3[X] = kData.nodes[n3].x * factor;
    point3[Y] = kData.nodes[n3].y * factor;
    point3[Z] = kData.nodes[n3].z * factor;

    point4[X] = kData.nodes[n4].x * factor;
    point4[Y] = kData.nodes[n4].y * factor;
    point4[Z] = kData.nodes[n4].z * factor;

    point5[X] = kData.nodes[n5].x * factor;
    point5[Y] = kData.nodes[n5].y * factor;
    point5[Z] = kData.nodes[n5].z * factor;

    point6[X] = kData.nodes[n6].x * factor;
    point6[Y] = kData.nodes[n6].y * factor;
    point6[Z] = kData.nodes[n6].z * factor;

    point7[X] = kData.nodes[n7].x * factor;
    point7[Y] = kData.nodes[n7].y * factor;
    point7[Z] = kData.nodes[n7].z * factor;

    point8[X] = kData.nodes[n8].x * factor;
    point8[Y] = kData.nodes[n8].y * factor;
    point8[Z] = kData.nodes[n8].z * factor;

    geometry.addArb(arbNumber.c_str(), point1, point2, point3, point4, point5, point6, point7, point8);
}


int main
(
    int   argc,
    char* argv[]
) {
    int        ret = 0;
    RegionList regions;

    if (argc < 3) {
	std::cerr << "Usage: " << argv[0] << " <keyword file> <BRL-CAD file>" << std::endl;
	ret = 1;
    }
    else {
	KData kData;

	if (!parse_k(argv[1], kData))
	    ret = 1;
	else {
	    rt_wdb* wdbp = wdb_fopen(argv[2]);

	    if (wdbp != nullptr) {
		std::string title = "Converted from ";
		title += argv[1];

		db_update_ident(wdbp->dbip, title.c_str(), wdbp->dbip->dbi_base2local);

		const double factor = 10.0;

		for (std::map<int, KPart>::iterator it = kData.parts.begin(); it != kData.parts.end(); it++) {
		    if ((it->second).elements.size() > 0) {
			std::string partName = std::to_string(it->first) + "_" + (it->second).title;

			Geometry& geometry = regions.addRegion(partName);
			int    section = (it->second).section;

			if (section > 0) {
			    double thick1 = kData.sections[section].thickness1;
			    double thick2 = kData.sections[section].thickness1;
			    double thick3 = kData.sections[section].thickness1;
			    double thick4 = kData.sections[section].thickness1;

			    if (!((fabs(thick1 - thick2) < SMALL_FASTF) && (fabs(thick2 - thick3) < SMALL_FASTF) && (fabs(thick3 - thick4) < SMALL_FASTF))) {
				std::string sectionName = std::to_string(section) + ": " + kData.sections[section].title;
				std::cout << "Different thicknesses in section " << sectionName.c_str() << '\n';
			    }

			    double averageThick = (thick1 + thick2 + thick3 + thick4) / 4.0;
			    geometry.setThickness(averageThick * factor);
			}
			else
			    std::cout << "Missing section to part" << partName.c_str() << '\n';

			for (std::set<int>::iterator itr = (it->second).elements.begin(); itr != (it->second).elements.end(); itr++) {
			    if ((kData.elements[*itr].nodes.size() == 3)) {
				int x = 1;
				KSectionBeam beamSection = kData.sectionsBeam[section];
				//check the first element Beam
			    }
			    else if (kData.elements[*itr].nodes.size() == 4) {

				point_t point1;
				point_t point2;
				point_t point3;
				point_t point4;

				int     n1 = kData.elements[*itr].nodes[0];
				int     n2 = kData.elements[*itr].nodes[1];
				int     n3 = kData.elements[*itr].nodes[2];
				int     n4 = kData.elements[*itr].nodes[3];

				point1[X] = kData.nodes[n1].x * factor;
				point1[Y] = kData.nodes[n1].y * factor;
				point1[Z] = kData.nodes[n1].z * factor;

				point2[X] = kData.nodes[n2].x * factor;
				point2[Y] = kData.nodes[n2].y * factor;
				point2[Z] = kData.nodes[n2].z * factor;

				point3[X] = kData.nodes[n3].x * factor;
				point3[Y] = kData.nodes[n3].y * factor;
				point3[Z] = kData.nodes[n3].z * factor;

				point4[X] = kData.nodes[n4].x * factor;
				point4[Y] = kData.nodes[n4].y * factor;
				point4[Z] = kData.nodes[n4].z * factor;

				if (n3 == n4) {
				    geometry.addTriangle(point1, point2, point3);
				}
				else {
				    geometry.addTriangle(point1, point2, point3);
				    geometry.addTriangle(point1, point3, point4);
				}
			    }
			    else if (kData.elements[*itr].nodes.size() == 8) {
				int     n1 = kData.elements[*itr].nodes[0];
				int     n2 = kData.elements[*itr].nodes[1];
				int     n3 = kData.elements[*itr].nodes[2];
				int     n4 = kData.elements[*itr].nodes[3];
				int     n5 = kData.elements[*itr].nodes[4];
				int     n6 = kData.elements[*itr].nodes[5];
				int     n7 = kData.elements[*itr].nodes[6];
				int     n8 = kData.elements[*itr].nodes[7];

				std::string arbNumber = std::to_string(*itr);

				AddArb(n1, n2, n3, n4, n5, n6, n7, n8, kData, arbNumber, geometry);


			    }
			    else if (kData.elements[*itr].nodes.size() == 7) {
				int     n1 = kData.elements[*itr].nodes[0];
				int     n2 = kData.elements[*itr].nodes[1];
				int     n3 = kData.elements[*itr].nodes[2];
				int     n4 = kData.elements[*itr].nodes[3];
				int     n5 = kData.elements[*itr].nodes[4];
				int     n6 = kData.elements[*itr].nodes[5];
				int     n7 = kData.elements[*itr].nodes[6];
				int     n8 = kData.elements[*itr].nodes[4];

				std::string arbNumber = std::to_string(*itr);

				AddArb(n1, n2, n3, n4, n5, n6, n7, n8, kData, arbNumber, geometry);
			    }
			    else if (kData.elements[*itr].nodes.size() == 6) {
				int     n1 = kData.elements[*itr].nodes[0];
				int     n2 = kData.elements[*itr].nodes[1];
				int     n3 = kData.elements[*itr].nodes[2];
				int     n4 = kData.elements[*itr].nodes[3];
				int     n5 = kData.elements[*itr].nodes[4];
				int     n6 = kData.elements[*itr].nodes[4];
				int     n7 = kData.elements[*itr].nodes[5];
				int     n8 = kData.elements[*itr].nodes[5];

				std::string arbNumber = std::to_string(*itr);

				AddArb(n1, n2, n3, n4, n5, n6, n7, n8, kData, arbNumber, geometry);

			    }
			    else if (kData.elements[*itr].nodes.size() == 5) {
				int     n1 = kData.elements[*itr].nodes[0];
				int     n2 = kData.elements[*itr].nodes[1];
				int     n3 = kData.elements[*itr].nodes[2];
				int     n4 = kData.elements[*itr].nodes[3];
				int     n5 = kData.elements[*itr].nodes[4];
				int     n6 = kData.elements[*itr].nodes[4];
				int     n7 = kData.elements[*itr].nodes[4];
				int     n8 = kData.elements[*itr].nodes[4];

				std::string arbNumber = std::to_string(*itr);

				AddArb(n1, n2, n3, n4, n5, n6, n7, n8, kData, arbNumber, geometry);
			    }
			    else
				std::cout << "Un supported element in k-file " << argv[1] << "\n";
			}

			regions.setAttributes(partName,(it->second).attributes);
		    }
		}

		regions.create(wdbp);
	    }
	}
    }

    regions.printStat();

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
