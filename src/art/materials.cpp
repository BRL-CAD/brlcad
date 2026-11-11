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

#include <string>

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

struct MaterialShader {
    std::string name = "as_plastic";
    std::string output = "out_outColor";
    bool output_explicit = false;
};

bool
read_shader(struct db_i *db, const struct rt_comb_internal *comb, MaterialShader &shader)
{
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
    const char *osl_name = bu_avs_get(&material->opticalProperties, "OSL");
    const char *osl_output = bu_avs_get(&material->opticalProperties, "OSL.output");
    if (osl_name) shader.name = osl_name;
    if (osl_output) {
	shader.output = osl_output;
	shader.output_explicit = true;
    }
    rt_db_free_internal(&intern);
    if (shader.name.empty() || shader.output.empty()) {
	bu_log("art: material '%s' has an empty OSL shader or output\n", material_name);
	return false;
    }
    return true;
}

bool
has_color_parameter(const asr::ShaderQuery &query, const char *name, bool closure_output)
{
    for (size_t i = 0; i < query.get_param_count(); ++i) {
	const asf::Dictionary &param = query.get_param_info(i);
	if (BU_STR_EQUAL(param.get("name"), name)) {
	    // OSL exposes closure storage as a pointer; isclosure defines its type.
	    return (closure_output || BU_STR_EQUAL(param.get("type"), "color")) &&
		param.get<bool>("isoutput") == closure_output &&
		param.get<bool>("isclosure") == closure_output &&
		!param.get<bool>("isarray");
	}
    }
    return false;
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

    auto query = asr::ShaderQueryFactory::create(search_paths);
    if (!query->open(shader.name.c_str())) {
	bu_log("art: cannot find or query OSL shader '%s' for material '%s'\n", shader.name.c_str(), name);
	return false;
    }
    const bool surface = BU_STR_EQUAL(query->get_shader_type(), "surface");
    if (!surface && !BU_STR_EQUAL(query->get_shader_type(), "shader")) {
	bu_log("art: unsupported OSL shader type '%s' for '%s'\n", query->get_shader_type(), shader.name.c_str());
	return false;
    }
    if (surface && shader.output_explicit) {
	bu_log("art: surface shader '%s' sets Ci directly and cannot specify OSL.output\n", shader.name.c_str());
	return false;
    }
    if (!surface && !has_color_parameter(query.ref(), shader.output.c_str(), true)) {
	bu_log("art: OSL shader '%s' has no non-array closure color output '%s'\n", shader.name.c_str(), shader.output.c_str());
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
    if (has_color_parameter(query.ref(), "in_color", false)) {
	const asf::Color3f linear = asf::srgb_to_linear_rgb(asf::Color3f(srgb[0], srgb[1], srgb[2]));
	struct bu_vls value = BU_VLS_INIT_ZERO;
	bu_vls_printf(&value, "color %g %g %g", linear[0], linear[1], linear[2]);
	params.insert("in_color", bu_vls_cstr(&value));
	bu_vls_free(&value);
    }
    // Preserve the established default plastic coat when restoring OSL.
    if (shader.name == "as_plastic") {
	params.insert("in_specular_weight", "float 0.06")
	    .insert("in_specular_roughness", "float 0.08")
	    .insert("in_ior", "float 1.5");
    }

    const std::string group_name = std::string(name) + "_shader";
    auto group = asr::ShaderGroupFactory().create(group_name.c_str(), asr::ParamArray());
    group->add_shader(surface ? "surface" : "shader", shader.name.c_str(), "material", params);
    if (!surface) {
	group->add_shader("surface", "as_closure2surface", "surface", asr::ParamArray());
	group->add_connection("material", shader.output.c_str(), "surface", "in_input");
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
