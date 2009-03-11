
/*
* NIST Data Probe Class Library
* clprobe-ui/seinstdisp.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: seinstdisp.cc,v 3.0.1.1 1997/11/05 23:01:10 sauderd DP3.1 $ */

#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/glue.h>
#include <IV-2_6/InterViews/sensor.h>
#include <variousmessage.h>	// ivfasd/variousmessage.h
#include <seinstdisp.h>  	// probe-ui/seinstdisp.h
#include <probe.h>	// "./probe.h"

// these have been moved to instcmdbufdisp.cc
//const int OrigColSize = 80; // this is the num of cols in the orig window
//const int OrigRowSize = 40; // this is the num of rows in the orig window

const char carriageReturn = '\r';

const int entityInstanceListRows = 40;  // any number of rows are allowed
const int entityInstanceListCols = 157; // this is the num of cols allowed
const boolean entityInstanceListUnique = true;
//const char entityInstanceListDone[] = "\r\022\023";
const char entityInstanceListDone[5] = {
	ILD_MODIFY_ACTION,
	carriageReturn,
	ILD_FORWARD_SEARCH, 
	ILD_BACKWARD_SEARCH, 
	0
     };

const int entityInstanceListHighlight = Reversed;

const char searchDone[4] = { ILD_FORWARD_SEARCH, ILD_BACKWARD_SEARCH, 
			     carriageReturn, 0 };

static const char RingBell = '\007';   // <CNTRL> G

extern Interactor* AddScroller (Interactor* grid);
extern Probe *dp;

///////////////////////////////////////////////////////////////////////////////

seInstListDisplay::seInstListDisplay()
{
//IVBUG    SetName("Data Probe Instances List");
//IVBUG    SetIconName("DP Instances List");

    input = new Sensor;
	// this object won't use key events but it will catch them only to be
	// stolen by other object that use them.  This e.g. enables the user to
	// type chars into the search box without the cursor being inside it.
    input->Catch(KeyEvent);
    entityInstanceListButSt = new ButtonState(0);
    entityInstanceListButSt->Attach(this);
    entityInstanceList = new InstCmdBufDisp(entityInstanceListButSt, 
			entityInstanceListRows, entityInstanceListCols, 
			entityInstanceListDone);
//    entityInstanceList = new MyStringBrowser(entityInstanceListButSt, 
//			entityInstanceListRows, entityInstanceListCols, 
//			entityInstanceListUnique, entityInstanceListHighlight,
//			entityInstanceListDone);

    CreateButtons();
    Scene::Insert(new MarginFrame(
	new VBox(
	    new BigBoldMessage("   Entity Instance List   "),
	    new Frame (AddScroller(entityInstanceList)),
	    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
	    new VBox(
		new HBox(
		    new VBox(
			new HBox(
				new HGlue(round(.01*inch)),
				new Message("Search Substring "),
				new HGlue(round(.01*inch)),
				caseSearchBut,
				new HGlue(round(.01*inch))
			),
			new VGlue(round(.01*inch)),
			new Frame(searchBuf)
		    ),
		    new HGlue(ILD_HSPACE),
		    new VBox(
			forwardSearchBut, 
			new VGlue(round(.01*inch), round(.01*inch), 0),
			backwardSearchBut
		    )
		),
		new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
	        new Message(
			"Each button executes its action immediately on the"),
	        new Message(
		     "selected instance.  The 'Execute' button executes all"),
	        new Message(
		     "of the marks next to the instances.  Use the key"),
	        new Message(
		     "bindings to mark the instances")
	    ),
	    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
	    new HBox(
		new VBox(
		    completeBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
		    incompleteBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
		    replicateBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, 0)
		),
		new HGlue(ILD_HSPACE, ILD_HSPACE, 0),
		new VBox(
		    executeBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
		    unmarkBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
		    deleteBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, vfil)
		),
		new HGlue(ILD_HSPACE, ILD_HSPACE, 0),
		new VBox(
		    modifyBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
		    viewBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, 0),
		    closeBut,
		    new VGlue(ILD_VSPACE, ILD_VSPACE, vfil)
		),
		new HGlue(ILD_HSPACE, ILD_HSPACE, hfil)
	    )
	), 5));
}

void seInstListDisplay::CreateButtons()
{

    searchBufButSt = new ButtonState(0);
    searchBufButSt->Attach(this);
    searchBuf = new StringEditor2(searchBufButSt, "", searchDone, 30);

    searchButtonsButSt = new ButtonState(ILD_FORWARD_SEARCH);
    forwardSearchBut = new MyRadioButton("Search Forward (^s)", 
				       searchButtonsButSt, ILD_FORWARD_SEARCH);
    backwardSearchBut = new MyRadioButton("Reverse Search (^r)", 
				      searchButtonsButSt, ILD_BACKWARD_SEARCH);

    caseButtonButSt = new ButtonState(ILD_IGNORE_CASE);
    caseSearchBut = new MyCheckBox("Consider Case", 
				   caseButtonButSt, 
				   ILD_CONSIDER_CASE, ILD_IGNORE_CASE);

    buttonsButSt = new ButtonState(0);
    buttonsButSt->Attach(this);

    completeBut = new MyPushButton("Save Complete (s)", buttonsButSt, 
					ILD_COMPLETE_ACTION);
    incompleteBut = new MyPushButton("Save Incomplete (i)", buttonsButSt, 
					ILD_INCOMPLETE_ACTION);
    replicateBut = new MyPushButton("Replicate (r)", buttonsButSt, 
					ILD_REPLICATE_ACTION);
//    replicateBut->Disable();
    executeBut = new MyPushButton("Execute (x)", buttonsButSt, 
					ILD_EXECUTE_ACTION);
    unmarkBut = new MyPushButton("Unmark (u)", buttonsButSt, 
					ILD_UNMARK_ACTION);
   deleteBut = new MyPushButton("Delete (d)", buttonsButSt, ILD_DELETE_ACTION);

    viewBut = new MyPushButton("View (v)", buttonsButSt, ILD_VIEW_ACTION);
   modifyBut = new MyPushButton("Modify (m)", buttonsButSt, ILD_MODIFY_ACTION);
    closeBut = new MyPushButton("Close (c)", buttonsButSt, ILD_CLOSE_ACTION);
}

