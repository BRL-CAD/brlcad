#ifndef BRLCAD_COMGEOM_READER_H
#define BRLCAD_COMGEOM_READER_H

#include "common.h"

#include <cstdio>
#include <string>
#include <optional>
#include <string_view>

class ComGeomReader {
public:
    struct Line {
        std::string s;
        size_t number = 0;
        std::string_view view() const { return s; }
    };

    explicit ComGeomReader(FILE *fp);
    ~ComGeomReader();

    std::optional<Line> next(const char *title);

    static int fieldInt(std::string_view sv, size_t start, size_t len);
    static double fieldDouble(std::string_view sv, size_t start, size_t len);

    static std::string rstrip(std::string_view s);
    static void toLowerInplace(std::string &s);
    static void trimLeadingSpacesInplace(std::string &s);

    size_t lineNo() const { return m_line; }

private:
    FILE *m_fp = nullptr;
    size_t m_line = 0;
};

#endif