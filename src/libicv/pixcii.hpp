// MIT License
//
// Copyright (c) 2025 Ashish
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// https://github.com/ashish0kumar/pixcii
//
// Modified to use BRL-CAD data types

#pragma once

#include <string>
#include <vector>

#include "common.h"
#include "icv.h"

namespace pixcii {

// Parameters for ASCII art generation
struct AsciiArtParams
{
    std::string ascii_chars = " .:-=+*#%@";
    bool color = false;
    bool invert_color = false;
    float brightness_boost = 1.0f;
};

// Generate ASCII art as text
std::string generateAsciiText(const icv_image_t *i, const AsciiArtParams &params);

#ifdef PIXCII_IMPLEMENTATION

// Information about a block of pixels
struct BlockInfo
{
    uint64_t sum_brightness = 0;
    float sum_mag = 0.0f;
    std::vector<uint64_t> sum_color = {0, 0, 0};
    int pixel_count = 0;
};

// Calculate block information for ASCII character selection
inline BlockInfo calculateBlockInfo(const icv_image_t *img, int x, int y, const AsciiArtParams &params)
{
    BlockInfo info;

    // Initialize sum_color with 3 zeros
    info.sum_color = {0, 0, 0};

    size_t ix = x;
    size_t iy = y;

    if (ix >= img->width || iy >= img->height) {
        return info;
    }

    size_t pixel_index = (iy * img->width + ix) * img->channels;
    if (pixel_index + 2 >= img->width * img->height * img->channels)
    {
        return info;
    }

    uint8_t r = lrint(img->data[pixel_index]*255.0);
    uint8_t g = lrint(img->data[pixel_index+1]*255.0);
    uint8_t b = lrint(img->data[pixel_index+2]*255.0);

    uint64_t gray = static_cast<uint64_t>(0.3f * r + 0.59f * g + 0.11f * b);
    info.sum_brightness += gray;

    if (params.color)
    {
        info.sum_color[0] += r;
        info.sum_color[1] += g;
        info.sum_color[2] += b;
    }

    info.pixel_count += 1;

    return info;
}

// Select an ASCII character based on block brightness
inline char selectAsciiChar(const BlockInfo &block_info, const AsciiArtParams &params)
{
    if (block_info.pixel_count == 0)
    {
        return ' ';
    }

    uint64_t value;

    // Use brightness
    uint64_t avg_brightness = block_info.sum_brightness / block_info.pixel_count;
    float boosted_brightness = static_cast<float>(avg_brightness) * params.brightness_boost;
    value = std::min(static_cast<uint64_t>(std::max(boosted_brightness, 0.0f)), static_cast<uint64_t>(255));

    if (value == 0)
    {
        return ' ';
    }

    size_t char_index = (value * params.ascii_chars.size()) / 256;
    char_index = std::min(char_index, params.ascii_chars.size() - 1);

    return params.ascii_chars[params.invert_color ? params.ascii_chars.size() - 1 - char_index : char_index];
}

// Generate ASCII art as text
std::string generateAsciiText(const icv_image_t *i, const AsciiArtParams &params)
{
    if (!i)
	return std::string("");

    std::string ascii_text;
    bool use_color = params.color && i->channels >= 3;
    const icv_image_t *img = i;

    for (size_t y = img->height-1; y != 0; y--)
    {
        for (size_t x = 0; x < img->width; x++)
        {
            BlockInfo block_info = calculateBlockInfo(img, x, y, params);
            char ascii_char = selectAsciiChar(block_info, params);

            if (use_color)
            {
                // Add ANSI color escape codes
                uint8_t r = block_info.sum_color[0] / block_info.pixel_count;
                uint8_t g = block_info.sum_color[1] / block_info.pixel_count;
                uint8_t b = block_info.sum_color[2] / block_info.pixel_count;

                // ANSI color escape code for RGB
                ascii_text += "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
                ascii_text += ascii_char;
            }
            else
            {
                ascii_text += ascii_char;
            }
        }

        // Reset color at the end of each line
        if (use_color)
        {
            ascii_text += "\033[0m";
        }

        ascii_text += '\n';
    }

    return ascii_text;
}

#endif

} // namespace pixcii


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
