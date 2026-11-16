/*                      E X R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>

#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfStdIO.h>
#include <OpenEXR/ImfTestFile.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "icv_private.h"

namespace {
namespace exr = OPENEXR_IMF_NAMESPACE;
using ImagePtr = std::unique_ptr<icv_image_t, decltype(&icv_destroy)>;

// Bound seeks and report the actual size so OpenEXR can validate chunk tables.
class ExrInput final : public exr::IStream {
    const unsigned char *m_data;
    size_t m_size;
    size_t m_position = 0;
public:
    ExrInput(const unsigned char *data, size_t size)
      : exr::IStream("libicv memory input"), m_data(data), m_size(size)
    {
	if (!data || size > static_cast<size_t>((std::numeric_limits<int64_t>::max)()))
	    throw std::runtime_error("invalid EXR input size");
    }
    bool read(char *data, int count) override
    {
	if (count < 0 || static_cast<size_t>(count) > m_size - m_position)
	    throw std::runtime_error("truncated EXR input");
	std::memcpy(data, m_data + m_position, static_cast<size_t>(count));
	m_position += static_cast<size_t>(count);
	return m_position < m_size;
    }
    uint64_t tellg() override { return m_position; }
    void seekg(uint64_t position) override
    {
	if (position > m_size)
	    throw std::runtime_error("EXR seek exceeds input size");
	m_position = static_cast<size_t>(position);
    }
    int64_t size() override { return static_cast<int64_t>(m_size); }
};

struct ChannelSamples {
    std::vector<float> floating;
    std::vector<uint32_t> integer;
};

static icv_image_t *
decode_exr(const unsigned char *buffer, size_t size)
{
    ExrInput input(buffer, size);
    bool tiled = false, deep = false, multipart = false;
    if (!exr::isOpenExrFile(input, tiled, deep, multipart) || deep || multipart)
	throw std::runtime_error("libicv requires a flat, single-part EXR image");
    input.seekg(0);
    exr::InputFile file(input);
    const auto window = file.header().dataWindow();
    const int64_t width = static_cast<int64_t>(window.max.x) - window.min.x + 1;
    const int64_t height = static_cast<int64_t>(window.max.y) - window.min.y + 1;
    if (width <= 0 || height <= 0 || width > (std::numeric_limits<int>::max)() ||
	height > (std::numeric_limits<int>::max)())
	throw std::runtime_error("EXR data window exceeds supported image dimensions");

    const auto &channels = file.header().channels();
    const bool rgb = channels.findChannel("R") && channels.findChannel("G") && channels.findChannel("B");
    if (!rgb && !channels.findChannel("Y"))
	throw std::runtime_error("EXR requires RGB or Y color channels");
    const bool alpha = channels.findChannel("A") != nullptr;
    std::vector<const char *> names = rgb
	? std::vector<const char *>{"R", "G", "B"} : std::vector<const char *>{"Y"};
    if (alpha)
	names.push_back("A");
    size_t channel_count = 0;
    for (auto channel = channels.begin(); channel != channels.end(); ++channel)
	++channel_count;
    if (channel_count != names.size())
	throw std::runtime_error("auxiliary EXR channels cannot be represented by icv_image_t");
    const size_t w = static_cast<size_t>(width), h = static_cast<size_t>(height);
    if (h > (std::numeric_limits<size_t>::max)() / w / names.size() / sizeof(double))
	throw std::runtime_error("EXR image size overflows pixel storage");

    exr::FrameBuffer frame;
    std::vector<ChannelSamples> samples(names.size());
    for (size_t c = 0; c < names.size(); ++c) {
	const auto &channel = *channels.findChannel(names[c]);
	if (channel.xSampling != 1 || channel.ySampling != 1)
	    throw std::runtime_error("subsampled EXR channels require resampling before import");
	void *data;
	size_t stride;
	const auto type = channel.type == exr::UINT ? exr::UINT : exr::FLOAT;
	if (type == exr::UINT) {
	    samples[c].integer.resize(w * h);
	    data = samples[c].integer.data();
	    stride = sizeof(uint32_t);
	} else {
	    samples[c].floating.resize(w * h);
	    data = samples[c].floating.data();
	    stride = sizeof(float);
	}
	frame.insert(names[c], exr::Slice::Make(type, data, window, stride, w * stride));
    }
    file.setFrameBuffer(frame);
    file.readPixels(window.min.y, window.max.y);
    ImagePtr image(icv_create_with_channels(w, h, rgb ? ICV_COLOR_SPACE_RGB : ICV_COLOR_SPACE_GRAY,
	names.size()), icv_destroy);
    if (!image)
	throw std::bad_alloc();
    for (size_t y = 0; y < h; ++y) {
	for (size_t x = 0; x < w; ++x) {
	    for (size_t c = 0; c < names.size(); ++c) {
		const size_t source = y * w + x;
		const double value = samples[c].integer.empty()
		    ? static_cast<double>(samples[c].floating[source])
		    : static_cast<double>(samples[c].integer[source]);
		image->data[((h - 1 - y) * w + x) * names.size() + c] = value;
	    }
	}
    }
    // EXR samples are not restricted to display range; arithmetic must retain HDR.
    image->flags |= ICV_OPERATIONS_MODE;
    image->flags &= ~ICV_SANITIZED;
    return image.release();
}

static std::string
encode_exr(const icv_image_t *image)
{
    if (!ICV_IMAGE_IS_INITIALIZED(image) || !image->data || !image->width || !image->height)
	throw std::runtime_error("invalid image for EXR output");
    const bool rgb = image->color_space == ICV_COLOR_SPACE_RGB;
    const size_t colors = rgb ? 3 : 1;
    if ((!rgb && image->color_space != ICV_COLOR_SPACE_GRAY) ||
	image->channels != colors + (image->alpha_channel ? 1 : 0))
	throw std::runtime_error("EXR output requires RGB(A) or gray(A) data");
    if (image->width > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
	image->height > static_cast<size_t>((std::numeric_limits<int>::max)()) ||
	image->height > (std::numeric_limits<size_t>::max)() / image->width / image->channels / sizeof(float))
	throw std::runtime_error("EXR output dimensions exceed supported pixel storage");
    const size_t row_samples = image->width * image->channels;
    std::vector<float> pixels(row_samples * image->height);
    for (size_t y = 0; y < image->height; ++y) {
	for (size_t x = 0; x < row_samples; ++x) {
	    const double value = image->data[(image->height - 1 - y) * row_samples + x];
	    if (std::isfinite(value) && std::abs(value) > (std::numeric_limits<float>::max)())
		throw std::runtime_error("EXR output sample exceeds the FLOAT channel range");
	    pixels[y * row_samples + x] = static_cast<float>(value);
	}
    }
    exr::Header header(static_cast<int>(image->width), static_cast<int>(image->height));
    header.compression() = exr::ZIP_COMPRESSION;
    exr::FrameBuffer frame;
    const std::array<const char *, 4> rgb_names{{"R", "G", "B", "A"}};
    const std::array<const char *, 2> gray_names{{"Y", "A"}};
    for (size_t c = 0; c < image->channels; ++c) {
	const char *name = rgb ? rgb_names[c] : gray_names[c];
	header.channels().insert(name, exr::Channel(exr::FLOAT));
	frame.insert(name, exr::Slice::Make(exr::FLOAT, pixels.data() + c, header.dataWindow(),
	    image->channels * sizeof(float), row_samples * sizeof(float)));
    }
    exr::StdOSStream output;
    {
	exr::OutputFile file(output, header);
	file.setFrameBuffer(frame);
	file.writePixels(static_cast<int>(image->height));
    }
    return output.str();
}
} // namespace

icv_image_t *
exr_read_mem(const unsigned char *buffer, size_t size)
{
    try {
	return decode_exr(buffer, size);
    } catch (const std::exception &error) {
	bu_log("exr_read_mem: %s\n", error.what());
	return nullptr;
    }
}

icv_image_t *
exr_read(FILE *fp)
{
    try {
	// Buffering supports nonseekable stdin as well as the memory API.
	constexpr size_t IO_CHUNK_SIZE = 64 * 1024;
	std::array<unsigned char, IO_CHUNK_SIZE> chunk;
	std::vector<unsigned char> buffer;
	size_t count;
	while ((count = std::fread(chunk.data(), 1, chunk.size(), fp)) != 0)
	    buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + count);
	if (std::ferror(fp))
	    throw std::runtime_error("cannot read EXR input stream");
	return decode_exr(buffer.data(), buffer.size());
    } catch (const std::exception &error) {
	bu_log("exr_read: %s\n", error.what());
	return nullptr;
    }
}

int
exr_write_mem(icv_image_t *image, unsigned char **buffer, size_t *size)
{
    try {
	const std::string encoded = encode_exr(image);
	*buffer = static_cast<unsigned char *>(bu_malloc(encoded.size(), "EXR output buffer"));
	std::memcpy(*buffer, encoded.data(), encoded.size());
	*size = encoded.size();
	return BRLCAD_OK;
    } catch (const std::exception &error) {
	bu_log("exr_write_mem: %s\n", error.what());
	return BRLCAD_ERROR;
    }
}

int
exr_write(icv_image_t *image, FILE *fp)
{
    try {
	const std::string encoded = encode_exr(image);
	if (std::fwrite(encoded.data(), 1, encoded.size(), fp) != encoded.size() || std::fflush(fp) != 0)
	    throw std::runtime_error("cannot write EXR output stream");
	return BRLCAD_OK;
    } catch (const std::exception &error) {
	bu_log("exr_write: %s\n", error.what());
	return BRLCAD_ERROR;
    }
}
