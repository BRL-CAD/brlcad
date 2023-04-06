#pragma once

#define TO_BOLD 1
#define TO_ITAL 2
#define TO_WHITE 4
#define TO_ELLIPSIS 6


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

	void drawImage(int x, int y, int width, int height, std::string imgPath);
	void drawImageFitted(int x, int y, int width, int height, std::string imgPath);
	void drawText(int x, int y, int height, int width, std::string text, int flags = 0);
	void drawTextCentered(int x, int y, int height, int width, std::string text, int flags = 0);
	void drawLine(int x1, int y1, int x2, int y2, int width, cv::Scalar color);
	void drawRect(int x1, int y1, int x2, int y2, int width, cv::Scalar color);
	int getTextWidth(int height, int width, std::string text, int flags = 0);
	void justify(int x, int y, int height, int width, std::vector<std::string> text, int flags = 0);
	void justifyWithCenterWord(int x, int y, int height, int width, std::string centerWord, std::vector<std::string> leftText, std::vector<std::string> rightText, int flags);
	void textWrapping(int x1, int y1, int x2, int y2, int width, int height, std::string text, int ellipsis, int numOfCharactersBeforeEllipsis, int flags = 0);

	void openInGUI();
	void exportToFile(std::string filePath);

};

