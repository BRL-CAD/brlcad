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

	//storing the desired image and resizing it to the desired width and height
	cv::Mat lilImage = imread(imgPath , cv::IMREAD_UNCHANGED);
	cv::Mat resized_image;
	resize(lilImage, resized_image, cv::Size(width, height), cv::INTER_LINEAR);

	//Trying to copyto the image on to the private image frame, img
	cv::Mat destRoi;
	try {
		destRoi = img(cv::Rect(x, y, resized_image.cols, resized_image.rows));
	}  catch (...) {
		std::cerr << "Trying to create roi out of image boundaries" << std::endl;
        return;
	}
	resized_image.copyTo(destRoi);
}

void IFPainter::drawText(int x, int y, double fontSize, int font_weight, std::string text, bool italics)
{
	// TODO: this
	// keep in mind that we'll need options for bold, italics, etc.
	// Try to make these methods as intuitive as possible in the idea of "we want text here, so we're confident that text will go here"
	// Keep in mind text wrapping.

	cv::Point text_position(x, y);
	cv::Scalar font_color(0, 0, 0);
	//boldness can be adjusted through font_weight
	//The italics doesnt look really look like the traditional italics
	if (italics == true) {
		putText(img, text, text_position, cv::FONT_ITALIC, fontSize, font_color, font_weight);
	}
	putText(img, text, text_position, cv::FONT_HERSHEY_PLAIN, fontSize, font_color, font_weight);
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

void IFPainter::drawRect(int x1, int y1, int x2, int y2, int width, cv::Scalar color)
{
	// TODO: this
	//Should I try to fill the rectangle?
	cv::Point topLeft(x1, y1);
	cv::Point bottomRight(x2, y2);
	rectangle(img,
			  topLeft,
			  bottomRight,
              color,
              width,
			  cv::LINE_8);
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