/*                  F A C T S H A N D L E R . H
 * BRL-CAD
 *
 * Copyright (c) 2023-2024 United States Government as represented by
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
 * The FactsHandler code autonomously modifies a section of the report page to include
 * multiple textual sections of information about the graphics file.
 *
 * The code will automatically layout the information in its designated section after
 * retreiving said information from the InformationGatherer helper file.
 */

class InformationGatherer;
class IFPainter;

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt);
void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt);
void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
