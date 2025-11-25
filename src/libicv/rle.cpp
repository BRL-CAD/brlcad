/*                           R L E . C P P
 * BRL-CAD
 *
 * Hardened Utah RLE read/write implementation for libicv using the internal
 * clean-room codec (rle.hpp). Legacy utahrle code paths removed.
 *
 * Security / Robustness:
 *   - Uses brlcad::rle::MAX_* limits from rle.hpp.
 *   - Safe multiplication guards.
 *   - Controlled background detection with unique color cap & early exits.
 *   - Error propagation via rle::Error.
 *   - Deterministic comments (timestamp/software/format only).
 */

#include "common.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "bu/log.h"
#include "bu/malloc.h"
extern "C" {
#include "icv_private.h"
}
#include "rle.hpp"   /* brlcad::rle */

/* -------------------- Local Helpers -------------------- */
namespace {

inline bool safe_mul_u64(uint64_t a, uint64_t b, uint64_t limit, uint64_t &out) {
    if (!a || !b) { out = 0; return true; }
    if (a > limit / b) return false;
    out = a * b;
    return true;
}

inline uint8_t dbl_to_u8(double v) {
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return static_cast<uint8_t>(lrint(v * 255.0));
}
inline double u8_to_dbl(uint8_t v) { return double(v) / 255.0; }

bool icv_to_u8_interleaved(const icv_image_t *img, std::vector<uint8_t> &buf) {
    if (!img || !img->data || img->channels < 3) return false;
    uint64_t npix;
    if (!safe_mul_u64(img->width, img->height, brlcad::rle::MAX_PIXELS, npix)) return false;
    if (npix == 0) return false;

    uint64_t total_bytes;
    if (!safe_mul_u64(npix, 3, brlcad::rle::MAX_ALLOC_BYTES, total_bytes)) return false;

    try {
        buf.resize(static_cast<size_t>(npix) * 3);
    } catch (...) { return false; }

    const double *src = img->data;
    for (uint64_t i = 0; i < npix; ++i) {
        buf[3*i + 0] = dbl_to_u8(src[3*i + 0]);
        buf[3*i + 1] = dbl_to_u8(src[3*i + 1]);
        buf[3*i + 2] = dbl_to_u8(src[3*i + 2]);
    }
    return true;
}

bool u8_interleaved_to_icv(const std::vector<uint8_t> &buf, size_t w, size_t h, icv_image_t *out) {
    if (!out || !w || !h) return false;
    uint64_t npix;
    if (!safe_mul_u64(w, h, brlcad::rle::MAX_PIXELS, npix)) return false;
    if (buf.size() < npix * 3) return false;

    uint64_t elems;
    if (!safe_mul_u64(npix, 3, brlcad::rle::MAX_ALLOC_BYTES / sizeof(double), elems)) return false;

    double *data = static_cast<double *>(
        bu_calloc(static_cast<size_t>(npix) * 3, sizeof(double), "rle_icv_data"));
    if (!data) return false;

    for (uint64_t i = 0; i < npix; ++i) {
        data[3*i + 0] = u8_to_dbl(buf[3*i + 0]);
        data[3*i + 1] = u8_to_dbl(buf[3*i + 1]);
        data[3*i + 2] = u8_to_dbl(buf[3*i + 2]);
    }

    out->width = w;
    out->height = h;
    out->channels = 3;
    out->color_space = ICV_COLOR_SPACE_RGB;
    out->data = data;
    out->magic = ICV_IMAGE_MAGIC;
    return true;
}

/* Background detection (bounded) */
struct BackgroundDecision {
    std::vector<uint8_t> color;
    brlcad::rle::Encoder::BackgroundMode mode;
};

BackgroundDecision detect_background(const std::vector<uint8_t> &rgb, size_t w, size_t h) {
    static constexpr size_t UNIQUE_CAP = 65536;
    static constexpr double CLEAR_THRESH = 0.50;
    static constexpr double OVERLAY_THRESH = 0.20;

    BackgroundDecision bd;
    bd.mode = brlcad::rle::Encoder::BG_SAVE_ALL;

    uint64_t npix;
    if (!safe_mul_u64(w, h, brlcad::rle::MAX_PIXELS, npix)) return bd;
    if (!npix || rgb.size() < npix * 3) return bd;

    uint64_t clear_needed   = uint64_t(npix * CLEAR_THRESH);
    uint64_t overlay_needed = uint64_t(npix * OVERLAY_THRESH);

    std::unordered_map<uint32_t, uint64_t> freq;
    freq.reserve(std::min<uint64_t>(npix, 4096));

    uint64_t maxCount = 0;
    uint32_t maxKey = 0;

    for (uint64_t i = 0; i < npix; ++i) {
        uint32_t key = (uint32_t(rgb[3*i + 0]) << 16) |
                       (uint32_t(rgb[3*i + 1]) << 8)  |
                       uint32_t(rgb[3*i + 2]);

        auto it = freq.find(key);
        if (it == freq.end()) {
            if (freq.size() >= UNIQUE_CAP) return bd; /* give up optimization */
            it = freq.emplace(key, 1).first;
        } else {
            ++(it->second);
        }

        if (it->second > maxCount) {
            maxCount = it->second;
            maxKey = key;
            if (maxCount >= clear_needed) {
                bd.mode = brlcad::rle::Encoder::BG_CLEAR;
                bd.color = { uint8_t((maxKey >> 16) & 0xFF),
                             uint8_t((maxKey >> 8)  & 0xFF),
                             uint8_t(maxKey & 0xFF) };
                return bd;
            } else if (maxCount >= overlay_needed && bd.mode != brlcad::rle::Encoder::BG_OVERLAY) {
                bd.mode = brlcad::rle::Encoder::BG_OVERLAY;
                bd.color = { uint8_t((maxKey >> 16) & 0xFF),
                             uint8_t((maxKey >> 8)  & 0xFF),
                             uint8_t(maxKey & 0xFF) };
            }
        }
    }

    if (bd.mode == brlcad::rle::Encoder::BG_SAVE_ALL && maxCount >= overlay_needed) {
        bd.mode = brlcad::rle::Encoder::BG_OVERLAY;
        bd.color = { uint8_t((maxKey >> 16) & 0xFF),
                     uint8_t((maxKey >> 8)  & 0xFF),
                     uint8_t(maxKey & 0xFF) };
    } else if (bd.mode == brlcad::rle::Encoder::BG_SAVE_ALL && maxCount >= clear_needed) {
        bd.mode = brlcad::rle::Encoder::BG_CLEAR;
        bd.color = { uint8_t((maxKey >> 16) & 0xFF),
                     uint8_t((maxKey >> 8)  & 0xFF),
                     uint8_t(maxKey & 0xFF) };
    }

    return bd;
}

/* Deterministic comments */
std::vector<std::string> build_comments() {
    std::vector<std::string> comments;
#if RLE_TIMESTAMP_ENABLED
    comments.emplace_back(std::string("CREATED=") + brlcad::rle::rle_utc_timestamp());
#endif
    comments.emplace_back("SOFTWARE=BRL-CAD libicv");
    comments.emplace_back("FORMAT=UtahRLE");
    return comments;
}

void log_rle_error(const char *context, brlcad::rle::Error e) {
    if (e == brlcad::rle::Error::OK) return;
    bu_log("%s: RLE error: %s\n", context, brlcad::rle::error_string(e));
}

} /* anonymous namespace */

