/*          M I M E _ R E G I S T R Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <sys/stat.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bu/malloc.h"
#include "bu/mime_runtime.h"
#include "bu/str.h"
#include "mime_private.h"
#include "mime_png.h"

struct mime_test {
    size_t offset;
    size_t last_offset;
    std::vector<unsigned char> value;
    std::vector<unsigned char> mask;
};

struct mime_rule {
    std::vector<mime_test> tests;
    bu_mime_evidence evidence;
    unsigned int priority;
};

struct mime_definition {
    std::string name;
    std::string description;
    std::vector<std::string> aliases;
    std::vector<std::string> extensions;
    std::vector<mime_rule> rules;
};

struct bu_mime_registry {
    std::mutex mutex;
    size_t scan_limit;
    bu_mime_token next_token = 1;
    bu_mime_token builtin_end = 1;
    std::map<bu_mime_token, mime_definition> definitions;
    std::map<std::string, std::string> names;
    std::map<std::string, std::set<bu_mime_token>> extensions;
};

struct mime_builtin {
    const char *name;
    const char *extensions;
    const char *description;
};

static const mime_builtin mime_builtins[] = {
#include "mime_builtin.inc"
};

struct mime_builtin_alias {
    const char *alias;
    const char *canonical;
};

static const mime_builtin_alias mime_aliases[] = {
#include "mime_aliases.inc"
};

static std::string
mime_lower(const char *str)
{
    std::string ret = str ? str : "";
    for (char &c : ret)
	if (c >= 'A' && c <= 'Z')
	    c += 'a' - 'A';
    return ret;
}

static bool
mime_name_valid(const std::string &name)
{
    size_t slash = name.find('/');
    /* RFC 6838 limits each component to 127 characters. */
    const size_t component_limit = 127;
    if (slash == std::string::npos || !slash || slash + 1 == name.size() ||
	slash > component_limit || name.size() - slash - 1 > component_limit)
	return false;
    for (size_t i = 0; i < name.size(); ++i) {
	char c = name[i];
	if (i == slash)
	    continue;
	if ((i == 0 || i == slash + 1) &&
	    !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
	    return false;
	if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
	      c == '!' || c == '#' || c == '$' || c == '&' || c == '-' ||
	      c == '^' || c == '_' || c == '.' || c == '+'))
	    return false;
    }
    return true;
}

static bool
mime_extension_valid(const std::string &extension)
{
    if (extension.empty() || extension.front() == '.' || extension.back() == '.')
	return false;
    for (unsigned char c : extension)
	if (c <= ' ' || c >= 127 || c == '/' || c == '\\' || c == '*' || c == '?')
	    return false;
    return true;
}

static void
mime_index(bu_mime_registry *registry, bu_mime_token token, const mime_definition &definition)
{
    registry->names[definition.name] = definition.name;
    for (const auto &alias : definition.aliases)
	registry->names[alias] = definition.name;
    for (const auto &extension : definition.extensions)
	registry->extensions[extension].insert(token);
}

static bu_mime_token
mime_insert(bu_mime_registry *registry, const mime_definition &definition)
{
    if (registry->next_token == std::numeric_limits<bu_mime_token>::max())
	return 0;
    bu_mime_token token = registry->next_token++;
    registry->definitions.emplace(token, definition);
    mime_index(registry, token, definition);
    return token;
}

