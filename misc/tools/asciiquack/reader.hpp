/// @file reader.hpp
/// @brief Line-by-line reader for AsciiDoc source text.
///
/// The Reader mirrors the design of the Ruby Asciidoctor Reader:
///   * Lines are stored in *reverse* order so that read_line() / peek_line()
///     are O(1) pop/back operations on the vector.
///   * unshift_line() pushes a line back (cheap prepend).
///   * The cursor tracks the current source file and line number for
///     diagnostics.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace asciiquack {

// ─────────────────────────────────────────────────────────────────────────────
// Cursor  –  position information for error messages
// ─────────────────────────────────────────────────────────────────────────────

struct Cursor {
    std::string path;     ///< source file path (or "<stdin>")
    int         lineno;   ///< 1-based line number

    [[nodiscard]] std::string line_info() const {
        return path + ": line " + std::to_string(lineno);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// SourceLine  –  a single line with its provenance
// ─────────────────────────────────────────────────────────────────────────────

struct SourceLine {
    std::string text;
    std::string file;
    int         lineno = 1;
};

// ─────────────────────────────────────────────────────────────────────────────
// Reader
// ─────────────────────────────────────────────────────────────────────────────

/// Reads lines from an AsciiDoc document, supporting lookahead, push-back,
/// and blank-line skipping.
class Reader {
public:
    /// Construct from full document text (may contain CRLF or LF line endings).
    explicit Reader(std::string_view content,
                    std::string      source_path = "<stdin>")
        : source_path_(std::move(source_path)) {
        init_from_content(content);
    }

    // ── Capacity ──────────────────────────────────────────────────────────────

    [[nodiscard]] bool has_more_lines() const noexcept { return !lines_.empty(); }
    [[nodiscard]] bool empty()          const noexcept { return lines_.empty(); }
    [[nodiscard]] int  lineno()         const noexcept { return lineno_; }
    [[nodiscard]] const std::string& source_path() const noexcept { return source_path_; }

    [[nodiscard]] Cursor cursor() const { return {source_path_, lineno_}; }

    // ── Core operations ───────────────────────────────────────────────────────

    /// Return the next line without consuming it.  Returns nullopt at EOF.
    [[nodiscard]] std::optional<std::string_view> peek_line() const noexcept {
        if (lines_.empty()) return std::nullopt;
        return std::string_view{lines_.back().text};
    }

    /// Advance past the next line, discarding its content.
    /// Use this instead of read_line() when the content is not needed.
    void skip_line() noexcept {
        if (!lines_.empty()) {
            lineno_ = lines_.back().lineno + 1;
            lines_.pop_back();
        }
    }

    /// Consume and return the next line.  Returns nullopt at EOF.
    [[nodiscard]] std::optional<std::string> read_line() {
        if (lines_.empty()) return std::nullopt;
        SourceLine sl = std::move(lines_.back());
        lines_.pop_back();
        lineno_ = sl.lineno + 1;
        return std::move(sl.text);
    }

    /// Push a single line back onto the front of the stream.
    void unshift_line(std::string line) {
        int ln = (lineno_ > 1) ? (lineno_ - 1) : 1;
        lines_.push_back(SourceLine{std::move(line), source_path_, ln});
        if (lineno_ > 1) { --lineno_; }
    }

    /// Push back multiple lines (element 0 will be the next line read).
    void unshift_lines(std::vector<std::string> lines_to_restore) {
        // Push in reverse order so that element 0 ends up at back.
        for (auto it = lines_to_restore.rbegin();
             it != lines_to_restore.rend(); ++it) {
            unshift_line(std::move(*it));
        }
    }

    // ── Skipping ──────────────────────────────────────────────────────────────

    /// Skip blank or whitespace-only lines.  Returns the number of lines skipped.
    int skip_blank_lines() noexcept {
        int skipped = 0;
        while (!lines_.empty()) {
            const std::string& top = lines_.back().text;
            if (!top.empty() && top.find_first_not_of(" \t") != std::string::npos) {
                break;
            }
            lineno_ = lines_.back().lineno + 1;
            lines_.pop_back();
            ++skipped;
        }
        return skipped;
    }

    /// Skip single-line comments (// …).  Block comments (////) are *not* skipped
    /// here; the parser handles those explicitly.
    void skip_comment_lines() noexcept {
        while (!lines_.empty()) {
            const std::string& top = lines_.back().text;
            if (top.size() >= 2 && top[0] == '/' && top[1] == '/' &&
                (top.size() == 2 || top[2] != '/')) {
                lineno_ = lines_.back().lineno + 1;
                lines_.pop_back();
            } else {
                break;
            }
        }
    }

    // ── Bulk reading ──────────────────────────────────────────────────────────

    /// Read lines until @p terminator is encountered (consuming it), or EOF.
    /// The terminator line itself is not included in the result.
    [[nodiscard]] std::vector<std::string> read_lines_until(std::string_view terminator) {
        std::vector<std::string> result;
        while (!lines_.empty()) {
            auto line = read_line();
            if (!line) { break; }
            if (*line == terminator) { break; }
            result.push_back(std::move(*line));
        }
        return result;
    }

    /// Read all remaining lines without consuming them (for lookahead).
    [[nodiscard]] std::vector<std::string_view> peek_lines(std::size_t count) const {
        std::vector<std::string_view> result;
        std::size_t n = std::min(count, lines_.size());
        for (std::size_t i = 0; i < n; ++i) {
            result.emplace_back(lines_[lines_.size() - 1 - i].text);
        }
        return result;
    }

private:
    /// Split content into SourceLine objects (reversed for stack efficiency).
    void init_from_content(std::string_view content) {
        std::vector<SourceLine> forward;
        std::string line;
        int ln = 1;
        line.reserve(128);

        for (char c : content) {
            if (c == '\n') {
                // Strip trailing CR (for CRLF files on any platform)
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                forward.push_back(SourceLine{std::move(line), source_path_, ln++});
                line.clear();
                line.reserve(128);
            } else {
                line += c;
            }
        }

        // Final line (no trailing newline)
        if (!line.empty()) {
            if (line.back() == '\r') { line.pop_back(); }
            forward.push_back(SourceLine{std::move(line), source_path_, ln});
        }

        // Store reversed so back() == next line
        lines_.reserve(forward.size());
        for (auto it = forward.rbegin(); it != forward.rend(); ++it) {
            lines_.push_back(std::move(*it));
        }

        lineno_ = 1;
    }

    std::string              source_path_;
    int                      lineno_ = 1;
    std::vector<SourceLine>  lines_;  ///< stored in *reverse* order
};

} // namespace asciiquack
