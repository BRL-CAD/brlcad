/*                    K _ P A R S E R . C P P
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
/** @file k_parser.cpp
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * k-file parsing function implementation
 */

#include "common.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "k_parser.h"


enum class KState {
    Ignore,
    Include,
    Node,
    Element_Shell,
    Element_Solid,
    Part,
    Part_Adaptive_Failure,
    Section_Shell,
    Section_Solid
};


static std::string read_line
(
    std::istream& is
) {
    std::string ret;

    for (;;) {
	if (!is)
	    break;

	char a;
	is.get(a);

	if ((a == '\n') || (a == '\r'))
	    break;

	ret += a;
    }

    return ret;
}

// The standard node line format is I8,3E16.0,2F8.0 (8 digit integer, 3x 16 digit doubles, 2x 8 digit floats).
static std::vector<std::string> read_line_node_standard
(
    const char* line
) {
    std::vector<std::string> ret;

    if (line != nullptr) {
	std::string temp;

	for (size_t i = 0; i < strlen(line); ++i) {
	    if ((i == 8) || (i == 24) || (i == 40) || (i == 56) || (i == 64) || (i == 72)) {
		if (temp.size() > 0) {
		    ret.push_back(temp);
		    temp.clear();

		    if (!(line[i] == ' ')) {
			temp += line[i];
		    }
		}
		else
		    continue;
	    }
	    else if ((line[i] == ' ')) {
		continue;
	    }
	    else if ((line[i] == '\t')) {
		break;
	    }
	    else
		temp += line[i];
	}

	if (temp.size() > 0)
	    ret.push_back(temp);
    }

    return ret;
}


static std::vector<std::string> parse_line
(
    const char* line
) {
    std::vector<std::string> ret;

    if (line != nullptr) {
	std::string temp;

	for (size_t i = 0; i < strlen(line); ++i) {
	    if ((line[i] == ' ') || (line[i] == '\t')) {
		if (temp.size() > 0) {
		    ret.push_back(temp);
		    temp.clear();
		}
	    }
	    else
		temp += line[i];
	}

	if (temp.size() > 0)
	    ret.push_back(temp);
    }

    return ret;
}


static std::vector<std::string> parse_command
(
    const char* command
) {
    std::vector<std::string> ret;

    if (command != nullptr) {
	std::string temp;

	for (size_t i = 0; i < strlen(command); ++i) {
	    if (command[i] == '_') {
		if (temp.size() > 0) {
		    ret.push_back(temp);
		    temp.clear();
		}
	    }
	    else
		temp += command[i];
	}

	if (temp.size() > 0)
	    ret.push_back(temp);
    }

    return ret;
}


