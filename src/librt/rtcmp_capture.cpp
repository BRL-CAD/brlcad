/*                   R T C M P _ C A P T U R E . C P P
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

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <iomanip>
#include <limits>
#include <locale>
#include <mutex>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "bu/str.h"
#include "raytrace.h"
#include "librt_private.h"

namespace {

constexpr size_t FLUSH_BYTES = 4 * 1024 * 1024;
constexpr size_t MAX_QUEUED_BYTES = 8 * 1024 * 1024;
constexpr std::chrono::seconds FLUSH_DELAY(2);

void
append_xyz(std::ostream &out, const fastf_t *values)
{
    out << "{\"X\":\"" << values[X] << "\",\"Y\":\"" << values[Y]
        << "\",\"Z\":\"" << values[Z] << "\"}";
}

void
append_json_string(std::ostream &out, const char *value)
{
    static const char HEX[] = "0123456789abcdef";

    out << '"';
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
        switch (*p) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            default:
                if (*p < 0x20) {
                    out << "\\u00" << HEX[*p >> 4] << HEX[*p & 0x0f];
                } else {
                    out << static_cast<char>(*p);
                }
        }
    }
    out << '"';
}

std::string
format_shot(const struct application *ap, const struct partition *head, const std::string *segments = nullptr)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<fastf_t>::max_digits10);
    out << "{\"partitions\":[";

    bool first = true;
    for (const struct partition *part = head->pt_forw; part != head; part = part->pt_forw) {
        struct hit in = *part->pt_inhit;
        struct hit out_hit = *part->pt_outhit;
        vect_t in_norm, out_norm;

        VJOIN1(in.hit_point, ap->a_ray.r_pt, in.hit_dist, ap->a_ray.r_dir);
        VJOIN1(out_hit.hit_point, ap->a_ray.r_pt, out_hit.hit_dist, ap->a_ray.r_dir);
        RT_HIT_NORMAL(in_norm, &in, part->pt_inseg->seg_stp, ap->a_ray, 0);
        RT_HIT_NORMAL(out_norm, &out_hit, part->pt_outseg->seg_stp, ap->a_ray, 0);

        if (!first) out << ',';
        first = false;
        out << "{\"in_dist\":\"" << in.hit_dist << "\",\"in_norm\":";
        append_xyz(out, in_norm);
        out << ",\"in_pt\":";
        append_xyz(out, in.hit_point);
        out << ",\"out_dist\":\"" << out_hit.hit_dist << "\",\"out_norm\":";
        append_xyz(out, out_norm);
        out << ",\"out_pt\":";
        append_xyz(out, out_hit.hit_point);
        out << ",\"region\":";
        append_json_string(out, part->pt_regionp->reg_name);
        out << '}';
    }

    out << "]";
    if (segments) out << ",\"segments\":[" << *segments << "]";
    out << ",\"ray_dir\":";
    append_xyz(out, ap->a_ray.r_dir);
    out << ",\"ray_pt\":";
    append_xyz(out, ap->a_ray.r_pt);
    out << "}\n";
    return out.str();
}

struct CaptureState {
    std::string segments;
};

void
append_segment(CaptureState &state, const struct application *ap, const struct seg *segp)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<fastf_t>::max_digits10);
    struct hit in = segp->seg_in;
    struct hit out_hit = segp->seg_out;
    struct soltab *stp = segp->seg_stp;
    const bool geometry_valid = stp && std::isfinite(in.hit_dist) &&
        std::isfinite(out_hit.hit_dist) && in.hit_dist <= out_hit.hit_dist;
    vect_t in_norm, out_norm;
    if (geometry_valid) {
        VJOIN1(in.hit_point, ap->a_ray.r_pt, in.hit_dist, ap->a_ray.r_dir);
        VJOIN1(out_hit.hit_point, ap->a_ray.r_pt, out_hit.hit_dist, ap->a_ray.r_dir);
        RT_HIT_NORMAL(in_norm, &in, stp, ap->a_ray, 0);
        RT_HIT_NORMAL(out_norm, &out_hit, stp, ap->a_ray, 0);
    }

    if (!state.segments.empty()) out << ',';
    out << "{\"primitive\":";
    if (stp && stp->st_path.magic == DB_FULL_PATH_MAGIC) {
        char *path = db_path_to_string(&stp->st_path);
        append_json_string(out, path);
        bu_free(path, "primitive path");
    } else {
        append_json_string(out, stp ? stp->st_name : "unnamed");
    }
    out << ",\"transform\":[";
    const mat_t identity = MAT_INIT_IDN;
    const fastf_t *mat = stp && stp->st_matp ? stp->st_matp : identity;
    for (size_t i = 0; i < ELEMENTS_PER_MAT; ++i) {
        if (i) out << ',';
        out << '\"' << mat[i] << '\"';
    }
    out << "],\"in_dist\":\"" << in.hit_dist << "\",\"in_norm\":";
    if (geometry_valid) append_xyz(out, in_norm);
    else out << "null";
    out << ",\"in_pt\":";
    if (geometry_valid) append_xyz(out, in.hit_point);
    else out << "null";
    out << ",\"in_surfno\":" << in.hit_surfno;
    out << ",\"out_dist\":\"" << out_hit.hit_dist << "\",\"out_norm\":";
    if (geometry_valid) append_xyz(out, out_norm);
    else out << "null";
    out << ",\"out_pt\":";
    if (geometry_valid) append_xyz(out, out_hit.hit_point);
    else out << "null";
    out << ",\"out_surfno\":" << out_hit.hit_surfno << '}';
    state.segments += out.str();
}

class CaptureWriter {
public:
    CaptureWriter()
    {
        const char *path = std::getenv("LIBRT_RTCMP_FILE");
        if (!path || !*path) {
            bu_log("RT_DEBUG_RTCMP requires LIBRT_RTCMP_FILE\n");
            return;
        }

        file = std::fopen(path, "ab");
        if (!file) {
            bu_log("RT_DEBUG_RTCMP: cannot append to %s: %s\n", path, std::strerror(errno));
            return;
        }

        const char *primitive_setting = std::getenv("LIBRT_RTCMP_PRIMITIVES");
        const char *skip_setting = std::getenv("LIBRT_RTCMP_SKIP_MISSES");
        primitive_mode = primitive_setting && bu_strcmp(primitive_setting, "1") == 0;
        skip_misses = skip_setting && bu_strcmp(skip_setting, "1") == 0;
        enabled.store(true, std::memory_order_release);
        try {
            worker = std::thread(&CaptureWriter::write, this);
        } catch (const std::exception &e) {
            enabled.store(false, std::memory_order_release);
            bu_log("RT_DEBUG_RTCMP: cannot start writer: %s\n", e.what());
            std::fclose(file);
            file = nullptr;
        }
    }

    ~CaptureWriter()
    {
        if (worker.joinable()) {
            {
                std::lock_guard<std::mutex> guard(mutex);
                stopping = true;
            }
            ready.notify_all();
            space.notify_all();
            worker.join();
        }
        if (file && std::fclose(file) != 0)
            bu_log("RT_DEBUG_RTCMP: closing output failed\n");
    }

    bool active() const { return enabled.load(std::memory_order_acquire); }
    bool primitives() const { return primitive_mode; }
    bool omit_misses() const { return skip_misses; }

    int flush()
    {
        std::unique_lock<std::mutex> guard(mutex);
        const size_t target = submitted;
        if (completed < target) {
            flush_requested = true;
            ready.notify_one();
        }
        drained.wait(guard, [&] { return completed >= target || !active(); });
        return active() ? 0 : -1;
    }

    void submit(std::string line)
    {
        std::unique_lock<std::mutex> guard(mutex);
        space.wait(guard, [&] {
            return !active() || queued.empty() ||
                (line.size() <= MAX_QUEUED_BYTES &&
                 queued_bytes <= MAX_QUEUED_BYTES - line.size());
        });
        if (!active()) return;

        if (queued.empty()) deadline = std::chrono::steady_clock::now() + FLUSH_DELAY;
        queued_bytes += line.size();
        queued.push_back(std::move(line));
        ++submitted;
        if (queued_bytes >= FLUSH_BYTES) ready.notify_one();
        else if (queued.size() == 1) ready.notify_one();
    }

    void disable(const char *reason)
    {
        if (!enabled.exchange(false, std::memory_order_acq_rel)) return;
        bu_log("RT_DEBUG_RTCMP: %s; capture disabled\n", reason);
        ready.notify_all();
        space.notify_all();
        drained.notify_all();
    }

private:
    void write()
    {
        for (;;) {
            std::deque<std::string> batch;
            {
                std::unique_lock<std::mutex> guard(mutex);
                ready.wait(guard, [&] { return stopping || !active() || !queued.empty(); });
                if (!stopping && queued_bytes < FLUSH_BYTES)
                    ready.wait_until(guard, deadline, [&] { return stopping || !active() || flush_requested || queued_bytes >= FLUSH_BYTES; });
                if (queued.empty()) {
                    if (stopping || !active()) return;
                    continue;
                }
                batch.swap(queued);
                queued_bytes = 0;
                flush_requested = false;
            }
            space.notify_all();

            for (const std::string &line : batch) {
                if (std::fwrite(line.data(), 1, line.size(), file) != line.size()) {
                    disable("writing output failed");
                    return;
                }
            }
            if (std::fflush(file) != 0) {
                disable("flushing output failed");
                return;
            }
            {
                std::lock_guard<std::mutex> guard(mutex);
                completed += batch.size();
            }
            drained.notify_all();
        }
    }

    std::atomic<bool> enabled{false};
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable space;
    std::condition_variable drained;
    std::deque<std::string> queued;
    size_t queued_bytes = 0;
    size_t submitted = 0;
    size_t completed = 0;
    bool primitive_mode = false;
    bool skip_misses = false;
    bool flush_requested = false;
    std::chrono::steady_clock::time_point deadline;
    std::thread worker;
    FILE *file = nullptr;
    bool stopping = false;
};

CaptureWriter &
writer()
{
    static CaptureWriter instance;
    return instance;
}

} // namespace

extern "C" void *
_rt_rtcmp_capture_begin(void)
{
    const char *mode = std::getenv("LIBRT_RTCMP_PRIMITIVES");
    if (!mode || bu_strcmp(mode, "1") != 0) return nullptr;
    try {
        CaptureWriter &capture = writer();
        return capture.active() && capture.primitives() ? new CaptureState : nullptr;
    } catch (const std::exception &e) {
        writer().disable(e.what());
        return nullptr;
    }
}

extern "C" void
_rt_rtcmp_capture_segment(void *state, const struct application *ap, const struct seg *segp)
{
    if (!state) return;
    try {
        append_segment(*static_cast<CaptureState *>(state), ap, segp);
    } catch (const std::exception &e) {
        writer().disable(e.what());
    }
}

extern "C" void
_rt_rtcmp_capture_finish(void *state, const struct application *ap, const struct partition *parts)
{
    if (!state) return;
    std::unique_ptr<CaptureState> shot(static_cast<CaptureState *>(state));
    try {
        CaptureWriter &capture = writer();
        if (!capture.active()) return;
        if (capture.omit_misses() && shot->segments.empty() && parts->pt_forw == parts) return;
        capture.submit(format_shot(ap, parts, &shot->segments));
    } catch (const std::exception &e) {
        writer().disable(e.what());
    }
}

extern "C" int
rt_rtcmp_capture_flush(void)
{
    return writer().flush();
}

extern "C" void
_rt_rtcmp_capture(const struct application *ap, const struct partition *parts)
{
    if (parts->pt_forw == parts) return;

    try {
        CaptureWriter &capture = writer();
        if (capture.active()) capture.submit(format_shot(ap, parts));
    } catch (const std::exception &e) {
        writer().disable(e.what());
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
