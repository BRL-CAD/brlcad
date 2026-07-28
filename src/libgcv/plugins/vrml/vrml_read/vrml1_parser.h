/*                 V R M L 1 _ P A R S E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GCV_VRML1_PARSER_H
#define GCV_VRML1_PARSER_H

#include "common.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vrml1 {

struct Field {
    std::vector<double> numbers;
    std::vector<long long> integers;
    std::vector<std::string> strings;
    std::string symbol;
};

struct Node {
    std::string type;
    std::string def_name;
    std::map<std::string, Field> fields;
    std::vector<std::shared_ptr<Node>> children;
};

using NodePtr = std::shared_ptr<Node>;

class Parser {
public:
    bool parse(const std::string &input, std::vector<NodePtr> &nodes, std::string &error);

private:
    class Impl;
};

const Field *field(const Node &node, const char *name);

} // namespace vrml1

#endif

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
