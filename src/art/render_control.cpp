/*                          R E N D E R _ C O N T R O L . C P P
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
#include <chrono>
#include <csignal>

#include "render_control.h"
#include "tile.h"
#include "bu/log.h"

namespace {
// Lock-free atomic stores are safe in a signal handler and visible to any
// renderer thread that polls the controller's abort switch.
static_assert(std::atomic<bool>::is_always_lock_free,
    "Render cancellation requires lock-free atomic flags");
std::atomic<bool> render_interrupted{false};

void
interrupt_render(int)
{
    render_interrupted.store(true, std::memory_order_relaxed);
}
}

ArtRenderControl::ArtRenderControl(const ArtTileCallback& callback, int frame, bool progress)
    : m_callback(callback), m_frame(frame), m_progress(progress)
{
    render_interrupted.store(false, std::memory_order_relaxed);
    m_previous = std::signal(SIGINT, interrupt_render);
    if (m_previous == SIG_ERR)
	bu_log("art: cannot install render interrupt handler\n");
}

ArtRenderControl::~ArtRenderControl()
{
    restore();
}

bool
ArtRenderControl::restore()
{
    if (m_previous == SIG_ERR)
	return true;
    const SignalHandler previous = m_previous;
    m_previous = SIG_ERR;
    if (std::signal(SIGINT, previous) == SIG_ERR) {
	bu_log("art: cannot restore render interrupt handler\n");
	return false;
    }
    return true;
}

bool
ArtRenderControl::cancelled() const
{
    return render_interrupted.load(std::memory_order_relaxed);
}

ArtRenderControl::Status
ArtRenderControl::get_status() const
{
    return cancelled() || m_callback.failed() ? AbortRendering : ContinueRendering;
}

void
ArtRenderControl::on_rendering_begin()
{
    m_started = Clock::now();
    m_last_update = m_started;
    m_last_completed = 0;
    if (m_progress)
	bu_log("art: frame %d rendering (Ctrl-C to cancel)\n", m_frame);
}

void
ArtRenderControl::on_progress()
{
    if (!m_progress)
	return;
    constexpr auto REPORT_INTERVAL = std::chrono::seconds(1);
    const auto now = Clock::now();
    const size_t total = m_callback.total_tiles();
    const size_t completed = m_callback.completed_tiles();
    if (!total || completed >= total || completed == m_last_completed || now - m_last_update < REPORT_INTERVAL)
	return;
    const auto percent = static_cast<unsigned int>(100.0 * static_cast<double>(completed) /
	static_cast<double>(total));
    const double seconds = std::chrono::duration<double>(now - m_started).count();
    bu_log("art: frame %d progress: %zu/%zu tiles (%u%%), %.1fs\n",
	m_frame, completed, total, percent, seconds);
    m_last_update = now;
    m_last_completed = completed;
}

void
ArtRenderControl::complete()
{
    if (!m_progress)
	return;
    const double seconds = std::chrono::duration<double>(Clock::now() - m_started).count();
    bu_log("art: frame %d complete: %zu/%zu tiles (100%%), %.1fs\n", m_frame,
	m_callback.completed_tiles(), m_callback.total_tiles(), seconds);
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
