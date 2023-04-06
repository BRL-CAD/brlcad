#include "IFPainter.h"


IFPainter::IFPainter(int width, int height)
	: img(width, height, CV_8UC3, cv::Scalar(255, 255, 255))
	, standardTextWeight(2)
	, boldTextWeight(4)
	, heightToFontSizeMap()
{
	if (img.empty())
	{
		std::cerr << "ISSUE: Image Frame failed to load (in IFPainter.cpp)" << std::endl;
	}
}

IFPainter::~IFPainter()
{
}

void IFPainter::drawImage(int x, int y, int width, int height, std::string imgPath)
{
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

void IFPainter::drawImageFitted(int x, int y, int width, int height, std::string imgPath)
{
    cv::Mat lilImage = imread(imgPath, cv::IMREAD_UNCHANGED);
	int imgWidth = lilImage.size().width;
	int imgHeight = lilImage.size().height;
	int heightOffset = 0;
	int widthOffset = 0;

	if ((double)imgWidth / imgHeight > (double)width / height)
	{
		// image width is too large; bound on width
		int newHeight = (int)(width * (double)imgHeight / imgWidth);
		heightOffset = (height - newHeight) / 2;
		height = newHeight;
	}
	else
	{
		// image height is too large; bound on height
		int newWidth = (int)(height * (double)imgWidth / imgHeight);
		widthOffset = (width - newWidth) / 2;
		width = newWidth;
	}

	cv::Mat resized_image;
	resize(lilImage, resized_image, cv::Size(width, height), cv::INTER_LINEAR);

	//Trying to copyto the image on to the private image frame, img
	cv::Mat destRoi;
	try {
		destRoi = img(cv::Rect(x + widthOffset, y + heightOffset, resized_image.cols, resized_image.rows));
	}
	catch (...) {
		std::cerr << "Trying to create roi out of image boundaries" << std::endl;
		return;
	}
	resized_image.copyTo(destRoi);
}

//Helper funciton to support getting the width of a text
int IFPainter::getTextWidth(int height, int width, std::string text, int flags)
{
	int fontWeight = (flags & TO_BOLD) ? boldTextWeight : standardTextWeight;
	int fontSize = getFontSizeFromHeightAndWidth(height, width, text);
	int textWidth = getTextSize(text, cv::FONT_HERSHEY_PLAIN, fontSize, fontWeight, 0).width;
	return textWidth;
}

int IFPainter::getFontSizeFromHeightAndWidth(int height, int width, std::string text)
{
	if (heightToFontSizeMap.find(height) != heightToFontSizeMap.end())
	{
		return heightToFontSizeMap[height];
	}

	int fontSize = 1;
	while (getTextSize("I", cv::FONT_HERSHEY_PLAIN, fontSize, standardTextWeight, 0).height < height)
		fontSize++;
	fontSize--;
	
	heightToFontSizeMap[height] = fontSize;
	while (getTextSize(text, cv::FONT_HERSHEY_PLAIN, fontSize, standardTextWeight, 0).width > width) {
		fontSize--;
	}
	return fontSize;
}

void IFPainter::drawText(int x, int y, int height, int width, std::string text, int flags)
{
	// for now, italic text is omitted
	int fontWeight = (flags & TO_BOLD) ? boldTextWeight : standardTextWeight;
	cv::Scalar color = (flags & TO_WHITE) ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);
	int fontSize = getFontSizeFromHeightAndWidth(height, width, text);

	cv::putText(img, text, cv::Point(x, y + height), cv::FONT_HERSHEY_PLAIN, fontSize, color, fontWeight);
}

void IFPainter::drawTextCentered(int x, int y, int height, int width, std::string text, int flags)
{
	// for now, italic text is omitted
	int fontWeight = (flags & TO_BOLD) ? boldTextWeight : standardTextWeight;
	cv::Scalar color = (flags & TO_WHITE) ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);
	int fontSize = getFontSizeFromHeightAndWidth(height, width, text);

	int textWidth = getTextSize(text, cv::FONT_HERSHEY_PLAIN, fontSize, fontWeight, 0).width;

	cv::putText(img, text, cv::Point(x - textWidth/2, y + height), cv::FONT_HERSHEY_PLAIN, fontSize, color, fontWeight);
}
void IFPainter::justify(int x, int y, int height, int width, std::vector<std::string> text, int flags)
{
	int totalTextWidth = 0;
	for (size_t i = 0; i < text.size(); i++) {
		totalTextWidth += getTextWidth(height, width, text[i], flags);
	}
	if (totalTextWidth >= width) {
		throw std::invalid_argument("Text is larger than the bounding box");
		return;
	}
	int fontWeight = (flags & TO_BOLD) ? boldTextWeight : standardTextWeight;
	cv::Scalar color = (flags & TO_WHITE) ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);
	int spacing = (width - totalTextWidth) / (text.size() + 1);
	int xPosition = x + spacing;
	for (size_t i = 0; i < text.size(); i++) {
		int fontSize = getFontSizeFromHeightAndWidth(height, width, text[i]);
		cv::putText(img, text[i], cv::Point(xPosition, y + height), cv::FONT_HERSHEY_PLAIN, fontSize, color, fontWeight);
		xPosition += spacing + getTextWidth(height, width, text[i], flags);
	}
}

