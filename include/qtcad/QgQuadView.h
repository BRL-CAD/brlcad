/*                    Q G Q U A D V I E W . H
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
/** @file QgQuadView.h
 *
 */

#ifndef QGQUADVIEW_H
#define QGQUADVIEW_H

#include "common.h"

#include <QWidget>

extern "C" {
#include "bu/ptbl.h"
#include "bv.h"
#include "dm.h"
}

#include "qtcad/defines.h"
#include "qtcad/QgView.h"

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

class QTCAD_EXPORT QgQuadView : public QWidget
{
    Q_OBJECT

    public:
	explicit QgQuadView(QWidget *parent, struct ged *gedpRef, int type);
	~QgQuadView();

	void stash_hashes(); // Store current dmp and v hash values
	bool diff_hashes();  // Set dmp dirty flags if current hashes != stashed hashes.  (Does not update stored hash values - use stash_hashes for that operation.)

	bool isValid();

	void default_views(int); // Set the aet of the four quadrants to their standard defaults.  If 0 is supplied the upper right quadrant isn't altered, if 1 is supplied all are adjusted.
	QgView *get(int quadrant_num = UPPER_RIGHT_QUADRANT);
	QgView *get(const QPoint &p); // Test is global point coordinates correspond to one of the quad view
	QgView *get(QEvent *e); // Given a MouseButtonPress QEvent, see if the point identifies a view
	QgView *curr_view(); // return the currently selected view
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
	void changed(QgView *);
	void selected(QgView *);
	void init_done();

    public slots:
	void do_view_update(unsigned long long);
        void do_view_changed();
	void do_init_done();
	void set_lmouse_move_default(int);

    private:
        bool eventFilter(QObject*, QEvent*);
        QgView *createView(unsigned int index);
        QGridLayout *createLayout();

        int graphicsType = QgView_SW;
        // Holds up to 4 views in single view only the first view is constructed
        // The other views are constructed if quad view mode is selected by the user
	QgView *views[4] = {nullptr, nullptr, nullptr, nullptr};
        QgView *currentView;
        // We need to hang on to this because we don't create the other quad views until the user switches mode
        struct ged *gedp;

        // Hang on to these pointers so when we need to clean up memory we have them
        QGridLayout *currentLayout = nullptr;
	// Note that we use QWidget here rather than QSpacerItem in order to allow
	// for color control to indicate active quadrants without having to resize
	// the windows in response to a selection.
        QWidget *spacerTop = nullptr;
        QWidget *spacerBottom = nullptr;
        QWidget *spacerLeft = nullptr;
        QWidget *spacerRight = nullptr;
        QWidget *spacerCenter = nullptr;

	// Flag for initializations of sub-widgets
	bool init_done_flag = false;
};

#endif /* QGQUADVIEW_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

