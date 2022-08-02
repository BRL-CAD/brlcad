/*                       G F I L E . H
 * BRL-ISST
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
/** @file cadapp.h
 *
 *  Manage database objects
 *
 */

#ifndef GFILE_H
#define GFILE_H

#include <QObject>
#include <QString>

extern "C" {
#include "bn/tol.h"
#include "raytrace.h"

#include "rt/tie.h"
#include "adrt.h"
#include "adrt_struct.h"
}

class GFile : public QObject
{
    Q_OBJECT

    public:
	int load_g(const char *filename, int argc, const char **argv);
	void closedb();

	struct tie_s *tie;
	struct adrt_mesh_s *meshes;
	struct db_i *dbip;
	TIE_3 **tribuf;

	QString current_file;
};

#endif // GFILE_H

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

