/*                      A R G D O C . C P P
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
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/process.h"

#include "../libbu/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char *synopsis_begin = "// BEGIN GENERATED BU_ARG SYNOPSIS";
static const char *synopsis_end = "// END GENERATED BU_ARG SYNOPSIS";
static const char *options_begin = "// BEGIN GENERATED BU_ARG OPTIONS";
static const char *options_end = "// END GENERATED BU_ARG OPTIONS";

struct settings {
    enum class mode { check, render, update } operation = mode::check;
    fs::path schema_file;
    fs::path man_page;
    std::string schema_name;
    std::string invocation;
    std::vector<std::string> command;
};

struct documented_option {
    std::set<std::string> spellings;
};

struct schema_view {
    std::string invocation;
    const json *schema;
};

static void
usage(const char *program)
{
    std::cerr
	<< "Usage: " << program
	<< " [--check|--render|--update] [--schema file] [--man-page file]\n"
	<< "       [--name schema-name] [--invocation text] [-- command [args ...]]\n";
}

static std::string
slurp(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
	throw std::runtime_error("cannot read " + path.string());
    return std::string(std::istreambuf_iterator<char>(input),
	std::istreambuf_iterator<char>());
}

static void
write_file(const fs::path &path, const std::string &contents)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
	throw std::runtime_error("cannot write " + path.string());
    output << contents;
    if (!output)
	throw std::runtime_error("failed writing " + path.string());
}

static std::string
run_schema_command(const std::vector<std::string> &command)
{
    std::vector<const char *> argv;
    struct bu_process *process = NULL;
    std::string output;
    char buffer[4096];
    int count;

    argv.reserve(command.size() + 1);
    for (const std::string &word : command)
	argv.push_back(word.c_str());
    argv.push_back(NULL);

    bu_process_create(&process, argv.data(), BU_PROCESS_HIDE_WINDOW);
    if (!process || bu_process_pid(process) < 0) {
	if (process)
	    (void)bu_process_wait_n(&process, 0);
	throw std::runtime_error("could not start schema command");
    }
    while ((count = bu_process_read_n(process, BU_PROCESS_STDOUT,
	    sizeof(buffer), buffer)) > 0)
	output.append(buffer, (size_t)count);
    int status = bu_process_wait_n(&process, 0);
    if (status != 0)
	throw std::runtime_error("schema command exited with status " +
	    std::to_string(status));
    return output;
}

static const json *
schema_named(const json &value, const std::string &name)
{
    if (!value.is_object())
	return NULL;
    if (value.value("name", std::string()) == name)
	return &value;
    if (value.contains("root") && value["root"].value("name", std::string()) == name)
	return &value;
    if (value.contains("schema") &&
	value["schema"].value("name", std::string()) == name)
	return &value;
    return NULL;
}

static const json *
find_named_schema(const json &value, const std::string &name)
{
    if (value.is_array()) {
	for (const json &entry : value) {
	    const json *match = schema_named(entry, name);
	    if (match)
		return match;
	}
	for (const json &entry : value) {
	    const json *match = find_named_schema(entry, name);
	    if (match)
		return match;
	}
	return NULL;
    }
    if (!value.is_object())
	return NULL;
    const json *match = schema_named(value, name);
    if (match)
	return match;
    for (const char *member : {"subcommands", "forms"}) {
	if (!value.contains(member) || !value[member].is_array())
	    continue;
	for (const json &entry : value[member]) {
	    const json *match = find_named_schema(entry, name);
	    if (match)
		return match;
	}
    }
    if (value.contains("grammar"))
	return find_named_schema(value["grammar"], name);
    return NULL;
}

static std::string
append_word(const std::string &prefix, const std::string &word)
{
    if (prefix.empty())
	return word;
    if (word.empty())
	return prefix;
    return prefix + " " + word;
}

static void
collect_schema_views(const json &grammar, const std::string &invocation,
	std::vector<schema_view> &views)
{
    if (!grammar.is_object())
	return;
    const std::string kind = grammar.value("kind", std::string());
    if (kind == "native") {
	views.push_back({invocation.empty() ?
	    grammar.value("name", std::string()) : invocation, &grammar});
	return;
    }
    if (kind == "native_tree") {
	const json &root = grammar.at("root");
	const std::string root_invocation = invocation.empty() ?
	    root.value("name", std::string()) : invocation;
	views.push_back({root_invocation, &root});
	for (const json &node : grammar.at("subcommands"))
	    collect_schema_views(node, root_invocation, views);
	return;
    }
    if (kind == "native_forms") {
	const std::string forms_invocation = invocation.empty() ?
	    grammar.value("name", std::string()) : invocation;
	for (const json &form : grammar.at("forms"))
	    collect_schema_views(form.at("grammar"), forms_invocation, views);
	return;
    }
    if (grammar.contains("schema")) {
	const json &schema = grammar.at("schema");
	const std::string node_invocation = append_word(invocation,
	    schema.value("name", std::string()));
	views.push_back({node_invocation, &schema});
	if (grammar.contains("subcommands")) {
	    for (const json &node : grammar.at("subcommands"))
		collect_schema_views(node, node_invocation, views);
	}
    }
}

static std::string
metavariable(const json &option)
{
    std::string argument = option.value("argument", std::string("value"));
    if (argument.empty())
	argument = "value";
    return "_" + argument + "_";
}

static std::string
option_term(const json &option)
{
    std::vector<std::string> terms;
    const std::string requirement = option.value("argument_requirement",
	std::string("none"));
    const std::string argument = metavariable(option);
    const std::string shortopt = option.value("short", std::string());
    const std::string longopt = option.value("long", std::string());

    if (!shortopt.empty()) {
	std::string term = shortopt == "*" ? "`-*`" : "*-" + shortopt + "*";
	if (requirement == "required")
	    term += " " + argument;
	else if (requirement == "optional")
	    term += " [" + argument + "]";
	terms.push_back(term);
    }
    if (!longopt.empty()) {
	std::string term = "*--" + longopt + "*";
	if (requirement == "required")
	    term += "=" + argument;
	else if (requirement == "optional")
	    term += "[=" + argument + "]";
	terms.push_back(term);
    }
    std::ostringstream result;
    for (size_t i = 0; i < terms.size(); i++) {
	if (i)
	    result << ", ";
	result << terms[i];
    }
    return result.str();
}

static std::string
sentence(std::string text)
{
    if (text.empty())
	return "No description.";
    const char last = text.back();
    if (last != '.' && last != '!' && last != '?')
	text.push_back('.');
    return text;
}

static std::vector<documented_option>
collect_options(const json &schema, bool include_hidden)
{
    std::map<std::string, documented_option> grouped;
    for (const json &option : schema.at("options")) {
	if (!include_hidden && option.value("hidden", false))
	    continue;
	std::string canonical = option.value("alias_of", std::string());
	if (canonical.empty())
	    canonical = option.value("canonical", std::string());
	documented_option &entry = grouped[canonical];
	const std::string shortopt = option.value("short", std::string());
	const std::string longopt = option.value("long", std::string());
	if (!shortopt.empty())
	    entry.spellings.insert("-" + shortopt);
	if (!longopt.empty())
	    entry.spellings.insert("--" + longopt);
    }
    std::vector<documented_option> result;
    for (const auto &entry : grouped)
	result.push_back(entry.second);
    return result;
}

static std::string
render_schema_options(const json &schema)
{
    std::vector<const json *> canonical;
    std::map<std::string, std::vector<const json *>> aliases;
    std::ostringstream output;

    for (const json &option : schema.at("options")) {
	if (option.value("hidden", false))
	    continue;
	const std::string alias_of = option.value("alias_of", std::string());
	if (alias_of.empty())
	    canonical.push_back(&option);
	else
	    aliases[alias_of].push_back(&option);
    }
    for (const json *canonical_option : canonical) {
	const json &option = *canonical_option;
	const std::string name = option.value("canonical", std::string());
	output << option_term(option);
	for (const json *alias : aliases[name]) {
	    std::string term = option_term(*alias);
	    if (!term.empty())
		output << ", " << term;
	}
	output << "::\n  " << sentence(option.value("help", std::string()));
	if (option.value("repeat", false))
	    output << " May be repeated.";
	if (option.contains("values") && option["values"].is_array() &&
	    !option["values"].empty()) {
	    output << " Accepted values: ";
	    for (size_t i = 0; i < option["values"].size(); i++) {
		if (i)
		    output << ", ";
		output << "*" << option["values"][i].get<std::string>() << "*";
	    }
	    output << ".";
	}
	output << "\n\n";
    }
    return output.str();
}

static std::string
render_options(const std::vector<schema_view> &views)
{
    struct option_group {
	std::string contents;
	std::vector<std::string> invocations;
    };
    std::vector<option_group> groups;
    std::ostringstream output;
    for (const schema_view &view : views) {
	if (view.schema->at("options").empty())
	    continue;
	std::string contents = render_schema_options(*view.schema);
	auto group = std::find_if(groups.begin(), groups.end(),
	    [&contents](const option_group &candidate) {
		return candidate.contents == contents;
	    });
	if (group == groups.end())
	    groups.push_back({contents, {view.invocation}});
	else
	    group->invocations.push_back(view.invocation);
    }
    output << options_begin << "\n";
    for (const option_group &group : groups) {
	if (groups.size() > 1) {
	    if (group.invocations.size() == 1) {
		output << "=== " << group.invocations.front() << " options\n\n";
	    } else {
		output << "=== Shared options\n\nApplies to ";
		for (size_t i = 0; i < group.invocations.size(); i++) {
		    if (i)
			output << (i + 1 == group.invocations.size() ? " and " : ", ");
		    output << "*" << group.invocations[i] << "*";
		}
		output << ".\n\n";
	    }
	}
	output << group.contents;
    }
    output << options_end << "\n";
    return output.str();
}

static std::string
render_synopsis(const std::vector<schema_view> &views)
{
    std::ostringstream output;
    output << synopsis_begin << "\n[source]\n----\n";
    for (const schema_view &view : views) {
	output << view.invocation;
	if (!view.schema->at("options").empty())
	    output << " [options]";
	for (const json &operand : view.schema->at("operands")) {
	    const std::string name = operand.value("name", std::string("argument"));
	    const size_t minimum = operand.value("min", (size_t)0);
	    std::string text = "_" + name + "_";
	    if (operand["max"].is_null() || operand.value("max", (size_t)1) > 1)
		text += " ...";
	    if (minimum == 0)
		text = "[" + text + "]";
	    output << " " << text;
	}
	output << "\n";
    }
    output << "----\n" << synopsis_end << "\n";
    return output.str();
}

static std::map<std::string, std::set<bool>>
documented_options(const std::string &page)
{
    std::map<std::string, std::set<bool>> result;
    std::istringstream lines(page);
    std::string line;
    const std::regex spelling(R"((^|[*,[:space:]])(--[A-Za-z0-9][A-Za-z0-9_-]*|-[^[:space:],*]))");
    const std::regex symbolic_short(R"(`(-\*)`)");
    const std::regex argument(R"(_[^_]+_)");
    while (std::getline(lines, line)) {
	if (line.find("::") == std::string::npos)
	    continue;
	const bool has_argument = std::regex_search(line, argument);
	for (std::sregex_iterator match(line.begin(), line.end(), spelling), last;
	    match != last; ++match)
	    result[(*match)[2].str()].insert(has_argument);
	for (std::sregex_iterator match(line.begin(), line.end(), symbolic_short), last;
	    match != last; ++match)
	    result[(*match)[1].str()].insert(has_argument);
    }
    return result;
}

static std::set<std::string>
schema_spellings(const json &schema, bool include_hidden)
{
    std::set<std::string> result;
    for (const documented_option &option : collect_options(schema, include_hidden))
	result.insert(option.spellings.begin(), option.spellings.end());
    return result;
}

static std::map<std::string, bool>
schema_arguments(const json &schema, bool include_hidden)
{
    std::map<std::string, bool> canonical;
    std::map<std::string, bool> result;
    for (const json &option : schema.at("options")) {
	if (!option.value("alias_of", std::string()).empty())
	    continue;
	canonical[option.value("canonical", std::string())] =
	    option.value("argument_requirement", std::string("none")) != "none";
    }
    for (const json &option : schema.at("options")) {
	if (!include_hidden && option.value("hidden", false))
	    continue;
	std::string name = option.value("alias_of", std::string());
	if (name.empty())
	    name = option.value("canonical", std::string());
	const bool has_argument = canonical[name];
	const std::string shortopt = option.value("short", std::string());
	const std::string longopt = option.value("long", std::string());
	if (!shortopt.empty())
	    result["-" + shortopt] = has_argument;
	if (!longopt.empty())
	    result["--" + longopt] = has_argument;
    }
    return result;
}

static bool
schema_accepts_opaque_tokens(const json &schema)
{
    if (!schema.at("options").empty() ||
	schema.value("parse_policy", std::string()) != "stop_at_first_operand")
	return false;
    for (const json &operand : schema.at("operands")) {
	if (operand.value("type", std::string()) == "raw")
	    return true;
    }
    return false;
}

static std::string
replace_region(const std::string &page, const char *begin, const char *end,
	const std::string &replacement)
{
    size_t first = page.find(begin);
    size_t last = page.find(end);
    if (first == std::string::npos || last == std::string::npos || last < first)
	throw std::runtime_error(std::string("man page lacks generated region ") + begin);
    last += std::strlen(end);
    if (last < page.size() && page[last] == '\r')
	last++;
    if (last < page.size() && page[last] == '\n')
	last++;
    return page.substr(0, first) + replacement + page.substr(last);
}

static settings
parse_settings(int argc, char **argv)
{
    settings result;
    for (int i = 1; i < argc; i++) {
	std::string arg = argv[i];
	if (arg == "--") {
	    for (++i; i < argc; i++)
		result.command.push_back(argv[i]);
	    break;
	}
	if (arg == "--check")
	    result.operation = settings::mode::check;
	else if (arg == "--render")
	    result.operation = settings::mode::render;
	else if (arg == "--update")
	    result.operation = settings::mode::update;
	else if ((arg == "--schema" || arg == "--man-page" || arg == "--name" ||
		arg == "--invocation") && i + 1 < argc) {
	    std::string value = argv[++i];
	    if (arg == "--schema") result.schema_file = value;
	    if (arg == "--man-page") result.man_page = value;
	    if (arg == "--name") result.schema_name = value;
	    if (arg == "--invocation") result.invocation = value;
	} else {
	    throw std::runtime_error("unknown or incomplete argument: " + arg);
	}
    }
    if (result.schema_file.empty() == result.command.empty())
	throw std::runtime_error("specify exactly one of --schema or a command after --");
    if (result.operation != settings::mode::render && result.man_page.empty())
	throw std::runtime_error("--check and --update require --man-page");
    return result;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    try {
	settings config = parse_settings(argc, argv);
	std::string schema_text = config.schema_file.empty() ?
	    run_schema_command(config.command) : slurp(config.schema_file);
	json document = json::parse(schema_text);
	const json *grammar = &document;
	if (!config.schema_name.empty()) {
	    grammar = find_named_schema(document, config.schema_name);
	    if (!grammar)
		throw std::runtime_error("schema not found: " + config.schema_name);
	}
	std::vector<schema_view> views;
	collect_schema_views(*grammar, config.invocation, views);
	if (views.empty())
	    throw std::runtime_error("selected grammar has no renderable schemas");
	std::string synopsis = render_synopsis(views);
	std::string options = render_options(views);

	if (config.operation == settings::mode::render) {
	    std::cout << synopsis << "\n" << options;
	    return 0;
	}

	std::string page = slurp(config.man_page);
	if (config.operation == settings::mode::update) {
	    page = replace_region(page, synopsis_begin, synopsis_end, synopsis);
	    page = replace_region(page, options_begin, options_end, options);
	    write_file(config.man_page, page);
	    return 0;
	}

	std::set<std::string> expected;
	std::set<std::string> accepted;
	std::map<std::string, std::set<bool>> argument_sets;
	bool accepts_opaque_tokens = false;
	for (const schema_view &view : views) {
	    std::set<std::string> view_expected = schema_spellings(*view.schema, false);
	    std::set<std::string> view_accepted = schema_spellings(*view.schema, true);
	    expected.insert(view_expected.begin(), view_expected.end());
	    accepted.insert(view_accepted.begin(), view_accepted.end());
	    accepts_opaque_tokens = accepts_opaque_tokens ||
		schema_accepts_opaque_tokens(*view.schema);
	    for (const auto &entry : schema_arguments(*view.schema, false))
		argument_sets[entry.first].insert(entry.second);
	}
	std::map<std::string, std::set<bool>> documented = documented_options(page);
	std::set<std::string> actual;
	for (const auto &entry : documented)
	    actual.insert(entry.first);
	bool failed = false;
	for (const std::string &spelling : expected) {
	    if (!actual.count(spelling)) {
		std::cerr << config.man_page.string() << ": missing option " << spelling << "\n";
		failed = true;
	    }
	}
	for (const std::string &spelling : actual) {
	    if (!accepts_opaque_tokens && !accepted.count(spelling)) {
		std::cerr << config.man_page.string() << ": undocumented by schema " << spelling << "\n";
		failed = true;
	    }
	}
	for (const auto &entry : argument_sets) {
	    if (entry.second.size() == 1 && documented.count(entry.first) &&
		!documented[entry.first].count(*entry.second.begin())) {
		std::cerr << config.man_page.string() << ": option " << entry.first
		    << (*entry.second.begin() ? " requires an argument" : " takes no argument") << "\n";
		failed = true;
	    }
	}
	return failed ? 1 : 0;
    } catch (const std::exception &error) {
	std::cerr << argv[0] << ": " << error.what() << "\n";
	usage(argv[0]);
	return 2;
    }
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
