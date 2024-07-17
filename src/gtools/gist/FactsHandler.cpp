/*                F A C T S H A N D L E R . C P P
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

#include "FactsHandler.h"
#include "RenderHandler.h"

void
makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height)
{
    //scalar color order is BGR
    if (info.getInfo("classification") == "UNCLASSIFIED") {		//Green
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(51, 122, 0));
    } else if (info.getInfo("classification") == "CONFIDENTIAL") {//Blue
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(160, 51, 0));
    } else if (info.getInfo("classification") == "SECRET") {		//Red
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(46, 16, 200));
    } else if (info.getInfo("classification") == "TOP SECRET") {	//Orange
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(31, 103, 255));
    } else {	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));
    }

    int textHeight = 3 * height / 8;
    int textYOffset = (height - textHeight) / 2;
    std::vector<std::string> text;
    std::vector<std::string> text2;

    if (info.getInfo("classification") != "") {
	text.push_back("Owner: " + info.getInfo("owner"));
	text.push_back("MD5 Checksum: " + info.getInfo("checksum"));
	text2.push_back("Last Updated: " + info.getInfo("lastUpdate"));
	text2.push_back("Source File: " + info.getInfo("file"));
	img.justifyWithCenterWord(offsetX, offsetY + textYOffset, textHeight, width, info.getInfo("classification"), text, text2, TO_WHITE);
    } else {
	text.push_back("Owner: " + info.getInfo("owner"));
	text.push_back("MD5 Checksum: " + info.getInfo("checksum"));
	text.push_back("Last Updated: " + info.getInfo("lastUpdate"));
	text.push_back("Source File: " + info.getInfo("file"));
	img.justify(offsetX, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
    }
}

void
makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height)
{
    //scalar color order is BGR
    if (info.getInfo("classification") == "UNCLASSIFIED") {		//Green
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(51, 122, 0));
    } else if (info.getInfo("classification") == "CONFIDENTIAL") {//Blue
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(160, 51, 0));
    } else if (info.getInfo("classification") == "SECRET") {		//Red
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(46, 16, 200));
    } else if (info.getInfo("classification") == "TOP SECRET") {	//Orange
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(31, 103, 255));
    } else {	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));
    }

    int textHeight = 3 * height / 8;
    int textYOffset = (height - textHeight) / 2;
    std::vector<std::string> text;
    std::vector<std::string> text2;

    if (info.getInfo("classification") != "") {
	text.push_back("Preparer: " + info.getInfo("preparer"));
	text2.push_back("Date Generated: " + info.getInfo("dateGenerated"));
	img.justifyWithCenterWord(offsetX, offsetY + textYOffset, textHeight, width, info.getInfo("classification"), text, text2, TO_WHITE);
    } else {
	text.push_back("Preparer: " + info.getInfo("preparer"));
	text.push_back("Date Generated: " + info.getInfo("dateGenerated"));
	img.justify(offsetX, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
    }
}

void
makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options &opt)
{

    // draw bounding rectangle
    img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(220, 220, 220));

    int headerOffset = width / 20;
    int textOffset = width / 10;
    int textHeight = height / 50;
    int textYOffset = textHeight * 8 / 5;

    int curiX = 1;

    std::string summaryTitle = "\"" + info.largestComponents[0].name + "\" Summary";

    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Geometry Type", TO_BOLD);
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("representation"));
    curiX++;
    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Orientation", TO_BOLD);
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, opt.getCoordSystem_asString() + ", " + opt.getUpAxis_asString());
    curiX++;
    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, summaryTitle, TO_BOLD);
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("primitives") + " primitives, " + info.getInfo("regions") + " regions");
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("assemblies") + " assemblies");
    curiX++;
    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Dimensions", TO_BOLD);
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "x: " + info.getFormattedInfo("dimX"));
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "y: " + info.getFormattedInfo("dimY"));
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "z: " + info.getFormattedInfo("dimZ"));
    curiX++;
    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Presented Area (az/el)", TO_BOLD);
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getFormattedInfo("surfaceArea00") + " (0/0)");
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getFormattedInfo("surfaceArea090") + " (0/90)");
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getFormattedInfo("surfaceArea900") + " (90/0)");
    curiX++;
    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Approximate Volume", TO_BOLD);
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getFormattedInfo("volume"));
    curiX++;
    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Mass", TO_BOLD);
    img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getFormattedInfo("mass"));
    curiX++;
    img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Notes", TO_BOLD);
    img.textWrapping(offsetX + textOffset, offsetY + curiX * textYOffset, offsetX + width, (offsetY + curiX * textYOffset) + textHeight * 3, width, textHeight, opt.getNotes(), TO_ELLIPSIS, (width*height)/11000);
}

void
makeHierarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt)
{

    int textHeight = height / 20;

    int N = 4; // number of sub components you want
    int offY = height * (2.6 - 1) / 3 + offsetY;
    int offX = offsetX + 5;
    int imgH = height / 2.6;
    int imgW = (width - 5*fmin(N, info.largestComponents.size()-1)) / fmin(N, info.largestComponents.size()-1);

    int centerPt = offX + imgW/2 + (fmin(N-1, info.largestComponents.size()-2)*imgW) / 2;

    // main component
    //img.drawImageFitted(offX + width/10, offsetY + textHeight/3, imgW, imgH, render);
    img.drawTextCentered(offsetX + width / 2, offY - 180, textHeight, width, info.largestComponents[0].name, TO_BOLD);

    img.drawLine(offX + imgW/2, offY - 100, offX + fmin(N-1, info.largestComponents.size()-2)*imgW + imgW/2, offY - 100, 3, cv::Scalar(94, 58, 32));
    img.drawLine(centerPt, offY-100, centerPt, offY-130, 3, cv::Scalar(94, 58, 32));
    img.drawCirc(centerPt, offY-130, 7, -1, cv::Scalar(94, 58, 32));
    // img.drawCirc(centerPt, offY-30, 20, 3, cv::Scalar(94, 58, 32));

    // entity summary
    // int curiX = 0;
    // img.drawTextRightAligned(offsetX + width*9/10, offsetY + 20 + curiX * textYOffset, textHeight/1.3, width, "Groups & Assemblies:", TO_BOLD);
    // img.drawText(offsetX + width*9/10, offsetY + 20 + curiX++ * textYOffset, textHeight/1.3, width, " " + info.getInfo("groups_assemblies"), TO_BOLD);
    // img.drawTextRightAligned(offsetX + width*9/10, offsetY + 20 + curiX * textYOffset, textHeight/1.3, width, "Regions & Parts:", TO_BOLD);
    // img.drawText(offsetX + width*9/10, offsetY + 20 + curiX++ * textYOffset, textHeight/1.3, width, " " + info.getInfo("regions_parts"), TO_BOLD);
    // img.drawTextRightAligned(offsetX + width*9/10, offsetY + 20 + curiX * textYOffset, textHeight/1.3, width, "Primitive Shapes:", TO_BOLD);
    // img.drawText(offsetX + width*9/10, offsetY + 20 + curiX++ * textYOffset, textHeight/1.3, width, " " + info.getInfo("primitives"), TO_BOLD);



    // sub components
    std::string render = "";
    for (int i = 1; i < fmin(N, info.largestComponents.size()); i++) {
	render = renderPerspective(GHOST, opt, info.largestComponents[i].name, info.largestComponents[0].name);
	// std::cout << "INSIDE factshandler DBG: " << render << std::endl;
	img.drawImageTransparentFitted(offX + (i-1)*imgW, offY, imgW, imgH, render);
	img.drawTextCentered(offX + (i-1)*imgW + imgW/2, offY-70, textHeight, width, info.largestComponents[i].name, TO_BOLD);
	img.drawLine(offX + (i-1)*imgW + imgW/2, offY-100, offX + (i-1)*imgW + imgW/2, offY-70, 3, cv::Scalar(94, 58, 32));
	img.drawCirc(offX + (i-1)*imgW + imgW/2, offY-70, 7, -1, cv::Scalar(94, 58, 32));
	// img.drawCirc(offX + (i-1)*imgW + imgW/2, offY+10, 20, 3, cv::Scalar(94, 58, 32));
	// img.drawLine(offX + (i-1)*imgW + imgW/2 - imgW/10, offY+10, offX + (i-1)*imgW + imgW/2 + imgW/10, offY+10, 3, cv::Scalar(0, 0, 0));
    }

    if (info.largestComponents.size() > (size_t)N) {
	// render the smaller sub components all in one
	std::string subcomponents = "";
	for (size_t i = N; i < info.largestComponents.size(); i++) {
	    subcomponents += info.largestComponents[i].name + " ";
	}
	render = renderPerspective(GHOST, opt, subcomponents, info.largestComponents[0].name);
	img.drawImageTransparentFitted(offX + (N-1)*imgW, offY, imgW, imgH, render);
	img.drawTextCentered(offX + (N-1)*imgW + imgW/2, offY-70, textHeight, width, "...", TO_BOLD);
	img.drawLine(offX + (N-1)*imgW + imgW/2, offY-100, offX + (N-1)*imgW + imgW/2, offY-70, 3, cv::Scalar(94, 58, 32));
	img.drawCirc(offX + (N-1)*imgW + imgW/2, offY-70, 7, -1, cv::Scalar(94, 58, 32));
	// img.drawCirc(offX + (N-1)*imgW + imgW/2, offY+10, 20, 3, cv::Scalar(94, 58, 32));
	// img.drawLine(offX + (N-1)*imgW + imgW/2 - imgW/10, offY+10, offX + (N-1)*imgW + imgW/2 + imgW/10, offY+10, 3, cv::Scalar(0, 0, 0));
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
