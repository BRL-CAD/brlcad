/*          M I M E _ T R A I T S . C P P
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

#include <set>
#include <string>
#include <vector>
#include "bu/malloc.h"
#include "bu/mime_runtime.h"
#include "bu/str.h"
#include "json.hpp"

static bool
mime_trait_key(const char *key)
{
    if (!key || !key[0])
	return false;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(key); *p; ++p)
	if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
	    *p == '.' || *p == '-' || *p == '_'))
	    return false;
    return true;
}

static bool
mime_trait_valid(const bu_mime_trait &item)
{
    if (!mime_trait_key(item.key))
	return false;
    if (item.kind == BU_MIME_VALUE_STRING)
	return item.value && item.value[0] && !item.members && !item.member_count;
    if (item.kind != BU_MIME_VALUE_SET || item.value || (item.member_count && !item.members))
	return false;
    for (size_t i = 0; i < item.member_count; ++i)
	if (!item.members[i] || !item.members[i][0])
	    return false;
    return true;
}

static bool
mime_set_subset(const bu_mime_trait &subset, const bu_mime_trait &superset)
{
    for (size_t i = 0; i < subset.member_count; ++i) {
	bool found = false;
	for (size_t j = 0; j < superset.member_count; ++j)
	    found = found || BU_STR_EQUAL(subset.members[i], superset.members[j]);
	if (!found)
	    return false;
    }
    return true;
}

static bool
mime_trait_equal(const bu_mime_trait &a, const bu_mime_trait &b)
{
    if (a.kind != b.kind)
	return false;
    if (a.kind == BU_MIME_VALUE_STRING)
	return BU_STR_EQUAL(a.value, b.value);
    return mime_set_subset(a, b) && mime_set_subset(b, a);
}

/* Absence and contradictory observations both prevent automatic selection.
 * Diagnostic snapshots retain the original observations in either case. */
static const bu_mime_trait *
mime_trait_find(const bu_mime_traits *traits, const char *key)
{
    if (!traits || !key)
	return NULL;
    const bu_mime_trait *found = NULL;
    for (size_t i = 0; i < traits->count; ++i) {
	const bu_mime_trait &item = traits->items[i];
	if (!BU_STR_EQUAL(item.key, key))
	    continue;
	if (found && !mime_trait_equal(*found, item))
	    return NULL;
	found = &item;
    }
    return found;
}

extern "C" int
bu_mime_traits_valid(const bu_mime_traits *traits, int unambiguous)
{
    if (!traits)
	return 1;
    if (traits->count && !traits->items)
	return 0;
    for (size_t i = 0; i < traits->count; ++i)
	if (!mime_trait_valid(traits->items[i]))
	    return 0;
    if (unambiguous)
	for (size_t i = 0; i < traits->count; ++i)
	    if (!mime_trait_find(traits, traits->items[i].key))
		return 0;
    return 1;
}

extern "C" const char *
bu_mime_traits_get(const bu_mime_traits *traits, const char *key)
{
    if (!bu_mime_traits_valid(traits, 0))
	return NULL;
    const bu_mime_trait *item = mime_trait_find(traits, key);
    return item && item->kind == BU_MIME_VALUE_STRING ? item->value : NULL;
}

extern "C" int
bu_mime_traits_match(const bu_mime_traits *observed, const bu_mime_traits *supplemental,
    const bu_mime_traits *requested)
{
    if (!bu_mime_traits_valid(observed, 0) || !bu_mime_traits_valid(supplemental, 0) ||
	!bu_mime_traits_valid(requested, 1))
	return 0;
    for (size_t i = 0; requested && i < requested->count; ++i) {
	const bu_mime_trait &assertion = requested->items[i];
	bool present = false;
	for (size_t j = 0; observed && j < observed->count; ++j)
	    if (BU_STR_EQUAL(observed->items[j].key, assertion.key)) {
		present = true;
		break;
	    }
	const bu_mime_trait *fact = mime_trait_find(present ? observed : supplemental, assertion.key);
	if (!fact || !mime_trait_equal(*fact, assertion))
	    return 0;
    }
    return 1;
}

extern "C" int
bu_mime_traits_get_set(const bu_mime_traits *traits, const char *key,
    const char *const **members, size_t *count)
{
    if (members) *members = NULL;
    if (count) *count = 0;
    if (!bu_mime_traits_valid(traits, 0))
	return 0;
    const bu_mime_trait *item = mime_trait_find(traits, key);
    if (!item || item->kind != BU_MIME_VALUE_SET)
	return 0;
    if (members) *members = item->members;
    if (count) *count = item->member_count;
    return 1;
}

