/*                  M A T E R I A L X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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

#include "materialX.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstddef>
#include <fstream>

#include <MaterialXCore/Library.h>
#include <MaterialXCore/Document.h>
#include <MaterialXFormat/Export.h>
#include <MaterialXFormat/File.h>
#include <MaterialXFormat/XmlIo.h>
#include <MaterialXGenOsl/OslShaderGenerator.h>
#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenShader/ShaderGenerator.h>
#include <MaterialXGenShader/GenContext.h>
#include <MaterialXGenShader/TypeDesc.h>
#include <MaterialXGenOsl/OslSyntax.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXGenShader/Shader.h>

#include "bu/app.h"
#include "bu/str.h"
#include "bu/log.h"


namespace mx = MaterialX;


void
rt_material_mtlx_to_osl(const rt_material_internal material_ip, struct bu_vls* vp)
{

    // get the desired node definition from material_ip's optical properties
    const char* desiredNodeDef = (char*) "ND_standard_surface_surfaceshader";
    struct bu_vls m = BU_VLS_INIT_ZERO;
    bu_vls_printf(&m, "%s", bu_avs_get(&material_ip.opticalProperties, "MTLX"));
    if (!BU_STR_EQUAL(bu_vls_cstr(&m), "(null)")) {
        desiredNodeDef = bu_vls_cstr(&m);
        bu_log("material->optical->OSL: %s\n", desiredNodeDef);
    }

    mx::DocumentPtr doc = mx::createDocument();

    // ensure library is configured properly
    char ts[MAXPATHLEN] = { 0 };
    bu_dir(ts, MAXPATHLEN, BU_DIR_DATA, "materialX", NULL);
    mx::FileSearchPath searchPath;
    searchPath.append(mx::FilePath(ts));
    mx::loadLibraries({ "targets", "appleseed", "stdlib", "pbrlib", "bxdf" }, searchPath, doc);

    // obtain a node instance from the desired node definition
    mx::NodeDefPtr ndp = doc->getNodeDef(desiredNodeDef);
    if (ndp == nullptr) {
        std::cout << "ERROR: NODE DEFINITION POINTER IS NULL" << std::endl;
    }
    std::string nodeInstanceName(desiredNodeDef);
    nodeInstanceName += "Instance";
    mx::NodePtr nodeInstance = doc->addNodeInstance(ndp, nodeInstanceName);
    if (nodeInstance == nullptr) {
        std::cout << "ERROR: NODE INSTANCE POINTER IS NULL" << std::endl;
    }

    mx::ShaderGeneratorPtr sgp = mx::OslShaderGenerator::create();
    mx::GenContext context(mx::OslShaderGenerator::create());
    context.registerSourceCodeSearchPath(searchPath);

    // generate the OSL shader source code and write it to a file
    mx::ShaderPtr shader = sgp->generate(nodeInstance->getName(), nodeInstance, context);
    const std::string fileType = ".osl";
    const std::string filepath = mx::FilePath::getCurrentPath() / mx::FilePath(strcat((char*) desiredNodeDef, fileType.c_str()));
    std::ofstream file;
    file.open(filepath);
    file << shader->getSourceCode();

    // save the newly generated file's path
    bu_vls_strcpy(vp, filepath.c_str());

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
