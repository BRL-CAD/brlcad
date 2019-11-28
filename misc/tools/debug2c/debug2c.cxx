/*                      D E B U G 2 C . C X X
 * BRL-CAD
 *
 * Copyright (c) 2018-2019 United States Government as represented by
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
/** @file debug2c.cxx
 *
 * Find all debugging variables defined by BRL-CAD and generate a file defining
 * information useful for the debugging command.
 */

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

/* Uncomment to debug stand-alone */
//#define TEST_MAIN 1

struct dentry {
    std::string key;
    std::string hex;
    std::string info;
};

int
process_file(std::string f, std::map<std::string, std::vector<struct dentry>> &entries, int verbose)
{
    std::regex srcfile_regex(".*debug[.](h|hpp|hxx)(\\.in)*$");
    if (std::regex_match(std::string(f), srcfile_regex)) {
	std::regex debug_regex("#define\\s+.*_DEBUG_.*\\s+0x.*");
	std::string sline;
	std::ifstream fs;
	fs.open(f);
	if (!fs.is_open()) {
	    std::cerr << "Unable to open " << f << " for reading, skipping\n";
	    return -1;
	}
	while (std::getline(fs, sline)) {
	    if (std::regex_match(sline, debug_regex)) {
		std::regex unused_regex(".*UNUSED.*");
		if (std::regex_match(sline, unused_regex)) {
		    continue;
		}
		std::regex parse_regex("#define\\s+([A-Z]+)_DEBUG_([A-Z_0-9]+)\\s+([0-9x]+)[/(< \t*]*([^*]*).*");
		std::smatch parsevar;
		if (!std::regex_search(sline, parsevar, parse_regex)) {
		    std::cerr << "Error, debug2c could not parse debug variable definition:\n";
		    std::cerr << sline << "\n";
		    return -1;
		} else {
		    struct dentry nentry;
		    nentry.key = std::string(parsevar[2]);
		    nentry.hex = std::string(parsevar[3]);
		    nentry.info = std::string(parsevar[4]);
		    entries[std::string(parsevar[1])].push_back(nentry);
		}
	    }
	}
	fs.close();
    }

    return 0;
}

int
main(int argc, const char *argv[])
{
    int verbose = 0;
    std::map<std::string, std::vector<struct dentry>> entries;
    std::map<std::string, std::vector<struct dentry>>::iterator e_it;
    std::set<std::string> libs;
    std::set<std::string>::iterator s_it;

    if (argc < 3) {
	std::cerr << "Usage: debug2c [-v] file_list output_file\n";
	return -1;
    }

    if (argc == 4) {
	if (std::string(argv[1]) == std::string("-v")) {
	    argc--;
	    argv++;
	    verbose = 1;
	} else {
	    std::cerr << "Usage: debug2c [-v] file_list output_file\n";
	    return -1;
	}
    }

    std::string sfile;
    std::ifstream fs;
    fs.open(argv[1]);
    if (!fs.is_open()) {
	std::cerr << "Unable to open file list " << argv[1] << "\n";
    }
    while (std::getline(fs, sfile)) {
	if (process_file(sfile, entries, verbose)) {
	    fs.close();
	    return -1;
	}
    }
    fs.close();

    std::ofstream ofile;
    ofile.open(argv[2], std::fstream::trunc);
    if (!ofile.is_open()) {
	std::cerr << "Unable to open output file " << argv[2] << " for writing!\n";
	return -1;
    }

    ofile << "#include <stddef.h>\n";
    ofile << "#include <stdio.h>\n\n";

    ofile << "struct bdebug_entry {\n";
    ofile << "  const char * const key;\n";
    ofile << "  unsigned long val;\n";
    ofile << "  const char * const info;\n";
    ofile << "};\n\n";

    for (e_it = entries.begin(); e_it != entries.end(); e_it++) {
	libs.insert(e_it->first);
    }

    for (s_it = libs.begin(); s_it != libs.end(); s_it++) {
	ofile << "static const struct bdebug_entry " << *s_it << "_dbg_entries[] = {\n";
	std::vector<struct dentry> lib_entries = entries[*s_it];
	for (size_t i = 0; i < lib_entries.size(); i++) {
	    ofile << "   {\"" << lib_entries[i].key << "\"," << lib_entries[i].hex << ",\"" << lib_entries[i].info << "\"},\n";
	}
	ofile << "   {\"NULL\",0,\"NULL\"}\n";
	ofile << " };\n";
    }

    ofile << "static const char * const dbg_libs[] = {\n";
    for (s_it = libs.begin(); s_it != libs.end(); s_it++) {
	ofile << "\"" << *s_it << "\",\n";
    }
    ofile << "\"NULL\"";
    ofile << "};\n\n";

#ifdef TEST_MAIN
    for (s_it = libs.begin(); s_it != libs.end(); s_it++) {
	ofile << "int ";
	const char *cstr = s_it->c_str();
	for (size_t i = 0; i < s_it->length(); i++) {
	    ofile << (char)tolower(cstr[i]);
	}
	ofile << "_debug;\n";
    }
#endif

    ofile << "static unsigned int * const dbg_vars[] = {\n";
    for (s_it = libs.begin(); s_it != libs.end(); s_it++) {
	ofile << "&";
	const char *cstr = s_it->c_str();
	for (size_t i = 0; i < s_it->length(); i++) {
	    ofile << (char)tolower(cstr[i]);
	}
	ofile << "_debug,\n";
    }
    ofile << "NULL";
    ofile << "};\n\n";

    ofile << "static const struct bdebug_entry * const dbg_lib_entries[] = {\n";
    for (s_it = libs.begin(); s_it != libs.end(); s_it++) {
	ofile << "   (const struct bdebug_entry *)&" << *s_it << "_dbg_entries,\n";
    }
    ofile << "NULL";
    ofile << "};\n\n";

#ifdef TEST_MAIN
    ofile << "int main() {\n";
    ofile << "  int lcnt = 0;\n";
    ofile << "  while (dbg_lib_entries[lcnt]) {\n";
    ofile << "    int ecnt = 0;\n";
    ofile << "    long eval = -1;\n";
    ofile << "    while (eval != 0) {\n";
    ofile << "       eval = dbg_lib_entries[lcnt][ecnt].val;\n";
    ofile << "       if (eval > 0) {\n";
    ofile << "         printf(\"%s: %s val: %ld (0x%08lx) %s\\n\", dbg_libs[lcnt], dbg_lib_entries[lcnt][ecnt].key, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].info);\n";
    ofile << "       }\n";
    ofile << "       ecnt++;\n";
    ofile << "    }\n";
    ofile << "    lcnt++;\n";
    ofile << "  }\n";
    ofile << "  return 0;\n";
    ofile << "}\n";
#endif

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

