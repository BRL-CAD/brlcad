#include "RenderHandler.h"

LayoutChoice::LayoutChoice(std::string map, bool lockRows)
	: map(map), lockRows(lockRows), coordinates()
{
	// make a vector to store coordinates of each item on the map AND ALSO the ambient occlusion view
	for (int i = 0; i < map.size() + 1; ++i)
	{
		coordinates.emplace_back();
	}
}

void LayoutChoice::initCoordinates(int secWidth, int secHeight, double modelLength, double modelDepth, double modelHeight)
{
	std::map<char, FaceDetails> faceDetails = getFaceDetails();

	// create workingWidth and workingHeight: the area in which we will place orthographic views
	double workingWidth = secWidth;
	double workingHeight = secHeight;

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

	int ambientR = -1;
	int ambientC = -1;
	for (int r = 0; r < numRows; ++r)
	{
		for (int c = 0; c < numCols; ++c)
		{
			int x = r * (numCols + 1) + c;
			if (map[x] == 'A')
			{
				ambientR = r;
				ambientC = c;
				break;
			}
		}
		if (ambientR != -1)
			break;
	}

	// to find the dimensions of the grid, 
	std::vector<double> rowHeights;
	std::vector<double> columnWidths;
	// these variables represent the total height/width of the orthographic views, without extra space.
	double orthoHeight = 0;
	double orthoWidth = 0;

	// traverse the first row to gather the width
	for (int i = 0; i < numCols; ++i)
	{
		double colWidth = 0;
		switch (map[i])
		{
		case '\n': case '-': case '|': case '.': case 'A':// items with no area
			break;
		default:
			FaceDetails fdet;

			if (map[i] == ' ')
				fdet = faceDetails[map[i + rowLen]]; // check the second row to snatch the width
			else
				fdet = faceDetails[map[i]]; // or use myself

			ModelDimension dim = fdet.widthContributor;

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
		double rowHeight = 0;
		switch (map[i])
		{
		case '\n': case '-': case '|': case '.': case 'A':// items with no area
			break;
		default:
			FaceDetails fdet;

			if (map[i] == ' ')
				fdet = faceDetails[map[i + 1]]; // check the second column to snatch the height
			else
				fdet = faceDetails[map[i]];

			ModelDimension dim = fdet.heightContributor;

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

	// update workingWidth and workingHeight to incorporate ambient occ
	if (ambientR == 0)
	{
		// ambient occlusion is on right side; take off some part of working width
		workingWidth *= 2.0 / 3;
	}
	else if (ambientC == 0)
	{
		// ambient occlusion is on bottom; take off some part of working height
		workingHeight *= 2.0 / 3;
	}
	else
	{
		double ambientXOffset = 0;
		double ambientYOffset = 0;
		// ambient occlusion is in middle; search row/col to get dimensions
		if (lockRows)
		{
			for (int i = 0; i < ambientR; ++i)
				ambientYOffset += rowHeights[i];
			for (int i = 0; i < ambientC; ++i)
			{
				switch (map[ambientR * rowLen + i])
				{
				case '-': case '|': case '.':
					continue;
				case '\n': case ' ': case 'A':
					std::cerr << "ISSUE: Found unexpected character!" << std::endl;
				default:
					ModelDimension dim = faceDetails[map[ambientR * rowLen + i]].widthContributor;

					if (dim == LENGTH)
						ambientXOffset += modelLength;
					else if (dim == DEPTH)
						ambientXOffset += modelDepth;
					else if (dim == HEIGHT)
						ambientXOffset += modelHeight;
				}
			}
		}
		else
		{
			for (int i = 0; i < ambientC; ++i)
				ambientXOffset += columnWidths[i];
			for (int i = 0; i < ambientR; ++i)
			{
				switch (map[i * rowLen + ambientC])
				{
				case '-': case '|': case '.':
					continue;
				case '\n': case ' ': case 'A':
					std::cerr << "ISSUE: Found unexpected character!" << std::endl;
				default:
					ModelDimension dim = faceDetails[map[i * rowLen + ambientC]].heightContributor;

					if (dim == LENGTH)
						ambientYOffset += modelLength;
					else if (dim == DEPTH)
						ambientYOffset += modelDepth;
					else if (dim == HEIGHT)
						ambientYOffset += modelHeight;
				}
			}
		}
		if (ambientXOffset > orthoWidth / 2)
			workingWidth /= 2 * (workingWidth - ambientXOffset);
		if (ambientYOffset > orthoHeight / 2)
			workingHeight /= 2 * (workingHeight - ambientYOffset);
	}

	// get aspect ratios
	double orthoAspectRatio = (double)orthoHeight / orthoWidth;
	double sectionAspectRatio = (double)workingHeight / workingWidth;

	// variables to store the actual width/height of the orthographic section, in image plane coordinate frame
	double actualOrthoWidth = 0;
	double actualOrthoHeight = 0;

	// fit the orthographic
	if (orthoAspectRatio > sectionAspectRatio)
	{
		// height of the orthographic section is relatively too large; cap the height.
		double conversionFactor = workingHeight / orthoHeight;

		actualOrthoHeight = (orthoHeight * conversionFactor);
		actualOrthoWidth = (orthoWidth * conversionFactor);
	}
	else
	{
		// width of the orthographic section is relatively too large; cap the width.
		double conversionFactor = workingWidth / orthoWidth;

		actualOrthoHeight = (orthoHeight * conversionFactor);
		actualOrthoWidth = (orthoWidth * conversionFactor);
	}

	// calculating variables used for finding the actual coordinates
	double offsetX = ambientC > 0 ? 0 : (workingWidth - actualOrthoWidth) / 2;
	double offsetY = ambientR > 0 ? 0 : (workingHeight - actualOrthoHeight) / 2;
	double widthConversionFactor = actualOrthoWidth / orthoWidth;
	double heightConversionFactor = actualOrthoHeight / orthoHeight;
	
	// now, filling out the coordinates
	if (lockRows)
	{
		// all elements in a row have the same height.  Iterate row by row
		double curX = offsetX, curY = offsetY, curWidth = 0, curHeight = 0;
		for (int r = 0; r < numRows; ++r)
		{
			curHeight = (heightConversionFactor * rowHeights[r]);

			for (int c = 0; c < numCols; ++c)
			{
				int i = rowLen * r + c;

				switch (map[i])
				{
				case '-':
					// check item above for the width
					curWidth = coordinates[i - rowLen][2] - coordinates[i - rowLen][0];
					break;
				case '|': case '.':
					// vertical lines have no width
					curWidth = 0;
					break;
				case 'A':
					curWidth = 0;
					break;
				default: // either ' ' or a rendering face
					FaceDetails fdet;

					// search for a neighbor that has a usable width (or use your own width, if valid)
					if (faceDetails.find(map[i]) != faceDetails.end())
						fdet = faceDetails[map[i]];
					else if (r > 0 && faceDetails.find(map[i-rowLen]) != faceDetails.end())
						fdet = faceDetails[map[i-rowLen]];
					else if (r < numRows - 1 && faceDetails.find(map[i+rowLen]) != faceDetails.end())
						fdet = faceDetails[map[i+rowLen]];
					else
					{
						std::cerr << "FATAL: couldn't find a width to use for rendering!" << std::endl;
					}

					ModelDimension dim = fdet.widthContributor;

					if (dim == LENGTH)
						curWidth = (widthConversionFactor * modelLength);
					else if (dim == DEPTH)
						curWidth = (widthConversionFactor * modelDepth);
					else if (dim == HEIGHT)
						curWidth = (widthConversionFactor * modelHeight);
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
		double curX = offsetX, curY = offsetY, curWidth = 0, curHeight = 0;
		for (int c = 0; c < numCols; ++c)
		{
			curWidth = (widthConversionFactor * columnWidths[c]);

			for (int r = 0; r < numRows; ++r)
			{
				int i = rowLen * r + c;

				switch (map[i])
				{
				case '|':
					// check item to the left for the height
					curHeight = coordinates[i - 1][3] - coordinates[i - 1][1];
					break;
				case '-': case '.':
					// horizontal lines have no height
					curHeight = 0;
					break;
				case 'A':
					curHeight = 0;
					break;
				default: // either ' ' or a rendering face
					FaceDetails fdet;
					
					// search for a neighbor that has a usable height (or use your own height, if valid)
					if (faceDetails.find(map[i]) != faceDetails.end())
						fdet = faceDetails[map[i]];
					else if (c > 0 && faceDetails.find(map[i - 1]) != faceDetails.end())
						fdet = faceDetails[map[i - 1]];
					else if (c < numCols - 1 && faceDetails.find(map[i + 1]) != faceDetails.end())
						fdet = faceDetails[map[i+1]];
					else
					{
						std::cerr << "FATAL: couldn't find a height to use for rendering!" << std::endl;
					}

					ModelDimension dim = fdet.heightContributor;

					if (dim == LENGTH)
						curHeight = (heightConversionFactor * modelLength);
					else if (dim == DEPTH)
						curHeight = (heightConversionFactor * modelDepth);
					else if (dim == HEIGHT)
						curHeight = (heightConversionFactor * modelHeight);
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

	coordinates[coordinates.size() - 1].push_back(ambientC == 0 ? 0 : coordinates[ambientR * rowLen + ambientC][0]);
	coordinates[coordinates.size() - 1].push_back(ambientR == 0 ? 0 : coordinates[ambientR * rowLen + ambientC][1]);
	coordinates[coordinates.size() - 1].push_back(secWidth);
	coordinates[coordinates.size() - 1].push_back(secHeight);

	// iterate through all of the rows, shifting them over if necessary
	for (int r = 0; r < numRows; ++r)
	{
		// test that row can be spaced
		bool works = true;
		for (int c = 0; c < numCols; ++c)
		{
			if (map[r * rowLen + c] == 'A')
				works = false;
		}

		if (! works || numCols <= 1)
			continue;
		double rowWidth = coordinates[r * rowLen + numCols - 1][2];
		double extraSpace = (secWidth - rowWidth) / (numCols - 1);

		// now, space out the row
		for (int c = 1; c < numCols; ++c)
		{
			coordinates[r * rowLen + c][0] += extraSpace * c;
			coordinates[r * rowLen + c][2] += extraSpace * c;
		}
	}

	// iterate through all of the columns, shifting them over if necessary
	for (int c = 0; c < numCols; ++c)
	{
		// test that column can be spaced
		bool works = true;
		for (int r = 0; r < numRows; ++r)
		{
			if (map[r * rowLen + c] == 'A')
				works = false;
		}

		if (!works || numRows <= 1)
			continue;
		double colHeight = coordinates[(numRows-1) * rowLen + c][3];
		double extraSpace = (secHeight - colHeight) / (numRows - 1);

		// now, space out the column
		for (int r = 1; r < numRows; ++r)
		{
			coordinates[r * rowLen + c][1] += extraSpace * r;
			coordinates[r * rowLen + c][3] += extraSpace * r;
		}
	}

	// iterate through all of the rows, shifting them over if necessary
	for (int r = 0; r < numRows; ++r)
	{
		// test that row can be spaced
		bool works = true;
		for (int c = 0; c < numCols; ++c)
		{
			if (map[r * rowLen + c] == 'A')
				works = false;
		}

		if (!works || numCols <= 1)
			continue;
		double rowWidth = coordinates[r * rowLen + numCols - 1][2];
		double extraSpace = (secWidth - rowWidth) / (numCols - 1);

		// now, space out the row
		for (int c = 1; c < numCols; ++c)
		{
			coordinates[r * rowLen + c][0] += extraSpace * c;
			coordinates[r * rowLen + c][2] += extraSpace * c;
		}
	}

	// iterate through all of the columns, shifting them over if necessary
	for (int c = 0; c < numCols; ++c)
	{
		// test that column can be spaced
		bool works = true;
		for (int r = 0; r < numRows; ++r)
		{
			if (map[r * rowLen + c] == 'A')
				works = false;
		}

		if (!works || numRows <= 1)
			continue;
		double colHeight = coordinates[(numRows - 1) * rowLen + c][3];
		double extraSpace = (secHeight - colHeight) / (numRows - 1);

		// now, space out the column
		for (int r = 1; r < numRows; ++r)
		{
			coordinates[r * rowLen + c][1] += extraSpace * r;
			coordinates[r * rowLen + c][3] += extraSpace * r;
		}
	}
}

double LayoutChoice::getTotalCoverage()
{
	double sum = 0;
	for (int i = 0; i < map.size(); ++i)
	{
		switch (map[i])
		{
		case ' ': case '\n': case '-': case '|': case '.': // items with no area
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
	std::vector<int> output;
	if (mapIndex == -1)
	{
		// want the coordinates for the ambient occlusion
		for (double coord : coordinates[map.size()])
			output.push_back((int)coord);
	}
	else
	{
		// want coordinates of an index in map
		for (double coord : coordinates[mapIndex])
			output.push_back((int)coord);
	}
	return output;
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

	layouts.emplace_back("1423\n05AA", false);
	layouts.emplace_back("102\n543\n.AA\n", false);

	return layouts;
}

void makeRenderSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt)
{
	double modelDepth = std::stod(info.getInfo("dimX"));
	double modelLength = std::stod(info.getInfo("dimY"));;
	double modelHeight = std::stod(info.getInfo("dimZ"));;
    // std::cout << "Model dimensions: " << modelLength << " " << modelDepth << " " << modelHeight << std::endl;
	// modelLength = 1683;
	// modelHeight = 625;
	// modelDepth = 1988;

	std::map<char, FaceDetails> faceDetails = getFaceDetails();

	LayoutChoice bestLayout = selectLayout(width, height, modelLength, modelDepth, modelHeight);

	// render all layour elements
	for (int i = 0; i < bestLayout.getMapSize(); ++i)
	{
		char next = bestLayout.getMapChar(i);
		std::vector<int> coords = bestLayout.getCoordinates(i);

		switch (next)
		{
		case ' ': case '\n': case '.': case 'A': // spacing character; nothing should be drawn here
			break;
		case '|': case '-': // draw separator line
			img.drawLine(offsetX + coords[0], offsetY + coords[1], offsetX + coords[2], offsetY + coords[3], 3, cv::Scalar(100, 100, 100));
			break;
		default: // draw face
			std::string render = renderPerspective(faceDetails[next].face, opt, info.largestComponents[0].name);

			//double GAP_PIXELS = 80;
			double oldW = coords[2] - coords[0];
			double oldH = coords[3] - coords[1];
			//double scale = oldW < oldH ? (oldW - GAP_PIXELS) / oldW : (oldH - GAP_PIXELS) / oldH;
			double scale = 0.92;
			double newW = oldW * scale;
			double newH = oldH * scale;
			double newX = coords[0] + oldW / 2 - newW / 2;
			double newY = coords[1] + oldH / 2 - newH / 2;

			img.drawImageFitted(offsetX + newX, offsetY + newY, newW, newH, render);
			/*
			if (next == '0')
			{
				img.drawLine(offsetX + newX + GAP_PIXELS / 8, offsetY + newY - GAP_PIXELS / 8, offsetX + newX + newW - GAP_PIXELS/8, offsetY + newY - GAP_PIXELS / 8, 2, cv::Scalar(160, 160, 160));
				img.drawLine(offsetX + newX - GAP_PIXELS / 8, offsetY + newY + GAP_PIXELS / 8, offsetX + newX - GAP_PIXELS / 8, offsetY + newY + newH - GAP_PIXELS / 8, 2, cv::Scalar(160, 160, 160));
			}
			if (next == '1')
			{
				img.drawLine(offsetX + newX - GAP_PIXELS / 8, offsetY + newY + GAP_PIXELS / 8, offsetX + newX - GAP_PIXELS / 8, offsetY + newY + newH - GAP_PIXELS / 8, 2, cv::Scalar(160, 160, 160));
			}*/
			break;
		}
	}

	// render ambient occlusion view
	std::vector<int> coords = bestLayout.getCoordinates(-1); // fetch ambient occlusion coordinates
	std::string render = renderPerspective(DETAILED, opt, info.largestComponents[0].name);
	img.drawDiagramFitted(offsetX + coords[0], offsetY + coords[1], coords[2] - coords[0], coords[3] - coords[1], render, info.getInfo("title"));
}

LayoutChoice selectLayout(int secWidth, int secHeight, double modelLength, double modelDepth, double modelHeight)
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
		return LayoutChoice("", true);
	}

	return *bestLayout;
}