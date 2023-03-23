#include "RenderHandler.h"

LayoutChoice::LayoutChoice(std::string map, bool ambientOnBottom, bool lockRows)
	: map(map), ambientOnBottom(ambientOnBottom), lockRows(lockRows), coordinates()
{
	// make a vector to store coordinates of each item on the map AND ALSO the ambient occlusion view
	for (int i = 0; i < map.size() + 1; ++i)
	{
		coordinates.emplace_back();
	}
}

void LayoutChoice::initCoordinates(int secWidth, int secHeight, int modelLength, int modelDepth, int modelHeight)
{
	std::map<RenderingFace, FaceDetails> faceDetails = getFaceDetails();

	// create workingWidth and workingHeight: the area in which we will place orthographic views
	int workingWidth = secWidth;
	int workingHeight = secHeight;

	// allocate some space for the ambient occlusion, removing it from workingWidth/workingHeight.
	if (ambientOnBottom)
	{
		// use bottom third for ambient occlusion
		workingHeight = workingHeight * 2 / 3;
	}
	else
	{
		// use right-most third for ambient occlusion
		workingWidth = workingWidth * 2 / 3;
	}

	// create an array to store the starting index of each row.
	std::vector<int> rowStartingIndex;
	rowStartingIndex.push_back(0);

	for (int i = 1; i < map.size(); ++i)
	{
		// Each row starts after a "\n"
		if (map[i - 1] == '\n')
			rowStartingIndex.push_back(i);
	}

	// number of rows is simply equal to the number of starting row indices.
	int numRows = rowStartingIndex.size();
	// number of columns is equal to the distance between two starting indexes of rows, omitting the '\n'
	int numCols = rowStartingIndex[1] - rowStartingIndex[0] - 1;
	int rowLen = numCols + 1; // length of a row is the number of columns plus the newline character.

	// to find the dimensions of the grid, 
	std::vector<int> rowHeights;
	std::vector<int> columnWidths;
	// these variables represent the total height/width of the orthographic views, without extra space.
	int orthoHeight = 0;
	int orthoWidth = 0;


	// calculate the height of each row

	// traverse the first row to gather the width
	for (int i = 0; i < numCols; ++i)
	{
		int colWidth = 0;
		switch (map[i])
		{
		case '\n': case '-': case '|': // items with no area
			break;
		default:
			RenderingFace face = (RenderingFace)(map[i] - '0'); // get the rendering face using the index (next - '0')

			if (map[i] >= '0' && map[i] <= '9')
				face = (RenderingFace)(map[i] - '0'); // get the rendering face using the index (next - '0');
			else face = (RenderingFace)(map[i+rowLen] - '0'); // check the second row to snatch the width

			ModelDimension dim = faceDetails[face].widthContributor;

			if (dim == LENGTH)
				colWidth = modelLength;
			else if (dim == DEPTH)
				colWidth = modelDepth;
			else if (dim == HEIGHT)
				colWidth = modelHeight;
		}
		if (!lockRows)
			columnWidths.push_back(colWidth);
		orthoWidth += colWidth;
	}

	// traverse the first column to gather the height
	for (int j = 0; j < numRows; ++j)
	{
		int i = rowLen * j;
		int rowHeight = 0;
		switch (map[i])
		{
		case '\n': case '-': case '|': // items with no area
			break;
		default:
			RenderingFace face;

			if (map[i] >= '0' && map[i] <= '9')
				face = (RenderingFace)(map[i] - '0'); // get the rendering face using the index (next - '0');
			else face = (RenderingFace)(map[i + 1] - '0'); // check the second column to snatch the height

			ModelDimension dim = faceDetails[face].heightContributor;

			if (dim == LENGTH)
				rowHeight = modelLength;
			else if (dim == DEPTH)
				rowHeight = modelDepth;
			else if (dim == HEIGHT)
				rowHeight = modelHeight;
		}
		if (lockRows)
			rowHeights.push_back(rowHeight);
		orthoHeight += rowHeight;
	}

	// get aspect ratios
	double orthoAspectRatio = (double)orthoHeight / orthoWidth;
	double sectionAspectRatio = (double)workingHeight / workingWidth;

	// variables to store the actual width/height of the orthographic section, in image plane coordinate frame
	int actualOrthoWidth = 0;
	int actualOrthoHeight = 0;

	// fit the orthographic
	if (orthoAspectRatio > sectionAspectRatio)
	{
		// height of the orthographic section is relatively too large; cap the height.
		double conversionFactor = (double)workingHeight / orthoHeight;

		actualOrthoHeight = (int)(orthoHeight * conversionFactor);
		actualOrthoWidth = (int)(orthoWidth * conversionFactor);
	}
	else
	{
		// width of the orthographic section is relatively too large; cap the width.
		double conversionFactor = (double)workingWidth / orthoWidth;

		actualOrthoHeight = (int)(orthoHeight * conversionFactor);
		actualOrthoWidth = (int)(orthoWidth * conversionFactor);
	}

	// now, if we have extra space on the page after this adjustment, allocate it for the ambient occlusion view
	if (ambientOnBottom && actualOrthoHeight < workingHeight)
	{
		workingHeight = actualOrthoHeight;
	}
	else if (!ambientOnBottom && actualOrthoWidth < workingWidth)
	{
		workingWidth = actualOrthoWidth;
	}

	// calculating variables used for finding the actual coordinates
	int offsetX = (workingWidth - actualOrthoWidth) / 2;
	int offsetY = (workingHeight - actualOrthoHeight) / 2;
	double widthConversionFactor = (double)actualOrthoWidth / orthoWidth;
	double heightConversionFactor = (double)actualOrthoHeight / orthoHeight;
	
	// now, filling out the coordinates
	if (lockRows)
	{
		// all elements in a row have the same height.  Iterate row by row
		int curX = offsetX, curY = offsetY, curWidth = 0, curHeight = 0;
		for (int r = 0; r < numRows; ++r)
		{
			curHeight = (int)(heightConversionFactor * rowHeights[r]);

			for (int c = 0; c < numCols; ++c)
			{
				int i = rowLen * r + c;

				std::cout << "processing " << map[i] << " at " << r << " " << c << std::endl;

				switch (map[i])
				{
				case '-':
					// check item above for the width
					curWidth = coordinates[i - rowLen][2] - coordinates[i - rowLen][0];
					break;
				case '|':
					// vertical lines have no width
					curWidth = 0;
					break;
				default: // either ' ' or a rendering face
					RenderingFace face;

					// search for a neighbor that has a usable width (or use your own width, if valid)
					if (map[i] >= '0' && map[i] <= '9')
						face = (RenderingFace)(map[i] - '0'); // get the rendering face using the index (next - '0')
					else if (r > 0 && map[i - rowLen] >= '0' && map[i - rowLen] <= '9')
						face = (RenderingFace)(map[i - rowLen] - '0'); // get the rendering face using the index (next - '0')
					else if (r < numRows - 1 && map[i + rowLen] >= '0' && map[i + rowLen] <= '9')
						face = (RenderingFace)(map[i + rowLen] - '0'); // get the rendering face using the index (next - '0')
					else
					{
						std::cerr << "FATAL: couldn't find a width to use for rendering!" << std::endl;
					}

					ModelDimension dim = faceDetails[face].widthContributor;

					if (dim == LENGTH)
						curWidth = (int)(widthConversionFactor * modelLength);
					else if (dim == DEPTH)
						curWidth = (int)(widthConversionFactor * modelDepth);
					else if (dim == HEIGHT)
						curWidth = (int)(widthConversionFactor * modelHeight);
				}

				coordinates[i].push_back(curX);
				coordinates[i].push_back(curY);
				coordinates[i].push_back(curX + curWidth);
				coordinates[i].push_back(curY + curHeight);

				curX += curWidth;
			}

			curX = offsetX;
			curY += curHeight;
		}
	}
	else
	{
		// all elements in a column have the same width.  Iterate column by column
		int curX = offsetX, curY = offsetY, curWidth = 0, curHeight = 0;
		for (int c = 0; c < numCols; ++c)
		{
			curWidth = (int)(widthConversionFactor * columnWidths[c]);

			for (int r = 0; r < numRows; ++r)
			{
				int i = rowLen * r + c;

				switch (map[i])
				{
				case '|':
					// check item to the left for the height
					curHeight = coordinates[i - 1][3] - coordinates[i - 1][1];
					break;
				case '-':
					// horizontal lines have no height
					curHeight = 0;
					break;
				default: // either ' ' or a rendering face
					RenderingFace face;
					
					// search for a neighbor that has a usable height (or use your own height, if valid)
					if (map[i] >= '0' && map[i] <= '9')
						face = (RenderingFace)(map[i] - '0'); // get the rendering face using the index (next - '0')
					else if (c > 0 && map[i-1] >= '0' && map[i-1] <= '9')
						face = (RenderingFace)(map[i-1] - '0'); // get the rendering face using the index (next - '0')
					else if (c < numCols-1 && map[i + 1] >= '0' && map[i + 1] <= '9')
						face = (RenderingFace)(map[i + 1] - '0'); // get the rendering face using the index (next - '0')
					else
					{
						std::cerr << "FATAL: couldn't find a height to use for rendering!" << std::endl;
					}

					ModelDimension dim = faceDetails[face].heightContributor;

					if (dim == LENGTH)
						curHeight = (int)(heightConversionFactor * modelLength);
					else if (dim == DEPTH)
						curHeight = (int)(heightConversionFactor * modelDepth);
					else if (dim == HEIGHT)
						curHeight = (int)(heightConversionFactor * modelHeight);
				}

				coordinates[i].push_back(curX);
				coordinates[i].push_back(curY);
				coordinates[i].push_back(curX + curWidth);
				coordinates[i].push_back(curY + curHeight);

				curY += curHeight;
			}

			curY = offsetY;
			curX += curWidth;
		}
	}

	// add ambient occlusion coordinates
	if (ambientOnBottom)
	{
		coordinates[coordinates.size() - 1].push_back(offsetX);
		coordinates[coordinates.size() - 1].push_back(workingHeight);
		coordinates[coordinates.size() - 1].push_back(offsetX + actualOrthoWidth);
		coordinates[coordinates.size() - 1].push_back(secHeight);
	}
	else
	{
		coordinates[coordinates.size() - 1].push_back(workingWidth);
		coordinates[coordinates.size() - 1].push_back(offsetY);
		coordinates[coordinates.size() - 1].push_back(secWidth);
		coordinates[coordinates.size() - 1].push_back(offsetY + actualOrthoHeight);
	}
}

