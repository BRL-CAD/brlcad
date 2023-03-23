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
	// TODO (Michael): Along with writing this method, figure out the best way to represent images in code.
	// Perhaps there's a better way to store images; reading in images multiple times is inefficient, though if we only need
	// to read in the image once (which I believe is the case), then reading images from a file should also be fine.
	// TODO: this
	cv::Mat lilImage = imread(imgPath, cv::IMREAD_UNCHANGED);
	cv::Mat resized_image;
	resize(lilImage, resized_image, cv::Size(width, height), cv::INTER_LINEAR);

	//Trying to copyto the image on to the private image frame, img
	cv::Mat destRoi;
	try {
		destRoi = img(cv::Rect(x, y, resized_image.cols, resized_image.rows));
	}
	catch (...) {
		std::cerr << "Trying to create roi out of image boundaries" << std::endl;
		return;
	}
	resized_image.copyTo(destRoi);
}

void IFPainter::drawText(int x, int y, int fontSize, int width, std::string text)
{
	// TODO: this
	// keep in mind that we'll need options for bold, italics, etc.
	// Try to make these methods as intuitive as possible in the idea of "we want text here, so we're confident that text will go here"
	// Keep in mind text wrapping.
}

void IFPainter::drawLine(int x1, int y1, int x2, int y2, int width, cv::Scalar color)
{
	// TODO: this
	int lineType = cv::LINE_8;
	cv::Point start(x1, y1);
	cv::Point end(x2, y2);
	line(img,
		start,
		end,
		color,
		width,
		lineType);
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