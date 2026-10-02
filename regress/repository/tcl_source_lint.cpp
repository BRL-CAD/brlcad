/*                T C L _ S O U R C E _ L I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The name of the author may not be used to endorse or promote
 *      products derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file tcl_source_lint.cpp
 *
 * Statically find first-party Tcl calls to reviewed compatibility commands.
 * Tcl's parser supplies word boundaries and nested-command tokens; this
 * scanner adds the script-argument and namespace knowledge needed to tell a
 * command invocation from a command name used as data.
 */

#include "common.h"

#include <tcl.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bu/app.h"
#include "bu/str.h"


namespace {

const size_t MAX_PARSE_DEPTH = 256;


enum class ParsePass {
    COLLECT_DEFINITIONS,
    CHECK_CALLS
};


struct AliasInfo {
    std::string target;
    /* Calls in the file defining a scripted compatibility wrapper remain
     * compatibility calls; unrelated same-named procedures are shadows. */
    std::set<std::string> definition_sources;
    bool mged_implementation = false;
};


struct Scope {
    std::string name = "::";
    bool known = true;
    bool class_body = false;
};


struct Finding {
    std::string source;
    size_t line = 0;
    std::string command;
    std::string target;
    bool mged_implementation = false;
};


struct ParseIssue {
    std::string source;
    size_t line = 0;
    std::string message;
};


struct SourceFile {
    std::filesystem::path path;
    std::string relative_path;
    std::string contents;
    std::vector<size_t> line_starts;
    std::set<std::string> definitions;

    size_t
    line_number(const char *position) const
    {
	if (!position || contents.empty())
	    return 1;
	const char *begin = contents.data();
	const char *end = begin + contents.size();
	if (position < begin || position > end)
	    return 1;
	size_t offset = static_cast<size_t>(position - begin);
	auto it = std::upper_bound(line_starts.begin(), line_starts.end(), offset);
	return static_cast<size_t>(it - line_starts.begin());
    }
};


struct Word {
    Tcl_Token *token = nullptr;
    bool literal = false;
    std::string value;
    const char *content_start = nullptr;
    int content_size = 0;
};


static std::string
trim(const std::string &input)
{
    size_t first = 0;
    while (first < input.size() &&
	std::isspace(static_cast<unsigned char>(input[first])))
	first++;

    size_t last = input.size();
    while (last > first &&
	std::isspace(static_cast<unsigned char>(input[last - 1])))
	last--;

    return input.substr(first, last - first);
}


static bool
starts_with(const std::string &value, const std::string &prefix)
{
    return value.size() >= prefix.size() &&
	value.compare(0, prefix.size(), prefix) == 0;
}


static bool
ends_with(const std::string &value, const std::string &suffix)
{
    return value.size() >= suffix.size() &&
	value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}


static std::string
lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
	[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}


static std::string
normalize_command(const std::string &command)
{
    if (starts_with(command, "::"))
	return command;
    return "::" + command;
}


static std::string
without_root_prefix(const std::string &command)
{
    return starts_with(command, "::") ? command.substr(2) : command;
}


static bool
command_is(const std::string &command, const char *name)
{
    return without_root_prefix(command) == name;
}


static std::string
qualify_name(const std::string &name, const Scope &scope)
{
    if (starts_with(name, "::"))
	return name;
    if (!scope.known)
	return std::string();
    if (scope.name == "::")
	return "::" + name;
    return scope.name + "::" + name;
}


static std::string
command_namespace(const std::string &command)
{
    size_t separator = command.rfind("::");
    if (separator == std::string::npos || separator == 0)
	return "::";
    return command.substr(0, separator);
}


static bool
path_prefix_matches(const std::string &path, const std::string &prefix)
{
    return path == prefix ||
	(path.size() > prefix.size() && starts_with(path, prefix) &&
	 path[prefix.size()] == '/');
}


static bool
is_tcl_source(const std::string &relative_path)
{
    std::string path = lowercase(relative_path);
    return ends_with(path, ".tcl") || ends_with(path, ".tk") ||
	ends_with(path, ".itcl") || ends_with(path, ".itk") ||
	ends_with(path, ".tcl.in");
}


class TclSourceLint;


class SourceParser {
public:
    SourceParser(TclSourceLint &lint, SourceFile &source, ParsePass pass);

    void parse();

private:
    TclSourceLint &lint;
    SourceFile &source;
    ParsePass pass;

    void parse_script(const char *script, size_t length, const Scope &scope,
	bool class_body, size_t depth);
    void parse_expression(const Word &word, const Scope &scope, size_t depth);
    void process_command(const Tcl_Parse &parse, const std::vector<Word> &words,
	const Scope &scope, bool class_body, size_t depth);
    void process_class_member(const std::vector<Word> &words, size_t command_index,
	const Scope &scope, size_t depth);
    void process_switch(const std::vector<Word> &words, const Scope &scope,
	size_t depth);
    void process_try(const std::vector<Word> &words, const Scope &scope,
	size_t depth);
    void process_callback_options(const std::vector<Word> &words,
	const Scope &scope, size_t depth);

    void parse_script_word(const Word &word, const Scope &scope,
	bool class_body, size_t depth);
    void parse_list_body(const Word &word, size_t element_index,
	const Scope &scope, bool class_body, size_t depth);
    void inspect_command_prefix(const Word &word, const Scope &scope,
	size_t depth);
    void inspect_list_builder(const Tcl_Token &token, const Scope &scope,
	size_t depth);
    void recurse_command_tokens(const Tcl_Parse &parse, const Scope &scope,
	size_t depth);

    std::vector<Word> words(const Tcl_Parse &parse) const;
    void define_command(const std::string &name, const Scope &scope);
    void define_class(const std::string &name, const Scope &scope);
    void check_command(const Word &word, const Scope &scope);
    void check_command_name(const std::string &name, const char *location,
	const Scope &scope);
    void parse_error(const char *location, const char *context);
};


class TclSourceLint {
public:
    TclSourceLint()
    {
	interp = Tcl_CreateInterp();
    }

