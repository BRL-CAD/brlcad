/*                      E X R _ T E S T . C P P
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

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Imath/half.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfDeepFrameBuffer.h>
#include <OpenEXR/ImfDeepScanLineOutputFile.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfMultiPartOutputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfOutputPart.h>
#include <OpenEXR/ImfPartType.h>
#include <OpenEXR/ImfTileDescription.h>
#include <OpenEXR/ImfTiledOutputFile.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "icv.h"
#include "vmath.h"

namespace exr = OPENEXR_IMF_NAMESPACE;
using ImagePtr = std::unique_ptr<icv_image_t, decltype(&icv_destroy)>;
static int failures = 0;

static void
check(bool condition, const char *message)
{
    if (!condition) {
	bu_log("EXR check failed: %s\n", message);
	++failures;
    }
}

static void
check_samples(const icv_image_t *image, const std::vector<double> &values)
{
    check(image && image->data, "image decoded");
    if (!image || !image->data)
	return;
    check(image->width * image->height * image->channels == values.size(), "sample count");
    if (image->width * image->height * image->channels != values.size())
	return;
    for (size_t i = 0; i < values.size(); ++i)
	check(NEAR_EQUAL(image->data[i], values[i], 1.0e-6), "sample value and row order");
}

static void
roundtrip(const std::string &path, const std::vector<const char *> &names, exr::PixelType type, bool tiled)
{
    constexpr size_t WIDTH = 4, HEIGHT = 2;
    const float color_samples[] = {-2.5f, 0.25f, 8.0f, 65504.0f};
    const bool gray = std::string(names[0]) == "Y";
    const bool alpha = std::string(names.back()) == "A";
    std::vector<float> values(WIDTH * HEIGHT * names.size());
    std::vector<uint32_t> integers(values.size());
    std::vector<IMATH_NAMESPACE::half> halves(values.size());
    std::vector<double> expected(values.size());
    for (size_t y = 0; y < HEIGHT; ++y) {
	for (size_t x = 0; x < WIDTH; ++x) {
	    for (size_t c = 0; c < names.size(); ++c) {
		const size_t index = (y * WIDTH + x) * names.size() + c;
		const size_t destination = ((HEIGHT - 1 - y) * WIDTH + x) * names.size() + c;
		values[index] = alpha && c == names.size() - 1 ? static_cast<float>(x % 3) * 0.5f
		    : color_samples[(x + y + c) % 4];
		halves[index] = IMATH_NAMESPACE::half(values[index]);
		integers[index] = 4000000001u + static_cast<uint32_t>(index);
		expected[destination] = type == exr::UINT ? static_cast<double>(integers[index])
		    : static_cast<double>(values[index]);
	    }
	}
    }
    exr::Header header(WIDTH, HEIGHT);
    // Both negative origins and a data window outside the display window are legal.
    header.dataWindow() = IMATH_NAMESPACE::Box2i(IMATH_NAMESPACE::V2i(-9, 13), IMATH_NAMESPACE::V2i(-6, 14));
    exr::FrameBuffer frame;
    for (size_t c = 0; c < names.size(); ++c) {
	header.channels().insert(names[c], exr::Channel(type));
	const void *data = values.data() + c;
	size_t component_size = sizeof(float);
	if (type == exr::UINT) {
	    data = integers.data() + c;
	    component_size = sizeof(uint32_t);
	} else if (type == exr::HALF) {
	    data = halves.data() + c;
	    component_size = sizeof(IMATH_NAMESPACE::half);
	}
	frame.insert(names[c], exr::Slice::Make(type, data, header.dataWindow(),
	    names.size() * component_size, WIDTH * names.size() * component_size));
    }
    if (tiled) {
	header.setTileDescription(exr::TileDescription(2, 2, exr::ONE_LEVEL));
	exr::TiledOutputFile file(path.c_str(), header);
	file.setFrameBuffer(frame);
	file.writeTiles(0, file.numXTiles() - 1, 0, file.numYTiles() - 1);
    } else {
	exr::OutputFile file(path.c_str(), header);
	file.setFrameBuffer(frame);
	file.writePixels(HEIGHT);
    }
    ImagePtr image(icv_read(path.c_str(), BU_MIME_IMAGE_AUTO, 0, 0), icv_destroy);
    check_samples(image.get(), expected);
    if (!image)
	return;
    check(image->color_space == (gray ? ICV_COLOR_SPACE_GRAY : ICV_COLOR_SPACE_RGB), "color layout");
    check(image->alpha_channel == (alpha ? 1u : 0u), "alpha layout");
    check(image->flags & ICV_OPERATIONS_MODE, "HDR arithmetic enabled");
    unsigned char *encoded = nullptr;
    size_t size = 0;
    check(icv_write_mem(image.get(), &encoded, &size, BU_MIME_IMAGE_EXR) == 0, "memory encode");
    if (!encoded)
	return;
    // The ICV writer deliberately uses FLOAT, including when the source was UINT.
    std::vector<double> rounded(expected);
    for (double &sample : rounded)
	sample = static_cast<double>(static_cast<float>(sample));
    ImagePtr decoded(icv_read_mem(encoded, size, BU_MIME_IMAGE_EXR, 0, 0), icv_destroy);
    check_samples(decoded.get(), rounded);
    check_samples(image.get(), expected);
    const std::string written = path + ".written.exr";
    check(icv_write(image.get(), written.c_str(), BU_MIME_IMAGE_AUTO) == 0, "file encode");
    exr::InputFile external(written.c_str());
    check(external.header().compression() == exr::ZIP_COMPRESSION, "lossless ZIP output");
    for (size_t c = 0; c < names.size(); ++c)
	check(external.header().channels().findChannel(names[c])->type == exr::FLOAT, "FLOAT channel output");
    const std::string prefixed = "EXR:" + written;
    ImagePtr prefixed_image(icv_read(prefixed.c_str(), BU_MIME_IMAGE_AUTO, 0, 0), icv_destroy);
    check_samples(prefixed_image.get(), rounded);
    for (size_t cut : {size_t(1), size_t(7), size_t(32), size - 1}) {
	ImagePtr truncated(icv_read_mem(encoded, cut, BU_MIME_IMAGE_EXR, 0, 0), icv_destroy);
	check(!truncated, "truncated input rejected");
    }
    bu_free(encoded, "EXR test encoded buffer");
}

static void
arithmetic_and_special_values()
{
    ImagePtr image(icv_create(1, 1, ICV_COLOR_SPACE_RGB), icv_destroy);
    if (!image) { check(false, "HDR image allocation"); return; }
    image->data[0] = -2.5; image->data[1] = 8.0; image->data[2] = 0.25;
    unsigned char *buffer = nullptr; size_t size = 0;
    check(icv_write_mem(image.get(), &buffer, &size, BU_MIME_IMAGE_EXR) == 0, "HDR source encode");
    ImagePtr hdr(icv_read_mem(buffer, size, BU_MIME_IMAGE_EXR, 0, 0), icv_destroy);
    bu_free(buffer, "HDR source");
    if (!hdr) { check(false, "HDR source decode"); return; }
    check(icv_add_val(hdr.get(), 2.0) == 0, "HDR scalar arithmetic");
    check(NEAR_EQUAL(hdr->data[0], -0.5, 1.0e-6) && NEAR_EQUAL(hdr->data[1], 10.0, 1.0e-6), "HDR scalar values not clipped");
    using Binary = icv_image_t *(*)(icv_image_t *, icv_image_t *);
    for (Binary operation : {icv_add, icv_sub, icv_multiply, icv_divide}) {
	ImagePtr output(operation(hdr.get(), image.get()), icv_destroy);
	check(output && (output->flags & ICV_OPERATIONS_MODE), "binary HDR mode propagated");
	if (output)
	    check(std::abs(output->data[1]) > 1.0, "binary HDR values not clipped");
	ImagePtr reversed(operation(image.get(), hdr.get()), icv_destroy);
	check(reversed && (reversed->flags & ICV_OPERATIONS_MODE), "right operand HDR mode propagated");
    }
    ImagePtr display(icv_add(image.get(), image.get()), icv_destroy);
    check(display && !(display->flags & ICV_OPERATIONS_MODE), "default arithmetic remains display range");
    if (display)
	check_samples(display.get(), {0.0, 1.0, 0.5});
    image->data[0] = (std::numeric_limits<double>::max)();
    buffer = nullptr; size = 0;
    check(icv_write_mem(image.get(), &buffer, &size, BU_MIME_IMAGE_EXR) != 0, "FLOAT overflow rejected");
    check(!buffer && size == 0, "failed encode leaves empty output");
    image->data[0] = (std::numeric_limits<double>::quiet_NaN)();
    image->data[1] = (std::numeric_limits<double>::infinity)();
    check(icv_write_mem(image.get(), &buffer, &size, BU_MIME_IMAGE_EXR) == 0, "IEEE special values encode");
    ImagePtr special(icv_read_mem(buffer, size, BU_MIME_IMAGE_EXR, 0, 0), icv_destroy);
    bu_free(buffer, "special EXR values");
    check(special && std::isnan(special->data[0]) && std::isinf(special->data[1]), "IEEE special values preserved");
}

static void
display_conversion()
{
    ImagePtr image(icv_create(2, 1, ICV_COLOR_SPACE_RGB), icv_destroy);
    check(image != nullptr, "display-conversion image allocation");
    if (!image) return;
    image->data[0] = (std::numeric_limits<float>::max)();
    image->data[1] = (std::numeric_limits<double>::quiet_NaN)();
    image->data[2] = -(std::numeric_limits<double>::infinity)();
    image->data[3] = (std::numeric_limits<double>::infinity)();
    image->data[4] = 0.5;
    image->data[5] = -0.5;
    unsigned char *buffer = nullptr; size_t size = 0;
    check(icv_write_mem(image.get(), &buffer, &size, BU_MIME_IMAGE_PNG) == 0, "HDR display conversion");
    ImagePtr decoded(icv_read_mem(buffer, size, BU_MIME_IMAGE_PNG, 0, 0), icv_destroy);
    bu_free(buffer, "display conversion buffer");
    check_samples(decoded.get(), {1.0, 0.0, 0.0, 1.0, 128.0 / 255.0, 0.0});
    check(std::isnan(image->data[1]) && std::isinf(image->data[3]), "display conversion preserves original samples");
}

static void
unsupported_files(const std::string &directory)
{
    float sample = 0.5f;
    exr::Header header(1, 1);
    exr::FrameBuffer frame;
    for (const char *name : {"R", "G", "B", "Z"}) {
	header.channels().insert(name, exr::Channel(exr::FLOAT));
	frame.insert(name, exr::Slice(exr::FLOAT, reinterpret_cast<char *>(&sample), sizeof(float), sizeof(float)));
    }
    const std::string auxiliary = directory + "/auxiliary.exr";
    { exr::OutputFile file(auxiliary.c_str(), header); file.setFrameBuffer(frame); file.writePixels(1); }
    ImagePtr auxiliary_image(icv_read(auxiliary.c_str(), BU_MIME_IMAGE_AUTO, 0, 0), icv_destroy);
    check(!auxiliary_image, "auxiliary channels rejected without silent loss");

    const std::string subsampled = directory + "/subsampled.exr";
    exr::Header sampled_header(4, 2);
    exr::FrameBuffer sampled_frame;
    float sampled_values[] = {0.0f, 0.25f, 0.5f, 1.0f};
    for (const char *name : {"R", "G", "B"}) {
	sampled_header.channels().insert(name, exr::Channel(exr::FLOAT, 2, 1));
	sampled_frame.insert(name, exr::Slice(exr::FLOAT, reinterpret_cast<char *>(sampled_values),
	    sizeof(float), 2 * sizeof(float), 2, 1));
    }
    { exr::OutputFile file(subsampled.c_str(), sampled_header); file.setFrameBuffer(sampled_frame); file.writePixels(2); }
    ImagePtr sampled_image(icv_read(subsampled.c_str(), BU_MIME_IMAGE_AUTO, 0, 0), icv_destroy);
    check(!sampled_image, "subsampled color image rejected");

    const std::string multipart = directory + "/multipart.exr";
    exr::Header headers[] = {header, header};
    headers[0].setName("first"); headers[1].setName("second");
    for (auto &part : headers) part.setType(exr::SCANLINEIMAGE);
    {
	exr::MultiPartOutputFile file(multipart.c_str(), headers, 2);
	for (int i = 0; i < 2; ++i) {
	    exr::OutputPart part(file, i); part.setFrameBuffer(frame); part.writePixels(1);
	}
    }
    ImagePtr multipart_image(icv_read(multipart.c_str(), BU_MIME_IMAGE_AUTO, 0, 0), icv_destroy);
    check(!multipart_image, "multipart image rejected");

    const std::string deep = directory + "/deep.exr";
    exr::Header deep_header(1, 1);
    deep_header.setType(exr::DEEPSCANLINE);
    deep_header.compression() = exr::ZIPS_COMPRESSION;
    deep_header.channels().insert("Y", exr::Channel(exr::FLOAT));
    uint32_t count = 0; float *values = nullptr;
    exr::DeepFrameBuffer deep_frame;
    deep_frame.insertSampleCountSlice(exr::Slice(exr::UINT, reinterpret_cast<char *>(&count), sizeof(count), sizeof(count)));
    deep_frame.insert("Y", exr::DeepSlice(exr::FLOAT, reinterpret_cast<char *>(&values), sizeof(values), sizeof(values), sizeof(float)));
    { exr::DeepScanLineOutputFile file(deep.c_str(), deep_header); file.setFrameBuffer(deep_frame); file.writePixels(1); }
    ImagePtr deep_image(icv_read(deep.c_str(), BU_MIME_IMAGE_AUTO, 0, 0), icv_destroy);
    check(!deep_image, "deep image rejected");
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 2) return 1;
    try {
	const std::string mode(argv[1]);
	const std::vector<double> stream_values{-2.5, 8.0, 0.25, 0.5, 1.0, 65504.0};
	if (mode == "--emit") {
	    ImagePtr image(icv_create(2, 1, ICV_COLOR_SPACE_RGB), icv_destroy);
	    if (!image) return 1;
	    for (size_t i = 0; i < stream_values.size(); ++i) image->data[i] = stream_values[i];
	    return icv_write(image.get(), nullptr, BU_MIME_IMAGE_EXR);
	}
	if (mode == "--copy" || mode == "--check-stream") {
	    ImagePtr image(icv_read(nullptr, BU_MIME_IMAGE_EXR, 0, 0), icv_destroy);
	    if (!image) return 1;
	    if (mode == "--copy") return icv_write(image.get(), nullptr, BU_MIME_IMAGE_EXR);
	    check_samples(image.get(), stream_values);
	    return failures ? 1 : 0;
	}
	const std::string directory(argv[1]);
	roundtrip(directory + "/rgb.exr", {"R", "G", "B"}, exr::FLOAT, false);
	roundtrip(directory + "/rgba.exr", {"R", "G", "B", "A"}, exr::HALF, false);
	roundtrip(directory + "/gray.exr", {"Y"}, exr::UINT, false);
	roundtrip(directory + "/gray-alpha.exr", {"Y", "A"}, exr::FLOAT, true);
	arithmetic_and_special_values();
	display_conversion();
	unsupported_files(directory);
    } catch (const std::exception &error) {
	bu_log("EXR test exception: %s\n", error.what()); return 1;
    }
    return failures ? 1 : 0;
}
