/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef CONV_IGES_RUNTIME_H
#define CONV_IGES_RUNTIME_H

#include "common.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "bu/vls.h"
#include "raytrace.h"

class ON_Brep;

namespace brlcad {
namespace iges {

struct FileCloser {
    void operator()(FILE *file) const noexcept;
};
using File = std::unique_ptr<FILE, FileCloser>;
void close_file(File &file);

class ProgressReporter {
public:
    ProgressReporter();
    ~ProgressReporter();
    ProgressReporter(const ProgressReporter &) = delete;
    ProgressReporter &operator=(const ProgressReporter &) = delete;
    void update(const char *stage, const char *activity, size_t completed = 0,
	size_t total = 0, int64_t entity = 0);

private:
    void run() noexcept;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopped_ = false;
    std::string stage_ = "input";
    std::string activity_ = "reading input";
    size_t completed_ = 0;
    size_t total_ = 0;
    int64_t entity_ = 0;
    std::chrono::steady_clock::time_point started_;
    std::thread worker_;
};

/** Own a temporary sibling of a destination until explicitly published.
 * Destruction never publishes partially written output. */
class StagedFile {
public:
    explicit StagedFile(const std::filesystem::path &destination);
    ~StagedFile();
    StagedFile(const StagedFile &) = delete;
    StagedFile &operator=(const StagedFile &) = delete;
    const std::string &path() const { return path_; }
    void publish();

private:
    std::filesystem::path target_;
    std::filesystem::path directory_;
    std::string path_;
};

/** Reject input/output aliases (including hard links) before opening writers. */
void validate_paths(const std::string &input, const std::vector<std::string> &outputs);

class Internal {
public:
    Internal() { RT_DB_INTERNAL_INIT(&value); }
    ~Internal() { rt_db_free_internal(&value); }
    Internal(const Internal &) = delete;
    Internal &operator=(const Internal &) = delete;
    struct rt_db_internal value;
};

/** Own the destination across a primitive callback, including exceptions. */
std::unique_ptr<ON_Brep> primitive_brep(struct rt_db_internal &internal,
    const struct bn_tol &tolerance);

class Vls {
public:
    Vls() = default;
    ~Vls() { bu_vls_free(&value); }
    Vls(const Vls &) = delete;
    Vls &operator=(const Vls &) = delete;
    struct bu_vls value = BU_VLS_INIT_ZERO;
};

int parse_debug_mask(struct bu_vls *message, size_t argc, const char **argv, void *destination);

} // namespace iges
} // namespace brlcad
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