    ~TclSourceLint()
    {
	if (interp)
	    Tcl_DeleteInterp(interp);
    }

    bool
    valid() const
    {
	return interp != nullptr;
    }

    bool load_manifest(const std::filesystem::path &manifest_path);
    bool load_sources(const std::filesystem::path &file_list,
	const std::filesystem::path &source_root);
    bool load_one_source(const std::filesystem::path &path,
	const std::string &relative_path);
    int run(bool report = true);
    int self_test(const std::filesystem::path &fixture_path);

    Tcl_Interp *interp = nullptr;
    std::map<std::string, AliasInfo> aliases;
    std::vector<std::string> exclusions;
    std::map<std::string, std::set<std::string>> lint_shadows;
    std::set<std::string> definitions;
    std::set<std::string> known_classes;
    std::map<std::string, std::vector<std::string>> class_bases;
    std::set<std::string> classes_with_dynamic_bases;
    std::vector<SourceFile> sources;
    std::vector<Finding> findings;
    std::vector<ParseIssue> parse_issues;
    std::set<std::string> finding_keys;
    std::set<std::string> parse_issue_keys;

    void add_finding(SourceFile &source, const char *location,
	const std::string &command, const AliasInfo &alias);
    void add_parse_issue(SourceFile &source, const char *location,
	const std::string &message);
    bool local_command_exists(const std::string &scope,
	const std::string &command) const;
    bool class_has_external_base(const std::string &class_name) const;
    bool source_shadows_command(const SourceFile &source,
	const std::string &command, const AliasInfo &alias) const;

private:
    bool excluded(const std::string &relative_path) const;
    void reset_analysis();
};


SourceParser::SourceParser(TclSourceLint &lint_ref, SourceFile &source_ref,
    ParsePass parse_pass) :
    lint(lint_ref), source(source_ref), pass(parse_pass)
{
}


std::vector<Word>
SourceParser::words(const Tcl_Parse &parse) const
{
    std::vector<Word> result;
    Tcl_Token *token = parse.tokenPtr;

    for (int i = 0; i < parse.numWords; i++) {
	Word word;
	word.token = token;
	if (token->type == TCL_TOKEN_SIMPLE_WORD && token->numComponents == 1) {
	    Tcl_Token *text = token + 1;
	    if (text->type == TCL_TOKEN_TEXT) {
		word.literal = true;
		word.value.assign(text->start, static_cast<size_t>(text->size));
		word.content_start = text->start;
		word.content_size = text->size;
	    }
	} else if (token->type == TCL_TOKEN_WORD && token->size >= 2 &&
	    token->start[0] == '{' && token->start[token->size - 1] == '}') {
	    /* Braced words remain literal even when backslash-newline tokens mean
	     * Tcl cannot classify them as TCL_TOKEN_SIMPLE_WORD. */
	    word.literal = true;
	    word.content_start = token->start + 1;
	    word.content_size = token->size - 2;
	    word.value.assign(word.content_start,
		static_cast<size_t>(word.content_size));
	}
	result.push_back(word);
	token += token->numComponents + 1;
    }

    return result;
}


void
SourceParser::parse_error(const char *location, const char *context)
{
    if (pass != ParsePass::CHECK_CALLS)
	return;

    std::string message = context;
    const char *detail = Tcl_GetStringResult(lint.interp);
    if (detail && detail[0] != '\0') {
	message += ": ";
	message += detail;
    }
    lint.add_parse_issue(source, location, message);
    Tcl_ResetResult(lint.interp);
}


void
SourceParser::define_command(const std::string &name, const Scope &scope)
{
    if (pass != ParsePass::COLLECT_DEFINITIONS || name.empty())
	return;

    std::string qualified = qualify_name(name, scope);
    if (!qualified.empty()) {
	lint.definitions.insert(qualified);
	source.definitions.insert(qualified);
    }
}


void
SourceParser::define_class(const std::string &name, const Scope &scope)
{
    define_command(name, scope);
    if (pass != ParsePass::COLLECT_DEFINITIONS)
	return;

    std::string qualified = qualify_name(name, scope);
    if (!qualified.empty())
	lint.known_classes.insert(qualified);
}


void
SourceParser::check_command_name(const std::string &name, const char *location,
    const Scope &scope)
{
    if (pass != ParsePass::CHECK_CALLS || name.empty())
	return;

    std::string candidate;
    bool explicit_global = starts_with(name, "::");
    if (explicit_global) {
	candidate = name;
    } else if (name.find("::") == std::string::npos) {
	if (!scope.known)
	    return;
	candidate = "::" + name;
    } else {
	return;
    }

    auto alias = lint.aliases.find(candidate);
    if (alias == lint.aliases.end())
	return;

    if (!explicit_global) {
	if (lint.source_shadows_command(source, candidate, alias->second))
	    return;
	if (scope.name != "::" &&
	    lint.local_command_exists(scope.name, name))
	    return;
	if (scope.class_body && lint.class_has_external_base(scope.name))
	    return;
    }

    lint.add_finding(source, location, candidate, alias->second);
}


void
SourceParser::check_command(const Word &word, const Scope &scope)
{
    if (word.literal)
	check_command_name(word.value, word.content_start, scope);
}


void
SourceParser::recurse_command_tokens(const Tcl_Parse &parse, const Scope &scope,
    size_t depth)
{
    for (int i = 0; i < parse.numTokens; i++) {
	const Tcl_Token &token = parse.tokenPtr[i];
	if (token.type != TCL_TOKEN_COMMAND || token.size < 2)
	    continue;
	parse_script(token.start + 1, static_cast<size_t>(token.size - 2),
	    scope, false, depth + 1);
    }
}


void
SourceParser::parse_expression(const Word &word, const Scope &scope, size_t depth)
{
    if (!word.literal || !word.content_start || word.content_size <= 0 ||
	depth > MAX_PARSE_DEPTH)
	return;

    Tcl_Parse parse;
    int ret = Tcl_ParseExpr(lint.interp, word.content_start, word.content_size,
	&parse);
    if (ret != TCL_OK) {
	/* Tk substitutes percent fields in binding scripts before Tcl sees the
	 * expression.  An invalid source template is not a lint parse failure. */
	Tcl_FreeParse(&parse);
	Tcl_ResetResult(lint.interp);
	return;
    }

    recurse_command_tokens(parse, scope, depth);
    Tcl_FreeParse(&parse);
}


void
SourceParser::inspect_list_builder(const Tcl_Token &token, const Scope &scope,
    size_t depth)
{
    if (token.size < 2 || depth > MAX_PARSE_DEPTH)
	return;

    Tcl_Parse parse;
    const char *script = token.start + 1;
    int length = token.size - 2;
    if (Tcl_ParseCommand(lint.interp, script, length, 0, &parse) != TCL_OK) {
	Tcl_FreeParse(&parse);
	Tcl_ResetResult(lint.interp);
	return;
    }

    std::vector<Word> command_words = words(parse);
    if (command_words.size() > 1 && command_words[0].literal &&
	command_is(command_words[0].value, "list")) {
	check_command(command_words[1], scope);
    }
    Tcl_FreeParse(&parse);
}


void
SourceParser::inspect_command_prefix(const Word &word, const Scope &scope,
    size_t depth)
{
    if (word.literal) {
	int element_count = 0;
	const char **elements = nullptr;
	if (Tcl_SplitList(lint.interp, word.value.c_str(), &element_count,
		&elements) == TCL_OK && element_count > 0) {
	    check_command_name(elements[0], word.content_start, scope);
	}
	if (elements)
	    Tcl_Free(reinterpret_cast<char *>(elements));
	Tcl_ResetResult(lint.interp);
	return;
    }

    if (!word.token)
	return;
    for (int i = 1; i <= word.token->numComponents; i++) {
	const Tcl_Token &component = word.token[i];
	if (component.type == TCL_TOKEN_COMMAND)
	    inspect_list_builder(component, scope, depth + 1);
    }
}


void
SourceParser::parse_script_word(const Word &word, const Scope &scope,
    bool class_body, size_t depth)
{
    if (word.literal && word.content_start && word.content_size > 0) {
	parse_script(word.content_start, static_cast<size_t>(word.content_size),
	    scope, class_body, depth + 1);
    } else {
	inspect_command_prefix(word, scope, depth + 1);
    }
}


void
SourceParser::parse_list_body(const Word &word, size_t element_index,
    const Scope &scope, bool class_body, size_t depth)
{
    if (!word.literal || !word.content_start || depth > MAX_PARSE_DEPTH)
	return;

    int element_count = 0;
    const char **elements = nullptr;
    if (Tcl_SplitList(lint.interp, word.value.c_str(), &element_count,
	    &elements) != TCL_OK) {
	parse_error(word.content_start, "invalid Tcl list containing script bodies");
	Tcl_ResetResult(lint.interp);
	return;
    }

    if (element_index >= static_cast<size_t>(element_count)) {
	Tcl_Free(reinterpret_cast<char *>(elements));
	return;
    }

    const char *element = elements[element_index];
    size_t element_length = strlen(element);
    if (element_length > 0) {
	const char *begin = word.content_start;
	const char *end = begin + word.content_size;
	const char *found = std::search(begin, end, element,
	    element + element_length);
	if (found != end) {
	    parse_script(found, element_length, scope, class_body, depth + 1);
	}
    }

    Tcl_Free(reinterpret_cast<char *>(elements));
}


void
SourceParser::process_class_member(const std::vector<Word> &command_words,
    size_t command_index, const Scope &scope, size_t depth)
{
    if (command_index >= command_words.size() ||
	!command_words[command_index].literal)
	return;

    const std::string &member_type = command_words[command_index].value;
    if (member_type == "method" || member_type == "proc" ||
	member_type == "typemethod") {
	if (command_words.size() <= command_index + 1 ||
	    !command_words[command_index + 1].literal)
	    return;
	define_command(command_words[command_index + 1].value, scope);
	if (command_words.size() > command_index + 3)
	    parse_script_word(command_words[command_index + 3], scope, false,
		depth);
	return;
    }

    if (member_type == "constructor") {
	if (command_words.size() > command_index + 2)
	    parse_script_word(command_words.back(), scope, false, depth);
	return;
    }

    if (member_type == "destructor" &&
	command_words.size() > command_index + 1) {
	parse_script_word(command_words[command_index + 1], scope, false, depth);
    }
}


void
SourceParser::process_switch(const std::vector<Word> &command_words,
    const Scope &scope, size_t depth)
{
    size_t index = 1;
    while (index < command_words.size() && command_words[index].literal &&
	starts_with(command_words[index].value, "-")) {
	const std::string &option = command_words[index].value;
	index++;
	if (option == "--")
	    break;
	if ((option == "-matchvar" || option == "-indexvar") &&
	    index < command_words.size())
	    index++;
    }

    if (index >= command_words.size())
	return;
    index++; /* value being matched */
    size_t remaining = command_words.size() - index;
    if (remaining == 1 && command_words[index].literal) {
	int element_count = 0;
	const char **elements = nullptr;
	if (Tcl_SplitList(lint.interp, command_words[index].value.c_str(),
		&element_count, &elements) == TCL_OK) {
	    for (int i = 1; i < element_count; i += 2) {
		if (!BU_STR_EQUAL(elements[i], "-"))
		    parse_list_body(command_words[index], static_cast<size_t>(i),
			scope, false, depth);
	    }
	}
	if (elements)
	    Tcl_Free(reinterpret_cast<char *>(elements));
	Tcl_ResetResult(lint.interp);
	return;
    }

    for (size_t i = index + 1; i < command_words.size(); i += 2) {
	if (!command_words[i].literal || command_words[i].value != "-")
	    parse_script_word(command_words[i], scope, false, depth);
    }
}


void
SourceParser::process_try(const std::vector<Word> &command_words,
    const Scope &scope, size_t depth)
{
    if (command_words.size() < 2)
	return;
    parse_script_word(command_words[1], scope, false, depth);

    size_t index = 2;
    while (index < command_words.size()) {
	if (!command_words[index].literal)
	    return;
	const std::string &clause = command_words[index].value;
	if ((clause == "on" || clause == "trap") &&
	    index + 3 < command_words.size()) {
	    parse_script_word(command_words[index + 3], scope, false, depth);
	    index += 4;
	    continue;
	}
	if (clause == "finally" && index + 1 < command_words.size()) {
	    parse_script_word(command_words[index + 1], scope, false, depth);
	}
	return;
    }
}


void
SourceParser::process_callback_options(const std::vector<Word> &command_words,
    const Scope &scope, size_t depth)
{
    static const std::set<std::string> callback_options = {
	"-browsecmd", "-closecmd", "-command", "-dblclickcmd", "-dropcmd",
	"-invalidcommand", "-invcmd", "-postcommand", "-raisecmd",
	"-selectcommand", "-validatecommand", "-vcmd", "-xscrollcommand",
	"-yscrollcommand"
    };

    for (size_t i = 1; i + 1 < command_words.size(); i++) {
	if (!command_words[i].literal ||
	    callback_options.find(command_words[i].value) == callback_options.end())
	    continue;
	parse_script_word(command_words[i + 1], scope, false, depth);
	inspect_command_prefix(command_words[i + 1], scope, depth);
	i++;
    }
}


void
SourceParser::process_command(const Tcl_Parse &parse,
    const std::vector<Word> &command_words, const Scope &scope,
    bool class_body, size_t depth)
{
    if (command_words.empty())
	return;

    recurse_command_tokens(parse, scope, depth);
    check_command(command_words[0], scope);
    if (!command_words[0].literal)
	return;

    const std::string &command = command_words[0].value;

    if (command_is(command, "proc") && command_words.size() >= 4) {
	if (command_words[1].literal) {
	    std::string full_name = qualify_name(command_words[1].value, scope);
	    define_command(command_words[1].value, scope);
	    Scope body_scope;
	    body_scope.known = !full_name.empty();
	    body_scope.name = body_scope.known ? command_namespace(full_name) : "";
	    parse_script_word(command_words[3], body_scope, false, depth);
	} else {
	    Scope unknown;
	    unknown.known = false;
	    parse_script_word(command_words[3], unknown, false, depth);
	}
	return;
    }

    if (command_is(command, "namespace") && command_words.size() >= 3 &&
	command_words[1].literal) {
	const std::string &subcommand = command_words[1].value;
	if (subcommand == "eval" && command_words.size() >= 4) {
	    Scope nested;
	    if (command_words[2].literal) {
		nested.name = qualify_name(command_words[2].value, scope);
		nested.known = !nested.name.empty();
	    } else {
		nested.known = false;
		nested.name.clear();
	    }
	    if (command_words.size() == 4)
		parse_script_word(command_words[3], nested, false, depth);
	    return;
	}
	if (subcommand == "code") {
	    parse_script_word(command_words[2], scope, false, depth);
	    inspect_command_prefix(command_words[2], scope, depth);
	    return;
	}
	if (subcommand == "inscope" && command_words.size() >= 4) {
	    Scope nested;
	    if (command_words[2].literal) {
		nested.name = qualify_name(command_words[2].value, scope);
		nested.known = !nested.name.empty();
	    } else {
		nested.known = false;
		nested.name.clear();
	    }
	    parse_script_word(command_words[3], nested, false, depth);
	    inspect_command_prefix(command_words[3], nested, depth);
	    return;
	}
    }

    std::string unrooted = without_root_prefix(command);
    if ((unrooted == "itcl::class" || unrooted == "itcl::extendedclass" ||
	unrooted == "snit::type" || unrooted == "snit::widget" ||
	unrooted == "snit::widgetadaptor") && command_words.size() >= 3) {
	Scope nested;
	if (command_words[1].literal) {
	    std::string class_name = qualify_name(command_words[1].value, scope);
	    define_class(command_words[1].value, scope);
	    nested.name = class_name;
	    nested.known = !class_name.empty();
	} else {
	    nested.name.clear();
	    nested.known = false;
	}
	nested.class_body = true;
	parse_script_word(command_words[2], nested, true, depth);
	return;
    }

    if (unrooted == "itcl::body" && command_words.size() >= 4) {
	Scope nested;
	if (command_words[1].literal) {
	    std::string method = qualify_name(command_words[1].value, scope);
	    nested.name = method.empty() ? "" : command_namespace(method);
	    nested.known = !method.empty();
	} else {
	    nested.name.clear();
	    nested.known = false;
	}
	nested.class_body = true;
	parse_script_word(command_words[3], nested, false, depth);
	return;
    }

    if (unrooted == "itcl::configbody" && command_words.size() >= 3) {
	Scope nested;
	if (command_words[1].literal) {
	    std::string method = qualify_name(command_words[1].value, scope);
	    nested.name = method.empty() ? "" : command_namespace(method);
	    nested.known = !method.empty();
	} else {
	    nested.name.clear();
	    nested.known = false;
	}
	nested.class_body = true;
	parse_script_word(command_words[2], nested, false, depth);
	return;
    }

    if ((unrooted == "oo::class" || unrooted == "oo::object") &&
	command_words.size() >= 4 && command_words[1].literal &&
	command_words[1].value == "create") {
	Scope nested;
	if (command_words[2].literal) {
	    std::string object_name = qualify_name(command_words[2].value, scope);
	    if (unrooted == "oo::class")
		define_class(command_words[2].value, scope);
	    else
		define_command(command_words[2].value, scope);
	    nested.name = object_name;
	    nested.known = !object_name.empty();
	} else {
	    nested.name.clear();
	    nested.known = false;
	}
	nested.class_body = true;
	parse_script_word(command_words[3], nested, true, depth);
	return;
    }

    if ((unrooted == "oo::define" || unrooted == "oo::objdefine") &&
	command_words.size() >= 3) {
	Scope nested;
	if (command_words[1].literal) {
	    nested.name = qualify_name(command_words[1].value, scope);
	    nested.known = !nested.name.empty();
	} else {
	    nested.name.clear();
	    nested.known = false;
	}
	nested.class_body = true;
	if (command_words.size() == 3) {
	    parse_script_word(command_words[2], nested, true, depth);
	} else {
	    process_class_member(command_words, 2, nested, depth);
	}
	return;
    }

    if (class_body) {
	if (command == "public" || command == "protected" || command == "private") {
	    if (command_words.size() == 2)
		parse_script_word(command_words[1], scope, true, depth);
	    else
		process_class_member(command_words, 1, scope, depth);
	    return;
	}
	if (command == "inherit") {
	    if (pass == ParsePass::COLLECT_DEFINITIONS && scope.known) {
		Scope parent_scope;
		parent_scope.name = command_namespace(scope.name);
		for (size_t i = 1; i < command_words.size(); i++) {
		    if (!command_words[i].literal) {
			lint.classes_with_dynamic_bases.insert(scope.name);
			continue;
		    }
		    std::string base = qualify_name(command_words[i].value,
			parent_scope);
		    if (!base.empty())
			lint.class_bases[scope.name].push_back(base);
		}
	    }
	    return;
	}
	if (command == "method" || command == "proc" ||
	    command == "typemethod" || command == "constructor" ||
	    command == "destructor") {
	    process_class_member(command_words, 0, scope, depth);
	    return;
	}
    }

    if (command_is(command, "if") && command_words.size() >= 3) {
	size_t index = 1;
	while (index < command_words.size()) {
	    parse_expression(command_words[index], scope, depth);
	    index++;
	    if (index < command_words.size() && command_words[index].literal &&
		command_words[index].value == "then")
		index++;
	    if (index >= command_words.size())
		break;
	    parse_script_word(command_words[index], scope, false, depth);
	    index++;
	    if (index >= command_words.size() || !command_words[index].literal)
		break;
	    if (command_words[index].value == "elseif") {
		index++;
		continue;
	    }
	    if (command_words[index].value == "else" &&
		index + 1 < command_words.size())
		parse_script_word(command_words[index + 1], scope, false, depth);
	    break;
	}
	return;
    }

    if (command_is(command, "while") && command_words.size() >= 3) {
	parse_expression(command_words[1], scope, depth);
	parse_script_word(command_words[2], scope, false, depth);
	return;
    }

    if (command_is(command, "for") && command_words.size() >= 5) {
	parse_script_word(command_words[1], scope, false, depth);
	parse_expression(command_words[2], scope, depth);
	parse_script_word(command_words[3], scope, false, depth);
	parse_script_word(command_words[4], scope, false, depth);
	return;
    }

    if ((command_is(command, "foreach") || command_is(command, "lmap")) &&
	command_words.size() >= 4) {
	parse_script_word(command_words.back(), scope, false, depth);
	return;
    }

    if (command_is(command, "catch") && command_words.size() >= 2) {
	parse_script_word(command_words[1], scope, false, depth);
	return;
    }

    if (command_is(command, "try")) {
	process_try(command_words, scope, depth);
	return;
    }

    if (command_is(command, "switch")) {
	process_switch(command_words, scope, depth);
	return;
    }

    if (command_is(command, "expr")) {
	if (command_words.size() == 2)
	    parse_expression(command_words[1], scope, depth);
	return;
    }

    if ((command_is(command, "eval") || command_is(command, "time")) &&
	command_words.size() >= 2) {
	if (command_words.size() == 2)
	    parse_script_word(command_words[1], scope, false, depth);
	else
	    inspect_command_prefix(command_words[1], scope, depth);
	return;
    }

    if (command_is(command, "uplevel") && command_words.size() >= 2) {
	size_t index = 1;
	if (command_words[index].literal &&
	    (starts_with(command_words[index].value, "#") ||
	     std::isdigit(static_cast<unsigned char>(command_words[index].value[0]))))
	    index++;
	if (index < command_words.size()) {
	    if (index + 1 == command_words.size())
		parse_script_word(command_words[index], scope, false, depth);
	    else
		inspect_command_prefix(command_words[index], scope, depth);
	}
	return;
    }

    if (command_is(command, "apply") && command_words.size() >= 2 &&
	command_words[1].literal) {
	int element_count = 0;
	const char **elements = nullptr;
	if (Tcl_SplitList(lint.interp, command_words[1].value.c_str(),
		&element_count, &elements) == TCL_OK && element_count >= 2) {
	    Scope nested = scope;
	    if (element_count >= 3) {
		nested.name = qualify_name(elements[2], scope);
		nested.known = !nested.name.empty();
	    }
	    parse_list_body(command_words[1], 1, nested, false, depth);
	}
	if (elements)
	    Tcl_Free(reinterpret_cast<char *>(elements));
	Tcl_ResetResult(lint.interp);
	return;
    }

    if (command_is(command, "dict") && command_words.size() >= 2 &&
	command_words[1].literal) {
	const std::string &subcommand = command_words[1].value;
	if ((subcommand == "for" || subcommand == "map") &&
	    command_words.size() >= 5)
	    parse_script_word(command_words[4], scope, false, depth);
	else if (subcommand == "filter" && command_words.size() >= 6 &&
	    command_words[3].literal && command_words[3].value == "script")
	    parse_script_word(command_words[5], scope, false, depth);
	else if ((subcommand == "update" || subcommand == "with") &&
	    command_words.size() >= 4)
	    parse_script_word(command_words.back(), scope, false, depth);
	return;
    }

    if (command_is(command, "interp") && command_words.size() >= 4 &&
	command_words[1].literal && command_words[1].value == "eval") {
	if (command_words.size() == 4)
	    parse_script_word(command_words[3], scope, false, depth);
	else
	    inspect_command_prefix(command_words[3], scope, depth);
	return;
    }

    if (command_is(command, "after") && command_words.size() >= 3 &&
	(!command_words[1].literal ||
	 (command_words[1].value != "cancel" && command_words[1].value != "info"))) {
	if (command_words.size() == 3) {
	    parse_script_word(command_words[2], scope, false, depth);
	    inspect_command_prefix(command_words[2], scope, depth);
	} else {
	    inspect_command_prefix(command_words[2], scope, depth);
	}
	return;
    }

    if (command_is(command, "bind") && command_words.size() >= 4) {
	parse_script_word(command_words[3], scope, false, depth);
	inspect_command_prefix(command_words[3], scope, depth);
	return;
    }

    if (command_is(command, "fileevent") && command_words.size() >= 4) {
	parse_script_word(command_words[3], scope, false, depth);
	inspect_command_prefix(command_words[3], scope, depth);
	return;
    }

    if (command_is(command, "wm") && command_words.size() >= 5 &&
	command_words[1].literal && command_words[1].value == "protocol") {
	parse_script_word(command_words[4], scope, false, depth);
	inspect_command_prefix(command_words[4], scope, depth);
	return;
    }

    if ((command_is(command, "tailcall") || command_is(command, "coroutine")) &&
	command_words.size() >= 2) {
	size_t command_index = command_is(command, "coroutine") ? 2 : 1;
	if (command_index < command_words.size())
	    check_command(command_words[command_index], scope);
	return;
    }

    if (command_is(command, "interp") && command_words.size() >= 5 &&
	command_words[1].literal && command_words[1].value == "alias" &&
	command_words[2].literal && command_words[2].value.empty() &&
	command_words[3].literal) {
	define_command(command_words[3].value, scope);
	return;
    }

    if (command_is(command, "rename") && command_words.size() >= 3 &&
	command_words[2].literal && !command_words[2].value.empty()) {
	define_command(command_words[2].value, scope);
	return;
    }

    process_callback_options(command_words, scope, depth);
}


void
SourceParser::parse_script(const char *script, size_t length, const Scope &scope,
    bool class_body, size_t depth)
{
    if (!script || length == 0)
	return;
    if (depth > MAX_PARSE_DEPTH) {
	if (pass == ParsePass::CHECK_CALLS)
	    lint.add_parse_issue(source, script,
		"Tcl script nesting exceeds the lint limit");
	return;
    }
    if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
	if (pass == ParsePass::CHECK_CALLS)
	    lint.add_parse_issue(source, script, "Tcl script is too large to parse");
	return;
    }

