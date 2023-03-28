#pragma once

#include "pch.h"

/**
 * The FactsHandler code autonomously modifies a section of the report page to include
 * multiple textual sections of information about the graphics file.
 *
 * The code will automatically layout the information in its designated section after
 * retreiving said information from the InformationGatherer helper file.
 */

// TODO: Organize this; I'm not sure how we want to present the information so don't want to template this out yet.
// TODO: should we change this into a general ReportGenerator class, which will have methods to build each part of the page?

class InformationGatherer;
class IFPainter;

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);