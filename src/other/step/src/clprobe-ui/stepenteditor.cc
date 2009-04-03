
/*
* NIST Data Probe Class Library
* clprobe-ui/stepenteditor.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: stepenteditor.cc,v 3.0.1.1 1998/02/17 19:42:07 sauderd DP3.1 $ */

#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stream.h>
#include <string.h>

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

#include <stepenteditor.h>	// "./stepenteditor.h"
#include <probe.h>	// "./probe.h"

#include <InterViews/event.h>
#include <InterViews/font.h>
#include <InterViews/resource.h>

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>
#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/perspective.h>
#include <IV-2_6/InterViews/sensor.h>
#include <IV-2_6/InterViews/world.h>

#include <mybuttonstate.h> 	// "~sauderd/src/iv/hfiles/mybuttonstate.h"
#include <subinfo.h> 	// "~sauderd/src/iv/hfiles/subinfo.h"
#include <intstreditorsub.h>	// "~sauderd/src/iv/hfiles/instreditor.h"
#include <seestreditors.h>	// "./seestreditors.h"
//#include <streditor2sub.h>	// "~sauderd/src/iv/hfiles/streditor2sub.h"
#include <label.h>		// "~sauderd/src/iv/hfiles/label.h"
#include <variousmessage.h>	// "~sauderd/src/iv/hfiles/variousmessage.h"
//#include <entitystreditor.h>	// "./entitystreditor.h"

#include <read_func.h>
#include <STEPcomplex.h>

///////////////////////////////////////////////////////////////////////////////
//	debug_level >= 2 => tells when a command is chosen
//	debug_level >= 3 => prints values within functions:
///////////////////////////////////////////////////////////////////////////////
static int debug_level = 1;
	// if debug_level is greater than this number then
	// function names get printed out when executed
static int PrintFunctionTrace = 2;
	// if debug_level is greater than this number then
	// values within functions get printed out
static int PrintValues = 3;

#define CNTRL_Q '\021'

//#define LABEL_WIDTH 30
	// default width for the editable attribute StringEditor
#define EDIT_WIDTH 20
//#define TYPE_WIDTH 23

// this is the label that appears to the left of the editor to type in comments
// on a SEE
// Comments must be typed in as valid part 21 comments or they will not be 
// saved to the file.
#define COMMENT_LABEL "Comments:"

#define MAX_ATTR_NAME_SIZE 80

#define SEE_VSPACE (round(.07 * inch))
#define SEE_ATTR_VSPACE (round(.01 * inch))
#define SEE_HSPACE (round(.07 * inch))

const char RingBell = '\007';	// <CNTRL> G
extern Probe *dp;

////////////////////////////////////////////////////////////////////////////// 
// char *QuotedString(const char *str)
// This function returns 'str' in a string with single quotes added around
// it and all inside single quotes doubled.  If the returned str is to
// be saved it must be copied.
////////////////////////////////////////////////////////////////////////////// 

char *QuotedString(const char *str)
{
    static char *quotedStr = 0;
    static int len = 0;

    int quoteCount = 0;
    char *sPtr = (char *)str;
    while(sPtr)
    {
	sPtr = strchr(sPtr, '\'');	// returns 0 if single quote not found
	if(sPtr) { sPtr++; quoteCount++; }
    }

//    delete [len] quotedStr;
    delete [] quotedStr;
    len = strlen(str) + 3 + quoteCount;
    quotedStr = new char[len];
    quotedStr[0] = '\'';
    int i;
    for(i = 1, sPtr = (char *)str; *sPtr; sPtr++, i++)
    {
	if(*sPtr == '\'')
	{
	    quotedStr[i++] = '\'';
	}
	quotedStr[i] = *sPtr;
    }
    quotedStr[len - 2] = '\'';
    quotedStr[len - 1] = '\0';
    return quotedStr;
}

///////////////////////////////////////////////////////////////////////////////
//
//  seeAttrRow member functions
//
///////////////////////////////////////////////////////////////////////////////


// boolean i.e. true = 1 and false = 0
boolean seeAttrRow::IsMapped()
{
    return (canvas != nil && canvas->status() == Canvas::mapped);
/* // this doesn't work
    if(canvas)
    {
	int bool = canvas->window()->is_mapped();
	return bool;
    }
    else
	return false;
*/
}

char * seeAttrRow::IndicateOptionality(const char *aName)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::IndicateOptionality()\n";
    static char name[MAX_ATTR_NAME_SIZE + 3];

//    if(aName[0] != "\0")
//    {
	if(stepAttr->Nullable())
	{
	    name[0] = '[';
	    name[1] = '\0';
	    strncat(name, aName, MAX_ATTR_NAME_SIZE);
	    strcat(name, "]");
	    return name;
	}
//    }
    return (char *)aName;
}

const char * seeAttrRow::GetAttrName()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::GetAttrName()\n";
    if(stepAttr){
	const char *attrName = stepAttr->Name();
	if(attrName)
	    return attrName;
    }
    return "";
}

const char * seeAttrRow::GetAttrTypeName()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::GetAttrTypeName()\n";
    if(stepAttr)
    {
	const char *n = stepAttr->TypeName();
	if(n)
	    return n;
    }	
    return "";
}

BASE_TYPE seeAttrRow::GetType()
{
    return stepAttr->Type();
}

seeAttrRow::seeAttrRow(STEPattribute * sAttr, InstMgr *im, ButtonState *b, 
		       int nameWidth, int editWidth, int typeWidth)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::seeAttrRow()\n";
    stepAttr = sAttr;	// IMPORTANT - do this before calling GetAttrName()
			// or GetAttrTypeName()

    butSt = b;
    butSt->Reference();


	// create attribute name label
    nameField = new myLabel(IndicateOptionality(GetAttrName()), Right, 
			  nameWidth);

//    static ButtonState *bs = new ButtonState;
    stepAttr->ClearErrorMsg();
    SCLstring attrVal;
/*
    // make sure you check redef and derive first!!
    if(stepAttr->_redefAttr)
    {
	attrVal = "REDEFINED";
    }
    else if(stepAttr->IsDerived())
    {
	// these are always mapped to Part 21 as an asterisk DAS
	attrVal = "*";
    }
    else
    {
	stepAttr->asStr(attrVal);
    }
   // the above are taken care of below
*/
    stepAttr->asStr(attrVal);
    // need to do the above assignment to save return val from asStr

    Severity sev;
    // make sure you check redef and derive first!!
    if( stepAttr->_redefAttr)
    {
	sev = SEVERITY_NULL;
    }
    else if( stepAttr->IsDerived())
    {
	sev = SEVERITY_NULL;
    }
    else if( (stepAttr->NonRefType() == STRING_TYPE) && 
	 stepAttr->ptr.S->is_undefined() )
	sev = stepAttr->ValidLevel(0, &(stepAttr->Error()), im);
    else
	sev = stepAttr->ValidLevel(attrVal, &(stepAttr->Error()), im);

    if(sev > SEVERITY_INCOMPLETE)
	statusMarker = new MyMessage(" ");
//	statusMarker = new StringDisplay(bs, " ");
    else
	statusMarker = new MyMessage("*");
//	statusMarker = new StringDisplay(bs, "*");
	// create attribute type label
//    typeField = new myLabel(GetAttrTypeName(), Left, typeWidth);
    typeField = new myLabel((char *)GetAttrTypeName(), Left, typeWidth);

//    const char *attrVal = stepAttr->asStr();

    //printf("attrVal from asStr: '%s'\n", attrVal);

    // make sure you check redef and derive first!!
    if (stepAttr->IsDerived ())
    {
	attrVal = "*";
	editField = new seeStringEditor(butSt, attrVal, editWidth);
    }
    else if( stepAttr->_redefAttr)
    {
	attrVal = "REDEFINED";
	editField = new seeStringEditor(butSt, attrVal, editWidth);
    }
    else
    switch(stepAttr->NonRefType()){
	case INTEGER_TYPE:
	    editField = new seeIntEditor(butSt, attrVal, editWidth);
	    ((seeIntEditor*)editField)->AcceptSigns();
	    break;
	case STRING_TYPE:
	    {
	      SCLstring tmp(attrVal);
	      attrVal = StrEditorVal(tmp);
/*
	      if(stepAttr->ptr.S->is_undefined())
		attrVal = "$";
*/
	      editField = new seeStringEditor(butSt, attrVal, editWidth);
	      break;
	    }
	case REAL_TYPE:
	case NUMBER_TYPE:
	    editField = new seeRealEditor(butSt, attrVal, editWidth);
	    break;
	case ENTITY_TYPE:
	    editField = new seeEntityEditor(butSt, attrVal, editWidth);
	    break;
	case AGGREGATE_TYPE:
	{
	    PrimitiveType aggrElemType = 
/*		stepAttr->AttrDescriptor->AggrElemType();*/
		stepAttr->aDesc->AggrElemType();
	    switch(aggrElemType)
//	    switch(stepAttr->BaseType())
	    {
	      case INTEGER_TYPE:
		    editField = 
			new seeIntAggrEditor(butSt, attrVal, editWidth);
		    break;
	      case REAL_TYPE:
	      case NUMBER_TYPE:
		    editField = 
			new seeRealAggrEditor(butSt, attrVal, editWidth);
		    break;
	      case ENTITY_TYPE:
		// save this for when we handle aggregate better
		    editField = 
			new seeEntAggrEditor(butSt, attrVal, editWidth);
		    break;
	      default:
		    editField = 
			new seeStringEditor(butSt, attrVal, editWidth);
		    break;
	    }
	    break;
	}
	case ENUM_TYPE:
	case LOGICAL_TYPE:
	case BOOLEAN_TYPE:
	    editField = new seeStringEditor(butSt, attrVal, editWidth);
	    break;
	case REFERENCE_TYPE:
	    cout << "BUG should not have type REFERENCE_TYPE in " <<
		"seeAttrRow::seeAttrRow() \n";
	    editField = new seeStringEditor(butSt, attrVal, editWidth);
	    break;
	    
	case SELECT_TYPE:
	case BINARY_TYPE:
	case GENERIC_TYPE:
	case UNKNOWN_TYPE:
	default:
	    editField = new seeStringEditor(butSt, attrVal, editWidth);
	    break;
    }
    Insert(nameField);
    Insert(new HGlue(SEE_HSPACE, 0, 0));
    Insert(statusMarker);
    Insert(new Frame(editField));
    Insert(new HGlue(SEE_HSPACE, 0, 0));
    Insert(typeField);