static bool
mime_traits_append(bu_mime_traits *traits, const bu_mime_trait &source)
{
    if (!traits || !mime_trait_valid(source) || !bu_mime_traits_valid(traits, 0) ||
	traits->count >= SIZE_MAX / sizeof(bu_mime_trait))
	return false;
    for (size_t i = 0; i < traits->count; ++i)
	if (BU_STR_EQUAL(traits->items[i].key, source.key) && mime_trait_equal(traits->items[i], source))
	    return true;
    /* Copy before reallocating: source may refer to this same collection. */
    bu_mime_trait copy = BU_MIME_TRAIT_STRING(bu_strdup(source.key), NULL);
    copy.kind = source.kind;
    if (source.kind == BU_MIME_VALUE_STRING) {
	copy.value = bu_strdup(source.value);
    } else {
	std::set<std::string> values;
	for (size_t i = 0; i < source.member_count; ++i)
	    values.insert(source.members[i]);
	if (!values.empty()) {
	    auto **members = static_cast<const char **>(bu_calloc(values.size(), sizeof(char *), "MIME set members"));
	    size_t i = 0;
	    for (const auto &value : values)
		members[i++] = bu_strdup(value.c_str());
	    copy.members = members;
	    copy.member_count = values.size();
	}
    }
    auto *items = static_cast<bu_mime_trait *>(bu_realloc(const_cast<bu_mime_trait *>(traits->items),
	(traits->count + 1) * sizeof(bu_mime_trait), "MIME traits"));
    items[traits->count++] = copy;
    traits->items = items;
    return true;
}

extern "C" int
bu_mime_traits_add(bu_mime_traits *traits, const char *key, const char *value)
{
    const bu_mime_trait item = BU_MIME_TRAIT_STRING(key, value);
    return mime_traits_append(traits, item);
}

extern "C" int
bu_mime_traits_add_set(bu_mime_traits *traits, const char *key, const char *const *members, size_t count)
{
    const bu_mime_trait item = BU_MIME_TRAIT_SET(key, members, count);
    return mime_traits_append(traits, item);
}

extern "C" void
bu_mime_traits_free(bu_mime_traits *traits)
{
    if (!traits)
	return;
    for (size_t i = 0; i < traits->count; ++i) {
	const bu_mime_trait &item = traits->items[i];
	bu_free(const_cast<char *>(item.key), "MIME trait key");
	bu_free(const_cast<char *>(item.value), "MIME trait value");
	for (size_t j = 0; j < item.member_count; ++j)
	    bu_free(const_cast<char *>(item.members[j]), "MIME set member");
	bu_free(const_cast<const char **>(item.members), "MIME set members");
    }
    bu_free(const_cast<bu_mime_trait *>(traits->items), "MIME traits");
    *traits = {NULL, 0};
}

extern "C" int
bu_mime_traits_copy(bu_mime_traits *destination, const bu_mime_traits *source)
{
    if (!destination || !bu_mime_traits_valid(source, 0))
	return 0;
    if (destination == source)
	return 1;
    bu_mime_traits copy = BU_MIME_TRAITS_INIT_ZERO;
    if (source)
	for (size_t i = 0; i < source->count; ++i)
	    if (!mime_traits_append(&copy, source->items[i])) {
		bu_mime_traits_free(&copy);
		return 0;
	    }
    bu_mime_traits_free(destination);
    *destination = copy;
    return 1;
}

static bool
mime_traits_extend(bu_mime_traits *destination, const bu_mime_traits *source)
{
    if (!destination || !bu_mime_traits_valid(source, 0))
	return false;
    bu_mime_traits copy = BU_MIME_TRAITS_INIT_ZERO;
    if (!bu_mime_traits_copy(&copy, destination))
	return false;
    if (source)
	for (size_t i = 0; i < source->count; ++i)
	    if (!mime_traits_append(&copy, source->items[i])) {
		bu_mime_traits_free(&copy);
		return false;
	    }
    bu_mime_traits_free(destination);
    *destination = copy;
    return true;
}

