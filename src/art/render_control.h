/*                          R E N D E R _ C O N T R O L . H
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

#ifndef ART_RENDER_CONTROL_H
#define ART_RENDER_CONTROL_H

#include "common.h"

#include <chrono>
#include <csignal>
#include "renderer/kernel/rendering/defaultrenderercontroller.h"

class ArtTileCallback;

// One controller owns SIGINT handling for the duration of one render.
class ArtRenderControl final : public renderer::DefaultRendererController
{
public:
    ArtRenderControl(const ArtTileCallback& callback, int frame, bool progress);
    ~ArtRenderControl() override;
    ArtRenderControl(const ArtRenderControl&) = delete;
    ArtRenderControl& operator=(const ArtRenderControl&) = delete;

    bool valid() const { return m_previous != SIG_ERR; }
    bool cancelled() const;
    bool restore();
    void complete();
    void on_rendering_begin() override;
    void on_progress() override;
    Status get_status() const override;

private:
    using Clock = std::chrono::steady_clock;
    using SignalHandler = void (*)(int);
    const ArtTileCallback& m_callback;
    const int m_frame;
    const bool m_progress;
    SignalHandler m_previous;
    Clock::time_point m_started = Clock::now();
    Clock::time_point m_last_update = m_started;
    size_t m_last_completed = 0;
};

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
