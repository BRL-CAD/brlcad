/*                         A N I M . C P P
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

#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/process.h"
#include "bu/app.h"

#include "icv_private.h"
#include "icv/anim.h"
#define APNGMINI_IMPLEMENTATION
#include "apngmini.hpp"

struct icv_anim_frame {
    icv_image_t *img;
    uint32_t delay_usec;
};

struct icv_anim {
    icv_anim_format_t format;
    uint32_t width;
    uint32_t height;
    int fps;
    std::vector<icv_anim_frame> frames;
};


extern "C" {

    icv_anim_t *
    icv_anim_create(icv_anim_format_t format, uint32_t width, uint32_t height, int fps)
    {
	icv_anim_t *anim = new icv_anim;
	anim->format = format;
	anim->width = width;
	anim->height = height;
	anim->fps = (fps > 0) ? fps : 10;
	return anim;
    }

    int
    icv_anim_destroy(icv_anim_t *anim)
    {
	if (!anim) return 0;
	for (size_t i = 0; i < anim->frames.size(); ++i) {
	    if (anim->frames[i].img) {
		icv_destroy(anim->frames[i].img);
	    }
	}
	delete anim;
	return 0;
    }

    size_t
    icv_anim_num_frames(const icv_anim_t *anim)
    {
	if (!anim) return 0;
	return anim->frames.size();
    }

    int
    icv_anim_set_fps(icv_anim_t *anim, int fps)
    {
	if (!anim || fps <= 0) return -1;
	anim->fps = (fps > 0) ? fps : 10;
	uint32_t delay_usec = 1000000u / (uint32_t)anim->fps;
	if (delay_usec == 0) delay_usec = 1;
	for (size_t i = 0; i < anim->frames.size(); ++i) {
	    anim->frames[i].delay_usec = delay_usec;
	}
	return 0;
    }

    int
    icv_anim_set_frame_delay(icv_anim_t *anim, size_t index, uint32_t delay_usec)
    {
	if (!anim || index >= anim->frames.size()) return -1;
	anim->frames[index].delay_usec = delay_usec;
	return 0;
    }


    static apngmini::Frame
    icv_to_apng(const icv_image_t *img, uint32_t delay_usec)
    {
	apngmini::Frame f;
	f.width = img->width;
	f.height = img->height;
	f.x_offset = 0;
	f.y_offset = 0;
	f.dispose_op = apngmini::DisposeOp::None;
	f.blend_op = apngmini::BlendOp::Source;

	// Set delay
	f.delay.numerator = delay_usec / 1000; // ms
	f.delay.denominator = 1000;
	if (f.delay.numerator == 0) {
	    f.delay.numerator = 1;
	    f.delay.denominator = 10;
	}

	size_t num_pixels = img->width * img->height;
	f.pixels.resize(num_pixels * 4, 255); // Alpha = 255

	for (size_t y = 0; y < (size_t)img->height; ++y) {
	    size_t flip_y = img->height - 1 - y;
	    for (size_t x = 0; x < (size_t)img->width; ++x) {
		size_t p_src = (flip_y * img->width + x) * img->channels; // img is bottom-up
		size_t p_dst = y * img->width + x;      // apng is top-down
		if (img->color_space == ICV_COLOR_SPACE_GRAY) {
		    uint8_t val = (uint8_t)(img->data[p_src] * 255.0 + 0.5);
		    f.pixels[p_dst*4 + 0] = val;
		    f.pixels[p_dst*4 + 1] = val;
		    f.pixels[p_dst*4 + 2] = val;
		    if (img->channels >= 2) f.pixels[p_dst*4 + 3] = (uint8_t)(img->data[p_src + 1] * 255.0 + 0.5);
		} else {
		    f.pixels[p_dst*4 + 0] = (uint8_t)(img->data[p_src + 0] * 255.0 + 0.5);
		    f.pixels[p_dst*4 + 1] = (uint8_t)(img->data[p_src + 1] * 255.0 + 0.5);
		    f.pixels[p_dst*4 + 2] = (uint8_t)(img->data[p_src + 2] * 255.0 + 0.5);
		    if (img->channels >= 4) f.pixels[p_dst*4 + 3] = (uint8_t)(img->data[p_src + 3] * 255.0 + 0.5);
		}
	    }
	}
	return f;
    }

    static icv_anim_format_t
    detect_anim_format(const char *filename)
    {
	FILE *fp = fopen(filename, "rb");
	if (!fp) return ICV_ANIM_UNKNOWN;
	unsigned char header[12];
	size_t read_bytes = fread(header, 1, 12, fp);
	fclose(fp);

	if (read_bytes >= 8) {
	    if (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G' &&
		header[4] == '\r' && header[5] == '\n' && header[6] == 0x1A && header[7] == '\n') {
		return ICV_ANIM_APNG;
	    }
	}
	if (read_bytes >= 12) {
	    if (header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F' &&
		header[8] == 'A' && header[9] == 'V' && header[10] == 'I') {
		return ICV_ANIM_MJPG;
	    }
	}

	if (strstr(filename, ".apng") != NULL || strstr(filename, ".png") != NULL) {
	    return ICV_ANIM_APNG;
	} else if (strstr(filename, ".avi") != NULL || strstr(filename, ".mjpg") != NULL) {
	    return ICV_ANIM_MJPG;
	}
	return ICV_ANIM_UNKNOWN;
    }

    static uint32_t avi_read_u32(FILE *fp)
    {
	unsigned char buf[4];
	if (fread(buf, 1, 4, fp) != 4) return 0;
	return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    }

    static icv_anim_t *
    read_mjpeg_avi(const char *filename)
    {
	FILE *fp = fopen(filename, "rb");
	if (!fp) return NULL;

	unsigned char magic[12];
	if (fread(magic, 1, 12, fp) != 12) {
	    fclose(fp);
	    return NULL;
	}
	if (magic[0] != 'R' || magic[1] != 'I' || magic[2] != 'F' || magic[3] != 'F' ||
	    magic[8] != 'A' || magic[9] != 'V' || magic[10] != 'I') {
	    fclose(fp);
	    return NULL;
	}

	icv_anim_t *anim = icv_anim_create(ICV_ANIM_MJPG, 0, 0, 10);
	uint32_t usec_per_frame = 100000;

	while (!feof(fp)) {
	    unsigned char chunk_id[4];
	    if (fread(chunk_id, 1, 4, fp) != 4) break;
	    uint32_t chunk_size = avi_read_u32(fp);

	    if (chunk_id[0] == 'L' && chunk_id[1] == 'I' && chunk_id[2] == 'S' && chunk_id[3] == 'T') {
		unsigned char list_type[4];
		if (fread(list_type, 1, 4, fp) != 4) break;
		if (list_type[0] == 'm' && list_type[1] == 'o' && list_type[2] == 'v' && list_type[3] == 'i') {
		    long movi_end = ftell(fp) + chunk_size - 4;
		    while (ftell(fp) < movi_end && !feof(fp)) {
			unsigned char sub_id[4];
			if (fread(sub_id, 1, 4, fp) != 4) break;
			uint32_t sub_size = avi_read_u32(fp);
			if (sub_id[0] == '0' && sub_id[1] == '0' && sub_id[2] == 'd' && sub_id[3] == 'c') {
			    if (sub_size > 256 * 1024 * 1024) break; /* Sanity limit 256MB */
			    unsigned char *buf = (unsigned char *)bu_malloc(sub_size, "avi jpeg frame");
			    if (fread(buf, 1, sub_size, fp) == sub_size) {
				icv_image_t *img = icv_read_mem(buf, sub_size, BU_MIME_IMAGE_JPEG, 0, 0);
				if (img) {
				    icv_anim_frame f;
				    f.img = img;
				    f.delay_usec = usec_per_frame;
				    anim->frames.push_back(f);
				    if (img->width > anim->width) anim->width = img->width;
				    if (img->height > anim->height) anim->height = img->height;
				}
			    }
			    bu_free(buf, "avi jpeg frame");
			} else {
			    fseek(fp, sub_size, SEEK_CUR);
			}
			if (sub_size & 1) fseek(fp, 1, SEEK_CUR);
		    }
		} else if (list_type[0] == 'h' && list_type[1] == 'd' && list_type[2] == 'r' && list_type[3] == 'l') {
		    long hdrl_end = ftell(fp) + chunk_size - 4;
		    while (ftell(fp) < hdrl_end && !feof(fp)) {
			unsigned char sub_id[4];
			if (fread(sub_id, 1, 4, fp) != 4) break;
			uint32_t sub_size = avi_read_u32(fp);
			if (sub_id[0] == 'a' && sub_id[1] == 'v' && sub_id[2] == 'i' && sub_id[3] == 'h') {
			    usec_per_frame = avi_read_u32(fp);
			    if (sub_size >= 4) {
				fseek(fp, sub_size - 4, SEEK_CUR);
			    }
			} else {
			    fseek(fp, sub_size, SEEK_CUR);
			}
			if (sub_size & 1) fseek(fp, 1, SEEK_CUR);
		    }
		} else {
		    fseek(fp, chunk_size - 4, SEEK_CUR);
		}
	    } else {
		fseek(fp, chunk_size, SEEK_CUR);
	    }
	    if (chunk_size & 1) fseek(fp, 1, SEEK_CUR);
	}
	fclose(fp);

	if (usec_per_frame > 0) anim->fps = 1000000 / usec_per_frame;

	if (anim->frames.empty()) {
	    icv_anim_destroy(anim);
	    return NULL;
	}
	return anim;
    }

    icv_anim_t *
    icv_anim_read(const char *filename)
    {
	if (!filename) return NULL;

	icv_anim_format_t fmt = detect_anim_format(filename);

	if (fmt == ICV_ANIM_APNG) {
	    try {
		apngmini::Animation aapng = apngmini::read_file(filename);
		if (aapng.frames.empty()) return NULL;

		icv_anim_t *anim = icv_anim_create(ICV_ANIM_APNG, aapng.canvas_width, aapng.canvas_height, 10);

		bool use_composed_frames = false;
		for (size_t i = 0; i < aapng.frames.size(); ++i) {
		    if (aapng.frames[i].x_offset != 0 || aapng.frames[i].y_offset != 0 ||
			aapng.frames[i].dispose_op != apngmini::DisposeOp::None ||
			aapng.frames[i].blend_op != apngmini::BlendOp::Source) {
			use_composed_frames = true;
			break;
		    }
		}

		apngmini::vector<apngmini::vector<uint8_t>> composed_frames;
		if (use_composed_frames) {
		    composed_frames = aapng.compose();
		    if (composed_frames.size() != aapng.frames.size()) {
			icv_anim_destroy(anim);
			return NULL;
		    }
		}

		for (size_t i = 0; i < aapng.frames.size(); ++i) {
		    uint32_t frame_width = use_composed_frames ? aapng.canvas_width : aapng.frames[i].width;
		    uint32_t frame_height = use_composed_frames ? aapng.canvas_height : aapng.frames[i].height;
		    const apngmini::vector<uint8_t>& frame_pixels = use_composed_frames ? composed_frames[i] : aapng.frames[i].pixels;
		    icv_image_t *img = icv_image_create(frame_width, frame_height, ICV_COLOR_SPACE_RGB);
		    if (!img) {
			bu_log("Failed to create img!\n");
			icv_anim_destroy(anim);
			return NULL;
		    }

		    // Raw control APNGs keep original frame dimensions.  Delta APNGs
		    // need composition before exposing frames through the icv API.
		    for (size_t y = 0; y < (size_t)frame_height; ++y) {
			size_t flip_y = frame_height - 1 - y;
			for (size_t x = 0; x < (size_t)frame_width; ++x) {
			    size_t p_src = y * frame_width + x;
			    size_t p_dst = flip_y * frame_width + x;
			    img->data[p_dst*3 + 0] = frame_pixels[p_src*4 + 0] / 255.0;
			    img->data[p_dst*3 + 1] = frame_pixels[p_src*4 + 1] / 255.0;
			    img->data[p_dst*3 + 2] = frame_pixels[p_src*4 + 2] / 255.0;
			}
		    }

		    icv_anim_frame f;
		    f.img = img;
		    f.delay_usec = (uint32_t)(aapng.frames[i].delay.seconds() * 1000000.0);
		    anim->frames.push_back(f);
		}
		return anim;
	    } catch (const std::exception &e) {
		bu_log("icv_anim_read (APNG): Exception parsing file '%s': %s\n", filename, e.what());
		return NULL;
	    }
	} else if (fmt == ICV_ANIM_MJPG) {
	    return read_mjpeg_avi(filename);
	}

	bu_log("icv_anim_read: Unsupported format or missing support for '%s'\n", filename);
	return NULL;
    }


    // RIFF/AVI MJPEG Writers
    static void avi_write_u32(FILE *fp, uint32_t v)
    {
	unsigned char buf[4];
	buf[0] = (unsigned char)(v & 0xFF);
	buf[1] = (unsigned char)((v >>  8) & 0xFF);
	buf[2] = (unsigned char)((v >> 16) & 0xFF);
	buf[3] = (unsigned char)((v >> 24) & 0xFF);
	fwrite(buf, 1, 4, fp);
    }
    static void avi_write_u16(FILE *fp, uint16_t v)
    {
	unsigned char buf[2];
	buf[0] = (unsigned char)(v & 0xFF);
	buf[1] = (unsigned char)((v >> 8) & 0xFF);
	fwrite(buf, 1, 2, fp);
    }
    static void avi_write_cc(FILE *fp, const char *cc)
    {
	fwrite(cc, 1, 4, fp);
    }
    static void avi_patch_size(FILE *fp, long size_pos, uint32_t size)
    {
	long save = ftell(fp);
	fseek(fp, size_pos, SEEK_SET);
	avi_write_u32(fp, size);
	fseek(fp, save, SEEK_SET);
    }

    static int write_mjpeg_avi(const std::vector<std::vector<unsigned char> > &frames, int width, int height, int fps, const char *out_path)
    {
	FILE *fp = fopen(out_path, "wb");
	if (!fp) return -1;
	uint32_t num_frames = (uint32_t)frames.size();
	uint32_t usec_per_frame = (fps > 0) ? (1000000u / (uint32_t)fps) : 41667u;
	uint32_t max_frame_bytes = 0;
	for (size_t i = 0; i < frames.size(); ++i) {
	    if ((uint32_t)frames[i].size() > max_frame_bytes) max_frame_bytes = (uint32_t)frames[i].size();
	}
	avi_write_cc(fp, "RIFF");
	long riff_size_pos = ftell(fp);
	avi_write_u32(fp, 0);
	avi_write_cc(fp, "AVI ");
	avi_write_cc(fp, "LIST");
	long hdrl_size_pos = ftell(fp);
	avi_write_u32(fp, 0);
	long hdrl_start = ftell(fp);
	avi_write_cc(fp, "hdrl");
	avi_write_cc(fp, "avih");
	avi_write_u32(fp, 56);
	avi_write_u32(fp, usec_per_frame);
	avi_write_u32(fp, max_frame_bytes * (uint32_t)fps);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 0x10);
	avi_write_u32(fp, num_frames);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 1);
	avi_write_u32(fp, max_frame_bytes);
	avi_write_u32(fp, (uint32_t)width);
	avi_write_u32(fp, (uint32_t)height);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 0);
	avi_write_cc(fp, "LIST");
	long strl_size_pos = ftell(fp);
	avi_write_u32(fp, 0);
	long strl_start = ftell(fp);
	avi_write_cc(fp, "strl");
	avi_write_cc(fp, "strh");
	avi_write_u32(fp, 56);
	avi_write_cc(fp, "vids");
	avi_write_cc(fp, "MJPG");
	avi_write_u32(fp, 0);
	avi_write_u16(fp, 0);
	avi_write_u16(fp, 0);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 1);
	avi_write_u32(fp, (uint32_t)fps);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, num_frames);
	avi_write_u32(fp, max_frame_bytes);
	avi_write_u32(fp, (uint32_t)-1);
	avi_write_u32(fp, 0);
	avi_write_u16(fp, 0);
	avi_write_u16(fp, 0);
	avi_write_u16(fp, (uint16_t)width);
	avi_write_u16(fp, (uint16_t)height);
	avi_write_cc(fp, "strf");
	avi_write_u32(fp, 40);
	avi_write_u32(fp, 40);
	avi_write_u32(fp, (uint32_t)width);
	avi_write_u32(fp, (uint32_t)height);
	avi_write_u16(fp, 1);
	avi_write_u16(fp, 24);
	avi_write_cc(fp, "MJPG");
	avi_write_u32(fp, (uint32_t)(width * height * 3));
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 0);
	avi_write_u32(fp, 0);
	long strl_end = ftell(fp);
	avi_patch_size(fp, strl_size_pos, (uint32_t)(strl_end - strl_start));
	long hdrl_end = ftell(fp);
	avi_patch_size(fp, hdrl_size_pos, (uint32_t)(hdrl_end - hdrl_start));
	avi_write_cc(fp, "LIST");
	long movi_size_pos = ftell(fp);
	avi_write_u32(fp, 0);
	long movi_start = ftell(fp);
	avi_write_cc(fp, "movi");
	std::vector<uint32_t> frame_offsets;
	std::vector<uint32_t> frame_sizes;
	for (size_t i = 0; i < frames.size(); ++i) {
	    const std::vector<unsigned char> &jpeg = frames[i];
	    uint32_t data_size = (uint32_t)jpeg.size();
	    uint32_t padded = (data_size + 1) & ~1u;
	    frame_offsets.push_back((uint32_t)(ftell(fp) - movi_start - 4));
	    avi_write_cc(fp, "00dc");
	    avi_write_u32(fp, data_size);
	    fwrite(jpeg.data(), 1, data_size, fp);
	    if (padded != data_size) fputc(0, fp);
	    frame_sizes.push_back(data_size);
	}
	long movi_end = ftell(fp);
	avi_patch_size(fp, movi_size_pos, (uint32_t)(movi_end - movi_start));
	avi_write_cc(fp, "idx1");
	avi_write_u32(fp, num_frames * 16u);
	for (uint32_t i = 0; i < num_frames; ++i) {
	    avi_write_cc(fp, "00dc");
	    avi_write_u32(fp, 0x10);
	    avi_write_u32(fp, frame_offsets[i]);
	    avi_write_u32(fp, frame_sizes[i]);
	}
	long file_end = ftell(fp);
	avi_patch_size(fp, riff_size_pos, (uint32_t)(file_end - 8));
	fclose(fp);
	return 0;
    }

    int
    icv_anim_write(const icv_anim_t *anim, const char *filename)
    {
	if (!anim || !filename || anim->frames.empty()) return -1;

	// Recalculate max width/height if not explicitly set
	uint32_t w = anim->width;
	uint32_t h = anim->height;
	if (w == 0 || h == 0) {
	    for (size_t i = 0; i < anim->frames.size(); ++i) {
		w = std::max(w, (uint32_t)anim->frames[i].img->width);
		h = std::max(h, (uint32_t)anim->frames[i].img->height);
	    }
	}

	if (anim->format == ICV_ANIM_APNG) {
	    apngmini::Animation aapng;
	    aapng.canvas_width = w;
	    aapng.canvas_height = h;
	    aapng.num_plays = 0;

	    for (size_t i = 0; i < anim->frames.size(); ++i) {
		aapng.frames.push_back(icv_to_apng(anim->frames[i].img, anim->frames[i].delay_usec));
	    }

	    if (!apngmini::write_file(filename, aapng, 9)) {
		return -1;
	    }
	    return 0;
	} else if (anim->format == ICV_ANIM_MJPG) {
	    std::vector<std::vector<unsigned char> > jpegs;

	    for (size_t i = 0; i < anim->frames.size(); ++i) {
		unsigned char *jpg_data = NULL;
		size_t jpg_size = 0;

		// Write image straight to memory buffer
		if (icv_write_mem(anim->frames[i].img, &jpg_data, &jpg_size, BU_MIME_IMAGE_JPEG) != 0) {
		    // Free any successfully compressed frames before returning on failure
		    return -1;
		}

		// Push it onto the output vectors
		std::vector<unsigned char> data(jpg_data, jpg_data + jpg_size);

		// Clean up the dynamically allocated frame buffer
		bu_free(jpg_data, "icv_write_mem buffer");

		jpegs.push_back(data);
	    }

	    // Assemble the AVI container using the in-memory JPEGs
	    return write_mjpeg_avi(jpegs, w, h, anim->fps, filename);
	}
	return -1;
    }

    int
    icv_anim_add_frame(icv_anim_t *anim, const icv_image_t *img)
    {
	if (!anim || !img) return -1;
	icv_anim_frame f;
	f.img = icv_clone(img);
	if (!f.img) return -1;
	f.delay_usec = 1000000u / (anim->fps > 0 ? anim->fps : 10);
	anim->frames.push_back(f);
	return 0;
    }

    int
    icv_anim_insert_frame(icv_anim_t *anim, size_t index, const icv_image_t *img)
    {
	if (!anim || !img || index > anim->frames.size()) return -1;
	icv_anim_frame f;
	f.img = icv_clone(img);
	if (!f.img) return -1;
	f.delay_usec = 1000000u / (anim->fps > 0 ? anim->fps : 10);
	anim->frames.insert(anim->frames.begin() + index, f);
	return 0;
    }

    int
    icv_anim_replace_frame(icv_anim_t *anim, size_t index, const icv_image_t *img)
    {
	if (!anim || !img || index >= anim->frames.size()) return -1;
	icv_image_t *clone = icv_clone(img);
	if (!clone) return -1;
	if (anim->frames[index].img) {
	    icv_destroy(anim->frames[index].img);
	}
	icv_anim_frame f;
	f.img = clone;
	// keep old delay
	f.delay_usec = anim->frames[index].delay_usec;
	anim->frames[index] = f;
	return 0;
    }

    int
    icv_anim_remove_frame(icv_anim_t *anim, size_t index)
    {
	if (!anim || index >= anim->frames.size()) return -1;
	if (anim->frames[index].img) {
	    icv_destroy(anim->frames[index].img);
	}
	anim->frames.erase(anim->frames.begin() + index);
	return 0;
    }

    icv_image_t *
    icv_anim_get_frame(const icv_anim_t *anim, size_t index)
    {
	if (!anim || index >= anim->frames.size()) return NULL;
	icv_image_t *src = anim->frames[index].img;
	return icv_clone(src);
    }

} /* extern "C" */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
