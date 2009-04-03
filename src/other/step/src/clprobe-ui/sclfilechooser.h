#ifndef	SCLFILECHOOSER_H
#define	SCLFILECHOOSER_H

/*
* NIST Data Probe Class Library
* clprobe-ui/sclfilechooser.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sclfilechooser.h,v 3.0.1.1 1997/11/05 23:01:13 sauderd DP3.1 $ */ 

#define CHOOSER_EXCHANGE 1
#define CHOOSER_WORKING 2

#include <mybutton.h>
class MyFileChooser;
#include <myfilechooser.h>

#include <IV-2_6/_enter.h>

class sclFileChooser : public MyFileChooser
{
  public:
    sclFileChooser(
        const char* title_ = "Please select a file:",
        const char* subtitle_ = "",
        const char* dir = "~",
        int rows = 10, int cols = 24,
        const char* acceptLabel = " Open ",
        Alignment a = Center,
	int restrictBool = 0
    );
    sclFileChooser(
		  const char* name, const char* title_, const char* subtitle_,
		  const char* dir, int rows, int cols, const char* acceptLabel,
		  Alignment a, int restrictBool = 0
    );
    // the 2 functs below use the defines above: 
    // CHOOSER_EXCHANGE and CHOOSER_WORKING
    int FileType() 
		{ int val = 0; fileButtonsButSt->GetValue(val); return val; }
    void SetFileType(int fileType) 
		{ fileButtonsButSt->SetValue(fileType); }

  protected:
    virtual Interactor* Interior(const char* acptlbl);

    ButtonState *fileButtonsButSt;
    MyRadioButton *exchangeBut;
    MyRadioButton *workingBut;

  private:

};

#include <IV-2_6/_leave.h>

#endif
