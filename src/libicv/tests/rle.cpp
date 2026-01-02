/*                         R L E . C P P
 * BRL-CAD
 *
 * Published in 2025 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file rle.cpp
 *
 * @brief Comprehensive test suite for RLE image format reader/writer
 *
 * - Basic I/O and roundtrip tests
 * - Error handling and validation coverage
 * - Positional validation with random patterns
 * - Background modes and long opcodes
 */

#include "common.h"

#include <iostream>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>

#include "bu.h"
#include "icv.h"

#include "../rle.hpp"

//==============================================================================
// Test Framework
//==============================================================================

struct TestStats {
    int total = 0;
    int passed = 0;
    int failed = 0;

    void record_pass() { total++; passed++; }
    void record_fail() { total++; failed++; }

    void print_summary() const {
	std::cout << "\n========================================\n";
	std::cout << "Test Summary:\n";
	std::cout << "  Total:   " << total << "\n";
	if (total > 0) {
	    std::cout << "  Passed:  " << passed << " (" << (100 * passed / total) << "%)\n";
	} else {
	    std::cout << "  Passed:  " << passed << "\n";
	}
	std::cout << "  Failed:  " << failed << "\n";
	std::cout << "========================================\n";
    }
};

TestStats g_stats;

// Helper macros for testing
#define TEST(name) \
    std::cout << "TEST: " << name << " ... "; \
bool test_passed = true;

#define EXPECT_TRUE(condition) \
    if (!(condition)) { \
	std::cout << "\n  FAILED at line " << __LINE__ << ": " #condition << std::endl; \
	test_passed = false; \
    }

#define EXPECT_FALSE(condition) \
    if ((condition)) { \
	std::cout << "\n  FAILED at line " << __LINE__ << ": !(" #condition ")" << std::endl; \
	test_passed = false; \
    }

#define EXPECT_EQ(a, b) \
    if ((a) != (b)) { \
	std::cout << "\n  FAILED at line " << __LINE__ << ": " #a " != " #b << std::endl; \
	test_passed = false; \
    }

#define EXPECT_NE(a, b) \
    if ((a) == (b)) { \
	std::cout << "\n  FAILED at line " << __LINE__ << ": " #a " == " #b << std::endl; \
	test_passed = false; \
    }

#define END_TEST() \
    if (test_passed) { \
	std::cout << "PASSED\n"; \
	g_stats.record_pass(); \
    } else { \
	g_stats.record_fail(); \
    }

//==============================================================================
// Helper Functions
//==============================================================================

// Create test image
icv_image_t* create_test_image(size_t width, size_t height, size_t channels = 3) {
    icv_image_t *img = (icv_image_t*)calloc(1, sizeof(icv_image_t));
    if (!img) return nullptr;

    img->magic = 0x6269666d;  // ICV_IMAGE_MAGIC
    img->width = width;
    img->height = height;
    img->channels = channels;
    img->alpha_channel = (channels >= 4) ? 1 : 0;
    img->color_space = ICV_COLOR_SPACE_RGB;
    img->gamma_corr = 0.0;

    size_t data_size = width * height * channels * sizeof(double);
    img->data = (double*)calloc(1, data_size);
    if (!img->data) {
	free(img);
	return nullptr;
    }

    return img;
}

// Free test image
void free_test_image(icv_image_t* img) {
    if (!img) return;
    if (img->data) {
	bu_free(img->data, "image data");
    }
    bu_free(img, "image struct");
}

// Compare pixel values with tolerance
bool pixels_match(const icv_image_t* img1, const icv_image_t* img2, double tolerance = 0.01) {
    if (!img1 || !img2) return false;
    if (img1->width != img2->width || img1->height != img2->height) return false;
    if (img1->channels != img2->channels) return false;

    size_t total_values = img1->width * img1->height * img1->channels;
    for (size_t i = 0; i < total_values; i++) {
	double diff = std::abs(img1->data[i] - img2->data[i]);
	if (diff > tolerance) {
	    size_t pixel_idx = i / img1->channels;
	    size_t channel = i % img1->channels;
	    size_t y = pixel_idx / img1->width;
	    size_t x = pixel_idx % img1->width;
	    std::cout << "\n  Pixel mismatch at (" << x << "," << y << ") channel " << channel
		<< ": " << img1->data[i] << " vs " << img2->data[i] << std::endl;
	    return false;
	}
    }
    return true;
}

// Helper for rle::Image functions
rle::Image create_rle_image_header(uint32_t w, uint32_t h, uint8_t ncolors = 3) {
    rle::Image img;
    img.header.xpos = 0;
    img.header.ypos = 0;
    img.header.xlen = uint16_t(w);
    img.header.ylen = uint16_t(h);
    img.header.ncolors = ncolors;
    img.header.pixelbits = 8;
    img.header.ncmap = 0;
    img.header.cmaplen = 0;
    img.header.flags |= rle::FLAG_NO_BACKGROUND;
    return img;
}

