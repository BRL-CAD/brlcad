#include "FactsHandler.h"

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0,0,0));

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;

	//Draw text on top
	std::string text = "Owner: " + info.getInfo("owner") + "     Version: " + info.getInfo("version") + "     Last Updated: " + info.getInfo("lastUpdate") + "     Classification: " + info.getInfo("classification");
	img.drawTextCentered(offsetX + width / 2, offsetY + textYOffset, textHeight, text, TO_WHITE);
}

void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) { 
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;

	//Draw text on bottom
	std::string text = "Preparer: " + info.getInfo("preparer") + "     Source File: " + info.getInfo("file") + "     Date Generated: " + info.getInfo("dateGenerated") + "     Model Checksum: " + info.getInfo("checksum");
	img.drawTextCentered(offsetX + width / 2, offsetY + textYOffset, textHeight, text, TO_WHITE);
}

void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	// draw bounding rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int headerOffset = width / 20;
	int textOffset = width / 10;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, "File Information", TO_BOLD);
	
	int curiX = 3;

	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Geometry Type", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "(Sample) Implicit - CSG");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "File Extension", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "(Sample) .g");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Orientation", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "(Sample) Right-Hand, Z-Up");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Entity Summary", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "XXX primitives, 0 regions");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "0 assemblies, 336 total");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Notes", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "(Sample) My Notes");
	
}
void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int headerOffset = width / 20;
	int textOffset = width / 10;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, "Verification", TO_BOLD);

	int curiX = 3;

	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Unit", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, info.getInfo("units"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Approximate Volume", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "(Sample) 912 m^3");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Surface Area", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "(Sample) 100 m^2 (3D)");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "(Sample) 128 m^2 (Projected)");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, "Mass", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, "2.5 Tonnes");
}

void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int textHeight = height / 20;
	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, "Object Hierarchy", TO_BOLD);
}

void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	int boxOffset = width / 40;
	int textHeightTitle = height / 20;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	int textOffset = 2 * boxOffset + textHeight;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeightTitle, "V&V Checks", TO_BOLD);

	// left side
	int curX = offsetX;
	int curY = offsetY + 2 * textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Has Material Properties");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Has Optical Properties");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Has Analytical Properties");
	curY += textOffset;

	// right side
	curX = offsetX + width*7/16;
	curY = offsetY + 2 * textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Watertight and Solid");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Has Overlaps/Interferences");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Has Domain-Specific Issues");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Suitable for Scientific Analysis");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Suitable for Gaming/Visualization");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, "Suitable for 3D Printing");
	curY += textOffset;
}