double LayoutChoice::getTotalCoverage()
{
	double sum = 0;
	for (int i = 0; i < map.size(); ++i)
	{
		switch (map[i])
		{
		case ' ': case '\n': case '-': case '|': // items with no area
			break;
		default:
			if (coordinates[i].empty())
			{
				std::cerr << "ISSUE: coordinates for an important map section not initialized!" << std::endl;
				break;
			}
			sum += (coordinates[i][2] - coordinates[i][0]) * (coordinates[i][3] - coordinates[i][1]);
		}
	}

	return sum;
}

std::vector<int> LayoutChoice::getCoordinates(int mapIndex)
{
	if (mapIndex == -1)
	{
		// want the coordinates for the ambient occlusion
		return coordinates[map.size()];
	}
	else
	{
		// want coordinates of an index in map
		return coordinates[mapIndex];
	}
}

int LayoutChoice::getMapSize()
{
	return map.size();
}

char LayoutChoice::getMapChar(int index)
{
	return map[index];
}



std::vector<LayoutChoice> initLayouts()
{
	std::vector<LayoutChoice> layouts;

	layouts.emplace_back("1 \n02\n--\n43\n5 \n--\n", true, true);
	layouts.emplace_back("1 |\n02|\n-- \n43|\n5 |\n", false, true);
	layouts.emplace_back("1 |43|\n02|5 |\n", false, false);

	return layouts;
}

void makeRenderSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height)
{
	int modelLength = 300;
	int modelDepth = 100;
	int modelHeight = 100;

	LayoutChoice bestLayout = selectLayout(width, height, modelLength, modelDepth, modelHeight);

	// render all layour elements
	for (int i = 0; i < bestLayout.getMapSize(); ++i)
	{
		char next = bestLayout.getMapChar(i);
		std::vector<int> coords = bestLayout.getCoordinates(i);

		switch (next)
		{
		case ' ': case '\n': // spacing character; nothing should be drawn here
			break;
		case '|': case '-': // draw separator line
			img.drawLine(coords[0], coords[1], coords[2], coords[3], 1, cv::Scalar(100, 100, 100));
			break;
		default: // draw face
			RenderingFace face = (RenderingFace)(next - '0'); // get the rendering face using the index (next - '0')

			std::string render = renderPerspective(face);

			img.drawImage(coords[0], coords[1], coords[2] - coords[0], coords[3] - coords[1], render);
			break;
		}
	}

	// render ambient occlusion view
	std::vector<int> coords = bestLayout.getCoordinates(-1); // fetch ambient occlusion coordinates
	std::string render = renderPerspective(DETAILED);
	img.drawImage(coords[0], coords[1], coords[2] - coords[0], coords[3] - coords[1], render);
}

LayoutChoice selectLayout(int secWidth, int secHeight, int modelLength, int modelDepth, int modelHeight)
{
	std::vector<LayoutChoice> allLayouts = initLayouts();

	double bestScore = -1;
	LayoutChoice* bestLayout = NULL;

	for (LayoutChoice& lc : allLayouts)
	{
		lc.initCoordinates(secWidth, secHeight, modelLength, modelDepth, modelHeight);

		double score = lc.getTotalCoverage();

		if (score > bestScore)
		{
			bestScore = score;
			bestLayout = &lc;
		}
	}

	if (bestLayout == NULL)
	{
		std::cerr << "ISSUE: no layouts found.  This is a major problem!  In selectLayout() in RenderHandler.cpp" << std::endl;
		return LayoutChoice("", false, true);
	}

	return *bestLayout;
}