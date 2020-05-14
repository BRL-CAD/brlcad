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

#include "common.h"

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

#include "bu/mapped_file.h"

#define MAX_LINES_CHECK 500
#define EXPECTED_PLATFORM_SYMBOLS 205

class repo_info_t {
    public:

	/* Standard regex patterns uses in searches */

	std::regex inc_regex;

	/* bio.h */
	std::regex bio_regex;
	std::map<std::string, std::regex> bio_filters;

	/* bnetwork.h */
	std::regex bnetwork_regex;
	std::map<std::string, std::regex> bnetwork_filters;

	/* common.h */
	std::regex common_regex;
	std::vector<std::regex> common_exempt_filters;

	/* api usage */
	std::vector<std::regex> api_file_filters;
	std::map<std::string, std::vector<std::regex>> api_exemptions;
	std::map<std::string, std::regex> api_func_filters;

	/* platform symbols */
	std::map<std::pair<std::string, std::string>, std::regex> platform_checks;
	std::vector<std::regex> platform_file_filters;


	std::string path_root;

	// Outputs
	std::vector<std::string> api_log;
	std::vector<std::string> bio_log;
	std::vector<std::string> bnetwork_log;
	std::vector<std::string> common_log;
	std::vector<std::string> symbol_inc_log;
	std::vector<std::string> symbol_src_log;
	std::vector<std::string> symbol_bld_log;
};


