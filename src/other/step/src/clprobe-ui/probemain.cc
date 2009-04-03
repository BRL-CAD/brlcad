
/*
* NIST Data Probe Class Library
* clprobe-ui/probemain.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: probemain.cc,v 3.0.1.1 1997/11/05 23:01:08 sauderd DP3.1 $ */

#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <IV-2_6/InterViews/adjuster.h>
#include <IV-2_6/InterViews/border.h>
#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/dialog.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>
#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/menu.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/scroller.h>
#include <IV-2_6/InterViews/shape.h>
//#include <IV-2_6/InterViews/scene.h>
#include <IV-2_6/InterViews/world.h>

//#include <InterViews/session.h> // included from world.h

#include <probe.h> 	// "./probe.h"

#include <IV-2_6/_enter.h>

#ifdef  SCL_LOGGING 
#include <fstream.h>
    ofstream *logStream;
#endif 

static PropertyData properties[] = {
    { "*font",			            "*fixed-medium-r-normal--13*"},
    { "myLabel*font",			    "*fixed-medium-r-normal--13*"},
    { "*SEEtitle*font",			    "*helvetica-bold-r-normal--12*"},
    { "*Message*font",			    "*helvetica-medium-r-normal--12*"},
    { "*Button*font",                       "*helvetica-medium-r-normal--12*"},
    { "*Banner*font",                   "*helvetica-bold-r-normal--24-*" },
    { "*Dialog*PushButton*font",             "*helvetica-bold-r-normal--12*" },
	//the following entries make the following classes do something.
    { "*GenTitleMessage*font",		"*times-bold-r-normal--24*" },
    { "*GenTitleItalMessage*font",	"*times-medium-i-normal--24*" },
    { "*GenTitleItalMenu*font",		"*times-medium-i-normal--14*" },
    { "*BoldMessage*font",		"*helvetica-bold-r-normal--12*" },
    { "*BoldMyMessage*font",		"*helvetica-bold-r-normal--12*" },
    { "*BigBoldMessage*font",		"*helvetica-bold-r-normal--18*" },
    { "*BigBoldMyMessage*font",		"*helvetica-bold-r-normal--18*" },
    { "Probe*AutoSaveFreq",		"30" },
    { "Probe*AutoSaveFileName",		"dpAutoSave.wf" },
    { "Probe*AutoSaveFileDir",		"~" },
    { "Probe*Restrict",			"0" },
    { nil }
};

static OptionDesc options[] = { 
    { "-saveFile", "Probe*AutoSaveFile", OptionValueNext },
    { "-s",	   "Probe*AutoSaveFile", OptionValueNext },
    { "-saveFreq", "Probe*AutoSaveFreq", OptionValueNext },
    { "-sf",	   "Probe*AutoSaveFreq", OptionValueNext },
    { "-restrict", "Probe*Restrict", OptionValueImplicit, "1" },
    { "-r",	   "Probe*Restrict", OptionValueImplicit, "1" },
    { "-workFile", "Probe*WorkFile", OptionValueNext, },
    { "-wf",	   "Probe*WorkFile", OptionValueNext, },
    { "-exchangeFile", "Probe*ExchangeFile", OptionValueNext, },
    { "-ef",	       "Probe*ExchangeFile", OptionValueNext, },
    { nil }
};

  Probe *dp;
  World * world;	// warning this variable is used in other files.

Interactor* AddScroller (Interactor* grid)
{
  return new HBox (
              new VBox (
                   new UpMover (grid, 1),
                   new HBorder,
                   new VScroller (grid),
                   new HBorder,
                   new DownMover (grid, 1)
              ),
              new VBorder,
              new MarginFrame (grid, 10)
         );
}
  

/***********************************/

int main (int argc, char *argv[])
{
#ifdef  SCL_LOGGING 
    logStream = new ofstream("scl.log",ios::noreplace);
    if(!*logStream)
    {
	cerr << "error opening scl.log with ios::noreplace option ; open with ios::ate" << endl ;
	(*logStream).open("scl.log", ios::ate);
    }
    if(!*logStream)
    {
	cerr << "error opening scl.log" << endl;
	logStream = 0;
    }
#endif 
    world = new World("Probe", argc, argv, options, properties);

    Session* session = world->session();

    dp = new Probe();

    Shape *s = dp->GetShape();
//    Coord l, b;
//IVBUG    world->Align(TopCenter, s->width, s->height, l, b);
      // insert main probe window at the TopRight of the root window 
//    dp->Align(Center, world->Width(), world->Height(), l, b);
//    world->InsertApplication(dp, l, b, TopCenter);
    world->InsertApplication(dp, (int)(world->Width()), (int)(world->Height()),
			     TopRight);

//    world->InsertApplication(dp);
    dp->ListEntityTypesCmd();

//    dp->Run();
    session->run();

}/* end of main */

#include <IV-2_6/_leave.h>
