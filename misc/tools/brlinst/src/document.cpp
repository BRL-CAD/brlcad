// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#include <algorithm>
#include <cstdint>
namespace installer {
std::string text_rtf(const std::string &text, const RtfOptions &options) {
    constexpr unsigned ascii_limit = 0x80;
    constexpr unsigned max_font_size_half_points = 32767;
    constexpr std::size_t max_font_family_length = 80;
    constexpr unsigned unicode_plane_size = 0x10000;
    constexpr unsigned high_surrogate_start = 0xd800;
    constexpr unsigned low_surrogate_start = 0xdc00;
    constexpr unsigned surrogate_mask = 0x3ff;
    const bool valid_font = !options.font_family.empty() &&
                            options.font_family.size() <= max_font_family_length &&
                            std::all_of(options.font_family.begin(), options.font_family.end(),
                                        [](unsigned char value) {
                                            return (value >= 'a' && value <= 'z') ||
                                                   (value >= 'A' && value <= 'Z') ||
                                                   (value >= '0' && value <= '9') || value == ' ' ||
                                                   value == '-';
                                        });
    if (!valid_font || !options.font_size_half_points ||
        options.font_size_half_points > max_font_size_half_points || !options.size_limit)
        throw Error("Invalid RTF options", ExitCode::arguments);
    (void)Json(text).dump(); // Reuse the JSON library's strict UTF-8 validation.
    std::string result = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 " + options.font_family +
                         ";}}\\f0\\fs" + std::to_string(options.font_size_half_points) + " ";
    auto emit = [&](unsigned codepoint) {
        if (codepoint == '\r')
            return;
        if (codepoint == '\n')
            result += "\\par\n";
        else if (codepoint == '\t')
            result += "\\tab ";
        else if (codepoint == '[')
            result += "\\'5b";
        else if (codepoint == ']')
            result += "\\'5d";
        else if (codepoint == '\\' || codepoint == '{' || codepoint == '}') {
            result += '\\';
            result += static_cast<char>(codepoint);
        } else if (codepoint < ascii_limit)
            result += static_cast<char>(codepoint);
        else
            result += "\\u" + std::to_string(static_cast<std::int16_t>(codepoint)) + "?";
    };
    for (std::size_t index = 0; index < text.size();) {
        unsigned codepoint = static_cast<unsigned char>(text[index++]);
        if (codepoint >= ascii_limit) {
            unsigned continuation_count = codepoint < 0xe0 ? 1 : codepoint < 0xf0 ? 2 : 3;
            codepoint &= (1U << (6 - continuation_count)) - 1;
            while (continuation_count--) {
                if (index == text.size())
                    throw Error("Invalid UTF-8 document");
                codepoint = (codepoint << 6) | (static_cast<unsigned char>(text[index++]) & 0x3f);
            }
        }
        if (codepoint >= unicode_plane_size) {
            codepoint -= unicode_plane_size;
            emit(high_surrogate_start + (codepoint >> 10));
            emit(low_surrogate_start + (codepoint & surrogate_mask));
        } else {
            emit(codepoint);
        }
    }
    result += '}';
    if (result.size() > options.size_limit)
        throw Error("RTF document exceeds supported size");
    return result;
}
} // namespace installer