extern "C" bu_mime_token
bu_mime_register(bu_mime_registry *registry, const bu_mime_definition *definition)
{
    if (!registry || !definition || definition->abi_version != BU_MIME_ABI_VERSION ||
	definition->struct_size < sizeof(*definition) ||
	(definition->alias_count && !definition->aliases) ||
	(definition->extension_count && !definition->extensions) ||
	(definition->rule_count && !definition->rules))
	return 0;

    mime_definition copy;
    copy.name = mime_lower(definition->name);
    if (!mime_name_valid(copy.name))
	return 0;
    copy.description = definition->description ? definition->description : "";
    for (size_t i = 0; i < definition->alias_count; ++i) {
	std::string alias = mime_lower(definition->aliases[i]);
	if (!mime_name_valid(alias))
	    return 0;
	copy.aliases.push_back(alias);
    }
    for (size_t i = 0; i < definition->extension_count; ++i) {
	std::string extension = mime_lower(definition->extensions[i]);
	if (!mime_extension_valid(extension))
	    return 0;
	copy.extensions.push_back(extension);
    }
    for (size_t i = 0; i < definition->rule_count; ++i) {
	const bu_mime_rule &rule = definition->rules[i];
	if (!rule.tests || !rule.test_count || rule.priority > BU_MIME_MAX_PRIORITY ||
	    (rule.evidence != BU_MIME_EVIDENCE_HEURISTIC &&
	     rule.evidence != BU_MIME_EVIDENCE_SIGNATURE))
	    return 0;
	mime_rule rule_copy;
	rule_copy.evidence = rule.evidence;
	rule_copy.priority = rule.priority;
	for (size_t j = 0; j < rule.test_count; ++j) {
	    const bu_mime_test &test = rule.tests[j];
	    if (!test.value || !test.length || test.offset > test.last_offset ||
		test.last_offset > registry->scan_limit ||
		test.length > registry->scan_limit - test.last_offset)
		return 0;
	    mime_test test_copy;
	    test_copy.offset = test.offset;
	    test_copy.last_offset = test.last_offset;
	    test_copy.value.assign(test.value, test.value + test.length);
	    if (test.mask)
		test_copy.mask.assign(test.mask, test.mask + test.length);
	    rule_copy.tests.push_back(std::move(test_copy));
	}
	copy.rules.push_back(std::move(rule_copy));
    }

    std::lock_guard<std::mutex> guard(registry->mutex);
    std::vector<std::string> names = copy.aliases;
    names.push_back(copy.name);
    for (const auto &name : names) {
	auto it = registry->names.find(name);
	if (it != registry->names.end() && it->second != copy.name)
	    return 0;
    }
    return mime_insert(registry, copy);
}

extern "C" int
bu_mime_unregister(bu_mime_registry *registry, bu_mime_token token)
{
    if (!registry)
	return 0;
    std::lock_guard<std::mutex> guard(registry->mutex);
    if (token < registry->builtin_end || !registry->definitions.erase(token))
	return 0;
    registry->names.clear();
    registry->extensions.clear();
    for (const auto &entry : registry->definitions)
	mime_index(registry, entry.first, entry.second);
    return 1;
}

extern "C" int
bu_mime_canonical(bu_mime_registry *registry, const char *name, bu_vls *canonical)
{
    if (!canonical)
	return 0;
    std::string requested = mime_lower(name);
    bu_vls_trunc(canonical, 0);
    if (!registry)
	return 0;
    std::lock_guard<std::mutex> guard(registry->mutex);
    auto it = registry->names.find(requested);
    if (it == registry->names.end())
	return 0;
    bu_vls_strcpy(canonical, it->second.c_str());
    return 1;
}

extern "C" int
bu_mime_description(bu_mime_registry *registry, const char *name, bu_vls *description)
{
    if (!description)
	return 0;
    std::string requested = mime_lower(name);
    bu_vls_trunc(description, 0);
    if (!registry)
	return 0;
    std::lock_guard<std::mutex> guard(registry->mutex);
    auto type = registry->names.find(requested);
    if (type == registry->names.end())
	return 0;
    for (auto it = registry->definitions.rbegin(); it != registry->definitions.rend(); ++it) {
	if (it->second.name == type->second && !it->second.description.empty()) {
	    bu_vls_strcpy(description, it->second.description.c_str());
	    break;
	}
    }
    return 1;
}

extern "C" void
bu_mime_list_free(bu_mime_info *info, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
	bu_free(const_cast<char *>(info[i].name), "MIME name");
	bu_free(const_cast<char *>(info[i].description), "MIME description");
	for (size_t j = 0; j < info[i].alias_count; ++j)
	    bu_free(const_cast<char *>(info[i].aliases[j]), "MIME alias");
	for (size_t j = 0; j < info[i].extension_count; ++j)
	    bu_free(const_cast<char *>(info[i].extensions[j]), "MIME extension");
	bu_free(info[i].aliases, "MIME aliases");
	bu_free(info[i].extensions, "MIME extensions");
    }
    bu_free(info, "MIME metadata");
}

