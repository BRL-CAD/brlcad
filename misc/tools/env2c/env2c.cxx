/*                       E N V 2 C . C X X
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
/** @file env2c.cxx
 *
 * Find all environment variables checked with getenv by BRL-CAD
 * and generate a file defining a static array listing them.
 */

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

void
process_file(std::string f, std::set<std::string> &evars, int verbose)
{
    std::regex srcfile_regex(".*[.](cxx|c|cpp|h|hpp|hxx)(\\.in)*$");
    if (std::regex_match(std::string(f), srcfile_regex)) {
	std::regex getenv_regex(".*getenv\\(\\\".*");
	std::string sline;
	std::ifstream fs;
	fs.open(f);
	if (!fs.is_open()) {
	    std::cerr << "Unable to open " << f << " for reading, skipping\n";
	    return;
	}
	while (std::getline(fs, sline)) {
	    if (std::regex_match(sline, getenv_regex)) {
		std::regex evar_regex(".*getenv\\(\\\"([^\\\"]+)\\\"\\).*");
		std::smatch envvar;
		if (!std::regex_search(sline, envvar, evar_regex)) {
		    std::cerr << "Error, could not find environment variable in file " << f << " line:\n" << sline << "\n";
		} else {
		    evars.insert(std::string(envvar[1]));
		    if (verbose) {
			std::cout << f << ": " << envvar[1] << "\n";
		    }
		}
	    }
	}
	fs.close();
    }
}

int
main(int argc, const char *argv[])
{
    int verbose = 0;
    std::set<std::string> evars;
    std::set<std::string>::iterator e_it;

    if (argc < 3) {
	std::cerr << "Usage: env2c [-v] file_list output_file\n";
	return -1;
    }

    if (argc == 4) {
	if (std::string(argv[1]) == std::string("-v")) {
	    argc--;
	    argv++;
	    verbose = 1;
	} else {
	    std::cerr << "Usage: env2c [-v] file_list output_file\n";
	    return -1;
	}
    }

    std::regex skip_regex(".*/(other|tests|tools|bullet|docbook)/.*");
    std::string sfile;
    std::ifstream fs;
    fs.open(argv[1]);
    if (!fs.is_open()) {
	std::cerr << "Unable to open file list " << argv[1] << "\n";
    }
    while (std::getline(fs, sfile)) {
	if (!std::regex_match(sfile, skip_regex)) {
	    process_file(sfile, evars, verbose);
	}
    }
    fs.close();

    std::ofstream ofile;
    ofile.open(argv[2], std::fstream::trunc);
    if (!ofile.is_open()) {
	std::cerr << "Unable to open output file " << argv[2] << " for writing!\n";
	return -1;
    }

    ofile << "static const char * const env_vars[] = {\n";
    for (e_it = evars.begin(); e_it != evars.end(); e_it++) {
	ofile << "\"" << *e_it << "\",\n";
    }
    ofile << "\"NULL\"";
    ofile << "};\n";

    ofile.close();

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

