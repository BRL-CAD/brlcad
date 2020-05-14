/*                   R E P O C H E C K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file repocheck.cpp
 *
 * Basic checks of the repository sources to make sure maintenance
 * burden code clean-up problems don't creep in.
 *
 */

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
#define MAX_LINES_CHECK 500
#define EXPECTED_PLATFORM_SYMBOLS 206

class repo_log_t {
    public:
	std::string path_root;

	std::vector<std::string> api_log;
	std::vector<std::string> bio_log;
	std::vector<std::string> bnet_log;
	std::vector<std::string> common_log;
	std::vector<std::string> symbol_inc_log;
	std::vector<std::string> symbol_src_log;
	std::vector<std::string> symbol_bld_log;
};

bool
bio_redundant_check(repo_log_t &l, std::vector<std::string> &srcs)
{
    bool ret = false;
    std::regex bio_regex("#[[:space:]]*include[[:space:]]*\"bio.h\".*");
    std::regex inc_regex("#[[:space:]]*include.*");
    const char *redundant_filters[] {
	".*<stdio.h>.*",
	".*<windows.h>.*",
	".*<io.h>.*",
	".*<unistd.h>.*",
	".*<fcntl.h>.*",
	NULL
    };

    std::map<std::string, std::regex> filters;
    int cnt = 0;
    const char *rf = redundant_filters[cnt];
    while (rf) {
	filters[std::string(rf)] = std::regex(rf);
	cnt++;
	rf = redundant_filters[cnt];
    }


    for (size_t i = 0; i < srcs.size(); i++) {
	std::string sline;

	std::map<std::string, std::set<int>> match_line_nums;

	std::ifstream fs;
	fs.open(srcs[i]);
	if (!fs.is_open()) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}

	int lcnt = 0;
	bool have_bio = false;
	while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	    lcnt++;
	    if (std::regex_match(sline, bio_regex)) {
		have_bio = true;
		continue;
	    }

	    std::map<std::string, std::regex>::iterator f_it;
	    for (f_it = filters.begin(); f_it != filters.end(); f_it++) {
		if (std::regex_match(sline, f_it->second)) {
		    match_line_nums[f_it->first].insert(lcnt);
		    continue;
		}
	    }
	}

	if (have_bio) {
	    std::map<std::string, std::set<int>>::iterator m_it;
	    for (m_it = match_line_nums.begin(); m_it != match_line_nums.end(); m_it++) {
		if (m_it->second.size()) {
		    std::set<int>::iterator l_it;
		    ret = true;
		    for (l_it = m_it->second.begin(); l_it != m_it->second.end(); l_it++) {
			std::string lstr = srcs[i].substr(l.path_root.length()+1) + std::string(" has bio.h, but also matches regex ") + m_it->first + std::string(" on line ") + std::to_string(lcnt) + std::string("\n");
			l.bio_log.push_back(lstr);
		    }
		}
	    }
	}
    }

    return ret;
}

bool
bnetwork_redundant_check(repo_log_t &l, std::vector<std::string> &srcs)
{
    bool ret = false;
    std::regex bnetwork_regex("#[[:space:]]*include[[:space:]]*\"bnetwork.h\".*");
    std::regex inc_regex("#[[:space:]]*include.*");
    const char *redundant_filters[] {
	".*<winsock2.h>.*",
	".*<netinet/in.h>.*",
	".*<netinet/tcp.h>.*",
	".*<arpa/inet.h>.*",
	NULL
    };

    std::map<std::string, std::regex> filters;
    int cnt = 0;
    const char *rf = redundant_filters[cnt];
    while (rf) {
	filters[std::string(rf)] = std::regex(rf);
	cnt++;
	rf = redundant_filters[cnt];
    }


    for (size_t i = 0; i < srcs.size(); i++) {
	std::string sline;

	std::map<std::string, std::set<int>> match_line_nums;

	std::ifstream fs;
	fs.open(srcs[i]);
	if (!fs.is_open()) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}

	int lcnt = 0;
	bool have_bnetwork = false;
	while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	    lcnt++;
	    if (std::regex_match(sline, bnetwork_regex)) {
		have_bnetwork = true;
		continue;
	    }

	    std::map<std::string, std::regex>::iterator f_it;
	    for (f_it = filters.begin(); f_it != filters.end(); f_it++) {
		if (std::regex_match(sline, f_it->second)) {
		    match_line_nums[f_it->first].insert(lcnt);
		    continue;
		}
	    }
	}

	if (have_bnetwork) {
	    std::map<std::string, std::set<int>>::iterator m_it;
	    for (m_it = match_line_nums.begin(); m_it != match_line_nums.end(); m_it++) {
		if (m_it->second.size()) {
		    std::set<int>::iterator l_it;
		    ret = true;
		    for (l_it = m_it->second.begin(); l_it != m_it->second.end(); l_it++) {
			std::string lstr = srcs[i].substr(l.path_root.length()+1) + std::string(" has bnetwork.h, but also matches regex ") + m_it->first + std::string(" on line ") + std::to_string(lcnt) + std::string("\n");
			l.bnet_log.push_back(lstr);
		    }
		}
	    }
	}
    }

    return ret;
}