extern "C" int
bu_mime_list(bu_mime_registry *registry, const char *name, bu_mime_info **info, size_t *count)
{
    if (!info || !count)
	return 0;
    *info = NULL;
    *count = 0;
    if (!registry)
	return 0;
    std::lock_guard<std::mutex> guard(registry->mutex);
    std::string requested;
    if (name) {
	auto found = registry->names.find(mime_lower(name));
	if (found == registry->names.end())
	    return 1;
	requested = found->second;
    }
    struct metadata {
	std::string description;
	std::set<std::string> aliases;
	std::set<std::string> extensions;
    };
    std::map<std::string, metadata> types;
    for (const auto &entry : registry->definitions) {
	const auto &definition = entry.second;
	if (name && definition.name != requested)
	    continue;
	auto &type = types[definition.name];
	if (!definition.description.empty())
	    type.description = definition.description;
	type.aliases.insert(definition.aliases.begin(), definition.aliases.end());
	type.aliases.erase(definition.name);
	type.extensions.insert(definition.extensions.begin(), definition.extensions.end());
    }
    if (types.empty())
	return 1;
    *info = static_cast<bu_mime_info *>(bu_calloc(types.size(), sizeof(bu_mime_info), "MIME metadata"));
    for (const auto &entry : types) {
	auto &output = (*info)[(*count)++];
	output.name = bu_strdup(entry.first.c_str());
	output.description = bu_strdup(entry.second.description.c_str());
	output.alias_count = entry.second.aliases.size();
	output.extension_count = entry.second.extensions.size();
	if (output.alias_count)
	    output.aliases = static_cast<const char **>(bu_calloc(output.alias_count, sizeof(char *), "MIME aliases"));
	if (output.extension_count)
	    output.extensions = static_cast<const char **>(bu_calloc(output.extension_count, sizeof(char *), "MIME extensions"));
	size_t i = 0;
	for (const auto &alias : entry.second.aliases)
	    output.aliases[i++] = bu_strdup(alias.c_str());
	i = 0;
	for (const auto &extension : entry.second.extensions)
	    output.extensions[i++] = bu_strdup(extension.c_str());
    }
    return 1;
}

extern "C" bu_mime_status
bu_mime_resolve_candidates(bu_mime_registry *registry, const char *format, bu_mime_result **result)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    if (!registry || !format || !format[0])
	return BU_MIME_ERROR;
    bu_vls canonical = BU_VLS_INIT_ZERO;
    if (bu_mime_canonical(registry, format, &canonical)) {
	*result = static_cast<bu_mime_result *>(bu_calloc(1, sizeof(bu_mime_result), "MIME resolution"));
	bu_mime_status status = bu_mime_result_add(registry, *result, bu_vls_cstr(&canonical),
	    BU_MIME_EVIDENCE_EXTENSION, BU_MIME_DEFAULT_PRIORITY);
	bu_vls_free(&canonical);
	if (status == BU_MIME_ERROR) {
	    bu_mime_result_free(*result);
	    *result = NULL;
	}
	return status;
    }
    bu_vls_free(&canonical);
    if (!mime_extension_valid(format)) {
	*result = static_cast<bu_mime_result *>(bu_calloc(1, sizeof(bu_mime_result), "MIME resolution"));
	return BU_MIME_UNKNOWN_RESULT;
    }
    std::string filename = std::string("format.") + format;
    return bu_mime_detect(registry, filename.c_str(), NULL, 0, 0, result);
}

extern "C" bu_mime_status
bu_mime_resolve(bu_mime_registry *registry, const char *format, bu_vls *canonical)
{
    if (!canonical)
	return BU_MIME_ERROR;
    /* format may point into the output vls. */
    std::string requested = format ? format : "";
    bu_vls_trunc(canonical, 0);
    bu_mime_result *result = NULL;
    bu_mime_status status = bu_mime_resolve_candidates(registry, requested.c_str(), &result);
    if (status == BU_MIME_MATCH)
	bu_vls_strcpy(canonical, result->candidates[0].name);
    bu_mime_result_free(result);
    return status;
}

static bool
mime_bytes_match(const mime_test &test, const unsigned char *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
	unsigned char mask = test.mask.empty() ? UCHAR_MAX : test.mask[i];
	if ((data[i] & mask) != (test.value[i] & mask))
	    return false;
    }
    return true;
}

