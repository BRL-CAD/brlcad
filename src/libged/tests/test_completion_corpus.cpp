/*              T E S T _ C O M P L E T I O N _ C O R P U S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the U.S.
 * Army Research Laboratory.
 */
/** @file test_completion_corpus.cpp
 *
 * Mine command examples from the AsciiDoc documentation and replay their
 * fixed-vocabulary tokens through libged completion.  Besides checking that
 * documented commands, options, and subcommands are offered, this validates
 * the replacement ranges used by interactive frontends to reassemble lines.
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <bu.h>
#include <ged.h>

namespace fs = std::filesystem;

struct doc_example {
    fs::path file;
    size_t line = 0;
    std::string text;
};

static std::string
trim(const std::string &s)
{
    size_t begin = 0;
    while (begin < s.size() && std::isspace((unsigned char)s[begin]))
	begin++;
    size_t end = s.size();
    while (end > begin && std::isspace((unsigned char)s[end - 1]))
	end--;
    return s.substr(begin, end - begin);
}

static std::string
strip_prompt(const std::string &s)
{
    std::string out = trim(s);
    const char *prompts[] = {"mged>", "qged>", "gsh>", "$"};
    for (const char *prompt : prompts) {
	size_t plen = std::strlen(prompt);
	if (out.compare(0, plen, prompt) == 0) {
	    out = trim(out.substr(plen));
	    break;
	}
    }
    if (out.size() > 2 && out.compare(out.size() - 2, 2, "::") == 0)
	out = trim(out.substr(0, out.size() - 2));
    return out;
}

static std::string
first_word(const std::string &s)
{
    size_t end = 0;
    while (end < s.size() && !std::isspace((unsigned char)s[end]))
	end++;
    return s.substr(0, end);
}

static std::vector<doc_example>
read_examples(const fs::path &docs)
{
    std::vector<doc_example> examples;
    const std::regex bold_re("\\*\\*([^*]+)\\*\\*");

    for (const auto &entry : fs::recursive_directory_iterator(docs)) {
	if (!entry.is_regular_file() || entry.path().extension() != ".adoc")
	    continue;
	std::ifstream input(entry.path());
	if (!input.good())
	    continue;
	bool in_examples = false;
	bool source_pending = false;
	bool in_source = false;
	bool collect_source = false;
	std::string line;
	size_t line_no = 0;
	while (std::getline(input, line)) {
	    line_no++;
	    if (line.rfind("[source", 0) == 0) {
		std::string language;
		size_t comma = line.find(',');
		size_t close = line.find(']');
		if (comma != std::string::npos && close != std::string::npos && close > comma + 1)
		    language = line.substr(comma + 1, close - comma - 1);
		collect_source = in_examples && (language.empty() || language == "mged" ||
			language == "gsh" || language == "qged");
		source_pending = true;
		continue;
	    }
	    if (line == "----" && (source_pending || in_source)) {
		in_source = source_pending;
		source_pending = false;
		if (!in_source)
		    continue;
		else
		    continue;
	    }
	    if (in_source) {
		if (collect_source) {
		    std::string text = strip_prompt(line);
		    if (!text.empty() && text[0] != '#')
			examples.push_back({entry.path(), line_no, text});
		}
		continue;
	    }
	    size_t heading_end = line.find(' ');
	    if (heading_end != std::string::npos && heading_end >= 2 &&
		    line.find_first_not_of('=', 0) == heading_end)
		in_examples = (line.substr(heading_end + 1) == "EXAMPLES");
	    if (in_examples) {
		for (std::sregex_iterator i(line.begin(), line.end(), bold_re), end; i != end; ++i) {
		    std::string text = strip_prompt((*i)[1].str());
		    if (!text.empty())
			examples.push_back({entry.path(), line_no, text});
		}
	    }
	}
    }
    std::sort(examples.begin(), examples.end(), [](const doc_example &a, const doc_example &b) {
	if (a.file != b.file)
	    return a.file < b.file;
	return a.line < b.line;
    });
    return examples;
}

static bool
plain_fixed_token(const std::string &token)
{
    if (token.empty())
	return false;
    for (char c : token) {
	if (std::isspace((unsigned char)c) || c == '"' || c == '\'' || c == '{' || c == '}' ||
		c == '[' || c == ']' || c == '\\' || c == '*' || c == '?' || c == '$')
	    return false;
    }
    return true;
}

static bool
looks_like_option(const std::string &token)
{
    if (token.size() < 2 || token[0] != '-')
	return false;
    return !(std::isdigit((unsigned char)token[1]) || token[1] == '.');
}

static bool
has_candidate(const struct ged_cmd_completion_result *result, const std::string &expected)
{
    if (!result || !result->completion_candidates)
	return false;
    for (size_t i = 0; i < result->completion_count; i++) {
	if (result->completion_candidates[i] && expected == result->completion_candidates[i])
	    return true;
    }
    return false;
}

static int
discard_log(void *, void *data)
{
    return data ? (int)std::strlen((const char *)data) : 0;
}

static bool
candidate_reanalyzes(struct ged *gedp, const std::string &line, size_t changed_at,
	const doc_example &example, const char *candidate)
{
    struct ged_cmd_analysis analysis = {0, NULL};
    bu_log_add_hook(discard_log, NULL);
    int analyze_ret = ged_cmd_analyze(gedp, line.c_str(), &analysis);
    bu_log_delete_hook(discard_log, NULL);
    if (analyze_ret != 0) {
	bu_log("ERROR: %s:%zu: reconstructed candidate [%s] could not be analyzed in [%s]\n",
		example.file.string().c_str(), example.line, candidate, line.c_str());
	return false;
    }

    bool recognized = false;
    int matched_role = -1;
    int matched_state = -1;
    size_t matched_start = 0;
    for (size_t i = 0; i < analysis.token_count; i++) {
	const struct ged_cmd_analysis_token *token = &analysis.tokens[i];
	if (token->char_end == line.size()) {
	    matched_role = (int)token->role;
	    matched_state = (int)token->semantic_state;
	    matched_start = token->char_start;
	}
	if (token->char_start <= changed_at && token->char_end == line.size()) {
	    recognized = token->role != GED_CMD_TOKEN_UNKNOWN &&
		token->semantic_state != GED_CMD_SEMANTIC_INVALID;
	    break;
	}
    }
    if (!recognized) {
	bu_log("ERROR: %s:%zu: reconstructed candidate [%s] is not recognized as valid or incomplete in [%s] "
		"(changed at %zu, final token starts %zu, role %d, state %d; source [%s])\n",
		example.file.string().c_str(), example.line, candidate, line.c_str(), changed_at,
		matched_start, matched_role, matched_state, example.text.c_str());
    }
    ged_cmd_analysis_clear(&analysis);
    return recognized;
}

int
main(int argc, char **argv)
{
    if (argc != 3) {
	bu_log("Usage: %s mann-doc-directory database.g\n", argv[0]);
	return 2;
    }

    fs::path docs(argv[1]);
    if (!fs::is_directory(docs)) {
	bu_log("ERROR: documentation directory does not exist: %s\n", argv[1]);
	return 2;
    }

    struct ged *gedp = ged_open("db", argv[2], 1);
    if (!gedp) {
	bu_log("ERROR: unable to open completion test database: %s\n", argv[2]);
	return 2;
    }

    const std::vector<doc_example> examples = read_examples(docs);
    size_t command_examples = 0;
    size_t fixed_tokens = 0;
    size_t coverage_gaps = 0;
    size_t prefix_checks = 0;
    size_t reconstruction_checks = 0;
    size_t candidate_reconstructions = 0;
    size_t semantic_reconstructions = 0;
    size_t geometry_prefix_checks = 0;
    size_t geometry_reconstructions = 0;
    size_t nested_path_checks = 0;
    size_t failures = 0;
    std::set<std::string> coverage_gap_keys;
    std::map<std::string, doc_example> coverage_gap_examples;

    for (const doc_example &example : examples) {
	std::string cmd = first_word(example.text);
	if (cmd.empty() || !ged_cmd_exists(cmd.c_str()))
	    continue;
	command_examples++;
	auto record_coverage_gap = [&](const std::string &token) {
	    std::string key = cmd + "\037" + token;
	    coverage_gaps++;
	    coverage_gap_keys.insert(key);
	    coverage_gap_examples.emplace(key, example);
	};

	struct ged_cmd_analysis analysis = {0, NULL};
	bu_log_add_hook(discard_log, NULL);
	int analyze_ret = ged_cmd_analyze(gedp, example.text.c_str(), &analysis);
	bu_log_delete_hook(discard_log, NULL);
	if (analyze_ret != 0) {
	    bu_log("ERROR: %s:%zu: analysis failed for [%s]\n",
		    example.file.string().c_str(), example.line, example.text.c_str());
	    failures++;
	    continue;
	}

	for (size_t ti = 0; ti < analysis.token_count; ti++) {
	    const struct ged_cmd_analysis_token *at = &analysis.tokens[ti];
	    if (at->char_end <= at->char_start || at->char_end > example.text.size())
		continue;
	    std::string token = example.text.substr(at->char_start, at->char_end - at->char_start);

	    if (at->value_type == BU_CMD_VALUE_DB_OBJECT || at->value_type == BU_CMD_VALUE_DB_PATH) {
		std::string seed = (at->value_type == BU_CMD_VALUE_DB_PATH && token.find('/') != std::string::npos) ? "/a" : "a";
		std::string input = example.text.substr(0, at->char_start) + seed;
		struct ged_cmd_completion_result result = GED_CMD_COMPLETION_RESULT_NULL;
		(void)ged_cmd_complete_result(gedp, input.c_str(), input.size(), &result);
		geometry_prefix_checks++;
		if (result.completion_count) {
		    size_t prefix_len = result.prefix ? std::strlen(result.prefix) : 0;
		    size_t expected_start = (input.size() >= prefix_len) ? input.size() - prefix_len : input.size();
		    if (result.replacement_start != expected_start || result.replacement_start < at->char_start ||
			    result.replacement_end != input.size()) {
			bu_log("ERROR: %s:%zu: geometry completion range [%zu,%zu] does not replace prefix at %zu in [%s]\n",
				example.file.string().c_str(), example.line, result.replacement_start,
				result.replacement_end, expected_start, input.c_str());
			failures++;
		    }
		    for (size_t ci = 0; ci < result.completion_count; ci++) {
			if (!result.completion_candidates[ci])
			    continue;
			std::string rebuilt = input;
			if (result.replacement_start <= result.replacement_end && result.replacement_end <= rebuilt.size()) {
			    rebuilt.replace(result.replacement_start,
				    result.replacement_end - result.replacement_start, result.completion_candidates[ci]);
			    std::string expected = input.substr(0, result.replacement_start) + result.completion_candidates[ci];
			    geometry_reconstructions++;
			    bool command_prefix_ok = rebuilt.compare(0, at->char_start,
				    example.text, 0, at->char_start) == 0;
			    if (rebuilt != expected || !command_prefix_ok) {
				bu_log("ERROR: %s:%zu: geometry candidate [%s] rebuilt [%s], expected [%s]\n",
					example.file.string().c_str(), example.line,
					result.completion_candidates[ci], rebuilt.c_str(), expected.c_str());
				failures++;
			    } else {
				semantic_reconstructions++;
				if (!candidate_reanalyzes(gedp, rebuilt, result.replacement_start,
					example, result.completion_candidates[ci]))
				    failures++;
			    }
			}
		    }
		} else {
		    record_coverage_gap(token);
		}
		ged_cmd_completion_result_clear(&result);

		if (at->value_type == BU_CMD_VALUE_DB_PATH) {
		    std::string nested_input = example.text.substr(0, at->char_start) + "all.g/t";
		    struct ged_cmd_completion_result nested = GED_CMD_COMPLETION_RESULT_NULL;
		    (void)ged_cmd_complete_result(gedp, nested_input.c_str(), nested_input.size(), &nested);
		    nested_path_checks++;
		    if (has_candidate(&nested, "tor.r")) {
			size_t expected_start = at->char_start + std::strlen("all.g/");
			if (nested.replacement_start != expected_start || nested.replacement_end != nested_input.size()) {
			    bu_log("ERROR: %s:%zu: nested path range [%zu,%zu] does not replace leaf at %zu in [%s]\n",
				    example.file.string().c_str(), example.line, nested.replacement_start,
				    nested.replacement_end, expected_start, nested_input.c_str());
			    failures++;
			} else {
			    std::string rebuilt = nested_input;
			    rebuilt.replace(nested.replacement_start,
				    nested.replacement_end - nested.replacement_start, "tor.r");
			    std::string expected = example.text.substr(0, at->char_start) + "all.g/tor.r";
			    if (rebuilt != expected) {
				bu_log("ERROR: %s:%zu: nested path rebuilt [%s], expected [%s]\n",
					example.file.string().c_str(), example.line, rebuilt.c_str(), expected.c_str());
				failures++;
			    } else {
				semantic_reconstructions++;
				if (!candidate_reanalyzes(gedp, rebuilt, nested.replacement_start,
					example, "tor.r"))
				    failures++;
			    }
			}
		    } else {
			record_coverage_gap("nested-path");
		    }
		    ged_cmd_completion_result_clear(&nested);
		}
	    }

	    bool recognized = (at->role == GED_CMD_TOKEN_COMMAND ||
		    ((at->role == GED_CMD_TOKEN_SUBCOMMAND || at->role == GED_CMD_TOKEN_OPTION) &&
		     at->semantic_state == GED_CMD_SEMANTIC_VALID));
	    if (!recognized && looks_like_option(token)) {
		record_coverage_gap(token);
	    }
	    bool required = recognized;
	    /* Attached option arguments are valid documentation, but the candidate
	     * is only the option-name portion.  Leave those to schema validation
	     * until the corpus lexer models the attachment spelling explicitly. */
	    if (at->role == GED_CMD_TOKEN_OPTION && token.size() > 2 && token[0] == '-' &&
		    token[1] != '-' && token.find('=') == std::string::npos) {
		record_coverage_gap(token);
		required = false;
	    }
	    if (token == "--" || token == "-")
		required = false;
	    if (!required || !plain_fixed_token(token))
		continue;

	    fixed_tokens++;
	    bool offered = false;
	    for (size_t chars = 1; chars <= token.size(); chars++) {
		std::string input = example.text.substr(0, at->char_start + chars);
		struct ged_cmd_completion_result result = GED_CMD_COMPLETION_RESULT_NULL;
		(void)ged_cmd_complete_result(gedp, input.c_str(), input.size(), &result);
		prefix_checks++;
		if (result.completion_count) {
		    if (result.replacement_start != at->char_start || result.replacement_end != input.size()) {
			bu_log("ERROR: %s:%zu: completion range [%zu,%zu] does not replace token at %zu in [%s]\n",
				example.file.string().c_str(), example.line, result.replacement_start,
				result.replacement_end, at->char_start, input.c_str());
			failures++;
		    }
		    for (size_t ci = 0; ci < result.completion_count; ci++) {
			if (!result.completion_candidates[ci])
			    continue;
			std::string rebuilt = input;
			if (result.replacement_start <= result.replacement_end &&
				result.replacement_end <= rebuilt.size()) {
			    rebuilt.replace(result.replacement_start,
				    result.replacement_end - result.replacement_start,
				    result.completion_candidates[ci]);
			    std::string expected = example.text.substr(0, at->char_start) + result.completion_candidates[ci];
			    candidate_reconstructions++;
			    if (rebuilt != expected) {
				bu_log("ERROR: %s:%zu: candidate [%s] rebuilt [%s], expected [%s]\n",
					example.file.string().c_str(), example.line,
					result.completion_candidates[ci], rebuilt.c_str(), expected.c_str());
				failures++;
			    } else {
				semantic_reconstructions++;
				if (!candidate_reanalyzes(gedp, rebuilt, result.replacement_start,
					example, result.completion_candidates[ci]))
				    failures++;
			    }
			}
		    }
		}
		if (has_candidate(&result, token)) {
		    offered = true;
		    if (result.replacement_start > result.replacement_end || result.replacement_end > input.size()) {
			bu_log("ERROR: %s:%zu: invalid replacement [%zu,%zu] for [%s]\n",
				example.file.string().c_str(), example.line, result.replacement_start,
				result.replacement_end, input.c_str());
			failures++;
		    } else {
			std::string rebuilt = input;
			rebuilt.replace(result.replacement_start,
				result.replacement_end - result.replacement_start, token);
			std::string expected = example.text.substr(0, at->char_end);
			reconstruction_checks++;
			if (rebuilt != expected) {
			    bu_log("ERROR: %s:%zu: completing [%s] as [%s] rebuilt [%s], expected [%s]\n",
				    example.file.string().c_str(), example.line, input.c_str(), token.c_str(),
				    rebuilt.c_str(), expected.c_str());
			    failures++;
			}
		    }
		}
		ged_cmd_completion_result_clear(&result);
	    }

	    if (!offered) {
		if (at->role == GED_CMD_TOKEN_COMMAND) {
		    bu_log("ERROR: %s:%zu: documented command [%s] is never offered\n",
			    example.file.string().c_str(), example.line, token.c_str());
		    failures++;
		} else {
		    record_coverage_gap(token);
		}
	    }
	}
	ged_cmd_analysis_clear(&analysis);
    }


    bu_log("completion corpus: %zu documentation fragments, %zu GED examples, %zu fixed tokens, "
	    "%zu prefixes, %zu documented and %zu candidate reconstructions (%zu reparsed); %zu geometry prefixes and "
	    "%zu geometry reconstructions and %zu nested paths; %zu schema coverage gaps (%zu unique)\n",
	    examples.size(), command_examples, fixed_tokens, prefix_checks, reconstruction_checks,
	    candidate_reconstructions, semantic_reconstructions, geometry_prefix_checks, geometry_reconstructions, nested_path_checks,
	    coverage_gaps, coverage_gap_keys.size());
    if (std::getenv("BRLCAD_COMPLETION_CORPUS_VERBOSE")) {
	for (const auto &gap : coverage_gap_examples) {
	    size_t split = gap.first.find('\037');
	    std::string gap_cmd = gap.first.substr(0, split);
	    std::string gap_token = (split == std::string::npos) ? std::string() : gap.first.substr(split + 1);
	    bu_log("COVERAGE: %s:%zu: %s -> %s [%s]\n", gap.second.file.string().c_str(),
		    gap.second.line, gap_cmd.c_str(), gap_token.c_str(), gap.second.text.c_str());
	}
    }
    ged_close(gedp);
    return failures ? 1 : 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