void
regex_init(repo_info_t &r) {
    int cnt = 0;
    const char *rf;
    r.inc_regex = std::regex("#[[:space:]]*include.*");

    /* bio.h regex */
    {
	r.bio_regex = std::regex("#[[:space:]]*include[[:space:]]*\"bio.h\".*");
	const char *bio_filter_strs[] {
	    "stdio.h", "windows.h", "io.h", "unistd.h", "fcntl.h",
	       	NULL
	};
	cnt = 0;
	rf = bio_filter_strs[cnt];
	while (rf) {
	    std::string rrf = std::string(".*<") + std::string(rf) + std::string(">.*");
	    r.bio_filters[std::string(rf)] = std::regex(rrf);
	    cnt++;
	    rf = bio_filter_strs[cnt];
	}
    }

    /* bnetwork.h regex */
    {
	r.bnetwork_regex = std::regex("#[[:space:]]*include[[:space:]]*\"bnetwork.h\".*");
	const char *bnetwork_filter_strs[] {
	    "winsock2.h", "netinet/in.h", "netinet/tcp.h", "arpa/inet.h",
	       	NULL
	};
	cnt = 0;
	rf = bnetwork_filter_strs[cnt];
	while (rf) {
	    std::string rrf = std::string(".*<") + std::string(rf) + std::string(">.*");
	    r.bnetwork_filters[std::string(rf)] = std::regex(rrf);
	    cnt++;
	    rf = bnetwork_filter_strs[cnt];
	}
    }

    /* common.h regex */
    {
	r.common_regex = std::regex("#[[:space:]]*include[[:space:]]*\"common.h\".*");
	const char *common_exempt_filter_strs[] {
	    "bio.h", "bnetwork.h", "config_win.h", "csg_parser.c",
		"csg_scanner.h", "obj_grammar.c", "obj_grammar.cpp",
		"obj_libgcv_grammar.cpp", "obj_obj-g_grammar.cpp",
		"obj_parser.h", "obj_rules.cpp", "obj_rules.l",
		"obj_scanner.h", "obj_util.h", "optionparser.h", "pinttypes.h",
		"points_scan.c", "pstdint.h", "schema.h", "script.c", "ttcp.c",
		"uce-dirent.h",
		NULL
	};
	cnt = 0;
	rf = common_exempt_filter_strs[cnt];
	while (rf) {
	    std::string rrf = std::string(".*/") + std::string(rf) + std::string("$");
	    r.common_exempt_filters.push_back(std::regex(rrf));
	    cnt++;
	    rf = common_exempt_filter_strs[cnt];
	}

    }

    /* API usage check regex */
    {
	const char *api_file_exemption_strs[] {
	    "CONFIG_CONTROL_DESIGN.*", "bu/log[.]h$", "bu/path[.]h$",
		"bu/str[.]h$", "cursor[.]c$", "ttcp[.]c$",
		"misc/CMake/compat/.*",
		NULL
	};
	cnt = 0;
	rf = api_file_exemption_strs[cnt];
	while (rf) {
	    std::string rrf = std::string(".*/") + std::string(rf);
	    r.api_file_filters.push_back(std::regex(rrf));
	    cnt++;
	    rf = api_file_exemption_strs[cnt];
	}

	const char *api_func_strs[] {
	    "abort", "dirname", "fgets", "getopt", "qsort", "remove", "rmdir",
		"strcasecmp", "strcat", "strcmp", "strcpy", "strdup",
		"stricmp", "strlcat", "strlcpy", "strncasecmp", "strncat",
		"strncmp", "strncpy", "unlink",
		NULL
	};
	cnt = 0;
	rf = api_func_strs[cnt];
	while (rf) {
	    std::string rrf = std::string(".*[^a-zA-Z0-9_:]") + std::string(rf) + std::string("[(].*");
	    r.api_func_filters[std::string(rf)] = std::regex(rrf);
	    cnt++;
	    rf = api_func_strs[cnt];
	}

	r.api_exemptions[std::string("abort")].push_back(std::regex(".*/bomb[.]c$"));
	r.api_exemptions[std::string("dirname")].push_back(std::regex(".*/tests/dirname[.]c$"));
	r.api_exemptions[std::string("remove")].push_back(std::regex(".*/file[.]c$"));
	r.api_exemptions[std::string("strcasecmp")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strcmp")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strdup")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strlcat")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strlcpy")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strncasecmp")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strncat")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strncmp")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strncpy")].push_back(std::regex(".*/rt/db4[.]h$"));
	r.api_exemptions[std::string("strncpy")].push_back(std::regex(".*/str[.]c$"));
	r.api_exemptions[std::string("strncpy")].push_back(std::regex(".*/vls[.]c$"));
	r.api_exemptions[std::string("strncpy")].push_back(std::regex(".*/wfobj/obj_util[.]cpp$"));
    }

    /* Platform symbol usage check regex */
    {
	const char *platform_strs[] {
	    "AIX", "APPLE", "CYGWIN", "DARWIN", "FREEBSD", "HAIKU", "HPUX",
		"LINUX", "MINGW", "MSDOS", "QNX", "SGI", "SOLARIS", "SUN",
		"SUNOS", "SVR4", "SYSV", "ULTRIX", "UNIX", "VMS", "WIN16",
		"WIN32", "WIN64", "WINE", "WINNT", NULL
	};
	cnt = 0;
	rf = platform_strs[cnt];
	while (rf) {
	    std::string p_upper(rf);
	    std::string p_lower = p_upper;
	    std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });
	    std::string rrf = std::string("^[[:space:]#]*(if|IF).*[[:space:](]_*(") + p_lower + std::string("|") + p_upper + std::string(")_*([[:space:]]|[)]|$).*$");
	    r.platform_checks[std::make_pair(p_lower,p_upper)] = std::regex(rrf);
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
	    r.platform_file_filters.push_back(std::regex(rf));
	    cnt++;
	    rf = platform_exemption_strs[cnt];
	}
    }
}

int pos_to_line_num(const char *buff, size_t pos)
{
    int line_cnt = 1;
    size_t offset = 0;
    while (offset < pos) {
	const char *b = &buff[offset+1];
	const char *l = strchr(b, '\n');
        offset = (int)(l - buff);
	if (offset < pos) {
	    line_cnt++;
	}
    }
    return line_cnt;
}

