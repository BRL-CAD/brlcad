/*             T E S T _ V R M L 1 _ P A R S E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "vrml1_parser.h"

#include "bu/app.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

bool
expect(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

bool
check_scene()
{
    const std::string input = R"vrml(
#VRML V1.0 ascii
DEF Shared Separator {
  Material {
    diffuseColor [ 1 0 0, 0 1 0 ]
    transparency [ 0, .25 ]
  }
  Translation { translation 1 2 3 }
  Coordinate3 {
    point [ 0 0 0, 2 0 0, 2 2 0, 1 1 0, 0 2 0 ]
  }
  ShapeHints { faceType UNKNOWN_FACE_TYPE }
  IndexedFaceSet {
    coordIndex [ 0, 1, 2, 3, 4, -1 ]
    materialIndex 1
  }
  InventorExtension { arbitrary [ 1 2 3 ] }
}
USE Shared
Switch {
  whichChild 0
  Cube { width 4 height 5 depth 6 }
  Sphere { radius 2 }
}
MatrixTransform {
  matrix
    1 0.5 0 0
    0 1   0 0
    0 0   1 0
    0 0   0 1
}
Cylinder { parts (SIDES | BOTTOM) }
Switch { whichChild 0x1 }
)vrml";

    vrml1::Parser parser;
    std::vector<vrml1::NodePtr> nodes;
    std::string error;
    if (!expect(parser.parse(input, nodes, error), error.c_str())) return false;
    if (!expect(nodes.size() == 6, "wrong top-level node count")) return false;
    if (!expect(nodes[0] == nodes[1], "USE did not resolve to the DEF node")) return false;
    if (!expect(nodes[0]->type == "Separator" && nodes[0]->def_name == "Shared",
	    "DEF separator was not parsed")) return false;
    if (!expect(nodes[0]->children.size() == 6, "wrong separator child count")) return false;

    const vrml1::Node &material = *nodes[0]->children[0];
    const vrml1::Field *diffuse = vrml1::field(material, "diffuseColor");
    if (!expect(diffuse && diffuse->numbers.size() == 6 && diffuse->numbers[3] == 0.0,
	    "material colors were not parsed")) return false;

    const vrml1::Node &translation = *nodes[0]->children[1];
    const vrml1::Field *vector = vrml1::field(translation, "translation");
    if (!expect(vector && vector->numbers.size() == 3 && vector->numbers[2] == 3.0,
	    "translation was not parsed")) return false;

    const vrml1::Node &face_set = *nodes[0]->children[4];
    const vrml1::Field *indices = vrml1::field(face_set, "coordIndex");
    if (!expect(indices && indices->integers.size() == 6 && indices->integers.back() == -1,
	    "coordinate indices were not parsed")) return false;
    if (!expect(nodes[2]->children.size() == 2 && nodes[2]->children[0]->type == "Cube",
	    "switch children were not parsed")) return false;

    const vrml1::Field *matrix = vrml1::field(*nodes[3], "matrix");
    if (!expect(matrix && matrix->numbers.size() == 16 && matrix->numbers[1] == 0.5,
	    "matrix was not parsed")) return false;
    const vrml1::Field *parts = vrml1::field(*nodes[4], "parts");
    if (!expect(parts && parts->symbol == "SIDES|BOTTOM", "bitmask was not parsed")) return false;
    const vrml1::Field *choice = vrml1::field(*nodes[5], "whichChild");
    return expect(choice && !choice->integers.empty() && choice->integers[0] == 1,
	"hexadecimal integer was not parsed");
}

bool
check_failures()
{
    vrml1::Parser parser;
    std::vector<vrml1::NodePtr> nodes;
    std::string error;
    if (!expect(!parser.parse("#VRML V1.0 ascii\nUSE Missing", nodes, error),
	    "undefined USE was accepted")) return false;
    if (!expect(error.find("undefined USE") != std::string::npos,
	    "undefined USE error lacks context")) return false;
    if (!expect(!parser.parse("#VRML V1.0 ascii\nDEF Loop Group { USE Loop }", nodes, error),
	    "recursive USE was accepted")) return false;
    return expect(!parser.parse("#VRML V1.0 ascii\nCube { width nope }", nodes, error),
	"invalid numeric field was accepted");
}

} // namespace

int
main(int argc, char **argv)
{
    (void)argc;
    bu_setprogname(argv[0]);
    return check_scene() && check_failures() ? 0 : 1;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
