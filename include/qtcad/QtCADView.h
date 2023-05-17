/*                     Q T C A D V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2023 United States Government as represented by
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
/** @file QtCADView.h
 *
 */

#ifndef QTCADVIEW_H
#define QTCADVIEW_H

#include "common.h"

extern "C" {
#include "bu/ptbl.h"
#include "bg/polygon.h"
#include "bv.h"
#include "dm.h"
}

#include <QBoxLayout>
#include <QObject>
#include <QWidget>
#include "qtcad/defines.h"
#include "qtcad/QtSW.h"
#ifdef BRLCAD_OPENGL
#  include "qtcad/QtGL.h"
#endif

#define QtCADView_AUTO 0
#define QtCADView_SW 1
#ifdef BRLCAD_OPENGL
#  define QtCADView_GL 2
#endif

class QTCAD_EXPORT QtCADView : public QWidget
{
    Q_OBJECT

    public:
	explicit QtCADView(QWidget *parent = nullptr, int type = 0, struct fb *fbp = NULL);
	~QtCADView();

	int view_type();
	void set_current(int);
	int current();

	void stash_hashes(); // Store current dmp and v hash values
	bool diff_hashes();  // Set dmp dirty flag if current hashes != stashed hashes.  (Does not update   stored hash values - use stash_hashes for that operation.)

	void save_image(int quad = 0);

	bool isValid();

	struct bview * view();
	struct dm * dmp();
	struct fb * ifp();

	void set_view(struct bview *);

	void aet(double a, double e, double t);

	QObject *curr_event_filter = NULL;
	void set_draw_custom(void (*draw_custom)(struct bview *, void *), void *draw_udata);

	void add_event_filter(QObject *);
	void clear_event_filter(QObject *);

	void enableDefaultKeyBindings();
	void disableDefaultKeyBindings();

	void enableDefaultMouseBindings();
	void disableDefaultMouseBindings();

    signals:
	void changed();
	void init_done();

    public slots:
	void need_update(unsigned long long);
	void do_view_changed();
	void do_init_done();
	void set_lmouse_move_default(int);

    private:
        QBoxLayout *l = NULL;
	QtSW *canvas_sw = NULL;
#ifdef BRLCAD_OPENGL
        QtGL *canvas_gl = NULL;
#endif
};

// Filters designed for specific editing modes
class QTCAD_EXPORT QPolyFilter : public QObject
{
    Q_OBJECT

    public:
	// Initialization common to the various polygon filter types
	QMouseEvent *view_sync(QEvent *e);

	// We want to be able to swap derived QPolyFilter classes in
	// parent calling code - make eventFilter virtual to help
	// simplify doing so.
	virtual bool eventFilter(QObject *, QEvent *) { return false; };

    signals:
        void view_updated(int);
        void finalized(bool);

    public:
	bool close_polygon();

	struct bview *v = NULL;
	bg_clip_t op = bg_None;
	struct bv_scene_obj *wp = NULL;
	int ptype = BV_POLYGON_CIRCLE;
	bool close_general_poly = true; // set to false if application wants to allow non-closed polygons
	struct bu_color fill_color = BU_COLOR_BLUE;
	struct bu_color edge_color = BU_COLOR_YELLOW;
	bool fill_poly = false;
	double fill_slope_x = 1.0;
	double fill_slope_y = 1.0;
	double fill_density = 10.0;
	std::string vname;
};

class QTCAD_EXPORT QPolyCreateFilter : public QPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
	void finalize(bool);

	struct bu_ptbl bool_objs = BU_PTBL_INIT_ZERO;
};

class QTCAD_EXPORT QPolyUpdateFilter : public QPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);

	struct bu_ptbl bool_objs = BU_PTBL_INIT_ZERO;
};

class QTCAD_EXPORT QPolySelectFilter : public QPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
};

class QTCAD_EXPORT QPolyPointFilter : public QPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
};

class QTCAD_EXPORT QPolyMoveFilter : public QPolyFilter
{
    Q_OBJECT

    public:
	bool eventFilter(QObject *, QEvent *e);
};


#endif /* QTCADVIEW_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

