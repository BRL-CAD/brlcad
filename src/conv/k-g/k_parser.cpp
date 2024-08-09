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
    Element_Beam,
    Element_Beam_Pulley,
    Element_Beam_Source,
    Element_Bearing,
    Element_Blanking,
    Element_Direct_Matrix_Input,
    Element_Discrete,
    Element_Discrete_Sphere,
    Element_Generalized_Shell,
    Element_Generalized_Solid,
    Element_Inertia,
    Element_Interpolation_Shell,
    Element_Interpolation_Solid,
    Element_Lancing,
    Element_Mass,
    Element_Mass_Matrix,
    Element_Mass_Part,
    Element_Plotel,
    Element_Seatbealt,
    Element_Seatbealt_Accelerometer,
    Element_Seatbealt_Pretensioner,
    Element_Seatbealt_Retractor,
    Element_Seatbealt_Sensor,
    Element_Seatbealt_Slipring,
    Element_Shell,
    Element_Shell_Nurbs_Patch,
    Element_Shell_Source_Sink,
    Element_Solid,
    Element_Solid_Nurbs_Patch,
    Element_Solid_Peri,
    Element_Sph,
    Element_Trim,
    Element_Tshell,
    Part,
    Part_Adaptive_Failure,
    Section_Ale1d,
    Section_Ale2d,
    Section_Beam,
    Section_Beam_AISC,
    Section_Discrete,
    Section_Fpd,
    Section_Point_Source,
    Section_Point_source_Mixture,
    Section_Seatbelt,
    Section_Shell,
    Section_Solid,
    Section_Solid_Peri,
    Section_Sph,
    Section_Tshell
};

