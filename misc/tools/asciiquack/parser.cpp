/// @file parser.cpp
/// @brief AsciiDoc parser – implementation.
///
/// Design notes
/// ─────────────
/// * Line-by-line single-pass: the parser reads one line at a time, using
///   peek_line() to look ahead without consuming.
/// * Attribute entries may appear anywhere in the document body, not just the
///   header; they are applied to the Document immediately when encountered.
/// * Delimited blocks (----  ....  ====  ____  ****  ++++  --) are consumed
///   by reading until the matching close delimiter.
/// * Sections are detected by counting leading '=' characters.
/// * Block-attribute lines ([source,java], .Title, [[anchor]]) accumulate in
///   a temporary map and are applied to the next non-attribute block.

#include "parser.hpp"
#include "substitutors.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

// Block-line scanner and attribute-list parser (C API — always compiled).
extern "C" {
#include "block_scanner.h"
#include "attr_list.h"
}

namespace asciiquack {

// ═════════════════════════════════════════════════════════════════════════════
// Internal helpers  (anonymous namespace)
// ═════════════════════════════════════════════════════════════════════════════
namespace {

/// Return true if @p line is empty or consists solely of whitespace.
bool is_blank(std::string_view line) noexcept {
    return line.find_first_not_of(" \t") == std::string_view::npos;
}

/// Return true if @p line is a line comment (// …) but not a block comment (////).
bool is_line_comment(std::string_view line) noexcept {
    return line.size() >= 2 && line[0] == '/' && line[1] == '/' &&
           (line.size() == 2 || line[2] != '/');
}

/// Trim leading and trailing ASCII whitespace from @p s in-place.
void trim(std::string& s) {
    // Left trim
    auto left = s.find_first_not_of(" \t");
    if (left == std::string::npos) { s.clear(); return; }
    if (left > 0) { s.erase(0, left); }
    // Right trim
    auto right = s.find_last_not_of(" \t");
    if (right != std::string::npos && right + 1 < s.size()) {
        s.erase(right + 1);
    }
}

/// Return a lower-cased copy of @p s.
std::string to_lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c + ('a' - 'A')); }
    }
    return s;
}

// ── Delimiter recognition ─────────────────────────────────────────────────────

/// Map a delimiter line to its BlockContext.
/// Returns BlockContext::Paragraph if the line is not a known delimiter.
struct DelimiterInfo {
    BlockContext context;
    ContentModel content_model;
};

std::optional<DelimiterInfo> classify_delimiter(std::string_view line) noexcept {
    if (line == "----") return DelimiterInfo{BlockContext::Listing, ContentModel::Verbatim};
    if (line == "....")  return DelimiterInfo{BlockContext::Literal, ContentModel::Verbatim};
    if (line == "====") return DelimiterInfo{BlockContext::Example, ContentModel::Compound};
    if (line == "____") return DelimiterInfo{BlockContext::Quote,   ContentModel::Compound};
    if (line == "****") return DelimiterInfo{BlockContext::Sidebar, ContentModel::Compound};
    if (line == "++++") return DelimiterInfo{BlockContext::Pass,    ContentModel::Raw};
    if (line == "--")   return DelimiterInfo{BlockContext::Open,    ContentModel::Compound};
    // Extended tildes / dashes for listing blocks
    if (line.size() >= 4) {
        bool all_same = true;
        for (char c : line) { if (c != line[0]) { all_same = false; break; } }
        if (all_same) {
            if (line[0] == '-') return DelimiterInfo{BlockContext::Listing, ContentModel::Verbatim};
            if (line[0] == '.') return DelimiterInfo{BlockContext::Literal, ContentModel::Verbatim};
            if (line[0] == '=') return DelimiterInfo{BlockContext::Example, ContentModel::Compound};
            if (line[0] == '_') return DelimiterInfo{BlockContext::Quote,   ContentModel::Compound};
            if (line[0] == '*') return DelimiterInfo{BlockContext::Sidebar, ContentModel::Compound};
            if (line[0] == '+') return DelimiterInfo{BlockContext::Pass,    ContentModel::Raw};
            if (line[0] == '~') return DelimiterInfo{BlockContext::Listing, ContentModel::Verbatim};
        }
    }
    return std::nullopt;
}

// ── Attribute entry parsing ────────────────────────────────────────────────────

/// If @p line is an attribute entry ( :name: value ) parse it and return
/// {name, value}.  Returns nullopt otherwise.
std::optional<std::pair<std::string,std::string>>
try_parse_attribute_entry(const std::string& line) {
    AqBlockScanResult r = aq_scan_block_line(line.c_str(), line.size());
    if (r.type != AQ_BT_ATTR_ENTRY) { return std::nullopt; }
    std::string name(line.data() + r.caps[0].off, r.caps[0].len);
    std::string value = r.caps[1].len
        ? std::string(line.data() + r.caps[1].off, r.caps[1].len) : "";
    trim(name);
    trim(value);
    return std::make_pair(std::move(name), std::move(value));
}

// ── Block attribute line recognition ──────────────────────────────────────────

/// Return true if @p line is a block attribute line: [...]
bool is_block_attribute_line(std::string_view line) noexcept {
    if (line.size() < 2) { return false; }
    if (line.front() != '[' || line.back() != ']') { return false; }
    // Must not look like an inline anchor [[id]]
    if (line.size() >= 4 && line[1] == '[') { return true; }  // [[anchor]]
    if (line[1] == ']') { return false; }  // empty: []
    return true;
}

/// Return true if @p line is a block-title line: .Title text
bool is_block_title_line(std::string_view line) noexcept {
    if (line.size() < 2) { return false; }
    if (line[0] != '.') { return false; }
    if (line[1] == ' ' || line[1] == '.') { return false; }  // .. or ". "
    return true;
}

/// Parse the attribute list content inside [ ... ].
/// Returns a map of positional and named attributes.
std::unordered_map<std::string, std::string>
parse_attribute_list(const std::string& line) {
    std::unordered_map<std::string, std::string> result;
    if (line.size() < 2) { return result; }

    // Handle [[anchor,reftext]] specially
    if (line.size() >= 4 && line[0] == '[' && line[1] == '[') {
        std::string inner = line.substr(2, line.size() - 4);
        result["anchor"] = inner;
        return result;
    }

    // Strip outer [ ]
    std::string inner = line.substr(1, line.size() - 2);
    if (inner.empty()) { return result; }

    // Delegate to the hand-written attr-list parser in block_scanner_hand.c.
    aq_parse_attr_list(
        inner.c_str(), inner.size(),
        [](void *ud, const char *k, std::size_t klen,
           const char *v, std::size_t vlen) {
            auto& m = *static_cast<std::unordered_map<std::string,std::string>*>(ud);
            m[std::string(k, klen)] = std::string(v, vlen);
        },
        &result);
    return result;
}

// ── List marker detection ──────────────────────────────────────────────────────

/// Try to classify a line as a list item and return its type and marker.
struct ListMatch {
    ListType    type;
    std::string marker;   ///< the leading marker characters
    std::string text;     ///< rest of the line after the marker
    std::string term;     ///< for description lists: the term before the marker (from regex)
};

std::optional<ListMatch> match_list_item(const std::string& line) {
    // Guard: exclude table rows (lines starting with '|').
    if (!line.empty() && line[0] == '|') { return std::nullopt; }

    AqBlockScanResult r = aq_scan_block_line(line.c_str(), line.size());

    // Helper: extract one scanner capture as std::string.
    auto cap = [&](int i) -> std::string {
        if (r.caps[i].len == 0) { return {}; }
        return std::string(line.data() + r.caps[i].off, r.caps[i].len);
    };

    switch (r.type) {
        case AQ_BT_LIST_UNORD:
            return ListMatch{ListType::Unordered, cap(0), cap(1), {}};
        case AQ_BT_LIST_ORD:
            return ListMatch{ListType::Ordered, cap(0), cap(1), {}};
        case AQ_BT_LIST_DESCRIPT: {
            // caps[0]=term (trimmed), caps[1]=separator, caps[2]=body
            std::string term = cap(0);
            trim(term);  // defensive: scanner already skips leading WS
            return ListMatch{ListType::Description, cap(1), cap(2), std::move(term)};
        }
        case AQ_BT_LIST_CALLOUT:
            return ListMatch{ListType::Callout, cap(0), cap(1), {}};
        default:
            return std::nullopt;
    }
}

// ── Image macro detection ──────────────────────────────────────────────────────

struct ImageMacro {
    std::string target;
    std::string alt;
    std::unordered_map<std::string, std::string> attrs;
};

// Return the filename stem (no directory, no extension) from a path,
// matching asciidoctor's default alt-text derivation for block images.
static std::string stem_from_path(const std::string& target) {
    auto slash = target.find_last_of("/\\");
    std::string name = (slash != std::string::npos) ? target.substr(slash + 1) : target;
    auto dot = name.rfind('.');
    if (dot != std::string::npos) { name = name.substr(0, dot); }
    return name;
}

std::optional<ImageMacro> match_block_image(const std::string& line) {
    AqBlockScanResult r = aq_scan_block_line(line.c_str(), line.size());
    if (r.type != AQ_BT_BLOCK_IMAGE) { return std::nullopt; }
    ImageMacro img;
    img.target = std::string(line.data() + r.caps[0].off, r.caps[0].len);
    std::string attr_str = "[" + std::string(line.data() + r.caps[1].off, r.caps[1].len) + "]";
    img.attrs  = parse_attribute_list(attr_str);
    img.alt    = img.attrs.count("1") ? img.attrs["1"] : stem_from_path(img.target);
    return img;
}

// ── Video / audio macro detection ────────────────────────────────────────────

struct MediaMacro {
    BlockContext context;   ///< Video or Audio
    std::string  target;
    std::unordered_map<std::string, std::string> attrs;
};

std::optional<MediaMacro> match_block_media(const std::string& line) {
    AqBlockScanResult r = aq_scan_block_line(line.c_str(), line.size());
    if (r.type != AQ_BT_BLOCK_MEDIA) { return std::nullopt; }
    MediaMacro mm;
    std::string mtype(line.data() + r.caps[0].off, r.caps[0].len);
    mm.context = (mtype == "video") ? BlockContext::Video : BlockContext::Audio;
    mm.target  = std::string(line.data() + r.caps[1].off, r.caps[1].len);
    std::string attr_str = "[" + std::string(line.data() + r.caps[2].off, r.caps[2].len) + "]";
    mm.attrs   = parse_attribute_list(attr_str);
    return mm;
}

// ── Conditional preprocessing helpers ────────────────────────────────────────

/// Evaluate whether an attribute name (possibly compound with +/,) is set.
/// + means ALL must be set; , means ANY must be set.
bool evaluate_attr_condition(const std::string& attr_spec,
                             const Document&    doc) {
    // Check for '+' (all-of) compound
    if (attr_spec.find('+') != std::string::npos) {
        std::istringstream ss(attr_spec);
        std::string part;
        while (std::getline(ss, part, '+')) {
            trim(part);
            if (!part.empty() && !doc.has_attr(part)) { return false; }
        }
        return true;
    }
    // Check for ',' (any-of) compound
    if (attr_spec.find(',') != std::string::npos) {
        std::istringstream ss(attr_spec);
        std::string part;
        while (std::getline(ss, part, ',')) {
            trim(part);
            if (!part.empty() && doc.has_attr(part)) { return true; }
        }
        return false;
    }
    // Simple single attribute
    return doc.has_attr(attr_spec);
}

