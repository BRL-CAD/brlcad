/*                  C A D V I E W M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file CADViewModel.cpp
 *
 * Brief description
 *
 */

#include <QPainter>
#include <QString>
#include <bv.h>

#include "bu/vls.h"
#include "qtcad/QgSignalFlags.h"
#include "qtcad/QgPluginContext.h"
#include "CADViewModel.h"

CADViewModel::CADViewModel(QObject *parentobj)
    : QgKeyValModel(parentobj)
{
    m_root = NULL;
}

CADViewModel::~CADViewModel()
{
    if (m_root)
	delete m_root;
}

void
CADViewModel::update()
{
    refresh(QG_VIEW_REFRESH);
}

void
CADViewModel::refresh(unsigned long long)
{
    struct bview *view = m_context ? m_context->activeViewContext() : NULL;
    if (!view)
	return;

    struct bu_vls val = BU_VLS_INIT_ZERO;
    int i = 0;
    beginResetModel();
    if (m_root)
	delete m_root;
    m_root = new QgKeyValNode();

    add_pair("Name", bu_vls_cstr(&view->gv_name), m_root, i);
    bu_vls_sprintf(&val, "%g", view->gv_size);
    add_pair("Size", bu_vls_cstr(&val), m_root, i);
    bu_vls_sprintf(&val, "%d", view->gv_width);
    add_pair("Width", bu_vls_cstr(&val), m_root, i);
    bu_vls_sprintf(&val, "%d", view->gv_height);
    add_pair("Height", bu_vls_cstr(&val), m_root, i);
    bu_vls_sprintf(&val, "%g", view->gv_aet[0]);
    add_pair("Az", bu_vls_cstr(&val), m_root, i);
    bu_vls_sprintf(&val, "%g", view->gv_aet[1]);
    add_pair("El", bu_vls_cstr(&val), m_root, i);
    bu_vls_sprintf(&val, "%g", view->gv_aet[2]);
    add_pair("Tw", bu_vls_cstr(&val), m_root, i);

    vect_t center;
    MAT_DELTAS_GET_NEG(center, view->gv_center);
    bu_vls_sprintf(&val, "%g", center[0]);
    add_pair("Center[X]", bu_vls_cstr(&val), m_root, i);
    bu_vls_sprintf(&val, "%g", center[1]);
    add_pair("Center[Y]", bu_vls_cstr(&val), m_root, i);
    bu_vls_sprintf(&val, "%g", center[2]);
    add_pair("Center[Z]", bu_vls_cstr(&val), m_root, i);

    endResetModel();

    bu_vls_free(&val);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