rle::Image create_rle_image(uint32_t w, uint32_t h, uint8_t ncolors = 3) {
    rle::Image img = create_rle_image_header(w, h, ncolors);
    rle::Error err;
    if (!img.allocate(err)) {
	std::cerr << "Failed to allocate image: " << rle::error_string(err) << std::endl;
	exit(1);
    }
    return img;
}

bool rle_roundtrip(const rle::Image& img, rle::Image& out, 
	rle::Encoder::BackgroundMode bg_mode = rle::Encoder::BG_SAVE_ALL) {
    const char* filename = "test_tmp_roundtrip.rle";

    FILE* f = fopen(filename, "wb");
    if (!f) return false;

    rle::Error err;
    bool write_ok = rle::Encoder::write(f, img, bg_mode, err);
    fclose(f);
    if (!write_ok) {
	std::cerr << "Write failed: " << rle::error_string(err) << std::endl;
	return false;
    }

    f = fopen(filename, "rb");
    if (!f) return false;

    auto res = rle::Decoder::read(f, out);
    fclose(f);
    bu_file_delete(filename);

    if (res.error != rle::Error::OK) {
	std::cerr << "Read failed: " << rle::error_string(res.error) << std::endl;
	return false;
    }

    return true;
}

bool rle_images_match(const rle::Image& a, const rle::Image& b) {
    if (a.header.width() != b.header.width() || a.header.height() != b.header.height()) return false;
    if (a.header.channels() != b.header.channels()) return false;

    for (uint32_t y = 0; y < a.header.height(); y++) {
	for (uint32_t x = 0; x < a.header.width(); x++) {
	    for (size_t c = 0; c < a.header.channels(); c++) {
		if (a.pixel(x, y)[c] != b.pixel(x, y)[c]) {
		    std::cerr << "Mismatch at (" << x << "," << y << ") channel " << c
			<< ": " << (int)a.pixel(x, y)[c] << " != " << (int)b.pixel(x, y)[c] << std::endl;
		    return false;
		}
	    }
	}
    }
    return true;
}

//==============================================================================
// SECTION 1: Basic I/O and Roundtrip Tests
//==============================================================================

void test_simple_roundtrip() {
    TEST("Simple roundtrip (24x18 RGB)");

    const size_t w = 24, h = 18;
    icv_image_t* original = create_test_image(w, h, 3);
    EXPECT_TRUE(original != nullptr);

    // Fill with test pattern
    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    original->data[idx + 0] = (double)x / w;
	    original->data[idx + 1] = (double)y / h;
	    original->data[idx + 2] = 0.5;
	}
    }

    FILE* fp = std::fopen("test_roundtrip.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    int result = rle_write(original, fp);
    std::fclose(fp);
    EXPECT_EQ(result, 0);

    fp = std::fopen("test_roundtrip.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_EQ(readback->width, w);
	EXPECT_EQ(readback->height, h);
	EXPECT_EQ(readback->channels, 3u);
	EXPECT_TRUE(pixels_match(original, readback));
	free_test_image(readback);
    }

    free_test_image(original);
    bu_file_delete("test_roundtrip.rle");

    END_TEST();
}

void test_solid_color() {
    TEST("Solid color image (32x32, all red)");

    const size_t w = 32, h = 32;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    for (size_t i = 0; i < w * h * 3; i += 3) {
	img->data[i + 0] = 1.0;
	img->data[i + 1] = 0.0;
	img->data[i + 2] = 0.0;
    }

    FILE* fp = std::fopen("test_solid.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_solid.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_solid.rle");

    END_TEST();
}

void test_gradient_pattern() {
    TEST("Gradient pattern (48x48)");

    const size_t w = 48, h = 48;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    img->data[idx + 0] = (double)x / (w - 1);
	    img->data[idx + 1] = (double)y / (h - 1);
	    img->data[idx + 2] = ((double)x + y) / (w + h - 2);
	}
    }

    FILE* fp = std::fopen("test_gradient.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_gradient.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_gradient.rle");

    END_TEST();
}

void test_minimum_size() {
    TEST("Minimum size image (1x1)");

    icv_image_t* img = create_test_image(1, 1, 3);
    EXPECT_TRUE(img != nullptr);

    img->data[0] = 0.8;
    img->data[1] = 0.6;
    img->data[2] = 0.4;

    FILE* fp = std::fopen("test_1x1.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_1x1.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_EQ(readback->width, 1u);
	EXPECT_EQ(readback->height, 1u);
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_1x1.rle");

    END_TEST();
}

void test_wide_image() {
    TEST("Wide image (256x4)");

    const size_t w = 256, h = 4;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    img->data[idx + 0] = (double)x / (w - 1);
	    img->data[idx + 1] = (double)y / (h - 1);
	    img->data[idx + 2] = 0.5;
	}
    }

    FILE* fp = std::fopen("test_wide.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_wide.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_wide.rle");

    END_TEST();
}

