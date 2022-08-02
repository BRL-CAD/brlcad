/*                  I N A C T V H D R S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file inactvhdrs.cpp
 *
 * Analyze a project's C/C++ source files and headers to see if any local
 * headers are not #included by either source files or headers.
 *
 * Note: needs C++17 to build:
 * g++ -O3 -std=c++17 -o inactvhdrs inactvhdrs.cpp
 */

#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <filesystem>
#include <set>

std::set<std::string> checked;
std::set<std::filesystem::directory_entry> sfiles;
std::set<std::filesystem::directory_entry> hdrs;
std::set<std::filesystem::directory_entry> found_hdrs;
std::set<std::filesystem::directory_entry> to_check_hdrs;

void
check_file(const std::filesystem::directory_entry *f)
{
    std::set<std::filesystem::directory_entry>::iterator h_it;
    std::regex li_regex = std::regex("#[[:space:]]*include[[:space:]]*\"(.*)\".*");
    std::regex rel_regex("([.][.][/\\\\])+");
    std::smatch incvar;
    std::ifstream fs;
    fs.open(f->path());
    if (!fs.is_open()) {
	std::cerr << "Unable to open " << f->path() << " for reading, skipping\n";
	return;
    }
    std::string sline;
    while (std::getline(fs, sline)) {
	if (std::regex_search(sline, incvar, li_regex)) {
	    // Strip any relative prefixes from the path
	    std::string hroot = std::regex_replace(std::string(incvar[1]), rel_regex, "");
	    if (checked.find(hroot) == checked.end()) {
		std::string hstr = std::string(".*") + hroot + std::string("$");
		std::regex h_regex = std::regex(hstr);
		checked.insert(hroot);
		std::set<std::filesystem::directory_entry> hmatches;
		for (h_it = hdrs.begin(); h_it != hdrs.end(); h_it++) {
		    if (std::regex_match(h_it->path().string(), h_regex)) {
			checked.insert(hroot);
			to_check_hdrs.insert(*h_it);
			found_hdrs.insert(*h_it);
		    }
		}
	    }
	}
    }
}

int
main(int argc, const char *argv[])
{
    if (argc != 2) {
	std::cerr << "Usage: actvhdrs <path>\n";
	return -1;
    }
    std::regex codefile_regex(".*[.](c|cpp|cxx|cc|h|hpp|hxx|y|yy|l)([.]in)?$");
    std::regex sfile_regex(".*[.](c|cpp|cxx|cc|y|yy|l)([.]in)?$");
    std::regex hdr_regex(".*[.](h|hpp|hxx)([.]in)?$");
    std::string path(argv[1]);
    std::set<std::filesystem::directory_entry>::iterator f_it;
    std::set<std::filesystem::directory_entry>::iterator h_it;

    // Break out files present into source files and headers.
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path)) {
	if (std::regex_match(entry.path().string(), codefile_regex)) {
	    if (std::regex_match(entry.path().string(), sfile_regex)) {
		sfiles.insert(entry);
	    }
	    if (std::regex_match(entry.path().string(), hdr_regex)) {
		hdrs.insert(entry);
	    }
	}
    }

    // Check source files to seed the process
    for (f_it = sfiles.begin(); f_it != sfiles.end(); f_it++) {
	check_file(&(*f_it));
    }

    // Check any headers that got included for more includes
    while (to_check_hdrs.size()) {
	h_it = to_check_hdrs.begin();
	std::filesystem::directory_entry h = *h_it;
	to_check_hdrs.erase(h_it);
	check_file(&h);
    }

    // Remove everything reported as found from the header set
    for (h_it = found_hdrs.begin(); h_it != found_hdrs.end(); h_it++) {
	hdrs.erase(*h_it);
    }

    // Report what's left
    for (h_it = hdrs.begin(); h_it != hdrs.end(); h_it++) {
	std::cout << h_it->path() << "\n";
    }
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
