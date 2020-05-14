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
#include <thread>
#define MAX_LINES_CHECK 500
#define EXPECTED_PLATFORM_SYMBOLS 206

class repo_state_t {
    public:

	// Thread state and result info
	std::string cfile;
	int ret = 0;
	int inc_cnt = 0;
	int src_cnt = 0;
	int bld_cnt = 0;

	// Initialize the regex info once per thread
	repo_state_t() {
	    int cnt = 0;
	    const char *rf;
	    inc_regex = std::regex("#[[:space:]]*include.*");

	    /* bio.h regex */
	    {
		bio_regex = std::regex("#[[:space:]]*include[[:space:]]*\"bio.h\".*");
		const char *bio_redundant_filter_strs[] {
		    "stdio.h",
		    "windows.h",
		    "io.h",
		    "unistd.h",
		    "fcntl.h",
		    NULL
		};
		cnt = 0;
		rf = bio_redundant_filter_strs[cnt];
		while (rf) {
		    std::string rrf = std::string(".*<") + std::string(rf) + std::string(">.*");
		    bio_redundant_filters[std::string(rf)] = std::regex(rrf);
		    cnt++;
		    rf = bio_redundant_filter_strs[cnt];
		}
	    }


	    /* bnetwork.h regex */
	    {
		bnetwork_regex = std::regex("#[[:space:]]*include[[:space:]]*\"bnetwork.h\".*");
		const char *bnetwork_redundant_filter_strs[] {
		    "winsock2.h",
		    "netinet/in.h",
		    "netinet/tcp.h",
		    "arpa/inet.h",
		    NULL
		};
		cnt = 0;
		rf = bnetwork_redundant_filter_strs[cnt];
		while (rf) {
		    std::string rrf = std::string(".*<") + std::string(rf) + std::string(">.*");
		    bnetwork_redundant_filters[std::string(rf)] = std::regex(rrf);
		    cnt++;
		    rf = bnetwork_redundant_filter_strs[cnt];
		}
	    }

	    /* common.h regex */
	    {
		common_regex = std::regex("#[[:space:]]*include[[:space:]]*\"common.h\".*");
		const char *common_exempt_filter_strs[] {
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
		cnt = 0;
		rf = common_exempt_filter_strs[cnt];
		while (rf) {
		    common_exempt_filters.push_back(std::regex(rf));
		    cnt++;
		    rf = common_exempt_filter_strs[cnt];
		}

	    }

	    /* API usage check regex */
	    {
		const char *api_file_exemption_strs[] {
		    ".*/CONFIG_CONTROL_DESIGN.*",
		    ".*/bu/log[.]h$",
		    ".*/bu/path[.]h$",
		    ".*/bu/str[.]h$",
		    ".*/cursor[.]c$",
		    ".*/ttcp[.]c$",
		    ".*/misc/CMake/compat/.*",
		    NULL
		};
		cnt = 0;
		rf = api_file_exemption_strs[cnt];
		while (rf) {
		    api_file_filters.push_back(std::regex(rf));
		    cnt++;
		    rf = api_file_exemption_strs[cnt];
		}

		const char *api_func_strs[] {
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
		cnt = 0;
		rf = api_func_strs[cnt];
		while (rf) {
		    std::string rrf = std::string(".*[^a-zA-Z0-9_:]") + std::string(rf) + std::string("[(].*");
		    api_func_filters[std::string(rf)] = std::regex(rrf);
		    cnt++;
		    rf = api_func_strs[cnt];
		}

		api_exemptions[std::string("abort")].push_back(std::regex(".*/bomb[.]c$"));
		api_exemptions[std::string("dirname")].push_back(std::regex(".*/tests/dirname[.]c$"));
		api_exemptions[std::string("remove")].push_back(std::regex(".*/file[.]c$"));
		api_exemptions[std::string("strcasecmp")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strcmp")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strdup")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strlcat")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strlcpy")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strncasecmp")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strncat")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strncmp")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strncpy")].push_back(std::regex(".*/rt/db4[.]h$"));
		api_exemptions[std::string("strncpy")].push_back(std::regex(".*/str[.]c$"));
		api_exemptions[std::string("strncpy")].push_back(std::regex(".*/vls[.]c$"));
		api_exemptions[std::string("strncpy")].push_back(std::regex(".*/wfobj/obj_util[.]cpp$"));
	    }

	    /* Platform symbol usage check regex */
	    {
		const char *platform_strs[] {
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
		cnt = 0;
		rf = platform_strs[cnt];
		while (rf) {
		    std::string p_upper(rf);
		    std::string p_lower = p_upper;
		    std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });
		    std::string rrf = std::string("^[[:space:]#]*(if|IF).*[[:space:](]_*(") + p_lower + std::string("|") + p_upper + std::string(")_*([[:space:]]|[)]|$).*$");
		    platform_checks[std::string(rf)] = std::regex(rrf);
		    cnt++;
		    rf = platform_strs[cnt];
		}

		const char *platform_exemption_strs[] {
		    ".*/pstdint[.]h$",
		    ".*/pinttypes[.]h$",
		    ".*/uce-dirent[.]h$",
		    NULL
		};
		cnt = 0;
		rf = platform_exemption_strs[cnt];
		while (rf) {
		    platform_file_filters.push_back(std::regex(rf));
		    cnt++;
		    rf = platform_exemption_strs[cnt];
		}
	    }
	}

	// Root of input source dir - used to trim the source dir prefix off of file paths
	std::string path_root;

	// Output logs
	std::vector<std::string> api_log;
	std::vector<std::string> bio_log;
	std::vector<std::string> bnet_log;
	std::vector<std::string> common_log;
	std::vector<std::string> symbol_inc_log;
	std::vector<std::string> symbol_src_log;
	std::vector<std::string> symbol_bld_log;

	/* Standard regex patterns uses in searches */

	std::regex inc_regex;

	/* bio.h */
	std::regex bio_regex;
	std::map<std::string, std::regex> bio_redundant_filters;

	/* bnetwork.h */
	std::regex bnetwork_regex;
	std::map<std::string, std::regex> bnetwork_redundant_filters;

	/* common.h */
	std::regex common_regex;
	std::vector<std::regex> common_exempt_filters;

	/* api usage */
	std::vector<std::regex> api_file_filters;
	std::map<std::string, std::vector<std::regex>> api_exemptions;
	std::map<std::string, std::regex> api_func_filters;

	/* platform symbols */
	std::map<std::string, std::regex> platform_checks;
	std::vector<std::regex> platform_file_filters;

};