    const char *cursor = script;
    const char *end = script + length;
    while (cursor < end) {
	Tcl_Parse parse;
	int ret = Tcl_ParseCommand(lint.interp, cursor,
	    static_cast<int>(end - cursor), 0, &parse);
	if (ret != TCL_OK) {
	    parse_error(parse.term ? parse.term : cursor, "invalid Tcl script");
	    Tcl_FreeParse(&parse);
	    return;
	}

	const char *next = parse.commandStart + parse.commandSize;
	if (parse.numWords > 0) {
	    std::vector<Word> command_words = words(parse);
	    process_command(parse, command_words, scope, class_body, depth);
	}
	Tcl_FreeParse(&parse);

	if (!next || next <= cursor)
	    break;
	cursor = next;
    }
}


void
SourceParser::parse()
{
    Scope global;
    parse_script(source.contents.data(), source.contents.size(), global, false, 0);
}


void
TclSourceLint::add_finding(SourceFile &source, const char *location,
    const std::string &command, const AliasInfo &alias)
{
    size_t offset = 0;
    if (location && !source.contents.empty() &&
	location >= source.contents.data() &&
	location <= source.contents.data() + source.contents.size())
	offset = static_cast<size_t>(location - source.contents.data());

    std::string key = source.relative_path + "|" + std::to_string(offset) +
	"|" + command;
    if (!finding_keys.insert(key).second)
	return;

    Finding finding;
    finding.source = source.relative_path;
    finding.line = source.line_number(location);
    finding.command = command;
    finding.target = alias.target;
    finding.mged_implementation = alias.mged_implementation;
    findings.push_back(finding);
}