//    Insert(new HGlue(SEE_HSPACE));
}
/*
	    TypeDescriptor *aggrTD = 
		stepAttr->AttrDescriptor->NonRefTypeDescriptor();
	    TypeDescriptor *aggrElemTD = aggrTD->ReferentType();
	    enum BASE_TYPE aggrElemType;
	    if(aggrElemTD)
	    {
		aggrElemType = aggrElemTD->NonRefType();
	    }
	    else
	    {
		aggrElemType = ENTITY_TYPE;
	    }
*/

seeAttrRow::~seeAttrRow()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::~seeAttrRow()\n";
    Unref(butSt);
    if(previous != nil && previous != this){
	previous->next = next;
    }
    if(next != nil && next != this){
	next->previous = previous;
    }
}

void seeAttrRow::UndoChanges()
{
//    char * attrVal = stepAttr->asStr();
    SCLstring attrVal;
    stepAttr->asStr(attrVal);
    if(stepAttr->NonRefType() == STRING_TYPE)
    {
	SCLstring tmp(attrVal);
	attrVal = StrEditorVal(tmp);
/*
	if(stepAttr->ptr.S->is_undefined())
	    attrVal = "$";
	else
	{
	    char *str = attrVal.chars();
	    while(isspace(*str)) str++;
	    if( (str[0] == '\\') && (str[1] == '$') )
	    {
		str = str + 2;
		while( isspace(*str) ) str++;
		if(*str == '\0')
		    attrVal = "\\$"; // call value \$ a string with a single $
	    }
	}
*/
    }
  // need to do the above assignment to save return val from asStr
  editField->Message(attrVal);
}

Severity seeAttrRow::Validate(InstMgr *instMgr, int setMark, 
			      Severity setMarkAtSev)
{
    Severity s;
    stepAttr->ClearErrorMsg();

    // make sure you check redef and derive first!!
    if(stepAttr->_redefAttr)
    {
	editField->Message("REDEFINED");
	s = SEVERITY_NULL;
    }
    else if(stepAttr->IsDerived())
    {
	// these are always mapped to Part 21 as an asterisk DAS
	editField->Message("*");
	s = SEVERITY_NULL;
    }
    else if(stepAttr->NonRefType() == STRING_TYPE)
    {
	s = stepAttr->ValidLevel(StrAttrAssignVal((char *)GetEditFieldText()),
				 &(stepAttr->Error()), instMgr);
    }
    else
	s = stepAttr->ValidLevel((char *)(GetEditFieldText()),
					  &(stepAttr->Error()), instMgr);
    if(setMark && IsMapped())
    {
	if(s <= setMarkAtSev)
	{
	    statusMarker->NewMessage("*");
	}
	else
	{
	    statusMarker->NewMessage(" ");
	}
    }
    return s;
}

//used to convert data probe values to string to assign to attr
char *seeAttrRow::StrEditorVal(SCLstring &s)
{
    if(stepAttr->ptr.S->is_undefined())
	s = "$";
    else
    {
	char *str = (char *)s.chars();
	while(isspace(*str)) str++;
	if(str[0] == '$')
	{
	    str++;
	    while( isspace(*str) ) str++;
	    if(*str == '\0')
		s = "\\$"; // call value \$ a string with a single $
	}
    }
    return (char *)s.chars();
}

// it was decided that attributes in the Data Probe of type string with value
// $ => string does not exist
// no value => they mean the string to exist but contain no chars (except of 
//		coursethe terminating null character which is not written into 
//		the part 21 file).
// a value => does not contain the surrounding delimiter single quotes, any  
//		internal single quotes will be interpreted as internal quotes
//		and hence the resulting single quotes in this string value 
//		will end up quoted in the part 21 file.
// If you call this function for an attribute which is not of type string it
// returns -1 otherwise it returns the string value to be assigned which
// is 0 if the string is meant to not exist, "" if it is meant to exist but
// contain no chars, or the char * otherwise.

char *seeAttrRow::StrAttrAssignVal(const char *s)
{
    if(stepAttr->NonRefType() == sdaiSTRING)
    {
	char *str = (char *)s;
	while(isspace(*str)) str++;
	if(*str == '$')
	{
	    str++;
	    while(isspace(*str)) str++;
	    if(*str == '\0')// we will call value $ a string with nothing in it
	    {
		return 0;
	    }
	    else // a string value starting with a $ with following chars.
//		stepAttr->StrToVal(QuotedString(s), dp->GetInstMgr(), 0);
		return (char *)s;
//		return QuotedString(s);
	}
	else if(*str == '\0')
	{ // no value will be absence of a string
//	    stepAttr->ptr.S->set_undefined();
	    return "";
	}
	else
	{
	    if( (str[0] == '\\') && (str[1] == '$') )
	    {
		str = str + 2;
		while( isspace(*str) ) str++;
		if(*str == '\0')
		    return "$"; // call value \$ a string with a single $
	    }
		// a string value - nothing out of the ordinary
	    return (char *)s;
	}
    }
    else
    {
	cerr << "Programmer error:  " << __FILE__ <<  __LINE__
	       << "\n" << _POC_ "\n";
	return (char *)-1; // this is an error... funct should not have been 
			   // called. read documentation a top of function
    }
}

#if 0
// it was decided that attributes in the Data Probe of type string with value
// no value => string does not exist
// '' => they mean the string to exist but contain no chars (except of course
//		the terminating null character which is not written into the 
//		part 21 file).
// a value => does not contain the surrounding delimiter single quotes, any  
//		internal single quotes will be interpreted as internal quotes
//		and hence the resulting single quotes in this string value 
//		will end up quoted in the part 21 file.
// If you call this function for an attribute which is not of type string it
// returns -1 otherwise it returns the string value to be assigned which
// is 0 if the string is meant to not exist, "" if it is meant to exist but
// contain no chars, or the char * otherwise.
other possible way of doing the function
char *seeAttrRow::StrAttrAssignVal(const char *s)
{
    if(stepAttr->NonRefType() == sdaiSTRING)
    {
	char *str = (char *)s;
	while(isspace(*str)) str++;
	if(*str == '\'')
	{
	    str++;
	    if(*str == '\'')
	    {
		str++;
		while(isspace(*str)) str++;
		// we will call value '' a string with nothing in it
		if(*str == '\0')
		{
//		    stepAttr->ptr.S->set_null(); // this effect - not exactly
		    return "";
		}
		else // a string value starting with two single quotes with
		     // following chars.
//		    stepAttr->StrToVal(QuotedString(s), dp->GetInstMgr(), 0);
		    return QuotedString(s);
	    }
	    else // a string value starting with one single quote (not a 
	         // delimiter) with following chars.
//		    stepAttr->StrToVal(QuotedString(s), dp->GetInstMgr(), 0);
		    return QuotedString(s);
	}
	else if(*str == '\0')
	{ // no value will be absence of a string
//	    stepAttr->ptr.S->set_undefined();
	    return 0;
	}
	else
	{ // a string value - nothing out of the ordinary
	    return QuotedString(s);
	}
    }
    else
	return (char *)-1; // this is an error... funct should not have been called
}
#endif

Severity seeAttrRow::SaveAttr(InstMgr *instMgr, Severity setMarkAtSev)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::SaveAttr()\n";
    
    Severity s = SEVERITY_NULL;
    const char *str = editField->Text();
    if(str > (const char *)0){
	s = Validate(instMgr, 1, setMarkAtSev);
	if(s >= SEVERITY_INCOMPLETE)
	{
	    // make sure you check redef and derive first!!
	    if(stepAttr->_redefAttr)
	    {
		editField->Message("REDEFINED");
	    }
	    else if(stepAttr->IsDerived())
	    {
		// these are always mapped to Part 21 as an asterisk DAS
		editField->Message("*");
	    }
	    else if(stepAttr->NonRefType() == sdaiSTRING)
	    {
		stepAttr->StrToVal(StrAttrAssignVal(str), dp->GetInstMgr(), 0);
	    }
	    else
	    {
		stepAttr->StrToVal(str, dp->GetInstMgr(), 0);
	    }
/*
	    char *strptr = (char *)str;
	    while(isspace(*strptr)) strptr++;
	    if(*strptr == '\'')
	    {
		strptr++;
		if(*strptr == '\'')
		{
		    strptr++;
		    while(isspace(*strptr)) strptr++;
			 // we will call value '' a string with nothing in it
		    if(*strptr == '\0')
		        stepAttr->ptr.S->set_null();
		    else
			stepAttr->StrToVal(QuotedString(str), dp->GetInstMgr(), 0);
		}
		else // a string value starting with a quote (not delimiter)
		    stepAttr->StrToVal(QuotedString(str), dp->GetInstMgr(), 0);
	    }
	    else if(*strptr == '\0')
	    { // no value will be absence of a string
		stepAttr->ptr.S->set_undefined();
	    }
	    else
		stepAttr->StrToVal(QuotedString(str), dp->GetInstMgr(), 0);
*/
	}
    }
    else if(!stepAttr->Nullable()) 
	return SEVERITY_INCOMPLETE;
    return s;
}

