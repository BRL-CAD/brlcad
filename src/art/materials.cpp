/*              M A T E R I A L S . C P P
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

#include <cmath>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "renderer/api/scene.h"
#include "renderer/api/material.h"
#include "renderer/api/shadergroup.h"
#include "renderer/api/surfaceshader.h"
#include "renderer/api/utility.h"
#include "foundation/image/colorspace.h"
#include "foundation/memory/autoreleaseptr.h"

#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "materials.h"

namespace asf = foundation;
namespace asr = renderer;

namespace {

constexpr char TerminalLayer[] = "material";
constexpr char SurfaceLayer[] = "surface";
constexpr char PropertyPrefix[] = "OSL.";
constexpr char ParameterPrefix[] = "OSL.param.";
constexpr char NodePrefix[] = "OSL.node.";
constexpr char ConnectionPrefix[] = "OSL.connect.";
constexpr char SingleNodeVersion[] = "1";
constexpr char NetworkVersion[] = "2";

struct MaterialNode {
    std::string name;
    std::map<std::string, std::string> parameters;
};

struct Endpoint {
    std::string node;
    std::string parameter;
};

struct Connection {
    Endpoint source;
    Endpoint destination;
};

struct MaterialShader {
    MaterialNode root{"as_plastic", {}};
    std::string output = "out_outColor";
    bool output_explicit = false;
    std::map<std::string, MaterialNode> nodes;
    std::vector<Connection> connections;
};

bool
valid_identifier(const std::string &name)
{
    auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (name.empty() || !letter(name.front()))
	return false;
    for (char c : name)
	if (!letter(c) && !(c >= '0' && c <= '9'))
	    return false;
    return true;
}

bool
parse_endpoint(const std::string &text, Endpoint &endpoint)
{
    const auto dot = text.find('.');
    if (dot == std::string::npos)
	return false;
    endpoint.node = text.substr(0, dot);
    endpoint.parameter = text.substr(dot + 1);
    return valid_identifier(endpoint.node) && valid_identifier(endpoint.parameter);
}

bool
parse_shader(const std::map<std::string, std::string> &properties,
    const char *material_name, MaterialShader &shader)
{
    const auto version = properties.find("OSL.version");
    const bool network = version != properties.end() && version->second == NetworkVersion;
    if (version != properties.end() && version->second != SingleNodeVersion && !network) {
	bu_log("art: material '%s' has unsupported OSL.version '%s'\n", material_name, version->second.c_str());
	return false;
    }
    auto fail_property = [&](const std::string &key) {
	bu_log("art: material '%s' has invalid or unsupported OSL property '%s'\n", material_name, key.c_str());
	return false;
    };

    // Declarations are independent of the attribute order in the database.
    if (network) {
	for (const auto &entry : properties) {
	    if (entry.first.compare(0, sizeof(NodePrefix) - 1, NodePrefix) != 0)
		continue;
	    const std::string node = entry.first.substr(sizeof(NodePrefix) - 1);
	    if (!valid_identifier(node) || node == TerminalLayer || node == SurfaceLayer || entry.second.empty())
		return fail_property(entry.first);
	    shader.nodes.emplace(node, MaterialNode{entry.second, {}});
	}
    }
    for (const auto &entry : properties) {
	const std::string &key = entry.first;
	const std::string &value = entry.second;
	if (key == "OSL") {
	    shader.root.name = value;
	} else if (key == "OSL.output") {
	    shader.output = value;
	    shader.output_explicit = true;
	} else if (key.compare(0, sizeof(ParameterPrefix) - 1, ParameterPrefix) == 0) {
	    const std::string input = key.substr(sizeof(ParameterPrefix) - 1);
	    if (network && input.find('.') != std::string::npos) {
		Endpoint endpoint;
		if (!parse_endpoint(input, endpoint) || shader.nodes.find(endpoint.node) == shader.nodes.end())
		    return fail_property(key);
		shader.nodes.at(endpoint.node).parameters.emplace(endpoint.parameter, value);
	    } else {
		shader.root.parameters.emplace(input, value);
	    }
	} else if (network && key.compare(0, sizeof(ConnectionPrefix) - 1, ConnectionPrefix) == 0) {
	    Connection connection;
	    if (!parse_endpoint(key.substr(sizeof(ConnectionPrefix) - 1), connection.destination) ||
		!parse_endpoint(value, connection.source))
		return fail_property(key);
	    shader.connections.push_back(connection);
	} else if (key == "OSL.version" ||
	    (network && key.compare(0, sizeof(NodePrefix) - 1, NodePrefix) == 0)) {
	    continue;
	} else if (key.compare(0, sizeof(PropertyPrefix) - 1, PropertyPrefix) == 0) {
	    return fail_property(key);
	}
    }
    if (shader.root.name.empty() || shader.output.empty()) {
	bu_log("art: material '%s' has an empty OSL shader or output\n", material_name);
	return false;
    }
    return true;
}

bool
read_shader(struct db_i *db, const struct rt_comb_internal *comb, MaterialShader &shader)
{
    std::map<std::string, std::string> properties;
    const char *material_name = bu_vls_cstr(&comb->material);
    if (!material_name[0])
	return true;

    struct directory *dp = db_lookup(db, material_name, LOOKUP_QUIET);
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (!dp || rt_db_get_internal(&intern, dp, db, NULL) < 0) {
	bu_log("art: cannot read material '%s'\n", material_name);
	return false;
    }
    if (intern.idb_type != ID_MATERIAL) {
	bu_log("art: '%s' is not a material object\n", material_name);
	rt_db_free_internal(&intern);
	return false;
    }

    const auto *material = static_cast<const struct rt_material_internal *>(intern.idb_ptr);
    const struct bu_attribute_value_pair *property;
    for (BU_AVS_FOR(property, &material->opticalProperties))
	properties.emplace(property->name, property->value ? property->value : "");
    rt_db_free_internal(&intern);

    return parse_shader(properties, material_name, shader);
}

const asf::Dictionary *
find_parameter(const asr::ShaderQuery &query, const char *name)
{
    for (size_t i = 0; i < query.get_param_count(); ++i) {
	const asf::Dictionary &param = query.get_param_info(i);
	if (BU_STR_EQUAL(param.get("name"), name))
	    return &param;
    }
    return nullptr;
}

bool
has_color_parameter(const asr::ShaderQuery &query, const char *name, bool closure_output)
{
    const auto *param = find_parameter(query, name);
    // OSL exposes closure storage as a pointer; isclosure defines its type.
    return param && (closure_output || BU_STR_EQUAL(param->get("type"), "color")) &&
	param->get<bool>("isoutput") == closure_output &&
	param->get<bool>("isclosure") == closure_output && !param->get<bool>("isarray");
}

const char *
parameter_error(const asr::ShaderQuery &query, const std::string &name, const std::string &value)
{
    const auto *param = find_parameter(query, name.c_str());
    if (!param)
	return "input does not exist";
    if (param->get<bool>("isoutput") || param->get<bool>("isclosure") ||
	param->get<bool>("isstruct") || param->get<bool>("isarray"))
	return "output, closure, structured, and array parameters cannot be assigned";

    std::istringstream input(value);
    input.imbue(std::locale::classic());
    std::string type;
    if (!(input >> type) || type != param->get("type"))
	return "declared type does not match the shader input";
    if (type == "string")
	return nullptr;

    const struct { const char *name; size_t components; } numeric_types[] = {
	{"int", 1}, {"float", 1}, {"color", 3}, {"point", 3},
	{"vector", 3}, {"normal", 3}, {"matrix", 16}
    };
    size_t components = 0;
    for (const auto &candidate : numeric_types)
	if (type == candidate.name) components = candidate.components;
    if (!components)
	return "unsupported input type";
    for (size_t i = 0; i < components; ++i) {
	if (type == "int") {
	    int number;
	    if (!(input >> number))
		return "expected a representable integer";
	} else {
	    float number;
	    if (!(input >> number) || !std::isfinite(number))
		return "expected finite numeric components";
	}
    }
    input >> std::ws;
    return input.eof() ? nullptr : "too many components or an invalid numeric value";
}

struct ReleaseQuery {
    void operator()(asr::ShaderQuery *query) const { query->release(); }
};
using QueryPtr = std::unique_ptr<asr::ShaderQuery, ReleaseQuery>;
using ShaderQueries = std::map<std::string, QueryPtr>;

bool
connection_order(const MaterialShader &shader, const ShaderQueries &queries,
    const char *material_name, std::vector<std::string> &order)
{
    auto supported_shape = [](const asf::Dictionary &param) {
	return !param.get<bool>("isstruct") &&
	    (!param.get<bool>("isarray") || param.get<int>("arraylen") > 0);
    };
    std::map<std::string, size_t> incoming;
    std::map<std::string, std::vector<std::string>> consumers, sources;
    for (const auto &query : queries)
	incoming.emplace(query.first, 0);
    for (const auto &connection : shader.connections) {
	const auto &src = connection.source;
	const auto &dst = connection.destination;
	const auto source = queries.find(src.node), destination = queries.find(dst.node);
	const asf::Dictionary *output = source == queries.end() ? nullptr : find_parameter(*source->second, src.parameter.c_str());
	const asf::Dictionary *input = destination == queries.end() ? nullptr : find_parameter(*destination->second, dst.parameter.c_str());
	// Query type strings include array element types and lengths.
	if (!output || !input || !output->get<bool>("isoutput") || input->get<bool>("isoutput") ||
	    !supported_shape(*output) || !supported_shape(*input) ||
	    output->get<bool>("isclosure") != input->get<bool>("isclosure") ||
	    !BU_STR_EQUAL(output->get("type"), input->get("type"))) {
	    bu_log("art: material '%s' has invalid OSL connection '%s.%s' -> '%s.%s': expected matching output/input types and fixed array lengths\n",
		material_name, src.node.c_str(), src.parameter.c_str(), dst.node.c_str(), dst.parameter.c_str());
	    return false;
	}
	++incoming.at(dst.node);
	consumers[src.node].push_back(dst.node);
	sources[dst.node].push_back(src.node);
    }

    std::set<std::string> ready;
    for (const auto &node : incoming)
	if (!node.second) ready.insert(node.first);
    while (!ready.empty()) {
	const std::string node = *ready.begin();
	ready.erase(ready.begin());
	order.push_back(node);
	for (const auto &consumer : consumers[node])
	    if (!--incoming.at(consumer)) ready.insert(consumer);
    }
    if (order.size() != queries.size()) {
	bu_log("art: material '%s' contains an OSL connection cycle\n", material_name);
	return false;
    }

    // Reject unused declarations rather than silently rendering a misspelled graph.
    std::set<std::string> reachable{TerminalLayer};
    std::vector<std::string> pending{TerminalLayer};
    while (!pending.empty()) {
	const std::string node = pending.back();
	pending.pop_back();
	for (const auto &source : sources[node])
	    if (reachable.insert(source).second) pending.push_back(source);
    }
    if (reachable.size() != queries.size()) {
	bu_log("art: every OSL node in material '%s' must contribute to the material layer\n", material_name);
	return false;
    }
    return true;
}

bool
set_parameters(const MaterialNode &node, const asr::ShaderQuery &query,
    const char *material_name, asr::ParamArray &params)
{
    for (const auto &parameter : node.parameters) {
	const char *error = parameter_error(query, parameter.first, parameter.second);
	if (error) {
	    bu_log("art: invalid OSL parameter '%s' for shader '%s' in material '%s': %s\n",
		parameter.first.c_str(), node.name.c_str(), material_name, error);
	    return false;
	}
	params.insert(parameter.first.c_str(), parameter.second.c_str());
    }
    return true;
}

}

bool
art_add_material(asr::Assembly &assembly, const char *name,
    const float srgb[3], struct db_i *db, const struct rt_comb_internal *comb,
    const asf::SearchPaths &search_paths)
{
    MaterialShader shader;
    if (!read_shader(db, comb, shader))
	return false;

    std::map<std::string, MaterialNode> nodes = shader.nodes;
    nodes.emplace(TerminalLayer, shader.root);
    ShaderQueries queries;
    for (const auto &node : nodes) {
	auto query = asr::ShaderQueryFactory::create(search_paths);
	if (!query->open(node.second.name.c_str())) {
	    bu_log("art: cannot find or query OSL shader '%s' for material '%s'\n", node.second.name.c_str(), name);
	    return false;
	}
	const bool surface_node = BU_STR_EQUAL(query->get_shader_type(), "surface");
	if ((!surface_node && !BU_STR_EQUAL(query->get_shader_type(), "shader")) ||
	    (surface_node && node.first != TerminalLayer)) {
	    bu_log("art: unsupported OSL shader type '%s' for layer '%s'\n", query->get_shader_type(), node.first.c_str());
	    return false;
	}
	queries.emplace(node.first, QueryPtr(query.release()));
    }
    const auto &query = queries.at(TerminalLayer);
    const bool surface = BU_STR_EQUAL(query->get_shader_type(), "surface");
    if (surface && shader.output_explicit) {
	bu_log("art: surface shader '%s' sets Ci directly and cannot specify OSL.output\n", shader.root.name.c_str());
	return false;
    }
    if (!surface && !has_color_parameter(*query, shader.output.c_str(), true)) {
	bu_log("art: OSL shader '%s' has no non-array closure color output '%s'\n", shader.root.name.c_str(), shader.output.c_str());
	return false;
    }
    if (!surface) {
	auto terminal = asr::ShaderQueryFactory::create(search_paths);
	if (!terminal->open("as_closure2surface")) {
	    bu_log("art: cannot find the as_closure2surface terminal shader\n");
	    return false;
	}
    }

    asr::ParamArray params;
    if (has_color_parameter(*query, "in_color", false)) {
	const asf::Color3f linear = asf::srgb_to_linear_rgb(asf::Color3f(srgb[0], srgb[1], srgb[2]));
	struct bu_vls value = BU_VLS_INIT_ZERO;
	bu_vls_printf(&value, "color %g %g %g", linear[0], linear[1], linear[2]);
	params.insert("in_color", bu_vls_cstr(&value));
	bu_vls_free(&value);
    }
    // Preserve the established default plastic coat when restoring OSL.
    if (shader.root.name == "as_plastic") {
	params.insert("in_specular_weight", "float 0.06")
	    .insert("in_specular_roughness", "float 0.08")
	    .insert("in_ior", "float 1.5");
    }

    // Explicit terminal inputs override region color and compatibility defaults.
    std::map<std::string, asr::ParamArray> parameters;
    parameters.emplace(TerminalLayer, params);
    for (const auto &node : nodes)
	if (!set_parameters(node.second, *queries.at(node.first), name, parameters[node.first]))
	    return false;

    std::vector<std::string> order;
    if (!connection_order(shader, queries, name, order))
	return false;
    const std::string group_name = std::string(name) + "_shader";
    auto group = asr::ShaderGroupFactory().create(group_name.c_str(), asr::ParamArray());
    for (const auto &node : order) {
	const bool surface_node = node == TerminalLayer && surface;
	group->add_shader(surface_node ? "surface" : "shader", nodes.at(node).name.c_str(),
	    node.c_str(), parameters.at(node));
    }
    for (const auto &connection : shader.connections)
	group->add_connection(connection.source.node.c_str(), connection.source.parameter.c_str(),
	    connection.destination.node.c_str(), connection.destination.parameter.c_str());
    if (!surface) {
	group->add_shader("surface", "as_closure2surface", SurfaceLayer, asr::ParamArray());
	group->add_connection(TerminalLayer, shader.output.c_str(), SurfaceLayer, "in_input");
    }
    assembly.shader_groups().insert(group);

    const std::string surface_name = std::string(name) + "_surface";
    assembly.surface_shaders().insert(asr::PhysicalSurfaceShaderFactory().create(
	surface_name.c_str(), asr::ParamArray()));
    assembly.materials().insert(asr::OSLMaterialFactory().create(name,
	asr::ParamArray().insert("osl_surface", group_name.c_str())
	.insert("surface_shader", surface_name.c_str())));
    return true;
}