enum class Options {
    Title,
    Thickness,
    Scalar,
    Scalr,
    Section,
    Pid,
    Offset,
    Orientation,
    Warpage,
    Elbow
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
		temp += toupper(command[i]);
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
	size_t                   elementLinesRead = 0;
	size_t                   cardCounter = 0;// this will replace sectionLinesRead elementLinesRead, and partLinesRead
	size_t                   optionsCounter = 0;
	std::string              sectionTitle;
	int                      sectionId        = -1;
	int                      sectionElForm    = 0;
	int                      CST              = 0;
	std::string              line             = read_line(is);
	std::vector<std::string> tokens;
	const size_t FirstNode = 2;
	std::vector<std::string> optionsContainer;
	std::vector<Options> options;

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
			    if ((command.size() > 1) && (command[1] == "BEAM")) {
				if ((command.size() > 2) && (command[2] == "PULLEY")) {
				    state = KState::Element_Beam_Pulley;
				}
				else if ((command.size() > 2) && (command[2] == "SOURCE")) {
				    state = KState::Element_Beam_Source;
				}
				else {
				    state = KState::Element_Beam;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());

				    if (optionsContainer.size() > 0) {
					for (size_t i_o = 0; i_o < optionsContainer.size(); ++i_o) {
					    if (optionsContainer[i_o] == "THICKNESS") {
						options.push_back(Options::Thickness);
					    }
					    else if (optionsContainer[i_o] == "SCALAR") {
						options.push_back(Options::Scalar);
					    }
					    else if (optionsContainer[i_o] == "SCALR") {
						options.push_back(Options::Scalr);
					    }
					    else if (optionsContainer[i_o] == "SECTION") {
						options.push_back(Options::Section);
					    }
					    else if (optionsContainer[i_o] == "PID") {
						options.push_back(Options::Pid);
					    }
					    else if (optionsContainer[i_o] == "OFFSET") {
						options.push_back(Options::Offset);
					    }
					    else if (optionsContainer[i_o] == "ORIENTATION") {
						options.push_back(Options::Orientation);
					    }
					    else if (optionsContainer[i_o] == "WARPAGE") {
						options.push_back(Options::Warpage);
					    }
					    else if (optionsContainer[i_o] == "ELBOW") {
						options.push_back(Options::Elbow);
					    }
					    else
						std::cout << "Unhandeled Element_Beam option" << optionsContainer[i_o] << "in k-file" << fileName << "\n";
					}
				    }
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "BEARING")) {
				state = KState::Element_Bearing;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());

				if (optionsContainer.size() > 0) {
				    elementLinesRead = 0;//element Bearing has only the Tilte option
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "BLANKING")) {
				state = KState::Element_Blanking;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
			    }
			    else if ((command.size() > 3) && (command[1] == "DIRECT") && (command[2] == "MATRIX") && (command[3] == "INPUT")) {
				state = KState::Element_Direct_Matrix_Input;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 4, command.end());
			    }
			    else if ((command.size() > 1) && (command[1] == "DISCRETE")) {
				if ((command.size() > 2) && (command[2] == "SPHERE")) {
				    state = KState::Element_Discrete_Sphere;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else {
				    state = KState::Element_Discrete;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
			    }
			    else if ((command.size() > 2) && (command[1] == "GENERALIZED")) {
				if ((command[2] == "SHELL")) {
				    state = KState::Element_Generalized_Shell;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else {
				    state = KState::Element_Generalized_Solid;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "INERTIA")) {
				state = KState::Element_Inertia;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
			    }
			    else if ((command.size() > 2) && (command[1] == "INTERPOLATION")) {
				if (command[2] == "SHELL") {
				    state = KState::Element_Interpolation_Shell;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else {
				    state = KState::Element_Interpolation_Solid;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "LANCING")) {
				state = KState::Element_Lancing;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
			    }
			    else if ((command.size() > 1) && (command[1] == "MASS")) {
				if ((command.size() > 2) && (command[2] == "MATRIX")) {
				    state = KState::Element_Mass_Matrix;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else if ((command.size() > 2) && (command[2] == "PART")) {
				    state = KState::Element_Mass_Part;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else {
				    state = KState::Element_Mass;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "PLOTEL")) {
				state = KState::Element_Plotel;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
			    }
			    else if ((command.size() > 1) && (command[1] == "SEATBELT")) {
				if ((command.size() > 2) && (command[2] == "ACCELEROMETER")) {
				    state = KState::Element_Seatbealt_Accelerometer;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else if ((command.size() > 2) && (command[2] == "PRETENSIONER")) {
				    state = KState::Element_Seatbealt_Pretensioner;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else if ((command.size() > 2) && (command[2] == "RETRACTOR")) {
				    state = KState::Element_Seatbealt_Retractor;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else if ((command.size() > 2) && (command[2] == "SENSOR")) {
				    state = KState::Element_Seatbealt_Sensor;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else if ((command.size() > 2) && (command[2] == "SLIPRING")) {
				    state = KState::Element_Seatbealt_Slipring;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else {
				    state = KState::Element_Seatbealt;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "SHELL")) {
				if ((command.size() > 2) && (command[2] == "NURBS")) {
				    state = KState::Element_Shell_Nurbs_Patch;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 4, command.end());
				}
				else if ((command.size() > 2) && (command[2] == "SOURCE")) {
				    state = KState::Element_Shell_Source_Sink;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 4, command.end());
				}
				else {
				    state = KState::Element_Shell;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "SOLID")) {
				if ((command.size() > 2) && (command[2] == "NURBS")) {
				    state = KState::Element_Solid_Nurbs_Patch;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 4, command.end());
				}
				else if ((command.size() > 2) && (command[2] == "PERI")) {
				    state = KState::Element_Solid_Peri;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 3, command.end());
				}
				else {
				    state = KState::Element_Solid;

				    optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
			    }
			    else if ((command.size() > 1) && (command[1] == "SPH")) {
				state = KState::Element_Sph;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
			    }
			    else if ((command.size() > 1) && (command[1] == "TRIM")) {
				state = KState::Element_Trim;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
			    }
			    else if ((command.size() > 1) && (command[1] == "TSHELL")) {
				state = KState::Element_Tshell;

				optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
			    }
			    else
				std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
			}
			else if (command[0] == "PART") {
			    if ((command.size() == 1) || (command[1] == "INERTIA")) {
				state = KState::Part;
				partLinesRead = 0;
				partTitle = "";
			    }
			    else if ((command.size() == 3) && (command[1] == "ADAPTIVE") && (command[2] == "FAILURE")) {
				state = KState::Part_Adaptive_Failure;
			    }
			    else
				std::cout << "Unexpected command " << tokens[0] << " in k-file " << fileName << "\n";
			}
			else if (command[0] == "SECTION") {
			    if (command[1] == "ALE1D"){
				state        = KState::Section_Ale1d;
				sectionTitle = "";
				sectionId    = -1;

				if (command.size() > 2) {
				    if (command[2] == "TITLE")
					sectionLinesRead = 0;
				    else
					optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
				else
				    sectionLinesRead = 1;
			    }
			    else if (command[1] == "ALE2D") {
				state = KState::Section_Ale2d;
				sectionTitle = "";
				sectionId = -1;

				if (command.size() > 2) {
				    if (command[2] == "TITLE")
					sectionLinesRead = 0;
				    else
					optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
				else
				    sectionLinesRead = 1;
			    }
			    else if (command[1] == "BEAM") {
				state = KState::Section_Beam;
				sectionTitle = "";
				sectionId = -1;

				if (command.size() > 2) {
				    if (command[2] == "TITLE")
					sectionLinesRead = 0;
				    else
					optionsContainer.insert(optionsContainer.end(), command.begin() + 2, command.end());
				}
				else
				    sectionLinesRead = 1;
			    }
			    else if (command[1] == "SHELL") {
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
			    element.nodes.push_back(stoi(tokens[i_n + FirstNode]));
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
			    element.nodes.push_back(stoi(tokens[i_n + FirstNode]));
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
		    case KState::Section_Ale1d: {
			switch (sectionLinesRead)
			{
			case 0: {
			    sectionTitle = line;
			    break;
			}
			case 1: {
			    if (tokens.size() < 4) {
				std::cout << "Too short SECTION_ALE1D in k-file " << fileName << "\n";
				break;
			    }
			    sectionId = stoi(tokens[0]);
			    data.sections[sectionId].title = sectionTitle;
			    break;
			}
			case 2: {
			    if (sectionId < 0) {
				std::cout << "Bad SECTION in k-file " << fileName << "\n";
				break;
			    }

			    if (tokens.size() < 2) {
				std::cout << "Too short SECTION_ALE1D in k-file " << fileName << "\n";
				break;
			    }

			    data.sections[sectionId].thickness1 = stod(tokens[0]);
			    data.sections[sectionId].thickness2 = stod(tokens[1]);
			}
			}
			++sectionLinesRead;
			break;
		    }
		    case KState::Section_Ale2d: {
			switch (sectionLinesRead)
			{
			case 0: {
			    sectionTitle = line;
			    break;
			}
			case 1: {
			    if (tokens.size() < 4) {
				std::cout << "Too short SECTION_ALE2D in k-file " << fileName << "\n";
				break;
			    }
			    sectionId = stoi(tokens[0]);
			    data.sections[sectionId].title = sectionTitle;
			    break;
			}
			}

			++sectionLinesRead;
			break;
		    }
		    case KState::Section_Beam: {
			KSectionBeam sectionBeam;
			switch (sectionLinesRead)
			{
			case 0: {
			    sectionTitle = line;
			    break;
			}
			case 1: {
			    if (tokens.size() < 8) {
				std::cout << "Too short SECTION_BEAM in k-file " << fileName << "\n";
				break;
			    }
			    sectionId     = stoi(tokens[0]);
			    sectionElForm = stoi(tokens[1]);
			    
			    sectionBeam.title = sectionTitle;
			    sectionBeam.CST   = stoi(tokens[4]);

			    break;
			}
			case 2: {
			    if ((sectionElForm == 1) || (sectionElForm == 11)) {
				if (tokens.size() < 6) {
				    std::cout << "Too short SECTION_BEAM card 2a in k-file " << fileName << "\n";
				    break;
				}

				sectionBeam.TS1 = stod(tokens[0]);
				sectionBeam.TS2 = stod(tokens[1]);
				sectionBeam.TT1 = stod(tokens[2]);
				sectionBeam.TT2 = stod(tokens[3]);
			    }
			    else if ((sectionElForm == 2) || (sectionElForm == 3) || (sectionElForm == 12)||(sectionElForm == 13)) {
				std::string first7characters;

				if (tokens[0].size() > 7) {
				    first7characters = tokens[0].substr(0, 7);
				}

				if ((first7characters == "SECTION")) {
				    sectionBeam.sectionType = tokens[0];

				    for (size_t i_d = 1; i_d < 7; ++i_d) {
					sectionBeam.D[i_d - 1] = stod(tokens[i_d]);
				    }
				}
				else {
				    sectionBeam.CrossSectionalArea = stod(tokens[0]);
				}
			    }
			    else if ((sectionElForm == 4) || (sectionElForm == 5)) {
				if (tokens.size() < 4) {
				    std::cout << "Too short SECTION_BEAM card 2e in k-file " << fileName << "\n";
				    break;
				}

				sectionBeam.TS1 = stod(tokens[0]);
				sectionBeam.TS2 = stod(tokens[1]);
				sectionBeam.TT1 = stod(tokens[2]);
				sectionBeam.TT2 = stod(tokens[3]);
			    }
			    else if (sectionElForm == 6) {
				//nothing to do.
				break;
			    }
			    else if ((sectionElForm == 7) || (sectionElForm == 8)) {
				if (tokens.size() < 2) {
				    std::cout << "Too short SECTION_BEAM card 2h in k-file " << fileName << "\n";
				    break;
				}

				sectionBeam.TS1 = stod(tokens[0]);
				sectionBeam.TS2 = stod(tokens[1]);
			    }
			    else if (sectionElForm == 9) {
				if (tokens.size() < 4) {
				    std::cout << "Too short SECTION_BEAM card 2i in k-file " << fileName << "\n";
				    break;
				}
			    }
			    else if (sectionElForm == 14) {
				//nothing to do 
				break;
			    }
			    break;
			}
			case 3: {
			    if (sectionElForm == 12) {
				//No information related to geometry.
				break;
			    }

			    break;
			}
			}

			++sectionLinesRead;
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

		    case KState::Element_Beam: {
			KElement element;
			int      pid;
			int      eid;

			if (options.size() == 0 || optionsCounter == 0) {
			    if (tokens.size() < 10) {
				std::cout << "Too short ELEMENT_BEAM in k-file" << fileName << "\n";
				break;
			    }
			    eid = stoi(tokens[0]);

			    if (data.elements.find(eid) != data.elements.end()) {
				std::cout << "Duplicat Element ID" << eid << " in k-file" << fileName << "\n";
				break;
			    }

			    for (int i_n = 0; i_n < 5; ++i_n) {
				element.nodes.push_back(stoi(tokens[i_n + FirstNode]));
			    }

			    data.elements[eid] = element;

			    pid = stoi(tokens[1]);
			    data.parts[pid].elements.insert(eid);
			    break;
			}
			else if ((options.size() > 0)) {
			    Options currentOption;

			    if (optionsCounter < options.size()) {
				currentOption = options[optionsCounter];
			    }
			    else {
				optionsCounter = 0;
				currentOption = options[optionsCounter];
			    }

			    switch (currentOption)
			    {
			    case Options::Thickness: {
				if (tokens.size() < 5) {
				    std::cout << "Too short option THICKNESS in k-file " << fileName << "\n";
				    break;
				}

				for (size_t i_p = 0; i_p < tokens.size(); ++i_p) {
				    double param = stod(tokens[i_p]);
				    element.options["THICKNESS"].push_back(param);
				}

				data.elements[eid] = element;

				++optionsCounter;
				break;
			    }
			    case Options::Scalar: {
				//Nothing to do 
				++optionsCounter;
				break;
			    }
			    case Options::Scalr: {
				//Nothing to do
				++optionsCounter;
				break;
			    }
			    case Options::Section: {
				if (tokens.size() < 7) {
				    std::cout << "Too short option Section in k-file " << fileName << "\n";
				    break;
				}
				double temp;

				if (tokens[0] == "EQ.SECTION_01") {
				    temp = 1.0;
				}
				else if (tokens[0] == "EQ.SECTION_02") {
				    temp = 2.0;
				}
				else if (tokens[0] == "EQ.SECTION_03") {
				    temp = 3.0;
				}
				else if (tokens[0] == "EQ.SECTION_04") {
				    temp = 4.0;
				}
				else if (tokens[0] == "EQ.SECTION_05") {
				    temp = 5.0;
				}
				else if (tokens[0] == "EQ.SECTION_06") {
				    temp = 6.0;
				}
				else if (tokens[0] == "EQ.SECTION_07") {
				    temp = 7.0;
				}
				else if (tokens[0] == "EQ.SECTION_08") {
				    temp = 8.0;
				}
				else if (tokens[0] == "EQ.SECTION_09") {
				    temp = 9.0;
				}
				else if (tokens[0] == "EQ.SECTION_10") {
				    temp = 10.0;
				}
				else if (tokens[0] == "EQ.SECTION_11") {
				    temp = 11.0;
				}
				else if (tokens[0] == "EQ.SECTION_12") {
				    temp = 12.0;
				}
				else if (tokens[0] == "EQ.SECTION_13") {
				    temp = 13.0;
				}
				else if (tokens[0] == "EQ.SECTION_14") {
				    temp = 14.0;
				}
				else if (tokens[0] == "EQ.SECTION_15") {
				    temp = 15.0;
				}
				else if (tokens[0] == "EQ.SECTION_16") {
				    temp = 16.0;
				}
				else if (tokens[0] == "EQ.SECTION_17") {
				    temp = 17.0;
				}
				else if (tokens[0] == "EQ.SECTION_18") {
				    temp = 18.0;
				}
				else if (tokens[0] == "EQ.SECTION_19") {
				    temp = 19.0;
				}
				else if (tokens[0] == "EQ.SECTION_20") {
				    temp = 20.0;
				}
				else if (tokens[0] == "EQ.SECTION_21") {
				    temp = 21.0;
				}
				else if (tokens[0] == "EQ.SECTION_22") {
				    temp = 22.0;
				}

				element.options["SECTION"].push_back(temp);

				for (size_t i_p = 1; i_p < tokens.size(); ++i_p) {
				    temp = stod(tokens[1]);
				    element.options["SECTION"].push_back(temp);
				}
				data.elements[eid] = element;
				++optionsCounter;
				break;
			    }
			    case Options::Pid: {
				//Nothing to do
				++optionsCounter;
				break;
			    }
			    case Options::Offset: {
				if (tokens.size() < 6) {
				    std::cout << "To short OFFSET option in k-file " << fileName << "\n";
				    break;
				}
				
				for (size_t i_p = 0; i_p < tokens.size(); ++i_p) {
				    double temp = stod(tokens[i_p]);
				    element.options["OFFSET"].push_back(temp);
				}

				data.elements[eid] = element;

				++optionsCounter;
				break;
			    }
			    case Options::Orientation: {
				if (tokens.size() < 3) {
				    std::cout << "To short ORIENTATION option in k-file" << fileName << "\n";
				    break;
				}

				for (size_t i_p = 0; i_p < tokens.size(); ++i_p) {
				    double temp = stod(tokens[i_p]);
				    element.options["ORIENTATION"].push_back(temp);
				}

				data.elements[eid] = element;

				++optionsCounter;
				break;
			    }
			    case Options::Warpage: {
				if (tokens.size() < 2) {
				    std::cout << "To short WARPAGE option in k-file" << fileName << "\n";
				    break;
				}

				for (size_t i_p = 0; i_p < tokens.size(); ++i_p) {
				    double temp = stod(tokens[i_p]);
				    element.options["WARPAGE"].push_back(temp);
				}

				data.elements[eid] = element;

				++optionsCounter;
				break;
			    }
			    case Options::Elbow: {
				if (tokens.size() < 1) {
				    std::cout << "Empty ELBOW option in k-file" << fileName << "\n";
				    break;
				}
				double temp = stod(tokens[0]);
				element.options["ELBOW"].push_back(temp);

				data.elements[eid] = element;

				++optionsCounter;
				break;
			    }
			    default:
				break;
			    }
			}
			break;
		    }

		    case KState::Element_Beam_Pulley: {
			if (tokens.size() < 8) {
			    std::cout << "Too short ELEMENT_BEAM_PULLEY in k-file " << fileName << "\n";
			    break;
			}
			KElementPulley pulley;
			int pulleyId= stoi(tokens[0]);

			pulley.truss1Id   = stoi(tokens[1]);
			pulley.truss2Id   = stoi(tokens[2]);
			pulley.pulleyNode = stoi(tokens[3]);

			data.elementsPulley[pulleyId] = pulley;
			break;
		    }
		    case KState::Element_Beam_Source: {
			if (tokens.size() < 7) {
			    std::cout << "Too short ELEMENT_BEAM_SOURCE in k-file" << fileName << "\n";
			    break;
			}

			KElementBeamSource source;
			
			int eid = stoi(tokens[0]);

			source.sourceNodeId          = stoi(tokens[1]);
			source.sourceElementId       = stoi(tokens[2]);
			source.nElements             = stoi(tokens[3]);
			source.beamElementLength     = stof(tokens[4]);
			source.minmumLengthToPullOut = stof(tokens[6]);

			data.elementsBeamSource[eid] = source;
			break;
		    }

		    case KState::Element_Bearing: {
			KElementBearing bearing;
			int eid;

			switch (elementLinesRead)
			{
			case 0: {
			    bearing.title = line;

			    break;
			}
			case 1: {
			    if (tokens.size() < 7) {
				std::cout << "Too short ELEMENT_BEARING in k-file " << fileName << "\n";
				break;
			    }

			    eid = stoi(tokens[0]);
			    bearing.bearingType            = stoi(tokens[1]);
			    bearing.n1                     = stoi(tokens[2]);
			    bearing.coordinateId1          = stoi(tokens[3]);
			    bearing.n2                     = stoi(tokens[4]);
			    bearing.coordinateId2          = stoi(tokens[5]);
			    bearing.numberOfBallsOrRollers = stoi(tokens[6]);

			    break;
			}
			case 2: {
			    //Nothing related to Geometry here.
			    break;
			}
			case 3: {
			    if (tokens.size() < 4) {
				std::cout << "Too short ELEMENT_BEARING in k-file " << fileName << "\n";
				break;
			    }

			    bearing.diameterOfBallsOrRollers = stof(tokens[0]);
			    bearing.boreInnerDiameter        = stof(tokens[1]);
			    bearing.boreOuterDiameter        = stof(tokens[2]);
			    bearing.pitchDiameter            = stof(tokens[3]);

			    break;
			}
			case 4: {
			    if (tokens.size() < 4) {
				std::cout << "Too short ELEMENT_BEARING in k-file " << fileName << "\n";
				break;
			    }

			    bearing.innerGroveRadiusToBallDiameterRatioOrRollerLength = stof(tokens[1]);
			    bearing.outerRaceGrooveRadiusToBallDiameterRatio          = stof(tokens[2]);
			    bearing.totalRadianceClearenceBetweenBallAndRaces         = stof(tokens[3]);

			    break;
			}
			case 5: {
			    //nothing related to Geometry.
			    break;
			}

			}

			data.elementBearing[eid] = bearing;
			++elementLinesRead;
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