void
TclSourceLint::add_parse_issue(SourceFile &source, const char *location,
    const std::string &message)
{
    size_t line = source.line_number(location);
    std::string key = source.relative_path + "|" + std::to_string(line) +
	"|" + message;
    if (!parse_issue_keys.insert(key).second)
	return;

    parse_issues.push_back({source.relative_path, line, message});
}


bool
TclSourceLint::local_command_exists(const std::string &scope,
    const std::string &command) const
{
    std::vector<std::string> pending = {scope};
    std::set<std::string> visited;

    while (!pending.empty()) {
	std::string current = pending.back();
	pending.pop_back();
	if (!visited.insert(current).second)
	    continue;
	if (definitions.find(current + "::" + command) != definitions.end())
	    return true;
	auto bases = class_bases.find(current);
	if (bases != class_bases.end())
	    pending.insert(pending.end(), bases->second.begin(), bases->second.end());
    }

    return false;
}


bool
TclSourceLint::class_has_external_base(const std::string &class_name) const
{
    if (known_classes.find(class_name) == known_classes.end())
	return false;

    std::vector<std::string> pending = {class_name};
    std::set<std::string> visited;

    while (!pending.empty()) {
	std::string current = pending.back();
	pending.pop_back();
	if (!visited.insert(current).second)
	    continue;
	if (classes_with_dynamic_bases.find(current) !=
	    classes_with_dynamic_bases.end())
	    return true;
	auto bases = class_bases.find(current);
	if (bases == class_bases.end())
	    continue;
	for (const std::string &base : bases->second) {
	    /* An external class may provide the bare method.  Explicit global
	     * calls are checked before reaching this ambiguity test. */
	    if (known_classes.find(base) == known_classes.end())
		return true;
	    pending.push_back(base);
	}
    }

    return false;
}