bool parse_k
(
    const char* fileName,
    KData&      data
) {
    bool          ret = true;
    std::ifstream is(fileName);

    if (!is.is_open()) {
	std::cerr << "Error reading k-file " << fileName << std::endl;

	ret = false;
    }
    else {
	KState                   state            = KState::Ignore;
	size_t                   partLinesRead    = 0;
	std::string              partTitle;
	size_t                   sectionLinesRead = 0;
	std::string              sectionTitle;
	int                      sectionId        = -1;
	std::string              line             = read_line(is);
	std::vector<std::string> tokens;

	if (line.size() > 0)
	    tokens = parse_line(line.c_str());

	while (is && !is.eof()) {
	    bool handledLine = false;

	    if (tokens.size() > 0) {
		if (tokens[0][0] == '$')
		    handledLine = true;
		else if (tokens[0][0] == '*') {
		    std::vector<std::string> command = parse_command(tokens[0].c_str() + 1);

		    state = KState::Ignore;

		    if (command.size() == 0) {
			std::cerr << "Empty command in k-file " << fileName << std::endl;

			ret = false;
		    }
		    else {
			if (command[0] == "INCLUDE") {
			    if (command.size() == 1)
				state = KState::Include;
			    else
				std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
			}
			else if (command[0] == "NODE") {
			    if (command.size() == 1)
				state = KState::Node;
			    else
				std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
			}
			else if (command[0] == "ELEMENT") {
			    if ((command.size() == 2) && (command[1] == "SHELL"))
				state = KState::Element_Shell;
			    else if ((command.size() == 2) && (command[1] == "SOLID")) {
				state = KState::Element_Solid;
			    }
			    else
				std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
			}
			else if (command[0] == "PART") {
			    if ((command.size() == 1) || (command[1] == "INERTIA")) {
				state         = KState::Part;
				partLinesRead = 0;
				partTitle     = "";
			    }
			    else if ((command.size() == 3) && (command[1]=="ADAPTIVE") && (command[2]=="FAILURE")) {
				state = KState::Part_Adaptive_Failure;
			    }
			    else
				std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
			}
			else if (command[0] == "SECTION") {
			    if (command[1] == "SHELL") {
				state        = KState::Section_Shell;
				sectionTitle = "";
				sectionId    = -1;

				if (command.size() == 3) {
				    if (command[2] == "TITLE")
					sectionLinesRead = 0;
				    else
					std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
				}
				else
				    sectionLinesRead = 1;
			    }
			    else if (command[1] == "SOLID") {
				state        = KState::Section_Solid;
				sectionTitle = "";
				sectionId    = -1;

				if (command.size() == 3) {
				    if (command[2] == "TITLE")
					sectionLinesRead = 0;
				    else
					std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
				}
			    }
			    else
				std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
			}
		    }

		    handledLine = true;
		}
	    }

	    if (!handledLine) {
		switch (state) {
		    case KState::Include:
			ret = parse_k(line.c_str(), data);
			break;

		    case KState::Node: {
			tokens = read_line_node_standard(line.c_str());

			if (tokens.size() < 4) {
			    std::cout << "Too short NODE in k-file " << fileName << "\n";
			    break;
			}

			int id = stoi(tokens[0]);

			if (data.nodes.find(id) != data.nodes.end()) {
			    std::cout << "Duplicate NODE ID " << id << " in k-file " << fileName << "\n";
			    break;
			}

			KNode node;

			node.x = stod(tokens[1]);
			node.y = stod(tokens[2]);
			node.z = stod(tokens[3]);

			data.nodes[id] = node;
			break;
		    }

		    case KState::Element_Shell: {
			if (tokens.size() < 6) {
			    std::cout << "Too short ELEMENT_SHELL in k-file " << fileName << "\n";
			    break;
			}

			int eid = stoi(tokens[0]);

			if (data.elements.find(eid) != data.elements.end()) {
			    std::cout << "Duplicate ELEMENT ID " << eid << " in k-file " << fileName << "\n";
			    break;
			}

			KElement element;

			for (int i_n = 0; i_n < 4; ++i_n) {
			    element.nodes.push_back(stoi(tokens[i_n + FIRST_NODE]));
			}

			data.elements[eid] = element;

			int pid = stoi(tokens[1]);
			data.parts[pid].elements.insert(eid);
			break;
		    }

		    case KState::Element_Solid: {
			if (tokens.size() < 10) {
			    std::cout << "Too short ELEMENT_SOLID in k-file " << fileName << "\n";
			    break;
			}

			int eid = stoi(tokens[0]);

			if (data.elements.find(eid) != data.elements.end()) {
			    std::cout << "Duplicated ELEMENT ID" << eid << "in k-file " << fileName << "\n";
			}

			KElement element;

			for (int i_n = 0; i_n < 8; ++i_n) {
			    element.nodes.push_back(stoi(tokens[i_n + FIRST_NODE]));
			}
			
			data.elements[eid] = element;

			int pid = stoi(tokens[1]);
			data.parts[pid].elements.insert(eid);
			break;
		    }

		    case KState::Part: {
			switch (partLinesRead) {
			    case 0:
				partTitle = line;
				break;

			    case 1: {
				if (tokens.size() == 0) {
				    std::cout << "Too short PART in k-file " << fileName << "\n";
				    break;
				}

				int pid = stoi(tokens[0]);

				data.parts[pid].title = partTitle;

				if (tokens.size() > 1)
				    data.parts[pid].section = stoi(tokens[1]);

				break;
			    }

			    default:
				std::cout << "Unexpected PART length in k-file " << fileName << "\n";
			}

			++partLinesRead;
			break;

		    case KState::Ignore:
			break;
		    }

		    case KState::Section_Shell: {
			switch (sectionLinesRead) {
			case 0:
			    sectionTitle = line;
			    break;

			case 1: {
			    if (tokens.size() == 0) {
				std::cout << "Too short SECTION in k-file " << fileName << "\n";
				break;
			    }

			    sectionId                      = stoi(tokens[0]);
			    data.sections[sectionId].title = sectionTitle;
			    break;
			}

			case 2: {
			    if (sectionId < 0) {
				std::cout << "Bad SECTION in k-file " << fileName << "\n";
				break;
			    }

			    if (tokens.size() < 4) {
				std::cout << "Too short SECTION in k-file " << fileName << "\n";
				break;
			    }

			    data.sections[sectionId].thickness1 = stod(tokens[0]);
			    data.sections[sectionId].thickness2 = stod(tokens[1]);
			    data.sections[sectionId].thickness3 = stod(tokens[2]);
			    data.sections[sectionId].thickness4 = stod(tokens[3]);

			    break;
			}

			default:
			    std::cout << "Unexpected SECTION length in k-file " << fileName << "\n";
			}

			++sectionLinesRead;
			break;
		    }
		    case KState::Section_Solid: {
			switch (sectionLinesRead) {
			case 0:
			    sectionTitle = line;
			    break;

			case 1: {
			    if (tokens.size() == 0) {
				std::cout << "Too short SECTION in k-file " << fileName << "\n";
				break;
			    }

			    sectionId = stoi(tokens[0]);
			    data.sections[sectionId].title = sectionTitle;
			    break;
			}

			default:
			    std::cout << "Unexpected SECTION length in k-file " << fileName << "\n";

			}

			++sectionLinesRead;
			break;
		    }
		    case KState::Part_Adaptive_Failure: {
			if (tokens.size() < 2) {
			    std::cout << "Too short PART_ADAPTIVE_FAILURE in k-file " << fileName << "\n";
			    break;
			}

			int pid = stoi(tokens[0]);

			if (tokens.size() == 2) {
			    data.parts[pid].attributes["PART_ADAPTIVE_FAILURE_STARTING_TIME"]=tokens[1];
			}
			else {
			    std::cout << "Attributes should come in pairs in k-file" << fileName << "\n";
			}
			break;
		    }

		}
	    }

	    if (!ret)
		break;

	    line = read_line(is);

	    if (line.size() > 0)
		tokens = parse_line(line.c_str());
	    else
		tokens.clear();
	}
    }

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
