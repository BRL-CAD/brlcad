/*                   R E P O C H E C K . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by the U.S.
 * Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions
 *      in the documentation and/or other materials provided
 *      with the distribution.
 *   3. The name of the author may not be used to endorse or promote
 *      products derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @file repocheck.cpp
 *
 * Repository maintenance checker.
 *
 * 2025 updates:
 *   - Parallel file scanning (env: REPOCHECK_JOBS).
 *   - Single-pass per file for all checks.
 *   - Removed custom bu_strnstr (unsafe & unnecessary).
 *   - Precise identifier boundary matching for API/DNU calls.
 *   - Full comment & literal sanitization for ALL checks (no matches
 *     inside comments or within string/char literals).
 *   - Removed fixed 500 line scan cap: by default we now scan
 *     entire files to avoid false all-clears (e.g. bu_setprogname after
 *     earlier limit). Optional env REPOCHECK_MAX_LINES may still impose
 *     a limit if explicitly set to a positive value; 0 or unset means
 *     unlimited.
 */

#include "common.h"

// STL includes (sstream needed for istringstream - keep high in order for MSVC)
#include <sstream>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/mapped_file.h"
#include "bu/str.h"

#define EXPECTED_PLATFORM_SYMBOLS 140
#define DEFAULT_FALLBACK_THREADS 4

/* -------- Environment helpers -------- */
static size_t
get_line_scan_limit()
{
    /* Return SIZE_MAX (effectively unlimited) if unset or zero */
    const char *e = std::getenv("REPOCHECK_MAX_LINES");
    if (!e || !*e) return SIZE_MAX;
    char *ep = nullptr;
    unsigned long long v = std::strtoull(e, &ep, 10);
    if (!ep || *ep) return SIZE_MAX;
    if (v == 0) return SIZE_MAX;
    return (size_t)v;
}

static unsigned
get_job_count()
{
    const char *e = std::getenv("REPOCHECK_JOBS");
    if (e && *e) {
	char *ep = nullptr;
	unsigned long v = std::strtoul(e, &ep, 10);
	if (ep && !*ep && v > 0) return (unsigned)v;
    }
    unsigned hc = std::thread::hardware_concurrency();
    if (!hc) hc = DEFAULT_FALLBACK_THREADS;
    return hc;
}

/* -------- String helpers -------- */
static std::string_view
ltrim_sv(std::string_view sv)
{
    size_t i = 0;
    while (i < sv.size() && std::isspace((unsigned char)sv[i])) ++i;
    return sv.substr(i);
}

/* Identifier char test */
static inline bool
is_ident_char(char c)
{
    return std::isalnum((unsigned char)c) || c == '_';
}

/* Produce sanitized code-only line */
static std::string
sanitize_code_line(const std::string &line, bool &in_block_comment)
{
    std::string out;
    out.reserve(line.size());

    bool in_string = false;
    bool in_char = false;

    for (size_t i = 0; i < line.size();) {
	char c = line[i];
	char next = (i + 1 < line.size()) ? line[i + 1] : '\0';

	if (in_block_comment) {
	    if (c == '*' && next == '/') {
		in_block_comment = false;
		i += 2;
	    } else {
		++i;
	    }
	    continue;
	}

	if (!in_string && !in_char) {
	    if (c == '/' && next == '/') {
		break; // end-of-line comment
	    }
	    if (c == '/' && next == '*') {
		in_block_comment = true;
		i += 2;
		continue;
	    }
	    if (c == '"') {
		in_string = true;
		out.push_back(' ');
		++i;
		continue;
	    }
	    if (c == '\'') {
		in_char = true;
		out.push_back(' ');
		++i;
		continue;
	    }
	    out.push_back(c);
	    ++i;
	    continue;
	}

	if (in_string) {
	    if (c == '\\') {
		out.push_back(' ');
		if (i + 1 < line.size()) out.push_back(' ');
		i += 2;
		continue;
	    }
	    if (c == '"') {
		in_string = false;
		out.push_back(' ');
		++i;
		continue;
	    }
	    out.push_back(' ');
	    ++i;
	    continue;
	}

	if (in_char) {
	    if (c == '\\') {
		out.push_back(' ');
		if (i + 1 < line.size()) out.push_back(' ');
		i += 2;
		continue;
	    }
	    if (c == '\'') {
		in_char = false;
		out.push_back(' ');
		++i;
		continue;
	    }
	    out.push_back(' ');
	    ++i;
	    continue;
	}
    }

    return out;
}