bool
TclSourceLint::source_shadows_command(const SourceFile &source,
    const std::string &command, const AliasInfo &alias) const
{
    auto configured = lint_shadows.find(source.relative_path);
    if (configured != lint_shadows.end() &&
	configured->second.find(command) != configured->second.end())
	return true;

    return source.definitions.find(command) != source.definitions.end() &&
	alias.definition_sources.find(source.relative_path) ==
	    alias.definition_sources.end();
}


bool
TclSourceLint::load_manifest(const std::filesystem::path &manifest_path)
{
    std::ifstream input(manifest_path);
    if (!input) {
	std::cerr << "Unable to read Tcl namespace manifest: "
	    << manifest_path.generic_string() << "\n";
	return false;
    }

    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
	line_number++;
	line = trim(line);
	if (line.empty() || line[0] == '#')
	    continue;

	int field_count = 0;
	const char **fields = nullptr;
	if (Tcl_SplitList(interp, line.c_str(), &field_count, &fields) != TCL_OK) {
	    std::cerr << manifest_path.generic_string() << ":" << line_number
		<< ": malformed Tcl list: " << Tcl_GetStringResult(interp) << "\n";
	    Tcl_ResetResult(interp);
	    return false;
	}

	bool valid = true;
	if (field_count == 2 && BU_STR_EQUAL(fields[0], "exclude")) {
	    exclusions.push_back(std::filesystem::path(fields[1]).generic_string());
	} else if (field_count == 3 && BU_STR_EQUAL(fields[0], "lint-shadow")) {
	    std::string source =
		std::filesystem::path(fields[1]).generic_string();
	    std::string command = normalize_command(fields[2]);
	    if (without_root_prefix(command).find("::") != std::string::npos) {
		std::cerr << manifest_path.generic_string() << ":" << line_number
		    << ": lint-shadow command must be global\n";
		valid = false;
	    } else {
		lint_shadows[source].insert(command);
	    }
	} else if (field_count == 5 &&
	    (BU_STR_EQUAL(fields[0], "script") ||
	     BU_STR_EQUAL(fields[0], "native"))) {
	    std::string command = normalize_command(fields[2]);
	    std::string kind = fields[3];
	    if (kind == "compatibility" ||
		(kind == "legacy" && starts_with(command, "::_mged_"))) {
		AliasInfo info;
		info.target = fields[4];
		info.mged_implementation = starts_with(command, "::_mged_");
		if (BU_STR_EQUAL(fields[0], "script"))
		    info.definition_sources.insert(
			std::filesystem::path(fields[1]).generic_string());
		if (info.target.empty() && info.mged_implementation)
		    info.target = "::brlcad::mged " + command.substr(8);
		auto prior = aliases.find(command);
		if (prior != aliases.end() && prior->second.target != info.target) {
		    std::cerr << manifest_path.generic_string() << ":" << line_number
			<< ": conflicting targets for " << command << "\n";
		    valid = false;
		} else if (prior != aliases.end()) {
		    prior->second.definition_sources.insert(
			info.definition_sources.begin(),
			info.definition_sources.end());
		    prior->second.mged_implementation =
			prior->second.mged_implementation || info.mged_implementation;
		} else {
		    aliases[command] = std::move(info);
		}
	    }
	} else {
	    std::cerr << manifest_path.generic_string() << ":" << line_number
		<< ": unrecognized manifest record\n";
	    valid = false;
	}

	Tcl_Free(reinterpret_cast<char *>(fields));
	if (!valid)
	    return false;
    }

    for (const auto &shadow : lint_shadows) {
	for (const std::string &command : shadow.second) {
	    if (aliases.find(command) == aliases.end()) {
		std::cerr << manifest_path.generic_string()
		    << ": lint-shadow for non-compatibility command " << command
		    << " in " << shadow.first << "\n";
		return false;
	    }
	}
    }

    return true;
}