/// Skip lines from @p reader until a matching endif:: directive (or EOF).
/// Supports one level of nesting.
void skip_conditional_block(Reader& reader) {
    int depth = 1;
    while (reader.has_more_lines() && depth > 0) {
        auto opt = reader.read_line();
        if (!opt) { break; }
        const std::string& l = *opt;

        // Detect nested conditional openings (multi-line form only)
        if (l.size() > 7) {
            bool is_open = (l.substr(0, 7) == "ifdef::" ||
                            l.substr(0, 8) == "ifndef::" ||
                            l.substr(0, 9) == "ifeval::");
            if (is_open) {
                // Only multi-line forms (empty bracket at end) deepen nesting
                if (!l.empty() && l.back() == ']') {
                    auto br = l.rfind('[');
                    if (br != std::string::npos &&
                        br + 2 == l.size() && l.back() == ']') {
                        ++depth;
                        continue;
                    }
                }
            }
        }
        if (l.size() >= 7 && l.substr(0, 7) == "endif::") {
            --depth;
        }
    }
}

/// Process an ifdef:: / ifndef:: directive line.
/// Returns false if the document pointer is null (shouldn't happen in practice).
bool handle_conditional(const std::string& line, Reader& reader, const Document* doc) {
    if (!doc) { return false; }

    bool is_ifdef = (line.size() >= 7 && line.substr(0, 7) == "ifdef::");
    std::size_t prefix_len = is_ifdef ? 7u : 8u;  // "ifdef::" or "ifndef::"

    auto bracket = line.find('[', prefix_len);
    if (bracket == std::string::npos) { return false; }

    std::string attr_spec = line.substr(prefix_len, bracket - prefix_len);
    trim(attr_spec);

    std::string content = line.substr(bracket + 1);
    if (!content.empty() && content.back() == ']') { content.pop_back(); }

    bool attr_set  = evaluate_attr_condition(attr_spec, *doc);
    bool condition = is_ifdef ? attr_set : !attr_set;

    if (content.empty()) {
        // Multi-line form: skip the block if condition is false
        if (!condition) {
            skip_conditional_block(reader);
        }
    } else {
        // Single-line form: push inline content back if condition is true
        if (condition) {
            reader.unshift_line(content);
        }
    }
    return true;
}

/// Evaluate a basic ifeval expression of the form:  lhs op rhs
/// where lhs/rhs are "string", {attr}, or numeric literals.
bool evaluate_ifeval(const std::string& expr, const Document& doc) {
    // Expand attribute references in the expression
    std::string expanded = sub_attributes(expr, doc.attributes());
    trim(expanded);

    // Find the leftmost two- or one-character comparison operator.
    // Scan left-to-right, skipping whitespace on both sides.
    const std::size_t n = expanded.size();
    std::string op;
    std::size_t op_pos = std::string::npos;
    std::size_t op_len = 0;

    for (std::size_t i = 0; i < n; ) {
        if (expanded[i] == ' ' || expanded[i] == '\t') { ++i; continue; }
        // Try two-char operators first
        if (i + 1 < n) {
            std::string_view two(expanded.data() + i, 2);
            if (two == "==" || two == "!=" || two == "<=" || two == ">=") {
                op = std::string(two);
                op_pos = i; op_len = 2;
                break;
            }
        }
        if (expanded[i] == '<' || expanded[i] == '>') {
            op = std::string(1, expanded[i]);
            op_pos = i; op_len = 1;
            break;
        }
        ++i;
    }

    if (op_pos == std::string::npos) { return false; }

    std::string lhs_raw = expanded.substr(0, op_pos);
    std::string rhs_raw = expanded.substr(op_pos + op_len);
    trim(lhs_raw); trim(rhs_raw);

    // Strip surrounding quotes
    auto strip_quotes = [](std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            s = s.substr(1, s.size() - 2);
        }
    };
    strip_quotes(lhs_raw);
    strip_quotes(rhs_raw);

    // Try numeric comparison first (for ordering operators only)
    try {
        double lhs_n = std::stod(lhs_raw);
        double rhs_n = std::stod(rhs_raw);
        if (op == "<")  return lhs_n <  rhs_n;
        if (op == "<=") return lhs_n <= rhs_n;
        if (op == ">")  return lhs_n >  rhs_n;
        if (op == ">=") return lhs_n >= rhs_n;
    } catch (...) {}

    // Fall back to string comparison
    if (op == "==") return lhs_raw == rhs_raw;
    if (op == "!=") return lhs_raw != rhs_raw;
    if (op == "<")  return lhs_raw <  rhs_raw;
    if (op == "<=") return lhs_raw <= rhs_raw;
    if (op == ">")  return lhs_raw >  rhs_raw;
    if (op == ">=") return lhs_raw >= rhs_raw;
    return false;
}

// ── include:: directive helper ────────────────────────────────────────────────

/// Process an include:: directive line and push included content into @p reader.
void handle_include(const std::string& line, Reader& reader, Document& doc) {
    // Safe-mode check
    if (doc.safe_mode() >= SafeMode::Secure) { return; }

    // Depth / count limit
    if (!doc.try_enter_include()) { return; }

    // Parse:  include::path[opts]
    const std::size_t prefix_len = 9;  // "include::"
    auto bracket = line.find('[', prefix_len);
    if (bracket == std::string::npos) { return; }

    std::string path_str = line.substr(prefix_len, bracket - prefix_len);
    trim(path_str);
    if (path_str.empty()) { return; }

    // Expand attribute references in the path
    path_str = sub_attributes(path_str, doc.attributes());

    // Resolve path relative to the document's base directory
    namespace fs = std::filesystem;
    fs::path target_path;
    try {
        if (fs::path(path_str).is_relative()) {
            std::string base = doc.base_dir();
            if (base.empty()) { base = "."; }
            target_path = fs::path(base) / path_str;
        } else {
            target_path = fs::path(path_str);
        }
    } catch (...) {
        return;
    }

    // In Safe mode, only allow paths within the base directory
    if (doc.safe_mode() == SafeMode::Safe) {
        try {
            auto canon_target = fs::weakly_canonical(target_path);
            std::string base  = doc.base_dir();
            if (base.empty()) { base = "."; }
            auto canon_base = fs::weakly_canonical(fs::path(base));
            // Check that the target is under base
            auto rel = fs::relative(canon_target, canon_base);
            std::string rel_str = rel.generic_string();
            if (!rel_str.empty() && rel_str[0] == '.') {
                return;  // outside base directory
            }
        } catch (...) {
            return;
        }
    }

    // Read the file
    std::ifstream file(target_path);
    if (!file.is_open()) {
        std::cerr << "asciiquack: WARNING: include file not found: "
                  << target_path.string() << '\n';
        return;
    }

    std::vector<std::string> included_lines;
    std::string cur;
    for (std::istreambuf_iterator<char> it(file), end_it; it != end_it; ++it) {
        char c = *it;
        if (c == '\n') {
            if (!cur.empty() && cur.back() == '\r') { cur.pop_back(); }
            included_lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        if (cur.back() == '\r') { cur.pop_back(); }
        included_lines.push_back(cur);
    }

    // Push the included lines back into the reader (in reverse, so first line
    // will be read first via unshift_lines)
    reader.unshift_lines(std::move(included_lines));
}

// ── Thematic break / page break detection ────────────────────────────────────

bool is_thematic_break(const std::string& line) noexcept {
    // Delegate to the hand-written scanner which already handles all cases.
    return aq_scan_block_line(line.c_str(), line.size()).type == AQ_BT_THEMATIC_BREAK;
}

bool is_page_break(const std::string& line) noexcept {
    return line.size() >= 3 && line[0] == '<' && line[1] == '<' && line[2] == '<';
}

// ── Author line detection / parsing ──────────────────────────────────────────

/// Return true if @p s is a valid author name word: starts with a letter/digit/
/// underscore, then has word chars, hyphens, apostrophes, or dots.
static bool is_author_word(const std::string& s) {
    if (s.empty()) { return false; }
    if (!std::isalnum(static_cast<unsigned char>(s[0])) && s[0] != '_') { return false; }
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) &&
            c != '_' && c != '-' && c != '\'' && c != '.') {
            return false;
        }
    }
    return true;
}

/// Try to parse a single author entry "Word [Word [Word]] [<email>]".
/// Returns false if the part doesn't look like an author name.
static bool parse_single_author(const std::string& part, AuthorInfo& out) {
    out = {};
    if (part.empty()) { return false; }

    // Find optional email in angle brackets — must appear at the END of the
    // string (with only whitespace after the closing '>').
    std::string name_part = part;
    auto email_open = name_part.rfind('<');
    if (email_open != std::string::npos) {
        auto email_close = name_part.find('>', email_open);
        if (email_close != std::string::npos) {
            // Verify that only whitespace follows the '>'
            bool only_ws_after = true;
            for (std::size_t k = email_close + 1; k < name_part.size(); ++k) {
                if (name_part[k] != ' ' && name_part[k] != '\t') {
                    only_ws_after = false;
                    break;
                }
            }
            if (only_ws_after) {
                out.email = name_part.substr(email_open + 1,
                                             email_close - email_open - 1);
                name_part = name_part.substr(0, email_open);
                while (!name_part.empty() && name_part.back() == ' ') {
                    name_part.pop_back();
                }
            }
        }
    }

    // Split name_part on spaces into up to 3 words
    std::vector<std::string> words;
    {
        std::size_t i = 0;
        const std::size_t n = name_part.size();
        while (i < n) {
            while (i < n && name_part[i] == ' ') ++i;
            if (i >= n) break;
            std::size_t ws = name_part.find(' ', i);
            words.push_back(name_part.substr(i, ws == std::string::npos ? std::string::npos : ws - i));
            i = (ws == std::string::npos) ? n : ws + 1;
        }
    }

    if (words.empty()) { return false; }
    if (!is_author_word(words[0])) { return false; }

    out.firstname = words[0];
    if (words.size() == 2) {
        if (!is_author_word(words[1])) { return false; }
        out.lastname = words[1];
    } else if (words.size() >= 3) {
        if (!is_author_word(words[1]) || !is_author_word(words[2])) { return false; }
        out.middlename = words[1];
        out.lastname   = words[2];
        // More than 3 words → not a valid author line
        if (words.size() > 3) { return false; }
    }
    return true;
}

bool looks_like_author_line(const std::string& line) {
    if (line.empty() || line[0] == ':') { return false; }
    const bool has_email = (line.find('<') != std::string::npos);
    if (!has_email) {
        char last = line.back();
        if (last == '.' || last == '?' || last == '!' || last == ',') { return false; }
    }
    // Check each "; "-separated segment
    std::string rest = line;
    while (true) {
        auto sep = rest.find("; ");
        std::string part = (sep != std::string::npos) ? rest.substr(0, sep) : rest;
        auto first = part.find_first_not_of(" \t");
        auto last_pos = part.find_last_not_of(" \t");
        if (first == std::string::npos) { return false; }
        part = part.substr(first, last_pos - first + 1);
        AuthorInfo dummy;
        if (!parse_single_author(part, dummy)) { return false; }
        if (sep == std::string::npos) { break; }
        rest = rest.substr(sep + 2);
    }
    return true;
}

