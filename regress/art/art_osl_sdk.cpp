/*              A R T _ O S L _ S D K . C P P
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
#include <OSL/oslcomp.h>
#include <OSL/oslexec.h>
#include <OSL/oslquery.h>
#include <OSL/rendererservices.h>
#include <OSL/shaderglobals.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failure(const char *message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return failure("Expected the installed stdosl.h path");
    OSL::OSLCompiler compiler;
    std::string bytecode;
    if (!compiler.compile_buffer("shader sdk(output float result=0) { result=2; }",
                                 bytecode, {}, argv[1]))
        return failure("Shader compilation failed");
    OSL::OSLQuery query;
    if (!query.open_bytecode(bytecode) || query.nparams() != 1)
        return failure("Shader query failed");

    OSL::RendererServices renderer;
    OSL::ShadingSystem system(&renderer);
    if (!system.LoadMemoryCompiledShader("sdk", bytecode))
        return failure("Compiled shader loading failed");
    auto group = system.ShaderGroupBegin("sdk");
    if (!group || !system.Shader(*group, "surface", "sdk", "surface") ||
        !system.ShaderGroupEnd(*group))
        return failure("Shader group construction failed");
    const char *outputs[] = {"result"};
    if (!system.attribute(group.get(), "renderer_outputs",
                          OSL::TypeDesc(OSL::TypeDesc::STRING, 1), outputs))
        return failure("Shader output registration failed");

    auto *thread = system.create_thread_info();
    if (!thread)
        return failure("Thread context allocation failed");
    auto *context = system.get_context(thread);
    if (!context) {
        system.destroy_thread_info(thread);
        return failure("Shading context allocation failed");
    }
    OSL::ShaderGlobals globals = {};
    bool valid = system.execute(*context, *group, 0, 0, globals, nullptr, nullptr);
    OSL::TypeDesc type;
    const auto *value = static_cast<const float *>(
        system.get_symbol(*context, OSL::ustring("result"), type));
    constexpr float ExpectedOutput = 2.0f;
    constexpr float OutputTolerance = 1.0e-6f;
    valid = valid && value && type == OSL::TypeDesc::FLOAT &&
        std::isfinite(*value) && std::abs(*value - ExpectedOutput) < OutputTolerance;
    system.release_context(context);
    system.destroy_thread_info(thread);
    return valid ? EXIT_SUCCESS : failure("Shader execution produced unexpected output");
}
