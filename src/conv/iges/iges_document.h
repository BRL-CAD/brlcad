/*                 I G E S _ D O C U M E N T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#ifndef CONV_IGES_IGES_DOCUMENT_H
#define CONV_IGES_IGES_DOCUMENT_H

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace brlcad {
namespace iges {

enum class Severity {
    Information,
    Warning,
    Error,
    Fatal
};

struct SourceLocation {
    size_t record = 0;
    size_t column = 0;
};

struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;
    std::string message;
    int64_t entity_id = 0;
    int entity_type = 0;
    SourceLocation location;
};

/** IGES directory-entry sequence number.  Zero denotes no reference. */
class EntityId {
public:
    EntityId() = default;
    explicit EntityId(int64_t value) : value_(value) {}

    int64_t value() const { return value_; }
    bool empty() const { return value_ == 0; }
    bool valid() const { return value_ > 0 && (value_ & 1) == 1; }

    bool operator<(const EntityId &other) const { return value_ < other.value_; }
    bool operator==(const EntityId &other) const { return value_ == other.value_; }

private:
    int64_t value_ = 0;
};

struct GlobalSection {
    char parameter_delimiter = ',';
    char record_delimiter = ';';
    std::vector<std::string> parameters;
};

struct DirectoryEntry {
    EntityId id;
    int type = 0;
    int parameter_record = 0;
    int structure = 0;
    int line_font = 0;
    int level = 0;
    int view = 0;
    EntityId transform;
    int label_associativity = 0;
    int status = 0;
    int line_weight = 0;
    int color = 0;
    int parameter_line_count = 0;
    int form = 0;
    std::string label;
    int subscript = 0;
    size_t first_record = 0;
};

struct Parameter {
    std::string raw;
    SourceLocation location;

    bool empty() const;
    bool integer(int64_t &value) const;
    bool real(double &value) const;
    bool string(std::string &value) const;
    bool entity(EntityId &value) const;
};

struct ParameterList {
    EntityId owner;
    std::vector<Parameter> values;
    bool terminated = false;
};

/**
 * Immutable, validated representation of an IGES physical file.
 *
 * parse_file() and parse_buffer() always return a document.  Call valid()
 * before translating it; diagnostics retain all errors found during the
 * bounded validation pass.
 */
class Document {
public:
    static Document parse_file(const std::string &path);
    static Document parse_buffer(const std::string &contents,
        const std::string &source_name = std::string());

    bool valid() const;
    const std::string &source_name() const { return source_name_; }
    const GlobalSection &global() const { return global_; }
    const std::vector<DirectoryEntry> &entities() const { return entities_; }
    const std::vector<Diagnostic> &diagnostics() const { return diagnostics_; }
    size_t record_count() const { return record_count_; }

    const DirectoryEntry *entity(EntityId id) const;
    const ParameterList *parameters(EntityId id) const;
    std::vector<const DirectoryEntry *> find(int entity_type) const;

private:
    friend class Parser;

    std::string source_name_;
    GlobalSection global_;
    std::vector<DirectoryEntry> entities_;
    std::map<EntityId, size_t> entity_index_;
    std::map<EntityId, ParameterList> parameters_;
    std::vector<Diagnostic> diagnostics_;
    size_t record_count_ = 0;
};

} /* namespace iges */
} /* namespace brlcad */

#endif /* CONV_IGES_IGES_DOCUMENT_H */

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
