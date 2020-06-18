/*                       E N V 2 C . C X X
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
/** @file env2c.cxx
 *
 * Find all environment variables checked with getenv by BRL-CAD
 * and generate a file defining a static array listing them.
 */

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <map>
#include <sstream>
#include <string>
#include <thread>

/* Uncomment to debug stand-alone */
//#define TEST_MAIN 1

class env_outputs {
    public:
	std::set<std::pair<std::string,std::string>> o_vars;
	std::set<std::pair<std::string,std::string>> blib_vars;
	std::set<std::pair<std::string,std::string>> bexe_vars;
	std::map<std::string, std::set<std::string>> c_vars;
	std::string f;
	int verbose;
};

void
process_file(env_outputs &env_t)
{
    env_outputs &env = const_cast<env_outputs &>(env_t);

    std::regex getenv_regex(".*getenv\\(\\\".*");
    std::regex evar_regex(".*getenv\\(\\\"([^\\\"]+)\\\"\\).*");
    std::regex o_regex(".*[\\/]other[\\/].*");
    std::regex sp_regex(".*[\\/]other[\\/]([A-Za-z0-9_-]+).*");
    std::regex lp_regex(".*[\\/]lib([A-Za-z0-9_-]+)[\\/].*");
    std::regex ep_regex(".*[\\/]src[\\/]([A-Za-z0-9_-]+)[\\/].*");
    std::regex bench_regex(".*[\\/](bench)[\\/].*");
    std::regex bullet_regex(".*[\\/](bullet)[\\/].*");

    std::regex srcfile_regex(".*[.](cxx|c|cpp|h|hpp|hxx)(\\.in)*$");
    if (!std::regex_match(env.f, srcfile_regex)) {
	return;
    }
    std::string sline;
    std::ifstream fs;
    fs.open(env.f);
    if (!fs.is_open()) {
	std::cerr << "Unable to open " << env.f << " for reading, skipping\n";
	return;
    }
    while (std::getline(fs, sline)) {
	if (!std::regex_match(sline, getenv_regex)) {
	    continue;
	}
	std::smatch envvar;
	if (!std::regex_search(sline, envvar, evar_regex)) {
	    std::cerr << "Error, could not find environment variable in file " << env.f << " line:\n" << sline << "\n";
	    continue;
	}

	if (std::regex_match(env.f, o_regex) || std::regex_match(env.f, bullet_regex)) {
	    std::smatch sp_match;
	    if (std::regex_search(env.f, sp_match, sp_regex)) {
		if (env.verbose) {
		    std::cout << sp_match[1] << "[SYSTEM]: " << envvar[1] << "\n";
		}
		env.o_vars.insert(std::make_pair(std::string(sp_match[1]), std::string(envvar[1])));
		env.c_vars[std::string(sp_match[1])].insert(std::string(envvar[1]));
		continue;
	    }
	    if (std::regex_match(env.f, bullet_regex)) {
		if (env.verbose) {
		    std::cout << "bullet[SYSTEM]: " << envvar[1] << "\n";
		}
		env.o_vars.insert(std::make_pair(std::string("bullet"), std::string(envvar[1])));
		env.c_vars[std::string("bullet")].insert(std::string(envvar[1]));
		continue;
	    }
	    std::cout << env.f << "[SYSTEM]: " << envvar[1] << "\n";
	    continue;
	}

	{
	    std::smatch lp_match;
	    if (std::regex_search(env.f, lp_match, lp_regex)) {
		if (env.verbose) {
		    std::cout << "lib" << lp_match[1] << ": " << envvar[1] << "\n";
		}
		env.blib_vars.insert(std::make_pair(std::string(lp_match[1]), std::string(envvar[1])));
		env.c_vars[std::string(lp_match[1])].insert(std::string(envvar[1]));
		continue;
	    }
	}

	{
	    std::smatch ep_match;
	    if (std::regex_search(env.f, ep_match, ep_regex)) {
		if (env.verbose) {
		    std::cout << ep_match[1] << ": " << envvar[1] << "\n";
		}
		env.bexe_vars.insert(std::make_pair(std::string(ep_match[1]), std::string(envvar[1])));
		env.c_vars[std::string(ep_match[1])].insert(std::string(envvar[1]));
		continue;
	    }
	}
	{
	    if (std::regex_match(env.f, bench_regex)) {
		if (env.verbose) {
		    std::cout << "bench: " << envvar[1] << "\n";
		}
		env.bexe_vars.insert(std::make_pair(std::string("bench"), std::string(envvar[1])));
		env.c_vars[std::string("bench")].insert(std::string(envvar[1]));
		continue;
	    }
	}

	std::cout << env.f << ": " << envvar[1] << "\n";
    }
    fs.close();

}

