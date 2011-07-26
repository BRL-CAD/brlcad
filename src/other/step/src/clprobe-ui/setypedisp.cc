
/*
* NIST Data Probe Class Library
* clprobe-ui/setypedisp.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: setypedisp.cc,v 3.0.1.1 1997/11/05 23:01:11 sauderd DP3.1 $ */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/glue.h>
#include <variousmessage.h>	// ivfasd/variousmessage.h
#include <setypedisp.h>  	// ./setypedisp.h
#include <probe.h>	// "./probe.h"

#include <IV-2_6/_enter.h>

const char carriageReturn = '\r';
static const char searchDone[4] = { TLD_FORWARD_SEARCH, TLD_BACKWARD_SEARCH, 
			     carriageReturn, 0 };

const int entityTypeListRows = 36;
const int entityTypeListCols = 50;
const boolean entityTypeListUnique = true;
const char entityTypeListDone[6] = {
	'c',
	't',
	TLD_FORWARD_SEARCH, 
	TLD_BACKWARD_SEARCH, 
	carriageReturn, 
	0 };
//	TLD_CREATE_ACTION, 
//	TLD_INFO_ACTION, 
//const char entityTypeListDone[] = "\r\022\023";
const int entityTypeListHighlight = Reversed;

static const char RingBell = '\007';   // <CNTRL> G

extern Interactor* AddScroller (Interactor* grid);
extern Probe *dp;

///////////////////////////////////////////////////////////////////////////////

seTypeListDisplay::seTypeListDisplay()
{
//IVBUG    SetName("Data Probe Types List");
//IVBUG    SetIconName("DP Types List");

    entityTypeListButSt = new ButtonState(0);
    entityTypeListButSt->Attach(this);
    entityTypeList = new MyStringBrowser(entityTypeListButSt, 
			entityTypeListRows, entityTypeListCols, 
			entityTypeListUnique, entityTypeListHighlight,
			entityTypeListDone);

    CreateButtons();
    Scene::Insert(new MarginFrame(
	new VBox(
	    new BigBoldMessage("   Entity Type List   "),
	    new Frame (AddScroller(entityTypeList)),
	    new VGlue(TLD_VSPACE, TLD_VSPACE, 0),
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
		    new HGlue(TLD_HSPACE),
		    new VBox(
			forwardSearchBut, 
			new VGlue(round(.01*inch), round(.01*inch), 0),
			backwardSearchBut
		    )
	    ),
	    new VGlue(TLD_VSPACE, TLD_VSPACE, 0),
	    new HBox(
		new HGlue(TLD_HSPACE, TLD_HSPACE, hfil),
		createBut,
		new HGlue(TLD_HSPACE, TLD_HSPACE, hfil),
		infoBut,
		new HGlue(TLD_HSPACE, TLD_HSPACE, hfil)
	    )
	), 5));
}

void seTypeListDisplay::CreateButtons()
{
    searchBufButSt = new ButtonState(0);
    searchBufButSt->Attach(this);
    searchBuf = new StringEditor2(searchBufButSt, "", searchDone, 30);

    searchButtonsButSt = new ButtonState(TLD_FORWARD_SEARCH);
    forwardSearchBut = new MyRadioButton("Search Forward (^s)", 
				       searchButtonsButSt, TLD_FORWARD_SEARCH);
    backwardSearchBut = new MyRadioButton("Reverse Search (^r)", 
				searchButtonsButSt, TLD_BACKWARD_SEARCH);

    caseButtonButSt = new ButtonState(TLD_IGNORE_CASE);
    caseSearchBut = new MyCheckBox("Consider Case", 
				   caseButtonButSt, 
				   TLD_CONSIDER_CASE, TLD_IGNORE_CASE);

    buttonsButSt = new ButtonState(0);
    buttonsButSt->Attach(this);

    infoBut = new MyPushButton("Type Information (t)", buttonsButSt, 
			       TLD_INFO_ACTION);
    createBut = new MyPushButton("Create (c)", buttonsButSt, 
				 TLD_CREATE_ACTION);
}

