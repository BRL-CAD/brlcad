#include "FactsHandler.h"

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0,0,0));

	//Draw text on top
	std::string text = "Owner: " + info.getInfo("owner") + " Version: " + info.getInfo("version") + " Last Updated: " + info.getInfo("lastUpdate") + " Classification: " + info.getInfo("classification");
	img.drawText(offsetX + width / 100, offsetY + height / 1.5, width / 1500, width / 2000, text, false, true);
}

void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) { 
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));

	//Draw text on top
	std::string text = "Preparer: " + info.getInfo("preparer") + " Source File: " + info.getInfo("file") + " Date Generated: " + info.getInfo("dateGenerated") + " Model Checksum: " + info.getInfo("checksum");
	img.drawText(offsetX + width / 100, offsetY + height / 1.5, width / 1500, width / 2000, text, false, true);
}