int
main(int argc, const char *argv[])
{
    int verbose = 0;
    std::set<std::string> all_vars;
    std::set<std::string> cad_vars;


    env_outputs env;
    std::set<std::pair<std::string,std::string>>::iterator v_it;
    std::map<std::string, std::set<std::string>>::iterator c_it;
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

    std::vector<std::string> sfiles;

    /*************************************************************/
    /*     Filter files down to the set we want to process       */
    /*************************************************************/
    std::regex skip_regex(".*/(tests|tools|bullet|docbook)/.*");
    //std::regex skip_regex(".*/(other|tests|tools|bullet|docbook)/.*");
    std::string sfile;
    std::ifstream fs;
    fs.open(argv[1]);
    if (!fs.is_open()) {
	std::cerr << "Unable to open file list " << argv[1] << "\n";
	return -1;
    }
    while (std::getline(fs, sfile)) {
	if (!std::regex_match(sfile, skip_regex)) {
	    sfiles.push_back(sfile);
	}
    }
    fs.close();


    /*************************************************************/
    /*  Process the files (mulithreaded mapping for performance) */
    /*************************************************************/
    unsigned int hwc = std::thread::hardware_concurrency();
    if (!hwc) {
	hwc = 10;
    }
    std::vector<env_outputs> envs;
    for (int i = 0; i < hwc; i++) {
	env_outputs enew;
	enew.verbose = verbose;
	envs.push_back(enew);
    }
    size_t fcnt = 0;
    while (fcnt < sfiles.size()) {
	int athreads = 0;
	std::vector<std::thread> t;
	for (int i = 0; i < hwc; i++) {
	    if (fcnt + i < sfiles.size()) {
		envs[i].f = sfiles[fcnt+i];
		t.push_back(std::thread(process_file, std::ref(envs[i])));
		athreads++;
	    }
	}
	for (int i = 0; i < athreads; i++) {
	    t[i].join();
	}
	fcnt += athreads;
    }

    /*************************************************************/
    /*     Reduce the individual thread results to single sets   */
    /*************************************************************/
    for (int i = 0; i < hwc; i++) {
	env.o_vars.insert(envs[i].o_vars.begin(), envs[i].o_vars.end());
	env.blib_vars.insert(envs[i].blib_vars.begin(), envs[i].blib_vars.end());
	env.bexe_vars.insert(envs[i].bexe_vars.begin(), envs[i].bexe_vars.end());
	std::map<std::string, std::set<std::string>>::iterator cv_it;
	for (cv_it = envs[i].c_vars.begin(); cv_it != envs[i].c_vars.end(); cv_it++) {
	    env.c_vars[cv_it->first].insert(cv_it->second.begin(), cv_it->second.end());
	}
    }


    /*************************************************************/
    /*            Build up categorized variable sets             */
    /*************************************************************/
    for (v_it = env.o_vars.begin(); v_it != env.o_vars.end(); v_it++) {
	all_vars.insert(v_it->second);
    }
    for (v_it = env.blib_vars.begin(); v_it != env.blib_vars.end(); v_it++) {
	all_vars.insert(v_it->second);
	cad_vars.insert(v_it->second);
    }
    for (v_it = env.bexe_vars.begin(); v_it != env.bexe_vars.end(); v_it++) {
	cad_vars.insert(v_it->second);
    }


    /*************************************************************/
    /*             Generate the command's source code            */
    /*************************************************************/
    std::ofstream ofile;
    ofile.open(argv[2], std::fstream::trunc);
    if (!ofile.is_open()) {
	std::cerr << "Unable to open output file " << argv[2] << " for writing!\n";
	return -1;
    }

    ofile << "#include \"common.h\"\n";
    ofile << "#include <stdlib.h>\n";
    ofile << "#include <string.h>\n";
    ofile << "#include \"bu/env.h\"\n";
    ofile << "#include \"bu/opt.h\"\n";
    ofile << "#include \"bu/path.h\"\n";
    ofile << "#include \"bu/str.h\"\n";
    ofile << "#include \"bu/vls.h\"\n";
    ofile << "\n";

    ofile << "static const char * const env_vars[] = {\n";
    for (e_it = all_vars.begin(); e_it != all_vars.end(); e_it++) {
	ofile << "  \"" << *e_it << "\",\n";
    }
    ofile << "\"NULL\"";
    ofile << "};\n\n";

    ofile << "static const char * const cad_env_vars[] = {\n";
    for (e_it = cad_vars.begin(); e_it != cad_vars.end(); e_it++) {
	ofile << "  \"" << *e_it << "\",\n";
    }
    ofile << "\"NULL\"";
    ofile << "};\n\n";

    ofile << "struct envcmd_entry {\n";
    ofile << "  const char * const key;\n";
    ofile << "  const char * const var;\n";
    ofile << "};\n\n";

    ofile << "static const struct envcmd_entry other_vars[] = {\n";
    for (v_it = env.o_vars.begin(); v_it != env.o_vars.end(); v_it++) {
	ofile << "  {\"" << v_it->first << "\",\"" << v_it->second << "\"},\n";
    }
    ofile << "  {\"NULL\", \"NULL\"}\n";
    ofile << "};\n\n";

    ofile << "static const struct envcmd_entry lib_vars[] = {\n";
    for (v_it = env.blib_vars.begin(); v_it != env.blib_vars.end(); v_it++) {
	ofile << "  {\"" << v_it->first << "\",\"" << v_it->second << "\"},\n";
    }
    ofile << "  {\"NULL\", \"NULL\"}\n";
    ofile << "};\n\n";

    ofile << "static const struct envcmd_entry exe_vars[] = {\n";
    for (v_it = env.bexe_vars.begin(); v_it != env.bexe_vars.end(); v_it++) {
	ofile << "  {\"" << v_it->first << "\",\"" << v_it->second << "\"},\n";
    }
    ofile << "  {\"NULL\", \"NULL\"}\n";
    ofile << "};\n\n";

    for (c_it = env.c_vars.begin(); c_it != env.c_vars.end(); c_it++) {
	std::string var_root = c_it->first;
	std::replace(var_root.begin(), var_root.end(), '-', '_');
	ofile << "static const char * const " << var_root << "_vars[] = {\n";
	std::set<std::string>::iterator s_it;
	for (s_it = c_it->second.begin(); s_it != c_it->second.end(); s_it++) {
	    ofile << "  \"" << *s_it << "\",\n";
	}
	ofile << "  NULL};\n";
    }

    ofile << "struct env_context_entry {\n";
    ofile << "  const char * const key;\n";
    ofile << "  const char * const * vars;\n";
    ofile << "};\n\n";

    ofile << "static const struct env_context_entry context_vars[] = {\n";
    for (c_it = env.c_vars.begin(); c_it != env.c_vars.end(); c_it++) {
	std::string var_root = c_it->first;
	std::replace(var_root.begin(), var_root.end(), '-', '_');
	ofile << "  {\"" << c_it->first << "\", (const char * const *)&" << var_root << "_vars},\n";
    }
    ofile << "{\"NULL\", NULL}\n};\n";

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
    ofile << "list_env(struct bu_vls *vl, const char *pattern, int list_all, int include_system, int include_context)\n";
    ofile << "{\n";
    ofile << "    int i = 0;\n";
    ofile << "    if (!include_context) {\n";
    ofile << "	if (include_system) {\n";
    ofile << "	    while (!BU_STR_EQUAL(env_vars[i], \"NULL\")) {\n";
    ofile << "		if (!bu_path_match(pattern, env_vars[i], 0)) {\n";
    ofile << "		    char *evval = getenv(env_vars[i]);\n";
    ofile << "		    if (!list_all && !evval) {\n";
    ofile << "			i++;\n";
    ofile << "			continue;\n";
    ofile << "		    }\n";
    ofile << "		    bu_vls_printf(vl, \"%s=%s\\n\", env_vars[i], evval);\n";
    ofile << "		}\n";
    ofile << "		i++;\n";
    ofile << "	    }\n";
    ofile << "	    return;\n";
    ofile << "	}\n";
    ofile << "	while (!BU_STR_EQUAL(cad_env_vars[i], \"NULL\")) {\n";
    ofile << "	    if (!bu_path_match(pattern, cad_env_vars[i], 0)) {\n";
    ofile << "		char *evval = getenv(cad_env_vars[i]);\n";
    ofile << "		if (!list_all && !evval) {\n";
    ofile << "		    i++;\n";
    ofile << "		    continue;\n";
    ofile << "		}\n";
    ofile << "		bu_vls_printf(vl, \"%s=%s\\n\", cad_env_vars[i], evval);\n";
    ofile << "	    }\n";
    ofile << "	    i++;\n";
    ofile << "	}\n";
    ofile << "	return;\n";
    ofile << "    }\n";
    ofile << "\n";
    ofile << "    if (include_system) {\n";
    ofile << "	while (!BU_STR_EQUAL(other_vars[i].key, \"NULL\")) {\n";
    ofile << "	    if (!bu_path_match(pattern, other_vars[i].var, 0)) {\n";
    ofile << "		char *evval = getenv(other_vars[i].var);\n";
    ofile << "		if (!list_all && !evval) {\n";
    ofile << "		    i++;\n";
    ofile << "		    continue;\n";
    ofile << "		}\n";
    ofile << "		bu_vls_printf(vl, \"[%s] %s=%s\\n\", other_vars[i].key, other_vars[i].var, evval);\n";
    ofile << "	    }\n";
    ofile << "	    i++;\n";
    ofile << "	}\n";
    ofile << "    }\n";
    ofile << "    i = 0;\n";
    ofile << "    while (!BU_STR_EQUAL(lib_vars[i].key, \"NULL\")) {\n";
    ofile << "	if (!bu_path_match(pattern, lib_vars[i].var, 0)) {\n";
    ofile << "	    char *evval = getenv(lib_vars[i].var);\n";
    ofile << "	    if (!list_all && !evval) {\n";
    ofile << "		i++;\n";
    ofile << "		continue;\n";
    ofile << "	    }\n";
    ofile << "	    bu_vls_printf(vl, \"[lib%s] %s=%s\\n\", lib_vars[i].key, lib_vars[i].var, evval);\n";
    ofile << "	}\n";
    ofile << "	i++;\n";
    ofile << "    }\n";
    ofile << "\n";
    ofile << "    i = 0;\n";
    ofile << "    while (!BU_STR_EQUAL(exe_vars[i].key, \"NULL\")) {\n";
    ofile << "	if (!bu_path_match(pattern, exe_vars[i].var, 0)) {\n";
    ofile << "	    char *evval = getenv(exe_vars[i].var);\n";
    ofile << "	    if (!list_all && !evval) {\n";
    ofile << "		i++;\n";
    ofile << "		continue;\n";
    ofile << "	    }\n";
    ofile << "	    bu_vls_printf(vl, \"[%s] %s=%s\\n\", exe_vars[i].key, exe_vars[i].var, evval);\n";
    ofile << "	}\n";
    ofile << "	i++;\n";
    ofile << "    }\n";
    ofile << "}\n";
    ofile << "\n";

    ofile << "static int\n";
    ofile << "env_cmd(struct bu_vls *s_out, int argc, const char **argv)\n";
    ofile << "{\n";
    ofile << "    int print_help = 0;\n";
    ofile << "    int list_all = 0;\n";
    ofile << "    int include_system = 0;\n";
    ofile << "    int include_context = 0;\n";
    ofile << "\n";
    ofile << "    static const char *usage1 = \"Usage: env [-hA] [pattern]\\n\";\n";
    ofile << "    static const char *usage2 = \" env get var_name\\n\";\n";
    ofile << "    static const char *usage3 = \" env set var_name var_val\\n\";\n";
    ofile << "\n";
    ofile << "    struct bu_opt_desc d[5];\n";
    ofile << "    BU_OPT(d[0], \"h\", \"help\", \"\", NULL, &print_help, \"Print help and exit\");\n";
    ofile << "    BU_OPT(d[1], \"A\", \"all\",  \"\", NULL, &list_all,   \"List all relevant variables, not just those currently set\");\n";
    ofile << "    BU_OPT(d[2], \"S\", \"system\",  \"\", NULL, &include_system,   \"Include known variables from src/other code, not just BRL-CAD's own variables\");\n";
    ofile << "    BU_OPT(d[3], \"C\", \"context\",  \"\", NULL, &include_context,   \"When listing variables, identify the library/executable in which they are used\");\n";
    ofile << "    BU_OPT_NULL(d[4]);\n";
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
    ofile << "	list_env(s_out, \"*\", list_all, include_system, include_context);\n";
    ofile << "    } else {\n";
    ofile << "	int i = 0;\n";
    ofile << "	for (i = 0; i < argc; i++) {\n";
    ofile << "	    list_env(s_out, argv[i], list_all, include_system, include_context);\n";
    ofile << "	}\n";
    ofile << "    }\n";
    ofile << "    return 0;\n";
    ofile << "}\n";

#ifdef TEST_MAIN
    ofile << "int main(int argc, const char **argv) {\n";
    ofile << "  int ret = 0;\n";
    ofile << "  struct bu_vls s_out = BU_VLS_INIT_ZERO;\n";
    ofile << "  ret = env_cmd(&s_out, argc, argv);\n";
    ofile << "  printf(\"%s\\n\", bu_vls_cstr(&s_out));\n";
    ofile << "  return ret;\n";
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

