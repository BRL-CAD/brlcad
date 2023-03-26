#pragma once

#include "pch.h"

/*
 * The IFPainter class serves to store the actual image frame that will be modified and output in the report.
 * All drawing and exporting operations will be done via function calls to this class.
 *
 */

#pragma once
class IFPainter
{

private:
	cv::Mat img;

public:

	IFPainter(int width, int height);
	~IFPainter();

	void drawImage(int x, int y, int width, int height, std::string imgPath);
	void drawText(int x, int y, double fontSize, int font_weight, std::string text, bool italics, bool isWhite);
	void drawImageFitted(int x, int y, int width, int height, std::string imgPath);
	void drawLine(int x1, int y1, int x2, int y2, int width, cv::Scalar color);
	void drawRect(int x1, int y1, int x2, int y2, int width, cv::Scalar color);

	void openInGUI();
	void exportToFile(std::string filePath);

};

