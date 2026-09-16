/*                     B R L M A N  . C P P
 * BRL-CAD
 *
 * Copyright (c) 2005-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file brlman.cpp
 *
 *  Man page viewer for BRL-CAD man pages.
 *
 */

#include "common.h"

#include <string.h>
#include <ctype.h> /* for isdigit */
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "bresource.h"

#include "bnetwork.h"
#include "bio.h"

#if !defined(USE_QT) && defined(HAVE_TK)
#  include "tcl.h"
#  include "tk.h"
#endif

#ifdef USE_QT
#include <QApplication>
#include <QObject>
#include <QFile>
#include <QDialog>
#include <QListWidget>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QSplitter>
#include <QDialogButtonBox>
#include "qtcad/QgAccordion.h"
#endif

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mapped_file.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"
#if !defined(USE_QT) && defined(HAVE_TK)
#  include "tclcad/setup.h"
#endif

/* Confine the constraining build conditions here - ultimately, we care
 * about graphical and non-graphical, whatever the reasons. */
#if defined(HAVE_TK) || defined(USE_QT)
#  define MAN_GUI 1
#endif
#ifndef HAVE_WINDOWS_H
#  define MAN_CMDLINE 1
#endif

/* Supported man sections */

struct man_page {
    std::string name;
    char section;
    std::string path;
    std::string summary;
    std::string synopsis;
    std::string text;
};

struct man_search_result {
    int score;
    size_t page;
};

static std::string
trim_copy(const std::string &s)
{
    size_t b = 0;
    size_t e = s.size();

    while (b < e && isspace((unsigned char)s[b]))
	b++;
    while (e > b && isspace((unsigned char)s[e - 1]))
	e--;

    return s.substr(b, e - b);
}


static std::string
lower_copy(const std::string &s)
{
    std::string ret = s;

    for (size_t i = 0; i < ret.size(); i++)
	ret[i] = (char)tolower((unsigned char)ret[i]);

    return ret;
}


static std::string
normalize_space(const std::string &s)
{
    std::string ret;
    bool inspace = false;

    for (size_t i = 0; i < s.size(); i++) {
	unsigned char c = (unsigned char)s[i];
	if (isspace(c)) {
	    if (!inspace && !ret.empty())
		ret.push_back(' ');
	    inspace = true;
	} else {
	    ret.push_back((char)c);
	    inspace = false;
	}
    }

    return trim_copy(ret);
}


static bool
read_file_to_string(const char *path, std::string &out)
{
    struct bu_mapped_file *mf = bu_open_mapped_file(path, NULL);
    if (!mf)
	return false;

    out.assign((const char *)mf->buf, mf->buflen);
    bu_close_mapped_file(mf);
    return true;
}


static std::string
clean_roff_inline(const std::string &s)
{
    std::string ret;

    for (size_t i = 0; i < s.size(); i++) {
	char c = s[i];

	if (c != '\\') {
	    ret.push_back(c);
	    continue;
	}

	if (i + 1 >= s.size())
	    continue;

	char n = s[i + 1];
	if (n == 'f' && i + 2 < s.size()) {
	    i += 2;
	    continue;
	}
	if (n == '(' && i + 3 < s.size()) {
	    std::string esc = s.substr(i + 2, 2);
	    if (esc == "em")
		ret.append("--");
	    else if (esc == "aq")
		ret.push_back('\'');
	    else if (esc == "en")
		ret.push_back('-');
	    i += 3;
	    continue;
	}
	if (n == '-' || n == 'e') {
	    ret.push_back(n == '-' ? '-' : '\\');
	    i++;
	    continue;
	}
	if (n == '&') {
	    i++;
	    continue;
	}
	if (isspace((unsigned char)n)) {
	    ret.push_back(' ');
	    i++;
	    continue;
	}

	ret.push_back(n);
	i++;
    }

    return ret;
}


static std::string
roff_macro_text(const std::string &line)
{
    if (line.size() < 2 || line[0] != '.')
	return clean_roff_inline(line);

    if (line.compare(0, 4, ".SH ") == 0)
	return clean_roff_inline(line.substr(4));
    if (line.compare(0, 4, ".SS ") == 0)
	return clean_roff_inline(line.substr(4));
    if (line.compare(0, 3, ".B ") == 0 || line.compare(0, 3, ".I ") == 0)
	return clean_roff_inline(line.substr(3));
    if (line.compare(0, 4, ".BR ") == 0 || line.compare(0, 4, ".IR ") == 0 ||
	line.compare(0, 4, ".BI ") == 0 || line.compare(0, 4, ".IB ") == 0)
	return clean_roff_inline(line.substr(4));

    return std::string();
}