bool
common_include_first(repo_log_t &l, std::vector<std::string> &srcs)
{
    bool ret = false;
    std::regex inc_regex("#[[:space:]]*include.*");
    std::regex common_regex("#[[:space:]]*include[[:space:]]*\"common.h\".*");
    const char *exempt_filters[] {
	".*/bio.h",
	".*/bnetwork.h",
	".*/config_win.h",
	".*/csg_parser.c",
	".*/csg_scanner.h",
	".*/obj_grammar.c",
	".*/obj_grammar.cpp",
	".*/obj_libgcv_grammar.cpp",
	".*/obj_obj-g_grammar.cpp",
	".*/obj_parser.h",
	".*/obj_rules.cpp",
	".*/obj_rules.l",
	".*/obj_scanner.h",
	".*/obj_util.h",
	".*/optionparser.h",
	".*/pinttypes.h",
	".*/points_scan.c",
	".*/pstdint.h",
	".*/schema.h",
	".*/script.c",
	".*/ttcp.c",
	".*/uce-dirent.h",
	NULL
    };

    std::vector<std::regex> filters;
    int cnt = 0;
    const char *rf = exempt_filters[cnt];
    while (rf) {
	filters.push_back(std::regex(rf));
	cnt++;
	rf = exempt_filters[cnt];
    }

    for (size_t i = 0; i < srcs.size(); i++) {
	bool skip = false;
	for (size_t j = 0; j < filters.size(); j++) {
	    if (std::regex_match(srcs[i], filters[j])) {
		skip = true;
		break;
	    }
	}
	if (skip) {
	    continue;
	}

	std::ifstream fs;
	fs.open(srcs[i]);
	if (!fs.is_open()) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}

	int lcnt = 0;
	int first_inc_line = -1;
	bool have_inc = false;
	std::string sline;
	while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	    lcnt++;
	    if (std::regex_match(sline, common_regex)) {
		if (have_inc) {
		    std::string lstr = srcs[i].substr(l.path_root.length()+1) + std::string(" includes common.h on line ") + std::to_string(lcnt) + std::string(" but a prior #include statement was found at line ") + std::to_string(first_inc_line) + std::string("\n");
		    l.common_log.push_back(lstr);
		    ret = true;
		}
		break;
	    }
	    if (!have_inc && std::regex_match(sline, inc_regex)) {
		have_inc = true;
		first_inc_line = lcnt;
	    }
	}
    }


    return ret;
}

