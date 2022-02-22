/*                      Q T G L Q U A D . H
 * BRL-CAD
 *
 * Copyright (c) 2021-2022 United States Government as represented by
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
/** @file QtGLQuad.h
 *
 */

#ifndef QTCADQUAD_H
#define QTCADQUAD_H

#include "common.h"

#include <QWidget>

extern "C" {
#include "bu/ptbl.h"
#include "bv.h"
#include "dm.h"
}

#include "qtcad/defines.h"
#include "qtcad/QtCADView.h"

class QTCAD_EXPORT QtCADQuad : public QWidget
{
    Q_OBJECT

    public:
	explicit QtCADQuad(QWidget *parent, struct ged *gedpRef, int type);
	~QtCADQuad();

	void stash_hashes(); // Store current dmp and v hash values
	bool diff_hashes();  // Set dmp dirty flags if current hashes != stashed hashes.  (Does not update stored hash values - use stash_hashes for that operation.)

	bool isValid();
	void fallback();

	void default_views(); // Set the aet of the four quadrants to their standard defaults.
	QtCADView *get(int quadrant_num = 0);
	struct bview * view(int quadrant_id = 0);

	void select(int quadrant_num);
	void select(const char *id); // valid inputs: ur, ul, ll and lr

        void changeToSingleFrame();
        void changeToQuadFrame();
        void changeToResizeableQuadFrame();

    signals:
	void changed();
	void selected(QtCADView *);

    public slots:
	void need_update(void *);
        void do_view_changed();

    private:
        bool eventFilter(QObject*, QEvent*);
        QtCADView *createView(int index);
        QGridLayout *createLayout();

        int graphicsType = QtCADView_SW;
        // Holds up to 4 views in single view only the first view is constructed
        // The other views are constructed if quad view mode is selected by the user
	QtCADView *views[4] = {nullptr, nullptr, nullptr, nullptr};
        QtCADView *currentView;
        // We need to hang on to this because we don't create the other quad views until the user switches mode
        struct ged *gedp;

        // Hang on to thiese pointers so when we need to clean up memory we have them
        QGridLayout *currentLayout = nullptr;
        QSpacerItem *spacerTop = nullptr;
        QSpacerItem *spacerBottom = nullptr;
        QSpacerItem *spacerLeft = nullptr;
        QSpacerItem *spacerRight = nullptr;
        QSpacerItem *spacerCenter = nullptr;
};

#endif /* QTCADQUAD_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

