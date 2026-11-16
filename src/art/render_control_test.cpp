/*              R E N D E R _ C O N T R O L _ T E S T . C P P
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

#include <csignal>
#include "bu/app.h"
#include "bu/log.h"
#include "render_control.h"
#include "tile.h"

namespace {
volatile std::sig_atomic_t prior_handler_calls = 0;

void
prior_handler(int)
{
    ++prior_handler_calls;
}
}

int
main(int argc, char **argv)
{
    bu_setprogname(argc ? argv[0] : "art_render_control_test");
    const auto original = std::signal(SIGINT, prior_handler);
    if (original == SIG_ERR)
	return 1;
    bool success = true;
    ArtTileCallback callback(nullptr);
    {
	ArtRenderControl control(callback, 0, false);
	success = control.valid() && !control.cancelled() &&
	    control.get_status() == renderer::IRendererController::ContinueRendering;
	if (success) {
	    success = std::raise(SIGINT) == 0 && control.cancelled() &&
		control.get_status() == renderer::IRendererController::AbortRendering;
	}
	success = control.restore() && success;
    }
    success = std::raise(SIGINT) == 0 && prior_handler_calls == 1 && success;
    // Some platforms reset a delivered handler, so establish the next prior handler explicitly.
    success = std::signal(SIGINT, prior_handler) != SIG_ERR && success;
    // A cancelled frame must not poison the next controller, and its
    // destructor must restore the caller's signal handler without restore().
    {
	ArtRenderControl control(callback, 1, false);
	success = control.valid() && !control.cancelled() && success;
    }
    success = std::raise(SIGINT) == 0 && prior_handler_calls == 2 && success;
    success = std::signal(SIGINT, prior_handler) != SIG_ERR && success;
    {
	ArtRenderControl control(callback, 2, false);
	control.fail();
	success = control.valid() && control.failed() && !control.cancelled() &&
	    control.get_status() == renderer::IRendererController::AbortRendering && success;
    }
    {
	ArtRenderControl control(callback, 3, false);
	success = control.valid() && !control.failed() && !control.cancelled() &&
	    control.get_status() == renderer::IRendererController::ContinueRendering && success;
    }
    success = std::signal(SIGINT, original) != SIG_ERR && success;
    if (!success)
	bu_log("Render termination state or signal restoration failed\n");
    return success ? 0 : 1;
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