bool
api_usage(repo_log_t &l, std::vector<std::string> &srcs)
{
    bool ret = false;
    std::map<std::string, std::vector<std::regex>> exemptions;
    exemptions[std::string("abort")].push_back(std::regex(".*/bomb[.]c$"));
    exemptions[std::string("dirname")].push_back(std::regex(".*/tests/dirname[.]c$"));
    exemptions[std::string("remove")].push_back(std::regex(".*/file[.]c$"));
    exemptions[std::string("strcasecmp")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strcmp")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strdup")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strlcat")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strlcpy")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strncasecmp")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strncat")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strncmp")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strncpy")].push_back(std::regex(".*/rt/db4[.]h$"));
    exemptions[std::string("strncpy")].push_back(std::regex(".*/str[.]c$"));
    exemptions[std::string("strncpy")].push_back(std::regex(".*/vls[.]c$"));
    exemptions[std::string("strncpy")].push_back(std::regex(".*/wfobj/obj_util[.]cpp$"));

    const char *file_exemptions[] {
	".*/CONFIG_CONTROL_DESIGN.*",
	".*/bu/log[.]h$",
	".*/bu/path[.]h$",
	".*/bu/str[.]h$",
	".*/cursor[.]c$",
	".*/ttcp[.]c$",
	".*/misc/CMake/compat/.*",
	NULL
    };

    const char *funcs[] {
	"abort",
	"dirname",
	"fgets",
	"getopt",
	"qsort",
	"remove",
	"rmdir",
	"strcasecmp",
	"strcat",
	"strcmp",
	"strcpy",
	"strdup",
	"stricmp",
	"strlcat",
	"strlcpy",
	"strncasecmp",
	"strncat",
	"strncmp",
	"strncpy",
	"unlink",
	NULL
    };

    std::vector<std::regex> file_filters;
    int cnt = 0;
    const char *rf = file_exemptions[cnt];
    while (rf) {
	file_filters.push_back(std::regex(rf));
	cnt++;
	rf = file_exemptions[cnt];
    }

    std::map<std::string, std::regex> func_filters;
    cnt = 0;
    rf = funcs[cnt];
    while (rf) {
	std::string rrf = std::string(".*[^a-zA-Z0-9_:]") + std::string(rf) + std::string("[(].*");
	func_filters[std::string(rf)] = std::regex(rrf);
	cnt++;
	rf = funcs[cnt];
    }


    for (size_t i = 0; i < srcs.size(); i++) {
	bool skip = false;
	for (size_t j = 0; j < file_filters.size(); j++) {
	    if (std::regex_match(srcs[i], file_filters[j])) {
		skip = true;
		break;
	    }
	}
	if (skip) {
	    continue;
	}

	std::ifstream fs;
	fs.open(srcs[i]);
	if (!fs.is_open()) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}

	std::map<std::string, std::set<int>> instances;

	int lcnt = 0;
	std::string sline;
	while (std::getline(fs, sline)) {
	    lcnt++;
	    std::map<std::string, std::regex>::iterator ff_it;
	    for (ff_it = func_filters.begin(); ff_it != func_filters.end(); ff_it++) {
		if (!std::strstr(sline.c_str(), ff_it->first.c_str())) {
		    // Only try the full regex if strstr says there is a chance
		    continue;
		}
		if (std::regex_match(sline, ff_it->second)) {
		    // If we have a it, make sure it's not an exemption
		    bool exempt = false;
		    if (exemptions.find(ff_it->first) != exemptions.end()) {
			std::vector<std::regex>::iterator e_it;
			for (e_it = exemptions[ff_it->first].begin(); e_it != exemptions[ff_it->first].end(); e_it++) {
			    if (std::regex_match(srcs[i], *e_it)) {
				exempt = true;
				break;
			    }
			}
		    }
		    if (!exempt) {
			instances[ff_it->first].insert(lcnt);
			ret = true;
		    }
		}
	    }
	}

	std::map<std::string, std::set<int>>::iterator i_it;

	for (i_it = instances.begin(); i_it != instances.end(); i_it++) {
	    std::set<int>::iterator num_it;
	    for (num_it = i_it->second.begin(); num_it != i_it->second.end(); num_it++) {
	    std::string lstr = srcs[i].substr(l.path_root.length()+1) + std::string(" matches ") + i_it->first + std::string(" on line ") + std::to_string(*num_it) + std::string("\n");
	    l.api_log.push_back(lstr);
	    }
	}
    }

    return ret;
}

class platform_entry {
    public:
	std::string symbol;
	std::string file;
	int line_num;
	std::string line;
};