int
bio_redundant_check(repo_info_t &r, std::vector<std::string> &srcs)
{
    int ret = 0;

    for (size_t i = 0; i < srcs.size(); i++) {
	bool have_bio = false;
	size_t bio_line = 0;
	std::map<std::string, std::set<int>> match_line_nums;

	struct bu_mapped_file *ifile = bu_open_mapped_file(srcs[i].c_str(), "bio.h candidate file");
	if (!ifile) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}
	
	const char *cline = (const char *)ifile->buf;

	// If we have anything in the buffer that looks like it might be
	// of interest, continue - otherwise we're done
	if (!std::strstr(cline, "bio.h")) {
	    continue;
	}

	// Look first for bio.h, then if found for other headers
	std::cmatch biovar;
        if (std::regex_search(cline, biovar, r.bio_regex)) {
	    have_bio = true;
	    bio_line = pos_to_line_num(cline, biovar.position());

	    // We have bio.h, check for things that shouldn't be there
	    std::cmatch cvar;
	    std::map<std::string, std::regex>::iterator f_it;
	    for (f_it = r.bnetwork_filters.begin(); f_it != r.bnetwork_filters.end(); f_it++) {
		if (!std::strstr(cline, f_it->first.c_str())) {
		    continue;
		}
		if (std::regex_search(cline, cvar, f_it->second)) {
		    size_t inc_line = pos_to_line_num(cline, cvar.position());
		    match_line_nums[f_it->first].insert(inc_line);
		    continue;
		}
	    }
	}

	bu_close_mapped_file(ifile);

	if (have_bio) {
	    std::map<std::string, std::set<int>>::iterator m_it;
	    for (m_it = match_line_nums.begin(); m_it != match_line_nums.end(); m_it++) {
		if (m_it->second.size()) {
		    std::set<int>::iterator l_it;
		    ret = 1;
		    for (l_it = m_it->second.begin(); l_it != m_it->second.end(); l_it++) {
			std::string lstr = srcs[i].substr(r.path_root.length()+1) + std::string(" includes bio.h on line ") + std::to_string(bio_line) + std::string(", but also includes ") + m_it->first + std::string(" on line ") + std::to_string(*l_it) + std::string("\n");
			r.bio_log.push_back(lstr);
		    }
		}
	    }
	}
    }

    return ret;
}

int
bnetwork_redundant_check(repo_info_t &r, std::vector<std::string> &srcs)
{
    int ret = 0;

    for (size_t i = 0; i < srcs.size(); i++) {
	bool have_bnetwork = false;
	size_t bnetwork_line = 0;
	std::map<std::string, std::set<int>> match_line_nums;

	struct bu_mapped_file *ifile = bu_open_mapped_file(srcs[i].c_str(), "bnetwork.h candidate file");
	if (!ifile) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}
	
	const char *cline = (const char *)ifile->buf;

	// If we have anything in the buffer that looks like it might be
	// of interest, continue - otherwise we're done
	if (!std::strstr(cline, "bnetwork.h")) {
	    continue;
	}

	// Look first for bnetwork.h, then if found for other headers
	std::cmatch bnetworkvar;
        if (std::regex_search(cline, bnetworkvar, r.bnetwork_regex)) {
	    have_bnetwork = true;
	    bnetwork_line = pos_to_line_num(cline, bnetworkvar.position());

	    // We have bnetwork.h, check for things that shouldn't be there
	    std::cmatch cvar;
	    std::map<std::string, std::regex>::iterator f_it;
	    for (f_it = r.bnetwork_filters.begin(); f_it != r.bnetwork_filters.end(); f_it++) {
		if (!std::strstr(cline, f_it->first.c_str())) {
		    continue;
		}
		if (std::regex_search(cline, cvar, f_it->second)) {
		    size_t inc_line = pos_to_line_num(cline, cvar.position());
		    match_line_nums[f_it->first].insert(inc_line);
		    continue;
		}
	    }
	}

	bu_close_mapped_file(ifile);

	if (have_bnetwork) {
	    std::map<std::string, std::set<int>>::iterator m_it;
	    for (m_it = match_line_nums.begin(); m_it != match_line_nums.end(); m_it++) {
		if (m_it->second.size()) {
		    std::set<int>::iterator l_it;
		    ret = 1;
		    for (l_it = m_it->second.begin(); l_it != m_it->second.end(); l_it++) {
			std::string lstr = srcs[i].substr(r.path_root.length()+1) + std::string(" includes bnetwork.h on line ") + std::to_string(bnetwork_line) + std::string(", but also includes ") + m_it->first + std::string(" on line ") + std::to_string(*l_it) + std::string("\n");
			r.bnetwork_log.push_back(lstr);
		    }
		}
	    }
	}
    }

    return ret;
}