/*
// Severity seeAttrRow::SaveAttr(InstMgr *instMgr, Severity setMarkAtSev)
///  has replaced the following two functions
Severity seeAttrRow::SaveComplete(InstMgr *instMgr)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::SaveComplete()\n";
    
    const char *str = editField->Text();
    if(str){
	s = ValidateAttr(this, 1);
	if(stepAttr->Type() == TYPE_STRING)
	{
	    char *strptr = str;
	    while(isspace(*strptr)) strptr++;
	    if(*strptr == '\'')
	    {
		strptr++;
		if(*strptr == '\'')
		{
		    while(isspace(*strptr)) strptr++;
			 // we will call value '' a string with nothing in it
		    if(*strptr == '\0')
		        stepAttr->ptr.S->set_null();
		    else
			stepAttr->StrToVal(QuotedString(str), dp->GetInstMgr(), 0);
		}
		else // a string value starting with a quote (not delimiter)
		    stepAttr->StrToVal(QuotedString(str), dp->GetInstMgr(), 0);
	    }
	    else if(*strptr == '\0')
	    { // no value will be absence of a string
		stepAttr->ptr.S->set_undefined();
	    }
	    else
		stepAttr->StrToVal(QuotedString(str), dp->GetInstMgr(), 0);
	}
	else
	{
	    stepAttr->StrToVal(str, dp->GetInstMgr(), 0);
	}
    }
	if(s == SEVERITY_NULL)
	    rowHead->SaveComplete();
	else if(s <= SEVERITY_NULL)
	    ok = 0;
}

boolean seeAttrRow::SaveIncomplete()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRow::SaveIncomplete()\n";
    const char *str = editField->Text();
    if(str){
	if(stepAttr->Type() == TYPE_STRING)
	{
	    stepAttr->StrToVal(QuotedString(str), dp->GetInstMgr(), 0);
	}
	else
	{
	    stepAttr->StrToVal(str, dp->GetInstMgr(), 0);
	}
    }
}
*/
///////////////////////////////////////////////////////////////////////////////
//
//  seeAttrRowList member functions
//
///////////////////////////////////////////////////////////////////////////////

seeAttrRowList::seeAttrRowList()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::seeAttrRowList()\n";
    currentRow = head = nil;
    rowCount = 0;
}

seeAttrRowList::~seeAttrRowList()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::~seeAttrRowList()\n";
    currentRow = head->Next();
    seeAttrRow *ptr = currentRow;

    if(currentRow == head){	// one element list
	 delete head;
    }
    else {
	do{
	    currentRow = currentRow->Next();
	    delete ptr;
	    ptr = currentRow;
	} while (currentRow != head);
	delete currentRow;
    }
}

int seeAttrRowList::SetCurRow(int index)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::SetCurRow()\n";
    int i;

    if(currentRow && index > 0 && index <= rowCount)
    {
	currentRow->NameField()->Highlight(false);
	currentRow->TypeField()->Highlight(false);
	for(i = 1, currentRow = head; i < index; 
	    i++, currentRow = currentRow->next)
	    ; // happily loop
	currentRow->NameField()->Highlight(true);
	currentRow->TypeField()->Highlight(true);
	return (i);
    }
    return (0);
}

seeAttrRow *seeAttrRowList::FindEditField(Interactor *targetStrEd)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::FindEditField()\n";
  if(currentRow){
    if(targetStrEd == currentRow->EditField())
    {
	return(currentRow);
    }
    else 
    {
	seeAttrRow *ptr;
	for(ptr = currentRow->Next();
	    targetStrEd != ptr->EditField() && ptr != currentRow;
	    ptr = ptr->Next())
	    ; // continue search
	if(targetStrEd == ptr->EditField())
	    return(ptr);
    }
  }
    return (0);
}

int seeAttrRowList::Find(seeAttrRow *r)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::Find()\n";
    seeAttrRow *ptr;
    int i;

    if(r == nil || head == nil)
	 return (0);
    else if(r == head)
	return (1);
    else {
	for( ptr = head->next, i = 2;
	     ptr != r && ptr != head;
	     ptr = ptr->next, i++
	    )
	    ; // loop away
	if(ptr == head)
	    return (0);
	else
	    return (i);
    }
}

int seeAttrRowList::InsertAfter(seeAttrRow *newRow, seeAttrRow *existingRow)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::InsertAfter()\n";
  if(newRow)
  {
    int index;

	// check to see if existingRow exists
    if(existingRow == LastRow()) // check this to speed up Append()
	index = rowCount;
    else
	index = Find(existingRow);
    if(index)  // existingRow exists
    {
	newRow->next = existingRow->next;
	newRow->next->previous = newRow;
	newRow->previous = existingRow;
	existingRow->next = newRow;
	rowCount++;
	return(index + 1);
    }
    else if(head == nil)
    {
	head = newRow;
	head->next = head->previous = head;
	currentRow = head;
//	currentRow->NameField()->Highlight(true);
//	currentRow->TypeField()->Highlight(true);
	rowCount++;
	return(1);
    }
  }
  return(0);  // error 
}

int seeAttrRowList::InsertBefore(seeAttrRow *newRow, seeAttrRow *existingRow)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::InsertBefore()\n";
  if(newRow)
  {
    int index;

	// check to see if existingRow exists
    if(existingRow == Head()) // may want to insert to front of the list often?
	index = 1;
    else
	index = Find(existingRow);
    if(index)  // existingRow exists
    {
	newRow->previous = existingRow->previous;
	newRow->previous->next = newRow;
	newRow->next = existingRow;
	existingRow->previous = newRow;
	rowCount++;
	return(index);
    }
    else if(head == nil)
    {
	head = newRow;
	head->next = head->previous = head;
	currentRow = head;
//	currentRow->NameField()->Highlight(true);
//	currentRow->TypeField()->Highlight(true);
	rowCount++;
	return(1);
    }
  }
  return(0);  // error 
}

void seeAttrRowList::ComponentsIgnore()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::ComponentsIgnore()\n";
    seeAttrRow *ptr = head;
    extern Sensor *noEvents;

  if (ptr){
    ptr->EditField()->Listen(noEvents);
    for(ptr = ptr->Next(); ptr != head; ptr = ptr->Next())
    {
	ptr->EditField()->Listen(noEvents);
    }
  }
}

void seeAttrRowList::ComponentsListen()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "seeAttrRowList::ComponentsListen()\n";
    seeAttrRow *ptr = head;
    static Sensor *s = 0;

  if(ptr){
    if(!s){
	s = new Sensor();
	s->Catch(KeyEvent);
	s->Catch(DownEvent);
    }
    ptr->EditField()->Listen(s);
    for(ptr = ptr->Next(); ptr != head; ptr = ptr->Next())
    {
	ptr->EditField()->Listen(s);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//  StepEntityEditor member functions
//
///////////////////////////////////////////////////////////////////////////////

StepEntityEditor::StepEntityEditor(MgrNode *mn, InstMgr *im, int pinBool, 
				   int modifyBool)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::StepEntityEditor()\n";
    mgrNode = mn;
    stepEnt = mgrNode->GetSTEPentity();
    instMgr = im;
	// this ButtonState is needed for below but is not of any real use.
    dispButSt = new ButtonState();
    messageDisp = new StringDisplay(dispButSt, 
"                                                             ");
//mdisp
    CreateButtonsAndStates(modifyBool, pinBool);
    input = new Sensor(); 
    input->Catch(DownEvent);
    input->Catch(KeyEvent);
    attrRowList = new seeAttrRowList();

    CreateHeading();
    CreateInsertAttributeEditors();
    InsertButtons();
    if(!modifyBool)
	ComponentsIgnore();
    editorsButSt->Reference();
    buttonsButSt->Reference();
    modifyTogButSt->Reference();
    pinTogButSt->Reference();
}

StepEntityEditor::StepEntityEditor(STEPentity *se, int pinBool, int modifyBool)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::StepEntityEditor()\n";
    mgrNode = 0;
    stepEnt = se;
	// this ButtonState is needed for below but is not of any real use.
    dispButSt = new ButtonState();
    messageDisp = new StringDisplay(dispButSt, 
"                                                             ");

    CreateButtonsAndStates(modifyBool, pinBool);
    input = new Sensor(); 
    input->Catch(DownEvent);
    input->Catch(KeyEvent);
    attrRowList = new seeAttrRowList();

    CreateHeading();
    CreateInsertAttributeEditors();
    InsertButtons();
    if(!modifyBool)
	ComponentsIgnore();
    editorsButSt->Reference();
    buttonsButSt->Reference();
    modifyTogButSt->Reference();
    pinTogButSt->Reference();
}

void DeleteSEE(StepEntityEditor *se)
{
    delete se;
}

StepEntityEditor::~StepEntityEditor()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::~StepEntityEditor()\n";

//    extern World *world;
//    if(IsMapped())
    if( (GetMgrNode()->DisplayState() == mappedWrite) || 
	(GetMgrNode()->DisplayState() == mappedView) )
	dp->RemoveSEE(this);
    Unref(editorsButSt);
    Unref(buttonsButSt);
    Unref(modifyTogButSt);
    Unref(pinTogButSt);
//    Unref(dispButSt);

/*	don't need these since these are inserted into
StepEntityEditor which is a VBox.  VBox deletes all of its components.
    delete attrRowList;
    delete label;
    delete saveCompleteBut;
    delete saveIncompleteBut;
    delete cancelBut;
    delete deleteBut;
    delete replicateBut;
    delete editBut;
    delete markBut;
    delete listBut;
    delete modifyTogBut;
    delete pinTogBut;
*/
}

void StepEntityEditor::CreateHeading()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::CreateHeading()\n";
    VBox::Insert(new VGlue(SEE_VSPACE, 0));

    char * str = new char[BUFSIZ];
    sprintf(str, "#%d", stepEnt->STEPfile_id);

    Message *titleMsg = new Message(stepEnt->EntityName());
//BUG needed to set the fontin the see window.
//    titleMsg->SetClassName("SEEtitle");
    VBox::Insert(new HBox(
			new HGlue(round(.08*inch), 0, 0), 
			pinTogBut,
//			modifyTogBut,
			new Message(str),
			new HGlue(round(.08*inch), 0, 0), 
			titleMsg,
			new HBox(
				 new HGlue,
				 sedBut,
				 new HGlue(round(.2*inch), round(.2*inch), 0)
			)
		     )
		);
    VBox::Insert(new VGlue(SEE_VSPACE, 0));
}

