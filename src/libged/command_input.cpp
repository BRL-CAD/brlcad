/*                 C O M M A N D _ I N P U T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "bu/malloc.h"
#include "bu/str.h"

#include "./tab_complete_private.h"


void
ged_input_parse_free(struct ged_input_parse *parsed)
{
    if (!parsed)
	return;
    if (parsed->argv)
	bu_free(parsed->argv, "argv array");
    if (parsed->copy)
	bu_free(parsed->copy, "input copy");
    parsed->copy = NULL;
    parsed->argv = NULL;
    parsed->argc = 0;
    parsed->cursor_arg = 0;
    parsed->input_len = 0;
    parsed->cursor_seed.clear();
    parsed->cursor_path_seed.clear();
    parsed->cursor_content_start = 0;
    parsed->cursor_component_start = 0;
    parsed->cursor_value_start = 0;
    parsed->cursor_replace_end = 0;
    parsed->char_starts.clear();
    parsed->char_ends.clear();
}


int
ged_input_parse_line(struct ged_input_parse *parsed, const char *input,
	size_t cursor_pos)
{
    std::vector<size_t> raw_starts;
    std::vector<size_t> raw_ends;
    bool in_token = false;
    bool in_quote = false;
    bool escaped = false;

    if (!parsed || !input)
	return -1;
    parsed->argv = NULL;
    parsed->argc = 0;
    parsed->cursor_arg = 0;
    parsed->copy = NULL;
    parsed->input_len = strlen(input);
    cursor_pos = std::min(cursor_pos, parsed->input_len);
    parsed->cursor_seed.clear();
    parsed->cursor_path_seed.clear();
    parsed->cursor_content_start = cursor_pos;
    parsed->cursor_component_start = cursor_pos;
    parsed->cursor_value_start = cursor_pos;
    parsed->cursor_replace_end = cursor_pos;
    parsed->copy = bu_strdup(input);

    /* Record source spans before bu_argv_from_string removes quotes and
     * escapes in place. */
    for (size_t i = 0; i < parsed->input_len; i++) {
	unsigned char c = (unsigned char)input[i];
	if (!in_token) {
	    if (isspace(c))
		continue;
	    raw_starts.push_back(i);
	    in_token = true;
	}
	if (escaped) {
	    escaped = false;
	    continue;
	}
	if (c == '\\') {
	    escaped = true;
	    continue;
	}
	if (c == '"') {
	    in_quote = !in_quote;
	    continue;
	}
	if (!in_quote && isspace(c)) {
	    raw_ends.push_back(i);
	    in_token = false;
	}
    }
    if (in_token)
	raw_ends.push_back(parsed->input_len);

    size_t len = parsed->input_len;
    while (len > 0 && isspace((unsigned char)parsed->copy[len - 1]))
	parsed->copy[--len] = '\0';
    parsed->argv = (char **)bu_calloc(parsed->input_len + 1,
	sizeof(char *), "argv array");
    parsed->argc = bu_argv_from_string(parsed->argv, parsed->input_len,
	parsed->copy);
    parsed->char_starts.resize(parsed->argc);
    parsed->char_ends.resize(parsed->argc);

    size_t raw_index = 0;
    for (size_t i = 0; i < parsed->argc; i++) {
	int span_found = 0;
	while (raw_index < raw_starts.size() && raw_index < raw_ends.size()) {
	    std::string raw(input + raw_starts[raw_index],
		raw_ends[raw_index] - raw_starts[raw_index]);
	    std::vector<char> raw_copy(raw.begin(), raw.end());
	    char *raw_argv[2] = {NULL, NULL};
	    raw_copy.push_back('\0');
	    size_t raw_argc = bu_argv_from_string(raw_argv, 1, raw_copy.data());
	    if (raw_argc == 1 && BU_STR_EQUAL(raw_argv[0], parsed->argv[i])) {
		parsed->char_starts[i] = raw_starts[raw_index];
		parsed->char_ends[i] = raw_ends[raw_index];
		raw_index++;
		span_found = 1;
		break;
	    }
	    raw_index++;
	}
	if (!span_found) {
	    parsed->char_starts[i] = (size_t)(parsed->argv[i] - parsed->copy);
	    parsed->char_ends[i] = parsed->char_starts[i] + strlen(parsed->argv[i]);
	}
    }

    parsed->cursor_arg = parsed->argc;
    for (size_t i = 0; i < parsed->argc; i++) {
	if (cursor_pos <= parsed->char_ends[i]) {
	    parsed->cursor_arg = i;
	    break;
	}
    }

    if (parsed->cursor_arg < parsed->argc &&
	    cursor_pos >= parsed->char_starts[parsed->cursor_arg]) {
	size_t token_start = parsed->char_starts[parsed->cursor_arg];
	size_t token_end = parsed->char_ends[parsed->cursor_arg];
	size_t limit = std::min(cursor_pos, token_end);
	bool prefix_started = false;
	bool escaped_prefix = false;
	bool quoted_prefix = false;
	parsed->cursor_content_start = token_start;
	parsed->cursor_component_start = token_start;
	parsed->cursor_value_start = token_start;
	parsed->cursor_replace_end = limit;
	for (size_t i = token_start; i < limit; i++) {
	    unsigned char c = (unsigned char)input[i];
	    if (escaped_prefix) {
		if (!prefix_started) {
		    parsed->cursor_content_start = i - 1;
		    parsed->cursor_component_start = i - 1;
		    parsed->cursor_value_start = i - 1;
		    prefix_started = true;
		}
		parsed->cursor_seed.push_back((char)c);
		if (c == '/' || c == '@' || c == '\\')
		    parsed->cursor_path_seed.push_back('\\');
		parsed->cursor_path_seed.push_back((char)c);
		escaped_prefix = false;
		continue;
	    }
	    if (c == '\\') {
		escaped_prefix = true;
		continue;
	    }
	    if (c == '"') {
		quoted_prefix = !quoted_prefix;
		if (!prefix_started) {
		    parsed->cursor_content_start = i + 1;
		    parsed->cursor_component_start = i + 1;
		    parsed->cursor_value_start = i + 1;
		}
		continue;
	    }
	    if (!prefix_started) {
		parsed->cursor_content_start = i;
		parsed->cursor_component_start = i;
		parsed->cursor_value_start = i;
		prefix_started = true;
	    }
	    parsed->cursor_seed.push_back((char)c);
	    parsed->cursor_path_seed.push_back((char)c);
	    if (c == '/')
		parsed->cursor_component_start = i + 1;
	    if (c == '=')
		parsed->cursor_value_start = i + 1;
	}
	if (!quoted_prefix && limit > token_start && input[limit - 1] == '"')
	    parsed->cursor_replace_end = limit - 1;
    }
    return 0;
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
