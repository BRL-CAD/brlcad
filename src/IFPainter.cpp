#include "IFPainter.h"


IFPainter::IFPainter(int width, int height)
	: img(width, height, CV_8UC3, cv::Scalar(255, 255, 255))
{
	if (img.empty())
	{
		std::cerr << "ISSUE: Image Frame failed to load (in IFPainter.cpp)" << std::endl;
	}
	// TODO: this
}

IFPainter::~IFPainter()
{
	// TODO: this
}

void IFPainter::drawImage(int x, int y, int width, int height, std::string imgPath)
{
	// TODO: this
}

void IFPainter::drawText(int x, int y, int fontSize, int width, std::string text)
{
	// TODO: this
}

void IFPainter::drawLine(int x1, int y1, int x2, int y2, int width, cv::Scalar color)
{
	// TODO: this
}

void IFPainter::drawRect(int x1, int y1, int x2, int y2, cv::Scalar color)
{
	// TODO: this
}


void IFPainter::openInGUI()
{
	cv::namedWindow("Report", cv::WINDOW_AUTOSIZE);
	cv::imshow("Report", this->img);

	cv::waitKey(0); //wait infinite time for a keypress

	try
	{
		cv::destroyWindow("Report");
	}
	catch (std::exception& e)
	{
	}
	// TODO: this
}

void IFPainter::exportToFile(std::string filePath)
{
	cv::imwrite(filePath, this->img);

	// TODO: this
}