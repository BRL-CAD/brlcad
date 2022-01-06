/*                      D E B U G 2 C . C X X
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
#include <map>
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
    try {
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
    }
    catch (const std::regex_error& e) {
	std::cout << "regex error: " << e.what() << '\n';
	return -1;
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

    for (int i = 0; i < argc; i++) {
	std::string arg(argv[i]);
	if (arg == std::string("--help") || arg == std::string("-h") || arg == std::string("-?")) {
	    std::cerr << "Usage: debug2c [-v] file_list output_file\n";
	    return 0;
	}
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

    ofile << "#include \"common.h\"\n";
    ofile << "#include <stddef.h>\n";
    ofile << "#include <stdio.h>\n\n";

#ifndef TEST_MAIN
    ofile << "#include \"bu/str.h\"\n";
    ofile << "#include \"bu/vls.h\"\n\n";
#endif

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

#ifndef TEST_MAIN
    ofile << "static void\n";
    ofile << "print_all_lib_flags(struct bu_vls *vls, int lcnt, int max_strlen)\n";
    ofile << "{\n";
    ofile << "    int ecnt = 0;\n";
    ofile << "    long eval = -1;\n";
    ofile << "    if (!vls) {\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "    while (eval != 0) {\n";
    ofile << "	eval = dbg_lib_entries[lcnt][ecnt].val;\n";
    ofile << "	if (eval > 0) {\n";
    ofile << "	    bu_vls_printf(vls, \"   %*s (0x%08lx): %s\\n\", max_strlen, dbg_lib_entries[lcnt][ecnt].key, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].info);\n";
    ofile << "	}\n";
    ofile << "	ecnt++;\n";
    ofile << "    }\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "\n";
    ofile << "static void\n";
    ofile << "print_set_lib_flags(struct bu_vls *vls, int lcnt, int max_strlen)\n";
    ofile << "{\n";
    ofile << "    int ecnt = 0;\n";
    ofile << "    if (!vls) {\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "    while (dbg_lib_entries[lcnt][ecnt].val) {\n";
    ofile << "	unsigned int *cvect = dbg_vars[lcnt];\n";
    ofile << "	if (*cvect & dbg_lib_entries[lcnt][ecnt].val) {\n";
    ofile << "	    bu_vls_printf(vls, \"   %*s (0x%08lx): %s\\n\", max_strlen, dbg_lib_entries[lcnt][ecnt].key, dbg_lib_entries[lcnt][ecnt].val, dbg_lib_entries[lcnt][ecnt].info);\n";
    ofile << "	}\n";
    ofile << "	ecnt++;\n";
    ofile << "    }\n";
    ofile << "    bu_vls_printf(vls, \"\\n\");\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static void\n";
    ofile << "print_all_set_lib_flags(struct bu_vls *vls, int max_strlen)\n";
    ofile << "{\n";
    ofile << "    int lcnt = 0;\n";
    ofile << "    if (!vls) {\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "	int ecnt = 0;\n";
    ofile << "	int have_active = 0;\n";
    ofile << "	while (dbg_lib_entries[lcnt][ecnt].val) {\n";
    ofile << "	    unsigned int *cvect = dbg_vars[lcnt];\n";
    ofile << "	    if (*cvect & dbg_lib_entries[lcnt][ecnt].val) {\n";
    ofile << "		have_active = 1;\n";
    ofile << "		break;\n";
    ofile << "	    }\n";
    ofile << "	    ecnt++;\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "	if (!have_active) {\n";
    ofile << "	    lcnt++;\n";
    ofile << "	    continue;\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "	bu_vls_printf(vls, \"%s flags:\\n\", dbg_libs[lcnt]);\n";
    ofile << "	print_set_lib_flags(vls, lcnt, max_strlen);\n";
    ofile << "\n";
    ofile << "	lcnt++;\n";
    ofile << "    }\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static int\n";
    ofile << "debug_max_strlen()\n";
    ofile << "{\n";
    ofile << "    int max_strlen = -1;\n";
    ofile << "    int lcnt = 0;\n";
    ofile << "    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "	int ecnt = 0;\n";
    ofile << "	while (dbg_lib_entries[lcnt][ecnt].val != 0) {\n";
    ofile << "	    int slen = strlen(dbg_lib_entries[lcnt][ecnt].key);\n";
    ofile << "	    max_strlen = (slen > max_strlen) ? slen : max_strlen;\n";
    ofile << "	    ecnt++;\n";
    ofile << "	}\n";
    ofile << "	lcnt++;\n";
    ofile << "    }\n";
    ofile << "    return max_strlen;\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static void\n";
    ofile << "debug_print_help(struct bu_vls *vls)\n";
    ofile << "{\n";
    ofile << "    int lcnt = 0;\n";
    ofile << "    if (!vls) {\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "    bu_vls_printf(vls, \"debug [-h] [-l [lib]] [-C [lib]] [-V [lib] [val]] [lib [flag]]\\n\\n\");\n";
    ofile << "    bu_vls_printf(vls, \"Available libs:\\n\");\n";
    ofile << "    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "	bu_vls_printf(vls, \"\t%s\\n\", dbg_libs[lcnt]);\n";
    ofile << "	lcnt++;\n";
    ofile << "    }\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static void\n";
    ofile << "print_all_flags(struct bu_vls *vls, int max_strlen)\n";
    ofile << "{\n";
    ofile << "    int lcnt = 0;\n";
    ofile << "    if (!vls) {\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "	bu_vls_printf(vls, \"%s flags:\\n\", dbg_libs[lcnt]);\n";
    ofile << "	print_all_lib_flags(vls, lcnt, max_strlen);\n";
    ofile << "	bu_vls_printf(vls, \"\\n\");\n";
    ofile << "	lcnt++;\n";
    ofile << "    }\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static void\n";
    ofile << "print_select_flags(struct bu_vls *vls, const char *filter, int max_strlen)\n";
    ofile << "{\n";
    ofile << "    int lcnt = 0;\n";
    ofile << "    if (!vls) {\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "	if (!BU_STR_EQUAL(filter, \"*\") && !(BU_STR_EQUIV(filter, dbg_libs[lcnt]))) {\n";
    ofile << "	    lcnt++;\n";
    ofile << "	    continue;\n";
    ofile << "	}\n";
    ofile << "	bu_vls_printf(vls, \"%s flags:\\n\", dbg_libs[lcnt]);\n";
    ofile << "	print_all_lib_flags(vls, lcnt, max_strlen);\n";
    ofile << "	bu_vls_printf(vls, \"\\n\");\n";
    ofile << "	lcnt++;\n";
    ofile << "    }\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static int\n";
    ofile << "toggle_debug_flag(struct bu_vls *e_vls, const char *lib_filter, const char *flag_filter)\n";
    ofile << "{\n";
    ofile << "    int	lcnt = 0;\n";
    ofile << "    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "	if (BU_STR_EQUIV(lib_filter, dbg_libs[lcnt])) {\n";
    ofile << "	    unsigned int *cvect = dbg_vars[lcnt];\n";
    ofile << "	    int ecnt = 0;\n";
    ofile << "	    int found = 0;\n";
    ofile << "	    while (dbg_lib_entries[lcnt][ecnt].val) {\n";
    ofile << "		if (BU_STR_EQUIV(flag_filter, dbg_lib_entries[lcnt][ecnt].key)) {\n";
    ofile << "		    if (*cvect & dbg_lib_entries[lcnt][ecnt].val) {\n";
    ofile << "			*cvect = *cvect & ~(dbg_lib_entries[lcnt][ecnt].val);\n";
    ofile << "		    } else {\n";
    ofile << "			*cvect |= dbg_lib_entries[lcnt][ecnt].val;\n";
    ofile << "		    }\n";
    ofile << "		    found = 1;\n";
    ofile << "		    break;\n";
    ofile << "		}\n";
    ofile << "		ecnt++;\n";
    ofile << "	    }\n";
    ofile << "	    if (!found) {\n";
    ofile << "		if (e_vls) {\n";
    ofile << "		    bu_vls_printf(e_vls, \"invalid %s flag paramter: %s\\n\", dbg_libs[lcnt], flag_filter);\n";
    ofile << "		}\n";
    ofile << "		return -1;\n";
    ofile << "	    } else {\n";
    ofile << "		return lcnt;\n";
    ofile << "	    }\n";
    ofile << "	}\n";
    ofile << "	lcnt++;\n";
    ofile << "    }\n";
    ofile << "\n";
    ofile << "    if (e_vls) {\n";
    ofile << "	bu_vls_printf(e_vls, \"invalid lib paramter: %s\\n\", lib_filter);\n";
    ofile << "    }\n";
    ofile << "    return -1;\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static void\n";
    ofile << "print_flag_hex_val(struct bu_vls *vls, int lcnt, int max_strlen, int labeled)\n";
    ofile << "{\n";
    ofile << "    unsigned int *cvect = dbg_vars[lcnt];\n";
    ofile << "    if (!vls) {\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "    if (labeled) {\n";
    ofile << "	bu_vls_printf(vls, \"%*s: 0x%08x\\n\", max_strlen, dbg_libs[lcnt], *cvect);\n";
    ofile << "    } else {\n";
    ofile << "	bu_vls_printf(vls, \"0x%08x\\n\", *cvect);\n";
    ofile << "    }\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static int\n";
    ofile << "set_flag_hex_value(struct bu_vls *o_vls, struct bu_vls *e_vls, const char *lib_filter, const char *hexstr, int max_strlen)\n";
    ofile << "{\n";
    ofile << "    int lcnt = 0;\n";
    ofile << "    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "	if (BU_STR_EQUIV(lib_filter, dbg_libs[lcnt])) {\n";
    ofile << "	    unsigned int *cvect = dbg_vars[lcnt];\n";
    ofile << "	    /* If we have a hex number, set it */\n";
    ofile << "	    if (hexstr[0] == '0' && hexstr[1] == 'x') {\n";
    ofile << "		long fvall = strtol(hexstr, NULL, 0);\n";
    ofile << "		if (fvall < 0) {\n";
    ofile << "		    if (e_vls) {\n";
    ofile << "			bu_vls_printf(e_vls, \"unusable hex value %ld\\n\", fvall);\n";
    ofile << "		    }\n";
    ofile << "		    return -1;\n";
    ofile << "		}\n";
    ofile << "		*cvect = (unsigned int)fvall;\n";
    ofile << "	    } else {\n";
    ofile << "		if (e_vls) {\n";
    ofile << "		    bu_vls_printf(e_vls, \"invalid hex string %s\\n\", hexstr);\n";
    ofile << "		}\n";
    ofile << "		return -1;\n";
    ofile << "	    }\n";
    ofile << "	    if (o_vls) {\n";
    ofile << "		print_flag_hex_val(o_vls, lcnt, max_strlen, 0);\n";
    ofile << "	    }\n";
    ofile << "	    return 0;\n";
    ofile << "	}\n";
    ofile << "	lcnt++;\n";
    ofile << "    }\n";
    ofile << "    if (e_vls) {\n";
    ofile << "	bu_vls_printf(e_vls, \"invalid input: %s\\n\", lib_filter);\n";
    ofile << "    }\n";
    ofile << "    return -1;\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static int\n";
    ofile << "debug_cmd(struct bu_vls *o_vls, struct bu_vls *e_vls, int argc, const char **argv)\n";
    ofile << "{\n";
    ofile << "    int lcnt = 0;\n";
    ofile << "    int max_strlen = -1;\n";
    ofile << "\n";
    ofile << "    if (argc > 4) {\n";
    ofile << "	debug_print_help(e_vls);\n";
    ofile << "	return -1;\n";
    ofile << "    }\n";
    ofile << "\n";
    ofile << "    max_strlen = debug_max_strlen();\n";
    ofile << "\n";
    ofile << "    if (argc > 1) {\n";
    ofile << "\n";
    ofile << "	if (BU_STR_EQUAL(argv[1], \"-h\")) {\n";
    ofile << "	    debug_print_help(o_vls);\n";
    ofile << "	    return 0;\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "	if (BU_STR_EQUAL(argv[1], \"-l\")) {\n";
    ofile << "	    if (argc == 2) {\n";
    ofile << "		print_all_flags(o_vls, max_strlen);\n";
    ofile << "		return 0;\n";
    ofile << "	    }\n";
    ofile << "\n";
    ofile << "	    if (argc == 3) {\n";
    ofile << "		print_select_flags(o_vls, argv[2], max_strlen);\n";
    ofile << "		return 0;\n";
    ofile << "	    }\n";
    ofile << "\n";
    ofile << "	    debug_print_help(e_vls);\n";
    ofile << "	    return -1;\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "	if (BU_STR_EQUAL(argv[1], \"-C\")) {\n";
    ofile << "	    if (argc == 2) {\n";
    ofile << "		/* Bare -C option - zero all the hex values */\n";
    ofile << "		lcnt = 0;\n";
    ofile << "		while (dbg_lib_entries[lcnt]) {\n";
    ofile << "		    unsigned int *cvect = dbg_vars[lcnt];\n";
    ofile << "		    *cvect = 0;\n";
    ofile << "		    lcnt++;\n";
    ofile << "		}\n";
    ofile << "		return 0;\n";
    ofile << "	    }\n";
    ofile << "	    if (argc == 3) {\n";
    ofile << "		/* -C with arg - clear a specific library */\n";
    ofile << "		lcnt = 0;\n";
    ofile << "		while (dbg_lib_entries[lcnt]) {\n";
    ofile << "		    if (BU_STR_EQUIV(argv[2], dbg_libs[lcnt])) {\n";
    ofile << "			unsigned int *cvect = dbg_vars[lcnt];\n";
    ofile << "			*cvect = 0;\n";
    ofile << "			return 0;\n";
    ofile << "		    }\n";
    ofile << "		    lcnt++;\n";
    ofile << "		}\n";
    ofile << "		if (e_vls) {\n";
    ofile << "		    bu_vls_printf(e_vls, \"invalid input: %s\\n\", argv[2]);\n";
    ofile << "		}\n";
    ofile << "		return -1;\n";
    ofile << "	    }\n";
    ofile << "\n";
    ofile << "	    debug_print_help(e_vls);\n";
    ofile << "	    return -1;\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "\n";
    ofile << "	if (BU_STR_EQUAL(argv[1], \"-V\")) {\n";
    ofile << "	    if (argc == 2) {\n";
    ofile << "		/* Bare -v option - print all the hex values */\n";
    ofile << "		lcnt = 0;\n";
    ofile << "		while (dbg_lib_entries[lcnt]) {\n";
    ofile << "		    print_flag_hex_val(o_vls, lcnt, max_strlen, 1);\n";
    ofile << "		    lcnt++;\n";
    ofile << "		}\n";
    ofile << "		return 0;\n";
    ofile << "	    }\n";
    ofile << "	    if (argc == 3) {\n";
    ofile << "		lcnt = 0;\n";
    ofile << "		while (dbg_lib_entries[lcnt]) {\n";
    ofile << "		    if (BU_STR_EQUIV(argv[2], dbg_libs[lcnt])) {\n";
    ofile << "			print_flag_hex_val(o_vls, lcnt, max_strlen, 0);\n";
    ofile << "			return 0;\n";
    ofile << "		    }\n";
    ofile << "		    lcnt++;\n";
    ofile << "		}\n";
    ofile << "		if (e_vls) {\n";
    ofile << "		    bu_vls_printf(e_vls, \"invalid input: %s\\n\", argv[2]);\n";
    ofile << "		}\n";
    ofile << "		return -1;\n";
    ofile << "	    }\n";
    ofile << "	    if (argc > 3) {\n";
    ofile << "		if (set_flag_hex_value(o_vls, e_vls, argv[2], argv[3], max_strlen)) {\n";
    ofile << "		    return -1;\n";
    ofile << "		}\n";
    ofile << "		return 0;\n";
    ofile << "	    }\n";
    ofile << "\n";
    ofile << "	    debug_print_help(e_vls);\n";
    ofile << "	    return -1;\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "\n";
    ofile << "	if (argc == 2) {\n";
    ofile << "	    lcnt = 0;\n";
    ofile << "	    while (dbg_lib_entries[lcnt]) {\n";
    ofile << "		if (BU_STR_EQUIV(argv[1], dbg_libs[lcnt])) {\n";
    ofile << "		    print_set_lib_flags(o_vls, lcnt, max_strlen);\n";
    ofile << "		    return 0;\n";
    ofile << "		}\n";
    ofile << "		lcnt++;\n";
    ofile << "	    }\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "	if (argc == 3) {\n";
    ofile << "	    lcnt = toggle_debug_flag(e_vls, argv[1], argv[2]);\n";
    ofile << "	    if (lcnt < 0) {\n";
    ofile << "		return -1;\n";
    ofile << "	    } else {\n";
    ofile << "		print_set_lib_flags(o_vls, lcnt, max_strlen);\n";
    ofile << "		return 0;\n";
    ofile << "	    }\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "	debug_print_help(e_vls);\n";
    ofile << "	return -1;\n";
    ofile << "    }\n";
    ofile << "\n";
    ofile << "    print_all_set_lib_flags(o_vls, max_strlen);\n";
    ofile << "\n";
    ofile << "    return 0;\n";
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

