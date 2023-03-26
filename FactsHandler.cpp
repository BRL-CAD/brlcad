#include "FactsHandler.h"

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0,0,0));

	//Draw text on top
	std::string owner = "Owner: " + info.getInfo("owner");
	img.drawText(offsetX+150, offsetY + 30, 1, 1, owner, false, true);

	std::string version = "Version: " + info.getInfo("version");
	img.drawText(offsetX + 550, offsetY + 30, 1, 1, version, false, true);

	std::string lastUpdate = "Last Updated: " + info.getInfo("lastUpdate");
	img.drawText(offsetX + 900, offsetY + 30, 1, 1, lastUpdate, false, true);

	std::string classification = "Classification: " + info.getInfo("classification");
	img.drawText(offsetX + 1250, offsetY + 30, 1, 1, classification, false, true);
}

void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) { 
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));

	//Draw text on top
	std::string preparer = "Preparer: " + info.getInfo("preparer");
	img.drawText(offsetX + 150, offsetY + 30, 1, 1, preparer, false, true);
}