static std::string
strip_roff(const std::string &raw)
{
    std::string ret;
    size_t pos = 0;

    while (pos < raw.size()) {
	size_t eol = raw.find('\n', pos);
	std::string line = (eol == std::string::npos) ? raw.substr(pos) : raw.substr(pos, eol - pos);
	std::string txt = roff_macro_text(line);

	if (!txt.empty()) {
	    ret.append(txt);
	    ret.push_back('\n');
	}

	if (eol == std::string::npos)
	    break;
	pos = eol + 1;
    }

    return normalize_space(ret);
}


static std::string
html_entity_decode(const std::string &s)
{
    std::string ret = s;
    struct repl {
	const char *from;
	const char *to;
    } reps[] = {
	{"&lt;", "<"},
	{"&gt;", ">"},
	{"&amp;", "&"},
	{"&quot;", "\""},
	{"&#34;", "\""},
	{"&#39;", "'"},
	{"&apos;", "'"},
	{"&nbsp;", " "},
	{NULL, NULL}
    };

    for (int i = 0; reps[i].from; i++) {
	size_t pos = 0;
	size_t flen = strlen(reps[i].from);
	while ((pos = ret.find(reps[i].from, pos)) != std::string::npos) {
	    ret.replace(pos, flen, reps[i].to);
	    pos += strlen(reps[i].to);
	}
    }

    return ret;
}


static std::string
strip_html(const std::string &raw)
{
    std::string filtered = raw;
    std::string lower = lower_copy(filtered);

    for (int pass = 0; pass < 2; pass++) {
	const char *open_tag = (pass == 0) ? "<style" : "<script";
	const char *close_tag = (pass == 0) ? "</style>" : "</script>";
	size_t pos = 0;
	while ((pos = lower.find(open_tag, pos)) != std::string::npos) {
	    size_t tag_end = lower.find('>', pos);
	    size_t close = lower.find(close_tag, (tag_end == std::string::npos) ? pos : tag_end);
	    size_t erase_end = (close == std::string::npos) ? ((tag_end == std::string::npos) ? pos + 1 : tag_end + 1) : close + strlen(close_tag);
	    filtered.erase(pos, erase_end - pos);
	    lower.erase(pos, erase_end - pos);
	}
    }

    std::string ret;
    bool intag = false;
    for (size_t i = 0; i < filtered.size(); i++) {
	char c = filtered[i];
	if (c == '<') {
	    intag = true;
	    ret.push_back(' ');
	    continue;
	}
	if (c == '>') {
	    intag = false;
	    ret.push_back(' ');
	    continue;
	}
	if (!intag)
	    ret.push_back(c);
    }

    return normalize_space(html_entity_decode(ret));
}


static std::string
extract_roff_section(const std::string &raw, const char *section)
{
    std::string ret;
    std::string want = lower_copy(section);
    bool active = false;
    size_t pos = 0;

    while (pos < raw.size()) {
	size_t eol = raw.find('\n', pos);
	std::string line = (eol == std::string::npos) ? raw.substr(pos) : raw.substr(pos, eol - pos);

	if (line.compare(0, 4, ".SH ") == 0) {
	    std::string heading = lower_copy(normalize_space(clean_roff_inline(line.substr(4))));
	    if (!heading.empty() && heading[0] == '"' && heading[heading.size() - 1] == '"')
		heading = heading.substr(1, heading.size() - 2);
	    active = (heading == want);
	} else if (active) {
	    std::string txt = roff_macro_text(line);
	    if (!txt.empty()) {
		ret.append(txt);
		ret.push_back('\n');
	    }
	}

	if (eol == std::string::npos)
	    break;
	pos = eol + 1;
    }

    return normalize_space(ret);
}


static std::string
extract_html_section(const std::string &raw, const char *section)
{
    std::string lower = lower_copy(raw);
    std::string id = std::string("id=\"_") + lower_copy(section) + "\"";
    size_t idpos = lower.find(id);
    if (idpos == std::string::npos)
	return std::string();

    size_t h2end = lower.find("</h2>", idpos);
    if (h2end == std::string::npos)
	return std::string();
    h2end += 5;

    size_t next = lower.find("<h2", h2end);
    std::string section_html = (next == std::string::npos) ? raw.substr(h2end) : raw.substr(h2end, next - h2end);

    return strip_html(section_html);
}


static std::string
page_section_text(const std::string &raw, const char *section, int html)
{
    return html ? extract_html_section(raw, section) : extract_roff_section(raw, section);
}


static std::string
page_plain_text(const std::string &raw, int html)
{
    return html ? strip_html(raw) : strip_roff(raw);
}