/* Parse #include header name from a sanitized line */
static std::string
parse_include_target(std::string_view line)
{
    auto l = ltrim_sv(line);
    if (l.empty() || l[0] != '#')
	return {};
    size_t inc_pos = l.find("include");
    if (inc_pos == std::string_view::npos)
	return {};
    for (size_t i = 1; i < inc_pos; ++i) {
	if (!std::isspace((unsigned char)l[i]))
	    return {};
    }
    inc_pos += 7;
    while (inc_pos < l.size() && std::isspace((unsigned char)l[inc_pos]))
	++inc_pos;
    if (inc_pos >= l.size())
	return {};
    char open = l[inc_pos];
    char close = (open == '<') ? '>' : (open == '"' ? '"' : '\0');
    if (!close)
	return {};
    size_t q = l.find(close, inc_pos + 1);
    if (q == std::string_view::npos)
	return {};
    return std::string(l.substr(inc_pos + 1, q - (inc_pos + 1)));
}

/* Boundary-checked identifier presence */
static bool
identifier_boundary_present(std::string_view line, std::string_view sym)
{
    size_t pos = line.find(sym);
    while (pos != std::string_view::npos) {
	bool left_ok = (pos == 0) || !is_ident_char(line[pos - 1]);
	size_t after = pos + sym.size();
	bool right_ok = (after >= line.size()) || !is_ident_char(line[after]);
	if (left_ok && right_ok)
	    return true;
	pos = line.find(sym, pos + 1);
    }
    return false;
}

/* Detect standalone identifier followed by '(' */
static bool
function_call_present(std::string_view line, std::string_view name)
{
    size_t pos = line.find(name);
    while (pos != std::string_view::npos) {
	/* Left boundary must not be part of an identifier AND must not be a
	 * member access operator ('.' or '->').  We DO allow namespace scope
	 * operator '::' so that std::remove still counts as a forbidden call.
	 */
	bool left_ok = false;
	if (pos == 0) {
	    left_ok = true;
	} else {
	    char p1 = line[pos - 1];
	    char p2 = (pos >= 2) ? line[pos - 2] : '\0';
	    if (p1 == '.') {
		left_ok = false;          /* object.remove(...) => ignore */
	    } else if (p1 == '>' && p2 == '-') {
		left_ok = false;          /* ptr->remove(...) => ignore */
	    } else if (is_ident_char(p1)) {
		left_ok = false;          /* part of larger identifier */
	    } else {
		/* p1 is a non-identifier separator (could be ':' from '::',
		 * space, '(', ',', ';', etc.)  Accept.
		 */
		left_ok = true;
	    }
	}
	if (left_ok) {
	    size_t after = pos + name.size();
	    if (after >= line.size() || !is_ident_char(line[after])) {
		while (after < line.size() && std::isspace((unsigned char)line[after])) ++after;
		if (after < line.size() && line[after] == '(') return true;
	    }
	}
	pos = line.find(name, pos + 1);
    }
    return false;
}

/* -------- Configuration structures (grouped with test filters) -------- */

/* Redundant header include test: files using primary should not include disallowed headers */
struct HeaderRedundancyCheck {
    std::string primary;
    std::set<std::string> disallowed_headers;
    std::vector<std::regex> exemptions; // skip this redundancy check for matching files
};

/* Common.h ordering test */
struct CommonIncludeCheck {
    std::vector<std::regex> exemptions; // skip ordering check for matching files
};

/* API usage test:
 * - names: list of forbidden API function names to flag
 * - exemptions: pairs of (file_regex, function_name). If function_name == "",
 *               skip ALL API checks for files matching file_regex.
 *               Otherwise, skip ONLY that function for that file.
 */
struct ApiCheck {
    std::vector<std::string> names;
    std::vector<std::pair<std::regex, std::string>> exemptions;
};

/* DNU usage test (similar to API) */
struct DnuCheck {
    std::vector<std::string> names;
    std::vector<std::pair<std::regex, std::string>> exemptions; // "" => skip all DNU for file
};

/* setprogname test */
struct SetPrognameCheck {
    std::vector<std::regex> exemptions; // skip setprogname test for matching files
};

