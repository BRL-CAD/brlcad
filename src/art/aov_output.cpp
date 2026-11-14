/*                          A O V _ O U T P U T . C P P
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
#include <filesystem>
#include <iterator>
#include <utility>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdocumentation"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif
#include "renderer/api/aov.h"
#include "renderer/api/frame.h"
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "aov_output.h"
#include "bu/log.h"

namespace {
constexpr const char *supported_aovs[] = {
    "depth", "normal", "position", "uv", "albedo", "diffuse", "direct_diffuse",
    "indirect_diffuse", "glossy", "direct_glossy", "indirect_glossy", "emission",
    "npr_shading", "npr_contour"
};
}

bool
ArtAOVOutput::assign(const std::string& value, std::string& reason)
{
    std::vector<std::string> names;
    if (value != "none") {
	size_t begin = 0;
	do {
	    const size_t end = value.find(',', begin);
	    const std::string name = value.substr(begin, end == std::string::npos ? end : end - begin);
	    if (std::find(std::begin(supported_aovs), std::end(supported_aovs), name) == std::end(supported_aovs)) {
		reason = "unsupported AOV '" + name + "'";
		return false;
	    }
	    if (std::find(names.begin(), names.end(), name) != names.end()) {
		reason = "duplicate AOV '" + name + "'";
		return false;
	    }
	    names.push_back(name);
	    if (end == std::string::npos)
		break;
	    begin = end + 1;
	} while (begin <= value.size());
    }
    m_names = std::move(names);
    return true;
}

std::string
ArtAOVOutput::value() const
{
    if (m_names.empty())
	return "none";
    std::string result;
    for (const auto& name : m_names) {
	if (!result.empty())
	    result += ',';
	result += name;
    }
    return result;
}

bool
ArtAOVOutput::populate(renderer::AOVContainer& aovs) const
{
    if (m_names.empty())
	return true;
    const renderer::AOVFactoryRegistrar registrar;
    for (const auto& name : m_names) {
	const auto *factory = registrar.lookup((name + "_aov").c_str());
	if (!factory) {
	    bu_log("art: renderer does not provide AOV '%s'\n", name.c_str());
	    return false;
	}
	renderer::ParamArray parameters;
	if (name == "depth")
	    parameters.insert("camera_space", true);
	aovs.insert(factory->create(parameters));
    }
    return true;
}

bool
ArtAOVOutput::write(const renderer::Frame& frame, const char *beauty_path) const
{
    if (m_names.empty())
	return true;
    std::filesystem::path path(beauty_path);
    path.replace_extension(".exr");
    if (!frame.write_aov_images(path.string().c_str())) {
	bu_log("art: could not save requested AOV images\n");
	return false;
    }
    return true;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
