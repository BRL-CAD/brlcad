#pragma once

#include "pch.h"

/**
 * The FactsHandler code autonomously modifies a section of the report page to include
 * multiple textual sections of information about the graphics file.
 *
 * The code will automatically layout the information in its designated section after
 * retreiving said information from the InformationGatherer helper file.
 */

class InformationGatherer;
class IFPainter;

void makeTopSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeBottomSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeFileInfoSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt);
void makeVerificationSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);
void makeHeirarchySection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height, Options& opt);
void makeVVSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height);