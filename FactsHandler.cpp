#include "FactsHandler.h"

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0,0,0));

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;

	//Draw text on top
	std::string text;
	if (info.getInfo("classification") == "CONFIDENTIAL") {
		text = "Owner: " + info.getInfo("owner") + "     Version: " + info.getInfo("version") + "     " + info.getInfo("classification") + "     Last Updated: " + info.getInfo("lastUpdate") + "     Source File: " + info.getInfo("file");
	}
	else {
		text = "Owner: " + info.getInfo("owner") + "     Version: " + info.getInfo("version") + "     Last Updated: " + info.getInfo("lastUpdate") + "     Source File: " + info.getInfo("file");
	}
	img.drawTextCentered(offsetX + width / 2, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
}

void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) { 
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;

	//Draw text on bottom
	std::string text;
	if (info.getInfo("classification") == "CONFIDENTIAL") {
		text = "Preparer: " + info.getInfo("preparer") + "     " + info.getInfo("classification") + "     Date Generated: " + info.getInfo("dateGenerated");
	}
	else {
		text = "Preparer: " + info.getInfo("preparer") + "     Date Generated: " + info.getInfo("dateGenerated");
	}
	img.drawTextCentered(offsetX + width / 2, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
}

void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	// draw bounding rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int headerOffset = width / 20;
	int textOffset = width / 10;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "File Information", TO_BOLD);
	
	int curiX = 3;

	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Geometry Type", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) Implicit - CSG");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "File Extension", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("extension"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Orientation", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) Right-Hand, Z-Up");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Entity Summary", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("primitives") + " primitives, " + info.getInfo("regions") + " regions");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "? assemblies, " + info.getInfo("total") + " total");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Notes", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) My Notes");
	
}
void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int headerOffset = width / 20;
	int textOffset = width / 10;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "Verification", TO_BOLD);

	int curiX = 3;

	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Unit", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, info.getInfo("units"));

	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Approximate Volume", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) 912 m^3");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Surface Area", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) 100 m^2 (3D)");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) 128 m^2 (Projected)");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Mass", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "2.5 Tonnes");
}

void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int textHeight = height / 20;
	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "Object Hierarchy", TO_BOLD);
}

void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int boxOffset = width / 40;
	int textHeightTitle = height / 20;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	int textOffset = 2 * boxOffset + textHeight;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeightTitle, width, "V&V Checks", TO_BOLD);

	// left side
	int curX = offsetX;
	int curY = offsetY + 2 * textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Material Properties");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Optical Properties");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Analytical Properties");
	curY += textOffset;

	// right side
	curX = offsetX + width*7/16;
	curY = offsetY + 2 * textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Watertight and Solid");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Overlaps/Interferences");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Domain-Specific Issues");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for Scientific Analysis");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for Gaming/Visualization");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for 3D Printing");
	curY += textOffset;
}