static std::string
summary_from_name_section(const std::string &name_section)
{
    std::string s = normalize_space(name_section);
    size_t pos = s.find(" - ");

    if (pos != std::string::npos)
	return trim_copy(s.substr(pos + 3));

    pos = s.find("-");
    if (pos != std::string::npos)
	return trim_copy(s.substr(pos + 1));

    return s;
}


static std::vector<std::string>
search_terms(const char *query)
{
    std::vector<std::string> terms;
    std::string term;

    if (!query)
	return terms;

    for (size_t i = 0; query[i] != '\0'; i++) {
	unsigned char c = (unsigned char)query[i];
	if (isalnum(c)) {
	    term.push_back((char)tolower(c));
	} else if (!term.empty()) {
	    terms.push_back(term);
	    term.clear();
	}
    }
    if (!term.empty())
	terms.push_back(term);

    return terms;
}


static int
term_count(const std::string &haystack, const std::string &needle)
{
    int cnt = 0;
    size_t pos = 0;

    if (needle.empty())
	return 0;

    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
	cnt++;
	pos += needle.size();
    }

    return cnt;
}


static int
weighted_match_score(const std::vector<std::string> &terms, const std::vector<std::pair<int, std::string> > &fields)
{
    int score = 0;

    for (size_t i = 0; i < terms.size(); i++) {
	int term_score = 0;

	for (size_t j = 0; j < fields.size(); j++) {
	    int cnt = term_count(fields[j].second, terms[i]);
	    if (cnt > 0) {
		if (cnt > 20)
		    cnt = 20;
		term_score += fields[j].first * cnt;
	    }
	}

	if (term_score == 0)
	    return -1;

	score += term_score;
    }

    return score;
}


static int
section_sort_order(char section)
{
    switch (section) {
	case '1':
	    return 1;
	case '3':
	    return 3;
	case '5':
	    return 5;
	case 'n':
	    return 9;
	default:
	    return 99;
    }
}


static void
man_file_info(char section, int gui, std::string &dir, std::string &ext)
{
    if (section == 'n') {
	dir = "mann";
	ext = gui ? "html" : "nged";
    } else {
	dir = std::string("man") + section;
	ext = gui ? "html" : std::string(1, section);
    }
}


static bool
man_section_dir(struct bu_vls *mdir, const char *lang, char section, int gui)
{
    const char *ddir = NULL;
    std::string dir;
    std::string ext;

    man_file_info(section, gui, dir, ext);
    ddir = gui ? bu_dir(NULL, 0, BU_DIR_DOC, "html", NULL) : bu_dir(NULL, 0, BU_DIR_MAN, NULL);

    bu_vls_sprintf(mdir, "%s/%s/%s", ddir, lang, dir.c_str());
    if (bu_file_directory(bu_vls_cstr(mdir)))
	return true;

    bu_vls_sprintf(mdir, "%s/%s", ddir, dir.c_str());
    return bu_file_directory(bu_vls_cstr(mdir));
}


static size_t
collect_man_pages(std::vector<man_page> &pages, const char *lang, char wanted_section, int gui, int include_full_text)
{
    const char sections[] = BRLCAD_MAN_SECTIONS;
    size_t total = 0;

    for (size_t si = 0; sections[si] != '\0'; si++) {
	char section = sections[si];
	if (wanted_section != '\0' && wanted_section != section)
	    continue;

	std::string dir;
	std::string ext;
	struct bu_vls mdir = BU_VLS_INIT_ZERO;
	struct bu_vls pattern = BU_VLS_INIT_ZERO;
	char **mfiles = NULL;
	size_t mcnt = 0;

	man_file_info(section, gui, dir, ext);
	if (!man_section_dir(&mdir, lang, section, gui)) {
	    bu_vls_free(&mdir);
	    continue;
	}

	bu_vls_sprintf(&pattern, "*.%s", ext.c_str());
	mcnt = bu_file_list(bu_vls_cstr(&mdir), bu_vls_cstr(&pattern), &mfiles);

	for (size_t i = 0; i < mcnt; i++) {
	    struct bu_vls mname = BU_VLS_INIT_ZERO;
	    struct bu_vls mpath = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&mpath, "%s/%s", bu_vls_cstr(&mdir), mfiles[i]);
	    if (bu_path_component(&mname, mfiles[i], BU_PATH_BASENAME_EXTLESS)) {
		std::string raw;
		if (read_file_to_string(bu_vls_cstr(&mpath), raw)) {
		    man_page mp;
		    mp.name = bu_vls_cstr(&mname);
		    mp.section = section;
		    mp.path = bu_vls_cstr(&mpath);
		    mp.summary = summary_from_name_section(page_section_text(raw, "name", gui));
		    mp.synopsis = page_section_text(raw, "synopsis", gui);
		    if (include_full_text)
			mp.text = page_plain_text(raw, gui);
		    pages.push_back(mp);
		    total++;
		}
	    }
	    bu_vls_free(&mpath);
	    bu_vls_free(&mname);
	}

	if (mcnt)
	    bu_argv_free(mcnt, mfiles);
	bu_vls_free(&pattern);
	bu_vls_free(&mdir);
    }

    return total;
}


