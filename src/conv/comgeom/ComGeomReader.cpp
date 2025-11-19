#include "ComGeomReader.h"

#include <cstdlib>
#include <cctype>
#include <algorithm>

ComGeomReader::ComGeomReader(FILE *fp) : m_fp(fp) {}
ComGeomReader::~ComGeomReader() = default;

std::optional<ComGeomReader::Line> ComGeomReader::next(const char * /*title*/) {
    if (!m_fp) return std::nullopt;
    std::string s;
    int c = 0;
    while ((c = fgetc(m_fp)) != EOF) {
        if (c == '\r') {
            int nc = fgetc(m_fp);
            if (nc != '\n' && nc != EOF) ungetc(nc, m_fp);
            break;
        }
        if (c == '\n') break;
        s.push_back(static_cast<char>(c));
    }
    if (c == EOF && s.empty())
        return std::nullopt;
    m_line++;
    return Line{std::move(s), m_line};
}

int ComGeomReader::fieldInt(std::string_view sv, size_t start, size_t len) {
    if (start >= sv.size()) return 0;
    size_t end = std::min(sv.size(), start + len);
    // Work on the field window only
    size_t b = start, e = end;
    // Trim leading whitespace
    while (b < e && std::isspace(static_cast<unsigned char>(sv[b]))) b++;
    // Trim trailing whitespace
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e-1]))) e--;
    if (b >= e) return 0;
    // Note that we preserve internal spaces to match the behavior of the
    // original C code - "1 23" would be read as 1 rather than 123.  Whether
    // or not this is really desirable behavior is a question - i.e. whether
    // there is a valid use case where a line would make the above separation
    // with intent in the COMGEOM formats - but for now we match legacy code.
    std::string token(sv.substr(b, e - b));
    return std::atoi(token.c_str());
}

double ComGeomReader::fieldDouble(std::string_view sv, size_t start, size_t len) {
    if (start >= sv.size()) return 0.0;
    size_t end = std::min(sv.size(), start + len);
    // Work on the field window only
    size_t b = start, e = end;
    // Trim leading whitespace
    while (b < e && std::isspace(static_cast<unsigned char>(sv[b]))) b++;
    // Trim trailing whitespace
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e-1]))) e--;
    if (b >= e) return 0.0;
    std::string token(sv.substr(b, e - b)); // preserve internal spaces to match legacy C
    return std::atof(token.c_str());
}

std::string ComGeomReader::rstrip(std::string_view s) {
    size_t end = s.size();
    while (end > 0 && s[end-1] == ' ') end--;
    return std::string(s.substr(0, end));
}

void ComGeomReader::toLowerInplace(std::string &s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

void ComGeomReader::trimLeadingSpacesInplace(std::string &s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i) s.erase(0, i);
}
