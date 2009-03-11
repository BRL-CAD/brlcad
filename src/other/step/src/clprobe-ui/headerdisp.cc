
/*
* NIST Data Probe Class Library
* clprobe-ui/headerdisp.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: headerdisp.cc,v 3.0.1.1 1997/11/05 23:01:05 sauderd DP3.1 $ */

#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <stream.h>

/*
// in order to include xcanvas.h you will need to have a -I option 
// for /usr/local/include because <X11/Xlib.h> from /usr/local/include/
// gets included from InterViews/include/IV-X11/Xlib.h
#include <IV-X11/xcanvas.h>
#include <InterViews/window.h>
// xcanvas.h and window.h were included so I could write the 
// function IsMapped()
*/
#include <InterViews/canvas.h>
#include <InterViews/window.h>

#define iv2_6_compatible

#include <IV-2_6/InterViews/dialog.h>
#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/border.h>
#include <IV-2_6/InterViews/glue.h>

#include <headerdisp.h>  	// "~pdevel/src/probe-ui/headerdisp.h"
#include <probe.h>	// "./probe.h"
#include <stepenteditor.h>
#include <variousmessage.h>	// "~pdevel/src/clivfasd/variousmessage.h"
#include <s_HEADER_SCHEMA.h>

#include <IV-2_6/_enter.h>

const char carriageReturn = '\r';

static const char RingBell = '\007';   // <CNTRL> G

extern Probe *dp;

#define HD_VSPACE (round(.08*inch))
#define HD_HSPACE (round(.05*inch))

///////////////////////////////////////////////////////////////////////////////

HeaderDisplay::HeaderDisplay(InstMgr *him, Probe *probe, int pinBool)
{
// IVBUG //    SetName("Data Probe Header Editor");
// IVBUG //    SetIconName("DP Header Editor");

    headerIM = him;
    dp = probe;

    CreateButtons(pinBool);

//    static Frame *HeaderEditor = 0;
    MgrNode *headerNode1, *headerNode2, *headerNode3;
    STEPentity *headerEntity1, *headerEntity2, *headerEntity3;

// headerIM needs to be removed if a file is read since this won't point at 
// the right header entities (they are recreated).

    int instCount = headerIM->InstanceCount();
    if(instCount)
    {
	headerNode1 = headerIM -> GetMgrNode (0);
	headerNode2 = headerIM -> GetMgrNode (1);
	headerNode3 = headerIM -> GetMgrNode (2);
    }
    else
    {
	headerEntity1 = new p21DIS_File_description;
	headerEntity2 = new p21DIS_File_name;
	headerEntity3 = new p21DIS_File_schema;
	headerNode1 = headerIM->Append(headerEntity1, newSE);
	headerNode2 = headerIM->Append(headerEntity2, newSE);
	headerNode3 = headerIM->Append(headerEntity3, newSE);
	
    }
    hee1 = new HeaderEntityEditor(headerNode1, headerIM, 0);
    hee2 = new HeaderEntityEditor(headerNode2, headerIM, 0);
    hee3 = new HeaderEntityEditor(headerNode3, headerIM, 0);

    VBox *HeaderEditorBox = new VBox(
				     hee1,
				     new HBorder(2),
				     hee2,
				     new HBorder(2),
				     hee3
				);
    Scene::Insert(
	new VBox(
		 new VGlue(HD_VSPACE, HD_VSPACE, 0),
		 new HBox(
			  new HBox(
				   new HGlue(HD_HSPACE, HD_HSPACE, hfil),
				   pinTogBut,
				   new HGlue(HD_HSPACE, HD_HSPACE, hfil),
				   new BigBoldMessage(" Header Editor "),
				   new HGlue(HD_HSPACE, HD_HSPACE, hfil),
				   saveCompleteBut
				   ),
			  new HBox(
				   new HGlue(HD_HSPACE, HD_HSPACE, hfil),
				   saveIncompleteBut,
				   new HGlue(HD_HSPACE, HD_HSPACE, hfil),
				   cancelBut,
				   new HGlue(HD_HSPACE, HD_HSPACE, hfil)
				   )
			  ),
		 new VGlue(HD_VSPACE, HD_VSPACE, 0),
		 new HBorder(2),
		 HeaderEditorBox
		 )
    );
}

HeaderDisplay::~HeaderDisplay()
{
    if(IsMapped())
    {
	dp->RemoveHeaderEditor();
//	dp->Sync();
	dp->Flush();
    }
}
    
void HeaderDisplay::CreateButtons(int pinBool)
{
    buttonsButSt = new ButtonState(0);
    buttonsButSt->Attach(this);

    if(pinBool)
	pinTogButSt = new ButtonState(SEE_PIN);
    else
	pinTogButSt = new ButtonState(SEE_NOT_PIN);
    pinTogButSt->Attach(this);
    pinTogBut = new PinCheckBox("", pinTogButSt,
					SEE_PIN, SEE_NOT_PIN);

    saveCompleteBut = new MyPushButton("save all", buttonsButSt, 
					SEE_SAVE_COMPLETE);
    saveIncompleteBut = new MyPushButton("save all incomplete", buttonsButSt,
					SEE_SAVE_INCOMPLETE);
    cancelBut = new MyPushButton("cancel all", buttonsButSt,
					SEE_CANCEL);
}

int HeaderDisplay::ExecuteCommand(int v)
{
    hee1->PressButton(v);
    hee2->PressButton(v);
    hee3->PressButton(v);
    
    int val;
    pinTogButSt->GetValue(val);
    if(val == SEE_NOT_PIN)
    {
	dp->RemoveHeaderEditor();
    }
    return v;
}

void HeaderDisplay::Update()
{
    int v;
	// don't need a SubordinateInfo object because the buttons set their
	// ButtonState subject to a unique value that identifies one button.

    buttonsButSt->GetValue(v);
    if(v){
	ExecuteCommand(v);
    }
    buttonsButSt->SetValue(0);
}

// boolean i.e. true = 1 and false = 0
boolean HeaderDisplay::IsMapped()
{
    return (canvas != nil && canvas->status() == Canvas::mapped);
/*
    if(canvas)
    {
	// this doesn't work...  seems to always return false DAS 
	int bool = canvas->window()->is_mapped();
	return bool;
//	return canvas->rep()->window_->is_mapped();
    }
    else
	return false;
*/
}
/* DAVETMP
DAVETMP */

#include <IV-2_6/_leave.h>