static int
search_man_pages(const char *query, const char *lang, char section, int full_text)
{
    std::vector<man_page> pages;
    std::vector<man_search_result> results;
    std::vector<std::string> terms = search_terms(query);

    if (terms.empty())
	return BRLCAD_ERROR;

#ifdef MAN_CMDLINE
    int gui_docs = 0;
#else
    int gui_docs = 1;
#endif

    collect_man_pages(pages, lang, section, gui_docs, full_text);
    for (size_t i = 0; i < pages.size(); i++) {
	std::vector<std::pair<int, std::string> > fields;
	int score = -1;

	fields.push_back(std::make_pair(200, lower_copy(pages[i].name)));
	fields.push_back(std::make_pair(80, lower_copy(pages[i].summary)));
	fields.push_back(std::make_pair(40, lower_copy(pages[i].synopsis)));
	if (full_text)
	    fields.push_back(std::make_pair(8, lower_copy(pages[i].text)));

	score = weighted_match_score(terms, fields);
	if (score >= 0) {
	    man_search_result r;
	    r.score = score;
	    r.page = i;
	    results.push_back(r);
	}
    }

    struct result_less {
	const std::vector<man_page> *pages;
	bool operator()(const man_search_result &a, const man_search_result &b) const {
	    const man_page &ap = (*pages)[a.page];
	    const man_page &bp = (*pages)[b.page];

	    if (a.score != b.score)
		return a.score > b.score;
	    if (section_sort_order(ap.section) != section_sort_order(bp.section))
		return section_sort_order(ap.section) < section_sort_order(bp.section);
	    return ap.name < bp.name;
	}
    } less_results;
    less_results.pages = &pages;

    std::stable_sort(results.begin(), results.end(), less_results);

    if (results.empty()) {
	bu_log("No matches found for \"%s\".\n", query);
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < results.size(); i++) {
	const man_page &mp = pages[results[i].page];
	if (!mp.summary.empty()) {
	    bu_log("%s(%c) - %s\n", mp.name.c_str(), mp.section, mp.summary.c_str());
	} else {
	    bu_log("%s(%c)\n", mp.name.c_str(), mp.section);
	}
    }

    return BRLCAD_OK;
}

