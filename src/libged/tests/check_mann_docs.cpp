/*             C H E C K _ M A N N _ D O C S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/**
 * @brief this tests audits BRL-CAD system documentation (mann pages)
 * against source code.
 *
 * This test parses command definitions from libged, mged, and
 * tclscripts, verifying:
 *
 *   1) corresponding AsciiDoc manpages exist,
 *   2) contain required sections (Name, Synopsis, Desc., Examples),
 *   3) accurately documented options.
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct registered_cmd {
    std::string cmd;
    std::string fn;
    std::string source;
    std::string origin;
    bool strict = false;
};

struct cmd_audit {
    registered_cmd reg;
    std::string page_name;
    std::set<std::string> options;
    std::vector<std::string> origins;
    bool page_exists = false;
    bool cmake_listed = false;
    bool has_synopsis = false;
    bool has_description = false;
    bool has_examples = false;
    bool has_example_block = false;
    bool name_matches = false;
    bool good_example = false;
    bool is_alias = false;
    bool alias_ok = false;
    bool options_ok = true;
    bool strict = false;
    std::string kind_override;
    std::string alias_target;
    std::string name_token;
    std::vector<std::string> missing_options;
    std::vector<std::string> failures;
    std::vector<std::string> notes;
};

static std::string
manpage_for_command(const std::string &cmd)
{
    if (cmd == "E")
	return "bigE";
    return cmd;
}

static std::string
slurp(const fs::path &p)
{
    std::ifstream ifs(p);
    if (!ifs.good())
	return std::string();
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

static std::string
trim(const std::string &s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start]))
	start++;
    if (start == s.size())
	return std::string();
    size_t end = s.size() - 1;
    while (end > start && std::isspace((unsigned char)s[end]))
	end--;
    return s.substr(start, end - start + 1);
}

static bool
contains(const std::string &txt, const std::string &needle)
{
    return txt.find(needle) != std::string::npos;
}

static std::string
decode_c_string(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
	char c = s[i];
	if (c != '\\' || i + 1 >= s.size()) {
	    out.push_back(c);
	    continue;
	}
	char n = s[++i];
	switch (n) {
	    case 'n':
		out.push_back('\n');
		break;
	    case 't':
		out.push_back('\t');
		break;
	    case 'r':
		out.push_back('\r');
		break;
	    case '\\':
		out.push_back('\\');
		break;
	    case '"':
		out.push_back('"');
		break;
	    default:
		out.push_back(n);
		break;
	}
    }
    return out;
}

static void
append_unique(std::vector<std::string> &v, const std::string &val)
{
    if (std::find(v.begin(), v.end(), val) == v.end())
	v.push_back(val);
}

static std::string
join_strings(const std::vector<std::string> &v, const std::string &sep)
{
    std::ostringstream oss;
    bool first = true;
    for (const auto &s : v) {
	if (!first)
	    oss << sep;
	oss << s;
	first = false;
    }
    return oss.str();
}

static bool
has_origin_prefix(const cmd_audit &a, const std::string &prefix)
{
    for (const auto &o : a.origins) {
	if (o.rfind(prefix, 0) == 0)
	    return true;
    }
    return false;
}

static void
add_origin(cmd_audit &a, const std::string &origin)
{
    append_unique(a.origins, origin);
}

static std::string
decode_mged_cmd_name(const std::string &raw)
{
    if (raw == "cmd3525")
	return "35, 25";
    if (raw == "cmd4545")
	return "45, 45";
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
	return decode_c_string(raw.substr(1, raw.size() - 2));
    return raw;
}

static std::string
find_function_body(const std::string &txt, const std::string &fn)
{
    std::regex fn_re("\\b(?:int|static\\s+int)\\s+" + fn + "\\s*\\([^\\)]*\\)\\s*\\{");
    std::smatch fm;
    if (!std::regex_search(txt, fm, fn_re))
	return std::string();

    size_t start = txt.find('{', fm.position(0));
    if (start == std::string::npos)
	return std::string();

    int depth = 0;
    bool in_string = false;
    bool in_line_comment = false;
    bool in_block_comment = false;
    bool escape = false;

    for (size_t i = start; i < txt.size(); i++) {
	char c = txt[i];
	char n = (i + 1 < txt.size()) ? txt[i + 1] : '\0';

	if (in_line_comment) {
	    if (c == '\n')
		in_line_comment = false;
	    continue;
	}
	if (in_block_comment) {
	    if (c == '*' && n == '/') {
		in_block_comment = false;
		i++;
	    }
	    continue;
	}
	if (in_string) {
	    if (escape) {
		escape = false;
		continue;
	    }
	    if (c == '\\') {
		escape = true;
		continue;
	    }
	    if (c == '"') {
		in_string = false;
	    }
	    continue;
	}

	if (c == '/' && n == '/') {
	    in_line_comment = true;
	    i++;
	    continue;
	}
	if (c == '/' && n == '*') {
	    in_block_comment = true;
	    i++;
	    continue;
	}
	if (c == '"') {
	    in_string = true;
	    continue;
	}

	if (c == '{')
	    depth++;
	if (c == '}') {
	    depth--;
	    if (depth == 0)
		return txt.substr(start, i - start + 1);
	}
    }

    return txt.substr(start);
}

static void
explode_usage_token(std::set<std::string> &opts, const std::string &tok)
{
    if (tok.empty())
	return;

    if (tok == "-?" || tok.rfind("--", 0) == 0) {
	opts.insert(tok);
	return;
    }

    if (tok.size() > 2 && tok[0] == '-') {
	bool all_alpha = true;
	for (size_t i = 1; i < tok.size(); i++) {
	    if (!std::isalpha((unsigned char)tok[i])) {
		all_alpha = false;
		break;
	    }
	}
	if (all_alpha) {
	    for (size_t i = 1; i < tok.size(); i++) {
		std::string s("-");
		s.push_back(tok[i]);
		opts.insert(s);
	    }
	    return;
	}
    }

    opts.insert(tok);
}

static std::set<std::string>
extract_usage_options(const std::string &body)
{
    std::set<std::string> opts;
    std::regex usage_re(R"re(([A-Za-z0-9_]*usage[A-Za-z0-9_]*)\s*=\s*"((?:\\.|[^"\\])*)")re");
    std::regex opt_re(R"((\-\?|\-\-?[A-Za-z0-9][A-Za-z0-9_-]*))");

    for (auto it = std::sregex_iterator(body.begin(), body.end(), usage_re); it != std::sregex_iterator(); ++it) {
	std::string decoded = decode_c_string((*it)[2].str());
	for (auto oit = std::sregex_iterator(decoded.begin(), decoded.end(), opt_re); oit != std::sregex_iterator(); ++oit) {
	    explode_usage_token(opts, (*oit)[1].str());
	}
    }

    return opts;
}

static std::set<std::string>
extract_buopt_options(const std::string &body)
{
    std::set<std::string> opts;
    std::regex buopt_re(R"re(BU_OPT\s*\([^, ]+, \s*"((?:\\.|[^"\\])*)"\s*, \s*"((?:\\.|[^"\\])*)")re");

    for (auto it = std::sregex_iterator(body.begin(), body.end(), buopt_re); it != std::sregex_iterator(); ++it) {
	std::string shorts = decode_c_string((*it)[1].str());
	std::string longs = decode_c_string((*it)[2].str());

	std::stringstream ss(shorts);
	std::string s;
	while (std::getline(ss, s, '|')) {
	    s = trim(s);
	    if (!s.empty())
		opts.insert("-" + s);
	}

	std::stringstream ls(longs);
	while (std::getline(ls, s, '|')) {
	    s = trim(s);
	    if (!s.empty())
		opts.insert("--" + s);
	}
    }

    return opts;
}

static std::set<std::string>
extract_getopt_options(const std::string &body)
{
    std::set<std::string> opts;
    std::regex getopt_re(R"re(bu_getopt\s*\([^\)]*"((?:\\.|[^"\\])*)"\))re");

    for (auto it = std::sregex_iterator(body.begin(), body.end(), getopt_re); it != std::sregex_iterator(); ++it) {
	std::string gstr = decode_c_string((*it)[1].str());
	for (size_t i = 0; i < gstr.size(); i++) {
	    char c = gstr[i];
	    if (c == ':')
		continue;
	    if (c == '?') {
		opts.insert("-?");
		continue;
	    }
	    if (std::isalnum((unsigned char)c)) {
		std::string s("-");
		s.push_back(c);
		opts.insert(s);
	    }
	}
    }

    return opts;
}

static void
collapse_case_pairs(std::set<std::string> &opts, const std::set<std::string> &usage_opts)
{
    std::map<std::string, std::set<std::string> > groups;
    for (const auto &o : opts) {
	if (o.size() == 2 && o[0] == '-' && std::isalpha((unsigned char)o[1])) {
	    std::string low = o;
	    low[1] = (char)std::tolower((unsigned char)low[1]);
	    groups[low].insert(o);
	}
    }

    for (const auto &g : groups) {
	if (g.second.size() < 2)
	    continue;

	std::set<std::string> keep;
	for (const auto &o : g.second) {
	    if (usage_opts.find(o) != usage_opts.end())
		keep.insert(o);
	}

	for (const auto &o : g.second)
	    opts.erase(o);

	if (!keep.empty()) {
	    opts.insert(keep.begin(), keep.end());
	} else {
	    opts.insert(g.first);
	}
    }
}

static std::set<std::string>
extract_command_options(const fs::path &src_path, const std::string &fn)
{
    std::string txt = slurp(src_path);
    std::string body = find_function_body(txt, fn);
    std::set<std::string> usage_opts = extract_usage_options(body);
    std::set<std::string> buopt_opts = extract_buopt_options(body);
    std::set<std::string> getopt_opts = extract_getopt_options(body);

    collapse_case_pairs(getopt_opts, usage_opts);

    std::set<std::string> opts = usage_opts;
    std::set<std::string> usage_lower;
    for (const auto &o : usage_opts) {
	std::string low = o;
	std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c){ return (char)std::tolower(c); });
	usage_lower.insert(low);
    }

    for (const auto &o : buopt_opts) {
	std::string low = o;
	std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c){ return (char)std::tolower(c); });
	if (o.rfind("--", 0) == 0 || usage_opts.find(o) != usage_opts.end() || usage_lower.find(low) == usage_lower.end())
	    opts.insert(o);
    }

    if (usage_opts.empty()) {
	opts.insert(getopt_opts.begin(), getopt_opts.end());
    } else {
	for (const auto &o : getopt_opts) {
	    std::string low = o;
	    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c){ return (char)std::tolower(c); });
	    if (o.rfind("--", 0) == 0 || o == "-h" || o == "-?" || usage_lower.find(low) == usage_lower.end())
		opts.insert(o);
	}
    }

    return opts;
}

static std::string
extract_name_token(const std::string &txt)
{
    std::regex nm_re(R"(== NAME\s*\n\s*([^\n]+?)(?:\s+-|\n))");
    std::smatch nm;
    if (std::regex_search(txt, nm, nm_re))
	return trim(nm[1].str());
    return std::string();
}

static std::string
detect_alias_target(const std::string &txt)
{
    std::smatch m;
    std::vector<std::regex> patterns = {
	std::regex(R"(shares its core implementation with\s+\*([^*]+)\*)"),
	std::regex(R"(synonym for the \*([^*]+)\* command)"),
	std::regex(R"(short alias for the detailed \*([^*]+)\* report)"),
	std::regex(R"(alias for the ([A-Za-z0-9_?]+) command)")
    };

    for (const auto &re : patterns) {
	if (std::regex_search(txt, m, re))
	    return trim(m[1].str());
    }

    return std::string();
}

static bool
has_detailed_invocation(const std::string &txt)
{
    std::regex inv_re(R"(mged>\s*\*{0,2}([^\*\n]+)\*{0,2})");
    for (auto it = std::sregex_iterator(txt.begin(), txt.end(), inv_re); it != std::sregex_iterator(); ++it) {
	std::string inv = trim((*it)[1].str());
	if (inv.find("[args]") != std::string::npos)
	    continue;
	if (inv.find("old name") != std::string::npos || inv.find("new name") != std::string::npos)
	    continue;
	std::stringstream ss(inv);
	std::string tok;
	size_t cnt = 0;
	while (ss >> tok)
	    cnt++;
	if (cnt > 1)
	    return true;
    }
    return false;
}

static bool
good_primary_example(const std::string &txt)
{
    if (!contains(txt, "== EXAMPLES"))
	return false;

    bool generic = contains(txt, "using the syntax shown in the synopsis");
    bool output_block = contains(txt, "....");
    bool detailed = has_detailed_invocation(txt);

    if (detailed || output_block)
	return true;
    return !generic;
}

static void
audit_page(cmd_audit &a, const fs::path &manndir, const std::set<std::string> &docs,
	   const std::set<std::string> &cmake_listed, bool check_options)
{
    fs::path mp = manndir / (a.page_name + ".adoc");
    a.page_exists = fs::exists(mp);
    a.cmake_listed = cmake_listed.find(a.page_name) != cmake_listed.end();

    if (!a.page_exists) {
	a.failures.push_back("missing manpage " + mp.generic_string());
	return;
    }
    if (!a.cmake_listed)
	a.failures.push_back("manpage not listed in mann CMakeLists: " + mp.filename().string());

    std::string txt = slurp(mp);
    a.has_synopsis = contains(txt, "== SYNOPSIS");
    a.has_description = contains(txt, "== DESCRIPTION");
    a.has_examples = contains(txt, "== EXAMPLES");
    a.has_example_block = contains(txt, "[example]") || contains(txt, "mged>");
    a.name_token = extract_name_token(txt);

    std::string expected_name = a.reg.cmd;
    if (!a.kind_override.empty() && a.kind_override == "support")
	expected_name = a.page_name;
    a.name_matches = (a.name_token == a.reg.cmd || a.name_token == a.page_name || a.name_token == expected_name);
    if (!a.name_matches && a.kind_override == "support") {
	std::string first_word = a.reg.cmd.substr(0, a.reg.cmd.find(' '));
	a.name_matches = (a.name_token == first_word || a.name_token.find(a.reg.cmd) != std::string::npos);
    }
    if (!a.name_matches)
	a.failures.push_back("NAME section does not identify command as " + a.reg.cmd);

    a.alias_target = detect_alias_target(txt);
    a.is_alias = !a.alias_target.empty() && a.alias_target != a.reg.cmd;

    if (!a.has_synopsis)
	a.failures.push_back("missing SYNOPSIS section");
    if (!a.has_description)
	a.failures.push_back("missing DESCRIPTION section");
    if (!a.is_alias && !a.has_examples)
	a.failures.push_back("missing EXAMPLES section");
    if (!a.is_alias && !a.has_example_block)
	a.failures.push_back("missing example block or invocation example");

    if (contains(txt, "libged command"))
	a.notes.push_back("placeholder NAME/DESCRIPTION text");
    if (contains(txt, "[args]"))
	a.notes.push_back("placeholder synopsis arguments");
    if (contains(txt, "Alias-compatible command"))
	a.notes.push_back("alias stub");

    if (a.is_alias) {
	std::string alias_page = manpage_for_command(a.alias_target);
	if (docs.find(alias_page) == docs.end())
	    a.failures.push_back("alias target page missing: " + alias_page + ".adoc");
    } else {
	if (check_options) {
	    for (const auto &o : a.options) {
		if (!contains(txt, o))
		    a.missing_options.push_back(o);
	    }
	    if (!a.missing_options.empty()) {
		std::ostringstream oss;
		oss << "missing documented options:";
		for (const auto &o : a.missing_options)
		    oss << " " << o;
		a.failures.push_back(oss.str());
	    }
	}
	if (!good_primary_example(txt))
	    a.failures.push_back("example section lacks a concrete or useful example");
    }

    a.options_ok = a.missing_options.empty();
    a.good_example = a.is_alias ? true : good_primary_example(txt);
}

static bool
alias_loop_exists(const std::string &cmd, const std::map<std::string, cmd_audit> &audits)
{
    std::set<std::string> seen;
    std::string curr = cmd;

    while (1) {
	auto it = audits.find(curr);
	if (it == audits.end())
	    return false;
	if (!it->second.is_alias || it->second.alias_target.empty())
	    return false;
	curr = it->second.alias_target;
	if (curr == cmd)
	    return true;
	if (seen.find(curr) != seen.end())
	    return true;
	seen.insert(curr);
    }
}

static void
write_inventory(const fs::path &out, const std::map<std::string, cmd_audit> &audits)
{
    std::ofstream ofs(out);
    ofs << "command\torigins\tsource\tfunction\tpage\tkind\talias_target\toptions\treview_status\tedit_status\tcleanup_status\tstatus\tnotes\n";
    for (const auto &it : audits) {
	const cmd_audit &a = it.second;
	std::string kind = a.kind_override.empty() ? (a.is_alias ? "alias" : "primary") : a.kind_override;
	if (a.is_alias && a.kind_override.empty())
	    kind = "alias";

	std::string status;
	if (kind == "unresolved") {
	    status = "unresolved_orphan";
	} else if (!a.page_exists) {
	    status = "missing_manpage";
	} else if (a.failures.empty( )) {
	    status = a.is_alias ? "alias_stub_ok" : "complete";
	} else {
	    status = "incomplete";
	}

	std::string review_status = "reviewed";
	std::string edit_status = "complete";
	std::string cleanup_status = "complete";
	if ( kind == "unresolved") {
	    review_status = "needs_source_trace";
	    edit_status = "needs_triage";
	    cleanup_status = "unresolved";
	} else if ( !a.page_exists) {
	    review_status = "identified";
	    edit_status = "needs_manpage";
	    cleanup_status = "needs_cleanup";
	} else if ( !a.failures.empty( )) {
	    review_status = "needs_review";
	    edit_status = "needs_edit";
	    cleanup_status = "needs_cleanup";
	}

	std::ostringstream optstr;
	bool first = true;
	for ( const auto &o : a.options) {
	    if ( !first)
		optstr << ", ";
	    optstr << o;
	    first = false;
	}

	std::ostringstream notes;
	first = true;
	for ( const auto &n : a.notes) {
	    if ( !first)
		notes << "; ";
	    notes << n;
	    first = false;
	}
	for ( const auto &n : a.failures) {
	    if ( !first)
		notes << "; ";
	    notes << n;
	    first = false;
	}

	ofs << a.reg.cmd << "\t"
	    << join_strings( a.origins, "; ") << "\t"
	    << a.reg.source << "\t"
	    << a.reg.fn << "\t"
	    << a.page_name << ".adoc\t"
	    << kind << "\t"
	    << a.alias_target << "\t"
	    << optstr.str( ) << "\t"
	    << review_status << "\t"
	    << edit_status << "\t"
	    << cleanup_status << "\t"
	    << status << "\t"
	    << notes.str( ) << "\n";
    }
}

static void
write_progress( const fs::path &out, const std::map<std::string, cmd_audit> &audits, const std::vector<std::string> &warnings)
{
    size_t primary_complete = 0;
    size_t alias_complete = 0;
    size_t incomplete = 0;
    size_t libged_count = 0;
    size_t mged_count = 0;
    size_t tcl_proc_count = 0;
    size_t tcl_help_count = 0;
    size_t support_count = 0;
    size_t unresolved_count = 0;
    size_t missing_manpages = 0;

    for ( const auto &it : audits) {
	const cmd_audit &a = it.second;
	if ( has_origin_prefix( a, "libged:"))
	    libged_count++;
	if ( has_origin_prefix( a, "mged_cmdtab:"))
	    mged_count++;
	if ( has_origin_prefix( a, "tcl_proc:"))
	    tcl_proc_count++;
	if ( has_origin_prefix( a, "tcl_help:"))
	    tcl_help_count++;
	if ( a.kind_override == "support")
	    support_count++;
	if ( a.kind_override == "unresolved")
	    unresolved_count++;
	if ( !a.page_exists)
	    missing_manpages++;

	if ( a.failures.empty( )) {
	    if ( a.is_alias)
		alias_complete++;
	    else
		primary_complete++;
	} else {
	    incomplete++;
	}
    }

    std::ofstream ofs( out);
    ofs << "# MGED/Libged Command Manpage Audit Progress\n\n";
    ofs << "Generated from the registered libged command list in `src/libged`, the MGED command table in `src/mged/setup.c`, documented Tcl/Tk commands in `src/tclscripts`, and the current `doc/asciidoc/system/mann` pages.\n\n";
    ofs << "## Summary\n\n";
    ofs << "- Unique tracked inventory rows: `" << audits.size( ) << "`\n";
    ofs << "- Inventory TSV physical lines: `" << ( audits.size( ) + 1) << "` ( one header row plus tracked rows)\n";
    ofs << "- Libged registered commands: `" << libged_count << "`\n";
    ofs << "- MGED command-table commands: `" << mged_count << "`\n";
    ofs << "- Documented Tcl proc commands: `" << tcl_proc_count << "`\n";
    ofs << "- Documented Tcl/help-table commands: `" << tcl_help_count << "`\n";
    ofs << "- Known support/subcommand pages: `" << support_count << "`\n";
    ofs << "- Unresolved mann pages: `" << unresolved_count << "`\n";
    ofs << "- Missing manpages for tracked commands: `" << missing_manpages << "`\n";
    ofs << "- Complete primary/support pages: `" << primary_complete << "`\n";
    ofs << "- Complete alias stubs: `" << alias_complete << "`\n";
    ofs << "- Tracked rows needing review/edit/cleanup: `" << incomplete << "`\n";
    ofs << "- Full per-command inventory: `libged_cmd_doc_inventory.tsv`\n\n";
    ofs << "The earlier `352` versus `353` discrepancy was a line-count mismatch: `352` was the libged command row count, while `353` counted the TSV header line as well.\n\n";
    ofs << "## Commands Needing Work\n\n";
    ofs << "| Command | Source | Kind | Status | Notes |\n";
    ofs << "| --- | --- | --- | --- | --- |\n";
    for ( const auto &it : audits) {
	const cmd_audit &a = it.second;
	if ( a.failures.empty( ))
	    continue;
	std::ostringstream notes;
	bool first = true;
	for ( const auto &n : a.failures) {
	    if ( !first)
		notes << "; ";
	    notes << n;
	    first = false;
	}
	std::string kind = a.kind_override.empty( ) ? ( a.is_alias ? "alias" : "primary") : a.kind_override;
	std::string status = !a.page_exists ? "missing_manpage" : "needs_edit";
	if ( kind == "unresolved")
	    status = "unresolved_orphan";
	ofs << "| `" << a.reg.cmd << "` | `" << a.reg.source << "` | `"
	    << kind << "` | `" << status << "` | "
	    << notes.str( ) << " |\n";
    }

    ofs << "\n## Unresolved Mann Pages\n\n";
    bool any_unresolved = false;
    for ( const auto &it : audits) {
	const cmd_audit &a = it.second;
	if ( a.kind_override != "unresolved")
	    continue;
	any_unresolved = true;
	ofs << "- `" << a.page_name << ".adoc`: " << join_strings( a.notes, "; ") << "\n";
    }
    if ( !any_unresolved)
	ofs << "None.\n";

    if ( !warnings.empty( ))
	( void)warnings;
}

int
main( int argc, const char *argv[])
{
    fs::path root = fs::current_path( );
    fs::path inventory_out;
    fs::path progress_out;

    for ( int i = 1; i < argc; i++) {
	std::string arg( argv[i]);
	if ( arg == "--write-inventory" && i + 1 < argc) {
	    inventory_out = argv[++i];
	    continue;
	}
	if ( arg == "--write-progress" && i + 1 < argc) {
	    progress_out = argv[++i];
	    continue;
	}
	root = fs::path( arg);
    }

    fs::path srcroot = root / "src/libged";
    fs::path manndir = root / "doc/asciidoc/system/mann";
    fs::path manncmake = manndir / "CMakeLists.txt";

    if ( !fs::exists( srcroot) || !fs::exists( manndir) || !fs::exists( manncmake)) {
	std::cerr << "Unable to locate libged sources or mann docs under: " << root << "\n";
	return 1;
    }

    std::map<std::string, registered_cmd> commands;
    std::regex list_begin( R"( ^\s*#\s*define\s+( [A-Za-z_][A-Za-z0-9_]*)\s*\( \s*X\s*, \s*XID\s*\)\s*\\\s*$)");
    std::regex x_re( R"( \bX\s*\( \s*( [A-Za-z_][A-Za-z0-9_]*)\s*, \s*( [A-Za-z_][A-Za-z0-9_]*)\s*, \s*( [^\)]*GED_CMD[^\)]*)\))");
    std::regex xid_re( R"re( \bXID\s*\( \s*( [A-Za-z_][A-Za-z0-9_]*)\s*, \s*"( (?:\\.|[^"\\])*)"\s*, \s*( [A-Za-z_][A-Za-z0-9_]*)\s*, \s*( [^\)]*GED_CMD[^\)]*)\))re");

    for ( auto const &entry : fs::recursive_directory_iterator( srcroot)) {
	if ( !entry.is_regular_file( ))
	    continue;
	auto ext = entry.path( ).extension( ).string( );
	if ( ext != ".c" && ext != ".cpp")
	    continue;

	std::ifstream ifs( entry.path( ));
	if ( !ifs.good( ))
	    continue;

	std::string line;
	bool in_list = false;
	std::string rpath = fs::relative( entry.path( ), root).generic_string( );

	while ( std::getline( ifs, line)) {
	    if ( !in_list) {
		if ( std::regex_match( line, list_begin))
		    in_list = true;
		continue;
	    }

	    std::smatch m;
	    if ( std::regex_search( line, m, x_re)) {
		registered_cmd rc;
		rc.cmd = m[1].str( );
		rc.fn = m[2].str( );
		rc.source = rpath;
		commands[rc.cmd] = rc;
	    }
	    if ( std::regex_search( line, m, xid_re)) {
		registered_cmd rc;
		rc.cmd = decode_c_string( m[2].str( ));
		rc.fn = m[3].str( );
		rc.source = rpath;
		commands[rc.cmd] = rc;
	    }

	    if ( !trim( line).empty( ) && line.back( ) != '\\')
		in_list = false;
	}
    }

    std::set<std::string> docs;
    for ( auto const &entry : fs::directory_iterator( manndir)) {
	if ( !entry.is_regular_file( ) || entry.path( ).extension( ) != ".adoc")
	    continue;
	docs.insert( entry.path( ).stem( ).string( ));
    }

    std::set<std::string> cmake_listed;
    std::string cmake_txt = slurp( manncmake);
    std::regex cmake_adoc_re( R"( \n\s*( [^\s\)]+\.adoc))");
    for ( auto it = std::sregex_iterator( cmake_txt.begin( ), cmake_txt.end( ), cmake_adoc_re); it != std::sregex_iterator( ); ++it) {
	cmake_listed.insert( fs::path( (*it)[1].str( )).stem( ).string( ));
    }

    std::map<std::string, cmd_audit> audits;
    for ( const auto &cit : commands) {
	cmd_audit a;
	a.reg = cit.second;
	a.reg.origin = "libged";
	a.reg.strict = true;
	a.page_name = manpage_for_command( a.reg.cmd);
	a.options = extract_command_options( root / a.reg.source, a.reg.fn);
	a.strict = true;
	add_origin( a, "libged:" + a.reg.source + ":" + a.reg.fn);
	audit_page( a, manndir, docs, cmake_listed, true);
	audits[a.reg.cmd] = a;
    }

    auto add_tracked_command = [&]( const std::string &key, const std::string &cmd,
				   const std::string &page_name, const std::string &source,
				   const std::string &fn, const std::string &origin,
				   const std::string &kind, bool check_options) {
	bool created = ( audits.find( key) == audits.end( ));
	cmd_audit &a = audits[key];
	if ( created) {
	    a.reg.cmd = cmd;
	    a.reg.source = source;
	    a.reg.fn = fn;
	    a.reg.origin = kind;
	    a.page_name = page_name.empty( ) ? manpage_for_command( cmd) : page_name;
	    a.kind_override = kind;
	    a.options = ( source.empty( ) || fn.empty( )) ? std::set<std::string>( ) : extract_command_options( root / source, fn);
	    audit_page( a, manndir, docs, cmake_listed, check_options);
	} else {
	    if ( a.reg.source.empty( ))
		a.reg.source = source;
	    if ( a.reg.fn.empty( ))
		a.reg.fn = fn;
	    if ( !kind.empty( ) && a.kind_override.empty( ) && !a.strict)
		a.kind_override = kind;
	}
	add_origin( a, origin);
    };

    fs::path setup_file = root / "src/mged/setup.c";
    std::string setup_txt = slurp( setup_file);
    std::map<std::string, std::string> mged_fn_sources;
    fs::path mgedroot = root / "src/mged";
    if ( fs::exists( mgedroot)) {
	std::regex mged_fn_def_re( R"re( ^\s*( ?:int\s+|static\s+int\s+|HIDDEN\s+int\s+)?( (?:f|cmd)_[A-Za-z0-9_]+)\s*\( )re");
	for ( auto const &entry : fs::recursive_directory_iterator( mgedroot)) {
	    if ( !entry.is_regular_file( ))
		continue;
	    auto ext = entry.path( ).extension( ).string( );
	    if ( ext != ".c" && ext != ".cpp")
		continue;

	    std::ifstream ifs( entry.path( ));
	    std::string line;
	    std::string rpath = fs::relative( entry.path( ), root).generic_string( );
	    while ( std::getline( ifs, line)) {
		std::smatch m;
		if ( !std::regex_search( line, m, mged_fn_def_re))
		    continue;
		if ( mged_fn_sources.find( m[1].str( )) == mged_fn_sources.end( ))
		    mged_fn_sources[m[1].str( )] = rpath;
	    }
	}
    }

    std::regex mged_cmd_re( R"re( \{\s*MGED_CMD_MAGIC\s*, \s*( "( ?:\\.|[^"\\])*"|cmd3525|cmd4545)\s*, \s*( [A-Za-z_][A-Za-z0-9_]*)\s*, \s*( [^, \}]+))re");
    for ( auto it = std::sregex_iterator( setup_txt.begin( ), setup_txt.end( ), mged_cmd_re); it != std::sregex_iterator( ); ++it) {
	std::string cmd = decode_mged_cmd_name( (*it)[1].str( ));
	std::string fn = ( *it)[2].str( );
	std::string ged_fn = trim( (*it)[3].str( ));
	std::string source = "src/mged/setup.c";
	auto fit = mged_fn_sources.find( fn);
	if ( fit != mged_fn_sources.end( ))
	    source = fit->second;
	std::string origin = "mged_cmdtab:src/mged/setup.c:" + fn;
	if ( ged_fn != "GED_FUNC_PTR_NULL")
	    origin += "->" + ged_fn;
	add_tracked_command( cmd, cmd, manpage_for_command( cmd), source, fn, origin, "mged", false);
    }

    fs::path tclroot = root / "src/tclscripts";
    if ( fs::exists( tclroot)) {
	std::regex tcl_proc_re( R"re( ^\s*proc\s+( [^\s\{]+)\s+( ?:\{[^\}]*\}|[^\s\{]+)\s*\{)re");
	for ( auto const &entry : fs::recursive_directory_iterator( tclroot)) {
	    if ( !entry.is_regular_file( ))
		continue;
	    if ( entry.path( ).extension( ) != ".tcl" && entry.path( ).filename( ).string( ) != "rtwizard")
		continue;

	    std::string rpath = fs::relative( entry.path( ), root).generic_string( );
	    std::ifstream ifs( entry.path( ));
	    std::string line;
	    while ( std::getline( ifs, line)) {
		std::smatch m;
		if ( !std::regex_search( line, m, tcl_proc_re))
		    continue;
		std::string cmd = m[1].str( );
		std::string page = manpage_for_command( cmd);
		if ( docs.find( page) == docs.end( ))
		    continue;
		add_tracked_command( cmd, cmd, page, rpath, "proc " + cmd, "tcl_proc:" + rpath, "tcl_proc", false);
	    }
	}
    }

    std::vector<std::string> help_files = {
	"src/tclscripts/mged/help.tcl",
	"src/tclscripts/mged/helpdevel.tcl",
	"src/tclscripts/helplib.tcl"
    };
    std::regex help_data_re( R"re( set\s+( mged_help_data|mged_helpdevel_data|helplib_data)\( ([^\)]+)\))re");
    for ( const auto &hf : help_files) {
	std::string txt = slurp( root / hf);
	for ( auto it = std::sregex_iterator( txt.begin( ), txt.end( ), help_data_re); it != std::sregex_iterator( ); ++it) {
	    std::string table = ( *it)[1].str( );
	    std::string cmd = ( *it)[2].str( );
	    std::string page = manpage_for_command( cmd);
	    if ( docs.find( page) == docs.end( ))
		continue;
	    add_tracked_command( cmd, cmd, page, hf, table + "( " + cmd + ")", "tcl_help:" + hf + ":" + table, "tcl_help", false);
	}
    }

    struct support_doc {
	const char *page;
	const char *cmd;
	const char *source;
	const char *fn;
    };
    const support_doc support_docs[] = {
	{"bot_check", "bot check", "src/libged/bot/check.cpp", "_bot_cmd_check"},
	{"bot_extrude", "bot extrude", "src/libged/bot/extrude.cpp", "_bot_cmd_extrude"},
	{"bot_repair", "bot repair", "src/libged/bot/repair.cpp", "_bot_cmd_repair"},
	{"brep_info", "brep info", "src/libged/brep/info.cpp", "brep_info"},
	{"brep_intersect", "brep intersect", "src/libged/brep/brep.cpp", "_brep_cmd_intersect"},
	{"brep_plot", "brep plot", "src/libged/brep/plot.cpp", "brep_plot"},
	{"brep_selection", "brep selection", "src/libged/brep/brep.cpp", "_brep_cmd_selection"},
	{"edit_rotate", "edit rotate", "src/libged/edit/edit.cpp", "edit_rotate_cmd"},
	{"edit_scale", "edit scale", "src/libged/edit/edit.cpp", "edit_scale_cmd"},
	{"edit_translate", "edit translate", "src/libged/edit/edit.cpp", "edit_translate_cmd"}
    };
    for ( const auto &sd : support_docs) {
	if ( docs.find( sd.page) == docs.end( ))
	    continue;
	add_tracked_command( sd.cmd, sd.cmd, sd.page, sd.source, sd.fn,
			    std::string( "support_page:") + sd.source + ":" + sd.fn, "support", false);
    }

    std::set<std::string> referenced_docs;
    for ( const auto &it : audits)
	referenced_docs.insert( it.second.page_name);
    for ( const auto &doc : docs) {
	if ( doc == "Introduction" || doc == "mged_cmd_template")
	    continue;
	if ( referenced_docs.find( doc) != referenced_docs.end( ))
	    continue;

	cmd_audit a;
	a.reg.cmd = doc;
	a.reg.source = "doc/asciidoc/system/mann/" + doc + ".adoc";
	a.reg.fn = "";
	a.reg.origin = "unresolved_mann_page";
	a.page_name = doc;
	a.kind_override = "unresolved";
	a.page_exists = true;
	a.cmake_listed = cmake_listed.find( doc) != cmake_listed.end( );
	a.failures.push_back( "mann page has no libged, mged command table, documented Tcl proc, Tcl help-table, or known subcommand source match");
	if ( doc == "animmate")
	    a.notes.push_back( "historical documentation references exist, but no command/proc source was found under src");
	if ( doc == "opendb.new")
	    a.notes.push_back( "duplicate/staging copy of opendb.adoc; not listed in mann CMakeLists.txt");
	add_origin( a, "unresolved_mann_page:doc/asciidoc/system/mann/" + doc + ".adoc");
	audits[doc] = a;
    }

    for ( auto &it : audits) {
	cmd_audit &a = it.second;
	if ( !a.is_alias)
	    continue;
	if ( alias_loop_exists( a.reg.cmd, audits))
	    a.failures.push_back( "alias loop detected via target " + a.alias_target);
    }

    std::vector<std::string> warnings;

    if ( !inventory_out.empty( ))
	write_inventory( inventory_out, audits);
    if ( !progress_out.empty( ))
	write_progress( progress_out, audits, warnings);

    size_t failures = 0;
    std::map<std::string, std::vector<std::string> > src_failures;
    for ( const auto &it : audits) {
	const cmd_audit &a = it.second;
	if ( !a.strict)
	    continue;
	if ( a.failures.empty( ))
	    continue;
	for ( const auto &f : a.failures) {
	    failures++;
	    src_failures[a.reg.source].push_back( a.reg.cmd + ": " + f);
	}
    }

    if ( failures) {
	for ( const auto &sf : src_failures) {
	    std::cerr << sf.first << "\n";
	    for ( const auto &msg : sf.second)
		std::cerr << "  " << msg << "\n";
	}
	std::cerr << "mann validation failed with " << failures << " error( s)\n";
	return 1;
    }

    std::cout << "Validated " << commands.size( ) << " registered libged commands and tracked "
	      << audits.size( ) << " total command/manpage rows against "
	      << manndir.generic_string( ) << "\n";
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
