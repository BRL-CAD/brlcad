/*                  S T R C L E A R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2023 United States Government as represented by
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
/** @file strclear.cpp
 *
 * Given a binary file and a string, replace any instances of the
 * string in the binary with null chars.
 *
 * TODO - for txt files, also want to be able to replace target
 * string with a different length replacement string - implement
 * that too
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int
main(int argc, const char *argv[])
{
    bool verbose = false;
    const char *usage = "Usage: strclear [-v] file string_to_clear";

    argc--; argv++;

    if (argc < 2) {
	std::cerr << usage << "\n";
	return -1;
    }

    if (argc == 3) {
	if (std::string(argv[0]) == std::string("-v")) {
	    argc--; argv++;
	    verbose = 1;
	} else {
	    std::cerr << usage << "\n";
	    return -1;
	}
    }

    std::string fname(argv[0]);
    std::string target_str(argv[1]);

    // Read binary contents
    std::ifstream input_fs;
    input_fs.open(fname, std::ios::binary);
    if (!input_fs.is_open()) {
	std::cerr << "Unable to open file " << argv[0] << "\n";
	return -1;
    }
    std::vector<char> bin_contents(std::istreambuf_iterator<char>(input_fs), {});
    input_fs.close();

    // Set up vectors of target and array of null chars
    std::vector<char> search_chars(target_str.begin(), target_str.end());
    std::vector<char> null_chars;
    for (size_t i = 0; i < search_chars.size(); i++)
	null_chars.push_back('\0');

    // Find instances of target string in binary, and replace any we find
    auto position = std::search(bin_contents.begin(), bin_contents.end(), search_chars.begin(), search_chars.end());
    int rcnt = 0;
    while (position != bin_contents.end()) {
	std::copy(null_chars.begin(), null_chars.end(), position);
	rcnt++;
	if (verbose)
	    std::cout << "Replacing instance #" << rcnt << " of " << argv[1] << "\n";
	position = std::search(position, bin_contents.end(), search_chars.begin(), search_chars.end());
    }
    if (!rcnt)
	return 0;

    // If we changed the contents, write them back out
    std::ofstream output_fs;
    output_fs.open(fname, std::ios::binary);
    if (!output_fs.is_open()) {
	std::cerr << "Unable to write updated file contents for " << argv[0] << "\n";
	return -1;
    }

    std::copy(bin_contents.begin(), bin_contents.end(), std::ostreambuf_iterator<char>(output_fs));
    output_fs.close();

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

