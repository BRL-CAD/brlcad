/*                     Q G V I E W . H
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
/** @file QgView.h
 *
 */

#ifndef QGVIEW_H
#define QGVIEW_H

#include "common.h"

extern "C" {
#include "bu/ptbl.h"
#include "bg/polygon.h"
#include "bv.h"
#include "dm.h"
}

#include <vector>
#include <QBoxLayout>
#include <QObject>
#include <QWidget>
#include "qtcad/defines.h"
#include "qtcad/QgSW.h"
#ifdef BRLCAD_OPENGL
#  include "qtcad/QgGL.h"
#endif

#define QgView_AUTO 0
#define QgView_SW 1
#ifdef BRLCAD_OPENGL
#  define QgView_GL 2
#endif

class QTCAD_EXPORT QgView : public QWidget
{
    Q_OBJECT

    public:
	explicit QgView(QWidget *parent = nullptr, int type = 0, struct fb *fbp = NULL);
	~QgView();

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

	// Wrappers around Qt's facility for adding eventFilter objects to
	// widgets.  This is how custom key binding modes are enabled and
	// disabled in QgView windows.
	void add_event_filter(QObject *);

	// If a filter object is supplied, remove just that filter.  If NULL is
	// passed in, remove all filters added using add_event_filter.  Does
	// not clear all event filters of any sort (i.e. internal filters used
	// by Qt), just those managed using these methods.
	void clear_event_filter(QObject *);

	void enableDefaultKeyBindings();
	void disableDefaultKeyBindings();

	void enableDefaultMouseBindings();
	void disableDefaultMouseBindings();

    signals:
	void changed(QgView *);
	void init_done();

    public slots:
	void need_update(unsigned long long);
	void do_view_changed();
	void do_init_done();
	void set_lmouse_move_default(int);

    private:
        QBoxLayout *l = NULL;
	QgSW *canvas_sw = NULL;
#ifdef BRLCAD_OPENGL
        QgGL *canvas_gl = NULL;
#endif
	std::vector<QObject *> filters;
};

#endif /* QGVIEW_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