int
common_include_first(repo_info_t &r, std::vector<std::string> &srcs)
{
    int ret = 0;

    for (size_t i = 0; i < srcs.size(); i++) {
	bool skip = false;
	for (size_t j = 0; j < r.common_exempt_filters.size(); j++) {
	    if (std::regex_match(srcs[i], r.common_exempt_filters[j])) {
		skip = true;
		break;
	    }
	}
	if (skip) {
	    continue;
	}


	struct bu_mapped_file *ifile = bu_open_mapped_file(srcs[i].c_str(), "bio.h candidate file");
	if (!ifile) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}
	
	const char *cline = (const char *)ifile->buf;

	// If we have anything in the buffer that looks like it might be
	// of interest, continue - otherwise we're done
	if (!std::strstr(cline, "common.h")) {
	    continue;
	}

	// Look first for common.h, then if found for other headers
	std::cmatch commonvar;
        if (std::regex_search(cline, commonvar, r.common_regex)) {
	    size_t common_pos = commonvar.position();
	    // We have common.h, check for other headers positioned before common.h
	    std::cmatch cvar;
	    if (std::regex_search(cline, cvar, r.inc_regex)) {
		size_t inc_pos = cvar.position();
		if (inc_pos < common_pos) {
		    size_t common_line = pos_to_line_num(cline, common_pos);
		    size_t inc_line = pos_to_line_num(cline, inc_pos);
		    std::string lstr = srcs[i].substr(r.path_root.length()+1) + std::string(" includes common.h on line ") + std::to_string(common_line) + std::string(" but a prior #include statement was found at line ") + std::to_string(inc_line) + std::string("\n");
		    r.common_log.push_back(lstr);
		    ret = 1;
		}
	    }
	}

	bu_close_mapped_file(ifile);
    }

    return ret;
}

