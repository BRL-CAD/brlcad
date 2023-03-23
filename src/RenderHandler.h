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

struct LayoutChoice
{
private:
	std::string map;
	bool ambientOnBottom;

	std::vector<std::vector<int>> coordinates;
public:
	LayoutChoice(std::string map, bool ambientOnBottom);

	void initCoordinates(int secWidth, int secHeight, int modelLength, int modelDepth, int modelHeight);

	double getTotalCoverage();

	std::vector<int> getCoordinates(int mapIndex);

	int getMapSize();
	char getMapChar(int index);
};

// calls selectLayout to find the most effective layout, then paints it onto the designated area.
void makeRenderSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);

// generate the list of layouts
std::vector<LayoutChoice> initLayouts();

// uses a simple heuristic to determine which layout is the best for the object.
LayoutChoice selectLayout(int secWidth, int secHeight, int modelLength, int modelDepth, int modelHeight);