/* Return -1 only if a matching prefix could become a complete match. */
static int
mime_test_match(const mime_test &test, const unsigned char *data, size_t size, bool complete)
{
    size_t length = test.value.size();
    if (size >= length && test.offset <= size - length) {
	size_t end = std::min(test.last_offset, size - length);
	for (size_t offset = test.offset; offset <= end; ++offset)
	    if (mime_bytes_match(test, data + offset, length))
		return 1;
    }
    if (complete)
	return 0;
    if (test.last_offset > size)
	return -1;
    size_t start = std::max(test.offset, size >= length ? size - length + 1 : 0);
    for (size_t offset = start; offset <= test.last_offset; ++offset)
	if (mime_bytes_match(test, data + offset, size - offset))
	    return -1;
    return 0;
}

static bool
mime_rank_greater(const bu_mime_candidate &a, const bu_mime_candidate &b)
{
    if (a.evidence != b.evidence)
	return a.evidence > b.evidence;
    return a.priority > b.priority;
}

static bool
mime_candidate_before(const bu_mime_candidate &a, const bu_mime_candidate &b)
{
    if (mime_rank_greater(a, b))
	return true;
    if (mime_rank_greater(b, a))
	return false;
    return bu_strcmp(a.name, b.name) < 0;
}

static bu_mime_status
mime_result_status(const bu_mime_result *result)
{
    if (!result->count)
	return BU_MIME_UNKNOWN_RESULT;
    if (result->count > 1 && !mime_rank_greater(result->candidates[0], result->candidates[1]))
	return BU_MIME_AMBIGUOUS;
    return BU_MIME_MATCH;
}

extern "C" bu_mime_status
bu_mime_result_add(bu_mime_registry *registry, bu_mime_result *result, const char *name,
		   bu_mime_evidence evidence, unsigned int priority)
{
    if (!registry || !result || (result->count && !result->candidates) || result->count >= std::numeric_limits<size_t>::max() / sizeof(bu_mime_candidate) ||
	priority > BU_MIME_MAX_PRIORITY ||
	evidence < BU_MIME_EVIDENCE_EXTENSION || evidence > BU_MIME_EVIDENCE_SIGNATURE)
	return BU_MIME_ERROR;
    bu_vls canonical = BU_VLS_INIT_ZERO;
    if (!bu_mime_canonical(registry, name, &canonical)) {
	bu_vls_free(&canonical);
	return BU_MIME_ERROR;
    }
    bu_mime_candidate candidate = {bu_vls_cstr(&canonical), evidence, priority, BU_MIME_TRAITS_INIT_ZERO};
    size_t index = 0;
    for (; index < result->count; ++index)
	if (BU_STR_EQUAL(result->candidates[index].name, candidate.name))
	    break;
    if (index == result->count) {
	result->candidates = static_cast<bu_mime_candidate *>(bu_realloc(result->candidates,
	    (result->count + 1) * sizeof(bu_mime_candidate), "MIME candidates"));
	result->candidates[index] = candidate;
	result->candidates[index].name = bu_strdup(candidate.name);
	++result->count;
    } else if (mime_rank_greater(candidate, result->candidates[index])) {
	result->candidates[index].evidence = evidence;
	result->candidates[index].priority = priority;
    }
    bu_vls_free(&canonical);
    std::sort(result->candidates, result->candidates + result->count, mime_candidate_before);
    return mime_result_status(result);
}

/* An animation control chunk must precede image data. Walk chunk boundaries
 * so an acTL spelling inside compressed data cannot identify an animation. */
static int
mime_apng(const unsigned char *data, size_t size, bool complete)
{
    mime_png::inspector header;
    header.feed(data, size);
    return header.facts.animated ? 1 : complete || header.finished() ? 0 : -1;
}

