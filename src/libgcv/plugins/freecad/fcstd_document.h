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
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "vmath.h"

class gp_Quaternion;
class gp_Trsf;

namespace freecad {

using Matrix = std::array<fastf_t, ELEMENTS_PER_MAT>;
using Properties = std::map<std::string, std::string>;
using Rgb = std::array<unsigned char, 3>;
using Scale = std::array<double, 3>;

Rgb unpack_rgb(uint32_t rgba);

Matrix placement_matrix(const gp_Trsf &transform);
Matrix placement_matrix(const gp_Quaternion &rotation, const std::array<double, 3> &position);

struct Object {
    std::string type;
    std::string shape_file;
    std::string mesh_file;
    std::string appearance_file;
    std::string diffuse_file;
    std::string linked_object;
    std::string linked_file;
    std::vector<std::string> linked_subobjects;
    std::optional<size_t> linked_subobject_count;
    std::vector<std::string> children;
    std::vector<std::string> array_elements;
    std::string element_visibility;
    std::string array_placements;
    std::string array_scales;
    size_t element_count = 0;
    Properties properties;
    std::optional<Matrix> placement;
    Matrix link_placement = MAT_INIT_IDN;
    Scale scale_vector = {1.0, 1.0, 1.0};
    double scale = 1.0;
    bool has_link_placement = false;
    bool has_scale_vector = false;
    bool link_transform = false;
    bool unsupported_link = false;
    bool suppressed = false;
    bool visible = true;
    bool has_view_visibility = false;
    bool override_material = false;
    std::optional<Rgb> color;
    std::optional<Rgb> material_color;
    std::optional<double> transparency;
    std::optional<double> material_transparency;

    bool is_link_group() const
    {
	return type == "App::LinkGroup" || type == "App::LinkGroupPython";
    }

    bool is_link_element() const
    {
	return type == "App::LinkElement" || type == "App::LinkElementPython";
    }

    bool is_link() const
    {
	return type == "App::Link" || type == "App::LinkPython" || is_link_element();
    }

    bool has_link() const
    {
	return !linked_object.empty() || is_link() || unsupported_link;
    }

    bool expanded_array() const
    {
	return element_count && !array_elements.empty();
    }

    bool element_visible(size_t index) const
    {
	// The archived bitset stores element zero at the right.
	return index >= element_visibility.size() ||
	    element_visibility[element_visibility.size() - 1 - index] == '1';
    }
};

struct Document {
    Properties properties;
    std::map<std::string, Object> objects;
    bool legacy_alpha = true;
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

Document read_document(const char *path, const char *view_path = nullptr);

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