void StepEntityEditor::CreateButtonsAndStates(int modifyBool, int pinBool)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::CreateButtonsAndStates()\n";
    editorsButSt = new MyButtonState(0);
    editorsButSt->Attach(this);

    buttonsButSt = new ButtonState(0);
    buttonsButSt->Attach(this);

    if(modifyBool)
    {
	modifyTogVal = SEE_MODIFY;  // set modifyTogButSt to same value as this
	modifyTogButSt = new ButtonState(SEE_MODIFY);
    }
    else
    {
	modifyTogVal = SEE_VIEW;  // set modifyTogButSt to same value as this
	modifyTogButSt = new ButtonState(SEE_VIEW);
    }
    modifyTogButSt->Attach(this);

    if(pinBool)
	pinTogButSt = new ButtonState(SEE_PIN);
    else
	pinTogButSt = new ButtonState(SEE_NOT_PIN);
    pinTogButSt->Attach(this);

    saveCompleteBut = new MyPushButton("save", buttonsButSt, 
					SEE_SAVE_COMPLETE);
    saveIncompleteBut = new MyPushButton("i", buttonsButSt,
					SEE_SAVE_INCOMPLETE);
    cancelBut = new MyPushButton("c", buttonsButSt,
					SEE_CANCEL);
    deleteBut = new MyPushButton("d", buttonsButSt,
					SEE_DELETE);
    replicateBut = new MyPushButton("r", buttonsButSt,
					SEE_REPLICATE);
//    replicateBut->Disable();	// BUG since we haven't implemented this yet
    editBut = new MyPushButton("e", buttonsButSt,
					SEE_EDIT);
    markBut = new MyPushButton("m", buttonsButSt,
					SEE_MARK);
//    listBut = new MyPushButton("l", buttonsButSt,
//					SEE_LIST);
    sedBut = new MyPushButton("type information", buttonsButSt,
					SEE_OPEN_SED);
    attrInfoBut = new MyPushButton("a", buttonsButSt,
					SEE_ATTR_INFO);
    modifyTogBut = new ModifyViewCheckBox(" ", modifyTogButSt,
					SEE_MODIFY, SEE_VIEW);
    pinTogBut = new PinCheckBox("    ", pinTogButSt,
					SEE_PIN, SEE_NOT_PIN);
}

void StepEntityEditor::InsertButtons()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::InsertButtons()\n";
    VBox::Insert(new VGlue(SEE_VSPACE, 0));
    VBox::Insert(new HBox(
			  new HGlue(round(.05*inch), 0, 0), 
			  new Frame(messageDisp), 
			  new HGlue(round(.05*inch), 0, 0)
			  ));
    VBox::Insert(new VGlue(SEE_VSPACE, 0));

    VBox::Insert(
	new HBox(
	    new HBox(
		new HGlue(round(.1*inch)),
//		pinTogBut,
		modifyTogBut,
		new HGlue(round(.1*inch)),
		saveCompleteBut,
		new HGlue(round(.1*inch)),
		saveIncompleteBut,
		new HGlue(round(.1*inch))
	    ),
	    new HBox(
		cancelBut,
		new HGlue(round(.1*inch)),
		deleteBut,
		new HGlue(round(.1*inch)),
		replicateBut,
		new HGlue(round(.1*inch))
	    ),
	    new HBox(
		editBut,
		new HGlue(round(.1*inch)),
		markBut,
		new HGlue(round(.1*inch))
	    ),
	    new HBox(
//		listBut,
//		new HGlue(round(.1*inch)),
		attrInfoBut,
		new HGlue(round(.1*inch))
	    )
	)
    );
/*
	new HBox(
	    new HGlue(round(.2*inch)),
	    new VBox(
		new HBox(
			new HGlue(round(.1*inch)), 
			saveCompleteBut,
			new HGlue
		),
		new VGlue(SEE_VSPACE, 0),
		new HBox(
			new HGlue(round(.1*inch)), 
			saveIncompleteBut,
			new HGlue
		),
		new VGlue(SEE_VSPACE, 0),
		new HBox(
			new HGlue(round(.1*inch)), 
			cancelBut,
			new HGlue
		),
		new VGlue(SEE_VSPACE, 0),
		new HBox(
			new HGlue(round(.1*inch)),
			deleteBut,
			new HGlue
		)
	    ), // end VBox
	    new HGlue(round(.2*inch)),
	    new VBox(
		new HBox(
			new HGlue(round(.1*inch)), 
			selectBut,
			new HGlue
		),
		new VGlue(SEE_VSPACE, 0),
		new HBox(
			new HGlue(round(.1*inch)), 
			createBut,
			new HGlue
		),
		new VGlue(SEE_VSPACE, 0),
		new HBox(
			new HGlue(round(.1*inch)),
			modifyTogBut,
			new HGlue
		)
	    ), // end VBox
	    new HGlue
	)
*/
    VBox::Insert(new VGlue(SEE_VSPACE, 0));
}

void StepEntityEditor::CreateInsertAttributeEditors()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::CreateInsertAttributeEditors()\n";
    STEPattribute * attr;
    STEPcomplex *sc = 0;

    int maxAttrNameWidth = strlen(COMMENT_LABEL);
    int maxAttrTypeNameWidth = 1;
//    label = new StringEditor2Sub(editorsButSt, " ", "\r\016\020\021", 
//				EDIT_WIDTH);

    if(stepEnt->IsComplex())
    {
	sc = ((STEPcomplex *)stepEnt)->head;
	while(sc)
	{
	    sc->ResetAttributes();
	    while (attr = sc->NextAttribute ()) {
		maxAttrNameWidth = max(maxAttrNameWidth, 
				       (int)strlen(attr->Name())+2);
		maxAttrTypeNameWidth = max( maxAttrTypeNameWidth, 
					   (int)strlen(attr->TypeName()) );
	    }
	    sc = sc->sc;
	}
    }
    else
    {
	stepEnt->ResetAttributes();
	while (attr = stepEnt->NextAttribute ()) {
	    maxAttrNameWidth = max(maxAttrNameWidth, 
				   (int)strlen(attr->Name())+2);
	    maxAttrTypeNameWidth = max( maxAttrTypeNameWidth, 
				       (int)strlen(attr->TypeName()) );
	}
    }

    if(stepEnt->P21CommentRep())
    {
	SCLstring ss;
	const char *s = stepEnt->P21Comment();
	while(*s) // get rid of newlines since editor can't handle them
	{
	    if(*s != '\n') 
		ss.Append(*s);
	    s++;
	}
	label = new seeStringEditor(editorsButSt, ss.chars(), EDIT_WIDTH);
    }
    else
	label = new seeStringEditor(editorsButSt, " ", EDIT_WIDTH);
    VBox::Insert(
		 new HBox(
			// edit maxAttrNameWidth + 1 extra for marker field.
		    new myLabel(COMMENT_LABEL, Right, maxAttrNameWidth + 1 + 1),
		    new HGlue(SEE_HSPACE, 0, 0),
		    new Frame(label)
		 )
		);
    VBox::Insert(new VGlue(SEE_VSPACE, 0));

    if(stepEnt->IsComplex())
    {
	sc = ((STEPcomplex *)stepEnt)->head;
	while(sc)
	{
	    sc->ResetAttributes();
	    while (attr = sc->NextAttribute ()) {
		Insert(attr, maxAttrNameWidth + 1, EDIT_WIDTH, 
		       maxAttrTypeNameWidth + 3);
	    }
	    sc = sc->sc;
	}
    }
    else
    {
	stepEnt->ResetAttributes();
	while (attr = stepEnt->NextAttribute ()) {
	    Insert(attr, maxAttrNameWidth + 1, EDIT_WIDTH, 
		   maxAttrTypeNameWidth + 3);
	}
    }
}

// boolean i.e. true = 1 and false = 0
boolean StepEntityEditor::IsMapped()
{
    return (canvas != nil && canvas->status() == Canvas::mapped);
/* // this doesn't work
    if(canvas)
    {
	int bool = canvas->window()->is_mapped();
	return bool;
    }
    else
	return false;
*/
}

int StepEntityEditor::ExecuteTermChar() 
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::ExecuteTermChar()\n";
	// need a SubordinateInfo object because the editors set their
	// ButtonState subject to the same set of terminating values
	// and it's necessary to know which editor set it to know which
	// editor will next be edited.
    int termCode = 0;
    SubordinateInfo *si = 0;

    if(debug_level >= PrintValues && attrRowList->CurRow()){
	cout << " curr attr value: '" << 
	attrRowList->CurRow()->GetEditFieldText() << "'\n";
    }

//    editorsButSt->GetValue(si); // this no longer works with Sun C++ => error
				  // Must assign as below  DAS
    void * tmpVoidPtr = 0;
    editorsButSt->GetValue(tmpVoidPtr);
    si = (SubordinateInfo *) tmpVoidPtr;

    if(si){
//	editorsButSt->SetValue(0);
	editorsButSt->SetNull();
	si->GetValue(termCode);
	ExecuteCommand(termCode);
    }
	// this would highlight the editable field's text which would be
	// deleted if a char was typed before a valid cntrl char
//   attrRowList->CurRow()->EditField()->Select(0, 
//			strlen(attrRowList->CurRow()->GetEditFieldText()));
    return (termCode);
};

///////////////////////////////////////////////////////////////////////////////
//
// I made this function because most of these actions which could only occur
// when a button was chosen, can now occur when a keystroke is typed inside
// one of the editors in the SEE.  So I pulled out this common code.
//
///////////////////////////////////////////////////////////////////////////////

