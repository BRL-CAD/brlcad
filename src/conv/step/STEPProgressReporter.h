/*               S T E P P R O G R E S S R E P O R T E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPPROGRESSREPORTER_H
#define CONV_STEP_STEPPROGRESSREPORTER_H

#include "STEPWrapper.h"

#include "bu/log.h"

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

#ifndef _WIN32
#  include <sys/resource.h>
#endif

/** Emit an alive/progress line independently of parser and geometry work.
 * A separate reporter is important for long individual pullback jobs, which
 * cannot promise to return to an outer conversion loop at regular intervals. */
class STEPProgressReporter
{
public:
    explicit STEPProgressReporter(const STEPWrapper &source,
	std::chrono::seconds interval = std::chrono::seconds(15))
	: wrapper(source), report_interval(interval), started(std::chrono::steady_clock::now()),
	  cpu_started(std::clock()), last_reported(started), cpu_last(cpu_started),
	  stopping(false), worker(&STEPProgressReporter::run, this)
    {
    }

    ~STEPProgressReporter()
    {
	{
	    std::lock_guard<std::mutex> guard(lock);
	    stopping = true;
	}
	wakeup.notify_all();
	if (worker.joinable()) worker.join();
    }

    STEPProgressReporter(const STEPProgressReporter &) = delete;
    STEPProgressReporter &operator=(const STEPProgressReporter &) = delete;

private:
    void run()
    {
	std::unique_lock<std::mutex> guard(lock);
	while (!wakeup.wait_for(guard, report_interval, [this]() { return stopping; })) {
	    guard.unlock();
	    const brlcad::step::ImportProgress progress = wrapper.Progress();
	    const std::chrono::steady_clock::time_point now =
		std::chrono::steady_clock::now();
	    const std::chrono::seconds elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		now - started);
	    if (!progress.phase.empty()) {
		std::ostringstream message;
		message << "STEP progress: elapsed=" << elapsed.count() << "s phase=\""
		    << progress.phase << '"';
		if (progress.total) {
		    const double percent = 100.0 * static_cast<double>(progress.completed) /
			static_cast<double>(progress.total);
		    message << ' ' << progress.completed << '/' << progress.total
			<< " (" << std::fixed << std::setprecision(1) << percent << "%)";
		} else if (progress.completed) {
		    message << " completed=" << progress.completed;
		}
		if (progress.current_entity_id > 0)
		    message << " entity=#" << progress.current_entity_id;
		if (!progress.secondary_label.empty()) {
		    message << ' ' << progress.secondary_label << '='
			<< progress.secondary_completed;
		    if (progress.secondary_total)
			message << '/' << progress.secondary_total;
		}
		if (!progress.detail.empty())
		    message << ' ' << progress.detail;
		const double wall_seconds = static_cast<double>(elapsed.count());
		const std::clock_t cpu_now = std::clock();
		const double cpu_seconds = static_cast<double>(cpu_now - cpu_started) /
		    static_cast<double>(CLOCKS_PER_SEC);
		const double interval_seconds = std::chrono::duration<double>(
		    now - last_reported).count();
		const double interval_cpu_seconds = static_cast<double>(cpu_now - cpu_last) /
		    static_cast<double>(CLOCKS_PER_SEC);
		if (wall_seconds > 0.0 && interval_seconds > 0.0)
		    message << " cpu=" << std::fixed << std::setprecision(1)
			<< cpu_seconds << "s interval-cores=" << std::setprecision(2)
			<< (interval_cpu_seconds / interval_seconds)
			<< " average-cores=" << (cpu_seconds / wall_seconds);
		const size_t peak_rss = peakResidentBytes();
		if (peak_rss)
		    message << " peak-rss=" << std::setprecision(1)
			<< (static_cast<double>(peak_rss) / (1024.0 * 1024.0))
			<< "MiB";
		if (progress.geometry_runnable_capacity) {
		    const uint64_t runnable = progress.geometry_workers_active +
			progress.geometry_jobs_queued +
			progress.geometry_jobs_materializing;
		    message << " scheduler={active="
			<< progress.geometry_workers_active
			<< ",helpers=" << progress.geometry_helpers_active
			<< ",queued=" << progress.geometry_jobs_queued
			<< ",ready=" << progress.geometry_jobs_ready
			<< ",spooled=" << progress.geometry_jobs_spooled
			<< ",finished=" << progress.geometry_jobs_finished
			<< ",materializing=" << progress.geometry_jobs_materializing
			<< ",runnable=" << runnable << '/'
			<< progress.geometry_runnable_capacity
			<< ",in-flight=" << progress.geometry_jobs_in_flight
			<< ",ready-memory=" << std::setprecision(1)
			<< (static_cast<double>(progress.geometry_ready_bytes) /
			    (1024.0 * 1024.0)) << '/'
			<< (static_cast<double>(progress.geometry_ready_byte_budget) /
			    (1024.0 * 1024.0)) << "MiB}";
		}
		message << '\n';
		bu_log("%s", message.str().c_str());
	    }
	    last_reported = now;
	    cpu_last = std::clock();
	    guard.lock();
	}
    }

    static size_t peakResidentBytes()
    {
#ifndef _WIN32
	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#  ifdef __APPLE__
	return static_cast<size_t>(usage.ru_maxrss);
#  else
	return static_cast<size_t>(usage.ru_maxrss) * 1024U;
#  endif
#else
	return 0;
#endif
    }

    const STEPWrapper &wrapper;
    const std::chrono::seconds report_interval;
    const std::chrono::steady_clock::time_point started;
    const std::clock_t cpu_started;
    std::chrono::steady_clock::time_point last_reported;
    std::clock_t cpu_last;
    std::mutex lock;
    std::condition_variable wakeup;
    bool stopping;
    std::thread worker;
};

#endif /* CONV_STEP_STEPPROGRESSREPORTER_H */
