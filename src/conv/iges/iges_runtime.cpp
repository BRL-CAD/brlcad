/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_runtime.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "bu/file.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/opt.h"
#include "brep.h"

namespace brlcad {
namespace iges {
namespace {
constexpr unsigned int STAGING_ATTEMPTS = 64;
constexpr std::chrono::seconds PROGRESS_INTERVAL(5);
}

void
FileCloser::operator()(FILE *file) const noexcept
{
    if (file && std::fclose(file))
	bu_log("IGES: could not close file\n");
}

void
close_file(File &file)
{
    if (file && std::fclose(file.release()))
	throw std::runtime_error("cannot close output file");
}

std::unique_ptr<ON_Brep>
primitive_brep(struct rt_db_internal &internal, const struct bn_tol &tolerance)
{
    if (internal.idb_type <= ID_NULL || internal.idb_type > ID_MAXIMUM ||
	!OBJ[internal.idb_type].ft_brep)
	return {};
    // The library contract passes an allocated destination by address;
    // retain ownership even if a callback replaces it before throwing.
    ON_Brep *destination = ON_Brep::New();
    try {
	OBJ[internal.idb_type].ft_brep(&destination, &internal, &tolerance);
    } catch (...) {
	delete destination;
	throw;
    }
    return std::unique_ptr<ON_Brep>(destination);
}

ProgressReporter::ProgressReporter() : started_(std::chrono::steady_clock::now()),
    worker_([this]() { run(); })
{
}

ProgressReporter::~ProgressReporter()
{
    {
	std::lock_guard<std::mutex> lock(mutex_);
	stopped_ = true;
    }
    wake_.notify_one();
    worker_.join();
}

void
ProgressReporter::update(const char *stage, const char *activity,
    size_t completed, size_t total, int64_t entity)
{
    std::lock_guard<std::mutex> lock(mutex_);
    stage_ = stage;
    activity_ = activity;
    completed_ = completed;
    total_ = total;
    entity_ = entity;
}

void
ProgressReporter::run() noexcept
{
    try {
	std::unique_lock<std::mutex> lock(mutex_);
	while (!wake_.wait_for(lock, PROGRESS_INTERVAL, [this]() { return stopped_; })) {
	    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - started_).count();
	    std::ostringstream message;
	    message << "IGES progress: " << elapsed << "s elapsed; " << stage_;
	    if (total_)
		message << ": " << completed_ << '/' << total_ << " source items processed";
	    message << "; " << activity_;
	    if (entity_)
		message << " (D" << entity_ << ')';
	    message << '\n';
	    lock.unlock();
	    bu_log("%s", message.str().c_str());
	    lock.lock();
	}
    } catch (const std::exception &error) {
	/* Reporting must not terminate an otherwise viable conversion. */
	bu_log("IGES: progress reporting stopped: %s\n", error.what());
    }
}

StagedFile::StagedFile(const std::filesystem::path &destination) :
    target_(std::filesystem::weakly_canonical(destination))
{
    const std::string stem = std::string(".iges-output-") + bu_temp_file_name(nullptr, 0);
    for (unsigned int attempt = 0; attempt < STAGING_ATTEMPTS; ++attempt) {
	directory_ = target_.parent_path() / (stem + '-' + std::to_string(attempt));
	path_ = (directory_ / target_.filename()).string();
	std::error_code error;
	if (std::filesystem::create_directory(directory_, error)) {
	    return;
	}
	if (error && error != std::errc::file_exists)
	    throw std::runtime_error("cannot stage output: " + error.message());
    }
    throw std::runtime_error("cannot create a unique output staging directory");
}

StagedFile::~StagedFile()
{
    std::error_code error;
    std::filesystem::remove(path_, error);
    error.clear();
    std::filesystem::remove(directory_, error);
    if (error)
	bu_log("IGES: cannot remove temporary output directory: %s\n", error.message().c_str());
}

void
StagedFile::publish()
{
    std::filesystem::rename(path_, target_);
}

void
validate_paths(const std::string &input, const std::vector<std::string> &outputs)
{
    namespace fs = std::filesystem;
    if (!fs::is_regular_file(input))
	throw std::runtime_error("input must identify a readable regular file");
    std::vector<fs::path> paths = {fs::weakly_canonical(input)};
    for (const auto &output : outputs) {
	if (output.empty())
	    continue;
	const auto path = fs::weakly_canonical(output);
	if (fs::exists(path) && !fs::is_regular_file(path))
	    throw std::runtime_error("output destinations must be regular files");
	for (const auto &other : paths)
	    if (path == other || bu_file_same(path.string().c_str(), other.string().c_str()))
		throw std::runtime_error("input and output destinations must be distinct files");
	paths.push_back(path);
    }
}

int
parse_debug_mask(struct bu_vls *message, size_t argc, const char **argv, void *destination)
{
    BU_OPT_CHECK_ARGV0(message, argc, argv, "debug mask");
    char *end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(argv[0], &end, 16);
    if (errno || argv[0][0] == '-' || end == argv[0] || *end ||
	value > std::numeric_limits<uint32_t>::max()) {
	if (message)
	    bu_vls_printf(message, "invalid hexadecimal debug mask: %s", argv[0]);
	return -1;
    }
    *static_cast<uint32_t *>(destination) = static_cast<uint32_t>(value);
    return 1;
}

} // namespace iges
} // namespace brlcad

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
