/*                         K - G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
 #include <vector>
 #include "rt/db_io.h"
 #include "k_parser.h"
 #include "region_list.h"
 #include "pipe.h"
 #include "sketch.h"
 
 // Helper: interpolate missing nodes for ARB (best approximation)
 static void AddArbFlexible(
	 const std::vector<int>& nodes,
	 KData& kData,
	 const std::string& arbNumber,
	 Geometry& geometry,
	 const double factor
 ) {
	 point_t pts[8];
 
	 size_t nCount = nodes.size();
	 for (size_t i = 0; i < nCount; i++) {
		 pts[i][X] = kData.nodes[nodes[i]].x * factor;
		 pts[i][Y] = kData.nodes[nodes[i]].y * factor;
		 pts[i][Z] = kData.nodes[nodes[i]].z * factor;
	 }
 
	 // Interpolate missing nodes if less than 8
	 if (nCount < 8) {
		 for (size_t i = nCount; i < 8; i++) {
			 size_t prev = i - 1;
			 size_t first = 0;
			 pts[i][X] = 0.5 * (pts[prev][X] + pts[first][X]);
			 pts[i][Y] = 0.5 * (pts[prev][Y] + pts[first][Y]);
			 pts[i][Z] = 0.5 * (pts[prev][Z] + pts[first][Z]);
		 }
	 }
 
	 geometry.addArb(arbNumber.c_str(), pts[0], pts[1], pts[2], pts[3],
					  pts[4], pts[5], pts[6], pts[7]);
 }
 
 // Seatbelt creation helper
 static void AddSeatBelt(
	 int n1, int n2, float area,
	 KData& kData, Geometry& geometry,
	 const std::string& arbNumber, const double factor
 ) {
	 point_t center1, center2, pts[8];
	 float beltWidth = 47;
	 float beltThickness = area / beltWidth;
 
	 center1[X] = kData.nodes[n1].x * factor;
	 center1[Y] = kData.nodes[n1].y * factor;
	 center1[Z] = kData.nodes[n1].z * factor;
 
	 center2[X] = kData.nodes[n2].x * factor;
	 center2[Y] = kData.nodes[n2].y * factor;
	 center2[Z] = kData.nodes[n2].z * factor;
 
	 pts[0][X] = center1[X] - beltThickness * 0.5; pts[0][Y] = center1[Y] - 10; pts[0][Z] = center1[Z] - beltWidth * 0.5;
	 pts[1][X] = center1[X] + beltThickness * 0.5; pts[1][Y] = center1[Y] - 10; pts[1][Z] = center1[Z] - beltWidth * 0.5;
	 pts[2][X] = center1[X] + beltThickness * 0.5; pts[2][Y] = center1[Y] + 10; pts[2][Z] = center1[Z] + beltWidth * 0.5;
	 pts[3][X] = center1[X] - beltThickness * 0.5; pts[3][Y] = center1[Y] + 10; pts[3][Z] = center1[Z] + beltWidth * 0.5;
	 pts[4][X] = center2[X] - beltThickness * 0.5; pts[4][Y] = center2[Y] - 10; pts[4][Z] = center2[Z] - beltWidth * 0.5;
	 pts[5][X] = center2[X] + beltThickness * 0.5; pts[5][Y] = center2[Y] - 10; pts[5][Z] = center2[Z] - beltWidth * 0.5;
	 pts[6][X] = center2[X] + beltThickness * 0.5; pts[6][Y] = center2[Y] + 10; pts[6][Z] = center2[Z] + beltWidth * 0.5;
	 pts[7][X] = center2[X] - beltThickness * 0.5; pts[7][Y] = center2[Y] + 10; pts[7][Z] = center2[Z] + beltWidth * 0.5;
 
	 geometry.addArb(arbNumber.c_str(), pts[0], pts[1], pts[2], pts[3],
					  pts[4], pts[5], pts[6], pts[7]);
 }
 
 // Main program
 int main(int argc, char* argv[]) {
	 if (argc < 3) {
		 std::cerr << "Usage: " << argv[0] << " <keyword file> <BRL-CAD file>" << std::endl;
		 return 1;
	 }
 
	 KData kData;
	 if (!parse_k(argv[1], kData))
		 return 1;
 
	 rt_wdb* wdbp = wdb_fopen(argv[2]);
	 if (!wdbp) return 1;
 
	 RegionList regions;
	 const double factor = 10.0;
 
	 for (auto& [partId, part] : kData.parts) {
		 if (part.elements.empty()) continue;
 
		 std::string partName = std::to_string(partId) + "_" + part.title;
		 Geometry& geometry = regions.addRegion(partName);
 
		 int sectionId = part.section;
		 if (sectionId > 0) {
			 auto& sec = kData.sections[sectionId];
			 double averageThick = (sec.thickness1 + sec.thickness1 + sec.thickness1 + sec.thickness1) / 4.0;
			 geometry.setThickness(averageThick * factor);
		 }
 
		 for (int elemId : part.elements) {
			 auto& elem = kData.elements[elemId];
 
			 if (!elem.nodes.empty() && elem.nodes.size() >= 3 && elem.nodes.size() <= 8) {
				 AddArbFlexible(elem.nodes, kData, std::to_string(elemId), geometry, factor);
			 }
			 else if (!kData.elementSeatBelt[elemId].nodes.empty()) {
				 auto& sb = kData.sectionsSeatBelt[sectionId];
				 auto& sbElem = kData.elementSeatBelt[elemId];
 
				 if (sbElem.nodes.size() > 2 && sbElem.nodes[2] > 0) {
					 // Quad seatbelt
					 point_t p[4];
					 for (int i = 0; i < 4; i++) {
						 int n = sbElem.nodes[i];
						 p[i][X] = kData.nodes[n].x * factor;
						 p[i][Y] = kData.nodes[n].y * factor;
						 p[i][Z] = kData.nodes[n].z * factor;
					 }
					 geometry.addTriangle(p[0], p[1], p[2]);
					 geometry.addTriangle(p[0], p[2], p[3]);
				 } else {
					 AddSeatBelt(sbElem.nodes[0], sbElem.nodes[1], sb.area, kData, geometry,
								 std::to_string(elemId), factor);
				 }
			 }
			 else if (kData.elementDiscreteSphere[elemId].radius > 0) {
				 auto& sphere = kData.elementDiscreteSphere[elemId];
				 point_t center;
				 int centerId = sphere.nodeId;
				 center[X] = kData.nodes[centerId].x * factor;
				 center[Y] = kData.nodes[centerId].y * factor;
				 center[Z] = kData.nodes[centerId].z * factor;
 
				 geometry.addSphere(std::to_string(elemId).c_str(), center, sphere.radius * factor);
			 }
			 else {
				 std::cerr << "Unsupported element #" << elemId << " in k-file " << argv[1] << std::endl;
			 }
		 }
 
		 regions.setAttributes(partName, part.attributes);
	 }
 
	 regions.create(wdbp);
	 regions.printStat();
 
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