/*                       C A D H E L P . H
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
/** @file cadhelp.h
 *
 * Brief description
 *
 */

#ifndef QCADHELP_H
#define QCADHELP_H 

#include <QObject>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDialogButtonBox>

#include "cadapp.h"

class CADManViewer : public QDialog
{
  Q_OBJECT

public:
  CADManViewer(QWidget* Parent, QString *manpage = 0);
  ~CADManViewer();

  QTextBrowser *browser;
  QDialogButtonBox *buttons;
};

int cad_man_view(QString *args, CADApp *app);

#endif //QCADHELP_H

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

