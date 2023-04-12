#pragma once

#include "pch.h"

/*
 * The Options class holds report-related parameters that can be specified by the user, such as
 * specifications on which renders should be present to the screen, report dimensions, section dimensions,
 * etc.
 */

class Options
{
public:
	Options();
	~Options();
	//Setter functions
	void setFilepath(std::string f);
	void setTemppath(std::string f);
	void setPPI(int p);
	void setIsFolder();
	void setOpenGUI();
	void setExportToFile();
	void setFileName(std::string n);
	void setName(std::string n);
	void setClassification(std::string c);
	void setOrientationRightLeft(bool rL);
	void setOrientationZYUp(bool zy);
	void setNotes(std::string n);

	//Getter functions
	std::string getFilepath();
	std::string getTemppath();
	int getWidth();
	int getLength();
	bool getIsFolder();
	bool getOpenGUI();
	bool getExportToFile();
	std::string getFileName();
	std::string getName();
	std::string getClassification();
	std::string getOrientationRightLeft();
	std::string getOrientationZYUp();
	std::string getNotes();
private:
	//Path to file that will be used to generate report
	std::string filepath;
	//Path to temporary directory to store the info
	std::string temppath;
	//Pixels per inch
	int ppi;
	//Dimensions of the output, in pixels
	int width;
	int length;
	//Whether path is a folder with multiple files or not
	bool isFolder;
	//Whether user specifices to open GUI as well
	bool openGUI;
	//Whether user decides to export to a png
	bool exportToFile;
	//Name of export file
	std::string fileName;
	//Name of preparer
	std::string name;
	//Classification word
	std::string classification;
	//Orientation
	std::string rightLeft;
	std::string zY;
	//Notes
	std::string notes;
};