void test_tall_image() {
    TEST("Tall image (4x256)");

    const size_t w = 4, h = 256;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    img->data[idx + 0] = (double)x / (w - 1);
	    img->data[idx + 1] = (double)y / (h - 1);
	    img->data[idx + 2] = 0.5;
	}
    }

    FILE* fp = std::fopen("test_tall.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_tall.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_tall.rle");

    END_TEST();
}

void test_checkerboard() {
    TEST("Checkerboard pattern (64x64)");

    const size_t w = 64, h = 64;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    bool is_black = ((x / 8) + (y / 8)) % 2 == 0;
	    double val = is_black ? 0.0 : 1.0;
	    img->data[idx + 0] = val;
	    img->data[idx + 1] = val;
	    img->data[idx + 2] = val;
	}
    }

    FILE* fp = std::fopen("test_checker.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_checker.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_checker.rle");

    END_TEST();
}

void test_large_image() {
    TEST("Large image (256x256)");

    const size_t w = 256, h = 256;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    img->data[idx + 0] = (double)(x % 256) / 255.0;
	    img->data[idx + 1] = (double)(y % 256) / 255.0;
	    img->data[idx + 2] = (double)((x + y) % 256) / 255.0;
	}
    }

    FILE* fp = std::fopen("test_large.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_large.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_large.rle");

    END_TEST();
}

void test_random_noise() {
    TEST("Random noise pattern (32x32)");

    const size_t w = 32, h = 32;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    unsigned int seed = 12345;
    for (size_t i = 0; i < w * h * 3; i++) {
	seed = (seed * 1103515245 + 12345) & 0x7fffffff;
	img->data[i] = (double)(seed % 256) / 255.0;
    }

    FILE* fp = std::fopen("test_noise.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_noise.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_noise.rle");

    END_TEST();
}

//==============================================================================
// SECTION 2: Alpha Channel Tests
//==============================================================================

void test_alpha_roundtrip() {
    TEST("Alpha channel roundtrip (RGBA 16x16)");

    const size_t w = 16, h = 16;
    icv_image_t* img = create_test_image(w, h, 4);
    EXPECT_TRUE(img != nullptr);
    img->alpha_channel = 1;

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 4;
	    img->data[idx + 0] = (double)x / (w - 1);
	    img->data[idx + 1] = (double)y / (h - 1);
	    img->data[idx + 2] = 0.5;
	    img->data[idx + 3] = (double)(x + y) / (w + h - 2);
	}
    }

    FILE* fp = std::fopen("test_alpha.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_alpha.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_EQ(readback->channels, 4u);
	EXPECT_EQ(readback->alpha_channel, 1);
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_alpha.rle");

    END_TEST();
}

void test_alpha_preservation() {
    TEST("Alpha preservation (various alpha values)");

    const size_t w = 8, h = 8;
    icv_image_t* img = create_test_image(w, h, 4);
    EXPECT_TRUE(img != nullptr);
    img->alpha_channel = 1;

    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 4;
	    img->data[idx + 0] = 0.5;
	    img->data[idx + 1] = 0.5;
	    img->data[idx + 2] = 0.5;
	    img->data[idx + 3] = (double)((x + y) % 5) / 4.0;
	}
    }

    FILE* fp = std::fopen("test_alpha_preserve.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    rle_write(img, fp);
    std::fclose(fp);

    fp = std::fopen("test_alpha_preserve.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_alpha_preserve.rle");

    END_TEST();
}

//==============================================================================
// SECTION 3: Error Handling Tests
//==============================================================================

