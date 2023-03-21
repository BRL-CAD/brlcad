#include "Options.h"

Options::Options()
{
	filepath = "";
	imgDimensions.width = 2480;
	imgDimensions.length = 3508;
	isFolder = false;
	openGUI = false;
	exportToFile = false;
}

Options::~Options()
{

}

void Options::setFilepath(std::string f) {
	filepath = f;
}

void Options::setImgDimensions(int w, int l) {
	imgDimensions.width = w;
	imgDimensions.length = l;
}

void Options::setIsFolder() {
	isFolder = true;
}

void Options::setOpenGUI() {
	openGUI = true;
}

void Options::setExportToFile() {
	exportToFile = true;
}

std::string Options::getFilepath() {
	return filepath;
}

int Options::getWidth() {
	return imgDimensions.width;
}

int Options::getLength() {
	return imgDimensions.length;
}

bool Options::getIsFolder() {
	return isFolder;
}

bool Options::getOpenGUI() {
	return openGUI;
}

bool Options::getExportToFile() {
	return exportToFile;
}