extern "C" int
bu_mime_traits_resolve(bu_mime_traits *destination, const bu_mime_traits *observed, const bu_mime_traits *requested)
{
    if (!destination || !bu_mime_traits_valid(observed, 0) || !bu_mime_traits_valid(requested, 1))
	return 0;
    if (requested && observed)
	for (size_t i = 0; i < requested->count; ++i)
	    for (size_t j = 0; j < observed->count; ++j)
		if (BU_STR_EQUAL(requested->items[i].key, observed->items[j].key) &&
		    !mime_trait_equal(requested->items[i], observed->items[j]))
		    return 0;
    bu_mime_traits combined = BU_MIME_TRAITS_INIT_ZERO;
    if (!bu_mime_traits_copy(&combined, observed) || !mime_traits_extend(&combined, requested)) {
	bu_mime_traits_free(&combined);
	return 0;
    }
    bu_mime_traits_free(destination);
    *destination = combined;
    return 1;
}

extern "C" void
bu_mime_capability_free(bu_mime_capability *capability)
{
    if (!capability)
	return;
    bu_mime_traits traits = {capability->traits, capability->trait_count};
    bu_mime_traits_free(&traits);
    bu_free(const_cast<char *>(capability->mime_type), "capability MIME type");
    *capability = {NULL, NULL, 0, 0, 0};
}

extern "C" int
bu_mime_capability_copy(bu_mime_capability *destination, const bu_mime_capability *source)
{
    if (!destination || !source || !source->mime_type || !source->mime_type[0] ||
	(source->flags & ~BU_MIME_DEFER_INPUT_VALIDATION))
	return 0;
    bu_mime_traits view = {source->traits, source->trait_count};
    if (!bu_mime_traits_valid(&view, 1))
	return 0;
    if (destination == source)
	return 1;
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    if (!bu_mime_traits_copy(&traits, &view))
	return 0;
    bu_mime_capability copy = {bu_strdup(source->mime_type), traits.items, traits.count, source->operations, source->flags};
    bu_mime_capability_free(destination);
    *destination = copy;
    return 1;
}

extern "C" int
bu_mime_capability_match(const bu_mime_capability *capability, const bu_mime_traits *traits,
    bu_mime_direction direction)
{
    if (!capability || !capability->mime_type || !capability->mime_type[0] ||
	(capability->flags & ~BU_MIME_DEFER_INPUT_VALIDATION) ||
	(direction != BU_MIME_INPUT && direction != BU_MIME_OUTPUT))
	return 0;
    bu_mime_traits supported = {capability->traits, capability->trait_count};
    if (!bu_mime_traits_valid(&supported, 1) || !bu_mime_traits_valid(traits, direction == BU_MIME_OUTPUT))
	return 0;
    const bu_mime_traits *constraints = direction == BU_MIME_INPUT ? &supported : traits;
    const bu_mime_traits *facts = direction == BU_MIME_INPUT ? traits : &supported;
    if (!constraints)
	return 1;
    for (size_t i = 0; i < constraints->count; ++i) {
	const bu_mime_trait &item = constraints->items[i];
	const bu_mime_trait *other = mime_trait_find(facts, item.key);
	if (!other && direction == BU_MIME_INPUT && (capability->flags & BU_MIME_DEFER_INPUT_VALIDATION)) {
	    bool absent = true;
	    if (facts)
		for (size_t j = 0; j < facts->count; ++j)
		    absent = absent && !BU_STR_EQUAL(facts->items[j].key, item.key);
	    if (absent) continue;
	}
	if (!other || other->kind != item.kind)
	    return 0;
	if (item.kind == BU_MIME_VALUE_STRING) {
	    if (!mime_trait_equal(item, *other)) return 0;
	} else if (direction == BU_MIME_INPUT ? !mime_set_subset(*other, item) : !mime_set_subset(item, *other)) {
	    return 0;
	}
    }
    return 1;
}

extern "C" int
bu_mime_capability_resolve(const bu_mime_capability *capability, const bu_mime_traits *traits,
    bu_mime_direction direction, bu_mime_traits *resolved)
{
    if (!resolved || !bu_mime_capability_match(capability, traits, direction))
	return 0;
    bu_mime_traits copy = BU_MIME_TRAITS_INIT_ZERO;
    if (!bu_mime_traits_copy(&copy, traits))
	return 0;
    if (direction == BU_MIME_OUTPUT && traits && traits->count)
	for (size_t i = 0; i < capability->trait_count; ++i)
	    if (capability->traits[i].kind == BU_MIME_VALUE_STRING && !mime_traits_append(&copy, capability->traits[i])) {
		bu_mime_traits_free(&copy);
		return 0;
	    }
    bu_mime_traits_free(resolved);
    *resolved = copy;
    return 1;
}

