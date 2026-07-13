/*                         J T _ P A R S E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef LIBGCV_JT_PARSER_H
#define LIBGCV_JT_PARSER_H

#include "common.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace jt {

using Guid = std::array<unsigned char, 16>;

struct TocEntry {
    Guid id;
    uint64_t offset;
    uint32_t length;
    uint32_t attributes;

    uint8_t type() const { return static_cast<uint8_t>(attributes >> 24); }
};

struct Header {
    std::string version_text;
    unsigned major_version;
    unsigned minor_version;
    bool little_endian;
    uint64_t toc_offset;
    Guid lsg_segment_id;
};

struct Segment {
    Guid id;
    uint64_t offset;
    uint32_t length;
    uint32_t type;
};

struct Element {
    Guid type_id;
    uint8_t base_type;
    int32_t object_id;
    uint64_t data_offset;
    uint32_t data_length;
};

struct Mesh {
    std::vector<double> vertices;
    std::vector<int> faces;
};

enum class Predictor {
    Lag1, Lag2, Stride1, Stride2, StripIndex, Ramp, Xor1, Xor2, None
};

class File {
  public:
    bool load(const char *path, std::string &error);
    bool load(const std::vector<unsigned char> &bytes, std::string &error);
    bool segment(const TocEntry &entry, Segment &result, std::string &error) const;
    bool elements(const Segment &segment, std::vector<Element> &result, std::string &error) const;
    bool int32_packet(uint64_t offset, Predictor predictor, std::vector<int32_t> &values,
	size_t &bytes_read, std::string &error) const;
    bool int32_packet2(uint64_t offset, Predictor predictor, std::vector<int32_t> &values,
	size_t &bytes_read, std::string &error) const;
    bool is_legacy_mesh(const Element &element) const;
    bool legacy_mesh(const Element &element, Mesh &mesh, std::string &error) const;

    const Header &header() const { return header_; }
    const std::vector<TocEntry> &toc() const { return toc_; }

  private:
    Header header_{};
    std::vector<TocEntry> toc_;
    std::vector<unsigned char> bytes_;
};

bool has_jt_signature(const char *path);

} // namespace jt

#endif
