/*                    V I E W _ M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file view_model.cpp
 *
 * Brief description
 *
 */

#include <QPainter>
#include <QString>

#include "view_model.h"
#include "bu/sort.h"
#include "bu/avs.h"
#include "bu/malloc.h"
#include "qtcad/SignalFlags.h"

CADViewModel::CADViewModel(QObject *parentobj, struct bview **v)
    : QKeyValModel(parentobj)
{
    m_root = NULL;
    m_v = v;
    refresh(0);
}

CADViewModel::~CADViewModel()
{
    if (m_root)
	delete m_root;
}

void
CADViewModel::update()
{
    printf("view model update\n");
    refresh(QTCAD_VIEW_REFRESH);
}

void
CADViewModel::refresh(unsigned long long)
{
    //m_v = gedp->ged_gvp;
    m_v = NULL;

    if (m_v && *m_v) {
	struct bview *v = *m_v;
	struct bu_vls val = BU_VLS_INIT_ZERO;
	QMap<QString, QKeyValNode*> standard_nodes;
	int i = 0;
	if (m_root)
	    delete m_root;
	m_root = new QKeyValNode();
	beginResetModel();

	standard_nodes.insert("Name", add_pair("Name", bu_vls_cstr(&v->gv_name), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_size);
	standard_nodes.insert("Size", add_pair("Size", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%d", v->gv_width);
	standard_nodes.insert("Width", add_pair("Width", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%d", v->gv_height);
	standard_nodes.insert("Height", add_pair("Height", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_aet[0]);
	standard_nodes.insert("Az", add_pair("Az", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_aet[1]);
	standard_nodes.insert("El", add_pair("El", bu_vls_cstr(&val), m_root, i));
	bu_vls_sprintf(&val, "%g", v->gv_aet[2]);
	standard_nodes.insert("Tw", add_pair("Tw", bu_vls_cstr(&val), m_root, i));

	endResetModel();

	bu_vls_free(&val);
    } else {
	if (m_root)
	    delete m_root;
	m_root = new QKeyValNode();
	beginResetModel();
	endResetModel();
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


