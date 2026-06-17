/*                   Q G C A N V A S S T A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
/** @file QgCanvasState.h
 *
 * Private (libqtcad-internal) pimpl struct that consolidates the state
 * shared between QgGL and QgSW, together with inline helper operations
 * that eliminate textual duplication in those two canvas implementation
 * files (Phase 3, §2.4 of the refactor plan).
 *
 * This header is NOT part of the installed libqtcad public API.
 */

#ifndef QGCANVASSTATE_H
#define QGCANVASSTATE_H

#include "common.h"

#include <climits>
#include <cmath>
#include <QSize>
#include <QWidget>

extern "C" {
#include "bu/ptbl.h"
#include "bu/malloc.h"
#include "bsg.h"
#include "bsg/view_state.h"
#define DM_WITH_RT
#include "dm.h"
}

#include "QgCanvasInput.h"

/**
 * Plain-data struct that consolidates the private state shared between
 * QgGL and QgSW.  It is held as a pimpl-style raw pointer in both canvas
 * class headers so that implementation details are not part of the
 * installed public interface.
 *
 * Ownership summary
 * ─────────────────
 * local_v  – Allocated by BU_GET in the canvas constructor; freed by BU_PUT
 *            in the canvas destructor.  The canvas owns it unconditionally.
 *
 * v        – Normally points to local_v (the widget-owned view).  When
 *            QgCanvasBase::set_view() is called with a non-null external
 *            view, v points to that caller-owned bsg_view instead.  Passing
 *            nullptr to set_view() reverts v back to local_v.  The canvas
 *            never frees v — it only frees local_v.
 *
 * dmp      – Opened lazily in the first paint event via dm_open(); closed
 *            by dm_close() in the canvas destructor.  The canvas owns it.
 *
 * ifp      – When the canvas is constructed without a caller-supplied fb*,
 *            it allocates a raw framebuffer (fb_raw()) and owns it; it is
 *            released by fb_close_existing() in the destructor.  When the
 *            caller supplies an fb* at construction time (fb_get_standalone
 *            returns non-zero), the canvas does not close it on destruction.
 *
 * dm_set   – An optional shared display-manager table managed by the
 *            caller (e.g. QgQuadView).  The canvas inserts its dmp into
 *            the table during initialisation but does NOT own the table.
 */
struct QgCanvasState {
	/* ---- view / dm / fb plumbing ---- */
	struct bsg_view    *v = nullptr;       /* active view: normally == local_v,
	                                       set_view() can redirect to an
	                                       external caller-owned bsg_view       */
	struct dm       *dmp = nullptr;     /* libdm display manager (canvas owns) */
	struct fb       *ifp = nullptr;     /* framebuffer (see ownership note)  */
	struct bu_ptbl  *dm_set = nullptr;  /* shared DM table (caller owns)     */
	struct bsg_view    *local_v = nullptr; /* widget-owned view (canvas owns)   */

	/* ---- hash tracking for incremental updates ---- */
	unsigned long long prev_dhash = 0;
	unsigned long long prev_vhash = 0;

	/* ---- input-binding flags ---- */
	bool use_default_keybindings   = true;
	bool use_default_mousebindings = true;
	int  lmouse_mode = -1;  /* set to BSG_SCALE in canvas constructor */

	/* ---- widget-level tracking ---- */
	int    current = 1;     /* 1 = this view is active */
	bool   m_init = false;  /* DM has been opened and configured */
	int    x_prev = -INT_MAX;
	int    y_prev = -INT_MAX;
	double x_press_pos = -INT_MAX;
	double y_press_pos = -INT_MAX;
	bool   fb_update_queued = false;

	/* ---- per-canvas input handler ---- */
	QgCanvasInput input;
};

/* ------------------------------------------------------------------ */
/* Shared inline helpers (static so each TU gets its own copy)        */
/* ------------------------------------------------------------------ */

/** Compute the physical render size of a widget (accounting for DPR). */
static inline QSize
qgcanvas_render_size(const QWidget *w)
{
	if (!w)
		return QSize();
	qreal dpr = w->devicePixelRatioF();
	return QSize(qMax(1, static_cast<int>(std::ceil(w->width()  * dpr))),
	             qMax(1, static_cast<int>(std::ceil(w->height() * dpr))));
}

/** Store current DM and view hashes in @p s. */
static inline void
qgcanvas_stash_hashes(QgCanvasState &s)
{
	s.prev_dhash = s.dmp ? dm_hash(s.dmp) : 0ULL;
	s.prev_vhash = s.v   ? bsg_hash(s.v)   : 0ULL;
}

/** Request a semantic view refresh and wake the canvas backend. */
static inline void
qgcanvas_request_update(QgCanvasState &s, uint32_t flags)
{
	if (s.v)
		bsg_view_refresh_request(s.v, flags ? flags : BSG_VIEW_REFRESH_ALL);
	if (s.dmp)
		dm_set_native_repaint_pending(s.dmp, 1);
}

/**
 * Compare current DM/view hashes against the stored values and update the
 * view refresh record and DM wakeup flag when differences are found.
 *
 * Returns true if any value changed.  The caller is responsible for calling
 * need_update() and emitting changed() — signal emission requires a QObject
 * context that QgCanvasState does not have.
 */
static inline bool
qgcanvas_diff_hashes_check(QgCanvasState &s)
{
	bool ret = false;
	unsigned long long c_dhash = s.dmp ? dm_hash(s.dmp) : 0ULL;
	unsigned long long c_vhash = s.v   ? bsg_hash(s.v)   : 0ULL;

	if (s.dmp && dm_get_native_repaint_pending(s.dmp)) {
		if (s.v)
			bsg_view_refresh_request(s.v, BSG_VIEW_REFRESH_FORCE);
		ret = true;
	}

	if (s.prev_dhash != c_dhash) {
		qgcanvas_request_update(s, BSG_VIEW_REFRESH_FRAMEBUFFER | BSG_VIEW_REFRESH_FORCE);
		ret = true;
	}
	if (s.prev_vhash != c_vhash) {
		qgcanvas_request_update(s, BSG_VIEW_REFRESH_VIEW | BSG_VIEW_REFRESH_DRAW);
		ret = true;
	}
	return ret;
}

/** Set the azimuth/elevation/twist of the view stored in @p s. */
static inline void
qgcanvas_aet(QgCanvasState &s, double a, double e, double t)
{
	if (!s.v)
		return;
	fastf_t aet_v[3];
	double  aetd[3] = {a, e, t};
	VMOVE(aet_v, aetd);
	bsg_view_set_aet(s.v, aet_v);
	bsg_update(s.v);
}

/** Bind an external bsg_view (or nullptr to revert to the widget-local view). */
static inline void
qgcanvas_set_view(QgCanvasState &s, struct bsg_view *nv)
{
	if (!nv) {
		/* Revert to the widget-owned local view. */
		s.v = s.local_v;
		return;
	}
	s.v = nv;
	if (!s.dmp)
		return;
	s.v->dmp = s.dmp;
	dm_configure_win(s.dmp, 0);
	s.v->gv_width  = dm_get_width(s.dmp);
	s.v->gv_height = dm_get_height(s.dmp);
}

#endif /* QGCANVASSTATE_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