void IFPainter::justifyConfidential(int x, int y, int height, int width, std::vector<std::string> leftText, std::vector<std::string> rightText, int flags)
{
	int confidentialWidth = getTextWidth(height, width, "CONFIDENTIAL", flags);
	drawTextCentered(width / 2, y, height, width, "CONFIDENTIAL", flags);
	int totalLeftWidth = 0;
	for (size_t i = 0; i < leftText.size(); i++) {
		totalLeftWidth += getTextWidth(height, width, leftText[i], flags);
	}
	int totalRightWidth = 0;
	for (size_t i = 0; i < leftText.size(); i++) {
		totalRightWidth += getTextWidth(height, width, rightText[i], flags);
	}
	int leftSpacing = (((width / 2) - (confidentialWidth / 2)) - totalLeftWidth) / (leftText.size() + 1);
	int rightSpacing = (((width / 2) - (confidentialWidth / 2)) - totalRightWidth) / (rightText.size() + 1);
	int spacing;
	if (totalLeftWidth >= (width / 2) - (confidentialWidth / 2) || totalRightWidth >= (width / 2) - (confidentialWidth / 2)) {
		throw std::invalid_argument("Text is larger than either the left or right side");
		return;
	}
	if (leftSpacing > rightSpacing) {
		spacing = rightSpacing;
	}
	else {
		spacing = leftSpacing;
	}

	int fontWeight = (flags & TO_BOLD) ? boldTextWeight : standardTextWeight;
	cv::Scalar color = (flags & TO_WHITE) ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);
	int xPosition = width / 2 - confidentialWidth / 2;
	for (int i = leftText.size() - 1; i >= 0; i--) {
		xPosition = xPosition - spacing - getTextWidth(height, width, leftText[i], flags);
		int fontSize = getFontSizeFromHeightAndWidth(height, width, leftText[i]);
		cv::putText(img, leftText[i], cv::Point(xPosition, y + height), cv::FONT_HERSHEY_PLAIN, fontSize, color, fontWeight);
	}

	xPosition = width / 2 + confidentialWidth / 2 + spacing;
	for (int i = 0; i < rightText.size(); i++) {
		int fontSize = getFontSizeFromHeightAndWidth(height, width, rightText[i]);
		cv::putText(img, rightText[i], cv::Point(xPosition, y + height), cv::FONT_HERSHEY_PLAIN, fontSize, color, fontWeight);
		xPosition = xPosition + spacing + getTextWidth(height, width, rightText[i], flags);
	}
}

void IFPainter::textWrapping(int x1, int y1, int x2, int y2, int width, int height, std::string text, int ellipsis, int numOfCharactersBeforeEllipsis, int flags)
{
	if (ellipsis == TO_ELLIPSIS) {
		if (numOfCharactersBeforeEllipsis < text.length()) {
			text = text.substr(0, numOfCharactersBeforeEllipsis) + "...";
		}
	}
	std::istringstream iss(text);
	std::vector<std::string> words;

	std::string word;
	while (iss >> word) { 
		words.push_back(word);
	}
	int x = x1;
	int y = y1;
	int wordWidth;
	int wordWidthWithSpace;
	int totalWordWidth;
	for (const auto& w : words) {
		wordWidth = getTextWidth(height, width, w, flags);
		wordWidthWithSpace = getTextWidth(height, width, w + " ", flags);
		if (x + wordWidth <= x2) {
			drawText(x, y, height, width, w + " ", flags);
			x += wordWidthWithSpace;
		}
		else {
			if (y + height <= y2 - height) {
				x = x1;
				y += height;
				drawText(x, y, height, width, w + " ", flags);
				x += wordWidthWithSpace;
			}
			else {
				throw std::invalid_argument("Text is larger than the bounding box");
				return;
			}
		}
	}
}

void IFPainter::drawLine(int x1, int y1, int x2, int y2, int width, cv::Scalar color)
{
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
}

void IFPainter::exportToFile(std::string filePath)
{
	cv::imwrite(filePath, this->img);
}