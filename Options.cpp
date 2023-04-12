#include "Options.h"

Options::Options()
{
	filepath = "";
	temppath = "../../../build/bin/";
	ppi = 300;
	width = 3508;
	length = 2480;
	isFolder = false;
    folderName = "";
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

void Options::setTemppath(std::string f) {
	temppath = f;
}

void Options::setPPI(int p) {
	ppi = p;
	width = int(ppi * 11.69);
	length = int(ppi * 8.27);
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

void Options::setFolder(std::string n) {
	folderName = n;
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

std::string Options::getTemppath() {
	return temppath;
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