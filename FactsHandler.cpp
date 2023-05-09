#include "FactsHandler.h"
#include "RenderHandler.h"

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	//Draw black rectangle
	if (info.getInfo("classification") == "CONFIDENTIAL") {
		img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 255));
	}
	else {
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
	}
	else {
		text.push_back("Owner: " + info.getInfo("owner"));
		text.push_back("Checksum: " + info.getInfo("checksum"));
		text.push_back("Last Updated : " + info.getInfo("lastUpdate"));
		text.push_back("Source File : " + info.getInfo("file"));
		img.justify(offsetX, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
	}
}

void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) { 
	//Draw black rectangle
	if (info.getInfo("classification") == "CONFIDENTIAL") {
		img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 255));
	}
	else {
		img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));
	}

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;
	std::vector<std::string> text;
	std::vector<std::string> text2;

	if (info.getInfo("classification") != "") {
		text.push_back("Preparer: " + info.getInfo("preparer"));
		text2.push_back("Date Generated : " + info.getInfo("dateGenerated"));
		img.justifyWithCenterWord(offsetX, offsetY + textYOffset, textHeight, width, info.getInfo("classification"), text, text2, TO_WHITE);
	}
	else {
		text.push_back("Preparer: " + info.getInfo("preparer"));
		text.push_back("Date Generated : " + info.getInfo("dateGenerated"));
		img.justify(offsetX, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
	}
}

void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options &opt) {
	//Determine units
	std::string unit;
	if (opt.isDefaultLength()) {
		unit = info.getInfo("units");
	}
	else {
		unit = opt.getUnitLength();
	}

	// draw bounding rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(220, 220, 220));

	int headerOffset = width / 20;
	int textOffset = width / 10;
	int textHeight = height / 50;
	int textYOffset = textHeight * 8 / 5;

	// Verification Calculations
	 // Calculate column offsets
	int col1Offset = (offsetX + width / 4) - textOffset;
	int col2Offset = offsetX + width / 2;
	int col3Offset = (offsetX + (width*3) / 4) + textOffset;

	int curiX = 0;

	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Geometry Type", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("representation"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Orientation", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, opt.getOrientationRightLeft() + ", " + opt.getOrientationZYUp());
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Entity Summary", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("primitives") + " primitives, " + info.getInfo("regions") + " regions");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("assemblies") + " assemblies, " + info.getInfo("total") + " total");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Units", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("units"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Dimensions (x, y, z)", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("dimX") + " " + unit);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("dimY") + " " + unit);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("dimZ") + " " + unit);
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Presented Area (az/el)", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("surfaceArea00") + " (0/0)");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("surfaceArea090") + " (0/90)");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("surfaceArea900") + " (90/0)");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Approximate Volume", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("volume"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Mass", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("mass"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Notes", TO_BOLD);
	img.textWrapping(offsetX + textOffset, offsetY + curiX * textYOffset, offsetX + width, (offsetY + curiX * textYOffset) + textHeight * 3, width, textHeight, opt.getNotes(), TO_ELLIPSIS, (width*height)/11000);

}

