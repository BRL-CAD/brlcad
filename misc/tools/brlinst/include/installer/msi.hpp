// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "installer/core.hpp"
namespace installer::msi {
// The model is platform independent for testing. Only the Windows backend can
// serialize it, using msi.dll and cabinet.dll. No alternate MSI implementation.
struct Column {
    std::string name, type, category;
    bool nullable = true, key = false, localized = false;
    std::string foreign_table;
    int foreign_column = 0;
};
struct Table {
    std::string name;
    std::vector<Column> columns;
    std::vector<Json> rows;
};
struct Payload {
    std::string id, path;
    fs::path source;
    std::uint64_t size;
};
struct Model {
    explicit Model(Architecture value) : architecture(value) {}
    std::vector<Table> tables;
    std::vector<Payload> files;
    std::string product_code, name;
    Architecture architecture;
    Table &table(const std::string &name);
};
using GuidFactory = std::function<std::string(const std::string &)>;
Model make_model(const WindowsPackageInput &input, const fs::path &notices, const fs::path &actions,
                 const GuidFactory &guid);
std::string sql_create(const Table &table);
std::string formatted_literal(const std::string &text);
#ifdef _WIN32
void create(const fs::path &config, const fs::path &source, const fs::path &output, const fs::path &actions,
            const Progress &progress = {});
void verify(const fs::path &package, const Progress &progress = {});
#endif
} // namespace installer::msi
