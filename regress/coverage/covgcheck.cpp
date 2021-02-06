/*                   C O V G C H E C K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file covgcheck.cpp
 *
 * Takes a list of executables and introspects test files to see
 * how many are addressed by the tests.
 *
 */

#include "common.h"

#include <cctype>
#include <cstdio>
#include <algorithm>
#include <locale>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <map>
#include <sstream>
#include <string>

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/mapped_file.h"
#include "bu/str.h"

#define TESTED_EXPECTED 17

class regress_info_t {
    public:

	/* comment filter */
	std::regex qfilter;

	/* command filters */
	std::map<std::string, std::vector<std::regex>> cmd_filters;

	/* match counts */
	std::map<std::string, int> r_cnts;
	std::map<std::string, int> cmd_cnts;

	// Outputs
	std::vector<std::string> cmd_log;
};


void
regex_init(regress_info_t &r, const char *exec_list, int verbosity) {

    r.qfilter = std::regex("[[:space:]]*#.*");

    std::ifstream exec_stream;
    exec_stream.open(exec_list);
    if (!exec_stream.is_open()) {
	std::cerr << "Unable to open exist list file " << exec_list << "\n";
	return;
    }

    std::string efile;
    while (std::getline(exec_stream, efile)) {
	std::string ename = std::string(efile);

	if (verbosity)
	    std::cout << ename << "\n";

	std::string crf;
	std::transform(ename.begin(), ename.end(), ename.begin(), [](unsigned char c){ return std::toupper(c); });

	crf = std::string(".*COMMAND.*[$][{]") + ename + std::string("(_EXE)?(C)?[}].*");
	r.cmd_filters[ename].push_back(std::regex(crf));
	crf = std::string("^[[:space:]]*(run)+[[:space:]]*[\"]?[$]") + ename + std::string("(_EXE)?(C)?.*");
	r.cmd_filters[ename].push_back(std::regex(crf));
	crf = std::string("^[[:space:]]*(cmd)+[[:space:]]*[=]+[[:space:]]*[[:space:]]*[\"]?[$]") + ename + std::string("(_EXE)?(C)?.*");
	r.cmd_filters[ename].push_back(std::regex(crf));
	crf = std::string("^[[:space:]]*(\")?[$]") + ename + std::string("(_EXE)?(C)?.*");
	r.cmd_filters[ename].push_back(std::regex(crf));

	r.r_cnts[ename] = 0;
	r.cmd_cnts[ename] = 0;

   }

}

int
cmd_cnt(regress_info_t &r, std::string &tfile, int verbosity)
{
    int ret = 0;

    if (verbosity)
	std::cout << tfile << "\n";

    struct bu_mapped_file *ifile = bu_open_mapped_file(tfile.c_str(), "candidate file");
    if (!ifile) {
	std::cerr << "Unable to open " << tfile << " for reading, skipping\n";
	return -1;
    }

    std::string fbuff((char *)ifile->buf);
    std::istringstream fs(fbuff);

    int lcnt = 0;
    std::string sline;
    while (std::getline(fs, sline)) {
	lcnt++;
	std::map<std::string, std::vector<std::regex>>::iterator ff_it;
	for (ff_it = r.cmd_filters.begin(); ff_it != r.cmd_filters.end(); ff_it++) {
	    int passed = 0;
	    if (!std::strstr(sline.c_str(), ff_it->first.c_str())) {
		// Only try the full regex if strstr says there is a chance
		continue;
	    }
	    if (std::regex_match(sline, r.qfilter)) {
		if (verbosity) {
		    std::cout << "Comment: " << sline << "\n";
		}
		continue;
	    }
	    std::vector<std::regex>::iterator r_it;
	    r.r_cnts[ff_it->first]++;
	    for (r_it = ff_it->second.begin(); r_it != ff_it->second.end(); r_it++) {
		if (std::regex_match(sline, *r_it)) {
		    r.cmd_cnts[ff_it->first]++;
		    passed = 1;
		    break;
		}
	    }
	    if (verbosity) {
		if (passed) {
		    std::cout << "Passed(" << ff_it->first << "): " << sline << "\n";
		} else {
		    std::cout << "Skipped(" << ff_it->first << "): " << sline << "\n";
		}
	    }
	}
    }

    bu_close_mapped_file(ifile);

    return ret;
}

int
main(int argc, const char *argv[])
{
    int verbosity = 0;

    if (argc < 3 || argc > 5) {
	std::cerr << "Usage: covgcheck [-v] prog_list source_list\n";
	return -1;
    }

    bu_setprogname(argv[0]);

    const char *f1, *f2;

    if (argc == 4) {
	if (BU_STR_EQUAL(argv[1], "-v")) {
	    verbosity = 1;
	    f1 = argv[2];
	    f2 = argv[3];
	} else {
	    bu_exit(-1, "invalid option %s", argv[1]);
	}
    } else {
	f1 = argv[1];
	f2 = argv[2];

    }

    regress_info_t r;
    regex_init(r, f1, verbosity);

    std::string sfile;
    std::ifstream src_file_stream;
    src_file_stream.open(f2);
    if (!src_file_stream.is_open()) {
	std::cerr << "Unable to open file list file " << argv[1] << "\n";
	return -1;
    }

    while (std::getline(src_file_stream, sfile)) {
	cmd_cnt(r, sfile, verbosity);
    }

    std::set<std::string> tested;
    std::set<std::string> untested;
    std::map<std::string, int>::iterator c_it;
    for (c_it = r.cmd_cnts.begin(); c_it != r.cmd_cnts.end(); c_it++) {
	std::string ename(c_it->first);
	std::transform(ename.begin(), ename.end(), ename.begin(), [](unsigned char c){ return std::tolower(c); });
	if (c_it->second) {
	    //std::cout << ename << ": " << r.r_cnts[c_it->first] << "," << c_it->second << "\n";
	    std::cout << ename << ": " << c_it->second << "\n";
	    tested.insert(ename);
	} else {
	    untested.insert(ename);
	}
    }
    if (untested.size()) {
	std::cout << "Untested: ";
	std::set<std::string>::iterator u_it;
	size_t ucnt = 1;
	for (u_it = untested.begin(); u_it != untested.end(); u_it++) {
	    if (ucnt < untested.size()) {
		std::cout << *u_it << ",";
	    } else {
		std::cout << *u_it << "\n";
	    }
	    ucnt++;
	}
    }

    if (tested.size() != TESTED_EXPECTED) {
	if (tested.size() < TESTED_EXPECTED) {
	    std::cerr << "Tested executable set is less than expected!\n";
	    return -1;
	} else {
	    std::cerr << "NOTE: update tested executables count. Expected " << TESTED_EXPECTED << ", found " << tested.size() << "\n";
	}
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
