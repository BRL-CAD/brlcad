/*                  A X E S R E N D E R E R . H
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
/** @file AxesRenderer.h */

#ifndef BRLCAD_AXESRENDERER_H
#define BRLCAD_AXESRENDERER_H


#include "Renderable.h"

class AxesRenderer: public Renderable {
    void render() override;
};


#endif //BRLCAD_AXESRENDERER_H