void seInstListDisplay::Update()
{
    int value = 0;
    int ignoreCase = 0;
    int startIndex;
    int indexFound;
    
    entityInstanceListButSt->GetValue(value);
//    if(value == '\r') {
    if(value == ILD_MODIFY_ACTION) {
	dp->ErrorMsg("Modify Instance");
	entityInstanceListButSt->Detach(this);
	entityInstanceListButSt->SetValue(0);
	entityInstanceListButSt->Attach(this);
	dp->ModifyInstanceCmd();
    } 
    else
    {
//	if(value != ILD_BACKWARD_SEARCH && value != ILD_FORWARD_SEARCH)
	if(value != ILD_BACKWARD_SEARCH && value != ILD_FORWARD_SEARCH &&
	   value != ILD_MODIFY_ACTION && value != '\r')
	{
	    searchBufButSt->GetValue(value);
	    searchBufButSt->Detach(this);
	    searchBufButSt->SetValue(0);
	    searchBufButSt->Attach(this);
	    if(value == '\r')
		searchButtonsButSt->GetValue(value);
	}
	else
	{
	    if(value == '\r')
	    {
		searchButtonsButSt->GetValue(value);
	    }
	    entityInstanceListButSt->Detach(this);
	    entityInstanceListButSt->SetValue(0);
	    entityInstanceListButSt->Attach(this);
	}
	switch(value)
	{
	  case ILD_FORWARD_SEARCH:
		caseButtonButSt->GetValue(ignoreCase);

		startIndex = entityInstanceList->Selection() + 1;
		if(startIndex > entityInstanceList->Count() - 1)
		    startIndex = 0;
		if(ignoreCase == TLD_IGNORE_CASE)
		    indexFound = entityInstanceList->SearchForward(
					(char *)searchBuf->Text(), startIndex, 
					1);
		else
		    indexFound = entityInstanceList->SearchForward(
					(char *)searchBuf->Text(), startIndex, 
					0);

		if(indexFound >= 0)
		{
		    entityInstanceList->UnselectAll();
		    entityInstanceList->Select(indexFound);
		    entityInstanceList->ScrollTo(indexFound);
		}
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
		break;
	  case ILD_BACKWARD_SEARCH:
		startIndex = entityInstanceList->Selection() - 1;
		if(startIndex < 0)
		    startIndex = entityInstanceList->Count() - 1;
		indexFound = entityInstanceList->SearchBackward(
					(char *)searchBuf->Text(), startIndex);
		if(indexFound >= 0)
		{
		    entityInstanceList->UnselectAll();
		    entityInstanceList->Select(indexFound);
		    entityInstanceList->ScrollTo(indexFound);
		}
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
		break;
	  default:
		CheckButtons();
		break;
	}
    }
}


void seInstListDisplay::Insert(const char* s, int index)
{
    entityInstanceList->Insert(s, index);
}

int seInstListDisplay::Append(const char* s)
{
    entityInstanceList->Append(s);
    return entityInstanceList->MaxIndex();
}

void seInstListDisplay::Remove(int index)
{
    entityInstanceList->Remove(index);
}

void seInstListDisplay::ReplaceText(const char* s, int index)
{
    entityInstanceList->ReplaceText(s, index);
}

void seInstListDisplay::CheckButtons()
{
    int value;
    buttonsButSt->GetValue(value);
    buttonsButSt->Detach(this);
    buttonsButSt->SetValue(0);
    buttonsButSt->Attach(this);
    int index = 0;
    switch(value) {
      case ILD_COMPLETE_ACTION :
		dp->SaveComplInstanceCmd();
		break;
      case ILD_INCOMPLETE_ACTION :
		dp->SaveIncomplInstanceCmd();
		break;
      case ILD_REPLICATE_ACTION :
		dp->ReplicateInstanceCmd();
		break;
      case ILD_EXECUTE_ACTION :
		dp->ExecuteInstanceCmds();
		break;
      case ILD_UNMARK_ACTION :
		dp->UnmarkInstanceCmd();
		break;
      case ILD_DELETE_ACTION :
		dp->DeleteInstanceCmd();
		break;
      case ILD_VIEW_ACTION :
		dp->ViewInstanceCmd();
		break;
      case ILD_MODIFY_ACTION :
		dp->ModifyInstanceCmd();
		break;
      case ILD_CLOSE_ACTION :
		dp->CloseInstanceCmd();
		break;
    }
}