bool
TclSourceLint::excluded(const std::string &relative_path) const
{
    for (const std::string &prefix : exclusions) {
	if (path_prefix_matches(relative_path, prefix))
	    return true;
    }
    return false;
}


bool
TclSourceLint::load_one_source(const std::filesystem::path &path,
    const std::string &relative_path)
{
    /* Tcl source channels normalize platform newlines before parsing. */
    std::ifstream input(path);
    if (!input) {
	std::cerr << "Unable to read Tcl source: " << path.generic_string() << "\n";
	return false;
    }

    SourceFile source;
    source.path = path;
    source.relative_path = relative_path;
    source.contents.assign(std::istreambuf_iterator<char>(input),
	std::istreambuf_iterator<char>());
    if (source.contents.size() >= 3 &&
	static_cast<unsigned char>(source.contents[0]) == 0xef &&
	static_cast<unsigned char>(source.contents[1]) == 0xbb &&
	static_cast<unsigned char>(source.contents[2]) == 0xbf)
	source.contents.erase(0, 3);

    source.line_starts.push_back(0);
    for (size_t i = 0; i < source.contents.size(); i++) {
	if (source.contents[i] == '\n')
	    source.line_starts.push_back(i + 1);
    }
    sources.push_back(std::move(source));
    return true;
}


