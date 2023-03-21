#include "RenderHandler.h"

void makeRenderSection(IFPainter& img, InformationGatherer& info, int offsetX, int offsetY, int width, int height)
{
    std::string ambOccImg = renderAmbientOcclusion();
    std::string frontImg = renderPerspective(FRONT);
    std::string rightImg = renderPerspective(RIGHT);
}

int selectLayout(int secWidth, int secHeight, int modelLength, int modelWidth, int modelHeight)
{
	return 0;
}