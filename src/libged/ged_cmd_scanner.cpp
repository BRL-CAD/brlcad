/*             G E D _ C M D _ S C A N N E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2024 United States Government as represented by
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
/** @file ged_cmd_scanner.cpp
 *
 * Given a file defining a libged plugin, extract and report the names of the
 * command(s) that this plugin provides.
 *
 * TODO - the conventions would be a bit different, but it might also be useful
 * to be able to scan for tables of subcommands (either bu_cmdtab or std::map -
 * to support such a scheme we could typedef specific types for tables intended
 * for scanning.)  Right now we have a lot of bot_* functions, for example,
 * that will eventually be subsumed into bot - if we want calling code to be
 * able to have some C compile-time assurance that those subcommands haven't
 * changed name or been removed, we might actually be able to do it.
 */

#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

int
main(int argc, const char *argv[])
{
    if (argc != 2) {
	std::cerr << "Usage: ged_cmd_scan input_file\n";
	return EXIT_FAILURE;
    }

    std::ifstream fs;
    fs.open(argv[1]);
    if (!fs.is_open()) {
	std::cerr << "Unable to open file " << argv[1] << "\n";
	return EXIT_FAILURE;
    }

    std::regex cmd_impl_regex(".*ged_cmd_impl.*");
    std::regex cmd_str_regex(".*\"([A-Za-z0-9?_]+)\".*");
    bool in_cmd_impl = false;
    std::string sline;
    while (std::getline(fs, sline)) {
	if (in_cmd_impl) {
	    std::smatch parsevar;
	    if (std::regex_search(sline, parsevar, cmd_str_regex)) {
		// In the ged_cmd_impl struct, we found a quoted
		// string - that's the command string, we're done.
		in_cmd_impl = false;
		// We can't use "?" in a C function name, so we have
		// to skip anything using that char
		std::string cmd = parsevar.str(1);
		if (cmd.find('?') != std::string::npos)
		    continue;
		std::cout << cmd << "\n";
	    }
	} else {
	    if (std::regex_match(sline, cmd_impl_regex)) {
		// If we're all on one line, snarf the name now
		std::smatch parsevar;
		if (std::regex_search(sline, parsevar, cmd_str_regex)) {
		    // We can't use "?" in a C function name, so we have
		    // to skip anything using that char
		    std::string cmd = parsevar.str(1);
		    if (cmd.find('?') != std::string::npos)
			continue;
		    std::cout << cmd << "\n";
		} else {
		    // No command - keep reading
		    in_cmd_impl = true;
		}
	    }
	}
    }

    fs.close();
    return EXIT_SUCCESS;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s

