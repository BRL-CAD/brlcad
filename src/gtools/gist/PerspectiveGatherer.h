/*           P E R S P E C T I V E G A T H E R E R . H
 * BRL-CAD
 *
 * Copyright (c) 2023-2026 United States Government as represented by
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

#pragma once

#include "pch.h"
/**
 * The PerspectiveGatherer files contain utility methods for the
 * RenderHandler class.
 *
 * These methods predominantly feature different rendering types /
 * perspectives.
 *
 */

class Options;

enum RenderingFace
{
    FRONT,
    TOP,
    RIGHT,
    LEFT,
    BACK,
    BOTTOM,
    DETAILED,
    GHOST
};

enum ModelDimension
{
    LENGTH,
    DEPTH,
    HEIGHT
};

struct FaceDetails
{
    RenderingFace face;
    std::string title;

    ModelDimension widthContributor;
    ModelDimension heightContributor;
};

struct RenderRequest
{
    RenderingFace face;
    std::string component;
    std::string ghost;
};

std::map<char,FaceDetails> getFaceDetails();

/** Render one model perspective using the configured CPU limit. */
std::string renderPerspective(RenderingFace face, Options& opt, std::string component, std::string ghost="");

/**
 * Render a group of independent perspectives.  Preview requests may run in
 * parallel, while full-quality requests remain sequential.
 */
std::vector<std::string> renderPerspectives(const std::vector<RenderRequest> &requests, Options &opt);

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