int
bio_redundant_check(repo_state_t &l, std::string &tfile, std::ifstream &fs)
{
    bool have_bio = false;
    int lcnt = 0;
    int ret = 0;
    std::map<std::string, std::set<int>> match_line_nums;
    std::string sline;

    while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	lcnt++;
	if (std::regex_match(sline, l.bio_regex)) {
	    have_bio = true;
	    continue;
	}

	std::map<std::string, std::regex>::iterator f_it;
	for (f_it = l.bio_redundant_filters.begin(); f_it != l.bio_redundant_filters.end(); f_it++) {
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
		ret = 1;
		for (l_it = m_it->second.begin(); l_it != m_it->second.end(); l_it++) {
		    std::string lstr = tfile.substr(l.path_root.length()+1) + std::string(" uses bio.h, but includes header ") + m_it->first + std::string(" on line ") + std::to_string(*l_it) + std::string("\n");
		    l.bio_log.push_back(lstr);
		}
	    }
	}
    }

    return ret;
}

int
bnetwork_redundant_check(repo_state_t &l, std::string &tfile, std::ifstream &fs)
{
    bool have_bnetwork = false;
    int lcnt = 0;
    int ret = 0;
    std::map<std::string, std::set<int>> match_line_nums;
    std::string sline;

    while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	lcnt++;
	if (std::regex_match(sline, l.bnetwork_regex)) {
	    have_bnetwork = true;
	    continue;
	}

	std::map<std::string, std::regex>::iterator f_it;
	for (f_it = l.bnetwork_redundant_filters.begin(); f_it != l.bnetwork_redundant_filters.end(); f_it++) {
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
		ret = 1;
		for (l_it = m_it->second.begin(); l_it != m_it->second.end(); l_it++) {
		    std::string lstr = tfile.substr(l.path_root.length()+1) + std::string(" uses bnetwork.h, but also includes header ") + m_it->first + std::string(" on line ") + std::to_string(*l_it) + std::string("\n");
		    l.bnet_log.push_back(lstr);
		}
	    }
	}
    }

    return ret;
}