/* Platform symbol test */
struct PlatformSymbolCheck {
    std::vector<std::string> upper;
    std::vector<std::string> lower;
    std::vector<std::regex> exemptions; // skip platform test for matching files
};

struct RepoConfig {
    std::vector<HeaderRedundancyCheck> redundancy;
    CommonIncludeCheck common;
    ApiCheck api;
    DnuCheck dnu;
    SetPrognameCheck setprog;
    PlatformSymbolCheck platform;
};

/* -------- Helpers for exemptions -------- */
static bool
any_regex_match(const std::vector<std::regex> &rs, const std::string &path)
{
    for (const auto &r : rs)
	if (std::regex_match(path, r))
	    return true;
    return false;
}

static bool
api_is_exempt(const ApiCheck &api, const std::string &path, const std::string &fname)
{
    for (const auto &p : api.exemptions) {
	if (std::regex_match(path, p.first)) {
	    if (p.second.empty() || p.second == fname)
		return true;
	}
    }
    return false;
}

static bool
dnu_is_exempt(const DnuCheck &dnu, const std::string &path, const std::string &fname)
{
    for (const auto &p : dnu.exemptions) {
	if (std::regex_match(path, p.first)) {
	    if (p.second.empty() || p.second == fname)
		return true;
	}
    }
    return false;
}