void test_null_image_write() {
    TEST("Error handling: null image write");

    FILE* fp = std::fopen("test_null.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    int result = rle_write(nullptr, fp);
    std::fclose(fp);
    EXPECT_NE(result, 0);

    bu_file_delete("test_null.rle");

    END_TEST();
}

void test_invalid_file() {
    TEST("Error handling: invalid file read");

    FILE* fp = std::fopen("nonexistent_file.rle", "rb");
    if (fp) {
	std::fclose(fp);
	std::cout << "SKIPPED (file exists)\n";
	return;
    }

    END_TEST();
}

void test_read_null_pointer() {
    TEST("Read with null file pointer");

    icv_image_t* img = rle_read(nullptr);
    EXPECT_TRUE(img == nullptr);

    END_TEST();
}

void test_read_truncated_header() {
    TEST("Read truncated header");

    FILE* fp = std::fopen("test_truncated.rle", "wb");
    EXPECT_TRUE(fp != nullptr);

    uint8_t data[] = {
	uint8_t(rle::RLE_MAGIC & 0xFF),
	uint8_t((rle::RLE_MAGIC >> 8) & 0xFF)
    };
    std::fwrite(data, 1, sizeof(data), fp);
    std::fclose(fp);

    fp = std::fopen("test_truncated.rle", "rb");
    EXPECT_TRUE(fp != nullptr);

    icv_image_t* img = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(img == nullptr);

    bu_file_delete("test_truncated.rle");

    END_TEST();
}

void test_write_invalid_channels() {
    TEST("Write with invalid channel count");

    icv_image_t* img = create_test_image(10, 10, 1);
    EXPECT_TRUE(img != nullptr);

    if (img) {
	FILE* fp = std::fopen("test_invalid_channels.rle", "wb");
	EXPECT_TRUE(fp != nullptr);

	int result = rle_write(img, fp);
	std::fclose(fp);
	EXPECT_NE(result, 0);

	free_test_image(img);
    }

    END_TEST();
}

void test_write_oversized_dimensions() {
    TEST("Write with oversized dimensions");

    icv_image_t* img = create_test_image(10, 10, 3);
    EXPECT_TRUE(img != nullptr);

    if (img) {
	img->width = rle::MAX_DIM + 1;
	img->height = rle::MAX_DIM + 1;

	FILE* fp = std::fopen("test_oversized.rle", "wb");
	EXPECT_TRUE(fp != nullptr);

	int result = rle_write(img, fp);
	std::fclose(fp);
	EXPECT_NE(result, 0);

	img->width = 10;
	img->height = 10;
	free_test_image(img);
    }

    END_TEST();
}

//==============================================================================
// SECTION 4: Header Validation Tests
//==============================================================================

void test_all_error_strings() {
    TEST("All error string coverage");

    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::OK), "OK") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::BAD_MAGIC), "Bad magic") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::HEADER_TRUNCATED), "Header truncated") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::UNSUPPORTED_ENDIAN), "Unsupported endian") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::DIM_TOO_LARGE), "Dimensions exceed max") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::PIXELS_TOO_LARGE), "Pixel count exceeds max") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::ALLOC_TOO_LARGE), "Allocation exceeds cap") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::COLORMAP_TOO_LARGE), "Colormap exceeds cap") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::COMMENT_TOO_LARGE), "Comment block too large") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::INVALID_NCOLORS), "Invalid ncolors") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::INVALID_PIXELBITS), "Invalid pixelbits") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::INVALID_BG_BLOCK), "Invalid background block") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::OPCODE_OVERFLOW), "Opcode operand overflow") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::OPCODE_UNKNOWN), "Unknown opcode") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::TRUNCATED_OPCODE), "Truncated opcode data") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::OP_COUNT_EXCEEDED), "Opcode count per row exceeded") == 0);
    EXPECT_TRUE(bu_strcmp(rle::error_string(rle::Error::INTERNAL_ERROR), "Internal error") == 0);

    END_TEST();
}

void test_invalid_header_dimensions() {
    TEST("Invalid header dimensions");

    rle::Header h;
    h.xlen = 0;
    h.ylen = 100;
    h.ncolors = 3;
    h.pixelbits = 8;
    h.flags = rle::FLAG_NO_BACKGROUND;

    rle::Error err;
    bool valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::DIM_TOO_LARGE);

    h.xlen = rle::MAX_DIM + 1;
    h.ylen = 100;
    valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::DIM_TOO_LARGE);

    END_TEST();
}

void test_invalid_pixelbits() {
    TEST("Invalid pixelbits");

    rle::Header h;
    h.xlen = 100;
    h.ylen = 100;
    h.ncolors = 3;
    h.pixelbits = 16;
    h.flags = rle::FLAG_NO_BACKGROUND;

    rle::Error err;
    bool valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::INVALID_PIXELBITS);

    END_TEST();
}

void test_invalid_ncolors() {
    TEST("Invalid ncolors");

    rle::Header h;
    h.xlen = 100;
    h.ylen = 100;
    h.ncolors = 0;
    h.pixelbits = 8;
    h.flags = rle::FLAG_NO_BACKGROUND;

    rle::Error err;
    bool valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::INVALID_NCOLORS);

    h.ncolors = 255;
    valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::INVALID_NCOLORS);

    END_TEST();
}

void test_invalid_background() {
    TEST("Invalid background block");

    rle::Header h;
    h.xlen = 100;
    h.ylen = 100;
    h.ncolors = 3;
    h.pixelbits = 8;
    h.flags = 0;
    h.background = {128, 128};

    rle::Error err;
    bool valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::INVALID_BG_BLOCK);

    END_TEST();
}

