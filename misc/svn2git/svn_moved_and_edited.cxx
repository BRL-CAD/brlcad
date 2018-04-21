/*        S V N _ M O V E D _ A N D _ E D I T E D . C X X
 *
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
/** @file svn_moved_and_edited.cxx
 *
 * Searches an SVN dump file for revisions with Node-paths that have both a
 * Note-copyfrom-path property and a non-zero Content-length - the idea being
 * that these are changes were a file was both moved and changed.
 *
 * These are potentially problematic commits when converting to git, since
 * they could break git log --follow
 */

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
int main(int argc, const char **argv)
{
    int rev = -1;
    int is_move = 0;
    int is_edit = 0;

    if (argc == 1) {
	std::cerr << "Error: need a file to work on!\n";
	return -1;
    }
    std::ifstream infile(argv[1]);
    std::string line;
    while (std::getline(infile, line))
    {
	std::istringstream ss(line);
	std::string s = ss.str();
	if (!s.compare(0, 16, "Revision-number:")) {
	    if (rev >= 0) {
		// Found a new rev - process the old one
		if (is_move && is_edit) {
		    std::cout << rev << "\n";
		}
		// Reset
		is_move = 0; is_edit = 0;
	    }
	    // Grab new revision number
	    rev = std::stoi(s.substr(17));
	    //std::cout <<  "Reading revision " << rev << ":\n";
	}
	if (!s.compare(0, 10, "Node-path:")) {
	    if (!(is_move && is_edit)) {
		// Previous node path wasn't a move/edit, reset
		is_move = 0;
		is_edit = 0;
	    }
	} else {
	    if (!s.compare(0, 19, "Node-copyfrom-path:")) {
		is_move = 1;
	    }
	    if (!s.compare(0, 15, "Content-length:")) {
		is_edit = 1;
	    }
	}
    }
    // If the newest commit satisfies the criteria, print it
    if (is_move && is_edit) {
	std::cout << rev << "\n";
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
