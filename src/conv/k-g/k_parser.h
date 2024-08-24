/*                      K _ P A R S E R . H
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
/** @file k_parser.h
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * k-file parsing function declaration
 */

#ifndef KPARSER_INCLUDED
#define KPARSER_INCLUDED

#include "common.h"

#include <map>
#include <set>
#include <string>
#include<vector>


struct KNode {
    double x;
    double y;
    double z;
};


struct KElement {
    std::vector<int>                           nodes;
    std::map<std::string, std::vector<double>> options;
};


struct KElementPulley {
    int truss1Id;
    int truss2Id;
    int pulleyNode;
};


struct KElementBeamSource {
    int   sourceNodeId;
    int   sourceElementId;
    int   nElements;
    float beamElementLength;
    float minmumLengthToPullOut;
};


struct KElementBearing {
    std::string title;
    int         bearingType;
    int         n1;
    int         coordinateId1;
    int         n2;
    int         coordinateId2;
    int         numberOfBallsOrRollers;
    float       diameterOfBallsOrRollers;
    float       boreInnerDiameter;
    float       boreOuterDiameter;
    float       pitchDiameter;
    float       innerGroveRadiusToBallDiameterRatioOrRollerLength;
    float       outerRaceGrooveRadiusToBallDiameterRatio;
    float       totalRadianceClearenceBetweenBallAndRaces;
};


struct KPart {
    std::string                        title;
    std::set<int>                      elements;
    int                                section;
    std::map<std::string, std::string> attributes;
};


struct KSection {
    std::string   title;
    double        thickness1;
    double        thickness2;
    double        thickness3;
    double        thickness4;
};

struct KSectionBeam {
    std::string   title;
    int                 CST;//cross section type
    std::string         sectionType;//this is different from cross section type
    double              TS1;
    double              TS2;
    double              TT1;
    double              TT2;
    std::vector<double> D;
    double              CrossSectionalArea;//The definition on *ELEMENT_BEAM_THICKNESS overrides the value defined here.
};

struct KData {
    std::map<int, KNode>              nodes;
    std::map<int, KElement>           elements;
    std::map<int, KElementPulley>     elementsPulley;
    std::map<int, KElementBeamSource> elementsBeamSource;
    std::map<int, KElementBearing>    elementBearing;
    std::map<int, KPart>              parts;
    std::map<int, KSection>           sections;
    std::map<int, KSectionBeam>       sectionsBeam;
};


bool parse_k
(
    const char* fileName,
    KData&      data
);


#endif // KPARSER_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
