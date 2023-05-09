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

std::pair<int, int> IFPainter::getCroppedImageDims(std::string imgPath)
{
	cv::Mat imageRaw = imread(imgPath, cv::IMREAD_UNCHANGED); // Load color image
	// Convert the image to grayscale for creating the mask
	cv::Mat gray_image;
	cv::cvtColor(imageRaw, gray_image, cv::COLOR_BGR2GRAY);
	// Create a mask of non-white pixels
	cv::Mat mask = gray_image < 255;
	// Find the bounding rectangle of non-white pixels
	cv::Rect bounding_rect = boundingRect(mask);
	// Crop the image to the bounding rectangle
	cv::Mat lilImage = imageRaw(bounding_rect);


	int imgWidth = lilImage.size().width;
	int imgHeight = lilImage.size().height;

	return std::pair(imgWidth, imgHeight);
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
	cv::Mat imageRaw = imread(imgPath, cv::IMREAD_UNCHANGED); // Load color image
	// Convert the image to grayscale for creating the mask
	cv::Mat gray_image;
	cv::cvtColor(imageRaw, gray_image, cv::COLOR_BGR2GRAY);
	// Create a mask of non-white pixels
	cv::Mat mask = gray_image < 255;
	// Find the bounding rectangle of non-white pixels
	cv::Rect bounding_rect = boundingRect(mask);
	// Crop the image to the bounding rectangle
	cv::Mat lilImage = imageRaw(bounding_rect);


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

	// prevent crashes
	if (width <= 0) width = 1;
	if (height <= 0) height = 1;

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

void IFPainter::drawDiagramFitted(int x, int y, int width, int height, std::string imgPath, std::string text)
{
	y += 65;
	height -= 65;
	cv::Mat imageRaw = imread(imgPath, cv::IMREAD_UNCHANGED); // Load color image
	// Convert the image to grayscale for creating the mask
	cv::Mat gray_image;
	cv::cvtColor(imageRaw, gray_image, cv::COLOR_BGR2GRAY);
	// Create a mask of non-white pixels
	cv::Mat mask = gray_image < 255;
	// Find the bounding rectangle of non-white pixels
	cv::Rect bounding_rect = boundingRect(mask);
	// Crop the image to the bounding rectangle
	cv::Mat lilImage = imageRaw(bounding_rect);


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

	// now, draw text and line
	this->drawLine(x + widthOffset, y + heightOffset - 5, x + widthOffset + width, y + heightOffset - 5, 5, cv::Scalar(0, 0, 0));
	this->drawTextCentered(x + widthOffset + width / 2, y + heightOffset - 65, 50, width, text, TO_BOLD);
}

//Helper funciton to support getting the width of a text
int IFPainter::getTextWidth(int height, int width, std::string text, int flags)
{
	int fontWeight = (flags & TO_BOLD) ? boldTextWeight : standardTextWeight;
	int fontSize = getFontSizeFromHeightAndWidth(height, width, text);
	int textWidth = getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fontSize, fontWeight, 0).width;
	return textWidth;
}

int IFPainter::getFontSizeFromHeightAndWidth(int height, int width, std::string text)
{
	if (heightToFontSizeMap.find(height) != heightToFontSizeMap.end())
	{
		return heightToFontSizeMap[height];
	}

	int fontSize = 1;
	while (getTextSize("I", cv::FONT_HERSHEY_DUPLEX, fontSize, standardTextWeight, 0).height < height)
		fontSize++;
	fontSize--;
	
	heightToFontSizeMap[height] = fontSize;
	while (getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fontSize, standardTextWeight, 0).width > width) {
		fontSize--;
	}
	return fontSize;
}

void IFPainter::drawText(int x, int y, int height, int width, std::string text, int flags)
{
	// for now, italic text is omitted
	int fontWeight = standardTextWeight;
	bool isUnderline = false;
	if ((flags & TO_BOLD)) {
    	fontWeight = boldTextWeight;
	}

	if (flags & TO_UNDERLINE) {
    	isUnderline = true;
	}
	

	cv::Scalar color = (flags & TO_WHITE) ? cv::Scalar(255, 255, 255) : (flags & TO_BLUE ? cv::Scalar(160, 0, 0) : cv::Scalar(0, 0, 0));
	int fontSize = getFontSizeFromHeightAndWidth(height, width, text);

	cv::Point textOrigin(x, y + height);
	cv::putText(img, text, cv::Point(x, y + height), cv::FONT_HERSHEY_DUPLEX, fontSize, color, fontWeight);

	if (isUnderline) {
        // Compute the position of the line below the text
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_PLAIN, fontSize, fontWeight, &baseline);
        int underlineY = textOrigin.y + baseline + 2;
        int underlineX1 = textOrigin.x;
        int underlineX2 = underlineX1 + textSize.width;

        // Draw the line using the same color as the text
        cv::line(img, cv::Point(underlineX1, underlineY), cv::Point(underlineX2, underlineY), color, 1);
    }
}