static bu_mime_status
mime_detect_rules(bu_mime_registry *registry, const char *filename, const void *data,
	       size_t size, int complete, bu_mime_result **result)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    if (!registry || (!data && size))
	return BU_MIME_ERROR;
    std::lock_guard<std::mutex> guard(registry->mutex);
    std::map<std::string, bu_mime_candidate> matches;
    std::set<bu_mime_token> extension_matches;
    std::string basename = mime_lower(filename);
    size_t separator = basename.find_last_of("/\\");
    if (separator != std::string::npos)
	basename.erase(0, separator + 1);
    for (size_t dot = basename.find('.'); dot != std::string::npos; dot = basename.find('.', dot + 1)) {
	auto it = registry->extensions.find(basename.substr(dot + 1));
	if (it != registry->extensions.end())
	    extension_matches.insert(it->second.begin(), it->second.end());
    }
    bool incomplete = false;
    size_t available = std::min(size, registry->scan_limit);
    bool entire = complete && available == size;
    for (const auto &entry : registry->definitions) {
	const mime_definition &definition = entry.second;
	bu_mime_candidate candidate = {definition.name.c_str(), BU_MIME_EVIDENCE_EXTENSION, 0, BU_MIME_TRAITS_INIT_ZERO};
	bool matched = extension_matches.count(entry.first) != 0;
	if (data) {
	    for (const auto &rule : definition.rules) {
		bool failed = false;
		bool pending = false;
		for (const auto &test : rule.tests) {
		    int status = mime_test_match(test, static_cast<const unsigned char *>(data), available, entire);
		    if (!status) {
			failed = true;
			break;
		    }
		    if (status < 0)
			pending = true;
		}
		if (!failed && pending)
		    incomplete = true;
		if (failed || pending)
		    continue;
		bu_mime_candidate rule_candidate = {definition.name.c_str(), rule.evidence, rule.priority, BU_MIME_TRAITS_INIT_ZERO};
		if (!matched || mime_rank_greater(rule_candidate, candidate))
		    candidate = rule_candidate;
		matched = true;
	    }
	}
	if (matched) {
	    auto it = matches.find(definition.name);
	    if (it == matches.end() || mime_rank_greater(candidate, it->second))
		matches[definition.name] = candidate;
	}
    }
    if (data && matches.count("image/png") &&
	matches["image/png"].evidence == BU_MIME_EVIDENCE_SIGNATURE) {
	int animated = mime_apng(static_cast<const unsigned char *>(data), available, entire);
	if (animated > 0) {
	    /* A structural subtype ranks ahead of its container signature. */
	    const unsigned int subtype_priority = BU_MIME_DEFAULT_PRIORITY + 1;
	    matches["image/apng"] = {"image/apng", BU_MIME_EVIDENCE_SIGNATURE, subtype_priority, BU_MIME_TRAITS_INIT_ZERO};
	} else if (animated < 0) {
	    incomplete = true;
	}
    }
    std::vector<bu_mime_candidate> ordered;
    for (const auto &match : matches)
	ordered.push_back(match.second);
    std::sort(ordered.begin(), ordered.end(), mime_candidate_before);
    bu_mime_result *output = static_cast<bu_mime_result *>(bu_calloc(1, sizeof(bu_mime_result), "MIME result"));
    output->incomplete = incomplete;
    output->count = ordered.size();
    if (!ordered.empty()) {
	output->candidates = static_cast<bu_mime_candidate *>(bu_calloc(ordered.size(), sizeof(bu_mime_candidate), "MIME candidates"));
	for (size_t i = 0; i < ordered.size(); ++i) {
	    output->candidates[i] = ordered[i];
	    output->candidates[i].name = bu_strdup(ordered[i].name);
	}
    }
    *result = output;
    return mime_result_status(output);
}

extern "C" bu_mime_status
bu_mime_identify(bu_mime_registry *registry, const char *filename, const bu_mime_probe_input *input,
    bu_mime_result **result)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    if (!registry || !input || (!input->data && input->size))
	return BU_MIME_ERROR;
    bu_mime_probe_input bounded = *input;
    bounded.size = std::min(input->size, registry->scan_limit);
    bounded.complete = input->complete && bounded.size == input->size;
    bu_mime_status status = mime_detect_rules(registry, filename, bounded.data, bounded.size, bounded.complete, result);
    if (status == BU_MIME_ERROR)
	return status;
    if (!mime_inspect_builtin(registry, filename, &bounded, *result)) {
	bu_mime_result_free(*result);
	*result = NULL;
	return BU_MIME_ERROR;
    }
    return mime_result_status(*result);
}