int StepEntityEditor::ExecuteCommand(int v)
{
    int rc = 0;
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::ExecuteCommand()\n";
    switch(v)
    {
	    case SEE_PIN : 
		int prevVal;
		pinTogButSt->GetValue(prevVal);
		if(prevVal == SEE_PIN)
		    pinTogButSt->SetValue(SEE_NOT_PIN);
		else
		    pinTogButSt->SetValue(SEE_PIN);
		
		return(v);

	    case SEE_SAVE_COMPLETE : 
//		if(attrRowList->CurRow()){
//		} else dp->ErrorMsg("Entity has no attributes");
		
		if(SaveComplete())
		    dp->seeSaveComplete(this);
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		    dp->seeSaveIncomplete(this);
		}
		buttonsButSt->SetValue(0);
	// don't do this since buttons need to be notified to redraw themselves
//		buttonsButSt->SetNull();
		return(v);
	    case SEE_SAVE_INCOMPLETE : 
//		if(attrRowList->CurRow()){
//		} else dp->ErrorMsg("Entity has no attributes");

		if(!SaveIncomplete())
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
		dp->seeSaveIncomplete(this);
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_CANCEL : 
		UndoChanges();
		messageDisp->Message(
			   "Entity values reverted to last saved state.");
		dp->seeCancel(this);
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_DELETE : 
		messageDisp->Message("Entity changed to a DELETE state.");
		dp->seeDelete(this);
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_REPLICATE : 
		dp->seeReplicate(this);
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_EDIT : 
		if(attrRowList->CurRow()){
		    if(dp->seeEdit(this))
		    {
			Severity s = ValidateAttr(attrRowList->CurRow(), 
						  SEVERITY_INCOMPLETE, 1);
			if(s >= SEVERITY_INCOMPLETE)
			    attrRowList->SetCurRow(attrRowList->NextRow());
		    }
		} else
		    messageDisp->Message(
				     "Select an attribute to edit first");
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_MARK : 
		if(attrRowList->CurRow()){
		    if(dp->seeSelectMark(this))
		    {
			Severity s = ValidateAttr(attrRowList->CurRow(), 
						  SEVERITY_INCOMPLETE, 1);
			if(s >= SEVERITY_INCOMPLETE)
			    attrRowList->SetCurRow(attrRowList->NextRow());
		    }
		} else
		  messageDisp->Message(
			  "Select an attribute to assign value to first");
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_ATTR_UNDO : 
	    {
		seeAttrRow *row = attrRowList->CurRow();
		if(row) row->UndoChanges();
		buttonsButSt->SetValue(0);
		return(v);
	    }
	    case SEE_MODIFY : 
		dp->seeToggle(this, SEE_MODIFY);
		ComponentsListen();
		return(v);
	    case SEE_VIEW : 
		if(attrRowList->CurRow()){
		    attrRowList->CurRow()->NameField()->Highlight(false);
		    attrRowList->CurRow()->TypeField()->Highlight(false);
		}
		dp->seeToggle(this, SEE_VIEW);
//		SetPinned(0);
		ComponentsIgnore();
		return(v);
	    case SEE_OPEN_SED:
		{
		dp->seeOpenSED(this);
		buttonsButSt->SetValue(0);
		return(v);
		}
	    case SEE_ATTR_INFO:
		{
		SetAttrTypeMessage(attrRowList->CurRow());
//		dp->seeOpenSED(this);
		buttonsButSt->SetValue(0);
		return(v);
		}
	    case SEE_NEXT:
		{
		    Severity s = ValidateAttr(attrRowList->CurRow(), 
					      SEVERITY_INCOMPLETE, 1);
		    if(s >= SEVERITY_INCOMPLETE)
		    {
			attrRowList->SetCurRow(attrRowList->NextRow());
			SetAttrTypeMessage(attrRowList->CurRow());
		    }
		    else
		    {
			fprintf(stderr, "%c" , RingBell);
			fflush(stderr);
		    }
		    return(v);
		}
	    case SEE_PREV:
		{
		    Severity s = ValidateAttr(attrRowList->CurRow(), 
					      SEVERITY_INCOMPLETE, 1);
		    if(s >= SEVERITY_INCOMPLETE)
		    {
			attrRowList->SetCurRow(attrRowList->PreviousRow());
			SetAttrTypeMessage(attrRowList->CurRow());
		    }
		    else
		    {
			fprintf(stderr, "%c" , RingBell);
			fflush(stderr);
		    }
		    return(v);
		}
	    case SEE_LIST : 
/*
		if(attrRowList->CurRow()){
		    if(dp->seeList(this))
		    {
			Severity s = ValidateAttr(attrRowList->CurRow(), 
						  SEVERITY_INCOMPLETE, 1);
			if(s >= SEVERITY_INCOMPLETE)
			    attrRowList->SetCurRow(attrRowList->NextRow());
		    }
		} else
		    messageDisp->Message(
				 "Select an attribute to list for first");
		buttonsButSt->SetValue(0);
		return(v);
*/
		return(0);
	    default:
		buttonsButSt->SetValue(0);
		return(0);
    }
}

void StepEntityEditor::NotifyUser(const char *msg, int numBellRings)
{
//    dp->ErrorMsg(msg);
    messageDisp->Message(msg);
    int i;
    for(i = 0; i < numBellRings; i++)
    {
	fprintf(stderr, "%c" , RingBell);
    }
    fflush(stderr);
}

void StepEntityEditor::SetAttrTypeMessage(seeAttrRow *row)
{
//    const AttrDescriptor *ad = row->StepAttr()->AttrDescriptor;
//    NotifyUser((char *)TypeString(ad), 0);
    SCLstring tmp2;
    SCLstring tmp(
	     row->StepAttr()->aDesc->DomainType()->TypeString(tmp2) );
    NotifyUser(tmp, 0);
}

Severity StepEntityEditor::ValidateAttr(seeAttrRow *row, Severity setMarkAtSev,
					int notifyUser)
{
    Severity sev = SEVERITY_NULL;
    STEPattribute *sa = row->StepAttr();
    sa->ClearErrorMsg();

    // make sure you check redef and derive first!!
    if( sa->_redefAttr)
    {
	sev = SEVERITY_NULL;
    }
    else if(sa->IsDerived())
    {
	// these are always mapped to Part 21 as an asterisk DAS
	sev = SEVERITY_NULL;
    }
    else if(sa->NonRefType() == STRING_TYPE)
	sev = sa->ValidLevel(
		    row->StrAttrAssignVal( (char *)(row->GetEditFieldText()) ),
			&(sa->Error()), instMgr);
    else
        sev = sa->ValidLevel((char *)(row->GetEditFieldText()),
			&(sa->Error()), instMgr);

    if(sev <= setMarkAtSev)
    {
	if(notifyUser)
	{
//	    NotifyUser((char *)sa->Error().UserMsg(), 0);
	    NotifyUser((char *)sa->Error().DetailMsg(), 0);
	}
	row->StatusMarker()->NewMessage("*");
    }
    else
    {
	NotifyUser(" ", 0);
	row->StatusMarker()->NewMessage(" ");
    }
    return sev;
}

int StepEntityEditor::CheckButtons()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::CheckButtons()\n";
    int v;
    int v2;
	// don't need a SubordinateInfo object because the buttons set their
	// ButtonState subject to a unique value that identifies one button.

    buttonsButSt->GetValue(v);
    modifyTogButSt->GetValue(v2);
	// The modify/view checkbox has to have its own ButtonState.
	// The reason: It sets its value based on the previous value.
	// If it didn't have its own ButtonState then the previous
	// value would be lost when the other buttons set the ButtonState.
	// So what we have to do is see which ButtonState was set.
	// That's the reason why below we have to see if the
	// modifyTogButState set its value.
    if(modifyTogVal != v2){

	v = modifyTogVal = v2;
    }
    if(v){
	return ExecuteCommand(v);
    }
    buttonsButSt->SetValue(0);
    return(0);
}

void StepEntityEditor::UndoChanges()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::UndoChanges()\n";
    seeAttrRow *rowHead = attrRowList->Head();
    if(rowHead){
	rowHead->UndoChanges();
	seeAttrRow *row = rowHead->Next();
	while(row != rowHead){
	    row->UndoChanges();
	    row = row->Next();
	}
    }
}

Severity StepEntityEditor::Validate()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Validate()\n";

    Severity s;
    Severity level = SEVERITY_NULL;
    seeAttrRow *rowHead = attrRowList->Head();

    if(rowHead){
	s = rowHead->Validate(instMgr, 1, SEVERITY_INCOMPLETE);
	if(s < level)
	    level = s;
	seeAttrRow *row = rowHead->Next();
	while(row != rowHead){
	    s = row->Validate(instMgr, 1, SEVERITY_INCOMPLETE);
	    if(s < level)
		level = s;
	    row = row->Next();
	}
    }
    return level;
}

// returns whether all values were successfully saved complete
boolean StepEntityEditor::SaveComplete()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::SaveComplete()\n";

    Severity s;
    Severity worst = SEVERITY_NULL;
    seeAttrRow *rowHead = attrRowList->Head();

    const char *text = label->Text();
    if(stepEnt && text)
    {
	SCLstring ss;
	const char *sTmp = ReadComment(ss, text);
	while(*sTmp) 
	{
	    sTmp = ReadComment(ss, sTmp);
	}
	if(ss.rep())
	    stepEnt->AddP21Comment(ss.chars());
    }

    if(rowHead){
	s = rowHead->SaveAttr(instMgr, SEVERITY_INCOMPLETE);
	if(s < worst)
	    worst = s;
	seeAttrRow *row = rowHead->Next();
	while(row != rowHead){
	    if(debug_level >= PrintValues)
		cout << "check to see if lastChar: '" 
		<< (int)(((StringEditor2 *)row->EditField())->LastChar())
		<< "' has a value. \n"
		<< "If it does then it doesn't need to be type checked.\n";

	    s = row->SaveAttr(instMgr, SEVERITY_INCOMPLETE);
	    if(s < worst)
		worst = s;
	    row = row->Next();
	}
    }

    if(worst < SEVERITY_USERMSG)
    {
	messageDisp->Message("Entity saved to an INCOMPLETE state.");
	return 0;
    }
    messageDisp->Message("Entity saved to a COMPLETE state.");
    return 1;