bool
TclSourceLint::load_sources(const std::filesystem::path &file_list,
    const std::filesystem::path &source_root)
{
    std::ifstream input(file_list);
    if (!input) {
	std::cerr << "Unable to read CMake source inventory: "
	    << file_list.generic_string() << "\n";
	return false;
    }

    std::error_code error;
    std::filesystem::path root = std::filesystem::weakly_canonical(source_root,
	error);
    if (error) {
	std::cerr << "Unable to normalize source root: "
	    << source_root.generic_string() << "\n";
	return false;
    }

    std::set<std::string> seen;
    std::string line;
    while (std::getline(input, line)) {
	line = trim(line);
	if (line.empty())
	    continue;

	std::filesystem::path path = std::filesystem::weakly_canonical(line, error);
	if (error) {
	    error.clear();
	    continue;
	}
	std::filesystem::path relative = path.lexically_relative(root);
	std::string relative_path = relative.generic_string();
	if (relative_path.empty() || starts_with(relative_path, "../") ||
	    !starts_with(relative_path, "src/") ||
	    relative_path.find("/tests/") != std::string::npos ||
	    !is_tcl_source(relative_path) || excluded(relative_path) ||
	    !seen.insert(relative_path).second)
	    continue;

	if (!load_one_source(path, relative_path))
	    return false;
    }

    if (sources.empty()) {
	std::cerr << "CMake source inventory contains no first-party Tcl sources\n";
	return false;
    }
    std::sort(sources.begin(), sources.end(),
	[](const SourceFile &a, const SourceFile &b) {
	    return a.relative_path < b.relative_path;
	});
    return true;
}


