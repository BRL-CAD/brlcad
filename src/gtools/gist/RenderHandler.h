/*                 R E N D E R H A N D L E R . H
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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
/** @file RenderHandler.h
 *
 * Brief description
 *
 */

#pragma once

#include "pch.h"

/**
 * The RenderHandler code autonomously modifies a section of the report page to include
 * multiple visuals of the graphics file from a multitude of perspectives and rendering
 * types (prominently ray tracers).
 * 
 * The code will automatically layout the section based on the dimensions of the graphics
 * file and will use the PerspectiveGatherer code to generate the renderings.  Then, this
 * code will populate its designated section on the report page with this information.
 * 
 */

class IFPainter;
class InformationGatherer;
class Options;

/**
 * The LayoutChoice class stores a layout encoding along with providing the functionality to determine
 * how to place elements on a page if that layout were to be selected.  The class encodes a layout using
 * two variables: map and "lockRows".
 * 
 * The map is encoded as a string, storing the 2D grid which makes up how the images will be placed.
 * Depending on the value of "lockRows", either a row-major (true) or column-major (false) ordering
 * will be followed when placing the images.
 * 
 * Each orthogonal face is encoded as a character.  Each row is terminated by a '\n'.
 * The mapping is as follows: F: front, T: top, R: right, L: left, B: back, b: bottom, A: ambient occlusion
 * Empty space is encoded as a '.', where nothing should be placed.
 * 
 * When lockRows is true, all images on the same row MUST have the same height.
 * When lockRows is false, all images on the same column MUST have the same width.
 * 
 * Here's an example of how LayoutChoice("TbFR\n..BL\n..AA\n", false) will be represented:
 * 
 *  ---------------    The diagram to the left shows the grid layout for the above layout.  Since "lockRows" is false,
 * | T | b | F | R |   a column-major ordering is used.  The top, bottom, front, and right views will be placed first
 * |---|---|---|---|   at the top of the page.  Then, the back view will be placed immediately below the front view,
 * |   |   | B | L |   with the same width as the front view.  A similar process is followed for the left view (placed
 * |---|---|-------|   below the right view).  Finally, the ambient view is placed just below the back and left views.
 * |   |   |  Amb  |   The ambient view will not protrude any space that is not marked as "A".  The layout to the left
 *  ---------------    is therefore useful for models where the top/bottom images are tall whilst the other views aren't.
 * 
 * The initCoordinates() "initializes" the layout, properly setting the coordinates of each image depending on the
 * model bounding box, maintaining both proportionality and maximum space usage.
 * 
 * The getTotalCoverage() returns a determined value proportional to the amount of whitespace used.  Generally, the higher
 * this value, the better match this layout is for a model.
*/
struct LayoutChoice
{
private:
	std::string map;
	bool lockRows;

	std::vector<std::vector<double>> coordinates;
public:
	std::vector<std::pair<int, int>> dimDetails;

	LayoutChoice(std::string map, bool lockRows);

	void initCoordinates(int secWidth, int secHeight, double modelLength, double modelDepth, double modelHeight);

	std::vector<int> getCoordinates(int mapIndex);

	int getMapSize();
	char getMapChar(int index);

	double getTotalCoverage(double ambientWidth, double ambientHeight);
};

// calls selectLayout to find the most effective layout, then paints it onto the designated area.
void makeRenderSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt);

// generate a list of layouts
std::vector<LayoutChoice> initLayouts();

// uses a simple heuristic to determine which layout is the best for the object.
LayoutChoice selectLayoutFromHeuristic(int secWidth, int secHeight, double modelLength, double modelDepth, double modelHeight, std::pair<int, int> ambientDims);

// the currently used method which manually picks layouts based on model dimension ratios
// if a fit isn't found (i.e. awkward dimensions), then the heuristic is called to find the best model that
LayoutChoice genLayout(int secWidth, int secHeight, double modelLength, double modelDepth, double modelHeight, std::pair<int, int> ambientDims);






/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