int
platform_symbols(repo_log_t &l, std::vector<std::string> &log, std::vector<std::string> &srcs)
{

    const char *platforms[] {
	"AIX",
	"APPLE",
	"CYGWIN",
	"DARWIN",
	"FREEBSD",
	"HAIKU",
	"HPUX",
	"LINUX",
	"MINGW",
	"MSDOS",
	"QNX",
	"SGI",
	"SOLARIS",
	"SUN",
	"SUNOS",
	"SVR4",
	"SYSV",
	"ULTRIX",
	"UNIX",
	"VMS",
	"WIN16",
	"WIN32",
	"WIN64",
	"WINE",
	"WINNT",
	NULL
    };
    std::map<std::pair<std::string, std::string>, std::regex> platform_checks;
    int cnt = 0;
    const char *rf = platforms[cnt];
    while (rf) {
	cnt++;
	std::string p_upper(rf);
	std::string p_lower = p_upper;
	std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });
	std::string pregex_str = std::string("^[[:space:]#]*(if|IF).*[[:space:](]_*(") + p_lower + std::string("|") + p_upper + std::string(")_*([[:space:]]|[)]|$).*$");
	platform_checks[std::make_pair(p_lower, p_upper)] = std::regex(pregex_str);
	rf = platforms[cnt];
    }

    const char *file_exemptions[] {
	".*/pstdint[.]h$",
	".*/pinttypes[.]h$",
	".*/uce-dirent[.]h$",
	NULL
    };

    std::vector<std::regex> file_filters;
    cnt = 0;
    rf = file_exemptions[cnt];
    while (rf) {
	file_filters.push_back(std::regex(rf));
	cnt++;
	rf = file_exemptions[cnt];
    }


    std::map<std::string, std::vector<platform_entry>> instances;
    for (size_t i = 0; i < srcs.size(); i++) {
	bool skip = false;
	for (size_t j = 0; j < file_filters.size(); j++) {
	    if (std::regex_match(srcs[i], file_filters[j])) {
		skip = true;
		break;
	    }
	}
	if (skip) {
	    continue;
	}

	std::ifstream fs;
	fs.open(srcs[i]);
	if (!fs.is_open()) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}

	//std::cout << "Reading " << srcs[i] << "\n";

	int lcnt = 0;
	std::string sline;
	while (std::getline(fs, sline)) {
	    lcnt++;

	    std::map<std::pair<std::string, std::string>, std::regex>::iterator  p_it;
	    for (p_it = platform_checks.begin(); p_it != platform_checks.end(); p_it++) {
		if (!std::strstr(sline.c_str(), p_it->first.first.c_str()) && !std::strstr(sline.c_str(), p_it->first.second.c_str())) {
		    // Only try the full regex if strstr says there is a chance
		    continue;
		}
		if (std::regex_match(sline, p_it->second)) {
		    //std::cout << "match on line: " << sline << "\n";
		    platform_entry pe;
		    pe.symbol = p_it->first.second;
		    pe.file = srcs[i].substr(l.path_root.length()+1);
		    pe.line_num = lcnt;
		    pe.line = sline;
		    instances[p_it->first.second].push_back(pe);
		}
	    }
	}
    }

    std::map<std::string, std::vector<platform_entry>>::iterator m_it;
    int match_cnt = 0;
    for (m_it = instances.begin(); m_it != instances.end(); m_it++) {
	for (size_t i = 0; i < m_it->second.size(); i++) {
	    platform_entry &pe = m_it->second[i];
	    std::string lstr = pe.file + std::string("(") + std::to_string(pe.line_num) + std::string("): ") + pe.line + std::string("\n");
	    log.push_back(lstr);
	    match_cnt++;
	}
    }

    return match_cnt;
}

