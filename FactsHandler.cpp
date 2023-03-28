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
	std::string text = "Preparer: " + info.getInfo("preparer") + " Source File: " + info.getInfo("file");
	img.drawText(offsetX + width / 100, offsetY + height / 1.5, width / 1500, width / 2000, text, false, true);
}

void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	/*
		Draw the bounds for the file information section and gather text data
	*/
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	// Store subtitles and data in vectors and loop to display them
	std::string Title = "File Information";
	std::vector<std::string> subtitles;
	std::vector<std::string> data;
}
void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	/*
		Draw the bounds for the file information section and gather text data
	*/
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	// Store subtitles and data in vectors and loop to display them
	std::string Title = "Verification";
	std::vector<std::string> subtitles;
	std::vector<std::string> data;
}

void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));
}

void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

	// Store subtitles and data in vectors and loop to display them
	std::string Title = "V&V Checks";
	std::vector<std::string> subtitles;
	std::vector<std::string> data;
}