int
api_usage(repo_info_t &r, std::vector<std::string> &srcs)
{
    int ret = 0;

    for (size_t i = 0; i < srcs.size(); i++) {
	bool skip = false;
	for (size_t j = 0; j < r.api_file_filters.size(); j++) {
	    if (std::regex_match(srcs[i], r.api_file_filters[j])) {
		skip = true;
		break;
	    }
	}
	if (skip) {
	    continue;
	}

	struct bu_mapped_file *ifile = bu_open_mapped_file(srcs[i].c_str(), "bio.h candidate file");
	if (!ifile) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}

	std::string fbuff((char *)ifile->buf);
	std::istringstream fs(fbuff);


	std::map<std::string, std::set<int>> instances;

	int lcnt = 0;
	std::string sline;
	while (std::getline(fs, sline)) {
	    lcnt++;
	    std::map<std::string, std::regex>::iterator ff_it;
	    for (ff_it = r.api_func_filters.begin(); ff_it != r.api_func_filters.end(); ff_it++) {
		if (!std::strstr(sline.c_str(), ff_it->first.c_str())) {
		    // Only try the full regex if strstr says there is a chance
		    continue;
		}
		if (std::regex_match(sline, ff_it->second)) {
		    // If we have a it, make sure it's not an exemption
		    bool exempt = false;
		    if (r.api_exemptions.find(ff_it->first) != r.api_exemptions.end()) {
			std::vector<std::regex>::iterator e_it;
			for (e_it = r.api_exemptions[ff_it->first].begin(); e_it != r.api_exemptions[ff_it->first].end(); e_it++) {
			    if (std::regex_match(srcs[i], *e_it)) {
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

	bu_close_mapped_file(ifile);

	std::map<std::string, std::set<int>>::iterator i_it;

	for (i_it = instances.begin(); i_it != instances.end(); i_it++) {
	    std::set<int>::iterator num_it;
	    for (num_it = i_it->second.begin(); num_it != i_it->second.end(); num_it++) {
	    std::string lstr = srcs[i].substr(r.path_root.length()+1) + std::string(" has ") + i_it->first + std::string(" on line ") + std::to_string(*num_it) + std::string("\n");
	    r.api_log.push_back(lstr);
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
platform_symbols(repo_info_t &r, std::vector<std::string> &log, std::vector<std::string> &srcs)
{

    std::map<std::string, std::vector<platform_entry>> instances;
    for (size_t i = 0; i < srcs.size(); i++) {
	bool skip = false;
	for (size_t j = 0; j < r.platform_file_filters.size(); j++) {
	    if (std::regex_match(srcs[i], r.platform_file_filters[j])) {
		skip = true;
		break;
	    }
	}
	if (skip) {
	    continue;
	}

	struct bu_mapped_file *ifile = bu_open_mapped_file(srcs[i].c_str(), "bio.h candidate file");
	if (!ifile) {
	    std::cerr << "Unable to open " << srcs[i] << " for reading, skipping\n";
	    continue;
	}

	std::string fbuff((char *)ifile->buf);
	std::istringstream fs(fbuff);

	//std::cout << "Reading " << srcs[i] << "\n";

	int lcnt = 0;
	std::string sline;
	while (std::getline(fs, sline)) {
	    lcnt++;

	    std::map<std::pair<std::string, std::string>, std::regex>::iterator  p_it;
	    for (p_it = r.platform_checks.begin(); p_it != r.platform_checks.end(); p_it++) {
		if (!std::strstr(sline.c_str(), p_it->first.first.c_str()) && !std::strstr(sline.c_str(), p_it->first.second.c_str())) {
		    // Only try the full regex if strstr says there is a chance
		    continue;
		}
		if (std::regex_match(sline, p_it->second)) {
		    //std::cout << "match on line: " << sline << "\n";
		    platform_entry pe;
		    pe.symbol = p_it->first.second;
		    pe.file = srcs[i].substr(r.path_root.length()+1);
		    pe.line_num = lcnt;
		    pe.line = sline;
		    instances[p_it->first.second].push_back(pe);
		}
	    }
	}

	bu_close_mapped_file(ifile);
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

    repo_info_t repo_info;
    repo_info.path_root = std::string(argv[2]);
    regex_init(repo_info);

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
	"/bullet/",
	"/doc/",
	"/shapelib/",
	".log",
	".svn",
	"misc/CMake/Find",
	"misc/repoconv",
	"misc/tools",
	"misc/debian",
	"pkg.h",
	"src/libpkg",
	"src/other/",
	"~",
	NULL
    };

    // Apply filters and build up the file sets we want to introspect.
    std::regex codefile_regex(".*[.](c|cpp|cxx|cc|h|hpp|hxx|y|yy|l)([.]in)?$");
    std::regex buildfile_regex(".*([.]cmake([.]in)?|CMakeLists.txt)$");
    std::vector<std::string> src_files;
    std::vector<std::string> inc_files;
    std::vector<std::string> build_files;

    while (std::getline(src_file_stream, sfile)) {
	bool reject = false;

	int cnt = 0;
	const char *rf = reject_filters[cnt];
	while (rf) {
	    if (std::strstr(sfile.c_str(), rf)) {
		reject = true;
		break;
	    }
	    cnt++;
	    rf = reject_filters[cnt];
	}
	if (reject) {
	    continue;
	}

	if (std::regex_match(sfile, codefile_regex)) {
	    if (std::strstr(sfile.c_str(), "include")) {
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

    ret += bio_redundant_check(repo_info, inc_files);
    ret += bio_redundant_check(repo_info, src_files);
    ret += bnetwork_redundant_check(repo_info, inc_files);
    ret += bnetwork_redundant_check(repo_info, src_files);
    ret += common_include_first(repo_info, src_files);
#if 1
    ret += api_usage(repo_info, src_files);

    int h_cnt = platform_symbols(repo_info, repo_info.symbol_inc_log, inc_files);
    int s_cnt = platform_symbols(repo_info, repo_info.symbol_src_log, src_files);
    int b_cnt = platform_symbols(repo_info, repo_info.symbol_bld_log, build_files);
    int psym_cnt = h_cnt + s_cnt + b_cnt;
    int expected_psym_cnt = EXPECTED_PLATFORM_SYMBOLS;
    if (psym_cnt > expected_psym_cnt) {
	std::cout << "FAILURE: expected " << expected_psym_cnt << " platform symbols, found " << psym_cnt << "\n";
	ret = 1;
    }
#endif

    if (ret) {
	std::sort(repo_info.api_log.begin(), repo_info.api_log.end());
	std::sort(repo_info.bio_log.begin(), repo_info.bio_log.end());
	std::sort(repo_info.bnetwork_log.begin(), repo_info.bnetwork_log.end());
	std::sort(repo_info.common_log.begin(), repo_info.common_log.end());
	std::sort(repo_info.symbol_inc_log.begin(), repo_info.symbol_inc_log.end());
	std::sort(repo_info.symbol_src_log.begin(), repo_info.symbol_src_log.end());
	std::sort(repo_info.symbol_bld_log.begin(), repo_info.symbol_bld_log.end());

	if (repo_info.api_log.size()) {
	    std::cout << "\nFound " << repo_info.api_log.size() << " instances of unguarded API usage:\n";
	    for (size_t i = 0; i < repo_info.api_log.size(); i++) {
		std::cout << repo_info.api_log[i];
	    }
	}
	if (repo_info.bio_log.size()) {
	    std::cout << "\nFound " << repo_info.bio_log.size() << " instances of redundant header inclusions in files using bio.h:\n";
	    for (size_t i = 0; i < repo_info.bio_log.size(); i++) {
		std::cout << repo_info.bio_log[i];
	    }
	}
	if (repo_info.bnetwork_log.size()) {
	    std::cout << "\nFound " << repo_info.bnetwork_log.size() << " instances of redundant header inclusions in files using bnetwork.h:\n";
	    for (size_t i = 0; i < repo_info.bnetwork_log.size(); i++) {
		std::cout << repo_info.bnetwork_log[i];
	    }
	}
	if (repo_info.common_log.size()) {
	    std::cout << "\nFound " << repo_info.common_log.size() << " instances of files using common.h with out-of-order inclusions:\n";
	    for (size_t i = 0; i < repo_info.common_log.size(); i++) {
		std::cout << repo_info.common_log[i];
	    }
	}
	if (repo_info.symbol_inc_log.size()) {
	    std::cout << "\nFound " << repo_info.symbol_inc_log.size() << " instances of platform symbol usage in header files:\n";
	    for (size_t i = 0; i < repo_info.symbol_inc_log.size(); i++) {
		std::cout << repo_info.symbol_inc_log[i];
	    }
	}
	if (repo_info.symbol_src_log.size()) {
	    std::cout << "\nFound " << repo_info.symbol_src_log.size() << " instances of platform symbol usage in source files:\n";
	    for (size_t i = 0; i < repo_info.symbol_src_log.size(); i++) {
		std::cout << repo_info.symbol_src_log[i];
	    }
	}
	if (repo_info.symbol_bld_log.size()) {
	    std::cout << "\nFound " << repo_info.symbol_bld_log.size() << " instances of platform symbol usage in build files:\n";
	    for (size_t i = 0; i < repo_info.symbol_bld_log.size(); i++) {
		std::cout << repo_info.symbol_bld_log[i];
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
