/*                 F C S T D _ D O C U M E N T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef GCV_FREECAD_FCSTD_DOCUMENT_H
#define GCV_FREECAD_FCSTD_DOCUMENT_H

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "vmath.h"

class gp_Trsf;

namespace freecad {

using Matrix = std::array<fastf_t, ELEMENTS_PER_MAT>;
using Properties = std::map<std::string, std::string>;
using Rgb = std::array<unsigned char, 3>;

Matrix placement_matrix(const gp_Trsf &transform);

struct Object {
    std::string type;
    std::string shape_file;
    std::string linked_object;
    std::vector<std::string> children;
    Properties properties;
    Matrix placement = MAT_INIT_IDN;
    Matrix link_placement = MAT_INIT_IDN;
    std::array<double, 3> scale_vector = {1.0, 1.0, 1.0};
    double scale = 1.0;
    bool has_link_placement = false;
    bool has_scale_vector = false;
    bool link_transform = false;
    bool unsupported_link = false;
    bool visible = true;
    bool has_view_visibility = false;
    bool override_material = false;
    std::optional<Rgb> color;
    std::optional<Rgb> material_color;
};

struct Document {
    Properties properties;
    std::map<std::string, Object> objects;
};

class Archive {
public:
    explicit Archive(const char *path);
    ~Archive();
    Archive(const Archive &) = delete;
    Archive &operator=(const Archive &) = delete;

    bool next(std::string &name);
    std::ptrdiff_t read(void *buffer, size_t size);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

Document read_document(const char *path);

}

#endif

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