///////////////////////////////////////////////////////////////////////////////
/*
    seeAttrRow *rowHead = attrRowList->Head();
    if(rowHead){
	rowHead->SaveComplete();
	seeAttrRow *row = rowHead->Next();
	while(row != rowHead){
	    if(debug_level >= PrintValues)
		cout << "check to see if lastChar: '" 
		<< (int)(((StringEditor2 *)row->EditField())->LastChar())
		<< "' has a value. \n"
		<< "If it does then it doesn't need to be type checked.\n";
	    row->SaveComplete();
	    row = row->Next();
	}
    }
*/
}

// returns whether all values were successfully saved incomplete
boolean StepEntityEditor::SaveIncomplete()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::SaveIncomplete()\n";

    Severity s;
    Severity worst = SEVERITY_NULL;
    seeAttrRow *rowHead = attrRowList->Head();

    const char *text = label->Text();
    if(stepEnt && text)
    {
	SCLstring ss;
	const char *sTmp = ReadComment(ss, text);
	while(*sTmp) 
	{
	    sTmp = ReadComment(ss, sTmp);
	}
	if(ss.rep())
	    stepEnt->AddP21Comment(ss.chars());
    }

    if(rowHead){
	s = rowHead->SaveAttr(instMgr, SEVERITY_INCOMPLETE);
	if(s < worst)
	    worst = s;
	seeAttrRow *row = rowHead->Next();
	while(row != rowHead){
	    if(debug_level >= PrintValues)
		cout << "check to see if lastChar: '" 
		<< (int)(((StringEditor2 *)row->EditField())->LastChar())
		<< "' has a value. \n"
		<< "If it does then it doesn't need to be type checked.\n";

	    s = row->SaveAttr(instMgr, SEVERITY_INCOMPLETE);
	    if(s < worst)
		worst = s;
	    row = row->Next();
	}
    }
    messageDisp->Message("Entity saved to an INCOMPLETE state.");
    if(worst < SEVERITY_INCOMPLETE)
	return 0;
    return 1;
///////////////////////////////////////////////////////////////////////////////
/*
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::SaveIncomplete()\n";
    seeAttrRow *rowHead = attrRowList->Head();
    if(rowHead){
	rowHead->SaveIncomplete();
	seeAttrRow *row = rowHead->Next();
	while(row != rowHead){
	    row->SaveIncomplete();
	    row = row->Next();
	}
    }
*/
}

	// edit the rows until an event other than a key event is received
void StepEntityEditor::EditRows() 
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::EditRows()\n";
    attrRowList->CurRow()->Edit();
    while(ExecuteTermChar())
    {
	attrRowList->CurRow()->Edit();
    }
}

void PrintEvent(Event *e)
{
    Interactor *target = e->target;
    EventType et = e->eventType;
    char * ks = e->keystring;
}

/*
int StepEntityEditor::Edit(Event &e) 
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Edit()\n";
    if(attrRowList->RowCount() > 0)
    {
	static Event e2;
	if(e.eventType == KeyEvent)
	    e2 = e;
	else
	{
	    e2.poll();
	    e2.eventType = FocusInEvent;
//	    e2.eventType = 5; // i.e. enum value other_event in class Event 
	}
	e2.target = attrRowList->CurRow()->EditField();
	PrintEvent(&e2);
	attrRowList->CurRow()->NameField()->Highlight(true);
	attrRowList->CurRow()->TypeField()->Highlight(true);
	UnRead(e2);
	return 1;
    }
    return 0;
}
*/

int StepEntityEditor::Edit() 
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Edit()\n";
    if(attrRowList->RowCount() > 0)
    {
	static Event e2;
	e2.poll();
	e2.eventType = FocusInEvent;
//	e2.eventType = 5; // i.e. enum value other_event in class Event 
	e2.target = attrRowList->CurRow()->EditField();
	PrintEvent(&e2);
	attrRowList->CurRow()->NameField()->Highlight(true);
	attrRowList->CurRow()->TypeField()->Highlight(true);
	UnRead(e2);
	return 1;
    }
    return 0;
}

#ifdef otherEditSEE
int StepEntityEditor::Edit() 
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Edit()\n";

    seeAttrRow *ptr;
    Event e;

if(attrRowList->CurRow()){
    if(debug_level >= PrintValues) 
	cout << "attrRowList->Head(): " << (int)attrRowList->Head() << "\n";

//    ComponentsListen();
    buttonsButSt->Detach(this);
    editorsButSt->Detach(this);

    buttonsButSt->SetValue(0);
//    editorsButSt->SetValue(0);
    editorsButSt->SetNull();
    input->Ignore(DownEvent);
    attrRowList->CurRow()->NameField()->Highlight(true);
    attrRowList->CurRow()->TypeField()->Highlight(true);
   
    EditRows();
    for(; ;){
	Read(e);
	ptr = attrRowList->FindEditField(e.target);
	if(ptr)
	{
	    attrRowList->SetCurRow(ptr);
		// call handle() instead of EditRows() so it sets 
		// the cursor correctly
	    ((StringEditor2 *)e.target)->Handle(e);
	    if(ExecuteTermChar())
		EditRows();
	}
	else if(e.target == saveCompleteBut || e.target == saveIncompleteBut ||
	       e.target == cancelBut || e.target == deleteBut)
	{
		e.target->Handle(e);
		int v;
		if(v = CheckButtons())
		{
		    if(!Pinned())
		    {
			//ComponentsIgnore();
			buttonsButSt->Attach(this);
			editorsButSt->Attach(this);
				input->Catch(DownEvent);
			return(v);
		    }
		    EditRows();
		}
	}
/*
	else if(e.target == replicateBut || e.target == editBut || 
		e.target == markBut || e.target == listBut || 
		e.target == pinTogBut || e.target == modifyTogBut)
*/
	else if(e.target == replicateBut || e.target == editBut || 
		e.target == markBut || e.target == pinTogBut || 
		e.target == modifyTogBut)
	{
		e.target->Handle(e);
		if(CheckButtons()) {
		    EditRows();
		}
	}
	else
	{
		UnRead(e);
		if(debug_level >= PrintFunctionTrace) 
		    cout << "leaving StepEntityEditor::Handle() \n";
//		ComponentsIgnore();
		buttonsButSt->SetValue(0);
		editorsButSt->SetNull();
//		editorsButSt->SetValue(0);
		buttonsButSt->Attach(this);
		editorsButSt->Attach(this);

		input->Catch(DownEvent);
		attrRowList->CurRow()->NameField()->Highlight(false);
		attrRowList->CurRow()->TypeField()->Highlight(false);
		return(0);
	}
    }
} else dp->ErrorMsg("Entity has no attributes");
};
#endif

///////////////////////////////////////////////////////////////////////////////
// Update()
// this function is called whenever either of editorsButSt's or buttonsButSt's
// values change (as long as the SEE is attached to them)
///////////////////////////////////////////////////////////////////////////////

void StepEntityEditor::Update()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Update()\n";
    seeAttrRow *ptr;
    Event e;

    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Update()\n";
    SubordinateInfo *si = 0;

//    editorsButSt->GetValue((void*)si);  // the cast makes this not work
//    editorsButSt->GetValue(si); // this no longer works with Sun C++ => error
				  // Must assign as below  DAS
    void * tmpVoidPtr = 0;
    editorsButSt->GetValue(tmpVoidPtr);
    si = (SubordinateInfo *) tmpVoidPtr;

    if(si){
	if(debug_level >= PrintValues) 
	    cout << "subordinate term char: " << (int)si->GetValue() 
		<< "   sub: " << (int)si->GetSubordinate() << "\n";
		 
	int v;
	si->GetValue(v);
		   // the following instance is derived from StringEditor2
	StringEditor2 *se = (StringEditor2 *)si->GetSubordinate();

	// this follows the initial hit in one of the SEE editors, this moves 
	// focus from the other object to the SEE.
	if(v == '\001')
	{
//	    messageDisp->Message(" ");
	    Read(e);
	    if(e.eventType == DownEvent)
	    {
		ptr = attrRowList->FindEditField(se);
		if(ptr) {
			// set current row and highlight it
			// set row to one just chosen
		    attrRowList->SetCurRow(ptr);
		    SetAttrTypeMessage(attrRowList->CurRow());
		}
	    }
	    editorsButSt->SetNull();

	    if(se != label)
	    {
		attrRowList->CurRow()->EditField()->StringEditor2::Handle(e);
	    }
	    else
		se->StringEditor2::Handle(e);

	    editorsButSt->SetNull();

/*
	    if(se != label)
	    {
		messageDisp->Message(" ");
		ptr = attrRowList->FindEditField(se);
		if(ptr) {
			// set current row and highlight it
			// set row to one just chosen
		    attrRowList->SetCurRow(ptr);
		    ((StringEditor2*)(attrRowList->CurRow()->EditField()))->StringEditor2::Handle(e);
		}
	    }
*/
	} 
//	else if(v == SEE_NEXT || v == SEE_PREV) // wrong: could be any cmd! 
	else if(v != 0)
	{
	// This initiates the edit of the next editor field in the SEE
	// based on the terminating char of the previous editor.
	// It initiates the edit by preloading an event for that editor.
//	    se->RemoveCursor();	// remove cursor from current editor.
	    StringEditor *nextEditor;
	    static Event e;
	    if(se == label)
	    {
		attrRowList->SetCurRow(attrRowList->Head());
		nextEditor = attrRowList->Head()->EditField();
		e.poll();
		e.target = nextEditor;
		e.eventType = FocusInEvent;
//		e.eventType = 5; // i.e. enum value other_event in class Event 
		UnRead(e);
	    }
	    else if(ExecuteTermChar())
	    {
		nextEditor = attrRowList->CurRow()->EditField();
		e.poll();
		e.target = nextEditor;
		e.eventType = FocusInEvent;
//		e.eventType = 5; // i.e. enum value other_event in class Event 
		UnRead(e);
	    }
	} 
//	editorsButSt->Detach(this);
//	editorsButSt->SetValue(0);
	editorsButSt->SetNull();
//	editorsButSt->Attach(this);
    }
    else {
	CheckButtons();
    }
#ifdef firstWaySEEUpdate
    int buttonValue;
    seeAttrRow *ptr;
    SubordinateInfo *si = 0;