/* -------- Configuration initialization (filters live with tests) -------- */
static void
init_repo_config(RepoConfig &cfg)
{
    /* Redundancy checks */
    {
	HeaderRedundancyCheck bio;
	bio.primary = "bio.h";
	const char *others[] = {"fcntl.h","io.h","stdio.h","unistd.h","windows.h",nullptr};
	for (int i=0; others[i]; ++i) bio.disallowed_headers.insert(others[i]);
	// Add file exemptions for bio redundancy here if needed:
	// bio.exemptions.emplace_back(std::regex(".*/some_bio_test.c$"));
	cfg.redundancy.push_back(std::move(bio));
    }
    {
	HeaderRedundancyCheck net;
	net.primary = "bnetwork.h";
	const char *others[] = {"winsock2.h","netinet/in.h","netinet/tcp.h","arpa/inet.h",nullptr};
	for (int i=0; others[i]; ++i) net.disallowed_headers.insert(others[i]);
	// net.exemptions.emplace_back(std::regex(".*/some_net_test.c$"));
	cfg.redundancy.push_back(std::move(net));
    }

    /* Common.h ordering */
    {
	const char *common_exempt_files[] = {
	    "bio.h",
	    "bnetwork.h",
	    "config_win.h",
	    "obj_grammar.c",
	    "obj_grammar.cpp",
	    "obj_libgcv_grammar.cpp",
	    "obj_obj-g_grammar.cpp",
	    "obj_parser.h",
	    "obj_rules.cpp",
	    "obj_rules.l",
	    "obj_scanner.h",
	    "obj_util.h",
	    "optionparser.h",
	    "pinttypes.h",
	    "points_scan.c",
	    "pstdint.h",
	    "schema.h",
	    "script.c",
	    "ttcp.c",
	    "uce-dirent.h",
	    nullptr
	};
	for (int i=0; common_exempt_files[i]; ++i)
	    cfg.common.exemptions.emplace_back(std::regex(std::string(".*/") + common_exempt_files[i] + "$"));
    }

    /* API usage test */
    {
	// Functions to check
	const char *api_funcs[] = {
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
	    nullptr
	};
	for (int i=0; api_funcs[i]; ++i) cfg.api.names.emplace_back(api_funcs[i]);

	// Per-function and whole-file exemptions. If second == "", exempt all API checks for the file.
	// Whole-file API exemptions (legacy api_file_exempt):
	const char *api_file_exempt[] = {
	    "CONFIG_CONTROL_DESIGN.*",
	    "bu/log[.]h$",
	    "bu/path[.]h$",
	    "bu/str[.]h$",
	    "cursor[.]c$",
	    "file[.]cpp$",
	    "linenoise[.]hpp$",
	    "misc/CMake/compat/.*",
	    "pstdint.h",
	    "ttcp[.]c$",
	    "uce-dirent.h",
	    nullptr
	};
	for (int i=0; api_file_exempt[i]; ++i) {
	    cfg.api.exemptions.emplace_back(std::make_pair(std::regex(std::string(".*/") + api_file_exempt[i]), std::string("")));
	}

	// Function-specific exemptions (migrated from per-function lists):
	auto add_func_ex = [&](const char *pattern, const char *fname) {
	    cfg.api.exemptions.emplace_back(std::make_pair(std::regex(pattern), std::string(fname)));
	};
	add_func_ex(".*/bomb[.]c$", "abort");
	add_func_ex(".*/tests/dirname[.]c$", "dirname");
	add_func_ex(".*/file[.]c$", "remove");
	add_func_ex(".*/libtermio[.]h$", "strncmp");

	const char *str_related[] = {
	    "strcasecmp",
	    "strcmp",
	    "strdup",
	    "strlcat",
	    "strlcpy",
	    "strncasecmp",
	    "strncat",
	    "strncmp",
	    "strncpy",
	    nullptr
	};
	for (int i=0; str_related[i]; ++i)
	    add_func_ex(".*/str[.]c$", str_related[i]);

	// Specific additional exemptions
	add_func_ex(".*/rt/db4[.]h$", "strncpy");
	add_func_ex(".*/vls[.]c$", "strncpy");
	add_func_ex(".*/wfobj/obj_util[.]cpp$", "strncpy");
	add_func_ex(".*/libtermio[.]h$", "strncpy");
    }

    /* DNU usage test (similar behavior to API) */
    {
	// Functions to flag
	cfg.dnu.names.push_back("std::system");
	// Whole-file DNU exemptions ("" means skip this test entirely for matching file)
	// cfg.dnu.exemptions.emplace_back(std::make_pair(std::regex(".*/legacy_tool.c$"), std::string("")));
	// Function-specific (rare for DNU, but supported):
	// cfg.dnu.exemptions.emplace_back(std::make_pair(std::regex(".*/ok_wrapper.c$"), std::string("std::system")));
    }

    /* setprogname test */
    {
	const char *sp_exempt[] = {
	    "debug2c.cpp",
	    "env2c.cpp",
	    "embedded_check.cpp",
	    "fftc.c",
	    "fftest.c",
	    "ged_cmd_scanner.cpp",
	    "ifftc.c",
	    "misc/",
	    "mt19937ar.c",
	    "other_check.cpp",
	    "perm_test.cpp",
	    "rt_ecmd_scanner.cpp",
	    "sha1.c",
	    "stb_truetype.h",
	    "ttcp.c",
	    nullptr
	};
	for (int i=0; sp_exempt[i]; ++i)
	    cfg.setprog.exemptions.emplace_back(std::regex(std::string(".*/") + sp_exempt[i] + ".*"));
	// Add additional specific exemptions here as needed.
    }

    /* Platform symbol test */
    {
	const char *platforms[] = {
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
	    "MSVC",
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
	    nullptr
	};
	for (int i=0; platforms[i]; ++i) {
	    std::string u = platforms[i];
	    std::string l = u;
	    std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) {
		return (char)std::tolower(c);
	    });
	    cfg.platform.upper.push_back(u);
	    cfg.platform.lower.push_back(l);
	}
	// There are a few bundled files we don't want these warnings
	// triggering on, since we don't own them and/or they are intended to
	// be stand-alone, without the benefit of BRL-CAD's configure time
	// detections.
	const char *platform_exempt[] = {
	    ".*/linenoise[.]hpp$",
	    ".*/pstdint[.]h$",
	    ".*/pinttypes[.]h$",
	    ".*/uce-dirent[.]h$",
	    nullptr
	};
	for (int i=0; platform_exempt[i]; ++i)
	    cfg.platform.exemptions.emplace_back(std::regex(platform_exempt[i]));
    }
}

/* -------- Result aggregation -------- */
enum class FileClass { Code, HeaderIncl, Build };

struct FileScanResult {
    std::vector<std::string> bio_log;
    std::vector<std::string> bnet_log;
    std::vector<std::string> common_log;
    std::vector<std::string> api_log;
    std::vector<std::string> dnu_log;
    std::vector<std::string> setprogname_log;
    std::vector<std::string> platform_log_headers;
    std::vector<std::string> platform_log_sources;
    std::vector<std::string> platform_log_build;
    int platform_count_headers = 0;
    int platform_count_sources = 0;
    int platform_count_build = 0;
};

