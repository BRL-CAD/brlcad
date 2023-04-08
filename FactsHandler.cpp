#include "FactsHandler.h"
#include "RenderHandler.h"

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0,0,0));

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;
	std::vector<std::string> text;
	std::vector<std::string> text2;

	//Draw text on top
	/*std::string text;
	if (info.getInfo("classification") == "CONFIDENTIAL") {
		text = "Owner: " + info.getInfo("owner") + "     Version: " + info.getInfo("version") + "     " + info.getInfo("classification") + "     Last Updated: " + info.getInfo("lastUpdate") + "     Source File: " + info.getInfo("file");
	}
	else {
		text = "Owner: " + info.getInfo("owner") + "     Version: " + info.getInfo("version") + "     Last Updated: " + info.getInfo("lastUpdate") + "     Source File: " + info.getInfo("file");
	}
	img.drawTextCentered(offsetX + width / 2, offsetY + textYOffset, textHeight, width, text, TO_WHITE); */ 

	if (info.getInfo("classification") == "CONFIDENTIAL") {
		text.push_back("Owner: " + info.getInfo("owner"));
		text.push_back("Version: " + info.getInfo("version"));
		text2.push_back("Last Updated : " + info.getInfo("lastUpdate"));
		text2.push_back("Source File : " + info.getInfo("file"));
		img.justifyConfidential(offsetX, offsetY + textYOffset, textHeight, width, text, text2, TO_WHITE);
	}
	else {
		text.push_back("Owner: " + info.getInfo("owner"));
		text.push_back("Version: " + info.getInfo("version"));
		text.push_back("Last Updated : " + info.getInfo("lastUpdate"));
		text.push_back("Source File : " + info.getInfo("file"));
		img.justify(offsetX, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
	}
}

void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) { 
	//Draw black rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(0, 0, 0));

	int textHeight = 3 * height / 8;
	int textYOffset = (height - textHeight) / 2;
	std::vector<std::string> text;
	std::vector<std::string> text2;

	//Draw text on bottom
	/*std::string text;
	if (info.getInfo("classification") == "CONFIDENTIAL") {
		text = "Preparer: " + info.getInfo("preparer") + "     " + info.getInfo("classification") + "     Date Generated: " + info.getInfo("dateGenerated");
	}
	else {
		text = "Preparer: " + info.getInfo("preparer") + "     Date Generated: " + info.getInfo("dateGenerated");
	}
	img.drawTextCentered(offsetX + width / 2, offsetY + textYOffset, textHeight, width, text, TO_WHITE);*/
	if (info.getInfo("classification") == "CONFIDENTIAL") {
		text.push_back("Preparer: " + info.getInfo("preparer"));
		text2.push_back("Date Generated : " + info.getInfo("dateGenerated"));
		img.justifyConfidential(offsetX, offsetY + textYOffset, textHeight, width, text, text2, TO_WHITE);
	}
	else {
		text.push_back("Preparer: " + info.getInfo("preparer"));
		text.push_back("Date Generated : " + info.getInfo("dateGenerated"));
		img.justify(offsetX, offsetY + textYOffset, textHeight, width, text, TO_WHITE);
	}
}

void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	// draw bounding rectangle
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(220, 220, 220));

	int headerOffset = width / 20;
	int textOffset = width / 10;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "File Information", TO_BOLD);
	
	int curiX = 3;

	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Geometry Type", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) Implicit - CSG");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "File Extension", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("extension"));
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Orientation", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) Right-Hand, Z-Up");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Entity Summary", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("primitives") + " primitives, " + info.getInfo("regions") + " regions");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "? assemblies, " + info.getInfo("total") + " total");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Notes", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) My Notes");
	
}
void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(220, 220, 220));

	int headerOffset = width / 20;
	int textOffset = width / 10;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "Verification", TO_BOLD);

	int curiX = 3;


	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Unit", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("units"));
  
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Approximate Volume", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, info.getInfo("volume") + " " + info.getInfo("units") + "^3");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Surface Area", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) 100 m^2 (3D)");
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "(Sample) 128 m^2 (Projected)");
	curiX++;
	img.drawText(offsetX + headerOffset, offsetY + curiX++ * textYOffset, textHeight, width, "Mass", TO_BOLD);
	img.drawText(offsetX + textOffset, offsetY + curiX++ * textYOffset, textHeight, width, "2.5 Tonnes");
}

void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, 3, cv::Scalar(0, 0, 0));

    int textOffset = width / 10;
	int textHeight = height / 20;
    int textXOffset = textHeight * 53 / 5;
    int textYOffset = textHeight * 15 / 5;

	// img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeight, width, "Object Hierarchy", TO_BOLD);

	std::string render = renderPerspective(DETAILED, opt, info.largestComponents[0].name);

    int N = 4; // number of sub components you want
    int offY = height / 2 + offsetY;
    int offX = offsetX + 5;
    int imgH = height / 2;
    int imgW = (width - 5*fmin(N, info.largestComponents.size()-1)) / fmin(N, info.largestComponents.size()-1);

    // main component
    img.drawImageFitted(offsetX + 5, offsetY + textHeight, imgW, imgH, render);
    
    // sub components
    for (int i = 1; i < fmin(N, info.largestComponents.size()); i++) {
        render = renderPerspective(GHOST, opt, info.largestComponents[i].name, info.largestComponents[0].name);
        // std::cout << "INSIDE factshandler DBG: " << render << std::endl;
        img.drawImageFitted(offX + (i-1)*imgW, offY, imgW, imgH, render);
        img.drawText(offX + (i-1)*imgW, offY - width/30, textHeight, width, info.largestComponents[i].name, TO_BOLD);
    }

    if (info.largestComponents.size() > N) {
        // render the smaller sub components all in one
        std::string subcomponents = "";
        for (int i = N; i < info.largestComponents.size(); i++)
            subcomponents += info.largestComponents[i].name + " ";
        render = renderPerspective(GHOST, opt, subcomponents, info.largestComponents[0].name);
        img.drawImageFitted(offX + (N-1)*imgW, offY, imgW, imgH, render);
        img.drawText(offX + (N-1)*imgW, offY - width/30, textHeight, width, "...", TO_BOLD);
    }
}

void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height) {
	img.drawRect(offsetX, offsetY, offsetX + width, offsetY + height, -1, cv::Scalar(220, 220, 220));

	int boxOffset = width / 40;
	int textHeightTitle = height / 20;
	int textHeight = height / 30;
	int textYOffset = textHeight * 8 / 5;

	int textOffset = 2 * boxOffset + textHeight;

	img.drawTextCentered(offsetX + width / 2, offsetY + textHeight, textHeightTitle, width, "V&V Checks", TO_BOLD);

	// left side
	int curX = offsetX;
	int curY = offsetY + 2 * textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Material Properties");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Optical Properties");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Analytical Properties");
	curY += textOffset;

	// right side
	curX = offsetX + width*7/16;
	curY = offsetY + 2 * textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Watertight and Solid");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Overlaps/Interferences");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Has Domain-Specific Issues");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for Scientific Analysis");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for Gaming/Visualization");
	curY += textOffset;
	img.drawRect(curX + boxOffset, curY, curX + boxOffset + textHeight, curY + textHeight, 2, cv::Scalar(0, 0, 0));
	img.drawText(curX + textOffset, curY, textHeight, width, "Suitable for 3D Printing");
	curY += textOffset;
}