extern "C" bu_mime_status
bu_mime_detect(bu_mime_registry *registry, const char *filename, const void *data, size_t size, int complete,
    bu_mime_result **result)
{
    bu_mime_probe_input input = {static_cast<const unsigned char *>(data), size, complete,
	complete ? size : 0, complete};
    return bu_mime_identify(registry, filename, &input, result);
}

extern "C" bu_mime_status
bu_mime_detect_file(bu_mime_registry *registry, const char *filename, bu_mime_result **result)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    if (!registry || !filename)
	return BU_MIME_ERROR;
    FILE *fp = fopen(filename, "rb");
    if (!fp)
	return BU_MIME_ERROR;
    struct stat info;
    bool known = !fstat(fileno(fp), &info) && S_ISREG(info.st_mode) && info.st_size >= 0;
    std::vector<unsigned char> prefix(registry->scan_limit);
    size_t size = fread(prefix.data(), 1, prefix.size(), fp);
    int complete = feof(fp);
    int error = ferror(fp);
    if (fclose(fp))
	error = 1;
    if (error)
	return BU_MIME_ERROR;
    bu_mime_probe_input input = {prefix.data(), size, complete,
	known ? static_cast<uint64_t>(info.st_size) : 0, known};
    return bu_mime_identify(registry, filename, &input, result);
}

extern "C" void
bu_mime_result_free(bu_mime_result *result)
{
    if (!result)
	return;
    for (size_t i = 0; i < result->count; ++i) {
	bu_mime_traits_free(&result->candidates[i].traits);
	bu_free(const_cast<char *>(result->candidates[i].name), "MIME name");
    }
    bu_free(result->candidates, "MIME candidates");
    bu_free(result, "MIME result");
}

extern "C" void
bu_mime_result_describe(const bu_mime_result *result, bu_vls *messages)
{
    if (!messages)
	return;
    if (!result || !result->count)
	bu_vls_strcat(messages, result ? "No recognized file type\n" : "Identification unavailable\n");
    if (!result)
	return;
    for (size_t i = 0; i < result->count; ++i) {
	const auto &candidate = result->candidates[i];
	const char *evidence = candidate.evidence == BU_MIME_EVIDENCE_SIGNATURE ? "signature" :
	    (candidate.evidence == BU_MIME_EVIDENCE_HEURISTIC ? "heuristic" : "extension");
	bu_vls_printf(messages, "%s: %s (priority %u)", candidate.name, evidence, candidate.priority);
	bu_mime_traits_describe(&candidate.traits, messages);
	bu_vls_putc(messages, '\n');
    }
    if (result->incomplete)
	bu_vls_strcat(messages, "Inspection incomplete; additional bytes could change identification\n");
}

static void
mime_seed_signature(bu_mime_registry *registry, const char *name,
		    const unsigned char *value, size_t length, unsigned int priority = BU_MIME_DEFAULT_PRIORITY)
{
    bu_mime_test test = {0, 0, value, NULL, length};
    bu_mime_rule rule = {&test, 1, BU_MIME_EVIDENCE_SIGNATURE, priority};
    bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(bu_mime_definition),
	name, NULL, NULL, 0, NULL, 0, &rule, 1};
    /* A caller may intentionally choose a limit smaller than a signature. */
    if (length <= registry->scan_limit)
	bu_mime_register(registry, &definition);
}

