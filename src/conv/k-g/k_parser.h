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


struct KNode {
    double x;
    double y;
    double z;
};


struct KElement {
    int node1;
    int node2;
    int node3;
    int node4;
};


struct KPart {
    std::string   title;
    std::set<int> elements;
    int           section;
};


struct KSection {
    std::string   title;
    double        thickness1;
    double        thickness2;
    double        thickness3;
    double        thickness4;
};


struct KData {
    std::map<int, KNode>    nodes;
    std::map<int, KElement> elements;
    std::map<int, KPart>    parts;
    std::map<int, KSection> sections;
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
