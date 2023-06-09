/*                  C A D V I E W M O D E L . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
#include "bv.h"
#include "raytrace.h"
#include "ged.h"
#endif

class CADViewModel : public QgKeyValModel
{
    Q_OBJECT

    public:
	explicit CADViewModel(QObject *parent = 0);
	~CADViewModel();

    public slots:
	void refresh(unsigned long long);
	void update();
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

