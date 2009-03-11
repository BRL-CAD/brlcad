
/*
* NIST Data Probe Class Library
* clprobe-ui/sclfilechooser.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sclfilechooser.cc,v 3.0.1.1 1997/11/05 23:01:13 sauderd DP3.1 $ */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <IV-2_6/InterViews/adjuster.h>
#include <IV-2_6/InterViews/border.h>
#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/button.h>
#include <InterViews/event.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/scroller.h>
#include <IV-2_6/InterViews/scene.h>
#include <IV-2_6/InterViews/sensor.h>
#include <IV-2_6/InterViews/streditor.h>

#include <myfilebrowser.h>
#include <myfilechooser.h>
#include <sclfilechooser.h>

#include <IV-2_6/_enter.h>

sclFileChooser::sclFileChooser(
        const char* title_, const char* subtitle_, const char* dir,
        int rows, int cols, const char* acceptLabel, Alignment a,
	int restrictBool) 
: MyFileChooser(title_, subtitle_, dir, rows, cols, acceptLabel, a, restrictBool)
{
    fileButtonsButSt = new ButtonState(CHOOSER_EXCHANGE);
    exchangeBut = new MyRadioButton(" STEP Exchange File ", 
				       fileButtonsButSt, CHOOSER_EXCHANGE);
    workingBut = new MyRadioButton(" Working File ", 
				fileButtonsButSt, CHOOSER_WORKING);
    Interactor **i;
    int index = 0;
    addButsArea->GetComponents(0, 0, i, index);
    addButsArea->Remove(i[0]);
//    delete i[0];
//    delete i;
    addButsArea->Insert(new VBox(
				 exchangeBut,
				 workingBut
			));
//    addButsArea->Propagate(true);
//    addButsArea->Change();
}

sclFileChooser::sclFileChooser(
		  const char* name, const char* title_, const char* subtitle_,
		  const char* dir, int rows, int cols, const char* acceptLabel,
		  Alignment a, int restrictBool
			       )
: MyFileChooser(name, title_, subtitle_, dir, rows, cols, acceptLabel, a, 
		restrictBool)
{
    fileButtonsButSt = new ButtonState(CHOOSER_EXCHANGE);
    exchangeBut = new MyRadioButton(" STEP Exchange File ", 
				       fileButtonsButSt, CHOOSER_EXCHANGE);
    workingBut = new MyRadioButton(" Working File ", 
				fileButtonsButSt, CHOOSER_WORKING);
    Interactor **i;
    int index = 0;
    addButsArea->GetComponents(0, 0, i, index);
    Remove(i[0]);
//    delete i[0];
//    delete i;
    addButsArea->Insert(new VBox(
				 exchangeBut,
				 workingBut
			));
    addButsArea->Propagate(true);
    addButsArea->Change();
}


Interactor* sclFileChooser::Interior (const char* acptLbl) {
    const int space = round(.5*cm);
    VBox* titleblock = new VBox(
        new HBox(title, new HGlue),
        new HBox(subtitle, new HGlue)
    );

    return new MarginFrame(
        new VBox(
            titleblock,
            new VGlue(space, 0),
            new Frame(new MarginFrame(_sedit, 2)),
            new VGlue(space, 0),
            new Frame(AddScroller(browser())),
            new VGlue(space, 0),
            new HBox(
                new VGlue(space, 0),
                new HGlue,
		new VBox(
			 new VGlue(round(.05*inch), 0),
			 exchangeBut,
			 new VGlue(round(.05*inch), 0),
			 workingBut,
			 new VGlue(round(.05*inch), 0)
			),
                new HGlue(space, 0),
                new PushButton("Cancel", state, '\007'),
                new HGlue(space, 0),
                new PushButton(acptLbl, state, '\r')
            )
        ), space, space/2, 0
    );
}

#include <IV-2_6/_leave.h>
