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
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

/** Emit an alive/progress line independently of parser and geometry work.
 * A separate reporter is important for long individual pullback jobs, which
 * cannot promise to return to an outer conversion loop at regular intervals. */
class STEPProgressReporter
{
public:
    explicit STEPProgressReporter(const STEPWrapper &source,
	std::chrono::seconds interval = std::chrono::seconds(15))
	: wrapper(source), report_interval(interval), started(std::chrono::steady_clock::now()),
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
	    const std::chrono::seconds elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - started);
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
		if (!progress.secondary_label.empty())
		    message << ' ' << progress.secondary_label << '='
			<< progress.secondary_completed;
		if (!progress.detail.empty())
		    message << ' ' << progress.detail;
		message << '\n';
		bu_log("%s", message.str().c_str());
	    }
	    guard.lock();
	}
    }

    const STEPWrapper &wrapper;
    const std::chrono::seconds report_interval;
    const std::chrono::steady_clock::time_point started;
    std::mutex lock;
    std::condition_variable wakeup;
    bool stopping;
    std::thread worker;
};

#endif /* CONV_STEP_STEPPROGRESSREPORTER_H */
