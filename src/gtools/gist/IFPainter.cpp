/*                   I F P A I N T E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023-2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "IFPainter.h"


IFPainter::IFPainter(int width, int height)
    : img(width, height, CV_8UC3, cv::Scalar(255, 255, 255))
    , standardTextWeight(2)
    , boldTextWeight(4)
    , heightToFontSizeMap()
{
    if (img.empty()) {
	bu_log("ERROR: Image Frame failed to load\n");
    }
}

IFPainter::~IFPainter()
{
}

std::pair<int, int>
IFPainter::getCroppedImageDims(std::string imgPath)
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

    return std::pair<int, int>(imgWidth, imgHeight);
}


void
IFPainter::drawImage(int x, int y, int width, int height, std::string imgPath)
{
    cv::Mat lilImage = imread(imgPath, cv::IMREAD_UNCHANGED);
    cv::Mat resized_image;
    resize(lilImage, resized_image, cv::Size(width, height), cv::INTER_LINEAR);

    //Trying to copyto the image on to the private image frame, img
    cv::Mat destRoi;
    try {
	destRoi = img(cv::Rect(x, y, resized_image.cols, resized_image.rows));
    } catch (...) {
	bu_log("WARNING: Trying to create roi out of image boundaries\n");
	return;
    }
    resized_image.copyTo(destRoi);
}

void
IFPainter::drawTransparentImage(int x, int y, int width, int height, std::string imgPath, int threshold)
{
    cv::Mat imageRaw = cv::imread(imgPath, cv::IMREAD_UNCHANGED);
    if (imageRaw.empty()) {
	// read error
	bu_log("WARNING: could not use (%s)\n", imgPath.c_str());
	return;
    }
    // Convert the image to grayscale for creating the mask
    cv::Mat gray_image;
    cv::cvtColor(imageRaw, gray_image, cv::COLOR_BGR2GRAY);

    // threshold gray image to create mask
    cv::Mat mask;
    cv::threshold(gray_image, mask, threshold, 255, cv::THRESH_BINARY_INV);

    // if image doesn't have alpha, manually try to mask out background
    cv::Mat alphaRaw(imageRaw.rows, imageRaw.cols, CV_8UC4);
    if (imageRaw.channels() == 3) {	    // BGR
	// create 4-channel RGBA - using mask as alpha
	std::vector<cv::Mat> split_raw(3);
	cv::split(imageRaw, split_raw);
	split_raw.push_back(mask);
	cv::merge(split_raw, alphaRaw);
    } else if (imageRaw.channels() == 4) {  // BGR-A
	// input image already has alpha channel
	alphaRaw = imageRaw;
    } else {
	bu_log("WARNING: error loading image (%s)\n", imgPath.c_str());
	return;
    }

    // Find the bounding rectangle of non-white pixels
    cv::Rect bounding_rect = boundingRect(mask);
    // Crop the image to the bounding rectangle
    cv::Mat lilImage = alphaRaw(bounding_rect);

    int imgWidth = lilImage.size().width;
    int imgHeight = lilImage.size().height;
    int heightOffset = 0;
    int widthOffset = 0;

    // maintain original aspect ratio
    if ((double)imgWidth / imgHeight > (double)width / height) {
	// image width is too large; bound on width
	int newHeight = (int)(width * (double)imgHeight / imgWidth);
	heightOffset = (height - newHeight) / 2;
	height = newHeight;
    } else {
	// image height is too large; bound on height
	int newWidth = (int)(height * (double)imgWidth / imgHeight);
	widthOffset = (width - newWidth) / 2;
	width = newWidth;
    }
    // factor in desired starting position
    heightOffset += y;
    widthOffset += x;

    // prevent crashes
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    cv::Mat resized_image;
    // resize to fit location constraints
    resize(lilImage, resized_image, cv::Size(width, height), cv::INTER_LINEAR);

    // copy pixels into final img with position offsets
    for(int r = 0; r < height; r++) {
	for(int c = 0; c < width; c++) {
	    // extract bgra
	    cv::Vec4b color = resized_image.at<cv::Vec4b>(cv::Point(c,r));
	    if (color[3]) {
		// draw pixel, eliminate alpha component of blend
		cv::Vec3b color_noAlpha(color[0], color[1], color[2]);
		img.at<cv::Vec3b>(cv::Point(c+widthOffset,r+heightOffset)) = color_noAlpha;
	    }
	}
    }
}

void
IFPainter::drawImageFitted(int x, int y, int width, int height, std::string imgPath)
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

    if ((double)imgWidth / imgHeight > (double)width / height) {
	// image width is too large; bound on width
	int newHeight = (int)(width * (double)imgHeight / imgWidth);
	heightOffset = (height - newHeight) / 2;
	height = newHeight;
    } else {
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
    } catch (...) {
	bu_log("WARNING: Tyring to create roi out of image boundaries\n");
	return;
    }
    resized_image.copyTo(destRoi);
}

void
IFPainter::drawImageTransparentFitted(int x, int y, int width, int height, std::string imgPath)
{
    drawTransparentImage(x, y, width, height, imgPath, 240);
}

int
IFPainter::drawDiagramFitted(int x, int y, int width, int height, std::string imgPath, std::string text)
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

    if ((double)imgWidth / imgHeight > (double)width / height) {
	// image width is too large; bound on width
	int newHeight = (int)(width * (double)imgHeight / imgWidth);
	heightOffset = (height - newHeight) / 2;
	height = newHeight;
    } else {
	// image height is too large; bound on height
	int newWidth = (int)(height * (double)imgWidth / imgHeight);
	widthOffset = (width - newWidth) / 2;
	widthOffset /= 2.5;
	width = newWidth;
    }

    cv::Mat resized_image;
    resize(lilImage, resized_image, cv::Size(width, height), cv::INTER_LINEAR);

    //Trying to copyto the image on to the private image frame, img
    cv::Mat destRoi;
    try {
	destRoi = img(cv::Rect(x + widthOffset, y + heightOffset, resized_image.cols, resized_image.rows));
    } catch (...) {
	bu_log("WARNING: Trying to create roi out of image boundaries\n");
	return -1;
    }
    resized_image.copyTo(destRoi);

    // truncate title until it fits on line
    int countCharDisplayedText = text.length();
    getTextWidth(50, width, text, TO_BOLD); // needed for while loop to run correctly
    while (getTextWidth(50, width, text, TO_BOLD) > width) {
	if ((size_t)countCharDisplayedText == text.length()) {
	    text = text + " ...";
	}
	if (text.length() >= 5) {
	    text.erase(text.length() - 5, 1);
	    countCharDisplayedText--;
	}
	while (text.length() >= 5 && text[text.length() - 5] == ' ') {
	    // Remove the fifth-to-last character (if it is a space)
	    text.erase(text.length() - 5, 1);
	    countCharDisplayedText--;
	}
    }
    // now, draw text and line
    this->drawLine(x + widthOffset, y + heightOffset - 5, x + widthOffset + width, y + heightOffset - 5, 5, cv::Scalar(0, 0, 0));
    this->drawTextCentered(x + widthOffset + width / 2, y + heightOffset - 65, 50, width, text, TO_BOLD);

    return countCharDisplayedText;
}

//Helper funciton to support getting the width of a text
int
IFPainter::getTextWidth(int height, int width, std::string text, int flags)
{
    int fontWeight = (flags & TO_BOLD) ? boldTextWeight : standardTextWeight;
    int fontSize = getFontSizeFromHeightAndWidth(height, width, text);
    int textWidth = getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fontSize, fontWeight, 0).width;
    return textWidth;
}

int
IFPainter::getFontSizeFromHeightAndWidth(int height, int width, std::string text)
{
    if (heightToFontSizeMap.find(height) != heightToFontSizeMap.end()) {
	return heightToFontSizeMap[height];
    }

    int fontSize = 1;
    while (getTextSize("I", cv::FONT_HERSHEY_DUPLEX, fontSize, standardTextWeight, 0).height < height) {
	fontSize++;
    }
    fontSize--;

    heightToFontSizeMap[height] = fontSize;
    while (getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fontSize, standardTextWeight, 0).width > width) {
	fontSize--;
    }
    return fontSize;
}

void
IFPainter::drawText(int x, int y, int height, int width, std::string text, int flags)
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

void
IFPainter::drawTextCentered(int x, int y, int height, int width, std::string text, int flags)
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


void
IFPainter::drawTextRightAligned(int x, int y, int height, int width, std::string text, int flags)
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


/* This function will evenly space out all of the texts in the header
 * and footer. The algorithm is to get the total lenght of all of the
 * words combined and then the spacing is the total length divided by
 * how many words there are
 */
