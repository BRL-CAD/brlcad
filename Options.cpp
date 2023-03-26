#include "Options.h"

Options::Options()
{
	filepath = "";
	width = 3508;
	length = 2480;
	isFolder = false;
	openGUI = false;
	exportToFile = false;
	fileName = "";
	name = "N/A";
}

Options::~Options()
{

}

void Options::setFilepath(std::string f) {
	filepath = f;
}

void Options::setWidth(int w) {
	width = w;
}

void Options::setLength(int l) {
	length = l;
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

void Options::setFileName(std::string n) {
	fileName = n;
}

void Options::setName(std::string n) {
	name = n;
}

std::string Options::getFilepath() {
	return filepath;
}

int Options::getWidth() {
	return width;
}

int Options::getLength() {
	return length;
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

std::string Options::getFileName() {
	return fileName;
}

std::string Options::getName() {
	return name;
}