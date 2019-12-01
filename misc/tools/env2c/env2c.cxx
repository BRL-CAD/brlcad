/*                       E N V 2 C . C X X
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


    ofile << "#include <stdlib.h>\n";
    ofile << "#include <string.h>\n";
    ofile << "#include \"bu/env.h\"\n";
    ofile << "#include \"bu/opt.h\"\n";
    ofile << "#include \"bu/path.h\"\n";
    ofile << "#include \"bu/str.h\"\n";
    ofile << "#include \"bu/vls.h\"\n";

    ofile << "static const char * const env_vars[] = {\n";
    for (e_it = evars.begin(); e_it != evars.end(); e_it++) {
	ofile << "\"" << *e_it << "\",\n";
    }
    ofile << "\"NULL\"";
    ofile << "};\n";

    ofile << "static const char *\n";
    ofile << "validate_env(const char *var)\n";
    ofile << "{\n";
    ofile << "    int i = 0;\n";
    ofile << "    while (!BU_STR_EQUAL(env_vars[i], \"NULL\")) {\n";
    ofile << "        if (BU_STR_EQUAL(env_vars[i], var)) return var;\n";
    ofile << "        i++;\n";
    ofile << "    }\n";
    ofile << "    return NULL;\n";
    ofile << "}\n";
    ofile << "\n";
    ofile << "static void\n";
    ofile << "list_env(struct bu_vls *vl, const char *pattern, int list_all)\n";
    ofile << "{\n";
    ofile << "    int i = 0;\n";
    ofile << "    while (!BU_STR_EQUAL(env_vars[i], \"NULL\")) {\n";
    ofile << "        if (!bu_path_match(pattern, env_vars[i], 0)) {\n";
    ofile << "            char *evval = getenv(env_vars[i]);\n";
    ofile << "            if (!list_all && !evval) {\n";
    ofile << "                i++;\n";
    ofile << "                continue;\n";
    ofile << "            }\n";
    ofile << "            bu_vls_printf(vl, \"%s=%s\\n\", env_vars[i], evval);\n";
    ofile << "        }\n";
    ofile << "        i++;\n";
    ofile << "    }\n";
    ofile << "}\n";

    ofile << "static int\n";
    ofile << "env_cmd(struct bu_vls *s_out, int argc, const char **argv)\n";
    ofile << "{\n";
    ofile << "    int print_help = 0;\n";
    ofile << "    int list_all = 0;\n";
    ofile << "\n";
    ofile << "    static const char *usage1 = \"Usage: env [-hA] [pattern]\\n\";\n";
    ofile << "    static const char *usage2 = \" env get var_name\\n\";\n";
    ofile << "    static const char *usage3 = \" env set var_name var_val\\n\";\n";
    ofile << "\n";
    ofile << "    struct bu_opt_desc d[3];\n";
    ofile << "    BU_OPT(d[0], \"h\", \"help\", \"\", NULL, &print_help, \"Print help and exit\");\n";
    ofile << "    BU_OPT(d[1], \"A\", \"all\",  \"\", NULL, &list_all,   \"List all relevant variables, not just those currently set\");\n";
    ofile << "    BU_OPT_NULL(d[2]);\n";
    ofile << "\n";
    ofile << "    /* skip command name argv[0] */\n";
    ofile << "    argc-=(argc>0); argv+=(argc>0);\n";
    ofile << "\n";
    ofile << "    /* parse standard options */\n";
    ofile << "    argc = bu_opt_parse(NULL, argc, argv, d);\n";
    ofile << "\n";
    ofile << "    if (print_help) {\n";
    ofile << "	bu_vls_printf(s_out, \"%s      %s      %s\", usage1, usage2, usage3);\n";
    ofile << "	return 2;\n";
    ofile << "    }\n";
    ofile << "\n";
    ofile << "    if (argc) {\n";
    ofile << "	if (BU_STR_EQUAL(argv[0], \"get\")) {\n";
    ofile << "	    if (argc != 2) {\n";
    ofile << "		bu_vls_printf(s_out, \"Usage: %s\", usage2);\n";
    ofile << "		return -1;\n";
    ofile << "	    }\n";
    ofile << "	    if (!validate_env(argv[1])) {\n";
    ofile << "		bu_vls_printf(s_out, \"unknown environment variable: %s\", argv[1]);\n";
    ofile << "		return -1;\n";
    ofile << "	    }\n";
    ofile << "\n";
    ofile << "	    bu_vls_printf(s_out, \"%s\", getenv(argv[1]));\n";
    ofile << "	    return 0;\n";
    ofile << "	}\n";
    ofile << "\n";
    ofile << "	if (BU_STR_EQUAL(argv[0], \"set\")) {\n";
    ofile << "	    if (argc != 3) {\n";
    ofile << "		bu_vls_printf(s_out, \"Usage: %s\", usage3);\n";
    ofile << "		return -1;\n";
    ofile << "	    }\n";
    ofile << "	    if (!validate_env(argv[1])) {\n";
    ofile << "		bu_vls_printf(s_out, \"unknown environment variable: %s\", argv[1]);\n";
    ofile << "		return -1;\n";
    ofile << "	    }\n";
    ofile << "\n";
    ofile << "	    if (bu_setenv(argv[1], argv[2], 1)) {\n";
    ofile << "		bu_vls_printf(s_out, \"error when setting variable %s to %s\", argv[1], argv[2]);\n";
    ofile << "		return -1;\n";
    ofile << "	    } else {\n";
    ofile << "		bu_vls_printf(s_out, \"%s\", argv[2]);\n";
    ofile << "		return 0;\n";
    ofile << "	    }\n";
    ofile << "	}\n";
    ofile << "    }\n";
    ofile << "\n";
    ofile << "    /* Not getting or setting, so we must be listing. */\n";
    ofile << "    if (!argc) {\n";
    ofile << "	list_env(s_out, \"*\", list_all);\n";
    ofile << "    } else {\n";
    ofile << "	int i = 0;\n";
    ofile << "	for (i = 0; i < argc; i++) {\n";
    ofile << "	    list_env(s_out, argv[i], list_all);\n";
    ofile << "	}\n";
    ofile << "    }\n";
    ofile << "    return 0;\n";
    ofile << "}\n";


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