std::vector<AuthorInfo> do_parse_author_line(const std::string& line) {
    std::vector<AuthorInfo> authors;
    std::string rest = line;
    while (true) {
        auto sep = rest.find("; ");
        std::string part = (sep != std::string::npos) ? rest.substr(0, sep) : rest;
        trim(part);
        AuthorInfo a;
        if (parse_single_author(part, a)) {
            authors.push_back(std::move(a));
        }
        if (sep == std::string::npos) { break; }
        rest = rest.substr(sep + 2);
    }
    return authors;
}

/// Return true if @p line looks like a revision line:
///   v1.0, 1.0.3, v1.0,2024-01-01: remark, or 2024-01-01
static bool looks_like_revision_line(const std::string& line) {
    if (line.empty()) { return false; }
    std::size_t i = 0;
    if (line[i] == 'v') { ++i; }
    if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i]))) { return false; }
    // Accept digits, letters, '.', '-' (version chars)
    while (i < line.size() &&
           (std::isalnum(static_cast<unsigned char>(line[i])) ||
            line[i] == '.' || line[i] == '-')) {
        ++i;
    }
    // Either end of string, or followed by comma (date/remark separator)
    return i == line.size() || line[i] == ',';
}

RevisionInfo do_parse_revision_line(const std::string& line) {
    RevisionInfo rev;
    std::string s = line;
    if (!s.empty() && s[0] == 'v') { s = s.substr(1); }

    auto comma1 = s.find(',');
    if (comma1 != std::string::npos) {
        rev.number = s.substr(0, comma1);
        trim(rev.number);
        std::string rest = s.substr(comma1 + 1);
        auto colon = rest.find(':');
        if (colon != std::string::npos) {
            rev.date   = rest.substr(0, colon);
            rev.remark = rest.substr(colon + 1);
            trim(rev.date);
            trim(rev.remark);
        } else {
            rev.date = rest;
            trim(rev.date);
        }
    } else {
        // Check if it's a bare ISO date: YYYY-MM-DD
        bool is_date = (s.size() == 10 &&
                        std::isdigit(static_cast<unsigned char>(s[0])) &&
                        std::isdigit(static_cast<unsigned char>(s[1])) &&
                        std::isdigit(static_cast<unsigned char>(s[2])) &&
                        std::isdigit(static_cast<unsigned char>(s[3])) &&
                        s[4] == '-' &&
                        std::isdigit(static_cast<unsigned char>(s[5])) &&
                        std::isdigit(static_cast<unsigned char>(s[6])) &&
                        s[7] == '-' &&
                        std::isdigit(static_cast<unsigned char>(s[8])) &&
                        std::isdigit(static_cast<unsigned char>(s[9])));
        if (is_date) {
            rev.date = s;
        } else {
            rev.number = s;
        }
    }
    return rev;
}

// ── Admonition label detection ────────────────────────────────────────────────

const char* const ADMONITION_LABELS[] = {
    "NOTE", "TIP", "WARNING", "IMPORTANT", "CAUTION", nullptr
};

std::optional<std::string> match_admonition_label(const std::string& line) {
    for (const char* const* lbl = ADMONITION_LABELS; *lbl; ++lbl) {
        std::string prefix = std::string(*lbl) + ":";
        if (line.size() > prefix.size() + 1 &&
            line.substr(0, prefix.size()) == prefix &&
            (line[prefix.size()] == ' ' || line[prefix.size()] == '\t')) {
            return std::string(*lbl);
        }
    }
    return std::nullopt;
}

} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
// Parser public API
// ═════════════════════════════════════════════════════════════════════════════

DocumentPtr Parser::parse(Reader& reader, const ParseOptions& opts) {
    auto doc = std::make_shared<Document>();

    // Apply pre-set options
    doc->set_safe_mode(opts.safe_mode);
    doc->set_doctype(opts.doctype);
    doc->set_backend(opts.backend);

    // Apply pre-set attributes
    for (const auto& [k, v] : opts.attributes) {
        doc->set_attr(k, v);
    }

    // Record source file and derive the base directory for include:: resolution
    const std::string& src_path = reader.source_path();
    doc->set_source_file(src_path);
    if (src_path != "<stdin>" && !src_path.empty()) {
        try {
            namespace fs = std::filesystem;
            fs::path p(src_path);
            std::string base = p.parent_path().string();
            if (!base.empty()) { doc->set_base_dir(base); }
        } catch (...) {}
    }

    // Parse the document header (title / author / revision + leading attrs)
    parse_document_header(reader, *doc);

    if (!opts.parse_header_only) {
        // Parse the document body
        parse_blocks(reader, *doc);
    }

    return doc;
}

DocumentPtr Parser::parse_string(const std::string&  content,
                                  const ParseOptions& opts,
                                  const std::string&  source_path) {
    Reader reader(content, source_path);
    return parse(reader, opts);
}

// ─────────────────────────────────────────────────────────────────────────────
// Document header
// ─────────────────────────────────────────────────────────────────────────────