void test_colormap_validation() {
    TEST("Colormap validation");

    rle::Header h;
    h.xlen = 10;
    h.ylen = 10;
    h.ncolors = 3;
    h.pixelbits = 8;
    h.flags = rle::FLAG_NO_BACKGROUND;
    h.ncmap = 3;
    h.cmaplen = 8;

    size_t entries = 3 * 256;
    h.colormap.resize(entries, 0x8080);

    rle::Error err;
    bool valid = h.validate(err);
    EXPECT_TRUE(valid);
    EXPECT_EQ(err, rle::Error::OK);

    h.colormap.resize(10);
    valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::COLORMAP_TOO_LARGE);

    END_TEST();
}

void test_colormap_too_large() {
    TEST("Colormap too large");

    rle::Header h;
    h.xlen = 10;
    h.ylen = 10;
    h.ncolors = 3;
    h.pixelbits = 8;
    h.flags = rle::FLAG_NO_BACKGROUND;
    h.ncmap = 4;
    h.cmaplen = 8;

    rle::Error err;
    bool valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::COLORMAP_TOO_LARGE);

    h.ncmap = 3;
    h.cmaplen = 9;
    valid = h.validate(err);
    EXPECT_FALSE(valid);
    EXPECT_EQ(err, rle::Error::COLORMAP_TOO_LARGE);

    END_TEST();
}

//==============================================================================
// SECTION 5: Positional Validation Tests (using rle:: API)
//==============================================================================

