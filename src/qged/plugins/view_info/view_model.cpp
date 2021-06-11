/*                    V I E W _ M O D E L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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

CADViewModel::CADViewModel(QObject *parentobj, struct bview **v)
    : QKeyValModel(parentobj)
{
    m_root = new QKeyValNode();
    refresh(v);
}

CADViewModel::~CADViewModel()
{
}

void
CADViewModel::update()
{
    printf("view model update\n");
    refresh(m_v);
}

void
CADViewModel::refresh(struct bview **nv)
{
    m_v = nv;

    if (m_v && *m_v) {
	struct bview *v = *m_v;
	struct bu_vls val = BU_VLS_INIT_ZERO;
	QMap<QString, QKeyValNode*> standard_nodes;
	int i = 0;
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
	m_root = new QKeyValNode();
	beginResetModel();
	endResetModel();
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