extern "C" void
bu_mime_traits_describe(const bu_mime_traits *traits, bu_vls *messages)
{
    if (!messages || !traits || !bu_mime_traits_valid(traits, 0))
	return;
    for (size_t i = 0; i < traits->count; ++i) {
	const bu_mime_trait &item = traits->items[i];
	if (item.kind == BU_MIME_VALUE_STRING) {
	    bu_vls_printf(messages, " %s=%s", item.key, item.value);
	} else {
	    nlohmann::json members = nlohmann::json::array();
	    for (size_t j = 0; j < item.member_count; ++j)
		members.push_back(item.members[j]);
	    bu_vls_printf(messages, " %s[]=%s", item.key, members.dump().c_str());
	}
    }
}

extern "C" int
bu_mime_result_add_traits(bu_mime_result *result, const char *canonical, const bu_mime_traits *traits)
{
    if (!result || !canonical || !bu_mime_traits_valid(traits, 0))
	return 0;
    for (size_t i = 0; i < result->count; ++i)
	if (BU_STR_EQUAL(result->candidates[i].name, canonical))
	    return mime_traits_extend(&result->candidates[i].traits, traits);
    return 0;
}

static bool
mime_version_parts(const char *value, std::vector<std::string> &parts)
{
    if (!value || !value[0]) return false;
    std::string text(value);
    size_t start = 0;
    for (;;) {
	size_t end = text.find('.', start);
	std::string part = text.substr(start, end == std::string::npos ? end : end - start);
	if (part.empty() || part.find_first_not_of("0123456789") != std::string::npos)
	    return false;
	size_t significant = part.find_first_not_of('0');
	parts.push_back(significant == std::string::npos ? "0" : part.substr(significant));
	if (end == std::string::npos) return true;
	start = end + 1;
    }
}

extern "C" int
bu_mime_version_compare(const char *left, const char *right, int *comparison)
{
    if (!comparison) return 0;
    *comparison = 0;
    std::vector<std::string> a, b;
    if (!mime_version_parts(left, a) || !mime_version_parts(right, b)) return 0;
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
	std::string first = i < a.size() ? a[i] : "0";
	std::string second = i < b.size() ? b[i] : "0";
	if (first == second) continue;
	*comparison = first.size() == second.size() ? (first < second ? -1 : 1) :
	    (first.size() < second.size() ? -1 : 1);
	return 1;
    }
    return 1;
}

extern "C" int
bu_opt_mime_trait(bu_vls *messages, size_t argc, const char **argv, void *set_var)
{
    auto *traits = static_cast<bu_mime_traits *>(set_var);
    if (!traits || !argc || !argv || !argv[0])
	return -1;
    std::string assignment(argv[0]);
    size_t equal = assignment.find('=');
    if (equal != std::string::npos && bu_mime_traits_valid(traits, 1)) {
	std::string key = assignment.substr(0, equal);
	std::string value = assignment.substr(equal + 1);
	bu_mime_traits requested = BU_MIME_TRAITS_INIT_ZERO;
	bool added = false;
	if (key.size() >= 2 && key.compare(key.size() - 2, 2, "[]") == 0) {
	    key.resize(key.size() - 2);
	    nlohmann::json array = nlohmann::json::parse(value, NULL, false);
	    if (array.is_array()) {
		std::vector<const char *> members;
		bool valid = true;
		for (const auto &member : array) {
		    if (!member.is_string() || member.get_ref<const std::string &>().empty()) {
			valid = false;
			break;
		    }
		    members.push_back(member.get_ref<const std::string &>().c_str());
		}
		if (valid) added = bu_mime_traits_add_set(&requested, key.c_str(), members.data(), members.size());
	    }
	} else {
	    added = bu_mime_traits_add(&requested, key.c_str(), value.c_str());
	}
	bool resolved = added && bu_mime_traits_resolve(traits, traits, &requested);
	bu_mime_traits_free(&requested);
	if (resolved) return 1;
    }
    if (messages)
	bu_vls_printf(messages, "Expected key=value or key[]=[\"member\",...] without conflicting requests: %s\n", argv[0]);
    return -1;
}