/* -------------------- Public API -------------------- */

extern "C" int
rle_write(icv_image_t *bif, FILE *fp)
{
    if (!bif || !fp) {
        bu_log("rle_write: null image or file pointer\n");
        return BRLCAD_ERROR;
    }
    if (bif->channels < 3) {
        bu_log("rle_write: image must have at least 3 channels (RGB)\n");
        return BRLCAD_ERROR;
    }
    if (bif->width > brlcad::rle::MAX_DIM || bif->height > brlcad::rle::MAX_DIM) {
        bu_log("rle_write: dimensions exceed maximum (%u x %u)\n",
               brlcad::rle::MAX_DIM, brlcad::rle::MAX_DIM);
        return BRLCAD_ERROR;
    }

    std::vector<uint8_t> rgb;
    if (!icv_to_u8_interleaved(bif, rgb)) {
        bu_log("rle_write: conversion to 8-bit buffer failed\n");
        return BRLCAD_ERROR;
    }

    BackgroundDecision bgd = detect_background(rgb, bif->width, bif->height);
    std::vector<std::string> comments = build_comments();

    brlcad::rle::Error err;
    bool ok = brlcad::rle::write_rgb(fp,
                                     rgb.data(),
                                     static_cast<uint32_t>(bif->width),
                                     static_cast<uint32_t>(bif->height),
                                     comments,
                                     bgd.color,
                                     false, /* no alpha */
                                     bgd.mode,
                                     err);

    if (!ok || err != brlcad::rle::Error::OK) {
        log_rle_error("rle_write", err);
        return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

extern "C" icv_image_t*
rle_read(FILE *fp)
{
    if (!fp) {
        bu_log("rle_read: null file pointer\n");
        return NULL;
    }

    std::vector<uint8_t> rgb;
    uint32_t width = 0, height = 0;
    bool has_alpha = false;
    std::vector<std::string> comments;
    brlcad::rle::Error err;

    bool ok = brlcad::rle::read_rgb(fp, rgb, width, height,
                                    &has_alpha, &comments, err);

    if (!ok || err != brlcad::rle::Error::OK) {
        log_rle_error("rle_read", err);
        /* Do not fclose(fp); caller owns the FILE* */
        return NULL;
    }

    if (width > brlcad::rle::MAX_DIM || height > brlcad::rle::MAX_DIM) {
        bu_log("rle_read: dimensions exceed maximum (%u x %u)\n",
               brlcad::rle::MAX_DIM, brlcad::rle::MAX_DIM);
        return NULL;
    }

    icv_image_t *img = NULL;
    BU_ALLOC(img, struct icv_image);
    ICV_IMAGE_INIT(img);

    if (!u8_interleaved_to_icv(rgb, static_cast<size_t>(width),
                               static_cast<size_t>(height), img)) {
        bu_log("rle_read: buffer to icv image conversion failed\n");
        bu_free(img, "icv_image");
        return NULL;
    }

    return img;
}

/* Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
