/*                     I F P A I N T E R . H
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
/** @file IFPainter.h
 *
 * Brief description
 *
 */

#pragma once

#define TO_BOLD 1
#define TO_ITAL 2
#define TO_WHITE 4
#define TO_ELLIPSIS 8
#define TO_UNDERLINE 16
#define TO_BLUE 32


#include "pch.h"

/*
 * The IFPainter class serves to store the actual image frame that will be modified and output in the report.
 * All drawing and exporting operations will be done via function calls to this class.
 *
 */

class IFPainter
{

private:
	cv::Mat img;
	
	int standardTextWeight;
	int boldTextWeight;
	
	std::map<int, int> heightToFontSizeMap;
	
	int getFontSizeFromHeightAndWidth(int height, int width, std::string text);

public:

	IFPainter(int width, int height);
	~IFPainter();

	std::pair<int, int> getCroppedImageDims(std::string imgPath);

	void drawImage(int x, int y, int width, int height, std::string imgPath);
	void drawImageFitted(int x, int y, int width, int height, std::string imgPath);
	void drawDiagramFitted(int x, int y, int width, int height, std::string imgPath, std::string text);
	void drawText(int x, int y, int height, int width, std::string text, int flags = 0);
	void drawTextCentered(int x, int y, int height, int width, std::string text, int flags = 0);
    void drawTextRightAligned(int x, int y, int height, int width, std::string text, int flags);
	void drawLine(int x1, int y1, int x2, int y2, int width, cv::Scalar color);
	void drawRect(int x1, int y1, int x2, int y2, int width, cv::Scalar color);
    void drawCirc(int x, int y, int radius, int width, cv::Scalar color);
	// void drawArc(int x, int y, int width, cv::Scalar color);
	int getTextWidth(int height, int width, std::string text, int flags = 0);
	void justify(int x, int y, int height, int width, std::vector<std::string> text, int flags = 0);
	void justifyWithCenterWord(int x, int y, int height, int width, std::string centerWord, std::vector<std::string> leftText, std::vector<std::string> rightText, int flags);
	void textWrapping(int x1, int y1, int x2, int y2, int width, int height, std::string text, int ellipsis, int numOfCharactersBeforeEllipsis, int flags = 0);

	void openInGUI();
	void exportToFile(std::string filePath);
};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