void IFPainter::drawTextCentered(int x, int y, int height, int width, std::string text, int flags)
{
	// for now, italic text is omitted
	int fontWeight = standardTextWeight;
	bool isUnderline = false;
	if ((flags & TO_BOLD)) {
    	fontWeight = boldTextWeight;
	}

	if (flags & TO_UNDERLINE) {
    	isUnderline = true;
	}


	cv::Scalar color = (flags & TO_WHITE) ? cv::Scalar(255, 255, 255) : (flags & TO_BLUE ? cv::Scalar(160, 0, 0) : cv::Scalar(0, 0, 0));
	int fontSize = getFontSizeFromHeightAndWidth(height, width, text);

	int textWidth = getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fontSize, fontWeight, 0).width;

	cv::Point textOrigin(x - textWidth/2, y + height);
	cv::putText(img, text, cv::Point(x - textWidth/2, y + height), cv::FONT_HERSHEY_DUPLEX, fontSize, color, fontWeight);

	if (isUnderline) {
        // Compute the position of the line below the text
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_PLAIN, fontSize, fontWeight, &baseline);
        int underlineY = textOrigin.y + baseline + 2;
        int underlineX1 = textOrigin.x;
        int underlineX2 = underlineX1 + textSize.width;

        // Draw the line using the same color as the text
        cv::line(img, cv::Point(underlineX1, underlineY), cv::Point(underlineX2, underlineY), color, 1);
    }
}


void IFPainter::drawTextRightAligned(int x, int y, int height, int width, std::string text, int flags)
{
	// for now, italic text is omitted
	int fontWeight = standardTextWeight;
	bool isUnderline = false;
	if ((flags & TO_BOLD)) {
    	fontWeight = boldTextWeight;
	}

	if (flags & TO_UNDERLINE) {
    	isUnderline = true;
	}


	cv::Scalar color = (flags & TO_WHITE) ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);
	int fontSize = getFontSizeFromHeightAndWidth(height, width, text);

	int textWidth = getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fontSize, fontWeight, 0).width;

	cv::Point textOrigin(x - textWidth, y + height);
	cv::putText(img, text, cv::Point(x - textWidth, y + height), cv::FONT_HERSHEY_DUPLEX, fontSize, color, fontWeight);

	if (isUnderline) {
        // Compute the position of the line below the text
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_PLAIN, fontSize, fontWeight, &baseline);
        int underlineY = textOrigin.y + baseline + 2;
        int underlineX1 = textOrigin.x;
        int underlineX2 = underlineX1 + textSize.width;

        // Draw the line using the same color as the text
        cv::line(img, cv::Point(underlineX1, underlineY), cv::Point(underlineX2, underlineY), color, 1);
    }
}

/*This function will evenly space out all of the texts in the header and footer. The algorithm is to get the total lenght of all of the words combined and then the spacing is the total 
length divided by how many words there are*/
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
		cv::putText(img, text[i], cv::Point(xPosition, y + height), cv::FONT_HERSHEY_DUPLEX, fontSize, color, fontWeight);
		xPosition += spacing + getTextWidth(height, width, text[i], flags);
	}
}
/*This is justification with a word being centered right in the middle. This is tricky now since the spacing is not going to be super straightforward. Essentially I split the section
into 2 and I used the same justify algorithm for the left and right side. Which ever side had the smallest spacing is the spacing I will choose for between the words.*/
void IFPainter::justifyWithCenterWord(int x, int y, int height, int width, std::string centerWord, std::vector<std::string> leftText, std::vector<std::string> rightText, int flags)
{
	int confidentialWidth = getTextWidth(height, width, centerWord, flags | TO_BOLD);
	drawTextCentered(width / 2, y, height, width, centerWord, flags | TO_BOLD);
	int totalLeftWidth = 0;
	for (size_t i = 0; i < leftText.size(); i++) {
		totalLeftWidth += getTextWidth(height, width, leftText[i], flags);
	}
	int totalRightWidth = 0;
	for (size_t i = 0; i < leftText.size(); i++) {
		totalRightWidth += getTextWidth(height, width, rightText[i], flags);
	}
	if (totalLeftWidth >= (width / 2) - (confidentialWidth / 2) || totalRightWidth >= (width / 2) - (confidentialWidth / 2)) {
		throw std::invalid_argument("Text is larger than either the left or right side");
		return;
	}
	int leftSpacing = (((width / 2) - (confidentialWidth / 2)) - totalLeftWidth) / (leftText.size() + 1);
	int rightSpacing = (((width / 2) - (confidentialWidth / 2)) - totalRightWidth) / (rightText.size() + 1);
	int spacing;
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
		cv::putText(img, leftText[i], cv::Point(xPosition, y + height), cv::FONT_HERSHEY_DUPLEX, fontSize, color, fontWeight);
	}

	xPosition = width / 2 + confidentialWidth / 2 + spacing;
	for (int i = 0; i < rightText.size(); i++) {
		int fontSize = getFontSizeFromHeightAndWidth(height, width, rightText[i]);
		cv::putText(img, rightText[i], cv::Point(xPosition, y + height), cv::FONT_HERSHEY_DUPLEX, fontSize, color, fontWeight);
		xPosition = xPosition + spacing + getTextWidth(height, width, rightText[i], flags);
	}
}

//This is a text wrapping function. If a text goes beyond the set x or y, it will wrap around to the next line. It also has ellipsis fucntion.
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

void IFPainter::drawCirc(int x, int y, int radius, int width, cv::Scalar color)
{
    circle(img, cv::Point(x,y), radius, color, width, cv::LINE_8);
    // ellipse(img, cv::Point(x,y), cv::Size(radius, radius), 0, 0, 180, color, width, cv::LINE_8);
}

// void IFPainter::drawArc(int x, int y, int width, cv::Scalar color) {
// 	int lineType = cv::LINE_8;
// 	cv::Point center(x, y);
//     for(int i=10;i<=250;i=i+10)
//     {
//         ellipse( image, center, Size( i, i ), 0, 30, 150, color, width, lineType );
//     }
// }


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