struct GlobalLogs {
    std::vector<std::string> bio_log;
    std::vector<std::string> bnet_log;
    std::vector<std::string> common_log;
    std::vector<std::string> api_log;
    std::vector<std::string> dnu_log;
    std::vector<std::string> setprogname_log;
    std::vector<std::string> symbol_inc_log;
    std::vector<std::string> symbol_src_log;
    std::vector<std::string> symbol_bld_log;
    void merge(FileScanResult &r)
    {
	bio_log.insert(bio_log.end(), r.bio_log.begin(), r.bio_log.end());
	bnet_log.insert(bnet_log.end(), r.bnet_log.begin(), r.bnet_log.end());
	common_log.insert(common_log.end(), r.common_log.begin(), r.common_log.end());
	api_log.insert(api_log.end(), r.api_log.begin(), r.api_log.end());
	dnu_log.insert(dnu_log.end(), r.dnu_log.begin(), r.dnu_log.end());
	setprogname_log.insert(setprogname_log.end(), r.setprogname_log.begin(), r.setprogname_log.end());
	symbol_inc_log.insert(symbol_inc_log.end(), r.platform_log_headers.begin(), r.platform_log_headers.end());
	symbol_src_log.insert(symbol_src_log.end(), r.platform_log_sources.begin(), r.platform_log_sources.end());
	symbol_bld_log.insert(symbol_bld_log.end(), r.platform_log_build.begin(), r.platform_log_build.end());
    }
};

/* -------- Thread-safe work queue -------- */
template <typename T>
class WorkQueue
{
    std::queue<T> q;
    std::mutex m;
    std::condition_variable cv;
    bool closed = false;
public:
    // Returns false if the queue is closed and the item was not enqueued
    bool push(T v)
    {
	std::lock_guard<std::mutex> lg(m);
	if (closed) return false; // or assert/LOG: "push after close"
	q.push(std::move(v));
	cv.notify_one();
	return true;
    }
    // Returns false when the queue is closed and drained
    bool pop(T &out)
    {
	std::unique_lock<std::mutex> lk(m);
	cv.wait(lk, [&] { return closed || !q.empty(); });
	if (q.empty()) return false;
	out = std::move(q.front());
	q.pop();
	return true;
    }
    void close()
    {
	std::lock_guard<std::mutex> lg(m);
	closed = true;
	cv.notify_all();
    }
};

