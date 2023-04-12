#include "Options.h"

Options::Options()
{
	filepath = "";
	temppath = "../../../build/bin/";
	ppi = 300;
	width = 3508;
	length = 2480;
	isFolder = false;
	openGUI = false;
	exportToFile = false;
	fileName = "";
	name = "N/A";
	classification = "";
	rightLeft = "Right hand";
	zY = "+Z-up";
	notes = "N/A";
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

void Options::setFileName(std::string n) {
	fileName = n;
}

void Options::setName(std::string n) {
	name = n;
}

void Options::setClassification(std::string c) {
	classification = c;
}

void Options::setOrientationRightLeft(bool rL) {
	if (rL) {
		rightLeft = "Left hand";
	}
}

void Options::setOrientationZYUp(bool zy) {
	if (zy) {
		zY = "+Y-up";
	}
}

void Options::setNotes(std::string n) {
	notes = n;
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

std::string Options::getClassification() {
	return classification;
}

std::string Options::getOrientationRightLeft() {
	return rightLeft;
}

std::string Options::getOrientationZYUp() {
	return zY;
}

std::string Options::getNotes() {
	return notes;
}