/*                      B I N D I N G S . H
 * BRL-CAD
 *
 * Copyright (c) 2021 United States Government as represented by
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
/** @file bindings.h
 *
 * CAD specific logic for use in Qt event binding overrides.
 *
 */

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "dm/bview.h"

int CADkeyPressEvent(struct bview *v, int x_prev, int y_prev, QKeyEvent *k);

int CADmousePressEvent(struct bview *v, int x_prev, int y_prev, QMouseEvent *e);

int CADmouseMoveEvent(struct bview *v, int x_prev, int y_prev, QMouseEvent *e);

int CADwheelEvent(struct bview *v, QWheelEvent *e);

