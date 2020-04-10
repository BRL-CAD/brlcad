/*               L I C E N S E _ C H E C K . C X X
 * BRL-CAD
 *
 * Copyright (c) 2018-2020 United States Government as represented by
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
/** @file license_check.cxx
 *
 * Check file for certain copyright/license signatures, and for
 * those that are 3rd party check that the appropriate license
 * information is included.
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
process_file(std::string f)
{
    std::regex copyright_regex(".*[Cc]opyright.*[12][0-9[0-9[0-9].*");
    std::regex gov_regex(".*United[ ]States[ ]Government.*");
    std::regex pd_regex(".*[Pp]ublic[ ][Dd]omain.*");
    std::string sline;
    std::ifstream fs;
    fs.open(f);
    if (!fs.is_open()) {
	std::cerr << "Unable to open " << f << " for reading, skipping\n";
	return -1;
    }
    int lcnt = 0;
    bool gov_copyright = false;
    bool other_copyright = false;
    bool public_domain = false;

    // Check the first 50 lines of the file for copyright statements
    while (std::getline(fs, sline) && lcnt < 50) {
	if (std::regex_match(sline, copyright_regex)) {
	    if (std::regex_match(sline, gov_regex)) {
		gov_copyright = true;
	    } else {
		other_copyright = true;
	    }
	} else {
	    if (std::regex_match(sline, pd_regex)) {
		public_domain = true;
	    }
	}
	lcnt++;
    }
    fs.close();

    if ((gov_copyright || other_copyright) && public_domain) {
	std::cout << f << " has copyright and public domain references\n";
	return 0;
    }
    if (gov_copyright && other_copyright) {
	std::cout << f << " has gov and non-gov copyright\n";
	return 0;
    }
    if (other_copyright) {
	std::cout << f << " has non-gov copyright\n";
	return 0;
    }
    if (public_domain) {
	std::cout << f << " references the public domain\n";
	return 0;
    }
    if (!gov_copyright && !other_copyright && !public_domain) {
	std::cout << f << " has no info\n";
    }
    return 0;
}

int
main(int argc, const char *argv[])
{
    std::regex o_regex(".*[\\/]other[\\/].*");
    std::regex t_regex(".*[\\/]misc/tools[\\/].*");
    std::regex r_regex(".*[\\/]misc/repoconv[\\/].*");
    std::regex srcfile_regex(".*[.](c|cpp|cxx|h|hpp|hxx|tcl)*$");

    if (argc < 3) {
	std::cerr << "Usage: license_check [-v] licenses_list file_list\n";
	return -1;
    }

    std::string sfile;
    std::ifstream src_file_stream;
    src_file_stream.open(argv[2]);
    if (!src_file_stream.is_open()) {
	std::cerr << "Unable to open source file list " << argv[2] << "\n";
    }
    while (std::getline(src_file_stream, sfile)) {
	if (std::regex_match(sfile, o_regex) || std::regex_match(sfile, t_regex) || std::regex_match(sfile, r_regex)) {
	    continue;
	}
	if (!std::regex_match(std::string(sfile), srcfile_regex)) {
	    continue;
	}
	//std::cout << "Checking " << sfile << "\n";
	if (process_file(sfile)) {
	    src_file_stream.close();
	    return -1;
	}
    }
    src_file_stream.close();

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