void test_random_rgb_pattern() {
    TEST("Random RGB pattern (32x32)");

    const size_t W = 32, H = 32;
    std::vector<uint8_t> data(W * H * 3);

    srand(12345);
    for (size_t i = 0; i < W * H * 3; i++) {
	data[i] = rand() % 256;
    }

    FILE* fp = fopen("test_random_rgb.rle", "wb");
    EXPECT_TRUE(fp != nullptr);

    rle::Error err;
    bool ok = rle::write_rgb(fp, data.data(), W, H, {}, {}, false,
	    rle::Encoder::BG_SAVE_ALL, err);
    fclose(fp);
    EXPECT_TRUE(ok && err == rle::Error::OK);

    std::vector<uint8_t> readback;
    uint32_t rw, rh;
    bool has_alpha;

    fp = fopen("test_random_rgb.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    ok = rle::read_rgb(fp, readback, rw, rh, &has_alpha, nullptr, err);
    fclose(fp);
    EXPECT_TRUE(ok && err == rle::Error::OK);
    EXPECT_EQ(rw, W);
    EXPECT_EQ(rh, H);

    bool match = (data == readback);
    EXPECT_TRUE(match);

    bu_file_delete("test_random_rgb.rle");

    END_TEST();
}

void test_random_rgba_pattern() {
    TEST("Random RGBA pattern (32x32)");

    const size_t W = 32, H = 32;
    std::vector<uint8_t> data(W * H * 4);

    srand(54321);
    for (size_t i = 0; i < W * H * 4; i++) {
	data[i] = rand() % 256;
    }

    FILE* fp = fopen("test_random_rgba.rle", "wb");
    EXPECT_TRUE(fp != nullptr);

    rle::Error err;
    bool ok = rle::write_rgb(fp, data.data(), W, H, {}, {}, true,
	    rle::Encoder::BG_SAVE_ALL, err);
    fclose(fp);
    EXPECT_TRUE(ok && err == rle::Error::OK);

    std::vector<uint8_t> readback;
    uint32_t rw, rh;
    bool has_alpha;

    fp = fopen("test_random_rgba.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    ok = rle::read_rgb(fp, readback, rw, rh, &has_alpha, nullptr, err);
    fclose(fp);
    EXPECT_TRUE(ok && err == rle::Error::OK);
    EXPECT_EQ(rw, W);
    EXPECT_EQ(rh, H);
    EXPECT_TRUE(has_alpha);

    bool match = (data == readback);
    EXPECT_TRUE(match);

    bu_file_delete("test_random_rgba.rle");

    END_TEST();
}

//==============================================================================
// SECTION 6: Background Mode Tests (unusual paths)
//==============================================================================

void test_bg_overlay_entire_rows() {
    TEST("BG_OVERLAY with entire background rows");

    rle::Image img = create_rle_image_header(100, 50);
    img.header.background = {100, 150, 200};
    img.header.flags &= ~rle::FLAG_NO_BACKGROUND;

    rle::Error err;
    EXPECT_TRUE(img.allocate(err));

    // Rows 0-9: non-background
    for (uint32_t y = 0; y < 10; y++) {
	for (uint32_t x = 0; x < img.header.width(); x++) {
	    img.pixel(x, y)[0] = 50;
	    img.pixel(x, y)[1] = 75;
	    img.pixel(x, y)[2] = 25;
	}
    }
    // Rows 10-19: background (100, 150, 200)
    // Rows 20-49: non-background
    for (uint32_t y = 20; y < img.header.height(); y++) {
	for (uint32_t x = 0; x < img.header.width(); x++) {
	    img.pixel(x, y)[0] = 200;
	    img.pixel(x, y)[1] = 100;
	    img.pixel(x, y)[2] = 50;
	}
    }

    rle::Image out;
    EXPECT_TRUE(rle_roundtrip(img, out, rle::Encoder::BG_OVERLAY));
    EXPECT_TRUE(rle_images_match(img, out));

    END_TEST();
}

void test_long_run_data() {
    TEST("Long form RUN_DATA opcode (>255 pixels)");

    rle::Image img = create_rle_image(512, 20);

    for (uint32_t y = 0; y < img.header.height(); y++) {
	uint8_t r = uint8_t(y * 10);
	uint8_t g = uint8_t(y * 5);
	uint8_t b = uint8_t(255 - y * 10);

	for (uint32_t x = 0; x < img.header.width(); x++) {
	    img.pixel(x, y)[0] = r;
	    img.pixel(x, y)[1] = g;
	    img.pixel(x, y)[2] = b;
	}
    }

    rle::Image out;
    EXPECT_TRUE(rle_roundtrip(img, out));
    EXPECT_TRUE(rle_images_match(img, out));

    END_TEST();
}

void test_long_skip_pixels() {
    TEST("Long form SKIP_PIXELS opcode (>255 pixels)");

    rle::Image img = create_rle_image_header(600, 15);
    img.header.background = {128, 128, 128};
    img.header.flags &= ~rle::FLAG_NO_BACKGROUND;

    rle::Error err;
    EXPECT_TRUE(img.allocate(err));

    for (uint32_t y = 0; y < img.header.height(); y++) {
	for (uint32_t x = 0; x < img.header.width(); x++) {
	    if (x < 50 || (x >= 350 && x < 400)) {
		img.pixel(x, y)[0] = uint8_t(x % 256);
		img.pixel(x, y)[1] = uint8_t(y * 10);
		img.pixel(x, y)[2] = 200;
	    }
	    // else: background (128, 128, 128)
	}
    }

    rle::Image out;
    EXPECT_TRUE(rle_roundtrip(img, out, rle::Encoder::BG_OVERLAY));
    EXPECT_TRUE(rle_images_match(img, out));

    END_TEST();
}

void test_long_skip_lines() {
    TEST("Long form SKIP_LINES opcode (>255 rows)");

    rle::Image img = create_rle_image_header(100, 300);
    img.header.background = {255, 255, 0};
    img.header.flags &= ~rle::FLAG_NO_BACKGROUND;

    rle::Error err;
    EXPECT_TRUE(img.allocate(err));

    // First 10 rows: pattern
    for (uint32_t y = 0; y < 10; y++) {
	for (uint32_t x = 0; x < img.header.width(); x++) {
	    img.pixel(x, y)[0] = uint8_t(x * 2);
	    img.pixel(x, y)[1] = uint8_t(y * 20);
	    img.pixel(x, y)[2] = 100;
	}
    }
    // Rows 10-269: background (260 rows)
    // Last 30 rows: pattern
    for (uint32_t y = 270; y < img.header.height(); y++) {
	for (uint32_t x = 0; x < img.header.width(); x++) {
	    img.pixel(x, y)[0] = uint8_t((x + y) % 256);
	    img.pixel(x, y)[1] = 150;
	    img.pixel(x, y)[2] = uint8_t(y);
	}
    }

    rle::Image out;
    EXPECT_TRUE(rle_roundtrip(img, out, rle::Encoder::BG_OVERLAY));
    EXPECT_TRUE(rle_images_match(img, out));

    END_TEST();
}

//==============================================================================
// SECTION 6B: Background Detection Threshold Tests
//==============================================================================

void test_bg_auto_detect_overlay_threshold() {
    TEST("Auto background detection: OVERLAY threshold (20%)");

    // Create image where 25% of pixels are the same color (above OVERLAY_THRESH=20%)
    const size_t w = 100, h = 100;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    // Fill 25% with red background color (2500 pixels)
    size_t bg_count = 0;
    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    if (bg_count < 2500) {  // 25% of 10000 pixels
		img->data[idx + 0] = 1.0;  // Red background
		img->data[idx + 1] = 0.0;
		img->data[idx + 2] = 0.0;
		bg_count++;
	    } else {
		// Varied colors for remaining 75%
		img->data[idx + 0] = (double)(x % 256) / 255.0;
		img->data[idx + 1] = (double)(y % 256) / 255.0;
		img->data[idx + 2] = (double)((x + y) % 256) / 255.0;
	    }
	}
    }

    // Write using automatic background detection
    FILE* fp = std::fopen("test_bg_overlay_auto.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    int result = rle_write(img, fp);
    std::fclose(fp);
    EXPECT_EQ(result, 0);

    // Read back and verify
    fp = std::fopen("test_bg_overlay_auto.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_bg_overlay_auto.rle");

    END_TEST();
}

void test_bg_auto_detect_clear_threshold() {
    TEST("Auto background detection: CLEAR threshold (50%)");

    // Create image where 55% of pixels are the same color (above CLEAR_THRESH=50%)
    const size_t w = 100, h = 100;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    // Fill 55% with blue background color (5500 pixels)
    size_t bg_count = 0;
    for (size_t y = 0; y < h; y++) {
	for (size_t x = 0; x < w; x++) {
	    size_t idx = (y * w + x) * 3;
	    if (bg_count < 5500) {  // 55% of 10000 pixels
		img->data[idx + 0] = 0.0;  // Blue background
		img->data[idx + 1] = 0.0;
		img->data[idx + 2] = 1.0;
		bg_count++;
	    } else {
		// Varied colors for remaining 45%
		img->data[idx + 0] = (double)((x * 7) % 256) / 255.0;
		img->data[idx + 1] = (double)((y * 13) % 256) / 255.0;
		img->data[idx + 2] = (double)((x + y) % 256) / 255.0;
	    }
	}
    }

    // Write using automatic background detection
    FILE* fp = std::fopen("test_bg_clear_auto.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    int result = rle_write(img, fp);
    std::fclose(fp);
    EXPECT_EQ(result, 0);

    // Read back and verify
    fp = std::fopen("test_bg_clear_auto.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_bg_clear_auto.rle");

    END_TEST();
}

void test_bg_auto_detect_early_exit() {
    TEST("Auto background detection: early exit when CLEAR threshold met");

    // Create image where majority is same color from the start
    // This should trigger early exit in detect_background (line 215-220)
    const size_t w = 80, h = 80;
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    // Fill first 60% (3840 pixels) with green, meeting CLEAR threshold early
    // Remaining 40% will be different colors
    size_t total = w * h;
    size_t bg_end = (total * 60) / 100;

    for (size_t i = 0; i < total; i++) {
	size_t idx = i * 3;
	if (i < bg_end) {
	    img->data[idx + 0] = 0.0;  // Green background
	    img->data[idx + 1] = 1.0;
	    img->data[idx + 2] = 0.0;
	} else {
	    img->data[idx + 0] = (double)((i * 3) % 256) / 255.0;
	    img->data[idx + 1] = (double)((i * 5) % 256) / 255.0;
	    img->data[idx + 2] = (double)((i * 7) % 256) / 255.0;
	}
    }

    // Write and verify roundtrip
    FILE* fp = std::fopen("test_bg_early_exit.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    int result = rle_write(img, fp);
    std::fclose(fp);
    EXPECT_EQ(result, 0);

    fp = std::fopen("test_bg_early_exit.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_bg_early_exit.rle");

    END_TEST();
}

void test_bg_auto_detect_post_loop() {
    TEST("Auto background detection: post-loop threshold check");

    // The post-loop check (lines 229-233) executes when:
    // 1. bd.mode stays BG_SAVE_ALL throughout the loop (line 222 never executes)
    // 2. After the loop, maxCount >= overlay_needed
    //
    // This happens when no single color dominates during iteration but
    // one color's final count meets the threshold. We achieve this by
    // ensuring the background color appears EXACTLY ONCE early,  then
    // all other pixels until the last batch are unique, then the background
    // color appears many times at the end.
    const size_t w = 60, h = 60;  // 3600 pixels
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    // First pixel: background color (count = 1)
    img->data[0] = 0.5;
    img->data[1] = 0.5;
    img->data[2] = 0.5;

    // Next 2879 pixels: all unique (counts stay at 1)
    for (size_t i = 1; i < 2880; i++) {
	size_t idx = i * 3;
	img->data[idx + 0] = (double)((i * 37) % 256) / 255.0;
	img->data[idx + 1] = (double)((i * 41) % 256) / 255.0;
	img->data[idx + 2] = (double)((i * 43) % 256) / 255.0;
    }

    // Last 720 pixels (20%): background color again
    // Now background count jumps from 1 to 721 (> 720 = 20% threshold)
    for (size_t i = 2880; i < 3600; i++) {
	size_t idx = i * 3;
	img->data[idx + 0] = 0.5;
	img->data[idx + 1] = 0.5;
	img->data[idx + 2] = 0.5;
    }

    FILE* fp = std::fopen("test_bg_post_loop.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    int result = rle_write(img, fp);
    std::fclose(fp);
    EXPECT_EQ(result, 0);

    fp = std::fopen("test_bg_post_loop.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_bg_post_loop.rle");

    END_TEST();
}

void test_bg_auto_detect_post_loop_clear() {
    TEST("Auto background detection: post-loop CLEAR threshold");

    // Similar to above but reaching CLEAR threshold (50%) after the loop
    const size_t w = 60, h = 60;  // 3600 pixels
    icv_image_t* img = create_test_image(w, h, 3);
    EXPECT_TRUE(img != nullptr);

    // First pixel: background color (count = 1)
    img->data[0] = 0.75;
    img->data[1] = 0.75;
    img->data[2] = 0.75;

    // Next 1799 pixels: all unique 
    for (size_t i = 1; i < 1800; i++) {
	size_t idx = i * 3;
	img->data[idx + 0] = (double)((i * 47) % 256) / 255.0;
	img->data[idx + 1] = (double)((i * 53) % 256) / 255.0;
	img->data[idx + 2] = (double)((i * 59) % 256) / 255.0;
    }

    // Last 1800 pixels (50%): background color
    // Background count jumps from 1 to 1801 (> 1800 = 50% threshold)
    for (size_t i = 1800; i < 3600; i++) {
	size_t idx = i * 3;
	img->data[idx + 0] = 0.75;
	img->data[idx + 1] = 0.75;
	img->data[idx + 2] = 0.75;
    }

    FILE* fp = std::fopen("test_bg_post_clear.rle", "wb");
    EXPECT_TRUE(fp != nullptr);
    int result = rle_write(img, fp);
    std::fclose(fp);
    EXPECT_EQ(result, 0);

    fp = std::fopen("test_bg_post_clear.rle", "rb");
    EXPECT_TRUE(fp != nullptr);
    icv_image_t* readback = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(readback != nullptr);

    if (readback) {
	EXPECT_TRUE(pixels_match(img, readback));
	free_test_image(readback);
    }

    free_test_image(img);
    bu_file_delete("test_bg_post_clear.rle");

    END_TEST();
}

//==============================================================================
// SECTION 7: Reference Image Test
//==============================================================================

void test_teapot_image() {
    TEST("Read teapot.rle reference image");

    FILE* fp = std::fopen("teapot.rle", "rb");
    if (!fp) {
	std::cout << "SKIPPED (teapot.rle not found)\n";
	return;
    }

    icv_image_t* img = rle_read(fp);
    std::fclose(fp);
    EXPECT_TRUE(img != nullptr);

    if (img) {
	EXPECT_EQ(img->width, 256u);
	EXPECT_EQ(img->height, 256u);
	EXPECT_EQ(img->channels, 3u);

        // Verify image contains non-zero data
	bool has_data = false;
	for (size_t i = 0; i < img->width * img->height * img->channels; i++) {
	    if (img->data[i] > 0.01) {
		has_data = true;
		break;
	    }
	}
	EXPECT_TRUE(has_data);

	free_test_image(img);
    }

    END_TEST();
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main(int, const char **av) {

    bu_setprogname(av[0]);

    std::cout << "========================================\n";
    std::cout << "RLE Comprehensive Test Suite\n";
    std::cout << "Consolidated from multiple test files\n";
    std::cout << "========================================\n\n";

    // Section 1: Basic I/O and Roundtrip Tests
    std::cout << "--- Basic I/O Tests ---\n";
    test_simple_roundtrip();
    test_solid_color();
    test_gradient_pattern();
    test_minimum_size();
    test_wide_image();
    test_tall_image();
    test_checkerboard();
    test_large_image();
    test_random_noise();

    // Section 2: Alpha Channel Tests
    std::cout << "\n--- Alpha Channel Tests ---\n";
    test_alpha_roundtrip();
    test_alpha_preservation();

    // Section 3: Error Handling Tests
    std::cout << "\n--- Error Handling Tests ---\n";
    test_null_image_write();
    test_invalid_file();
    test_read_null_pointer();
    test_read_truncated_header();
    test_write_invalid_channels();
    test_write_oversized_dimensions();

    // Section 4: Header Validation Tests
    std::cout << "\n--- Header Validation Tests ---\n";
    test_all_error_strings();
    test_invalid_header_dimensions();
    test_invalid_pixelbits();
    test_invalid_ncolors();
    test_invalid_background();
    test_colormap_validation();
    test_colormap_too_large();

    // Section 5: Positional Validation Tests
    std::cout << "\n--- Positional Validation Tests ---\n";
    test_random_rgb_pattern();
    test_random_rgba_pattern();

    // Section 6: Unusual Path Tests (background modes, long opcodes)
    std::cout << "\n--- Unusual Path Tests ---\n";
    test_bg_overlay_entire_rows();
    test_long_run_data();
    test_long_skip_pixels();
    test_long_skip_lines();

    // Section 6B: Background Detection Threshold Tests
    std::cout << "\n--- Background Detection Threshold Tests ---\n";
    test_bg_auto_detect_overlay_threshold();
    test_bg_auto_detect_clear_threshold();
    test_bg_auto_detect_early_exit();
    test_bg_auto_detect_post_loop();
    test_bg_auto_detect_post_loop_clear();

    // Section 7: Reference Image Test
    std::cout << "\n--- Reference Image Tests ---\n";
    test_teapot_image();

    // Print summary
    g_stats.print_summary();

    return (g_stats.failed == 0) ? 0 : 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