//    editorsButSt->GetValue((void*)si);  // the cast makes this not work
    editorsButSt->GetValue(si);
    if(si){
	int v;
	si->GetValue(v);
		   // the following instance is derived from StringEditor2
	StringEditor2 *se = (StringEditor2 *)si->GetSubordinate();
	if(v == '\001')
	{
	    if(se != label)
	    {
		ptr = attrRowList->FindEditField(se);
		if(ptr) {
			// set current row and highlight it
			// set row to one just chosen
		    attrRowList->SetCurRow(ptr);
		}
	    }
	    editorsButSt->Detach(this);
//	    editorsButSt->SetValue(0);
	    editorsButSt->SetNull();
	    editorsButSt->Attach(this);
	} 
//	else if(v == '\r' || v == '\016' || v == '\020')
	else if(v == SEE_NEXT || v == SEE_PREV)
	{
			// set row based on editing termination
	    editorsButSt->Detach(this);
		// remove cursor
	    se->RemoveCursor();
	    if(se == label)
	    {
		attrRowList->SetCurRow(attrRowList->Head());
		EditRows();
	    }
	    else if(ExecuteTermChar())
	    {
		EditRows();
	    }
//	    editorsButSt->SetValue(0);
	    editorsButSt->SetNull();
	    editorsButSt->Attach(this);
	} 
    }
    else {
	CheckButtons();
    }
#endif
#ifdef secondWaySEEUpdate
    int buttonValue;
    seeAttrRow *ptr;
    Event e;

    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Update()\n";
    SubordinateInfo *si = 0;

//    editorsButSt->GetValue((void*)si);  // the cast makes this not work
    editorsButSt->GetValue(si);
    if(si){
	if(debug_level >= PrintValues) 
	    cout << "subordinate term value: " << (int)si->GetValue() 
		<< "   sub: " << (int)si->GetSubordinate() << "\n";
		 
		// the following instance is derived from StringEditor2
	StringEditor2 *se = (StringEditor2 *)si->GetSubordinate();
	ptr = attrRowList->FindEditField(se);
	if(ptr) {
	    if(debug_level >= PrintValues) 
		cout << "found the editor you choose, now remove "
		<< "cursor from previous editor\n";
		// remove cursor
	    se->RemoveCursor();
		// set current row and highlight it

	    attrRowList->SetCurRow(ptr);	// set row to one just chosen
	    editorsButSt->Detach(this);
//	    editorsButSt->SetValue(0);
	    editorsButSt->SetNull();
	    Read(e);	// re-read event
	    ((StringEditor2 *)e.target)->Handle(e);

	    if(ExecuteTermChar())	// set row based on editing termination
		Edit();	// continue editing at current row
	    else
	    {
//		editorsButSt->SetValue(0);
		editorsButSt->SetNull();
		editorsButSt->Attach(this);
	    }
	} else if(se == label){
//	    editorsButSt->Detach(this);
//	    editorsButSt->SetValue(0);
	    Read(e);	// re-read event
	    ((StringEditor2Sub *)e.target)->StringEditor2::Handle(e);

	    SubordinateInfo *si;
	    editorsButSt->GetValue(si);
	    if(si){
//		int termCode = 0;
//		si->GetValue(termCode);
		attrRowList->SetCurRow(attrRowList->Head());
		Edit();	// continue editing at current row
	    }
/*
	    else
	    {
//		editorsButSt->SetValue(0);
//		editorsButSt->Attach(this);
	    }
*/
	}
    }
    else {
	CheckButtons();
    }
#endif
}

void StepEntityEditor::Handle(Event& e)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Handle()\n";
    if(attrRowList->RowCount() > 0)
    {
	static Event e2;
	if(e.eventType == KeyEvent)
	{
	    // copy the event info. (most importantly the keystroke info.)
	    e2 = e;
	}
	else
	{
	    // just to make sure everything gets assigned that needs to
//	    e2.poll(); //DAS IVBUG
	    e2 = e;
	    e2.eventType = FocusInEvent; // assign some harmless event type
//	    e2.eventType = 5; // i.e. enum value other_event in class Event 
	}
	/* assign the attr. editor for the current attr to receive the event */
	e2.target = attrRowList->CurRow()->EditField();
	PrintEvent(&e2);
	attrRowList->CurRow()->NameField()->Highlight(true);
	attrRowList->CurRow()->TypeField()->Highlight(true);
	if(e2.target)
//	    UnRead(e2);
	    e2.target->Handle(e2);
    }
}

void 
StepEntityEditor::Insert(STEPattribute *attr, int nameWidth, int editWidth,
			int typeWidth)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Insert()\n";
//    extern Sensor *noEvents;
    seeAttrRow *temp;

    temp = new seeAttrRow(attr, instMgr, editorsButSt, nameWidth, editWidth, 
			  typeWidth);
//    temp->EditField()->Listen(noEvents);
    attrRowList->InsertAfter(temp, attrRowList->LastRow());
    if(temp != attrRowList->Head()){
	VBox::Insert(new VGlue(SEE_ATTR_VSPACE, SEE_ATTR_VSPACE));
	VBox::Insert(temp);
    } else {
	VBox::Insert(temp);
    }
}

void StepEntityEditor::ComponentsIgnore()
{
/*
    static Sensor *sensor = 0;
    if(!sensor)
    {
	sensor = new Sensor();
	sensor->Ignore(DownEvent);
	sensor->Ignore(KeyEvent);
    }
*/
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::ComponentsIgnore()\n";
    attrRowList->ComponentsIgnore();
//    saveCompleteBut->Disable();
//    saveIncompleteBut->Disable();
//    cancelBut->Disable();
//    deleteBut->Disable();

//    replicateBut->Disable();	// BUG since we haven't implemented this yet
    editBut->Disable();
    markBut->Disable();
//    listBut->Disable();
//    pinTogBut->Disable();

//    input = sensor;
    input->Ignore(DownEvent);
    input->Ignore(KeyEvent);
};

void StepEntityEditor::ComponentsListen()
{
/*
    static Sensor *sensor = 0;
    if(!sensor)
    {
	sensor = new Sensor();
	sensor->Catch(DownEvent);
	sensor->Catch(KeyEvent);
    }
*/
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::ComponentsListen()\n";
    attrRowList->ComponentsListen();
//    saveCompleteBut->Enable();
//    saveIncompleteBut->Enable();
//    cancelBut->Enable();
//    deleteBut->Enable();

//    replicateBut->Enable();	// BUG since we haven't implemented this yet
    editBut->Enable();
    markBut->Enable();
//    listBut->Enable();
//    pinTogBut->Enable();

//    input = sensor;
    input->Catch(DownEvent);
    input->Catch(KeyEvent);
};

void StepEntityEditor::PressButton(int butCode)
{
    switch(butCode)
    {
	case SEE_SAVE_COMPLETE:
	case SEE_SAVE_INCOMPLETE:
	case SEE_CANCEL:
	case SEE_DELETE:
	case SEE_REPLICATE:
	case SEE_EDIT:
	case SEE_MARK:
	case SEE_LIST:
		buttonsButSt->SetValue(butCode);
		break;
	case SEE_MODIFY:
	case SEE_VIEW:
		modifyTogButSt->SetValue(butCode);
		break;
	case SEE_PIN:
	case SEE_NOT_PIN:
		pinTogButSt->SetValue(butCode);
		break;
    }
}

// the constructor StepEntityEditor() {} gets called for this class.
HeaderEntityEditor::HeaderEntityEditor(MgrNode *mn, InstMgr *im, int pinBool, 
				       int modifyBool)
{
    mgrNode = mn;
    stepEnt = mgrNode->GetSTEPentity();
    instMgr = im;
	// this ButtonState is needed for below but is not of any real use.
    dispButSt = new ButtonState();
    messageDisp = new StringDisplay(dispButSt, 
	"                                                             ");
    CreateButtonsAndStates(modifyBool, pinBool);
    input = new Sensor(); 
    input->Catch(DownEvent);
    input->Catch(KeyEvent);
    attrRowList = new seeAttrRowList();

    CreateHeading();
    CreateInsertAttributeEditors();
    InsertButtons();
    if(!modifyBool)
	ComponentsIgnore();

    editorsButSt->Reference();
    buttonsButSt->Reference();
    modifyTogButSt->Reference();
    pinTogButSt->Reference();
}


HeaderEntityEditor::~HeaderEntityEditor()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "HeaderEntityEditor::~HeaderEntityEditor()\n";

// these don't get called by StepEntityEditor's destructor this redefines it.
    Unref(editorsButSt);
    Unref(buttonsButSt);
    Unref(modifyTogButSt);
    Unref(pinTogButSt);
}

void HeaderEntityEditor::ComponentsIgnore()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "HeaderEntityEditor::ComponentsIgnore()\n";
    attrRowList->ComponentsIgnore();

    input->Ignore(DownEvent);
    input->Ignore(KeyEvent);
}

void HeaderEntityEditor::ComponentsListen()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "HeaderEntityEditor::ComponentsListen()\n";
    attrRowList->ComponentsListen();

    input->Catch(DownEvent);
    input->Catch(KeyEvent);
}

void HeaderEntityEditor::PressButton(int butCode)
{
    switch(butCode)
    {
	case SEE_SAVE_COMPLETE:
	case SEE_SAVE_INCOMPLETE:
	case SEE_CANCEL:
		buttonsButSt->SetValue(butCode);
		break;
	case SEE_MODIFY:
	case SEE_VIEW:
		modifyTogButSt->SetValue(butCode);
		break;
	case SEE_PIN:
	case SEE_NOT_PIN:
		pinTogButSt->SetValue(butCode);
		break;
    }
}

	// Called when a subject (buttonsButSt or editorsButSt) is set to
	// a NEW value.  StepEntityEditor is 'Subject::Attach()ed' to the
	// two subjects except during the scope of StepEntityEditor::Edit()
	// when it is 'Subject::Detach()ed'.  This means that it's called
	// when (inside a SEE) a button is pushed or an attribute field is
	// changed (and terminated by a terminating char) outside of
	// the scope of StepEntityEditor::Edit().  It does the appropriate
	// action associated with the button or the term char for an attribute
	// edit.  

