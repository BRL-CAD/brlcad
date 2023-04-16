#include "FactsHandler.h"
#include "RenderHandler.h"

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0,0,0));

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;
	std::vector<std::string> text;
	std::vector<std::string> text2;

	if (info.getInfo("classification") != "") {
		text.push_back("Owner: " + info.getInfo("owner"));
		text.push_back("Checksum: " + info.getInfo("checksum"));
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
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));

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

	int curiX = 2;

	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Geometry Type", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("representation"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "File Extension", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("extension"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Orientation", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, opt.getOrientationRightLeft() + ", " + opt.getOrientationZYUp());
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Entity Summary", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("primitives") + " primitives, " + info.getInfo("regions") + " regions");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("assemblies") + " assemblies, " + info.getInfo("total") + " total");

	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Surface Area - Projected and Exposed", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "128.222298 in^2");


    img.drawTextCentered(col1Offset, offsetY + textHeight + curiX * textYOffset, textHeight, (width - 2 * headerOffset) / 3, "Unit", TO_BOLD);
    img.drawTextCentered(col2Offset, offsetY + textHeight + curiX * textYOffset, textHeight, (width - 2 * headerOffset) / 3, "Volume", TO_BOLD);
    img.drawTextCentered(col3Offset, offsetY + textHeight + curiX * textYOffset, textHeight, (width - 2 * headerOffset) / 3, "Mass", TO_BOLD);
	curiX++;
	img.drawTextCentered(col1Offset, offsetY + textHeight + curiX * textYOffset, textHeight, width, info.getInfo("units"));
	img.drawTextCentered(col2Offset, offsetY + textHeight + curiX * textYOffset, textHeight, width, "912 m^3");
	img.drawTextCentered(col3Offset, offsetY + textHeight + curiX * textYOffset, textHeight, width, "2.5 Tonnes");
	curiX+=2;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Notes", TO_BOLD);
	img.textWrapping(offsetX + textOffset, offsetY + curiX++ * textYOffset, offsetX + width, offsetY + (curiX + 3) * textYOffset, width, textHeight, opt.getNotes(), TO_ELLIPSIS, (width*height)/8750);

}

void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt) {
	// img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

    int textOffset = width / 10;
	int textHeight = height / 20;
    int textXOffset = textHeight * 53 / 5;
    int textYOffset = textHeight * 15 / 5;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "Object Hierarchy", TO_BOLD);

    int curiX = 0;
    int curiY = 3;

	// img.drawText(offsetX + textOffset, offsetY + curiY++ * textYOffset, textHeight, width, "Test", TO_BOLD);
	// img.drawText(offsetX + textOffset, offsetY + curiY++ * textYOffset, textHeight, width, "Test2", TO_BOLD);

    // TODO put in for loop to prevent out of bounds. 
	std::string render = renderPerspective(DETAILED, info.largestComponents[0].second, opt);
    // main component
    img.drawImageFitted(offsetX + 5, offsetY + textYOffset, width - 10, height / 2 - textYOffset, render);

    int offY = height / 2 + offsetY;
    int offX = offsetX + 5;

    int imgH = height / 2;
    int imgW = (width - (info.largestComponents.size()-1)*5) / (info.largestComponents.size()-1);
    
    // sub components
    for (int i = 1; i < info.largestComponents.size(); i++) {
        render = renderPerspective(DETAILED, info.largestComponents[i].second, opt);
        img.drawImageFitted(offX + (i-1)*imgW, offY, imgW, imgH, render);
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