extern "C" bu_mime_registry *
bu_mime_registry_create(size_t scan_limit)
{
    bu_mime_registry *registry = new bu_mime_registry;
    registry->scan_limit = scan_limit ? scan_limit : BU_MIME_DEFAULT_SCAN_LIMIT;
    for (const auto &builtin : mime_builtins) {
	mime_definition definition;
	definition.name = builtin.name;
	definition.description = builtin.description;
	std::string extensions = builtin.extensions;
	size_t start = 0;
	while (start < extensions.size()) {
	    size_t end = extensions.find(',', start);
	    definition.extensions.push_back(extensions.substr(start, end - start));
	    if (end == std::string::npos)
		break;
	    start = end + 1;
	}
	mime_insert(registry, definition);
    }
    for (const auto &alias : mime_aliases) {
	mime_definition definition;
	definition.name = alias.canonical;
	definition.aliases.emplace_back(alias.alias);
	mime_insert(registry, definition);
    }
    static const unsigned char glb_signature[] = {'g', 'l', 'T', 'F'};
    mime_seed_signature(registry, "model/gltf-binary", glb_signature, sizeof(glb_signature));
    static const unsigned char riff[] = {'R', 'I', 'F', 'F'};
    static const unsigned char avi[] = {'A', 'V', 'I', ' '};
    const bu_mime_test avi_tests[] = {{0, 0, riff, NULL, sizeof(riff)}, {8, 8, avi, NULL, sizeof(avi)}};
    const bu_mime_rule avi_rule = {avi_tests, 2, BU_MIME_EVIDENCE_SIGNATURE, BU_MIME_DEFAULT_PRIORITY};
    const bu_mime_definition avi_definition = {BU_MIME_ABI_VERSION, sizeof(bu_mime_definition),
	"video/x-msvideo", NULL, NULL, 0, NULL, 0, &avi_rule, 1};
    if (registry->scan_limit >= 12)
	bu_mime_register(registry, &avi_definition);
    static const unsigned char png[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    static const unsigned char jpeg[] = {0xff, 0xd8, 0xff};
    static const unsigned char gif87[] = {'G', 'I', 'F', '8', '7', 'a'};
    static const unsigned char gif89[] = {'G', 'I', 'F', '8', '9', 'a'};
    static const unsigned char ply[] = {'p', 'l', 'y', '\n'};
    static const unsigned char ply_crlf[] = {'p', 'l', 'y', '\r', '\n'};
    static const unsigned char tiff_le[] = {'I', 'I', 42, 0};
    static const unsigned char tiff_be[] = {'M', 'M', 0, 42};
    static const unsigned char bigtiff_le[] = {'I', 'I', 43, 0, 8, 0, 0, 0};
    static const unsigned char bigtiff_be[] = {'M', 'M', 0, 43, 0, 8, 0, 0};
    static const unsigned char rle[] = {0x52, 0xcc};
    static const unsigned char rle_swapped[] = {0xcc, 0x52};
    static const unsigned char step[] = "ISO-10303-21;";
    mime_seed_signature(registry, "image/png", png, sizeof(png));
    mime_seed_signature(registry, "image/jpeg", jpeg, sizeof(jpeg));
    mime_seed_signature(registry, "image/gif", gif87, sizeof(gif87));
    mime_seed_signature(registry, "image/gif", gif89, sizeof(gif89));
    mime_seed_signature(registry, "application/x-ply", ply, sizeof(ply));
    mime_seed_signature(registry, "application/x-ply", ply_crlf, sizeof(ply_crlf));
    mime_seed_signature(registry, "image/tiff", tiff_le, sizeof(tiff_le));
    mime_seed_signature(registry, "image/tiff", tiff_be, sizeof(tiff_be));
    mime_seed_signature(registry, "image/tiff", bigtiff_le, sizeof(bigtiff_le));
    mime_seed_signature(registry, "image/tiff", bigtiff_be, sizeof(bigtiff_be));
    mime_seed_signature(registry, "image/rle", rle, sizeof(rle));
    mime_seed_signature(registry, "image/rle", rle_swapped, sizeof(rle_swapped));
    /* Prefer a recognized schema over the generic serialization. */
    const unsigned int container_priority = BU_MIME_DEFAULT_PRIORITY - 1;
    mime_seed_signature(registry, "application/p21", step, sizeof(step) - 1, container_priority);
    static const unsigned char pnm_kinds[] = {'2', '3', '5', '6'};
    static const unsigned char separators[] = {' ', '\t', '\r', '\n', '\f', '\v', '#'};
    for (unsigned char kind : pnm_kinds) {
	for (unsigned char separator : separators) {
	    unsigned char signature[] = {'P', kind, separator};
	    mime_seed_signature(registry, kind == '2' || kind == '5' ? "image/x-portable-graymap" : "image/x-portable-pixmap",
		signature, sizeof(signature));
	}
    }
    registry->builtin_end = registry->next_token;
    return registry;
}

extern "C" size_t
bu_mime_scan_limit(const bu_mime_registry *registry)
{
    return registry ? registry->scan_limit : 0;
}

extern "C" void
bu_mime_registry_destroy(bu_mime_registry *registry)
{
    delete registry;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