int
IFPainter::justify(int x, int y, int height, int width, std::vector<std::string> text, int flags)
{
    int totalTextWidth = 0;
    for (size_t i = 0; i < text.size(); i++) {
	totalTextWidth += getTextWidth(height, width, text[i], flags);
    }
    if (totalTextWidth >= width) {
	throw std::invalid_argument("Text is larger than the bounding box");
	return -1;
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
    return xPosition-spacing;	//end of text xPosition
}


/* This is justification with a word being centered right in the
 * middle. This is tricky now since the spacing is not going to be
 * super straightforward. Essentially I split the section into 2 and I
 * used the same justify algorithm for the left and right side. Which
 * ever side had the smallest spacing is the spacing I will choose for
 * between the words.
 */
void
IFPainter::justifyWithCenterWord(int UNUSED(x), int y, int height, int width, std::string centerWord, std::vector<std::string> leftText, std::vector<std::string> rightText, int flags)
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
    } else {
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
    for (size_t i = 0; i < rightText.size(); i++) {
	int fontSize = getFontSizeFromHeightAndWidth(height, width, rightText[i]);
	cv::putText(img, rightText[i], cv::Point(xPosition, y + height), cv::FONT_HERSHEY_DUPLEX, fontSize, color, fontWeight);
	xPosition = xPosition + spacing + getTextWidth(height, width, rightText[i], flags);
    }
}