/* -------- File scanning (single pass) -------- */
static FileScanResult
scan_file(const RepoConfig &cfg,
	  const std::string &path,
	  const std::string &root,
	  FileClass fclass,
	  size_t line_limit)
{
    FileScanResult result;

    struct bu_mapped_file *mf = bu_open_mapped_file(path.c_str(), "scan target");
    if (!mf || !mf->buf) {
	if (mf) bu_close_mapped_file(mf);
	return result;
    }
    std::string_view data((const char *)mf->buf, mf->buflen);
    std::string rel = (path.size() > root.size()) ? path.substr(root.size() + 1) : path;

    // Early guard for setprogname check
    bool possible_main = (data.find("main") != std::string::npos);

    std::string filecopy(data.data(), data.size());
    bu_close_mapped_file(mf);

    std::istringstream is(filecopy);
    std::string line;
    int lnum = 0;

    // common.h ordering
    bool first_include_seen = false;
    int first_include_line = -1;
    bool common_included = false;
    int common_line = -1;

    // redundancy tracking
    struct RedundState {
	bool primary_present = false;
	std::map<std::string, std::set<int>> disallowed_lines;
    };
    std::map<std::string, RedundState> redund_map;
    for (auto &rc : cfg.redundancy)
	redund_map[rc.primary] = RedundState();

    // setprogname tracking
    bool main_found = false;
    int main_line = 0;
    bool setprog_called = false;

    // block comment state
    bool in_block_comment = false;

    // Which checks apply
    bool check_api = (fclass != FileClass::Build);
    bool check_platform = true;
    bool check_redundant = (fclass != FileClass::Build);
    bool check_common = (fclass != FileClass::Build) && !any_regex_match(cfg.common.exemptions, path);
    bool check_setprog = (fclass == FileClass::Code) &&
			 !any_regex_match(cfg.setprog.exemptions, path) && possible_main;

    while (std::getline(is, line)) {
	++lnum;
	if (line_limit != SIZE_MAX && (size_t)lnum > line_limit) break;

	if (line.empty()) continue;

	// Sanitize once per line for all detection:
	std::string sanitized = sanitize_code_line(line, in_block_comment);
	if (sanitized.empty())
	    sanitized.assign(line.size(), ' ');

	// Includes (redundancy + common ordering)
	if ((check_redundant || check_common) && !in_block_comment) {
	    if (!sanitized.empty() && sanitized[0] == '#') {
		std::string inc = parse_include_target(sanitized);
		if (!inc.empty()) {
		    if (check_common) {
			if (!first_include_seen) {
			    first_include_seen = true;
			    first_include_line = lnum;
			}
			if (inc == "common.h") {
			    common_included = true;
			    common_line = lnum;
			}
		    }
		    if (check_redundant) {
			for (auto &rc : cfg.redundancy) {
			    if (any_regex_match(rc.exemptions, path)) continue;
			    if (inc == rc.primary) {
				redund_map[rc.primary].primary_present = true;
			    } else if (redund_map.count(rc.primary) &&
				       rc.disallowed_headers.count(inc)) {
				redund_map[rc.primary].disallowed_lines[inc].insert(lnum);
			    }
			}
		    }
		}
	    }
	}

	// API usage & DNU
	if (check_api && !in_block_comment) {
	    // API calls
	    for (const auto &fname : cfg.api.names) {
		if (sanitized.find(fname) == std::string::npos) continue;
		if (!function_call_present(sanitized, fname)) continue;
		if (api_is_exempt(cfg.api, path, fname)) continue;
		result.api_log.push_back(rel + " has " + fname +
					 " on line " + std::to_string(lnum) + "\n");
	    }
	    // DNU calls
	    for (const auto &dname : cfg.dnu.names) {
		if (sanitized.find(dname) == std::string::npos) continue;
		if (!function_call_present(sanitized, dname)) continue;
		if (dnu_is_exempt(cfg.dnu, path, dname)) continue;
		result.dnu_log.push_back(rel + " uses " + dname +
					 " on line " + std::to_string(lnum) + "\n");
	    }
	}

	// setprogname
	if (check_setprog && !in_block_comment) {
	    if (!main_found && sanitized.find("main") != std::string::npos &&
		function_call_present(sanitized, "main")) {
		main_found = true;
		main_line = lnum;
	    } else if (main_found && !setprog_called &&
		       sanitized.find("bu_setprogname") != std::string::npos &&
		       function_call_present(sanitized, "bu_setprogname")) {
		setprog_called = true;
	    }
	}

	// Platform symbols
	if (check_platform && !any_regex_match(cfg.platform.exemptions, path) && !in_block_comment) {
	    std::string_view sv = ltrim_sv(sanitized);
	    bool plausible =
		(!sv.empty() && (sv[0] == '#' ||
				 sv.rfind("if", 0) == 0 || sv.rfind("IF",0) == 0 ||
				 sv.rfind("elseif",0) == 0 || sv.rfind("ELSEIF",0) == 0));
	    if (plausible) {
		std::string lower = sanitized;
		std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) {
		    return (char)std::tolower(c);
		});
		for (size_t i = 0; i < cfg.platform.upper.size(); ++i) {
		    const std::string &U = cfg.platform.upper[i];
		    const std::string &L = cfg.platform.lower[i];
		    if (sanitized.find(U) == std::string::npos && lower.find(L) == std::string::npos)
			continue;
		    if (identifier_boundary_present(lower, L) ||
			identifier_boundary_present(sanitized, U)) {
			std::string out = rel + "(" + std::to_string(lnum) + "): " + line + "\n";
			switch (fclass) {
			    case FileClass::HeaderIncl:
				result.platform_log_headers.push_back(out);
				++result.platform_count_headers;
				break;
			    case FileClass::Code:
				result.platform_log_sources.push_back(out);
				++result.platform_count_sources;
				break;
			    case FileClass::Build:
				result.platform_log_build.push_back(out);
				++result.platform_count_build;
				break;
			}
		    }
		}
	    }
	}
    }

    // Post-processing common.h ordering
    if (check_common && common_included && first_include_seen && common_line > first_include_line) {
	result.common_log.push_back(rel + " includes common.h on line " +
				    std::to_string(common_line) +
				    " but a prior #include was on line " +
				    std::to_string(first_include_line) + "\n");
    }

    // Redundancy results
    for (auto &rc : cfg.redundancy) {
	if (any_regex_match(rc.exemptions, path)) continue;
	auto &st = redund_map[rc.primary];
	if (!st.primary_present) continue;
	for (auto &kv : st.disallowed_lines) {
	    for (int l : kv.second) {
		if (rc.primary == "bio.h")
		    result.bio_log.push_back(rel + " has bio.h, but also includes " +
					     kv.first + " on line " + std::to_string(l) + "\n");
		else if (rc.primary == "bnetwork.h")
		    result.bnet_log.push_back(rel + " has bnetwork.h, but also includes " +
					      kv.first + " on line " + std::to_string(l) + "\n");
	    }
	}
    }

    // setprogname result
    if ((fclass == FileClass::Code) &&
	!any_regex_match(cfg.setprog.exemptions, path) &&
	possible_main && main_found && !setprog_called) {
	result.setprogname_log.push_back(rel + " defines a main() function on line " +
					 std::to_string(main_line) +
					 " but does not call bu_setprogname\n");
    }

    return result;
}