void HeaderEntityEditor::Update()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::Update()\n";
    seeAttrRow *ptr;
    Event e;

    if(debug_level >= PrintFunctionTrace) 
	cout << "HeaderEntityEditor::Update()\n";
    SubordinateInfo *si = 0;

//    editorsButSt->GetValue((void*)si);  // the cast makes this not work
//    editorsButSt->GetValue(si);
    void * tmpVoidPtr = 0;
    editorsButSt->GetValue(tmpVoidPtr);
    si = (SubordinateInfo *) tmpVoidPtr;

    if(si){
	if(debug_level >= PrintValues) 
	    cout << "subordinate term char: " << (int)si->GetValue() 
		<< "   sub: " << (int)si->GetSubordinate() << "\n";
		 
	int v;
	si->GetValue(v);
		   // the following instance is derived from StringEditor2
	StringEditor2 *se = (StringEditor2 *)si->GetSubordinate();
	// this follows the initial hit in one of the SEE editors, this moves 
	// focus from the other object to the SEE.
	if(v == '\001')
	{
	    messageDisp->Message(" ");
	    Read(e);
	    if(e.eventType == DownEvent)
	    {
		ptr = attrRowList->FindEditField(se);
		if(ptr) {
			// set current row and highlight it
			// set row to one just chosen
		    attrRowList->SetCurRow(ptr);
		}
	    }
	    editorsButSt->SetNull();

	    if(se != label)
	    {
		attrRowList->CurRow()->EditField()->StringEditor2::Handle(e);
	    }
	    else
		se->StringEditor2::Handle(e);

	    editorsButSt->SetNull();
/********************************************
	    if(se != label)
	    {
		messageDisp->Message(" ");
		ptr = attrRowList->FindEditField(se);
		if(ptr) {
			// set current row and highlight it
			// set row to one just chosen
		    attrRowList->SetCurRow(ptr);
		    Read(e);
		    ((StringEditor2*)(attrRowList->CurRow()->EditField()))->StringEditor2::Handle(e);
		}
	    }
********************************************/
	} 
//	else if(v == SEE_NEXT || v == SEE_PREV) // wrong: could be any cmd! 
	else if(v != 0)
	{
	// This initiates the edit of the next editor field in the SEE
	// based on the terminating char of the previous editor.
	// It initiates the edit by preloading an event for that editor.
//	    se->RemoveCursor();	// remove cursor from current editor.
	    StringEditor *nextEditor;
	    static Event e;
	    if(se == label)
	    {
		attrRowList->SetCurRow(attrRowList->Head());
		nextEditor = attrRowList->Head()->EditField();
		e.poll();
		e.target = nextEditor;
		e.eventType = FocusInEvent;
//		e.eventType = 5; // i.e. enum value other_event in class Event 
		UnRead(e);
	    }
	    else if(ExecuteTermChar())
	    {
		nextEditor = attrRowList->CurRow()->EditField();
		e.poll();
		e.target = nextEditor;
		e.eventType = FocusInEvent;
//		e.eventType = 5; // i.e. enum value other_event in class Event 
		UnRead(e);
	    }
	} 
//	editorsButSt->Detach(this);
//	editorsButSt->SetValue(0);
	editorsButSt->SetNull();
//	editorsButSt->Attach(this);
    }
    else {
	CheckButtons();
    }
}

void HeaderEntityEditor::CreateHeading()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "HeaderEntityEditor::CreateHeading()\n";
    VBox::Insert(new VGlue(SEE_VSPACE, 0));

    Message *titleMsg = new Message(stepEnt->EntityName());
//BUG need this to set the font of the title?
//    titleMsg->SetClassName("SEEtitle");
    VBox::Insert(new HBox(
			new HGlue(round(.08*inch), 0, 0), 
			titleMsg,
			new HGlue
		     )
		);
    VBox::Insert(new VGlue(SEE_VSPACE, 0));
}

void HeaderEntityEditor::CreateButtonsAndStates(int modifyBool, int pinBool)
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::CreateButtonsAndStates()\n";
    editorsButSt = new MyButtonState(0);
    editorsButSt->Attach(this);

    buttonsButSt = new ButtonState(0);
    buttonsButSt->Attach(this);

    if(modifyBool)
    {
	modifyTogVal = SEE_MODIFY;  // set modifyTogButSt to same value as this
	modifyTogButSt = new ButtonState(SEE_MODIFY);
    }
    else
    {
	modifyTogVal = SEE_VIEW;  // set modifyTogButSt to same value as this
	modifyTogButSt = new ButtonState(SEE_VIEW);
    }
    modifyTogButSt->Attach(this);

    if(pinBool)
	pinTogButSt = new ButtonState(SEE_PIN);
    else
	pinTogButSt = new ButtonState(SEE_NOT_PIN);
    pinTogButSt->Attach(this);

    saveCompleteBut = new MyPushButton("save", buttonsButSt, 
					SEE_SAVE_COMPLETE);
    saveIncompleteBut = new MyPushButton("i", buttonsButSt,
					SEE_SAVE_INCOMPLETE);
    cancelBut = new MyPushButton("c", buttonsButSt,
					SEE_CANCEL);
//    editBut = new MyPushButton("e", buttonsButSt,
//					SEE_EDIT);
    attrInfoBut = new MyPushButton("a", buttonsButSt,
					SEE_ATTR_INFO);
    modifyTogBut = new ModifyViewCheckBox(" ", modifyTogButSt,
					SEE_MODIFY, SEE_VIEW);
    pinTogBut = new PinCheckBox("    ", pinTogButSt,
					SEE_PIN, SEE_NOT_PIN);
}

void HeaderEntityEditor::InsertButtons()
{
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::InsertButtons()\n";
    VBox::Insert(new VGlue(SEE_VSPACE, 0));
    VBox::Insert(new HBox(
			  new HGlue(round(.05*inch), 0, 0), 
			  new Frame(messageDisp), 
			  new HGlue(round(.05*inch), 0, 0)
			  ));
    VBox::Insert(new VGlue(SEE_VSPACE, 0));

    VBox::Insert(
	new HBox(
	    new HBox(
		new HGlue(round(.1*inch)),
		modifyTogBut,
		new HGlue(round(.1*inch)),
		saveCompleteBut,
		new HGlue(round(.1*inch)),
		saveIncompleteBut,
		new HGlue(round(.1*inch))
	    ),
	    new HBox(
		cancelBut,
		new HGlue(round(.1*inch))
	    ),
	    new HBox(
//		editBut,
//		new HGlue(round(.1*inch)),
		attrInfoBut,
		new HGlue(round(.1*inch))
	    )
	)
    );
    VBox::Insert(new VGlue(SEE_VSPACE, 0));
}

int HeaderEntityEditor::ExecuteCommand(int v)
{
    int rc = 0;
    if(debug_level >= PrintFunctionTrace) 
	cout << "StepEntityEditor::ExecuteCommand()\n";
    switch(v){
	    case SEE_PIN : 
		int prevVal;
		pinTogButSt->GetValue(prevVal);
		if(prevVal == SEE_PIN)
		    pinTogButSt->SetValue(SEE_NOT_PIN);
		else
		    pinTogButSt->SetValue(SEE_PIN);
		
		return(v);

	    case SEE_SAVE_COMPLETE : 

		if(SaveComplete())
;//		    dp->seeSaveComplete(this);
		else
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
;//		    dp->seeSaveIncomplete(this);
		}
		buttonsButSt->SetValue(0);
	// don't do this since buttons need to be notified to redraw themselves
//		buttonsButSt->SetNull();
		return(v);
	    case SEE_SAVE_INCOMPLETE : 

		if(!SaveIncomplete())
		{
		    fprintf(stderr, "%c" , RingBell);
		    fflush(stderr);
		}
;//		dp->seeSaveIncomplete(this);
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_CANCEL : 
		UndoChanges();
		messageDisp->Message(
			   "Entity values reverted to last saved state.");
;//		dp->seeCancel(this);
		buttonsButSt->SetValue(0);
		return(v);
	    case SEE_ATTR_UNDO : 
	    {
		seeAttrRow *row = attrRowList->CurRow();
		if(row) row->UndoChanges();
		buttonsButSt->SetValue(0);
		return(v);
	    }
	    case SEE_MODIFY : 
//		dp->seeToggle(this, SEE_MODIFY);
		ComponentsListen();
		return(v);
	    case SEE_VIEW : 
		if(attrRowList->CurRow()){
		    attrRowList->CurRow()->NameField()->Highlight(false);
		    attrRowList->CurRow()->TypeField()->Highlight(false);
		}
//		dp->seeToggle(this, SEE_VIEW);
//		SetPinned(0);
		ComponentsIgnore();
		return(v);
	    case SEE_ATTR_INFO:
		{
		SetAttrTypeMessage(attrRowList->CurRow());
//		dp->seeOpenSED(this);
		buttonsButSt->SetValue(0);
		return(v);
		}
	    case SEE_NEXT:
		{
		    Severity s = ValidateAttr(attrRowList->CurRow(), 
					      SEVERITY_INCOMPLETE, 1);
		    if(s >= SEVERITY_INCOMPLETE)
			attrRowList->SetCurRow(attrRowList->NextRow());
		    else
		    {
			fprintf(stderr, "%c" , RingBell);
			fflush(stderr);
		    }
		    return(v);
		}
	    case SEE_PREV:
		{
		    Severity s = ValidateAttr(attrRowList->CurRow(), 
					      SEVERITY_INCOMPLETE, 1);
		    if(s >= SEVERITY_INCOMPLETE)
			attrRowList->SetCurRow(attrRowList->PreviousRow());
		    else
		    {
			fprintf(stderr, "%c" , RingBell);
			fflush(stderr);
		    }
		    return(v);
		}
	    default:
		buttonsButSt->SetValue(0);
		return(0);
	}
}


