/*
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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

int
main(int argc, const char *argv[])
{
    if (argc < 3) {
	std::cerr << "Usage: tktable_hdr ifile ofile\n";
	return -1;
    }

    std::ifstream fs;
    fs.open(argv[1]);
    if (!fs.is_open()) {
	std::cerr << "Unable to open input file " << argv[1] << "\n";
    }

    std::ofstream ofile;
    ofile.open(argv[2], std::fstream::trunc);
    if (!ofile.is_open()) {
	std::cerr << "Unable to open output file " << argv[2] << " for writing!\n";
	return -1;
    }

    std::string sline;
    while (std::getline(fs, sline)) {
	if (!sline.length()) {
	    continue;
	}
	if (sline[0] == '#') {
	    continue;
	}
	std::string nline;
	for (int i = 0; i < sline.length(); i++) {
	    if (sline[i] == '\\') {
		nline.append("\\\\");
	    } else if (sline[i] == '"') {
		nline.append("\\\"");
	    } else {
		nline.push_back(sline[i]);
	    }
	}
	ofile << "\"" << nline << "\\n\"\n";
    }
    fs.close();

    ofile.close();

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