int
main(int argc, const char *argv[])
{
    if (argc != 3) {
	std::cerr << "Usage: repocheck file_list.txt source_dir\n";
	return -1;
    }

    repo_log_t repo_log;
    repo_log.path_root = std::string(argv[2]);

    std::string sfile;
    std::ifstream src_file_stream;
    src_file_stream.open(argv[1]);
    if (!src_file_stream.is_open()) {
	std::cerr << "Unable to open file list file " << argv[1] << "\n";
	return -1;
    }


    // Build a set of filters that will cull out files which would otherwise
    // be matches for items of interest
    const char *reject_filters[] {
	".*/bullet/.*",
	".*/doc/.*",
	".*/shapelib/.*",
	".*[.]log",
	".*[.]svn.*",
	".*misc/CMake/Find.*",
	".*misc/repoconv.*",
	".*misc/tools.*",
	".*misc/debian.*",
	".*pkg.h",
	".*src/libpkg.*",
	".*src/other.*",
	".*~",
	NULL
    };

    std::vector<std::regex> filters;
    int cnt = 0;
    const char *rf = reject_filters[cnt];
    while (rf) {
	filters.push_back(std::regex(rf));
	cnt++;
	rf = reject_filters[cnt];
    }


    // Apply filters and build up the file sets we want to introspect.
    std::regex codefile_regex(".*[.](c|cpp|cxx|cc|h|hpp|hxx|y|yy|l)([.]in)?$");
    std::regex buildfile_regex(".*([.]cmake([.]in)?|CMakeLists.txt)$");
    std::regex hdrfile_regex(".*/include/.*");
    std::vector<std::string> src_files;
    std::vector<std::string> inc_files;
    std::vector<std::string> build_files;

    while (std::getline(src_file_stream, sfile)) {
	bool reject = false;
	for (size_t i = 0; i < filters.size(); i++) {
	    if (std::regex_match(sfile, filters[i])) {
		reject = true;
		break;
	    }
	}
	if (reject) {
	    continue;
	}

	if (std::regex_match(sfile, codefile_regex)) {
	    if (std::regex_match(sfile, hdrfile_regex)) {
		inc_files.push_back(sfile);
	    } else {
		src_files.push_back(sfile);
	    }
	    continue;
	}
	if (std::regex_match(sfile, buildfile_regex)) {
	    build_files.push_back(sfile);
	    continue;
	}
    }

    int ret = 0;

    if (bio_redundant_check(repo_log, src_files)) {
	ret = -1;
    }

    if (bnetwork_redundant_check(repo_log, src_files)) {
	ret = -1;
    }

    if (common_include_first(repo_log, src_files)) {
	ret = -1;
    }

    if (api_usage(repo_log, src_files)) {
	ret = -1;
    }

    int h_cnt = platform_symbols(repo_log, repo_log.symbol_inc_log, inc_files);
    int s_cnt = platform_symbols(repo_log, repo_log.symbol_src_log, src_files);
    int b_cnt = platform_symbols(repo_log, repo_log.symbol_bld_log, build_files);
    int psym_cnt = h_cnt + s_cnt + b_cnt;
    int expected_psym_cnt = EXPECTED_PLATFORM_SYMBOLS;
    if (psym_cnt > expected_psym_cnt) {
	std::cout << "FAILURE: expected " << expected_psym_cnt << " platform symbols, found " << psym_cnt << "\n";
	ret = -1;
    }

    if (ret == -1) {
	std::sort(repo_log.api_log.begin(), repo_log.api_log.end());
	std::sort(repo_log.bio_log.begin(), repo_log.bio_log.end());
	std::sort(repo_log.bnet_log.begin(), repo_log.bnet_log.end());
	std::sort(repo_log.common_log.begin(), repo_log.common_log.end());
	std::sort(repo_log.symbol_inc_log.begin(), repo_log.symbol_inc_log.end());
	std::sort(repo_log.symbol_src_log.begin(), repo_log.symbol_src_log.end());
	std::sort(repo_log.symbol_bld_log.begin(), repo_log.symbol_bld_log.end());

	if (repo_log.api_log.size()) {
	    std::cout << "\nFound " << repo_log.api_log.size() << " instances of unguarded API usage:\n";
	    for (size_t i = 0; i < repo_log.api_log.size(); i++) {
		std::cout << repo_log.api_log[i];
	    }
	}
	if (repo_log.bio_log.size()) {
	    std::cout << "\nFound " << repo_log.bio_log.size() << " instances of redundant header inclusions in files using bio.h:\n";
	    for (size_t i = 0; i < repo_log.bio_log.size(); i++) {
		std::cout << repo_log.bio_log[i];
	    }
	}
	if (repo_log.bnet_log.size()) {
	    std::cout << "\nFound " << repo_log.bnet_log.size() << " instances of redundant header inclusions in files using bnetwork.h:\n";
	    for (size_t i = 0; i < repo_log.bnet_log.size(); i++) {
		std::cout << repo_log.bnet_log[i];
	    }
	}
	if (repo_log.common_log.size()) {
	    std::cout << "\nFound " << repo_log.common_log.size() << " instances of files using common.h with out-of-order inclusions:\n";
	    for (size_t i = 0; i < repo_log.common_log.size(); i++) {
		std::cout << repo_log.common_log[i];
	    }
	}
	if (repo_log.symbol_inc_log.size()) {
	    std::cout << "\nFound " << repo_log.symbol_inc_log.size() << " instances of platform symbol usage in header files:\n";
	    for (size_t i = 0; i < repo_log.symbol_inc_log.size(); i++) {
		std::cout << repo_log.symbol_inc_log[i];
	    }
	}
	if (repo_log.symbol_src_log.size()) {
	    std::cout << "\nFound " << repo_log.symbol_src_log.size() << " instances of platform symbol usage in source files:\n";
	    for (size_t i = 0; i < repo_log.symbol_src_log.size(); i++) {
		std::cout << repo_log.symbol_src_log[i];
	    }
	}
	if (repo_log.symbol_bld_log.size()) {
	    std::cout << "\nFound " << repo_log.symbol_bld_log.size() << " instances of platform symbol usage in build files:\n";
	    for (size_t i = 0; i < repo_log.symbol_bld_log.size(); i++) {
		std::cout << repo_log.symbol_bld_log[i];
	    }
	}
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
