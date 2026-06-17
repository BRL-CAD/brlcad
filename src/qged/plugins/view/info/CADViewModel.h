/*                  C A D V I E W M O D E L . H
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
/** @file CADViewModel.h
 *
 * Brief description
 *
 */

#ifndef CADVIEWMODEL_H
#define CADVIEWMODEL_H

#include "qtcad/QgKeyVal.h"

#ifndef Q_MOC_RUN
#include "bu/avs.h"
#include "bsg.h"
#include "raytrace.h"
#include "ged.h"
#endif

class QgPluginContext;

class CADViewModel : public QgKeyValModel
{
    Q_OBJECT

    public:
	explicit CADViewModel(QObject *parent = 0);
	~CADViewModel();

	/* Supply the host context so refresh() can reach the active view
	 * without casting qApp to a qged-specific type. */
	void setContext(QgPluginContext *ctx) { m_ctx = ctx; }

    public slots:
	void refresh(unsigned long long);
	void update();

    private:
	QgPluginContext *m_ctx = nullptr;
};

#endif /*CADVIEWMODEL_H*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

