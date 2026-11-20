/* G C V _ P R I V A T E . H
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
#ifndef GCV_MIME_CATALOG_H
#define GCV_MIME_CATALOG_H

#include "common.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "gcv/api.h"

/* SDK format lists are discovered once when a module is loaded.  Keep all
 * descriptor storage stable until the loader copies it into its context. */
class gcv_mime_catalog {
public:
    using set_constraints = std::vector<std::pair<std::string, std::vector<std::string>>>;
private:
    struct format {
	std::string name;
	std::string description;
	std::vector<std::string> extensions;
	std::vector<const char *> extension_views;
	std::vector<std::pair<std::string, std::string>> traits;
	set_constraints sets;
	std::vector<std::vector<const char *>> member_views;
	std::vector<bu_mime_trait> trait_views;
	bool read = false;
	bool write = false;
	unsigned int read_flags = 0;
    };
    std::unique_ptr<bu_mime_registry, decltype(&bu_mime_registry_destroy)> aliases{
	bu_mime_registry_create(0), bu_mime_registry_destroy};
    std::vector<std::unique_ptr<format>> formats;
    std::vector<bu_mime_definition> definitions;
    std::vector<bu_mime_capability> read_types;
    std::vector<bu_mime_capability> write_types;
    std::vector<gcv_handler> handlers;
    std::unique_ptr<gcv_conversion> reader_conversion;
    std::unique_ptr<gcv_conversion> writer_conversion;
    gcv_plugin_v2 descriptor = {};
public:
    static std::vector<std::string> split(const char *extensions)
    {
	std::istringstream input(extensions ? extensions : "");
	std::vector<std::string> result;
	std::string extension;
	while (input >> extension)
	    result.push_back(extension);
	return result;
    }

    void add(const std::string &name, const char *description,
	     const std::vector<std::string> &extensions, bool read, bool write,
	     const std::vector<std::pair<std::string, std::string>> &traits = {}, unsigned int read_flags = 0,
	     const set_constraints &sets = {})
    {
	bu_vls canonical = BU_VLS_INIT_ZERO;
	std::string resolved = bu_mime_canonical(aliases.get(), name.c_str(), &canonical) ?
	    bu_vls_cstr(&canonical) : name;
	bu_vls_free(&canonical);
	format *entry = NULL;
	for (auto &candidate : formats)
	    if (candidate->name == resolved && candidate->traits == traits && candidate->sets == sets && candidate->read_flags == read_flags)
		entry = candidate.get();
	if (!entry) {
	    formats.emplace_back(new format);
	    entry = formats.back().get();
	    entry->name = resolved;
	    entry->description = description ? description : name;
	    entry->traits = traits;
	    entry->sets = sets;
	    entry->read_flags = read_flags;
	}
	for (const auto &extension : extensions)
	    if (std::find(entry->extensions.begin(), entry->extensions.end(), extension) == entry->extensions.end())
		entry->extensions.push_back(extension);
	entry->read = entry->read || read;
	entry->write = entry->write || write;
    }

    void finish(const gcv_filter *reader, const gcv_filter *writer,
	decltype(gcv_handler::probe) reader_probe = NULL,
	decltype(gcv_handler::inspect) reader_inspect = NULL)
    {
	for (auto &entry : formats) {
	    for (const auto &extension : entry->extensions)
		entry->extension_views.push_back(extension.c_str());
	    definitions.push_back({BU_MIME_ABI_VERSION, sizeof(bu_mime_definition),
		entry->name.c_str(), entry->description.c_str(), NULL, 0,
		entry->extension_views.data(), entry->extension_views.size(), NULL, 0});
	    for (const auto &trait : entry->traits)
		entry->trait_views.push_back(BU_MIME_TRAIT_STRING(trait.first.c_str(), trait.second.c_str()));
	    entry->member_views.resize(entry->sets.size());
	    for (size_t i = 0; i < entry->sets.size(); ++i) {
		const auto &set = entry->sets[i];
		auto &members = entry->member_views[i];
		for (const auto &member : set.second) members.push_back(member.c_str());
		entry->trait_views.push_back(BU_MIME_TRAIT_SET(set.first.c_str(), members.data(), members.size()));
	    }
	    if (entry->read)
		read_types.push_back({entry->name.c_str(), entry->trait_views.data(), entry->trait_views.size(), 0, entry->read_flags});
	    if (entry->write)
		write_types.push_back(BU_MIME_CAPABILITY(entry->name.c_str(), entry->trait_views.data(), entry->trait_views.size(), 0));
	}
	if (reader && !read_types.empty()) {
	    reader_conversion.reset(new gcv_conversion{reader->name, reader->filter_type,
		reader->create_opts_fn, reader->free_opts_fn, reader->filter_fn});
	    handlers.push_back({reader_conversion.get(), read_types.data(), read_types.size(), GCV_FALLBACK_PRIORITY, reader_probe, reader_inspect});
	}
	if (writer && !write_types.empty()) {
	    writer_conversion.reset(new gcv_conversion{writer->name, writer->filter_type,
		writer->create_opts_fn, writer->free_opts_fn, writer->filter_fn});
	    handlers.push_back({writer_conversion.get(), write_types.data(), write_types.size(), GCV_FALLBACK_PRIORITY, NULL, NULL});
	}
	aliases.reset();
	descriptor = {GCV_PLUGIN_ABI_VERSION, sizeof(gcv_plugin_v2), definitions.data(),
	    definitions.size(), handlers.data(), handlers.size()};
    }

    const gcv_plugin_v2 *plugin() const { return &descriptor; }
};

#endif