static char *
find_man_file(const char *man_name, const char *lang, char section, int gui)
{
    const char *ddir;
    char *ret = NULL;
    struct bu_vls data_dir = BU_VLS_INIT_ZERO;
    struct bu_vls file_ext = BU_VLS_INIT_ZERO;
    struct bu_vls mfile = BU_VLS_INIT_ZERO;
    if (gui) {
	bu_vls_sprintf(&file_ext, "html");
	ddir = bu_dir(NULL, 0, BU_DIR_DOC, "html", NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    } else {
	if (section != 'n') {
	    bu_vls_sprintf(&file_ext, "%c", section);
	} else {
	    bu_vls_sprintf(&file_ext, "nged");
	}
	ddir = bu_dir(NULL, 0, BU_DIR_MAN, NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    }
    bu_vls_sprintf(&mfile, "%s/%s/man%c/%s.%s", bu_vls_addr(&data_dir), lang, section, man_name, bu_vls_addr(&file_ext));
    if (bu_file_exists(bu_vls_addr(&mfile), NULL)) {
	ret = bu_strdup(bu_vls_addr(&mfile));
    } else {
	bu_vls_sprintf(&mfile, "%s/man%c/%s.%s", bu_vls_addr(&data_dir), section, man_name, bu_vls_addr(&file_ext));
	if (bu_file_exists(bu_vls_addr(&mfile), NULL)) {
	    ret = bu_strdup(bu_vls_addr(&mfile));
	}
    }
    bu_vls_free(&data_dir);
    bu_vls_free(&file_ext);
    bu_vls_free(&mfile);
    return ret;
}


#if !defined(USE_QT) && defined(HAVE_TK)
// Tk based GUI
int
tk_man_gui(const char *man_name, char man_section, const char *man_file)
{
    const char *result;
    const char *fullname;
    const char *tcl_man_file;
    const char *brlman_tcl = NULL;
    struct bu_vls tlog = BU_VLS_INIT_ZERO;
    Tcl_DString temp;
    Tcl_Interp *interp = Tcl_CreateInterp();
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

#ifdef HAVE_WINDOWS_H
    Tk_InitConsoleChannels(interp);
#endif

    int status = tclcad_init(interp, 1, &tlog);

    if (status == TCL_ERROR) {
	bu_log("brlman tclcad init failure:\n%s\n", bu_vls_addr(&tlog));
	bu_vls_free(&tlog);
	bu_exit(1, "tcl init error");
    }
    bu_vls_free(&tlog);

    /* Have gui flag and specified man page but no file found - graphical failure notice */
    if (man_name && !man_file) {
	if (man_section != '\0') {
	    bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error: man page not found for %s in section %c\" -type ok", man_name, man_section);
	} else {
	    bu_vls_sprintf(&tcl_cmd, "tk_messageBox -message \"Error: man page not found for %s\" -type ok", man_name);
	}
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	bu_exit(EXIT_FAILURE, NULL);
    }

    /* Pass the key variables into the interp */
    if (man_section != '\0') {
	bu_vls_sprintf(&tcl_cmd, "set ::section_number %c", man_section);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (man_file) {
	Tcl_Obj *pathP, *norm;

	bu_vls_sprintf(&tcl_cmd, "set ::man_name %s", man_name);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

	pathP = Tcl_NewStringObj(man_file, -1);
	norm = Tcl_FSGetTranslatedPath(interp, pathP);
	tcl_man_file = Tcl_GetString(norm);
	bu_vls_sprintf(&tcl_cmd, "set ::man_file %s", tcl_man_file);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    } else {
	bu_vls_sprintf(&tcl_cmd, "set ::data_dir %s/html", bu_dir(NULL, 0, BU_DIR_DOC, NULL));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    brlman_tcl = bu_dir(NULL, 0, BU_DIR_DATA, "tclscripts", "brlman", "brlman.tcl", NULL);
    Tcl_DStringInit(&temp);
    fullname = Tcl_TranslateFileName(interp, brlman_tcl, &temp);
    status = Tcl_EvalFile(interp, fullname);
    Tcl_DStringFree(&temp);

    result = Tcl_GetStringResult(interp);
    if (strlen(result) > 0 && status == TCL_ERROR) {
	bu_log("%s\n", result);
    }
    Tcl_DeleteInterp(interp);

    return BRLCAD_OK;
}
#endif

#ifdef USE_QT

static size_t
list_man_files(QListWidget *l, const char *lang, char section, int gui)
{
    const char *ddir;
    struct bu_vls data_dir = BU_VLS_INIT_ZERO;
    struct bu_vls file_ext = BU_VLS_INIT_ZERO;
    struct bu_vls mpattern = BU_VLS_INIT_ZERO;
    struct bu_vls mdir = BU_VLS_INIT_ZERO;
    if (gui) {
	bu_vls_sprintf(&file_ext, "html");
	ddir = bu_dir(NULL, 0, BU_DIR_DOC, "html", NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    } else {
	if (section != 'n') {
	    bu_vls_sprintf(&file_ext, "%c", section);
	} else {
	    bu_vls_sprintf(&file_ext, "nged");
	}
	ddir = bu_dir(NULL, 0, BU_DIR_MAN, NULL);
	bu_vls_sprintf(&data_dir, "%s", ddir);
    }

    bu_vls_sprintf(&mdir, "%s/%s/man%c", bu_vls_addr(&data_dir), lang, section);
    if (!bu_file_directory(bu_vls_addr(&mdir))) {
	bu_vls_sprintf(&mdir, "%s/man%c", bu_vls_addr(&data_dir), section);
	if (!bu_file_directory(bu_vls_addr(&mdir))) {
	    return 0;
	}
    }

    bu_vls_sprintf(&mpattern, "*.%s", bu_vls_cstr(&file_ext));

    char **mfiles = NULL;
    size_t mcnt = bu_file_list(bu_vls_cstr(&mdir), bu_vls_cstr(&mpattern), &mfiles);

    bu_vls_free(&data_dir);
    bu_vls_free(&mpattern);
    bu_vls_free(&file_ext);
    bu_vls_free(&mdir);

    if (mcnt) {
	for (size_t i = 0; i < mcnt; i++) {
	    struct bu_vls mname = BU_VLS_INIT_ZERO;
	    if (bu_path_component(&mname, mfiles[i], BU_PATH_BASENAME_EXTLESS)) {
		new QListWidgetItem(bu_vls_cstr(&mname), l);
	    }
	    bu_vls_free(&mname);
	}
    }

    return mcnt;
}

class ManViewer : public QDialog
{
    public:
	ManViewer(QWidget *p = NULL, const char *man_name = NULL, char man_section = '0', const char *lang = NULL);
	~ManViewer();

	void do_select(const char *man_name, char man_section);

	QListWidget *l1;
	QListWidget *l3;
	QListWidget *l5;
	QListWidget *ln;

    public slots:
        void do_man1(QListWidgetItem *i, QListWidgetItem *p);
        void do_man3(QListWidgetItem *i, QListWidgetItem *p);
        void do_man5(QListWidgetItem *i, QListWidgetItem *p);
        void do_mann(QListWidgetItem *i, QListWidgetItem *p);

    private:
	struct bu_vls mlang = BU_VLS_INIT_ZERO;
	QgAccordion *lists;
	QTextBrowser *browser;
	QDialogButtonBox *buttons;
};

void
ManViewer::do_man1(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, '1');
}

void
ManViewer::do_man3(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, '3');
}

void
ManViewer::do_man5(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, '5');
}

void
ManViewer::do_mann(QListWidgetItem *i, QListWidgetItem *UNUSED(p))
{
    QString mpage = i->text();
    const char *man_name = mpage.toLocal8Bit();
    do_select(man_name, 'n');
}

void
ManViewer::do_select(const char *man_name, char man_section)
{
    const char *man_file = find_man_file(man_name, bu_vls_cstr(&mlang), man_section, 1);
    if (!man_file)
	return;

    struct bu_vls title = BU_VLS_INIT_ZERO;
    if (man_name) {
	bu_vls_sprintf(&title, "%s (%c)", man_name, man_section);
    } else {
	bu_vls_sprintf(&title, "BRL-CAD Manual Pages");
    }
    this->setWindowTitle(QString(bu_vls_cstr(&title)));
    bu_vls_free(&title);

    QString filename(man_file);
    QFile manfile(filename);
    if (manfile.open(QFile::ReadOnly | QFile::Text)) {
	browser->setHtml(manfile.readAll());
    }
}

ManViewer::ManViewer(QWidget *pparent, const char *man_name, char man_section, const char *lang) : QDialog(pparent)
{
    bu_vls_sprintf(&mlang, "%s", lang);

    QVBoxLayout *l = new QVBoxLayout;
    buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &ManViewer::accept);

    QSplitter *s = new QSplitter(Qt::Horizontal, this);
    lists = new QgAccordion();
    s->addWidget(lists);

    l1 = new QListWidget(this);
    list_man_files(l1, bu_vls_cstr(&mlang), '1', 1);
    l1->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *a1 = new QgAccordionObject(lists, l1, "Programs (man1)");
    lists->addObject(a1);
    QObject::connect(l1, &QListWidget::currentItemChanged, this, &ManViewer::do_man1);

    l3 = new QListWidget(this);
    list_man_files(l3, bu_vls_cstr(&mlang), '3', 1);
    l3->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *a3 = new QgAccordionObject(lists, l3, "Libraries (man3)");
    lists->addObject(a3);
    QObject::connect(l3, &QListWidget::currentItemChanged, this, &ManViewer::do_man3);

    l5 = new QListWidget(this);
    list_man_files(l5, bu_vls_cstr(&mlang), '5', 1);
    l5->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *a5 = new QgAccordionObject(lists, l5, "Conventions (man5)");
    lists->addObject(a5);
    QObject::connect(l5, &QListWidget::currentItemChanged, this, &ManViewer::do_man5);

    ln = new QListWidget(this);
    list_man_files(ln, bu_vls_cstr(&mlang), 'n', 1);
    ln->setSizeAdjustPolicy(QListWidget::AdjustToContents);
    QgAccordionObject *an = new QgAccordionObject(lists, ln, "GED (mann)");
    lists->addObject(an);
    QObject::connect(ln, &QListWidget::currentItemChanged, this, &ManViewer::do_mann);

    switch (man_section) {
	case '1':
	    emit a1->select(a1);
	    break;
	case '3':
	    emit a3->select(a3);
	    break;
	case '5':
	    emit a5->select(a5);
	    break;
	case 'n':
	    emit an->select(an);
	    break;
	default:
	    emit a1->select(a1);
	    break;
    }

    browser = new QTextBrowser();

    if (!man_name) {
	do_select("Introduction", 'n');
    } else {
	do_select(man_name, man_section);
    }

    s->addWidget(browser);

    s->setStretchFactor(0, 0);
    s->setStretchFactor(1, 100000);

    l->addWidget(s);

    l->addWidget(buttons);
    setLayout(l);


    struct bu_vls title = BU_VLS_INIT_ZERO;
    if (man_name) {
	bu_vls_sprintf(&title, "%s (%c)", man_name, man_section);
    } else {
	bu_vls_sprintf(&title, "BRL-CAD Manual Pages");
    }
    this->setWindowTitle(QString(bu_vls_cstr(&title)));
    bu_vls_free(&title);

    // TODO - find something smarter to do for size
    setMinimumWidth(800);
    setMinimumHeight(600);

    setWindowModality(Qt::NonModal);
}

