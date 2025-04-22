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

#include "common.h"

#include <string>
#include <vector>

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

// --- Constants ---
namespace constants
{
    // Standard luminance weights for RGB to Grayscale conversion (Rec. 601 standard)
    // These weights are used when converting a color image to a single grayscale value per pixel.
    const float GRAYSCALE_WEIGHT_R = 0.299f;
    const float GRAYSCALE_WEIGHT_G = 0.587f;
    const float GRAYSCALE_WEIGHT_B = 0.114f;
}
// --- End Constants ---

// Information about a block of pixels
struct PixelInfo
{
    uint64_t brightness = 0;                 // Grayscale brightness value of the pixel
    std::vector<uint64_t> color = {0, 0, 0}; // RGB color values of the pixel
};

// Calculate relevant information (brightness, color, edge_magnitude) for a single pixel
// This function processes one pixel at the given (x, y) coordinates
PixelInfo getPixelInfo(const icv_image_t *img, size_t x, size_t y, const AsciiArtParams &params)
{
    PixelInfo info; // Create a struct to hold pixel information

    // Initialize color vector with zeros
    info.color = {0, 0, 0};

    // --- Bounds Check ---
    // Basic bounds check to ensure coordinates are within image dimensions
    // Although calling code should ideally provide valid coordinates, this adds safety
    if (x >= img->width || y >= img->height)
    {
	// If out of bounds, return default (zero-initialized) info
	return info;
    }

    // Calculate the starting index of the pixel's data in the image vector
    int pixel_index = (y * img->width + x) * img->channels;

    // Get the color components based on the number of channels
    // Safely access channels if they exist
    uint8_t r = (img->channels >= 1) ? img->data[pixel_index]*255.0 : 0;
    uint8_t g = (img->channels >= 2) ? img->data[pixel_index + 1]*255.0 : 0;
    uint8_t b = (img->channels >= 3) ? img->data[pixel_index + 2]*255.0 : 0;


    // Calculate grayscale brightness using standard luminance weights
    // These weights are defined as constants in image.cpp (conceptually)
    uint64_t gray = static_cast<uint64_t>(
	    constants::GRAYSCALE_WEIGHT_R * r +
	    constants::GRAYSCALE_WEIGHT_G * g +
	    constants::GRAYSCALE_WEIGHT_B * b
	    );


    // Use the calculated brightness value
    info.brightness = gray;

    // If color output is enabled and the image has enough channels (at least 3 for RGB)
    if (params.color && img->channels >= 3)
    {
	// Store the pixel's color values directly
	info.color[0] = r;
	info.color[1] = g;
	info.color[2] = b;
    }

    // The struct now holds info for this specific pixel
    return info;
}


// Select an ASCII character based on block brightness
inline char selectAsciiChar(const PixelInfo &pixel_info, const AsciiArtParams &params)
{
    uint64_t value; // Value used for character selection (brightness or edge magnitude)

    // Use brightness
    uint64_t brightness = pixel_info.brightness;
    // Apply brightness boost and clamp between 0 and 255
    float boosted_brightness = static_cast<float>(brightness) * params.brightness_boost;
    value = std::min(static_cast<uint64_t>(std::max(boosted_brightness, 0.0f)), static_cast<uint64_t>(255));

    // Handle the case where the value is 0. Map to the first character unless inverted.
    // The first character is typically space for brightness.
    if (value == 0 && !params.invert_color)
    {
	return params.ascii_chars.front(); // Return the first character (lowest brightness)
    }
    // Handle the case where the value is 0 and inverted. Map to the last character.
    else if (value == 0 && params.invert_color)
    {
	return params.ascii_chars.back(); // Return the last character (highest brightness in inverted scale)
    }

    // Map the value (0-255) to an index in the ASCII character set
    // The index is proportional to the value relative to the 0-256 range
    size_t char_index = (value * params.ascii_chars.size()) / 256;

    // Ensure the calculated index is within the bounds of the character set vector
    char_index = std::min(char_index, params.ascii_chars.size() - 1);

    // Return the character at the selected index
    // If invert_color is true, select from the end of the character set
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
	    // Get the information for the current pixel
	    // Pass the edge magnitudes pointer
	    PixelInfo pixel_info = getPixelInfo(img, x, y, params);

	    // Select the ASCII character corresponding to this pixel's info
	    char ascii_char = selectAsciiChar(pixel_info, params);

	    if (use_color)
	    {
		// Add ANSI color escape codes
		uint8_t r = pixel_info.color[0];
		uint8_t g = pixel_info.color[1];
		uint8_t b = pixel_info.color[2];

		// Append ANSI color escape code for 24-bit color (RGB)
		// Format: \033[38;2;R;G;Bm
		ascii_text += "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
		// Append the selected ASCII character
		ascii_text += ascii_char;
	    }
	    else
	    {
		// If color is not enabled, just append the ASCII character
		ascii_text += ascii_char;
	    }
	}

	// --- Color Reset and Newline ---
	// Reset color at the end of each line to prevent bleeding into the next line or prompt
	if (use_color)
	{
	    ascii_text += "\033[0m"; // ANSI reset code
	}

	// Add a newline character at the end of each row to move to the next line of ASCII art
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
