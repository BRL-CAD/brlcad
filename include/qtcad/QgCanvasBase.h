/*                 Q G C A N V A S B A S E . H
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
/** @file QgCanvasBase.h
 *
 * Abstract interface for libqtcad canvas widgets.
 *
 * Both QgGL (hardware OpenGL via QOpenGLWidget) and QgSW (software
 * rasterizer via QWidget) implement this interface so that higher-level
 * widgets (QgView, QgQuadView, …) can hold a single typed pointer and
 * call virtual methods without sprinkling BRLCAD_OPENGL preprocessor
 * guards throughout their implementation.
 *
 * QgCanvasBase is deliberately NOT a QObject subclass.  Because Qt
 * forbids multiple QObject inheritance the concrete canvas widgets
 * remain direct QWidget derivatives and simply additionally derive from
 * this interface.
 *
 * To connect signals emitted by the concrete canvas, call asQObject()
 * and use the SIGNAL/SLOT macro syntax:
 *
 *   QObject::connect(canvas->asQObject(), SIGNAL(changed()),
 *                    receiver,            SLOT(onChanged()));
 */

#ifndef QGCANVASBASE_H
#define QGCANVASBASE_H

#include "common.h"
#include "qtcad/defines.h"

class QImage;
class QObject;
class QString;
class QWidget;

struct bu_ptbl;
struct bsg_view;
struct dm;
struct fb;

class QTCAD_EXPORT QgCanvasBase {
public:
    virtual ~QgCanvasBase() = default;

    /** Return this canvas as a QWidget (for embedding in layouts). */
    virtual QWidget *canvasWidget() = 0;

    /** Return this canvas as a QObject (for signal/slot connections). */
    virtual QObject *asQObject() = 0;

    /**
     * Returns true if the canvas is initialised and ready to render.
     * QgGL delegates to QOpenGLWidget::isValid(); QgSW always returns
     * true once the widget has been constructed.
     */
    virtual bool isValid() const = 0;

    /** bsg_view associated with this canvas (may be nullptr). */
    virtual struct bsg_view *view() const = 0;

    /** Underlying libdm display manager (nullptr before first paint). */
    virtual struct dm *displayManager() const = 0;

    /** Framebuffer handle (may be nullptr). */
    virtual struct fb *frameBuffer() const = 0;

    /** Bind an external bsg_view.  Pass nullptr to revert to the local view. */
    virtual void set_view(struct bsg_view *) = 0;

    /** Register this canvas's DM in a shared display-manager table. */
    virtual void setDisplayManagerSet(struct bu_ptbl *) = 0;

    /** Store current DM and view hash values for later comparison. */
    virtual void stash_hashes() = 0;

    /**
     * Compare current hashes against the stored values.
     * Sets the DM dirty flag and emits changed() if they differ.
     * Does NOT update the stored values (call stash_hashes() for that).
     */
    virtual bool diff_hashes() = 0;

    /** Set azimuth, elevation and twist of the current view. */
    virtual void aet(double a, double e, double t) = 0;

    /** Capture the current rendered frame to "file.png". */
    virtual void save_image() = 0;

    /**
     * Render the current view and save it to @p filename.
     * Safe to call before the widget is shown (headless / off-screen mode).
     */
    virtual void render_to_file(const QString &filename) = 0;

    /**
     * Render the current view and return the pixel data in @p img.
     * Sets @p img to a null QImage on failure.
     */
    virtual void get_viewport_image(QImage &img) = 0;

    virtual void enableDefaultKeyBindings()  = 0;
    virtual void disableDefaultKeyBindings() = 0;
    virtual void enableDefaultMouseBindings()  = 0;
    virtual void disableDefaultMouseBindings() = 0;

    /** Currently configured left-mouse-drag mode. */
    virtual int lmouseMoveDefault() const = 0;

    /**
     * In a multi-view setup only the "current" canvas processes mouse/key
     * events.  currentView() returns 1 (enabled) or 0 (disabled).
     */
    virtual int currentView() const = 0;
    virtual void set_current(int active) = 0;

    /* -- virtual slots (dispatched directly by QgView) -- */
    virtual void need_update() = 0;
    virtual void queued_update() = 0;
    virtual void set_lmouse_move_default(int mode) = 0;
};

#endif /* QGCANVASBASE_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
