/*               O T H E R _ C H E C K . C X X
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file other_check.cxx
 *
 * Check dist files defined for src/other components against the
 * doc/legal/other license files - should be a license file for
 * each dist file.
 */

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <map>
#include <sstream>
#include <string>

int
main(int argc, const char *argv[])
{
    int ret = 0;

    try {

	if (argc < 3) {
	    std::cerr << "Usage: other_check [-v] licenses_list file_list\n";
	    return -1;
	}

	std::regex o_regex(".*[\\/]other[\\/](.*).dist$");
	std::regex o_e_regex(".*[\\/]other[\\/]ext[\\/](.*).dist$");
	std::regex txt_regex(".*[\\/](.*).txt$");

	std::set<std::string> dists;
	std::set<std::string> licenses;

	std::string lfile;
	std::ifstream license_file_stream;
	license_file_stream.open(argv[1]);
	if (!license_file_stream.is_open()) {
	    std::cerr << "Unable to open license file list " << argv[1] << "\n";
	}
	while (std::getline(license_file_stream, lfile)) {
	    if (!std::regex_match(std::string(lfile), txt_regex)) {
		continue;
	    }
	    std::smatch lfile_ref;
	    if (!std::regex_search(lfile, lfile_ref, txt_regex)) {
		continue;
	    }
	    std::string lroot = std::string(lfile_ref[1]);
	    licenses.insert(lroot);
	}
	license_file_stream.close();

	std::string dfile;
	std::ifstream dist_file_stream;
	dist_file_stream.open(argv[2]);
	if (!dist_file_stream.is_open()) {
	    std::cerr << "Unable to open dist file list " << argv[2] << "\n";
	}
	while (std::getline(dist_file_stream, dfile)) {
	    if (!std::regex_match(dfile, o_regex) && !std::regex_match(dfile, o_e_regex)) {
		continue;
	    }
	    std::smatch dfile_ref;
	    if (std::regex_search(dfile, dfile_ref, o_e_regex)) {
		std::string droot = std::string(dfile_ref[1]);
		if (droot == std::string("perplex")) {
		    // perplex is a BRL-CAD developed tool, but is present in
		    // src/other/ext to support the stepcode build and simplify
		    // other build logic.  We don't need a separate license file
		    // for it, so we skip inserting it here.
		    continue;
		}
		dists.insert(droot);
		continue;
	    }
	    if (std::regex_search(dfile, dfile_ref, o_regex)) {
		std::string droot = std::string(dfile_ref[1]);
		dists.insert(droot);
		continue;
	    }
	}
	dist_file_stream.close();

	std::set<std::string> unmatched_licenses;

	while (licenses.size() && dists.size()) {
	    std::string l = *licenses.begin();
	    licenses.erase(l);
	    if (dists.find(l) != dists.end()) {
		dists.erase(l);
	    } else {
		unmatched_licenses.insert(l);
	    }
	}

	if (unmatched_licenses.size() || dists.size()) {
	    ret = -1;
	}

	std::set<std::string>::iterator s_it;
	if (unmatched_licenses.size()) {
	    std::cout << "License files present in doc/legal/other with no corresponding dist file:\n";
	    for (s_it = unmatched_licenses.begin(); s_it != unmatched_licenses.end(); s_it++) {
		std::cout << *s_it << "\n";
	    }
	}
	if (dists.size()) {
	    std::cout << "Dist files present in src/other or src/other/ext with no corresponding license file:\n";
	    for (s_it = dists.begin(); s_it != dists.end(); s_it++) {
		std::cout << *s_it << "\n";
	    }
	}
    }

    catch (const std::regex_error& e) {
	std::cout << "regex error: " << e.what() << '\n';
	return -1;
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