ManViewer::~ManViewer()
{
    delete browser;
}


int
qt_man_gui(const char *man_name, char man_section, const char *UNUSED(man_file))
{
    int ac = 0;
    char **av = NULL;
    QApplication brlman(ac, av);
    ManViewer *v = new ManViewer(NULL, man_name, man_section);
    v->show();
    brlman.exec();
    return BRLCAD_OK;
}
#endif

#ifndef HAVE_WINDOWS_H
#  define APIENTRY
#  define BRLMAN_MAIN main
#else
#  define BRLMAN_MAIN WinMain
#endif

/* main() */
int APIENTRY
BRLMAN_MAIN(
#ifdef HAVE_WINDOWS_H
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow
#else
    int argc,
    const char **argv
#endif
    )
{

#if !defined(MAN_CMDLINE) && !defined(MAN_GUI)
    bu_exit(EXIT_FAILURE, "Error: man page display is not supported.");
#endif

    int i = 0;
    int status = BRLCAD_ERROR;
    int uac = 0;
#ifndef MAN_CMDLINE
    int enable_gui = 1;
#else
    int enable_gui = 0;
#endif
    int disable_gui = 0;
    int print_help = 0;
    int search_mode = 0;
    const char *short_search = NULL;
    const char *full_search = NULL;
    const char *man_cmd = NULL;
    const char *man_name = NULL;
    const char *man_file = NULL;
    char man_section = '\0';
    struct bu_vls search_query = BU_VLS_INIT_ZERO;
    struct bu_vls lang = BU_VLS_INIT_ZERO;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[8];

#ifdef HAVE_WINDOWS_H
    const char **argv;
    int argc;

    /* Get our args from the c-runtime. Ignore lpszCmdLine. */
    argc = __argc;
    argv = (const char **)__argv;
#endif

    /* initialize progname for run-time resource finding */
    bu_setprogname(argv[0]);

    /* Handle options in C */
    BU_OPT(d[0], "h", "help",        "",                NULL, &print_help,  "Print help and exit");
    BU_OPT(d[1], "g", "gui",         "",                NULL, &enable_gui,  "Enable GUI");
    BU_OPT(d[2], "",  "no-gui",      "",                NULL, &disable_gui, "Disable GUI");
    BU_OPT(d[3], "L", "language",  "lg",        &bu_opt_lang, &lang,        "Set language");
    BU_OPT(d[4], "S", "section",    "#", &bu_opt_man_section, &man_section, "Set section");
    BU_OPT(d[5], "k", "keyword",    "kw",          &bu_opt_str, &short_search, "Search names, synopses, and brief descriptions");
    BU_OPT(d[6], "K", "full-text",  "kw",          &bu_opt_str, &full_search,  "Search full manual page text");
    BU_OPT_NULL(d[7]);

    /* Skip first arg */
    argv++; argc--;
    uac = bu_opt_parse(&optparse_msg, argc, argv, d);
    if (uac == -1) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    /* If we want help, print help */
    if (print_help) {
	char *option_help = bu_opt_describe(d, NULL);
	bu_log("Usage: brlman [options] [man_page]\n");
	if (option_help) {
	    bu_log("Options:\n%s\n", option_help);
	}
	bu_free(option_help, "help str");
	bu_exit(EXIT_SUCCESS, NULL);
    }

    if (short_search && full_search) {
	bu_exit(EXIT_FAILURE, "Error: specify only one of -k or -K\n");
    }
    if (short_search) {
	search_mode = 1;
	bu_vls_sprintf(&search_query, "%s", short_search);
    }
    if (full_search) {
	search_mode = 2;
	bu_vls_sprintf(&search_query, "%s", full_search);
    }

    if (search_mode && uac > 0) {
	for (i = 0; i < uac; i++) {
	    bu_vls_printf(&search_query, " %s", argv[i]);
	}
	uac = 0;
    }

    /* If we only have one non-option arg, assume it's the man name */
    if (uac == 1) {
	man_name = argv[0];
    } else {
	/* If we've got more, check for a section number. */
	for (i = 0; i < uac; i++) {
	    if (strlen(argv[i]) == 1 && (isdigit(argv[i][0]) || argv[i][0] == 'n')) {

		/* Record the section */
		man_section = argv[i][0];

		/* If we have a section identifier and it's not already the last
		 * element in argv, adjust the array */
		if (i < uac - 1) {
		    const char *tmp = argv[uac-1];
		    argv[uac-1] = argv[i];
		    argv[i] = tmp;
		}

		/* Found a number and processed - one less argv entry. */
		uac--;

		/* First number wins. */
		break;
	    }
	}

	/* For now, only support specifying one man page at a time */
	if (uac > 1) {
	    bu_exit(EXIT_FAILURE, "Error - need a single man page name");
	}

	/* If we have one, use it.  Zero is allowed if we're in graphical
	 * mode, so uac == at this point isn't necessarily a failure */
	if (uac == 1) {
	    man_name = argv[0];
	}
    }

    if (enable_gui && disable_gui) {
#if defined(MAN_GUI) && defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform supports both, enabling GUI.\n");
	disable_gui = 0;
#endif
#if !defined(MAN_GUI) && defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform only supports command line, disabling GUI.\n");
#endif
#if defined(MAN_GUI) && !defined(MAN_CMDLINE)
	bu_log("Warning - gui explicitly enabled *and* disabled? - platform only supports GUI viewer, enabling GUI.\n");
#endif
    }

    /* If we're not graphical and we don't have a name or search at this point, we're done. */
    if (!enable_gui && !man_name && !search_mode) {
	bu_exit(EXIT_FAILURE, "Error: Please specify man page");
    }

    /* If we're not graphical, make sure we have a man command to use for page display. */
    if (!enable_gui && !search_mode) {
#ifndef MAN_CMDLINE
	bu_exit(EXIT_FAILURE, "Error: Non-graphical man display is not supported.");
#else
	man_cmd = bu_which("man");
	if (!man_cmd) {
	    if (disable_gui) {
		bu_exit(EXIT_FAILURE, "Error: Graphical man display is disabled, but no man command is available.");
	    } else {
		/* Don't have any specific instructions - fall back on gui */
		bu_log("Warning: Graphical mode was not specified, but no man command is available - enabling GUI.");
		enable_gui = 1;
	    }
	}
#endif
    }

    /* If we don't have a language, check environment */
    if (!bu_vls_strlen(&lang)) {
	char *env_lang = getenv("LANG");
	if (env_lang && strlen(env_lang) >= 2) {
	    /* Grab the first two characters */
	    bu_vls_strncat(&lang, env_lang, 2);
	}
    }

    /* If we *still* don't have a language, use en */
    if (!bu_vls_strlen(&lang)) {
	bu_vls_sprintf(&lang, "en");
    }

    if (search_mode) {
	status = search_man_pages(bu_vls_cstr(&search_query), bu_vls_cstr(&lang), man_section, (search_mode == 2));
	bu_vls_free(&lang);
	bu_vls_free(&search_query);
	return (status == BRLCAD_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* Now we have a lang and name - find the file */

    if (man_section != '\0') {
	man_file = find_man_file(man_name, bu_vls_addr(&lang), man_section, enable_gui);
    } else {
	const char sections[] = BRLCAD_MAN_SECTIONS;
	i = 0;
	while(sections[i] != '\0') {
	    man_file = find_man_file(man_name, bu_vls_addr(&lang), sections[i], enable_gui);
	    if (man_file) {
		man_section = sections[i];
		break;
	    }
	    i++;
	}
    }

    if (!enable_gui) {
#ifdef MAN_CMDLINE
	if (!man_file) {
	    if (man_section != '\0') {
		bu_exit(EXIT_FAILURE, "No man page found for %s in section %c\n", man_name, man_section);
	    } else {
		bu_exit(EXIT_FAILURE, "No man page found for %s\n", man_name);
	    }
	}
	/* Note - for reasons that are unclear, passing in man_cmd as the first
	 * argument to execlp will *not* work... */
	status = execlp("man", man_cmd, man_file, (char *)NULL);
	return status;
#else
	/* Shouldn't get here - should be caught by above tests */
	bu_exit(EXIT_FAILURE, "Error: Non-graphical man display is not supported.");
#endif
    }

#ifdef MAN_GUI
#  if !defined(USE_QT) && defined(HAVE_TK)
    return tk_man_gui(man_name, man_section, man_file);
#  else
    return qt_man_gui(man_name, man_section, man_file);
#  endif
#else
    /* Shouldn't get here - should be caught by above tests */
    bu_exit(EXIT_FAILURE, "Error: graphical man display is not supported.");
#endif
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
