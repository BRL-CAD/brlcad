/*                         G R O U P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
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

#include "common.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ssdeep.hpp"

extern "C" {
#include "bu/app.h"
#include "bu/file.h"
#include "./gdiff.h"
}

std::string
globToRegex(const std::string& globPattern) {
    std::string regexPattern;
    for (char c : globPattern) {
	switch (c) {
	    case '*':
		regexPattern += ".*";
		break;
	    case '?':
		regexPattern += ".";
		break;
	    case '.': // Escape literal dots
	    case '+':
	    case '(':
	    case ')':
	    case '[':
	    case ']':
	    case '{':
	    case '}':
	    case '^':
	    case '$':
	    case '\\':
	    case '|':
		regexPattern += '\\'; // Escape special regex characters
		regexPattern += c;
		break;
	    default:
		regexPattern += c;
		break;
	}
    }
    return regexPattern;
}

void
GlobMatch(std::set<std::string> *files, const std::string& gpattern, const char *root) {
    std::string sregex = std::string(".*") + globToRegex(gpattern);
    std::regex fregex(sregex);
    std::string rstr(root);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(rstr)) {
	if (!entry.is_regular_file())
	    continue;
	if (std::regex_match(entry.path().string(), fregex))
	    files->insert(entry.path().string());
    }
}

int
gdiff_group(int argc, const char **argv, long threshold)
{
    // If we have no patterns, there's nothing to group
    if (!argc || !argv)
	return BRLCAD_OK;

    // Build a set of file paths from the argv patterns
    std::set<std::string> files;
    char cwd[MAXPATHLEN] = {0};
    bu_dir(cwd, MAXPATHLEN, BU_DIR_CURR, NULL);
    for (int i = 0; i < argc; i++) {
	// 1.  If we've been given a directory, skip - we need either
	// a fully qualified file or a pattern
	if (bu_file_directory(argv[i]))  {
	    bu_log("WARNING - %s is a directory, need either a file or a pattern - skipping\n", argv[i]);
	    continue;
	}

	// 2.  If we've been given a fully qualified path, just store that
	if (bu_file_exists(argv[i], NULL) && !bu_file_directory(argv[i])) {
	    files.insert(std::string(argv[i]));
	    continue;
	}

	// 3.  Try a pattern match
	GlobMatch(&files, std::string(argv[i]), cwd);
    }

    // Hash each file
    std::unordered_map<std::string, std::string> path2hash;
    std::unordered_map<std::string, std::set<std::string>> hash2path;
    std::set<std::string>::iterator s_it;
    for (s_it = files.begin(); s_it != files.end(); ++s_it) {
	std::string dhash;
	try {
	    dhash = ssdeep::FuzzyHash::compute_from_file(*s_it);
	} catch (const std::exception& ex) {
	    std::cerr << "Error hashing " << *s_it << ": " << ex.what() << "\n";
	    continue;
	} catch (...) {
	    std::cerr << *s_it << ": Unknown error occurred." << std::endl;
	    continue;
	}
	std::string fpath(*s_it);
	path2hash[fpath] = dhash;
	hash2path[dhash].insert(fpath);
    }

    // Compare hashes and find similarity groupings
    std::unordered_map<std::string, std::unordered_set<std::string>> groups;
    std::unordered_map<std::string, std::unordered_set<std::string>>::iterator g_it;
    std::unordered_map<std::string, std::set<std::string>>::iterator h_it;
    for (h_it = hash2path.begin(); h_it != hash2path.end(); ++h_it) {
	std::string h = h_it->first;
	//std::cout << h << "\n";
	int hcompare = 0;
	std::string gkey;
	for (g_it = groups.begin(); g_it != groups.end(); ++g_it) {
	    std::string c = g_it->first;
	    int score = ssdeep::FuzzyHash::CompareHashes(h, c);
	    if (h == c || (score > threshold && score > hcompare)) {
		gkey = c;
		hcompare = score;
	    }
	}
	if (hcompare) {
	    groups[gkey].insert(h);
	} else {
	    groups[h].insert(h);
	}
    }

    std::cout << "\n\nGrouping Summary:\n\n";

    // Report
    std::unordered_set<std::string>::iterator us_it;
    for (g_it = groups.begin(); g_it != groups.end(); ++g_it) {
	// Don't print out trivial groups
	bool trivial = true;
	for (us_it = g_it->second.begin(); us_it != g_it->second.end(); ++us_it) {
	    if (g_it->first != *us_it || hash2path[g_it->first].size() > 1) {
		trivial = false;
		break;
	    }
	}
	if (trivial)
	    continue;
	std::string ganchor = *hash2path[g_it->first].begin();
	std::cout << ganchor << ":\n";
	for (us_it = g_it->second.begin(); us_it != g_it->second.end(); ++us_it) {
	    std::set<std::string>::iterator hp_it;
	    for (hp_it = hash2path[*us_it].begin(); hp_it != hash2path[*us_it].end(); ++hp_it) {
		if (*hp_it == ganchor)
		    continue;
		int score = ssdeep::FuzzyHash::CompareHashes(g_it->first, *us_it);
		std::cout << "\t" << *hp_it << "(" << score << ")" << "\n";
	    }
	}
	std::cout << "\n";
    }

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