/* -------- Main -------- */
int
main(int argc, const char *argv[])
{
    try {
	int verbosity = 0;
	if (argc < 3 || argc > 5) {
	    std::cerr << "Usage: repocheck [-v] file_list.txt source_dir\n";
	    return -1;
	}

	bu_setprogname(argv[0]);

	if (argc == 4) {
	    if (BU_STR_EQUAL(argv[1], "-v")) {
		verbosity = 1;
		for (int i = 2; i < argc; i++) {
		    argv[i - 1] = argv[i];
		}
		argc--;
	    } else {
		bu_exit(-1, "invalid option %s", argv[1]);
	    }
	}

	std::string file_list = argv[1];
	std::string source_root = argv[2];

	RepoConfig cfg;
	init_repo_config(cfg);

	std::ifstream fls(file_list);
	if (!fls.is_open()) {
	    std::cerr << "Unable to open file list file " << file_list << "\n";
	    return -1;
	}

	const char *reject_filters[] = {
	    ".git",".log","/detria.hpp","/doc/","/fontstash/","/json.hpp",
	    "/linenoise.hpp","/shapelib/","/spsr/","/whereami.c","/xxhash.h",
	    "misc/CMake/Find","misc/debian","misc/opencl-raytracer-tests",
	    "misc/tools","pkg.h","src/libdm/wgl/wintk/","src/libpkg","subprocess.h","~",nullptr
	};

	std::regex codefile_regex(".*[.](c|cpp|cxx|cc|h|hpp|hxx|y|yy|l)([.]in)?$");
	std::regex buildfile_regex(".*([.]cmake([.]in)?|CMakeLists.txt)$");

	std::vector<std::pair<std::string, FileClass>> all_files;
	std::string line;

	while (std::getline(fls, line)) {
	    if (line.empty()) continue;
	    bool reject = false;
	    for (int i=0; reject_filters[i]; ++i) {
		if (std::strstr(line.c_str(), reject_filters[i])) {
		    reject = true;
		    break;
		}
	    }
	    if (reject) continue;

	    if (std::regex_match(line, buildfile_regex)) {
		all_files.emplace_back(line, FileClass::Build);
		continue;
	    }
	    if (std::regex_match(line, codefile_regex)) {
		if (line.find("include") != std::string::npos)
		    all_files.emplace_back(line, FileClass::HeaderIncl);
		else
		    all_files.emplace_back(line, FileClass::Code);
	    }
	}

	size_t line_limit = get_line_scan_limit();
	unsigned jobs = get_job_count();
	if (jobs == 0) jobs = 1;

	WorkQueue<std::pair<std::string, FileClass>> wq;
	std::mutex merge_mutex;
	GlobalLogs logs;
	std::atomic<int> platform_header_count{0};
	std::atomic<int> platform_source_count{0};
	std::atomic<int> platform_build_count{0};

	for (auto &pf : all_files) wq.push(pf);
	wq.close();

	auto worker = [&]() {
	    std::pair<std::string, FileClass> item;
	    while (wq.pop(item)) {
		auto res = scan_file(cfg, item.first, source_root, item.second, line_limit);
		{
		    std::lock_guard<std::mutex> lg(merge_mutex);
		    logs.merge(res);
		    platform_header_count += res.platform_count_headers;
		    platform_source_count += res.platform_count_sources;
		    platform_build_count += res.platform_count_build;
		}
	    }
	};

	std::vector<std::thread> threads;
	for (unsigned i = 0; i < jobs; ++i) threads.emplace_back(worker);
	for (auto &t : threads) t.join();

	int psym_cnt = platform_header_count + platform_source_count + platform_build_count;
	int expected_psym_cnt = EXPECTED_PLATFORM_SYMBOLS;

	bool failed = false;
	if (!logs.api_log.empty()) failed = true;
	if (!logs.bio_log.empty()) failed = true;
	if (!logs.bnet_log.empty()) failed = true;
	if (!logs.common_log.empty()) failed = true;
	if (!logs.dnu_log.empty()) failed = true;
	if (!logs.setprogname_log.empty()) failed = true;
	if (psym_cnt > expected_psym_cnt) failed = true;

	if (psym_cnt < expected_psym_cnt) {
	    std::cout << "\n\nNote: need to update EXPECTED_PLATFORM_SYMBOLS - looking for "
		      << expected_psym_cnt << ", but only found " << psym_cnt << "\n\n\n";
	}

	if (failed || verbosity) {
	    auto sorter = [](auto &v) {
		std::sort(v.begin(), v.end());
	    };
	    sorter(logs.api_log);
	    sorter(logs.bio_log);
	    sorter(logs.bnet_log);
	    sorter(logs.common_log);
	    sorter(logs.dnu_log);
	    sorter(logs.setprogname_log);
	    sorter(logs.symbol_inc_log);
	    sorter(logs.symbol_src_log);
	    sorter(logs.symbol_bld_log);

	    if (!logs.api_log.empty()) {
		std::cout << "\nFAILURE: found " << logs.api_log.size()
			  << " instances of unguarded API usage:\n";
		for (auto &s : logs.api_log) std::cout << s;
	    }
	    if (!logs.bio_log.empty()) {
		std::cout << "\nFAILURE: found " << logs.bio_log.size()
			  << " instances of redundant header inclusions in files using bio.h:\n";
		for (auto &s : logs.bio_log) std::cout << s;
	    }
	    if (!logs.bnet_log.empty()) {
		std::cout << "\nFAILURE: found " << logs.bnet_log.size()
			  << " instances of redundant header inclusions in files using bnetwork.h:\n";
		for (auto &s : logs.bnet_log) std::cout << s;
	    }
	    if (!logs.common_log.empty()) {
		std::cout << "\nFAILURE: found " << logs.common_log.size()
			  << " instances of files using common.h with out-of-order inclusions:\n";
		for (auto &s : logs.common_log) std::cout << s;
	    }
	    if (!logs.dnu_log.empty()) {
		std::cout << "\nFAILURE: found " << logs.dnu_log.size()
			  << " instances of proscribed function usage:\n";
		for (auto &s : logs.dnu_log) std::cout << s;
	    }
	    if (!logs.setprogname_log.empty()) {
		std::cout << "\nFAILURE: found " << logs.setprogname_log.size()
			  << " missing bu_setprogname calls:\n";
		for (auto &s : logs.setprogname_log) std::cout << s;
	    }
	    if (psym_cnt > expected_psym_cnt) {
		std::cout << "\n**************************************************************************\n";
		std::cout << "FAILURE: expected " << expected_psym_cnt
			  << " platform symbols, found " << psym_cnt << "\n";
		std::cout << "**************************************************************************\n";
	    }

	    if (psym_cnt > expected_psym_cnt || verbosity) {
		if (!logs.symbol_inc_log.empty()) {
		    std::cout << "\nFound " << logs.symbol_inc_log.size()
			      << " instances of platform symbol usage in header files:\n";
		    for (auto &s : logs.symbol_inc_log) std::cout << s;
		}
		if (!logs.symbol_src_log.empty()) {
		    std::cout << "\nFound " << logs.symbol_src_log.size()
			      << " instances of platform symbol usage in source files:\n";
		    for (auto &s : logs.symbol_src_log) std::cout << s;
		}
		if (!logs.symbol_bld_log.empty()) {
		    std::cout << "\nFound " << logs.symbol_bld_log.size()
			      << " instances of platform symbol usage in build files:\n";
		    for (auto &s : logs.symbol_bld_log) std::cout << s;
		}
	    }
	}

	return failed ? 1 : 0;
    } catch (const std::exception &ex) {
	std::cerr << "Unhandled exception: " << ex.what() << "\n";
	return -1;
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