void Parser::parse_document_header(Reader& reader, Document& doc) {
    reader.skip_blank_lines();
    if (!reader.has_more_lines()) { return; }

    // ── Document title ────────────────────────────────────────────────────────
    {
        auto peeked = reader.peek_line();
        if (!peeked) { return; }
        std::string line{*peeked};

        int lvl = section_level(line);
        bool setext_title = false;
        std::string title_line;

        if (lvl == 0) {
            // ATX-style: "= Title"
            reader.skip_line();
            title_line = line;
        } else {
            // Setext-style (two-line) title: text on line 1, ==... on line 2
            // The first line must look like title text: not a block attribute [...]
            // or attribute entry :name: or comment //.
            bool candidate_title = !line.empty() &&
                                   line[0] != '[' && line[0] != ':' &&
                                   line.compare(0, 2, "//") != 0;
            if (candidate_title) {
                auto next_lines = reader.peek_lines(2);
                if (next_lines.size() >= 2) {
                    std::string second{next_lines[1]};
                    if (!second.empty() &&
                        second.find_first_not_of('=') == std::string::npos &&
                        second.size() >= 2) {
                        setext_title = true;
                        reader.skip_line();
                        title_line = line;
                        reader.skip_line(); // consume the == line
                    }
                }
            }
        }

        if (lvl == 0 || setext_title) {
            std::string title = section_title_text(title_line);
            doc.set_doctitle(title);
            doc.header().has_header = true;

            // For doctype: manpage, parse "name(volnum)" from the title
            // e.g. "git-commit(1)" → manname="git-commit", manvolnum="1"
            // e.g. "analyze(nged)"  → manname="analyze",   manvolnum="nged"
            // Pattern: anything up to last '(', then alnum chars, then ')'
            if (doc.doctype() == "manpage") {
                auto paren_open  = title.rfind('(');
                if (paren_open != std::string::npos &&
                    title.back() == ')') {
                    std::string volnum = title.substr(paren_open + 1,
                                                      title.size() - paren_open - 2);
                    bool alnum_only = !volnum.empty();
                    for (char c2 : volnum) {
                        if (!std::isalnum(static_cast<unsigned char>(c2))) {
                            alnum_only = false; break;
                        }
                    }
                    if (alnum_only) {
                        doc.set_attr("manname",   title.substr(0, paren_open));
                        doc.set_attr("manvolnum", volnum);
                    }
                }
            }

            // Register in catalog
            std::string id_prefix = doc.attr("idprefix", "_");
            std::string id_sep    = doc.attr("idseparator", "_");
            std::string doc_id    = generate_id(title, id_prefix, id_sep);
            if (doc.id().empty()) { doc.set_id(doc_id); }

            // ── Author line ───────────────────────────────────────────────────
            reader.skip_blank_lines();
            if (!reader.has_more_lines()) { return; }
            {
                auto ap = reader.peek_line();
                if (ap) {
                    std::string aline{*ap};
                    if (looks_like_author_line(aline)) {
                        reader.skip_line();
                        auto authors = do_parse_author_line(aline);
                        for (auto& a : authors) { doc.add_author(a); }

                        // Set author-related attributes
                        if (!authors.empty()) {
                            const auto& a = authors[0];
                            doc.set_attr("author",          a.fullname());
                            doc.set_attr("authorinitials",  a.initials());
                            doc.set_attr("firstname",       a.firstname);
                            if (!a.middlename.empty())
                                doc.set_attr("middlename",  a.middlename);
                            if (!a.lastname.empty())
                                doc.set_attr("lastname",    a.lastname);
                            if (!a.email.empty())
                                doc.set_attr("email",       a.email);

                            // Build "authors" attribute  (all authors)
                            std::string all_authors;
                            for (std::size_t i = 0; i < authors.size(); ++i) {
                                if (i > 0) { all_authors += ", "; }
                                all_authors += authors[i].fullname();
                            }
                            doc.set_attr("authors", all_authors);
                        }

                        // ── Revision line ─────────────────────────────────────
                        if (reader.has_more_lines()) {
                            auto rp = reader.peek_line();
                            if (rp) {
                                std::string rline{*rp};
                                if (looks_like_revision_line(rline)) {
                                    reader.skip_line();
                                    RevisionInfo rev = do_parse_revision_line(rline);
                                    doc.set_revision(rev);
                                    if (!rev.number.empty())
                                        doc.set_attr("revnumber", rev.number);
                                    if (!rev.date.empty())
                                        doc.set_attr("revdate",   rev.date);
                                    if (!rev.remark.empty())
                                        doc.set_attr("revremark", rev.remark);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Leading attribute entries ─────────────────────────────────────────────
    reader.skip_blank_lines();
    while (reader.has_more_lines()) {
        auto peeked = reader.peek_line();
        if (!peeked) { break; }
        std::string line{*peeked};

        if (is_blank(line)) {
            reader.skip_line();
            continue;
        }
        // Also handle conditional directives in the header attribute block
        if (line.size() >= 7 && (line.substr(0, 7) == "ifdef::" ||
                                  line.substr(0, 8) == "ifndef::")) {
            reader.skip_line();
            handle_conditional(line, reader, &doc);
            continue;
        }
        if (line.size() >= 7 && line.substr(0, 7) == "endif::") {
            reader.skip_line();
            continue;
        }
        if (!parse_attribute_entry(reader, doc)) { break; }
    }

    // ── Post-header manpage title fix-up ──────────────────────────────────────
    if (doc.doctype() == "manpage" && !doc.has_attr("manname")) {
        const std::string& title = doc.doctitle();
        auto paren_open = title.rfind('(');
        if (paren_open != std::string::npos && title.back() == ')') {
            std::string volnum = title.substr(paren_open + 1,
                                              title.size() - paren_open - 2);
            bool alnum_only = !volnum.empty();
            for (char c2 : volnum) {
                if (!std::isalnum(static_cast<unsigned char>(c2))) {
                    alnum_only = false; break;
                }
            }
            if (alnum_only) {
                doc.set_attr("manname",   title.substr(0, paren_open));
                doc.set_attr("manvolnum", volnum);
            }
        }
    }
}

bool Parser::parse_attribute_entry(Reader& reader, Document& doc) {
    auto peeked = reader.peek_line();
    if (!peeked) { return false; }
    std::string line{*peeked};

    auto result = try_parse_attribute_entry(line);
    if (!result) { return false; }

    reader.skip_line();  // consume the first (and possibly only) attribute line

    auto& [name, value] = *result;

    // Handle trailing ' \' multi-line continuation (Bug #6 / P1#5)
    while (value.size() >= 2 &&
           value.back() == '\\' &&
           value[value.size() - 2] == ' ') {
        value.resize(value.size() - 2);  // strip trailing ' \'
        if (!reader.has_more_lines()) { break; }
        auto next_opt = reader.read_line();
        if (!next_opt) { break; }
        std::string cont = *next_opt;
        trim(cont);
        if (!value.empty()) { value += ' '; }
        value += cont;
    }
    trim(value);

    if (!name.empty() && name[0] == '!') {
        doc.remove_attr(name.substr(1));
    } else {
        doc.set_attr(name, value);
    }
    return true;
}

bool Parser::parse_attribute_entry(const std::string& line, Document& doc) {
    auto result = try_parse_attribute_entry(line);
    if (!result) { return false; }

    auto& [name, value] = *result;
    if (!name.empty() && name[0] == '!') {
        // Unset: :!name:
        doc.remove_attr(name.substr(1));
    } else {
        doc.set_attr(name, value);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Section title helpers
// ─────────────────────────────────────────────────────────────────────────────

int Parser::section_level(const std::string& line) {
    if (line.empty() || line[0] != '=') { return -1; }
    std::size_t i = 0;
    while (i < line.size() && line[i] == '=') { ++i; }
    if (i > 6) { return -1; }  // max 6 '='
    if (i >= line.size() || (line[i] != ' ' && line[i] != '\t')) { return -1; }
    return static_cast<int>(i) - 1;  // "=" → 0, "==" → 1, …
}

std::string Parser::section_title_text(const std::string& line) {
    if (line.empty() || line[0] != '=') { return line; }
    // Use the scanner's capture which already trims the setext marker.
    AqBlockScanResult r = aq_scan_block_line(line.c_str(), line.size());
    if (r.type == AQ_BT_SECTION_TITLE && r.caps[1].len > 0) {
        return std::string(line.data() + r.caps[1].off, r.caps[1].len);
    }
    // Setext-style or plain: skip leading '=' chars and whitespace,
    // then strip optional trailing '=' run.
    std::size_t i = 0;
    while (i < line.size() && line[i] == '=') { ++i; }
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) { ++i; }
    std::string text = line.substr(i);
    // Strip optional trailing ' ==...' setext marker
    std::size_t j = text.size();
    while (j > 0 && text[j-1] == '=') { --j; }
    while (j > 0 && (text[j-1] == ' ' || text[j-1] == '\t')) { --j; }
    if (j < text.size()) { text.resize(j); }
    return text;
}

// ─────────────────────────────────────────────────────────────────────────────
// Block-attribute accumulation
// ─────────────────────────────────────────────────────────────────────────────

void Parser::collect_block_attributes(
        Reader& reader,
        std::unordered_map<std::string, std::string>& pending_attrs,
        std::string& pending_title,
        std::string& pending_id)
{
    while (reader.has_more_lines()) {
        auto peeked = reader.peek_line();
        if (!peeked) { break; }
        std::string line{*peeked};

        if (is_blank(line)) { break; }

        if (is_block_title_line(line)) {
            reader.skip_line();
            pending_title = line.substr(1);  // strip leading '.'
            trim(pending_title);
            continue;
        }

        if (is_block_attribute_line(line)) {
            reader.skip_line();

            // Detect anchor [[id]] vs attribute list [...]
            if (line.size() >= 4 && line[1] == '[') {
                // [[id]] or [[id,reftext]]
                std::string inner = line.substr(2, line.size() - 4);
                auto comma = inner.find(',');
                if (comma != std::string::npos) {
                    pending_id = inner.substr(0, comma);
                    trim(pending_id);
                    // reftext stored as title if no explicit title yet
                    if (pending_title.empty()) {
                        pending_title = inner.substr(comma + 1);
                        trim(pending_title);
                    }
                } else {
                    pending_id = inner;
                    trim(pending_id);
                }
            } else {
                auto attrs = parse_attribute_list(line);
                for (auto& [k, v] : attrs) {
                    pending_attrs[k] = v;
                }
            }
            continue;
        }

        break;
    }
}

void Parser::apply_block_attributes(
        Block& block,
        std::unordered_map<std::string, std::string>& pending_attrs,
        const std::string& pending_title,
        const std::string& pending_id)
{
    if (!pending_id.empty())    { block.set_id(pending_id); }
    if (!pending_title.empty()) { block.set_title(pending_title); }
    for (auto& [k, v] : pending_attrs) {
        block.set_attr(k, v);
    }
    // style is the first positional attribute (key "1")
    auto sit = pending_attrs.find("1");
    if (sit != pending_attrs.end() && block.style().empty()) {
        block.set_style(sit->second);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Body block parsing
// ─────────────────────────────────────────────────────────────────────────────

void Parser::parse_blocks(Reader& reader, Block& parent) {
    while (reader.has_more_lines()) {
        parse_next_block(reader, parent);
    }
}

void Parser::parse_blocks_until(Reader& reader, Block& parent,
                                const std::string& terminator) {
    while (reader.has_more_lines()) {
        // Skip blank lines before checking for the closing delimiter.
        // Without this, a blank line separating the last block from the
        // closing delimiter would be consumed inside parse_next_block(),
        // causing the delimiter to be mistaken for a new block opening.
        reader.skip_blank_lines();
        if (!reader.has_more_lines()) { break; }
        auto peeked = reader.peek_line();
        if (peeked && std::string{*peeked} == terminator) {
            reader.skip_line();  // consume the closing delimiter
            return;
        }
        parse_next_block(reader, parent);
    }
    // EOF before finding terminator – treat as unclosed block (Ruby does the same)
    std::cerr << "asciiquack: WARNING: unclosed block: expected '" << terminator
              << "' before end of document\n";
}

bool Parser::parse_next_block(Reader& reader, Block& parent) {
    reader.skip_blank_lines();
    if (!reader.has_more_lines()) { return false; }

    // Collect block attributes and pending title / id
    std::unordered_map<std::string, std::string> pending_attrs;
    std::string pending_title;
    std::string pending_id;
    collect_block_attributes(reader, pending_attrs, pending_title, pending_id);

    reader.skip_blank_lines();
    if (!reader.has_more_lines()) { return false; }

    auto peeked = reader.peek_line();
    if (!peeked) { return false; }
    std::string line{*peeked};

    // ── Attribute entry ────────────────────────────────────────────────────────
    {
        Document* doc = parent.document();
        if (doc && parse_attribute_entry(reader, *doc)) {
            return true;
        }
    }

    // ── Conditional preprocessing (ifdef:: / ifndef:: / ifeval::) ──────────────
    if (line.size() >= 7 && (line.substr(0, 7) == "ifdef::" ||
                              line.substr(0, 8) == "ifndef::")) {
        reader.skip_line();
        handle_conditional(line, reader, parent.document());
        return true;
    }
    if (line.size() >= 7 && line.substr(0, 7) == "endif::") {
        reader.skip_line();  // consume orphaned endif (inside a true block)
        return true;
    }
    if (line.size() > 9 && line.substr(0, 9) == "ifeval::[") {
        // ifeval: evaluate expression; skip block if false
        std::string expr = line.substr(9);
        if (!expr.empty() && expr.back() == ']') { expr.pop_back(); }
        reader.skip_line();
        const Document* doc = parent.document();
        if (!doc || !evaluate_ifeval(expr, *doc)) {
            skip_conditional_block(reader);
        }
        return true;
    }

    // ── include:: directive ────────────────────────────────────────────────────
    if (line.size() > 9 && line.substr(0, 9) == "include::") {
        reader.skip_line();
        Document* doc_mut = parent.document();
        if (doc_mut) { handle_include(line, reader, *doc_mut); }
        return true;
    }

    // ── Comment block ─────────────────────────────────────────────────────────
    if (line == "////") {
        reader.skip_line();
        (void)reader.read_lines_until("////");  // consume until closing ////
        return true;
    }

    // ── Comment line ──────────────────────────────────────────────────────────
    if (is_line_comment(line)) {
        reader.skip_line();
        return true;
    }

    // ── Orphaned list-continuation marker '+' ─────────────────────────────────
    // A lone '+' at the section/document level (outside a list item's
    // collect_item loop) is an AsciiDoc list-continuation marker that has no
    // attached list item – typically because blank lines separate it from the
    // preceding dlist entry.  Consume it silently to prevent an infinite loop
    // where parse_paragraph immediately returns nullptr each iteration.
    if (line == "+") {
        reader.skip_line();
        return true;
    }

    // ── Thematic break ────────────────────────────────────────────────────────
    if (is_thematic_break(line)) {
        reader.skip_line();
        auto block = std::make_shared<Block>(BlockContext::ThematicBreak, &parent, ContentModel::Empty);
        parent.append(block);
        return true;
    }

    // ── Page break ────────────────────────────────────────────────────────────
    if (is_page_break(line)) {
        reader.skip_line();
        auto block = std::make_shared<Block>(BlockContext::PageBreak, &parent, ContentModel::Empty);
        parent.append(block);
        return true;
    }

    // ── Section title (may be floating if [discrete] pending) ─────────────────
    if (section_level(line) >= 0) {
        // Floating title: [discrete] turns a section heading into a free-standing <hN>
        auto style_it = pending_attrs.find("1");
        if (style_it != pending_attrs.end() &&
            to_lower(style_it->second) == "discrete") {
            reader.skip_line();  // consume the title line
            int   lv    = section_level(line);
            auto  title = section_title_text(line);

            auto block = std::make_shared<Block>(
                BlockContext::FloatingTitle, &parent, ContentModel::Empty);
            block->set_source(title);
            block->set_attr("level", std::to_string(lv));
            apply_block_attributes(*block, pending_attrs, pending_title, pending_id);
            parent.append(block);
            return true;
        }

        auto sect = parse_section(reader, parent, line, pending_attrs);
        if (sect) {
            apply_block_attributes(*sect, pending_attrs, pending_title, pending_id);
            parent.append(sect);
        }
        return true;
    }

    // ── Delimited block ───────────────────────────────────────────────────────
    if (auto di = classify_delimiter(line)) {
        reader.skip_line();  // consume opening delimiter

        BlockPtr block;

        // Dispatch based on context and pending style
        auto style_it  = pending_attrs.find("1");
        std::string style = (style_it != pending_attrs.end()) ? style_it->second : "";

        switch (di->context) {
            case BlockContext::Listing: {
                auto b = std::make_shared<Block>(BlockContext::Listing, &parent, ContentModel::Verbatim);
                b->set_lines(reader.read_lines_until(line));
                // Rebuild source from lines
                std::string src;
                for (auto& l : b->lines()) { src += l; src += '\n'; }
                if (!src.empty() && src.back() == '\n') { src.pop_back(); }
                b->set_source(src);
                block = b;
                break;
            }
            case BlockContext::Literal: {
                auto b = std::make_shared<Block>(BlockContext::Literal, &parent, ContentModel::Verbatim);
                b->set_lines(reader.read_lines_until(line));
                std::string src;
                for (auto& l : b->lines()) { src += l; src += '\n'; }
                if (!src.empty() && src.back() == '\n') { src.pop_back(); }
                b->set_source(src);
                block = b;
                break;
            }
            case BlockContext::Example: {
                if (to_lower(style) == "note"     || to_lower(style) == "tip" ||
                    to_lower(style) == "warning"   || to_lower(style) == "important" ||
                    to_lower(style) == "caution") {
                    block = parse_admonition_block(reader, parent, pending_attrs, line);
                } else {
                    auto b = std::make_shared<Block>(BlockContext::Example, &parent, ContentModel::Compound);
                    parse_blocks_until(reader, *b, line);
                    block = b;
                }
                break;
            }
            case BlockContext::Quote: {
                auto b = std::make_shared<Block>(
                    (to_lower(style) == "verse") ? BlockContext::Verse : BlockContext::Quote,
                    &parent, ContentModel::Compound);
                // For verse, the content is verbatim
                if (b->context() == BlockContext::Verse) {
                    b->set_lines(reader.read_lines_until(line));
                    std::string src;
                    for (auto& l : b->lines()) { src += l; src += '\n'; }
                    if (!src.empty() && src.back() == '\n') { src.pop_back(); }
                    b->set_source(src);
                } else {
                    parse_blocks_until(reader, *b, line);
                }
                block = b;
                break;
            }
            case BlockContext::Sidebar: {
                auto b = std::make_shared<Block>(BlockContext::Sidebar, &parent, ContentModel::Compound);
                parse_blocks_until(reader, *b, line);
                block = b;
                break;
            }
            case BlockContext::Pass: {
                auto b = std::make_shared<Block>(BlockContext::Pass, &parent, ContentModel::Raw);
                b->set_lines(reader.read_lines_until(line));
                std::string src;
                for (auto& l : b->lines()) { src += l; src += '\n'; }
                if (!src.empty() && src.back() == '\n') { src.pop_back(); }
                b->set_source(src);
                block = b;
                break;
            }
            case BlockContext::Open: {
                auto b = std::make_shared<Block>(BlockContext::Open, &parent, ContentModel::Compound);
                parse_blocks_until(reader, *b, line);
                block = b;
                break;
            }
            default:
                break;
        }

        if (block) {
            apply_block_attributes(*block, pending_attrs, pending_title, pending_id);
            parent.append(block);
        }
        return true;
    }

    // ── Table ─────────────────────────────────────────────────────────────────
    if (line.rfind("|===", 0) == 0 || line.rfind(",===", 0) == 0) {
        auto tbl = parse_table(reader, parent, pending_attrs);
        if (tbl) {
            apply_block_attributes(*tbl, pending_attrs, pending_title, pending_id);
            parent.append(tbl);
        }
        return true;
    }

    // ── Block image ────────────────────────────────────────────────────────────
    if (auto img = match_block_image(line)) {
        reader.skip_line();
        auto block = std::make_shared<Block>(BlockContext::Image, &parent, ContentModel::Empty);
        block->set_attr("target", img->target);
        block->set_attr("alt",    img->alt);
        for (auto& [k, v] : img->attrs) { block->set_attr(k, v); }
        apply_block_attributes(*block, pending_attrs, pending_title, pending_id);
        parent.append(block);
        return true;
    }

    // ── Video / audio block macro ──────────────────────────────────────────────
    if (auto mm = match_block_media(line)) {
        reader.skip_line();
        auto block = std::make_shared<Block>(mm->context, &parent, ContentModel::Empty);
        block->set_attr("target", mm->target);
        for (auto& [k, v] : mm->attrs) { block->set_attr(k, v); }
        apply_block_attributes(*block, pending_attrs, pending_title, pending_id);
        parent.append(block);
        return true;
    }

    // ── List item ─────────────────────────────────────────────────────────────
    if (auto lm = match_list_item(line)) {
        auto lst = parse_list(reader, parent, lm->type, line, pending_attrs);
        if (lst) {
            apply_block_attributes(*lst, pending_attrs, pending_title, pending_id);
            parent.append(lst);
        }
        return true;
    }

    // ── Multi-term dlist: extra term lines before the actual term:: line ───────
    // AsciiDoc allows multiple terms to share a single dlist body by listing
    // extra term lines (without '::') immediately before the 'term::' line.
    // e.g.:
    //   *-0*
    //   *-1*
    //   *-6*::
    //   Description body.
    // The extra-term lines don't have '::' so match_list_item() misses them.
    // Here we look ahead: if the current line looks like a potential extra term
    // (non-blank, non-delimiter, not a section title, not a list item) and the
    // NEXT non-blank line is a dlist item, treat the accumulated lines as extra
    // terms and synthesize them onto the dlist item's term.
    {
        // Collect candidate extra-term lines from the reader (we'll push them
        // back if this isn't actually a multi-term dlist situation).
        std::vector<std::string> extra_terms;
        bool is_multi_term = false;
        std::string dlist_line;

        // Consume the current line as a candidate extra term
        if (!is_blank(line) && !classify_delimiter(line) && section_level(line) < 0 &&
            line != "+" && !match_block_image(line)) {
            reader.skip_line();  // consume 'line'
            extra_terms.push_back(line);

            // Peek ahead: collect more non-blank lines until we hit a blank,
            // delimiter, section, or list item
            while (reader.has_more_lines()) {
                auto np = reader.peek_line();
                if (!np) { break; }
                std::string nl{*np};
                if (is_blank(nl) || classify_delimiter(nl) || section_level(nl) >= 0 ||
                    nl == "+" || match_block_image(nl)) { break; }
                // If this line IS a dlist item, we found the real term::
                if (auto nm = match_list_item(nl);
                    nm && nm->type == ListType::Description) {
                    dlist_line = nl;
                    is_multi_term = true;
                    // Don't consume the dlist line — parse_list() will skip it
                    // internally (it expects the first_line to still be in the
                    // reader so it can consume it via reader.skip_line()).
                    break;
                }
                // If it's a non-dlist list item, stop - not a multi-term dlist
                if (match_list_item(nl)) { break; }
                reader.skip_line();
                extra_terms.push_back(nl);
            }

            if (is_multi_term) {
                // Parse the actual dlist item from the dlist_line as normal,
                // then prepend the extra terms to the resulting item's term.
                auto lst = parse_list(reader, parent, ListType::Description,
                                      dlist_line, pending_attrs);
                if (lst && !lst->items().empty()) {
                    auto& first_item = lst->items().front();
                    // Prepend extra terms (joined with spaces) to the existing term
                    std::string extra_prefix;
                    for (const auto& et : extra_terms) {
                        if (!extra_prefix.empty()) { extra_prefix += ' '; }
                        extra_prefix += et;
                    }
                    if (!extra_prefix.empty()) {
                        std::string combined = extra_prefix;
                        if (!first_item->term().empty()) {
                            combined += ' ';
                            combined += first_item->term();
                        }
                        first_item->set_term(combined);
                    }
                    apply_block_attributes(*lst, pending_attrs, pending_title, pending_id);
                    parent.append(lst);
                }
                return true;
            } else {
                // Not a multi-term dlist – push the consumed lines back and fall
                // through to paragraph parsing below.
                reader.unshift_lines(extra_terms);
            }
        }
    }

    // ── Paragraph (including admonition paragraph) ─────────────────────────────
    {
        // If the pending style is an admonition keyword (note, tip, warning,
        // important, caution) applied to a paragraph block, convert the block
        // to a Simple admonition — matching asciidoctor's behaviour where
        // [NOTE]\ntext and [NOTE]\n====\ntext\n==== both produce admonitions.
        auto style_it = pending_attrs.find("1");
        bool pending_is_admonition = (style_it != pending_attrs.end()) &&
            (to_lower(style_it->second) == "note"      ||
             to_lower(style_it->second) == "tip"       ||
             to_lower(style_it->second) == "warning"   ||
             to_lower(style_it->second) == "important" ||
             to_lower(style_it->second) == "caution");

        auto block = parse_paragraph(reader, parent, pending_attrs);
        if (block) {
            if (pending_is_admonition &&
                    block->context() == BlockContext::Paragraph) {
                std::string lbl = style_it->second;
                block->set_context(BlockContext::Admonition);
                block->set_content_model(ContentModel::Simple);
                block->set_attr("name",    to_lower(lbl));
                block->set_attr("caption", lbl);
            }
            apply_block_attributes(*block, pending_attrs, pending_title, pending_id);
            parent.append(block);
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Section
// ─────────────────────────────────────────────────────────────────────────────

std::shared_ptr<Section> Parser::parse_section(
        Reader& reader, Block& parent,
        const std::string& title_line,
        std::unordered_map<std::string, std::string>& pending_attrs)
{
    reader.skip_line();  // consume the title line

    int level = section_level(title_line);
    if (level < 0) { return nullptr; }

    auto sect = std::make_shared<Section>(level, &parent);
    std::string title = section_title_text(title_line);
    sect->set_title(title);

    // Generate an id from the title
    const Document* doc = sect->document();
    std::string id_prefix = doc ? doc->attr("idprefix", "_") : "_";
    std::string id_sep    = doc ? doc->attr("idseparator", "_") : "_";
    sect->set_id(generate_id(title, id_prefix, id_sep));

    // Register in catalogue
    if (doc) {
        // doc is const, but we cast: the document owns the catalogue and this
        // is a legitimate mutation during parsing.
        const_cast<Document*>(doc)->register_ref(sect->id(), title);  // NOLINT
    }

    // ── Section numbering (P1 #4) ─────────────────────────────────────────────
    if (doc && doc->has_attr("sectnums") && level >= 1) {
        int secnumlevel = 3;  // default depth
        if (doc->has_attr("sectnumlevels")) {
            try { secnumlevel = std::stoi(doc->attr("sectnumlevels")); }
            catch (...) {}
        }
        if (level <= secnumlevel) {
            Document* doc_mut = const_cast<Document*>(doc);  // NOLINT
            doc_mut->reset_secnums_below(level);
            int num = doc_mut->increment_secnum(level);
            sect->set_numbered(true);
            sect->set_number(num);

            // Build the full number string: "1.", "1.2.", "1.2.3.", …
            std::string numstr;
            for (int l = 1; l <= level; ++l) {
                if (!numstr.empty()) { numstr += '.'; }
                numstr += std::to_string(doc_mut->secnum_at(l));
            }
            numstr += '.';
            sect->set_sectnum_string(numstr);
        }
    }

    // ── TOC entry registration (P1 #3) ───────────────────────────────────────
    if (doc && level >= 1) {
        TocEntry entry;
        entry.id      = sect->id();
        entry.title   = title;
        entry.level   = level;
        entry.sectnum = sect->sectnum_string();
        const_cast<Document*>(doc)->add_toc_entry(std::move(entry));  // NOLINT
    }

    // Determine the expected child section level
    int child_level = level + 1;

    // Consume blocks up to the next section at the same or higher level
    while (reader.has_more_lines()) {
        auto peeked = reader.peek_line();
        if (!peeked) { break; }
        std::string next_line{*peeked};

        if (is_blank(next_line)) { reader.skip_line(); continue; }

        int next_level = section_level(next_line);
        if (next_level >= 0 && next_level <= level) {
            // Next section at same or higher level – stop here.
            break;
        }
        if (next_level == child_level || next_level > child_level) {
            // Nested section: if deeper than the expected child level, warn.
            if (next_level > child_level) {
                std::cerr << "asciiquack: WARNING: section nesting skips a level"
                             " (level " << next_level << " inside level " << level
                          << "); treating as level " << child_level << " child\n";
            }
            std::unordered_map<std::string, std::string> sub_attrs;
            auto sub = parse_section(reader, *sect, next_line, sub_attrs);
            if (sub) { sect->append(sub); }
        } else {
            // Before calling parse_next_block, look ahead past any block-attribute /
            // anchor / blank lines to find the next meaningful content line.  If that
            // line turns out to be a section heading at the same or higher level
            // (smaller or equal level number), stop consuming here so the parent
            // section (or the document-level loop) can handle the whole group
            // (attribute + heading) together.  Without this check a block anchor
            // like "[[description]]" that immediately precedes a peer section would
            // cause parse_next_block to consume the anchor *and* then recursively
            // call parse_section with the wrong parent, incorrectly nesting the peer
            // section as a child.
            {
                bool peer_ahead = false;
                auto ahead = reader.peek_lines(64);
                for (std::string_view sv : ahead) {
                    std::string ln{sv};
                    if (is_blank(ln)) { continue; }
                    if (is_block_attribute_line(ln) || is_block_title_line(ln)) { continue; }
                    int ahead_lv = section_level(ln);
                    if (ahead_lv >= 0 && ahead_lv <= level) { peer_ahead = true; }
                    break;
                }
                if (peer_ahead) { break; }
            }
            parse_next_block(reader, *sect);
        }
    }

    // ── Special section names ([preface], [appendix], …) ─────────────────────
    // If the immediately preceding block-attribute line set a style that is a
    // recognised special section name, apply it to the section.
    {
        static const std::unordered_set<std::string> special_names = {
            "preface", "appendix", "abstract", "colophon",
            "glossary", "bibliography", "index"
        };
        auto sit = pending_attrs.find("1");
        if (sit != pending_attrs.end()) {
            std::string sname = sit->second;
            for (char& c : sname) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
            if (special_names.count(sname)) {
                sect->set_special(true);
                sect->set_sectname(sname);
                sect->set_style(sname);
            }
        }
    }

    (void)pending_attrs;  // will be applied by caller
    return sect;
}

// ─────────────────────────────────────────────────────────────────────────────
// Paragraph
// ─────────────────────────────────────────────────────────────────────────────

BlockPtr Parser::parse_paragraph(
        Reader& reader, Block& parent,
        std::unordered_map<std::string, std::string>& pending_attrs)
{
    (void)pending_attrs;

    // Collect lines until blank / section title / delimiter / list / image
    std::vector<std::string> lines;
    while (reader.has_more_lines()) {
        auto peeked = reader.peek_line();
        if (!peeked) { break; }
        std::string line{*peeked};

        if (is_blank(line)) { break; }
        if (is_line_comment(line)) { reader.skip_line(); break; }
        if (section_level(line) >= 0) { break; }
        if (classify_delimiter(line)) { break; }
        // A standalone '+' on its own line is an AsciiDoc list-continuation
        // marker.  It must NOT be consumed as paragraph text; the list-item
        // collector that invoked parse_next_block() will handle it.
        if (line == "+") { break; }
        if (match_block_image(line))  { break; }
        if (match_list_item(line))    { break; }
        if (line.rfind("|===", 0) == 0) { break; }

        reader.skip_line();
        lines.push_back(std::move(line));
    }

    if (lines.empty()) { return nullptr; }

    // Check for admonition paragraph: "NOTE: text"
    if (auto lbl = match_admonition_label(lines[0])) {
        auto block = std::make_shared<Block>(BlockContext::Admonition, &parent, ContentModel::Simple);
        block->set_attr("name", to_lower(*lbl));
        block->set_attr("caption", *lbl);
        // Strip the label prefix from the first line
        std::string first = lines[0].substr(lbl->size() + 1);
        trim(first);
        lines[0] = first;

        // Join lines
        std::string src;
        for (auto& l : lines) { src += l; src += ' '; }
        if (!src.empty() && src.back() == ' ') { src.pop_back(); }
        block->set_source(src);
        return block;
    }

    // Check for literal paragraph (leading whitespace)
    if (!lines[0].empty() && (lines[0][0] == ' ' || lines[0][0] == '\t')) {
        auto block = std::make_shared<Block>(BlockContext::Literal, &parent, ContentModel::Verbatim);
        block->set_lines(lines);
        std::string src;
        for (auto& l : lines) { src += l; src += '\n'; }
        if (!src.empty() && src.back() == '\n') { src.pop_back(); }
        block->set_source(src);
        return block;
    }

    // Normal paragraph: join lines with a single space
    std::string src;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            // Preserve hard-break marker
            if (!src.empty() && src.back() == '+') {
                src += '\n';
            } else {
                src += ' ';
            }
        }
        src += lines[i];
    }

    auto block = std::make_shared<Block>(BlockContext::Paragraph, &parent, ContentModel::Simple);
    block->set_source(src);
    return block;
}

// ─────────────────────────────────────────────────────────────────────────────
// Admonition block
// ─────────────────────────────────────────────────────────────────────────────

BlockPtr Parser::parse_admonition_block(
        Reader& reader, Block& parent,
        std::unordered_map<std::string, std::string>& pending_attrs,
        const std::string& delimiter)
{
    auto style_it = pending_attrs.find("1");
    std::string label = (style_it != pending_attrs.end()) ? style_it->second : "NOTE";

    auto block = std::make_shared<Block>(BlockContext::Admonition, &parent, ContentModel::Compound);
    block->set_attr("name",    to_lower(label));
    block->set_attr("caption", label);

    // Parse compound body blocks until the matching closing delimiter.
    parse_blocks_until(reader, *block, delimiter);
    return block;
}

// ─────────────────────────────────────────────────────────────────────────────
// List
// ─────────────────────────────────────────────────────────────────────────────

std::string Parser::list_marker(ListType lt, const std::string& line) {
    auto lm = match_list_item(line);
    if (lm && lm->type == lt) { return lm->marker; }
    return "";
}

int Parser::list_marker_level(ListType lt, const std::string& marker) {
    if (lt == ListType::Unordered) {
        return static_cast<int>(marker.size());  // * = 1, ** = 2, …
    }
    if (lt == ListType::Ordered) {
        return static_cast<int>(marker.size());  // . = 1, .. = 2, …
    }
    return 1;
}

std::shared_ptr<List> Parser::parse_list(
        Reader& reader, Block& parent,
        ListType list_type, const std::string& first_line,
        std::unordered_map<std::string, std::string>& pending_attrs)
{
    auto list = std::make_shared<List>(list_type, &parent);

    // ── Apply ordered-list style from block attribute ─────────────────────────
    if (list_type == ListType::Ordered) {
        auto sit = pending_attrs.find("1");
        if (sit != pending_attrs.end()) {
            std::string style_lc = sit->second;
            for (char& c : style_lc) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
            if      (style_lc == "arabic")     { list->set_ordered_style(OrderedListStyle::Arabic); }
            else if (style_lc == "loweralpha") { list->set_ordered_style(OrderedListStyle::LowerAlpha); }
            else if (style_lc == "upperalpha") { list->set_ordered_style(OrderedListStyle::UpperAlpha); }
            else if (style_lc == "lowerroman") { list->set_ordered_style(OrderedListStyle::LowerRoman); }
            else if (style_lc == "upperroman") { list->set_ordered_style(OrderedListStyle::UpperRoman); }
        }
        // start= attribute
        auto start_it = pending_attrs.find("start");
        if (start_it != pending_attrs.end()) {
            list->set_attr("start", start_it->second);
        }
    }
    (void)pending_attrs;

    // Determine the first marker so we can detect siblings vs sub-lists
    auto first_match = match_list_item(first_line);
    if (!first_match) { return nullptr; }
    std::string root_marker = first_match->marker;

    // Helper: collect lines for the item body (up to blank line or next
    // same-level item).  After the first line, a '+' on its own line
    // introduces compound block content that is parsed into the item.
    auto collect_item = [&](const std::string& text_line) -> std::shared_ptr<ListItem> {
        auto item = std::make_shared<ListItem>(list.get());
        item->set_marker(root_marker);

        // Source starts with the rest of the first line
        auto lm = match_list_item(text_line);
        if (!lm) { return nullptr; }

        if (list_type == ListType::Description) {
            // Use the term captured directly by the regex matcher rather than
            // searching for the marker in the line with find().  Using find()
            // would stop at the FIRST occurrence of '::' in the line, which
            // is wrong when the term itself contains '::' (e.g. attribute-style
            // terms like '*simulate::type=_TYPE_*::').  The regex correctly
            // identifies the term via the lazy match that leaves the trailing
            // '::' + end-of-line as the dlist separator.
            std::string term = lm->term;
            // term may be empty for '::-only' patterns; that is acceptable.
            trim(term);
            item->set_term(term);
        }

        // Collect simple text continuation lines
        std::string src = lm->text;
        // Track whether item->set_source() has already been called.  After the
        // first continuation block, any further plain-text lines must be emitted
        // as child Paragraph blocks rather than via a second set_source() call
        // (which would overwrite the first source).
        bool source_set = false;

        while (reader.has_more_lines()) {
            auto peeked = reader.peek_line();
            if (!peeked) { break; }
            std::string nxt{*peeked};

            if (is_blank(nxt)) { break; }

            // Sibling list item at the same level
            if (auto nm = match_list_item(nxt)) {
                if (nm->type == list_type && nm->marker == root_marker) { break; }
            }

            // List continuation '+' – what follows may be block content
            if (nxt == "+") {
                reader.skip_line();  // consume '+'
                // Set source so far on the item before parsing the attached block
                if (!src.empty()) {
                    item->set_source(src);
                    src.clear();
                    source_set = true;
                }
                // Skip blank lines before the next attached block
                reader.skip_blank_lines();
                if (!reader.has_more_lines()) { break; }

                // Peek next: if it looks like a block delimiter or starts with the
                // list marker, parse it as a block attached to this item.
                parse_next_block(reader, *item);

                // After the attached block there may be another '+' continuation
                reader.skip_blank_lines();
                if (!reader.has_more_lines()) { break; }
                auto after = reader.peek_line();
                if (!after) { break; }
                std::string after_str{*after};
                // If the next line is another same-level list item, stop here
                if (auto nm = match_list_item(after_str)) {
                    if (nm->type == list_type && nm->marker == root_marker) { break; }
                }
                continue;
            }

            // A block delimiter (|===, ----, ====, etc.) or a block attribute
            // line ([source,...]) terminates the item body paragraph.  Without
            // an explicit list-continuation '+', these blocks belong to the
            // parent context, not to this list item.
            if (classify_delimiter(nxt).has_value()) { break; }
            if (nxt.rfind("|===", 0) == 0 || nxt.rfind(",===", 0) == 0) { break; }
            if (!nxt.empty() && nxt[0] == '[' && nxt.back() == ']') { break; }
            // A block title line (starts with '.' followed by non-'. ') also
            // terminates the item body.  Block titles belong to the block that
            // follows them, not to the current list item's paragraph text.
            if (is_block_title_line(nxt)) { break; }

            // A same-type list item with a DEEPER marker creates a nested
            // (sub-)list attached to this item.  A SHALLOWER marker ends
            // this item so it can be processed by the parent list context.
            // A DIFFERENT-TYPE list item (e.g. an ordered list inside a dlist
            // body) is attached to this item as an implicit continuation block,
            // without requiring an explicit '+' marker.
            if (auto nm = match_list_item(nxt)) {
                if (nm->type == list_type) {
                    int root_level = list_marker_level(list_type, root_marker);
                    int next_level = list_marker_level(list_type, nm->marker);
                    if (next_level > root_level) {
                        // Set source so far before parsing the sub-list
                        if (!src.empty()) {
                            item->set_source(src);
                            src.clear();
                        }
                        std::unordered_map<std::string, std::string> sub_attrs;
                        auto sub_list = parse_list(reader, *item,
                                                   list_type, nxt, sub_attrs);
                        if (sub_list) { item->append(sub_list); }
                        // After attaching the sub-list, continue collecting only
                        // if the next line is another list item or an explicit '+'
                        // continuation.  A plain paragraph line after the sub-list
                        // belongs to the parent context, not this list item.
                        {
                            auto after = reader.peek_line();
                            if (!after) { break; }
                            std::string after_str{*after};
                            if (is_blank(after_str)) { break; }
                            if (after_str == "+") { continue; }
                            if (match_list_item(after_str)) { continue; }
                            break;
                        }
                    } else if (next_level < root_level) {
                        // Shallower item – return to parent
                        break;
                    }
                    // Same level handled above (root_marker check)
                } else {
                    // Different-type list (e.g. ordered list immediately after
                    // a dlist body, without an explicit '+').  Attach it as an
                    // implicit continuation block of this item.
                    if (!src.empty()) {
                        item->set_source(src);
                        src.clear();
                    }
                    parse_next_block(reader, *item);
                    reader.skip_blank_lines();
                    continue;
                }
            }

            // A DIFFERENT-TYPE list item (e.g. an ordered list inside a dlist
            // body) is attached to this item as an implicit continuation block,
            // without requiring an explicit '+' marker.
            if (auto nm = match_list_item(nxt)) {
                if (nm->type == list_type) {
                    int root_level = list_marker_level(list_type, root_marker);
                    int next_level = list_marker_level(list_type, nm->marker);
                    if (next_level > root_level) {
                        // Set source so far before parsing the sub-list
                        if (!src.empty()) {
                            item->set_source(src);
                            src.clear();
                            source_set = true;
                        }
                        std::unordered_map<std::string, std::string> sub_attrs;
                        auto sub_list = parse_list(reader, *item,
                                                   list_type, nxt, sub_attrs);
                        if (sub_list) { item->append(sub_list); }
                        continue;
                    } else if (next_level < root_level) {
                        // Shallower item – return to parent
                        break;
                    }
                    // Same level handled above (root_marker check)
                } else {
                    // Different-type list (e.g. ordered list immediately after
                    // a dlist body, without an explicit '+').  Attach it as an
                    // implicit continuation block of this item.
                    if (!src.empty()) {
                        item->set_source(src);
                        src.clear();
                        source_set = true;
                    }
                    parse_next_block(reader, *item);
                    reader.skip_blank_lines();
                    continue;
                }
            }

            // Otherwise it's simple text continuation
            reader.skip_line();
            if (source_set) {
                // Source was already committed; add subsequent text as a child Para
                auto child = std::make_shared<Block>(
                    BlockContext::Paragraph, item.get(), ContentModel::Simple);
                child->set_source(nxt);
                // Accumulate further lines into this child paragraph
                while (reader.has_more_lines()) {
                    auto np = reader.peek_line();
                    if (!np) { break; }
                    std::string nl{*np};
                    if (is_blank(nl) || nl == "+" || classify_delimiter(nl).has_value() ||
                        match_list_item(nl)) {
                        break;
                    }
                    reader.skip_line();
                    child->set_source(child->source() + ' ' + nl);
                }
                item->append(child);
            } else {
                if (!src.empty()) { src += ' '; }
                src += nxt;
            }
        }

        if (!src.empty()) {
            item->set_source(src);
        }
        return item;
    };

    // Process the first item (already peeked, now consume)
    reader.skip_line();
    auto first_item = collect_item(first_line);
    if (first_item) { list->add_item(first_item); }

    // Process subsequent items at the same level
    while (reader.has_more_lines()) {
        reader.skip_blank_lines();
        if (!reader.has_more_lines()) { break; }

        auto peeked = reader.peek_line();
        if (!peeked) { break; }
        std::string line{*peeked};

        auto lm = match_list_item(line);
        if (!lm) { break; }
        if (lm->type != list_type) { break; }

        // Deeper marker: attach as a sub-list to the last item (handles the
        // case where a blank line separates a parent item from its children --
        // AsciiDoc nesting is determined by marker depth, not blank lines).
        if (lm->marker != root_marker) {
            int root_level = list_marker_level(list_type, root_marker);
            int next_level = list_marker_level(list_type, lm->marker);
            if (next_level > root_level && !list->items().empty()) {
                // Attach sub-list to the last item in this list
                auto& last_item = list->items().back();
                std::unordered_map<std::string, std::string> sub_attrs;
                auto sub_list = parse_list(reader, *last_item,
                                           list_type, line, sub_attrs);
                if (sub_list) { last_item->append(sub_list); }
                continue;
            }
            break;
        }

        reader.skip_line();
        auto item = collect_item(line);
        if (item) { list->add_item(item); }
    }

    return list;
}

// ─────────────────────────────────────────────────────────────────────────────
// Table
// ─────────────────────────────────────────────────────────────────────────────

// ── Table cell spec helpers ──────────────────────────────────────────────────
//
// AsciiDoc table cell specifiers have the form:
//   [colspan][.rowspan]+[halign][valign][style]|content
//
// Examples:
//   |text            – plain cell
//   2+|text          – colspan 2
//   .3+|text         – rowspan 3
//   2.3+|text        – colspan 2, rowspan 3
//   ^|text           – centre-aligned
//   a|text           – AsciiDoc-style cell
//
// The cell spec is ONLY recognised when it occupies the entire space between
// the previous pipe (or start of line) and the current pipe with no other
// intervening content.  This prevents literal '+', digits, or style letters
// in cell content from being mistaken for specs.

namespace {  // anonymous – file-local helpers

// Describes one cell boundary found in a table row line.
struct CellBound {
    std::size_t spec_start;  ///< Start of the cell spec (== pipe_pos if no spec)
    std::size_t pipe_pos;    ///< Position of the '|' separator
    int colspan  = 1;
    int rowspan  = 1;
};

// Scan `line` for all unescaped '|' separators starting at `start`.
// For each '|', check if the region between the previous pipe's content-end
// and this pipe is a valid cell spec.  Populate and return a list of
// CellBound entries in left-to-right order.
static std::vector<CellBound> split_table_row_cells(
        const std::string& line, std::size_t start = 0)
{
    std::vector<CellBound> bounds;
    const std::size_t len = line.size();

    for (std::size_t i = start; i < len; ++i) {
        if (line[i] != '|') { continue; }

        CellBound b;
        b.pipe_pos = i;
        b.colspan  = 1;
        b.rowspan  = 1;

        // The "available region" for a cell spec begins just after the
        // previous pipe's '|' (or at `start` for the first cell).
        std::size_t prev_end = bounds.empty()
            ? start
            : bounds.back().pipe_pos + 1;

        // Scan backward from `i` looking for a cell spec.
        std::size_t j = i;

        // Optional style character [a d e m h l s]
        if (j > prev_end) {
            constexpr std::string_view STYLES = "ademhls";
            if (STYLES.find(line[j - 1]) != std::string_view::npos) { --j; }
        }
        // Optional horizontal-alignment character: < ^ >
        // May be preceded by '.' for vertical alignment: .< .^ .>
        if (j > prev_end &&
                (line[j-1] == '<' || line[j-1] == '^' || line[j-1] == '>')) {
            --j;
            if (j > prev_end && line[j-1] == '.') { --j; }
        }
        // Optional '+' (span marker – required when there are span digits)
        if (j > prev_end && line[j-1] == '+') {
            --j;
            // Digits immediately before '+' (rowspan or colspan)
            std::size_t num_end = j;
            while (j > prev_end &&
                   std::isdigit(static_cast<unsigned char>(line[j-1]))) { --j; }

            if (j < num_end) {
                // Digits found.  A preceding '.' means these are rowspan digits;
                // otherwise they are colspan digits.
                if (j > prev_end && line[j-1] == '.') {
                    b.rowspan = std::stoi(line.substr(j, num_end - j));
                    --j;  // consume '.'
                    // Colspan digits before '.'
                    std::size_t cs_end = j;
                    while (j > prev_end &&
                           std::isdigit(static_cast<unsigned char>(line[j-1]))) { --j; }
                    if (j < cs_end) {
                        b.colspan = std::stoi(line.substr(j, cs_end - j));
                    }
                } else {
                    b.colspan = std::stoi(line.substr(j, num_end - j));
                }
            }
        }

        // A cell spec is valid ONLY when `j` reaches all the way back to
        // `prev_end`, meaning the spec occupies the entire region between
        // the previous cell's content and this pipe.  If there is any
        // non-spec content in that region, reset to a plain pipe.
        if (j != prev_end) {
            j = i;
            b.colspan = 1;
            b.rowspan = 1;
        }
        b.spec_start = j;
        bounds.push_back(b);
    }

    return bounds;
}

// Returns true when `line` is a table cell line.
// A line is a cell line when it starts with '|' or starts with a valid
// cell spec (digits / '.' / '+' / alignment / style) that ends in '|'.
static bool is_table_cell_line(const std::string& line) {
    if (line.empty()) { return false; }
    if (line[0] == '|') { return true; }

    // Quick forward parse: look for [N][.M]+[align][style]| at the very start.
    std::size_t i = 0;
    bool has_spec = false;

    // Optional colspan digits
    if (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
        while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) { ++i; }
        has_spec = true;
    }
    // Optional '.' rowspan digits
    if (i < line.size() && line[i] == '.') {
        ++i;
        if (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
            while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) { ++i; }
            has_spec = true;
        } else {
            return false;
        }
    }
    // '+' required when span digits were found
    if (has_spec) {
        if (i < line.size() && line[i] == '+') { ++i; }
        else { return false; }
    } else {
        return false;
    }
    // Optional alignment
    if (i < line.size() && (line[i]=='<' || line[i]=='^' || line[i]=='>')) { ++i; }
    else if (i < line.size() && line[i] == '.' &&
             i + 1 < line.size() &&
             (line[i+1]=='<' || line[i+1]=='^' || line[i+1]=='>')) { i += 2; }
    // Optional style
    if (i < line.size()) {
        constexpr std::string_view STYLES = "ademhls";
        if (STYLES.find(line[i]) != std::string_view::npos) { ++i; }
    }
    return i < line.size() && line[i] == '|';
}

} // anonymous namespace

std::shared_ptr<Table> Parser::parse_table(
        Reader& reader, Block& parent,
        std::unordered_map<std::string, std::string>& pending_attrs)
{
    // Consume the opening |=== line
    auto opener = reader.read_line();
    if (!opener) { return nullptr; }

    auto tbl = std::make_shared<Table>(&parent);

    // Check for header / noheader option in pending_attrs.
    // AsciiDoc supports two equivalent syntaxes:
    //   [options="header"]  or  [%header]   → explicit header row
    //   [options="noheader"] or [%noheader] → no header row (also suppresses
    //                                         automatic header detection)
    bool has_header  = false;
    bool no_header   = false;
    auto opt_it = pending_attrs.find("options");
    if (opt_it != pending_attrs.end()) {
        if (opt_it->second.find("noheader") != std::string::npos) {
            no_header = true;
        } else if (opt_it->second.find("header") != std::string::npos) {
            has_header = true;
        }
    }
    // Also handle the [%noheader] / [%header] short-hand (stored as "1").
    auto role_it = pending_attrs.find("1");
    if (role_it != pending_attrs.end()) {
        if (role_it->second == "%noheader") {
            no_header = true;
        } else if (role_it->second == "%header") {
            has_header = true;
        }
    }
    // "cols" attribute for column specs
    auto cols_it = pending_attrs.find("cols");
    if (cols_it != pending_attrs.end()) {
        // Full parsing of column spec strings like:
        //   "1,2,3"          – proportional widths
        //   "1*,2*,3*"       – explicit proportional widths
        //   ">1,^2,<3"       – alignment prefix (right, center, left)
        //   "1h,2e,3"        – column style suffix (h=header, e=emphasis, etc.)
        //   "~"              – auto-width column (width=0 signals auto)
        std::string cols_str = cols_it->second;
        // Handle repeat notation "3*" at the top level (e.g., "3*,2") – multiply
        // a single spec by N.  Full repeat syntax is "N*specifier".
        std::istringstream ss(cols_str);
        std::string col;
        while (std::getline(ss, col, ',')) {
            trim(col);
            if (col.empty()) { continue; }

            ColumnSpec spec;

            // 1. Check for a repeat prefix "N*" (e.g., "3*>1m" or "3*")
            // Distinguish: "3*>" (repeat=3, align=right) vs "3*1" (width=3, proportional)
            int repeat = 1;
            std::size_t ci = 0;
            if (ci < col.size() && std::isdigit(static_cast<unsigned char>(col[ci]))) {
                // Find the position one past the leading digit run
                std::size_t star_pos = ci;
                while (star_pos < col.size() && std::isdigit(static_cast<unsigned char>(col[star_pos]))) {
                    ++star_pos;
                }
                // A repeat prefix looks like "N*" where what follows is either
                // end-of-string or a non-digit (alignment char, style, etc.).
                bool is_repeat_prefix =
                    star_pos < col.size() && col[star_pos] == '*' &&
                    (star_pos + 1 >= col.size() ||
                     !std::isdigit(static_cast<unsigned char>(col[star_pos + 1])));
                if (is_repeat_prefix) {
                    repeat = std::stoi(col.substr(0, star_pos));
                    col    = col.substr(star_pos + 1);
                    ci = 0;
                }
            }

            // 2. Alignment prefix character: '<' left, '^' center, '>' right
            if (ci < col.size()) {
                if (col[ci] == '<') { spec.halign = "left";   ++ci; }
                else if (col[ci] == '^') { spec.halign = "center"; ++ci; }
                else if (col[ci] == '>') { spec.halign = "right";  ++ci; }
            }

            // 3. Width (integer, or '~' for auto-width)
            if (ci < col.size() && col[ci] == '~') {
                spec.width = 0;  // 0 signals auto-width
                ++ci;
            } else if (ci < col.size() && std::isdigit(static_cast<unsigned char>(col[ci]))) {
                std::size_t pos = 0;
                try { spec.width = std::stoi(col.substr(ci), &pos); ci += pos; }
                catch (...) { spec.width = 1; }
            } else {
                spec.width = 1;
            }

            // 4. Optional '*' or '%' suffix after the width (already consumed
            //    if we parsed a digit; handle the case where col started with '~')
            if (ci < col.size() && (col[ci] == '*' || col[ci] == '%')) {
                ++ci;
            }

            // 5. Style character suffix: d(efault), s(trong/bold), e(mphasis),
            //    m(onospace), h(eader), l(iteral), a(sDoc)
            if (ci < col.size()) {
                static const std::string STYLE_CHARS = "dsemhla";
                if (STYLE_CHARS.find(col[ci]) != std::string::npos) {
                    spec.style = std::string(1, col[ci]);
                }
            }

            for (int r = 0; r < repeat; ++r) {
                tbl->add_column_spec(spec);
            }
        }
    }

    // Read rows until |===
    std::vector<std::string> raw_lines;
    while (reader.has_more_lines()) {
        auto ln = reader.read_line();
        if (!ln) { break; }
        if (*ln == "|===" || *ln == ",===") { break; }
        raw_lines.push_back(*ln);
    }

    // Auto-detect implicit header: a blank line immediately following the first
    // group of row lines signals that those rows are the header, matching the
    // upstream Asciidoctor Ruby converter behaviour.
    // [%noheader] / [options="noheader"] suppresses this auto-detection.
    if (!has_header && !no_header) {
        bool seen_row = false;
        for (const auto& ln : raw_lines) {
            if (is_table_cell_line(ln)) {
                seen_row = true;
            } else if (seen_row && is_blank(ln)) {
                has_header = true;
                break;
            }
        }
    }

    // Determine the expected number of columns.
    // If a [cols] attribute provided the column specs we already know ncols.
    // Otherwise infer from the first row that has content, counting the logical
    // column width (accounting for any colspan cell specs).
    int ncols = static_cast<int>(tbl->column_specs().size());
    if (ncols == 0) {
        for (const auto& ln : raw_lines) {
            if (is_blank(ln) || !is_table_cell_line(ln)) { continue; }
            auto bounds = split_table_row_cells(ln);
            for (const auto& b : bounds) { ncols += b.colspan; }
            break;
        }
        if (ncols == 0) { ncols = 1; }
    }

    // Build rows by accumulating cells across lines.
    //
    // AsciiDoc allows cells to be written one-per-line OR several-per-line;
    // a row is completed whenever `ncols` logical columns have been gathered
    // (each cell contributes cell.colspan logical columns).  The blank line
    // separates the header group from the body.
    //
    // Multi-line cell content is supported: lines that do not start a new
    // cell (i.e. are not cell-start lines) are appended to the source of
    // the last open cell, separated by a single space.  This handles the
    // common case where the DocBook-to-AsciiDoc XSL emits each entry on
    // its own line followed by continuation lines for block-level content
    // (inline images, extra paragraphs, etc.).
    //
    // Blank line semantics:
    //   - A blank line that arrives when there is no pending partial row
    //     (pending.empty()) ends the header group (header/body separator).
    //   - A blank line that arrives in the middle of a row (pending not
    //     empty, pending_cols < ncols) is treated as whitespace within the
    //     last open cell – it does NOT commit the row or end the header.
    //
    // Cell spec prefixes like "N+|" (colspan N), ".N+|" (rowspan N),
    // "N.M+|" (colspan N, rowspan M) are parsed and stored on the cell.
    //
    // Upstream reference:
    //   https://github.com/asciidoctor/asciidoctor (lib/asciidoctor/converter/*)
    std::vector<std::shared_ptr<TableCell>> pending;
    bool in_header_group = has_header;
    int pending_cols = 0;  // logical column count including colspans

    auto commit_row = [&]() {
        if (pending.empty()) { return; }
        TableRow row;
        for (auto& c : pending) { row.add_cell(c); }
        pending.clear();
        pending_cols = 0;
        if (in_header_group) {
            tbl->head_rows().push_back(std::move(row));
        } else {
            tbl->body_rows().push_back(std::move(row));
        }
    };

    for (const auto& row_line : raw_lines) {
        if (is_blank(row_line)) {
            if (pending.empty()) {
                // Blank line between complete rows: end the header group.
                in_header_group = false;
            }
            // Blank lines within a partial row are silently absorbed;
            // they do not commit the row or change the header group state.
            continue;
        }
        if (!is_table_cell_line(row_line)) {
            // Continuation line (not a cell-start line): append its text to
            // the last open cell so that multi-line cell content is preserved.
            if (!pending.empty()) {
                auto& last = pending.back();
                std::string src = last->source();
                if (!src.empty()) { src += ' '; }
                std::string cont = row_line;
                trim(cont);
                src += cont;
                last->set_source(src);
            }
            continue;
        }

        // Parse all cell boundaries on this line.
        auto bounds = split_table_row_cells(row_line);
        for (std::size_t bi = 0; bi < bounds.size(); ++bi) {
            const auto& b  = bounds[bi];
            // Cell content: from (pipe_pos + 1) to the start of the next
            // cell's spec (or end of string for the last cell).
            std::size_t content_start = b.pipe_pos + 1;
            std::size_t content_end   = (bi + 1 < bounds.size())
                ? bounds[bi + 1].spec_start
                : row_line.size();

            std::string cell_text = row_line.substr(content_start,
                                                    content_end - content_start);
            trim(cell_text);

            auto cell = std::make_shared<TableCell>();
            cell->set_source(cell_text);
            cell->set_colspan(b.colspan);
            cell->set_rowspan(b.rowspan);
            if (in_header_group) {
                cell->set_cell_context(TableCellContext::Head);
            }
            pending.push_back(std::move(cell));
            pending_cols += b.colspan;

            // Commit a full row as soon as we have ncols logical columns.
            if (pending_cols >= ncols) {
                commit_row();
            }
        }
    }
    // Flush any trailing partial row (malformed table or last row without
    // a trailing newline) into the body.
    if (!pending.empty()) {
        in_header_group = false;
        commit_row();
    }

    return tbl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Author / revision forwarding
// ─────────────────────────────────────────────────────────────────────────────

std::vector<AuthorInfo> Parser::parse_author_line(const std::string& line) {
    return do_parse_author_line(line);
}

RevisionInfo Parser::parse_revision_line(const std::string& line) {
    return do_parse_revision_line(line);
}

} // namespace asciiquack
