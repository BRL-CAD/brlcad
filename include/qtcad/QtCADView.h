/*                     Q T C A D V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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

	void stash_hashes(); // Store current dmp and v hash values
	bool diff_hashes();  // Set dmp dirty flag if current hashes != stashed hashes.  (Does not update   stored hash values - use stash_hashes for that operation.)

	void save_image(int quad = 0);

	bool isValid();
	void fallback();

	void select(int quad);

	struct bview * view();
	struct dm * dmp();
	struct fb * ifp();
	double base2local();
	double local2base();

	void set_view(struct bview *, int quad = 1);
	void set_dmp(struct dm *, int quad = 1);
	void set_dm_current(struct dm **, int quad = 1);
	void set_ifp(struct fb *, int quad = 1);
	void set_base2local(double *);
	void set_local2base(double *);

    public slots:
	void need_update();

    private:
        QBoxLayout *l = NULL;
	QtSW *canvas_sw = NULL;
#ifdef BRLCAD_OPENGL
        QtGL *canvas_gl = NULL;
#endif
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

