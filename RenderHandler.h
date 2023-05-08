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
 * When lockRows is true, all images on the same row MUST have the same height.
 * When lockRows is false, all images on the same column MUST have the same width.
 * 
 * The initCoordinates() "initializes" the layout, properly setting the coordinates of each image depending on the
 * model bounding box, maintaining both proportionality and maximum space usage.
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

	// DEPRECATED: OLD VERSION OF HEURISTIC
	// double getTotalCoverage(double ambientWidth, double ambientHeight);
};

// calls selectLayout to find the most effective layout, then paints it onto the designated area.
void makeRenderSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt);

// the currently used method which manually picks layouts based on model dimensions
LayoutChoice genLayout(double modelLength, double modelDepth, double modelHeight);


// DEPRECATED: OLD VERSION OF HEURISTIC
// generate the list of layouts
//std::vector<LayoutChoice> initLayouts();
// uses a simple heuristic to determine which layout is the best for the object.
//LayoutChoice selectLayout(int secWidth, int secHeight, double modelLength, double modelDepth, double modelHeight, std::pair<int, int> ambientDims);

