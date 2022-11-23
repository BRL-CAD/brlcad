/*                        Q G E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
/** @file QgUtil.h
 *
 * General purpose .g editing related Qt widgets - the "building blocks"
 * used to define editing widgets
 */

#ifndef QGEDIT_H
#define QGEDIT_H

#include "common.h"

#include <QLineEdit>

#include "qtcad/defines.h"

#ifndef Q_MOC_RUN
#include "raytrace.h"
#include "ged.h"
#endif

/* DBIP aware .g object name entry.  Will change the text color based on
 * whether the current string is present in the database - default color if
 * yes, red if no.  If expected_type is not the default value, also turns red
 * if the object is present but of a non-matching type.)  n_state reflects the
 * current state of the specified name in the database - 0 == present, 1 == not
 * present, 2 == present but of a different type. */
class QTCAD_EXPORT QgNameEdit: public QLineEdit
{
    Q_OBJECT

    public:
	QgNameEdit(QWidget *p = NULL, struct db_i *e_dbip = NULL, int e_type = DB5_MINORTYPE_RESERVED);
	~QgNameEdit();

	int n_state = 1;

    private:
	struct db_i *dbip = NULL;
	// Minor type from rt/db5.h
	int expected_type = DB5_MINORTYPE_RESERVED;
}

/* 3D point/vector input */
class QTCAD_EXPORT QVectEdit: public QWidget
{
    Q_OBJECT

    public:
	QgVectEdit(QWidget *p = NULL);
	~QgVectEdit();

	// QDoubleValidator inputs for each value
	QLineEdit *vx;
	QLineEdit *vy;
	QLineEdit *vz;

	vect_t v = VINIT_ZERO;
}


/* Need a "pre-packaged" one line palette of controls for standard operations
 * applicable to all individual objects.  More or less based on the archer
 * "edit" menu icons in the menu bar:
 *
 * Rot,Tra,Scale,Center,Add,(Clone?),Write,Revert */
//
// Related to QToolPalette and QToolPaletteButton, but we don't have panel
// elements for this application so it's probably a bit different.  Not sure
// yet whether to try and generalize QToolPalette in some fashion or define
// a simpler widget for this purpose.



/* I'm thinking for the "instance editing" panel, the way we may really want to
 * do that is to have the comb object editing panel show an MGED comb editor
 * display of the tree and a variation on the above "standard" buttons with
 * push/xpush added (which are only relevant to combs).  We can also then
 * handle the "above/below" matrix application question - if one or a subset of
 * comb tree elements are selected while editing the selected comb, only those
 * instances will have their matrices updated.  So if an instance is selected
 * in the tree, we select the "comb" object edit button, and within that dialog's
 * tree highlight the instance corresponding to that selected in the main tree.
 * (Recognizing that an "instance edit" is in fact a selective edit of the parent
 * comb object.)  If the user selects multiple instances in the comb edit we would
 * also want to reflect that in the master tree, so there are considerations both
 * ways... We would probably need to upgrade the signal/slot channels to
 * be able to convey the extra information, but on the larger scale it would
 * collapse the current View/Instance/Edit paradigm in qged to just View/Edit. */


/* Need to consider whether

#endif //QGEDIT_H

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