// This is a text wrapping function. If a text goes beyond the set x
// or y, it will wrap around to the next line. It also has ellipsis
// fucntion.
void
IFPainter::textWrapping(int x1, int y1, int x2, int y2, int width, int height, std::string text, int ellipsis, int numOfCharactersBeforeEllipsis, int flags)
{
    if (ellipsis == TO_ELLIPSIS) {
	if ((size_t)numOfCharactersBeforeEllipsis < text.length()) {
	    text = text.substr(0, numOfCharactersBeforeEllipsis) + " ...";
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
    for (const auto& w : words) {
	wordWidth = getTextWidth(height, width, w, flags);
	wordWidthWithSpace = getTextWidth(height, width, w + " ", flags);
	if (x + wordWidth <= x2) {
	    drawText(x, y, height, width, w + " ", flags);
	    x += wordWidthWithSpace;
	} else {
	    if (y + height <= y2 - height) {
		x = x1;
		y += height;
		drawText(x, y, height, width, w + " ", flags);
		x += wordWidthWithSpace;
	    } else {
		throw std::invalid_argument("Text is larger than the bounding box");
		return;
	    }
	}
    }
}

void
IFPainter::drawLine(int x1, int y1, int x2, int y2, int width, cv::Scalar color)
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

void
IFPainter::drawRect(int x1, int y1, int x2, int y2, int width, cv::Scalar color)
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

void
IFPainter::drawCirc(int x, int y, int radius, int width, cv::Scalar color)
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

void
IFPainter::openInGUI()
{
    cv::String win_name("Report Preview");
    cv::namedWindow(win_name, cv::WINDOW_NORMAL);
    cv::moveWindow(win_name, 0, 0);
    cv::imshow(win_name, this->img);

    cv::waitKey(0); //wait infinite time for a keypress

    try {
	cv::destroyAllWindows();
    } catch (std::exception& e) {
    }
}

void
IFPainter::exportToFile(std::string filePath)
{
    cv::imwrite(filePath, this->img);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