void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt) {
	// img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

  int textOffset = width / 10;
  int textHeight = height / 20;
  int textXOffset = textHeight * 53 / 5;
  int textYOffset = textHeight * 8 / 5;

	// img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "Object Hierarchy", TO_BOLD);

  int N = 4; // number of sub components you want
  int offY = height / 2 + offsetY;
  int offX = offsetX + 5;
  int imgH = height / 2;
  int imgW = (width - 5*fmin(N, info.largestComponents.size()-1)) / fmin(N, info.largestComponents.size()-1);

  int centerPt = offX + imgW/2 + (fmin(N-1, info.largestComponents.size()-2)*imgW) / 2;

  // main component
  std::string render = renderPerspective(DETAILED, opt, info.largestComponents[0].name);
  //img.drawImageFitted(offX + width/10, offsetY + textHeight/3, imgW, imgH, render);
  img.drawTextCentered(offsetX + width / 2, offY - 180, textHeight, width, info.largestComponents[0].name, TO_BOLD);

  img.drawLine(offX + imgW/2, offY - 100, offX + fmin(N-1, info.largestComponents.size()-2)*imgW + imgW/2, offY - 100, 3, cv::Scalar(94, 58, 32));
  img.drawLine(centerPt, offY-100, centerPt, offY-130, 3, cv::Scalar(94, 58, 32));
  img.drawCirc(centerPt, offY-130, 7, -1, cv::Scalar(94, 58, 32));
  // img.drawCirc(centerPt, offY-30, 20, 3, cv::Scalar(94, 58, 32));

    // entity summary
    int curiX = 0;
    // img.drawTextRightAligned(offsetX + width*9/10, offsetY + 20 + curiX * textYOffset, textHeight/1.3, width, "Groups & Assemblies:", TO_BOLD);
    // img.drawText(offsetX + width*9/10, offsetY + 20 + curiX++ * textYOffset, textHeight/1.3, width, " " + info.getInfo("groups_assemblies"), TO_BOLD);
	// img.drawTextRightAligned(offsetX + width*9/10, offsetY + 20 + curiX * textYOffset, textHeight/1.3, width, "Regions & Parts:", TO_BOLD);
	// img.drawText(offsetX + width*9/10, offsetY + 20 + curiX++ * textYOffset, textHeight/1.3, width, " " + info.getInfo("regions_parts"), TO_BOLD);
	// img.drawTextRightAligned(offsetX + width*9/10, offsetY + 20 + curiX * textYOffset, textHeight/1.3, width, "Primitive Shapes:", TO_BOLD);
	// img.drawText(offsetX + width*9/10, offsetY + 20 + curiX++ * textYOffset, textHeight/1.3, width, " " + info.getInfo("primitives"), TO_BOLD);

    
    
    // sub components
    for (int i = 1; i < fmin(N, info.largestComponents.size()); i++) {
        render = renderPerspective(GHOST, opt, info.largestComponents[i].name, info.largestComponents[0].name);
        // std::cout << "INSIDE factshandler DBG: " << render << std::endl;
        img.drawImageFitted(offX + (i-1)*imgW, offY, imgW, imgH, render);
        img.drawTextCentered(offX + (i-1)*imgW + imgW/2, offY-70, textHeight, width, info.largestComponents[i].name, TO_BOLD);
        img.drawLine(offX + (i-1)*imgW + imgW/2, offY-100, offX + (i-1)*imgW + imgW/2, offY-70, 3, cv::Scalar(94, 58, 32));
        img.drawCirc(offX + (i-1)*imgW + imgW/2, offY-70, 7, -1, cv::Scalar(94, 58, 32));
        // img.drawCirc(offX + (i-1)*imgW + imgW/2, offY+10, 20, 3, cv::Scalar(94, 58, 32));
        // img.drawLine(offX + (i-1)*imgW + imgW/2 - imgW/10, offY+10, offX + (i-1)*imgW + imgW/2 + imgW/10, offY+10, 3, cv::Scalar(0, 0, 0));
    }

    if (info.largestComponents.size() > N) {
        // render the smaller sub components all in one
        std::string subcomponents = "";
        for (int i = N; i < info.largestComponents.size(); i++)
            subcomponents += info.largestComponents[i].name + " ";
        render = renderPerspective(GHOST, opt, subcomponents, info.largestComponents[0].name);
        img.drawImageFitted(offX + (N-1)*imgW, offY, imgW, imgH, render);
        img.drawTextCentered(offX + (N-1)*imgW + imgW/2, offY-70, textHeight, width, "...", TO_BOLD);
        img.drawLine(offX + (N-1)*imgW + imgW/2, offY-100, offX + (N-1)*imgW + imgW/2, offY-70, 3, cv::Scalar(94, 58, 32));
        img.drawCirc(offX + (N-1)*imgW + imgW/2, offY-70, 7, -1, cv::Scalar(94, 58, 32));
        // img.drawCirc(offX + (N-1)*imgW + imgW/2, offY+10, 20, 3, cv::Scalar(94, 58, 32));
        // img.drawLine(offX + (N-1)*imgW + imgW/2 - imgW/10, offY+10, offX + (N-1)*imgW + imgW/2 + imgW/10, offY+10, 3, cv::Scalar(0, 0, 0));
    }
}






// Depricated Code





// void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
// 	// img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(220, 220, 220));

// 	int headerOffset = width / 20;
// 	int textOffset = width / 10;
// 	int textHeight = height / 30;
// 	int textYOffset = textHeight * 8 / 5;

// 	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "Verification", TO_BOLD);

// 	int curiX = 3;


// 	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Unit", TO_BOLD);
// 	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("units"));
  
// 	curiX++;
// 	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Approximate Volume", TO_BOLD);
// 	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("volume"));
// 	curiX++;
// 	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Surface Area", TO_BOLD);
// 	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("surfaceArea") + " (Projected)");
// 	//img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) 128 m^2 (Projected)");
// 	curiX++;
// 	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Mass", TO_BOLD);
// 	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("mass"));
// }

// void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
// 	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(220, 220, 220));

// 	int boxOffset = width / 40;
// 	int textHeightTitle = height / 20;
// 	int textHeight = height / 30;
// 	int textYOffset = textHeight * 8 / 5;

// 	int textOffset = 2 * boxOffset + textHeight;

// 	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeightTitle, width, "V&V Checks", TO_BOLD);

// 	// left side
// 	int curX = offsetX;
// 	int curY = offsetY + 2 * textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Has Material Properties");
// 	curY += textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Has Optical Properties");
// 	curY += textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Has Analytical Properties");
// 	curY += textOffset;

// 	// right side
// 	curX = offsetX + width*7/16;
// 	curY = offsetY + 2 * textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Watertight and Solid");
// 	curY += textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Has Overlaps/Interferences");
// 	curY += textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Has Domain-Specific Issues");
// 	curY += textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for Scientific Analysis");
// 	curY += textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for Gaming/Visualization");
// 	curY += textOffset;
// 	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
// 	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for 3D Printing");
// 	curY += textOffset;
// }
