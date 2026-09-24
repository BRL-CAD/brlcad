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
#include <sstream>
#include <string>
#include <thread>
#include <utility>

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
format_shot(const struct application *ap, const struct partition *head)
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

    out << "],\"ray_dir\":";
    append_xyz(out, ap->a_ray.r_dir);
    out << ",\"ray_pt\":";
    append_xyz(out, ap->a_ray.r_pt);
    out << "}\n";
    return out.str();
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
        if (queued_bytes >= FLUSH_BYTES) ready.notify_one();
        else if (queued.size() == 1) ready.notify_one();
    }

    void disable(const char *reason)
    {
        if (!enabled.exchange(false, std::memory_order_acq_rel)) return;
        bu_log("RT_DEBUG_RTCMP: %s; capture disabled\n", reason);
        ready.notify_all();
        space.notify_all();
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
                    ready.wait_until(guard, deadline, [&] { return stopping || !active() || queued_bytes >= FLUSH_BYTES; });
                if (queued.empty()) {
                    if (stopping || !active()) return;
                    continue;
                }
                batch.swap(queued);
                queued_bytes = 0;
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
        }
    }

    std::atomic<bool> enabled{false};
    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable space;
    std::deque<std::string> queued;
    size_t queued_bytes = 0;
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