int
common_include_first(repo_state_t &l, std::string &tfile, std::ifstream &fs)
{
    int ret = 0;

    bool skip = false;
    for (size_t j = 0; j < l.common_exempt_filters.size(); j++) {
	if (std::regex_match(tfile, l.common_exempt_filters[j])) {
	    skip = true;
	    break;
	}
    }
    if (skip) {
	return ret;
    }

    int lcnt = 0;
    int first_inc_line = -1;
    bool have_inc = false;
    std::string sline;
    while (std::getline(fs, sline) && lcnt < MAX_LINES_CHECK) {
	lcnt++;
	if (std::regex_match(sline, l.common_regex)) {
	    if (have_inc) {
		std::string lstr = tfile.substr(l.path_root.length()+1) + std::string(" includes common.h on line ") + std::to_string(lcnt) + std::string(" but a prior #include statement was found at line ") + std::to_string(first_inc_line) + std::string("\n");
		l.common_log.push_back(lstr);
		ret = 1;
	    }
	    break;
	}
	if (!have_inc && std::regex_match(sline, l.inc_regex)) {
	    have_inc = true;
	    first_inc_line = lcnt;
	}
    }

    return ret;
}

int
api_usage(repo_state_t &l, std::string &tfile, std::ifstream &fs)
{
    int ret = 0;

    bool skip = false;
    for (size_t j = 0; j < l.api_file_filters.size(); j++) {
	if (std::regex_match(tfile, l.api_file_filters[j])) {
	    skip = true;
	    break;
	}
    }
    if (skip) {
	return ret;
    }

    std::map<std::string, std::set<int>> instances;

    int lcnt = 0;
    std::string sline;
    while (std::getline(fs, sline)) {
	lcnt++;
	std::map<std::string, std::regex>::iterator ff_it;
	for (ff_it = l.api_func_filters.begin(); ff_it != l.api_func_filters.end(); ff_it++) {
	    if (std::regex_match(sline, ff_it->second)) {
		// If we have a it, make sure it's not an exemption
		bool exempt = false;
		if (l.api_exemptions.find(ff_it->first) != l.api_exemptions.end()) {
		    std::vector<std::regex>::iterator e_it;
		    for (e_it = l.api_exemptions[ff_it->first].begin(); e_it != l.api_exemptions[ff_it->first].end(); e_it++) {
			if (std::regex_match(tfile, *e_it)) {
			    exempt = true;
			    break;
			}
		    }
		}
		if (!exempt) {
		    instances[ff_it->first].insert(lcnt);
		    ret = 1;
		}
	    }
	}
    }

    std::map<std::string, std::set<int>>::iterator i_it;

    for (i_it = instances.begin(); i_it != instances.end(); i_it++) {
	std::set<int>::iterator num_it;
	for (num_it = i_it->second.begin(); num_it != i_it->second.end(); num_it++) {
	    std::string lstr = tfile.substr(l.path_root.length()+1) + std::string(" matches ") + i_it->first + std::string(" on line ") + std::to_string(*num_it) + std::string("\n");
	    l.api_log.push_back(lstr);
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
platform_symbols(repo_state_t &l, std::vector<std::string> &log, std::string &tfile, std::ifstream &fs)
{
    std::map<std::string, std::vector<platform_entry>> instances;
    bool skip = false;
    for (size_t j = 0; j < l.platform_file_filters.size(); j++) {
	if (std::regex_match(tfile, l.platform_file_filters[j])) {
	    skip = true;
	    break;
	}
    }
    if (skip) {
	return 0;
    }

    int lcnt = 0;
    std::string sline;
    while (std::getline(fs, sline)) {
	lcnt++;

	std::map<std::string, std::regex>::iterator  p_it;
	for (p_it = l.platform_checks.begin(); p_it != l.platform_checks.end(); p_it++) {
	    if (std::regex_match(sline, p_it->second)) {
		//std::cout << "match on line: " << sline << "\n";
		platform_entry pe;
		pe.symbol = p_it->first;
		pe.file = tfile.substr(l.path_root.length()+1);
		pe.line_num = lcnt;
		pe.line = sline;
		instances[p_it->first].push_back(pe);
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


void
process_inc_file(repo_state_t &r_t)
{
    std::ifstream fs;
    fs.open(r_t.cfile);
    if (!fs.is_open()) {
	return;
    }
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.ret += bio_redundant_check(r_t, r_t.cfile, fs);
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.ret += bnetwork_redundant_check(r_t, r_t.cfile, fs);
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.inc_cnt += platform_symbols(r_t, r_t.symbol_inc_log, r_t.cfile, fs);
    fs.close();
}

void
process_src_file(repo_state_t &r_t)
{
    std::ifstream fs;
    fs.open(r_t.cfile);
    if (!fs.is_open()) {
	return;
    }
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.ret += bio_redundant_check(r_t, r_t.cfile, fs);
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.ret += bnetwork_redundant_check(r_t, r_t.cfile, fs);
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.ret += common_include_first(r_t, r_t.cfile, fs);
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.ret += api_usage(r_t, r_t.cfile, fs);
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.src_cnt += platform_symbols(r_t, r_t.symbol_src_log, r_t.cfile, fs);
    fs.close();
}

void
process_bld_file(repo_state_t &r_t)
{
    std::ifstream fs;
    fs.open(r_t.cfile);
    if (!fs.is_open()) {
	return;
    }
    fs.clear();
    fs.seekg(0, std::ios::beg);
    r_t.bld_cnt += platform_symbols(r_t, r_t.symbol_bld_log, r_t.cfile, fs);
    fs.close();
}

int
main(int argc, const char *argv[])
{
    if (argc != 3) {
	std::cerr << "Usage: repocheck file_list.txt source_dir\n";
	return -1;
    }

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
    std::vector<std::string> bld_files;

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
	    bld_files.push_back(sfile);
	    continue;
	}
    }

    int ret = 0;


    // Regex testing is CPU intensive - go parallel for main checks
    size_t fcnt = 0;
    unsigned int hwc = std::thread::hardware_concurrency();
    if (!hwc) {
	hwc = 10;
    }
    std::vector<repo_state_t> repo_states;
    for (unsigned int i = 0; i < hwc; i++) {
	repo_state_t repo_state;
	repo_state.path_root = std::string(argv[2]);
	repo_states.push_back(repo_state);
    }

    fcnt = 0;
    while (fcnt < inc_files.size()) {
        int athreads = 0;
        std::vector<std::thread> t;
        for (unsigned int i = 0; i < hwc; i++) {
            if (fcnt + i < inc_files.size()) {
                repo_states[i].cfile = inc_files[fcnt+i];
                t.push_back(std::thread(process_inc_file, std::ref(repo_states[i])));
                athreads++;
            }
        }
        for (int i = 0; i < athreads; i++) {
            t[i].join();
        }
        fcnt += athreads;
    }

    fcnt = 0;
    while (fcnt < src_files.size()) {
        int athreads = 0;
        std::vector<std::thread> t;
        for (unsigned int i = 0; i < hwc; i++) {
            if (fcnt + i < src_files.size()) {
                repo_states[i].cfile = src_files[fcnt+i];
                t.push_back(std::thread(process_src_file, std::ref(repo_states[i])));
                athreads++;
            }
        }
        for (int i = 0; i < athreads; i++) {
            t[i].join();
        }
        fcnt += athreads;
    }

    fcnt = 0;
    while (fcnt < bld_files.size()) {
        int athreads = 0;
        std::vector<std::thread> t;
        for (unsigned int i = 0; i < hwc; i++) {
            if (fcnt + i < bld_files.size()) {
                repo_states[i].cfile = bld_files[fcnt+i];
                t.push_back(std::thread(process_bld_file, std::ref(repo_states[i])));
                athreads++;
            }
        }
        for (int i = 0; i < athreads; i++) {
            t[i].join();
        }
        fcnt += athreads;
    }

    repo_state_t repo_state;

    // Consolidate
    for (unsigned int i = 0; i < hwc; i++) {
	repo_state_t &rs = repo_states[i];
	repo_state.ret += rs.ret;
	repo_state.inc_cnt += rs.inc_cnt;
	repo_state.src_cnt += rs.src_cnt;
	repo_state.bld_cnt += rs.bld_cnt;
    
	repo_state.api_log.insert(repo_state.api_log.end(), rs.api_log.begin(), rs.api_log.end());
	repo_state.bio_log.insert(repo_state.bio_log.end(), rs.bio_log.begin(), rs.bio_log.end());
	repo_state.bnet_log.insert(repo_state.bnet_log.end(), rs.bnet_log.begin(), rs.bnet_log.end());
	repo_state.common_log.insert(repo_state.common_log.end(), rs.common_log.begin(), rs.common_log.end());
	repo_state.symbol_inc_log.insert(repo_state.symbol_inc_log.end(), rs.symbol_inc_log.begin(), rs.symbol_inc_log.end());
	repo_state.symbol_src_log.insert(repo_state.symbol_src_log.end(), rs.symbol_src_log.begin(), rs.symbol_src_log.end());
	repo_state.symbol_bld_log.insert(repo_state.symbol_bld_log.end(), rs.symbol_bld_log.begin(), rs.symbol_bld_log.end());
    }

    int psym_cnt = repo_state.inc_cnt + repo_state.src_cnt + repo_state.bld_cnt;
    int expected_psym_cnt = EXPECTED_PLATFORM_SYMBOLS ;
    if (psym_cnt > expected_psym_cnt) {
	ret += 1;
    }

    if (ret) {
	std::sort(repo_state.api_log.begin(), repo_state.api_log.end());
	std::sort(repo_state.bio_log.begin(), repo_state.bio_log.end());
	std::sort(repo_state.bnet_log.begin(), repo_state.bnet_log.end());
	std::sort(repo_state.common_log.begin(), repo_state.common_log.end());
	std::sort(repo_state.symbol_inc_log.begin(), repo_state.symbol_inc_log.end());
	std::sort(repo_state.symbol_src_log.begin(), repo_state.symbol_src_log.end());
	std::sort(repo_state.symbol_bld_log.begin(), repo_state.symbol_bld_log.end());

	if (repo_state.api_log.size()) {
	    std::cout << "\nFound " << repo_state.api_log.size() << " instances of unguarded API usage:\n";
	    for (size_t i = 0; i < repo_state.api_log.size(); i++) {
		std::cout << repo_state.api_log[i];
	    }
	}
	if (repo_state.bio_log.size()) {
	    std::cout << "\nFound " << repo_state.bio_log.size() << " instances of redundant header inclusions in files using bio.h:\n";
	    for (size_t i = 0; i < repo_state.bio_log.size(); i++) {
		std::cout << repo_state.bio_log[i];
	    }
	}
	if (repo_state.bnet_log.size()) {
	    std::cout << "\nFound " << repo_state.bnet_log.size() << " instances of redundant header inclusions in files using bnetwork.h:\n";
	    for (size_t i = 0; i < repo_state.bnet_log.size(); i++) {
		std::cout << repo_state.bnet_log[i];
	    }
	}
	if (repo_state.common_log.size()) {
	    std::cout << "\nFound " << repo_state.common_log.size() << " instances of files using common.h with out-of-order inclusions:\n";
	    for (size_t i = 0; i < repo_state.common_log.size(); i++) {
		std::cout << repo_state.common_log[i];
	    }
	}

	if (psym_cnt > expected_psym_cnt) {
	    std::cout << "\n\nFAILURE: expected " << expected_psym_cnt << " platform symbols, found " << psym_cnt << ":\n";
	}

	if (repo_state.symbol_inc_log.size()) {
	    std::cout << "\nFound " << repo_state.symbol_inc_log.size() << " instances of platform symbol usage in header files:\n";
	    for (size_t i = 0; i < repo_state.symbol_inc_log.size(); i++) {
		std::cout << repo_state.symbol_inc_log[i];
	    }
	}
	if (repo_state.symbol_src_log.size()) {
	    std::cout << "\nFound " << repo_state.symbol_src_log.size() << " instances of platform symbol usage in source files:\n";
	    for (size_t i = 0; i < repo_state.symbol_src_log.size(); i++) {
		std::cout << repo_state.symbol_src_log[i];
	    }
	}
	if (repo_state.symbol_bld_log.size()) {
	    std::cout << "\nFound " << repo_state.symbol_bld_log.size() << " instances of platform symbol usage in build files:\n";
	    for (size_t i = 0; i < repo_state.symbol_bld_log.size(); i++) {
		std::cout << repo_state.symbol_bld_log[i];
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