void
TclSourceLint::reset_analysis()
{
    definitions.clear();
    known_classes.clear();
    class_bases.clear();
    classes_with_dynamic_bases.clear();
    findings.clear();
    parse_issues.clear();
    finding_keys.clear();
    parse_issue_keys.clear();
    for (SourceFile &source : sources)
	source.definitions.clear();
}


int
TclSourceLint::run(bool report)
{
    reset_analysis();
    for (SourceFile &source : sources) {
	SourceParser parser(*this, source, ParsePass::COLLECT_DEFINITIONS);
	parser.parse();
    }
    for (SourceFile &source : sources) {
	SourceParser parser(*this, source, ParsePass::CHECK_CALLS);
	parser.parse();
    }

    std::sort(findings.begin(), findings.end(),
	[](const Finding &a, const Finding &b) {
	    if (a.source != b.source)
		return a.source < b.source;
	    if (a.line != b.line)
		return a.line < b.line;
	    return a.command < b.command;
	});
    std::sort(parse_issues.begin(), parse_issues.end(),
	[](const ParseIssue &a, const ParseIssue &b) {
	    if (a.source != b.source)
		return a.source < b.source;
	    if (a.line != b.line)
		return a.line < b.line;
	    return a.message < b.message;
	});

    if (report) {
	for (const ParseIssue &issue : parse_issues)
	    std::cerr << issue.source << ":" << issue.line << ": "
		<< issue.message << "\n";
	for (const Finding &finding : findings) {
	    std::cerr << finding.source << ":" << finding.line << ": "
		<< (finding.mged_implementation ?
		    "legacy MGED implementation alias " : "compatibility command ")
		<< finding.command;
	    if (!finding.target.empty())
		std::cerr << "; use " << finding.target;
	    std::cerr << "\n";
	}
    }

    if (!parse_issues.empty() || !findings.empty()) {
	if (report)
	    std::cerr << "Tcl source lint failed (" << parse_issues.size()
		<< " parse issues, " << findings.size()
		<< " compatibility calls)\n";
	return 1;
    }

    if (report)
	std::cout << "Tcl source lint passed (" << sources.size()
	    << " first-party files, " << aliases.size()
	    << " compatibility commands)\n";
    return 0;
}


int
TclSourceLint::self_test(const std::filesystem::path &fixture_path)
{
    aliases.clear();
    exclusions.clear();
    lint_shadows.clear();
    sources.clear();

    std::string fixture_name = fixture_path.filename().generic_string();
    AliasInfo legacy;
    legacy.target = "::brlcad::test command";
    legacy.definition_sources.insert(fixture_name);
    aliases["::legacy_cmd"] = legacy;

    AliasInfo native_shadow;
    native_shadow.target = "::brlcad::test native_shadow";
    aliases["::native_shadow"] = native_shadow;

    AliasInfo runtime_shadow;
    runtime_shadow.target = "::brlcad::test runtime_shadow";
    aliases["::runtime_shadow"] = runtime_shadow;
    lint_shadows[fixture_name].insert("::runtime_shadow");

    AliasInfo mged_implementation;
    mged_implementation.target = "::brlcad::mged draw";
    mged_implementation.mged_implementation = true;
    aliases["::_mged_draw"] = mged_implementation;

    if (!load_one_source(fixture_path, fixture_name))
	return 2;

    std::vector<std::pair<size_t, std::string>> expected;
    std::istringstream lines(sources.front().contents);
    std::string line;
    size_t line_number = 0;
    while (std::getline(lines, line)) {
	line_number++;
	size_t marker = line.find("# lint ");
	if (marker == std::string::npos)
	    continue;
	std::istringstream expectation(line.substr(marker + 7));
	std::string command;
	expectation >> command;
	if (!command.empty())
	    expected.emplace_back(line_number, normalize_command(command));
    }

    int result = run(false);
    std::vector<std::pair<size_t, std::string>> actual;
    for (const Finding &finding : findings)
	actual.emplace_back(finding.line, finding.command);
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());

    if (!parse_issues.empty() || actual != expected) {
	std::cerr << "Tcl source lint self-test mismatch\n";
	for (const auto &item : expected) {
	    if (!std::binary_search(actual.begin(), actual.end(), item))
		std::cerr << "  missing expected finding at line " << item.first
		    << ": " << item.second << "\n";
	}
	for (const auto &item : actual) {
	    if (!std::binary_search(expected.begin(), expected.end(), item))
		std::cerr << "  unexpected finding at line " << item.first
		    << ": " << item.second << "\n";
	}
	return 1;
    }

    if (result != 1 || findings.empty()) {
	std::cerr << "Tcl source lint self-test did not exercise diagnostics\n";
	return 1;
    }

    std::cout << "Tcl source lint self-test passed (" << findings.size()
	<< " expected findings)\n";
    return 0;
}

} /* namespace */


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    Tcl_FindExecutable(argv[0]);
    int result = 2;
    {
	TclSourceLint lint;
	if (!lint.valid()) {
	    std::cerr << "Unable to create Tcl parser interpreter\n";
	} else if (argc == 3 && BU_STR_EQUAL(argv[1], "--self-test")) {
	    result = lint.self_test(argv[2]);
	} else if (argc == 4) {
	    if (lint.load_manifest(argv[1]) && lint.load_sources(argv[2], argv[3]))
		result = lint.run();
	} else {
	    std::cerr << "Usage: " << argv[0]
		<< " --self-test fixture.tcl\n       " << argv[0]
		<< " manifest cmakefiles.cmake source-root\n";
	}
    }

    Tcl_Finalize();
    return result;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
