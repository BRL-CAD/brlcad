#pragma once

#include "pch.h"

/*
 * The Options class holds report-related parameters that can be specified by the user, such as
 * specifications on which renders should be present to the screen, report dimensions, section dimensions,
 * etc.
 */

// TODO: Task (Ally): Prepare this class.  There's a lot of freedom to how you can build this class.  You can
// mix in constant variables along with modifiable ones, and allow certain parameters to be set via the user.
// you should code some way to set and retrieve the values of certain parameters.  You can use a map and/or
// add a bunch of variables manually; it's up to you.

struct Dimensions {
	int width;
	int length;
};

class Options
{
public:
	Options();
	~Options();
	//Setter functions
	void setFilepath(std::string f);
	void setImgDimensions(int w, int l);
	void setIsFolder();
	void setOpenGUI();
	void setExportToFile();
	//Getter functions
	std::string getFilepath();
	int getWidth();
	int getLength();
	bool getIsFolder();
	bool getOpenGUI();
	bool getExportToFile();
private:
	std::string filepath;
	Dimensions imgDimensions;
	bool isFolder;
	bool openGUI;
	bool exportToFile;
};