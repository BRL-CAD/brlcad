/*         P O L Y G O N _ C I R C L E _ C O N T R O L . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file polygon_circle_control.h
 *
 */

#include <QPushButton>

class QCirclePolyControl : public QPushButton
{
    Q_OBJECT

    public:
	QCirclePolyControl(QString s);
	~QCirclePolyControl();

    signals:
	void view_updated(struct bview **);

    protected:
	bool eventFilter(QObject *, QEvent *);

    private:
	int cpoly_cnt = 0;
	struct bv_scene_obj *p = NULL;
};


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
