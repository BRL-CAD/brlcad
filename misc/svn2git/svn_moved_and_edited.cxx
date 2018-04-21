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
#include <set>
#include <map>

int main(int argc, const char **argv)
{
    int rev = -1;
    int is_move = 0;
    int is_edit = 0;
    int node_is_file = 0;
    int have_node_path = 0;
    int node_path_filtered = 0;
    int rev_problem = 0;

    if (argc == 1) {
	std::cerr << "Error: need a file to work on!\n";
	return -1;
    }
    std::ifstream infile(argv[1]);
    std::string line;
    std::string node_path;
    std::string node_copyfrom_path;
    std::set<int> problem_revs;
    std::multimap<int, std::pair<std::string, std::string> > revs_move_map;
    while (std::getline(infile, line))
    {
	std::istringstream ss(line);
	std::string s = ss.str();
	if (!s.compare(0, 16, "Revision-number:")) {
	    // Grab new revision number
	    rev = std::stoi(s.substr(17));
	    have_node_path = 0; is_move = 0; is_edit = 0; node_is_file = 0;
	}
	if (rev >= 0) {
	    // OK , now we have a revision - start looking for content
	    if (!s.compare(0, 10, "Node-path:")) {
		if (is_move && is_edit && node_is_file) {
		    rev_problem = 1;
		    std::pair<std::string, std::string> move(node_copyfrom_path, node_path);
		    std::pair<int, std::pair<std::string, std::string> > mvmap(rev, move);
		    revs_move_map.insert(mvmap);
		    problem_revs.insert(rev);
		}
		have_node_path = 1; is_move = 0; is_edit = 0; node_is_file = 0;
		node_path = s.substr(11);
		if (node_path.compare(0, 6, "brlcad") != 0) {
		    node_path_filtered = 1;
		} else {
		    node_path_filtered = 0;
		}
		//std::cout <<  "Node path: " << node_path << "\n";
	    } else {
		if (have_node_path && !node_path_filtered) {
		    if (!s.compare(0, 19, "Node-copyfrom-path:")) {
			is_move = 1;
			node_copyfrom_path = s.substr(19);
			//std::cout <<  "Node copyfrom path: " << node_copyfrom_path << "\n";
		    }
		    if (!s.compare(0, 15, "Content-length:")) {
			is_edit = 1;
		    }
		    if (!s.compare(0, 15, "Node-kind: file")) {
			node_is_file = 1;
		    }
		}
	    }
	}
    }
    std::set<int>::iterator iit;
    for (iit = problem_revs.begin(); iit != problem_revs.end(); iit++) {
	std::cout << "Revision " << *iit << ":\n";
	std::multimap<int, std::pair<std::string, std::string> >::iterator rmit;
	std::pair<std::multimap<int, std::pair<std::string, std::string> >::iterator, std::multimap<int, std::pair<std::string, std::string> >::iterator> revrange;
	revrange = revs_move_map.equal_range(*iit);
	for (rmit = revrange.first; rmit != revrange.second; rmit++) {
	    std::cout << "  " << rmit->second.first << " -> " << rmit->second.second << "\n";
	}
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
