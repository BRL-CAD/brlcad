/*               E M B E D D E D _ C H E C K . C X X
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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
/** @file embedded_check.cxx
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

/* For performance, we don't read the entire file looking for
 * the copyright/license information. */
#define MAX_LINES_CHECK 100

int
process_file(std::string f, std::map<std::string, std::string> &file_to_license)
{
    std::regex cad_regex(".*BRL-CAD.*");
    std::regex copyright_regex(".*[Cc]opyright.*[1-2][0-9][0-9][0-9].*");
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
    bool brlcad_file = false;
    bool gov_copyright = false;
    bool other_copyright = false;
    bool public_domain = false;

    // Check the first 50 lines of the file for copyright statements
    while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	if (std::regex_match(sline, cad_regex)) {
	    brlcad_file = true;
	}
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


    if (gov_copyright && public_domain) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cerr << "FILE " << f << " has no associated reference in a license file! (gov copyright + public domain references)\n";
	    return 1;
	}
	return 0;
    }
    if (gov_copyright && other_copyright) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cerr << "FILE " << f << " has gov copyright + additional copyrights, bot no documenting file in doc/legal/embedded\n";
	    return 1;
	}
	return 0;
    }
    if (other_copyright) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cerr << "FILE " << f << " has no associated reference in a license file!\n";
	    return 1;
	}
	return 0;
    }
    if (public_domain) {
	if (!brlcad_file) {
	    if (file_to_license.find(f) == file_to_license.end()) {
		std::cout << f << " references the public domain, is not a BRL-CAD file, but has no documenting file in doc/legal/embedded\n";
		return 1;
	    }
	}
	return 0;
    }
    if (!gov_copyright && !other_copyright && !public_domain) {
	if (file_to_license.find(f) == file_to_license.end()) {
	    std::cout << "FILE " << f << " has no info\n";
	    return 1;
	} else {
	    std::cout << f << " has no embedded info but is referenced by license file " << file_to_license[f] << "\n";
	    return 1;
	}
    }
    return 0;
}

int
main(int argc, const char *argv[])
{
    try {

	if (argc < 4) {
	    std::cerr << "Usage: embedded_check [-v] licenses_list file_list src_root\n";
	    return -1;
	}

	std::regex f_regex("file:/(.*)");
	std::regex o_regex(".*[\\/]other[\\/].*");
	std::regex t_regex(".*[\\/]misc/tools[\\/].*");
	std::regex c_regex(".*[\\/]misc/CMake[\\/].*");
	std::regex r_regex(".*[\\/]misc/repoconv[\\/].*");
	std::regex rw_regex(".*[\\/]misc/repowork[\\/].*");
	std::regex d_regex(".*[\\/]doc[\\/].*");
	std::regex l_regex(".*[\\/]embedded_check.cpp");
	std::regex srcfile_regex(".*[.](c|cpp|cxx|h|hpp|hxx|tcl)*$");
	std::regex svn_regex(".*[\\/][.]svn[\\/].*");
	std::string root_path(argv[3]);

	std::map<std::string, std::string> file_to_license;
	std::set<std::string> unused_licenses;

	int bad_ref_cnt = 0;
	std::string lfile;
	std::ifstream license_file_stream;
	license_file_stream.open(argv[1]);
	if (!license_file_stream.is_open()) {
	    std::cerr << "Unable to open license file list " << argv[1] << "\n";
	}
	while (std::getline(license_file_stream, lfile)) {
	    if (std::regex_match(lfile, svn_regex)) {
		std::cerr << "Skipping .svn file " << lfile << "\n";
		continue;
	    }
	    int valid_ref_cnt = 0;
	    std::string lline;
	    std::ifstream license_stream;
	    license_stream.open(lfile);
	    if (!license_stream.is_open()) {
		std::cerr << "Unable to open license file " << lfile << "\n";
		continue;
	    }
	    while (std::getline(license_stream, lline)) {
		if (!std::regex_match(std::string(lline), f_regex)) {
		    continue;
		}
		std::smatch lfile_ref;
		if (!std::regex_search(lline, lfile_ref, f_regex)) {
		    continue;
		}
		std::string lfile_id =  root_path + std::string("/") + std::string(lfile_ref[1]);
		std::ifstream lfile_s(lfile_id);
		if (!lfile_s.good()) {
		    std::cout << "Bad reference in license file " << lfile << ": " << lline << "\n";
		    std::cout << "    file \"" << lfile_id << "\" not found on filesystem.\n";
		    bad_ref_cnt++;
		    continue;
		}
		lfile_s.close();
		//std::cout << "License file reference: " << lfile_id << "\n";
		file_to_license[lfile_id] = lfile;
		valid_ref_cnt++;
	    }
	    license_stream.close();
	    if (!valid_ref_cnt) {
		std::cout << "Unused license: " << lfile << "\n";
		unused_licenses.insert(lfile);
	    }
	}
	license_file_stream.close();

	int process_fail_cnt = 0;
	std::string sfile;
	std::ifstream src_file_stream;
	src_file_stream.open(argv[2]);
	if (!src_file_stream.is_open()) {
	    std::cerr << "Unable to open source file list " << argv[2] << "\n";
	}
	while (std::getline(src_file_stream, sfile)) {
	    if (std::regex_match(sfile, o_regex) || std::regex_match(sfile, t_regex)
		    || std::regex_match(sfile, r_regex) || std::regex_match(sfile, c_regex)
		    || std::regex_match(sfile, d_regex) ||  std::regex_match(sfile, l_regex)
		    || std::regex_match(sfile, rw_regex)) {
		continue;
	    }
	    if (!std::regex_match(std::string(sfile), srcfile_regex)) {
		continue;
	    }
	    //std::cout << "Checking " << sfile << "\n";
	    process_fail_cnt += process_file(sfile, file_to_license);
	}
	src_file_stream.close();

	if (unused_licenses.size() || bad_ref_cnt || process_fail_cnt) {
	    return -1;
	}

    }

    catch (const std::regex_error& e) {
	std::cout << "regex error: " << e.what() << '\n';
	return -1;
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