void seTypeListDisplay::Update()
{
    int value = 0;
    int ignoreCase = 0;
    int startIndex;
    int indexFound;

    entityTypeListButSt->GetValue(value);
//    if(value == '\r') {
//    if(value == TLD_CREATE_ACTION) {
    if(value == 'c') {
	dp->ErrorMsg("Create Instance");
	entityTypeListButSt->Detach(this);
	entityTypeListButSt->SetValue(0);
	entityTypeListButSt->Attach(this);
	dp->CreateInstanceCmd();
    } 
    else
    {
	if(value != ILD_BACKWARD_SEARCH && value != ILD_FORWARD_SEARCH &&
	   value != 'c' && value != 't' && value != '\r')
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
	    entityTypeListButSt->Detach(this);
	    entityTypeListButSt->SetValue(0);
	    entityTypeListButSt->Attach(this);
	}
	switch(value)
	{
	  case TLD_FORWARD_SEARCH :
		caseButtonButSt->GetValue(ignoreCase);

		searchBufButSt->Detach(this);
		searchBufButSt->SetValue(0);
		searchBufButSt->Attach(this);
		startIndex = entityTypeList->Selection() + 1;
		if(startIndex > entityTypeList->Count() - 1)
		    startIndex = 0;
		if(ignoreCase == TLD_IGNORE_CASE)
		    indexFound = entityTypeList->SearchForward(
					(char *)searchBuf->Text(), startIndex, 
					1);
		else
		    indexFound = entityTypeList->SearchForward(
					(char *)searchBuf->Text(), startIndex, 
					0);
		if(indexFound >= 0)
		{
		    entityTypeList->UnselectAll();
		    entityTypeList->Select(indexFound);
		    entityTypeList->ScrollTo(indexFound);
		}
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
		break;
	  case TLD_BACKWARD_SEARCH :
		caseButtonButSt->GetValue(ignoreCase);

		searchBufButSt->Detach(this);
		searchBufButSt->SetValue(0);
		searchBufButSt->Attach(this);
		startIndex = entityTypeList->Selection() - 1;
		if(startIndex < 0)
		    startIndex = entityTypeList->Count() - 1;
		if(ignoreCase == TLD_IGNORE_CASE)
		    indexFound = entityTypeList->SearchBackward(
					(char *)searchBuf->Text(), startIndex, 
					1);
		else
		    indexFound = entityTypeList->SearchBackward(
					(char *)searchBuf->Text(), startIndex, 
					0);
		if(indexFound >= 0)
		{
		    entityTypeList->UnselectAll();
		    entityTypeList->Select(indexFound);
		    entityTypeList->ScrollTo(indexFound);
		}
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
		break;
	  case 'c':
		dp->ErrorMsg("Create Instance");
		dp->CreateInstanceCmd();
		break;
	  case 't':
		dp->ErrorMsg("Display STEP entity type info.");
		dp->DescribeEntityCmd();
		break;
	  default:
		buttonsButSt->GetValue(value);
		buttonsButSt->Detach(this);
		buttonsButSt->SetValue(0);
		buttonsButSt->Attach(this);
//		if(value == TLD_CREATE_ACTION || value == '\r') {
		if(value == TLD_CREATE_ACTION) {
		    dp->ErrorMsg("Create Instance");
		    dp->CreateInstanceCmd();
		}
		else if(value == TLD_INFO_ACTION) {
		    dp->ErrorMsg("Display STEP entity type info.");
		    dp->DescribeEntityCmd();
		}
		break;
	}
    }
/*****************************
    if(value == '\r') {
	dp->ErrorMsg("Create Instance");
	entityTypeListButSt->Detach(this);
	entityTypeListButSt->SetValue(0);
	entityTypeListButSt->Attach(this);
	dp->CreateInstanceCmd();
    } 
    else
    {
	if(value != ILD_BACKWARD_SEARCH && value != ILD_FORWARD_SEARCH)
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
	    entityTypeListButSt->Detach(this);
	    entityTypeListButSt->SetValue(0);
	    entityTypeListButSt->Attach(this);
	}
	switch(value)
	{
	  case TLD_FORWARD_SEARCH :
		searchBufButSt->Detach(this);
		searchBufButSt->SetValue(0);
		searchBufButSt->Attach(this);
		startIndex = entityTypeList->Selection() + 1;
		if(startIndex > entityTypeList->Count() - 1)
		    startIndex = 0;
		indexFound = entityTypeList->SearchForward(
					(char *)searchBuf->Text(), startIndex);
		if(indexFound >= 0)
		{
		    entityTypeList->UnselectAll();
		    entityTypeList->Select(indexFound);
		    entityTypeList->ScrollTo(indexFound);
		}
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
		break;
	  case TLD_BACKWARD_SEARCH :
		searchBufButSt->Detach(this);
		searchBufButSt->SetValue(0);
		searchBufButSt->Attach(this);
		startIndex = entityTypeList->Selection() - 1;
		if(startIndex < 0)
		    startIndex = entityTypeList->Count() - 1;
		indexFound = entityTypeList->SearchBackward(
					(char *)searchBuf->Text(), startIndex);
		if(indexFound >= 0)
		{
		    entityTypeList->UnselectAll();
		    entityTypeList->Select(indexFound);
		    entityTypeList->ScrollTo(indexFound);
		}
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
		break;
	  default:
		buttonsButSt->GetValue(value);
		buttonsButSt->Detach(this);
		buttonsButSt->SetValue(0);
		buttonsButSt->Attach(this);
		if(value == TLD_CREATE_ACTION || value == '\r') {
		    dp->ErrorMsg("Create Instance");
		    dp->CreateInstanceCmd();
		}
		else if(value == TLD_INFO_ACTION) {
		    dp->ErrorMsg(
			"Display STEP entity type info not implemented yet.");
		}
		break;
	}
    }
*********************/
}

void seTypeListDisplay::Insert(const char* s, int index)
{
    entityTypeList->Insert(s, index);
}

void seTypeListDisplay::Append(const char* s)
{
    entityTypeList->Append(s);
}

void seTypeListDisplay::Remove(int index)
{
    entityTypeList->Remove(index);
}

void seTypeListDisplay::ReplaceText(const char* s, int index)
{
    entityTypeList->ReplaceText(s, index);
}

#include <IV-2_6/_leave.h>
