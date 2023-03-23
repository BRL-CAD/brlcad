#include "RenderHandler.h"

LayoutChoice::LayoutChoice(std::string map, bool ambientOnBottom)
	: map(map), ambientOnBottom(ambientOnBottom), coordinates()
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
		if (map[i-1] == '\n')
			rowStartingIndex.push_back(i);
	}

	// number of rows is simply equal to the number of starting row indices.
	int numRows = rowStartingIndex.size();
	// number of columns is equal to the distance between two starting indexes of rows, omitting the '\n'
	int numCols = rowStartingIndex[1] - rowStartingIndex[0] - 1;


	// to find the dimensions of the grid, 
	std::vector<int> rowHeights;
	std::vector<int> columnWidths;
	// these variables represent the total height/width of the orthographic views, without extra space.
	int orthoHeight = 0;
	int orthoWidth = 0;

	// calculate the height of each row
	for (int r = 0; r < numRows; ++r)
	{
		int maxHeight = 0;

		// traverse the row until a '\n' is reached
		for (int i = rowStartingIndex[r]; map[i] != '\n' && i < map.size(); ++i)
		{
			switch (map[i])
			{
			case ' ': case '\n': case '_': case '|': // items with no area
				break;
			default:
				RenderingFace face = (RenderingFace)(map[i] - '0'); // get the rendering face using the index (next - '0')

				ModelDimension dim = faceDetails[face].heightContributor;

				if (dim == LENGTH)
					maxHeight = std::max(maxHeight, modelLength);
				else if (dim == DEPTH)
					maxHeight = std::max(maxHeight, modelDepth);
				else if (dim == HEIGHT)
					maxHeight = std::max(maxHeight, modelHeight);
			}
		}

		rowHeights.push_back(maxHeight);
		orthoHeight += maxHeight;
	}

	// calculate the width of each column
	// calculate the height of each row
	for (int c = 0; c < numCols; ++c)
	{
		int maxWidth = 0;

		// traverse the column.  Each element in the column will be the c'th character after a row start index
		for (int i : rowStartingIndex)
		{
			switch (map[i + c])
			{
			case ' ': case '\n': case '_': case '|': // items with no area
				break;
			default:
				RenderingFace face = (RenderingFace)(map[i + c] - '0'); // get the rendering face using the index (next - '0')

				ModelDimension dim = faceDetails[face].widthContributor;

				if (dim == LENGTH)
					maxWidth = std::max(maxWidth, modelLength);
				else if (dim == DEPTH)
					maxWidth = std::max(maxWidth, modelDepth);
				else if (dim == HEIGHT)
					maxWidth = std::max(maxWidth, modelHeight);
			}
		}

		columnWidths.push_back(maxWidth);
		orthoWidth += maxWidth;
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
	double widthConversionFactor = (double) actualOrthoWidth / orthoWidth;
	double heightConversionFactor = (double)actualOrthoHeight / orthoHeight;

	// now, filling out the coordinates
	int curX = offsetX, curY = offsetY;
	int curRow = 0, curCol = 0;

	for (int i = 0; i < map.size(); ++i)
	{
		int curHeight = (int) (heightConversionFactor * rowHeights[curRow]);

		if (map[i] == '\n')
		{
			// just finished traversing a row.  Move to the next row.
			curRow++;
			curCol = 0;

			// update curX and curY accordingly
			curX = offsetX;
			curY += curHeight;

			continue;
		}

		int curWidth = (int) (widthConversionFactor * columnWidths[curCol]);

		coordinates[i].push_back(curX);
		coordinates[i].push_back(curY);
		coordinates[i].push_back(curX + curWidth);
		coordinates[i].push_back(curY + curHeight);

		curCol++;
		curX += curWidth;
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
		case ' ': case '\n': case '_': case '|': // items with no area
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

	layouts.emplace_back("1 \n02\n__\n43\n5 \n__\n", true);
	layouts.emplace_back("1 |\n02|\n__\n43|\n5 |\n", false);

	return layouts;
}

void makeRenderSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height)
{
	int modelLength = 100;
	int modelDepth = 100;
	int modelHeight = 200;

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
		case '|': case '_': // draw separator line
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
		return LayoutChoice("", false);
	}

	return *bestLayout;
}