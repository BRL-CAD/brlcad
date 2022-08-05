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

// Abbreviations:
//
// ur == 0 == Upper Right Quadrant
// ul == 1 == Upper Left Quadrant
// ll == 2 == Lower Left Quadrant
// lr == 3 == Lower Right Quadrant

#define UPPER_RIGHT_QUADRANT 0
#define UPPER_LEFT_QUADRANT 1
#define LOWER_LEFT_QUADRANT 2
#define LOWER_RIGHT_QUADRANT 3

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
	QtCADView *get(int quadrant_num = UPPER_RIGHT_QUADRANT);
	struct bview * view(int quadrant_id = UPPER_RIGHT_QUADRANT);

	void select(int quadrant_num);
	void select(const char *id); // valid inputs: ur, ul, ll and lr
	int get_selected(); // returns UPPER_RIGHT_QUADRANT, UPPER_LEFT_QUADRANT, LOWER_LEFT_QUADRANT, or LOWER_RIGHT_QUADRANT

        void changeToSingleFrame();
        void changeToQuadFrame();
        void changeToResizeableQuadFrame();

	void enableDefaultKeyBindings();
	void disableDefaultKeyBindings();

	void enableDefaultMouseBindings();
	void disableDefaultMouseBindings();

    signals:
	void changed(QtCADView *);
	void selected(QtCADView *);
	void init_done();

    public slots:
	void need_update(unsigned long long);
        void do_view_changed();
	void do_init_done();
	void set_lmouse_move_default(int);

    private:
        bool eventFilter(QObject*, QEvent*);
        QtCADView *createView(unsigned int index);
        QGridLayout *createLayout();

        int graphicsType = QtCADView_SW;
        // Holds up to 4 views in single view only the first view is constructed
        // The other views are constructed if quad view mode is selected by the user
	QtCADView *views[4] = {nullptr, nullptr, nullptr, nullptr};
        QtCADView *currentView;
        // We need to hang on to this because we don't create the other quad views until the user switches mode
        struct ged *gedp;

        // Hang on to these pointers so when we need to clean up memory we have them
        QGridLayout *currentLayout = nullptr;
        QSpacerItem *spacerTop = nullptr;
        QSpacerItem *spacerBottom = nullptr;
        QSpacerItem *spacerLeft = nullptr;
        QSpacerItem *spacerRight = nullptr;
        QSpacerItem *spacerCenter = nullptr;

	// Flag for initializations of sub-widgets
	bool init_done_flag = false;
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

