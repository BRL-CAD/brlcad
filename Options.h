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
	void setOverrideImages();
	void setExportFolder(std::string fldr);
	void setFileName(std::string n);
	void setFolder(std::string n);
	void setName(std::string n);
	void setClassification(std::string c);
	void setOrientationRightLeft(bool rL);
	void setOrientationZYUp(bool zy);
	void setNotes(std::string n);

	void setTopComp(std::string t);

	void setUnitLength(std::string l);
	void setUnitMass(std::string m);


	//Getter functions
	std::string getFilepath();
	std::string getTemppath();
	int getWidth();
	int getLength();
	bool getIsFolder();
	bool getOpenGUI();
	bool getExportToFile();
	bool getOverrideImages();
	std::string getFileName();
	std::string getFolder();
	std::string getExportFolder();
	std::string getName();
	std::string getClassification();
	std::string getOrientationRightLeft();
	std::string getOrientationZYUp();
	std::string getNotes();

	std::string getTopComp();

	std::string getUnitLength();
	std::string getUnitMass();
	bool isDefaultLength();
	bool isDefaultMass();

private:
	// Path to file that will be used to generate report
	std::string filepath;
	// Path to temporary directory to store the info
	std::string temppath;
	// Pixels per inch
	int ppi;
	// Dimensions of the output, in pixels
	int width;
	int length;
	// Whether path is a folder with multiple files or not
	bool isFolder;
	// Whether user specifices to open GUI as well
	bool openGUI;
	//Whether user decides to export to a png
	bool exportToFile;
	// Whether or not to override pre-existing rt/rtwizard output files
	bool overrideImages;
	//Name of export file
	std::string fileName;
    // Name of folder that contains input models
	std::string folderName;
    // Name of folder you want to create report.png in 
	std::string exportFolderName;
	// Name of preparer
	std::string name;
	// Classification word
	std::string classification;
	// Orientation
	std::string rightLeft;
	std::string zY;
	// Notes
	std::string notes;

    // top component
    std::string topComp;

	// Unit length
	std::string uLength;
	bool defaultLength;
	// Unit mass
	std::string uMass;
	bool defaultMass;
};