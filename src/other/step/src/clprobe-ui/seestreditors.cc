
/*
* NIST Data Probe Class Library
* clprobe-ui/seestreditors.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: seestreditors.cc,v 3.0.1.1 1997/11/05 23:01:09 sauderd DP3.1 $ */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <stdio.h>	// for printf()
#include <ctype.h>	// for iscntrl()
#include <string.h>	// for strchr()
#include <IV-2_6/InterViews/textbuffer.h>
#include <seedefines.h>
#include <seestreditors.h>

const char RingBell = '\007';   // <CNTRL> G 
const char Pound = '#';
const char CommaSpace[] = ", ";
const char CommaSpacePound[] = ", #";
const char OpenParen = '(';
const char CloseParen = ')';

#define EDITOR_TYPE_CHECKING 0

int CheckTermChar(char c, const char lastChar, const char *done)
{
    if(lastChar == CONTROL_X){
	switch(c){
	    case CONTROL_S:
	    case 's':
	    case 'S':
//		printf("^x s - SAVE COMPLETE\n");
		return SEE_SAVE_COMPLETE;
	    case CONTROL_I:
	    case 'i':
	    case 'I':
//		printf("^x i - SAVE INCOMPLETE\n");
		return SEE_SAVE_INCOMPLETE;
	    case CONTROL_C:
	    case 'c':
	    case 'C':
//		printf("^x c - CANCEL\n");
		return SEE_CANCEL;
	    case CONTROL_D:
	    case 'd':
	    case 'D':
//		printf("^x d - DELETE\n");
		return SEE_DELETE;
	    case CONTROL_R:
	    case 'r':
	    case 'R':
//		printf("^x r - REPLICATE\n");
		return SEE_REPLICATE;
	    case CONTROL_E:
	    case 'e':
	    case 'E':
//		printf("^x e - EDIT\n");
		return SEE_EDIT;
	    case CONTROL_M:
	    case 'm':
	    case 'M':
//		printf("^x m - MARK\n");
		return SEE_MARK;
	    case CONTROL_L:
	    case 'l':
	    case 'L':
//		printf("^x l - LIST\n");
		return SEE_LIST;
	    case CONTROL_A:
	    case 'a':
	    case 'A':
//		printf("^x a - ATTR_INFO\n");
		return SEE_ATTR_INFO;
	    case CONTROL_T:
	    case 't':
	    case 'T':
//		printf("^x t - OPEN_SED\n");
		return SEE_OPEN_SED;
	    case CONTROL_U:
	    case 'u':
	    case 'U':
//		printf("^x u - ATTRIBUTE UNDO\n");
		return SEE_ATTR_UNDO;
	    case CONTROL_P:
	    case 'p':
	    case 'P':
		return SEE_PIN;
	}
    }
    else if(strchr(done, c) != nil)
    {
	switch(c){
	    case CONTROL_N:
	    case CARRAIGE_RETURN:
		return SEE_NEXT;
	    case CONTROL_P:
		return SEE_PREV;
	}
	return c;
    }
    return 0;
};

///////////////////////////////////////////////////////////////////////////////
//
// seeStringEditor class member functions
//
///////////////////////////////////////////////////////////////////////////////

seeStringEditor::~seeStringEditor() 
{
}

/*
 this is defined so the supervisor will be notified before this 
 seeStringEditor handles it's own event.  Since this is defined 
 StringEditor2::Handle() needs to be called for this object when the 
 supervisor has done what it wanted to do before this object handled it's 
 event.  You will need to read the event if you don't call Handle() for this 
 object otherwise this function will cause an infinite loop (i.e. read the 
 event, unread the event, read the event, unread the event, etc)
*/
void seeStringEditor::Handle(Event& e)
{
	if(e.eventType == DownEvent || e.eventType == KeyEvent)
//	if(e.eventType == 2 || e.eventType == 4)
	{
	    UnRead(e);
	    subInfo->SetValue('\001');
	    subject->SetValue(subInfo);  // don't cast this to (void*)
//		setting a value for 'subject' will cause the subject to call 
//		Notify() which calls StepEntityEditor::Update() since 
//		StepEntityEditor is Attach()ed to the subject
//	    StringEditor2::Handle(e);
	}
	else
	    StringEditor2::Handle(e);
/*
    if (e.target == this) {
    }
    else
	UnRead(e);
*/
}

void seeStringEditor::NewEdit()
{
    Event e;
    e.target = nil;
    e.eventType = EnterEvent;
    StringEditor2::Handle(e);
}

///////////////////////////////////////////////////////////////////////////////
// This checks for valid keystrokes that won't be entered into
// the text of the editor.  This function is defined for these reasons:
// 1) in order to add two keystroke commands you don't have to
//	redefine InsertValidChar() (can use the derived one)
// 2) logically groups the group of non-terminating characters in
//	multiple character cmds
// 3) keeps InsertValidChar() from beeping when you type
//	non-terminating chars in a 2 char cmd.
///////////////////////////////////////////////////////////////////////////////

boolean seeStringEditor::CheckForCmdChars(char c)
{
//    if(c == CONTROL_X && lastChar != CONTROL_X)
    if(c == CONTROL_X)
    {
	return true;
    }
    else // added else because CenterLine could not handle inline without it
	return false;
}

int seeStringEditor::TermChar(char c)
{
    int rc = CheckTermChar(c, (const char)lastChar, done);
    if(rc)
	lastChar = rc;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
//
// seeIntEditor class member functions
//
///////////////////////////////////////////////////////////////////////////////

seeIntEditor::~seeIntEditor() 
{
}

/*
 this is defined so the supervisor will be notified before this 
 seeIntEditor handles it's own event.  Since this is defined 
 StringEditor2::Handle() needs to be called for this object when the 
 supervisor has done what it wanted to do before this object handled it's 
 event.  You will need to read the event if you don't call Handle() for this 
 object otherwise this function will cause an infinite loop (i.e. read the 
 event, unread the event, read the event, unread the event, etc)
*/
void seeIntEditor::Handle(Event& e)
{
	if(e.eventType == DownEvent || e.eventType == KeyEvent)
//	if(e.eventType == 2 || e.eventType == 4)
	{
	    UnRead(e);
	    subInfo->SetValue('\001');
	    subject->SetValue(subInfo);  // don't cast this to (void*)
//		call notify for subject (which calls see update)
//	    StringEditor2::Handle(e);
	}
	else
	    StringEditor2::Handle(e);
/*
    if (e.target == this) {
    }
    else
	UnRead(e);
*/
}

void seeIntEditor::NewEdit()
{
    Event e;
    e.target = nil;
    e.eventType = EnterEvent;
    StringEditor2::Handle(e);
}

///////////////////////////////////////////////////////////////////////////////
// This checks for valid keystrokes that won't be entered into
// the text of the editor.  This function is defined for these reasons:
// 1) in order to add two keystroke commands you don't have to
//	redefine InsertValidChar() (can use the derived one)
// 2) logically groups the group of non-terminating characters in
//	multiple character cmds
// 3) keeps InsertValidChar() from beeping when you type
//	non-terminating chars in a 2 char cmd.
///////////////////////////////////////////////////////////////////////////////

boolean seeIntEditor::CheckForCmdChars(char c)
{
//    if(c == CONTROL_X && lastChar != CONTROL_X)
    if(c == CONTROL_X)
    {
	return true;
    }
    else // added else because CenterLine could not handle inline without it
	return false;
}

int seeIntEditor::TermChar(char c)
{
    int rc = CheckTermChar(c, (const char)lastChar, done);
    if(rc)
	lastChar = rc;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
//
// seeIntAggrEditor class member functions
//
///////////////////////////////////////////////////////////////////////////////

seeIntAggrEditor::~seeIntAggrEditor()
{
    delete subInfo; 
}

void seeIntAggrEditor::Handle(Event& e)
{
	if(e.eventType == DownEvent || e.eventType == KeyEvent)
//	if(e.eventType == 2 || e.eventType == 4)
	{
	    UnRead(e);
	    subInfo->SetValue('\001');
	    subject->SetValue(subInfo);  // don't cast this to (void*)
//		call notify for subject (which calls see update)
//	    StringEditor2::Handle(e);
	}
	else
	    StringEditor2::Handle(e);
/*
    if (e.target == this) {
    }
    else
	UnRead(e);
*/
}

void seeIntAggrEditor::NewEdit()
{
    Event e;
    e.target = nil;
    e.eventType = EnterEvent;
    StringEditor2::Handle(e);
}

int seeIntAggrEditor::TypeCheck()
{

#if EDITOR_TYPE_CHECKING

    const char *attrVal = text->Text();
    char *curAttr = (char *)attrVal;

    long int tmpI = 0;
    char tmpstr[3];
    int numFound = 0;
    char prevCh = '\0';
    while(curAttr){
    
	tmpI = 0;
	numFound = sscanf(curAttr," %ld %1s", &tmpI, tmpstr);
	switch(numFound){
	  case 0:
		// 0 => invalid beginning;
		numFound = sscanf(curAttr," %1s", tmpstr);
		if(numFound && tmpstr[0] == ',')
		{ // found null int value 
		    curAttr = strchr(curAttr, ',');
		    curAttr++;
		    prevCh = ',';
		}
		else
		{
		    fprintf(stderr, "%c invalid int aggregate attr value\n", 
			RingBell);
		    fflush(stderr);
		    return 0;
		}
		break;
	  case 2:
	    // 2 => invalid ending
		if(tmpstr[0] == ','){
		    curAttr = strchr(curAttr, ',');
		    curAttr++;
		    prevCh = ',';
		}
		else{
		    fprintf(stderr, "%c invalid int attr value: '%c'\n", 
				RingBell, tmpstr[0]);
		    fflush(stderr);
		    return 0;
		}
		break;
	  case EOF:
	    // EOF => blank field
		if(prevCh == '\0' || prevCh == ',')
		    return SETYPE_SIGNED_INTEGER;
		else {
		    fprintf(stderr, "%c invalid int attr value: '%c'\n", 
				RingBell, prevCh);
		    fflush(stderr);
		    return 0;
		}
	  case 1:
	    // 1 => success
//		printf("edit results returned %s\n", attrVal);
		return SETYPE_SIGNED_INTEGER;
	}
    }
#else
    return SETYPE_SIGNED_INTEGER;
#endif
}

boolean seeIntAggrEditor::InsertValidChar(char c)
{
    switch (c) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '-':
	case '+':
		InsertText(&c, 1);
		return true;
	case ' ':
	case ',':
//		if(left != 0){	// do not insert initial commas
		    InsertText(CommaSpace, 2);
		    return true;
//		} else 
//		    return false;
	case OpenParen:	// '('
		if(left == 0){
		    InsertText(&OpenParen, 1);
		    return true;
		} else 
		    return false;
	case CloseParen: // ')'
	{
		int lastCharIndex = text->Width();
		if(left == lastCharIndex)
		{
		    InsertText(&CloseParen, 1);
		    return true;
		} else 
		    return false;
	}
	default:
		return false;
    }
}

void seeIntAggrEditor::SetSubject(int c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}

///////////////////////////////////////////////////////////////////////////////
// This checks for valid keystrokes that won't be entered into
// the text of the editor.  This function is defined for these reasons:
// 1) in order to add two keystroke commands you don't have to
//	redefine InsertValidChar() (can use the derived one)
// 2) logically groups the group of non-terminating characters in
//	multiple character cmds
// 3) keeps InsertValidChar() from beeping when you type
//	non-terminating chars in a 2 char cmd.
///////////////////////////////////////////////////////////////////////////////

boolean seeIntAggrEditor::CheckForCmdChars(char c)
{
//    if(c == CONTROL_X && lastChar != CONTROL_X)
    if(c == CONTROL_X)
    {
	return true;
    }
    else // added else because CenterLine could not handle inline without it
	return false;
}

int seeIntAggrEditor::TermChar(char c)
{
    int rc = CheckTermChar(c, (const char)lastChar, done);
    if(rc)
	lastChar = rc;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
//
// seeRealEditor class member functions
//
///////////////////////////////////////////////////////////////////////////////

seeRealEditor::~seeRealEditor() 
{
}

/*
 this is defined so the supervisor will be notified before this 
 seeRealEditor handles it's own event.  Since this is defined 
 StringEditor2::Handle() needs to be called for this object when the 
 supervisor has done what it wanted to do before this object handled it's 
 event.  You will need to read the event if you don't call Handle() for this 
 object otherwise this function will cause an infinite loop (i.e. read the 
 event, unread the event, read the event, unread the event, etc)
*/
void seeRealEditor::Handle(Event& e)
{
	if(e.eventType == DownEvent || e.eventType == KeyEvent)
//	if(e.eventType == 2 || e.eventType == 4)
	{
	    UnRead(e);
	    subInfo->SetValue('\001');
	    subject->SetValue(subInfo);  // don't cast this to (void*)
//		call notify for subject (which calls see update)
//	    StringEditor2::Handle(e);
	}
	else
	    StringEditor2::Handle(e);
/*
    if (e.target == this) {
    }
    else
	UnRead(e);
*/
}

void seeRealEditor::NewEdit()
{
    Event e;
    e.target = nil;
    e.eventType = EnterEvent;
    StringEditor2::Handle(e);
}

///////////////////////////////////////////////////////////////////////////////
// This checks for valid keystrokes that won't be entered into
// the text of the editor.  This function is defined for these reasons:
// 1) in order to add two keystroke commands you don't have to
//	redefine InsertValidChar() (can use the derived one)
// 2) logically groups the group of non-terminating characters in
//	multiple character cmds
// 3) keeps InsertValidChar() from beeping when you type
//	non-terminating chars in a 2 char cmd.
///////////////////////////////////////////////////////////////////////////////

boolean seeRealEditor::CheckForCmdChars(char c)
{
//    if(c == CONTROL_X && lastChar != CONTROL_X)
    if(c == CONTROL_X)
    {
	return true;
    }
    else // added else because CenterLine could not handle inline without it
	return false;
}

int seeRealEditor::TermChar(char c)
{
    int rc = CheckTermChar(c, (const char)lastChar, done);
    if(rc)
	lastChar = rc;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
//
// seeRealAggrEditor class member functions
//
///////////////////////////////////////////////////////////////////////////////

seeRealAggrEditor::~seeRealAggrEditor() 
{
    delete subInfo; 
}

void seeRealAggrEditor::Handle(Event& e)
{
	if(e.eventType == DownEvent || e.eventType == KeyEvent)
//	if(e.eventType == 2 || e.eventType == 4)
	{
	    UnRead(e);
	    subInfo->SetValue('\001');
	    subject->SetValue(subInfo);  // don't cast this to (void*)
//		call notify for subject (which calls see update)
//	    StringEditor2::Handle(e);
	}
	else
	    StringEditor2::Handle(e);
/*
    if (e.target == this) {
    }
    else
	UnRead(e);
*/
}

void seeRealAggrEditor::NewEdit()
{
    Event e;
    e.target = nil;
    e.eventType = EnterEvent;
    StringEditor2::Handle(e);
}

int seeRealAggrEditor::TypeCheck()
{

#if EDITOR_TYPE_CHECKING

    const char *attrVal = text->Text();
    char *curAttr = (char *)attrVal;

    double tmpD = 0;
    char tmpstr[3];
    int numFound = 0;
    char prevCh = '\0';
    while(curAttr){
    
	tmpD = 0;
	numFound = sscanf((char *)text->Text(),"%lf%1s", &tmpD,
				tmpstr);
	numFound = sscanf(curAttr," %lf %1s", &tmpD, tmpstr);
	switch(numFound){
	  case 0:
		// 0 => invalid beginning;
		numFound = sscanf(curAttr," %1s", tmpstr);
		if(numFound && tmpstr[0] == ',')
		{ // found null int value 
		    curAttr = strchr(curAttr, ',');
		    curAttr++;
		    prevCh = ',';
		}
		else
		{
		    fprintf(stderr, "%c invalid real aggregate attr value\n", 
			RingBell);
		    fflush(stderr);
		    return 0;
		}
		break;
	  case 2:
	    // 2 => invalid ending
		if(tmpstr[0] == ','){
		    curAttr = strchr(curAttr, ',');
		    curAttr++;
		    prevCh = ',';
		}
		else{
		   fprintf(stderr, 
			   "%c invalid real aggregate attr value: '%c'\n",
			    RingBell, tmpstr[0]);
		    fflush(stderr);
		    return 0;
		}
		break;
	  case EOF:
	    // EOF => blank field
		if(prevCh == '\0' || prevCh == ',')
		    return SETYPE_REAL;
		else {
		  fprintf(stderr, 
			  "%c invalid real aggregate attr value: '%c'\n",
			   RingBell, prevCh);
		    fflush(stderr);
		    return 0;
		}
	  case 1:
	    // 1 => success
//		printf("edit results returned %s\n", attrVal);
		return SETYPE_REAL;
	}
    }
#else
    return SETYPE_REAL;
#endif
}

boolean seeRealAggrEditor::InsertValidChar(char c)
{
    switch (c) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'e':
	case 'E':
	case '.':
	case '-':
	case '+':
		InsertText(&c, 1);
		return true;
	case ' ':
	case ',':
//		if(left != 0){	// do not insert initial commas
		    InsertText(CommaSpace, 2);
		    return true;
//		} else 
//		    return false;
	case OpenParen:	// '('
		if(left == 0){
		    InsertText(&OpenParen, 1);
		    return true;
		} else 
		    return false;
	case CloseParen: // ')'
	{
		int lastCharIndex = text->Width();
		if(left == lastCharIndex){
		    InsertText(&CloseParen, 1);
		    return true;
		} else 
		    return false;
	}
	default:
		return false;
    }
}

void seeRealAggrEditor::SetSubject(int c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}

///////////////////////////////////////////////////////////////////////////////
// This checks for valid keystrokes that won't be entered into
// the text of the editor.  This function is defined for these reasons:
// 1) in order to add two keystroke commands you don't have to
//	redefine InsertValidChar() (can use the derived one)
// 2) logically groups the group of non-terminating characters in
//	multiple character cmds
// 3) keeps InsertValidChar() from beeping when you type
//	non-terminating chars in a 2 char cmd.
///////////////////////////////////////////////////////////////////////////////

boolean seeRealAggrEditor::CheckForCmdChars(char c)
{
//    if(c == CONTROL_X && lastChar != CONTROL_X)
    if(c == CONTROL_X)
    {
	return true;
    }
    else // added else because CenterLine could not handle inline without it
	return false;
}

int seeRealAggrEditor::TermChar(char c)
{
    int rc = CheckTermChar(c, (const char)lastChar, done);
    if(rc)
	lastChar = rc;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
//
//  seeEntityEditor member functions.
//
///////////////////////////////////////////////////////////////////////////////

seeEntityEditor::~seeEntityEditor()
{
    delete subInfo; 
}

void seeEntityEditor::NewEdit()
{
    Event e;
    e.target = nil;
    e.eventType = EnterEvent;
    StringEditor2::Handle(e);
}

void seeEntityEditor::Handle(Event& e)
{
	if(e.eventType == DownEvent || e.eventType == KeyEvent)
//	if(e.eventType == 2 || e.eventType == 4)
	{
	    UnRead(e);
	    subInfo->SetValue('\001');
	    subject->SetValue(subInfo);  // don't cast this to (void*)
//		call notify for subject (which calls see update)
//	    StringEditor2::Handle(e);
	}
	else
	    StringEditor2::Handle(e);
/*
    if (e.target == this) {
    }
    else
	UnRead(e);
*/
}

int seeEntityEditor::TypeCheck()
{
#if EDITOR_TYPE_CHECKING
    long int tmpI = 0;
    char tmpstr[3];
    int numFound = sscanf((char *)text->Text(),"#%ld%1s", &tmpI, 
				tmpstr);
    switch(numFound){
	case 0:
	    // 0 => invalid beginning;
		fprintf(stderr, "%c invalid entity attr value\n" , RingBell);
		fflush(stderr);
		return 0;
	case 2:
	    // 2 => invalid ending
		fprintf(stderr, "%c invalid entity attr value: '%c'\n",
			RingBell, tmpstr[0]);
		fflush(stderr);
		return 0;
	case 1:
	    // 1 => success;
//		printf("edit results returned %d\n", numFound);
//		printf("attr value: %ld\n", tmpI);
		return SETYPE_ENTITY;
	case EOF:
	    // EOF => blank field
//		printf("edit results returned %d\n", numFound);
//		printf("attr value: %ld\n", tmpI);
		return SETYPE_ENTITY;
    }
#else
    return SETYPE_ENTITY;
#endif
}

boolean seeEntityEditor::InsertValidChar(char c)
{
    switch (c) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		InsertText(&c, 1);
		return true;
	case ' ':
	case Pound:
		if(left == 0){
		    InsertText(&Pound, 1);
		    return true;
		} else 
		    return false;
	default:
		return false;
    }
}


void seeEntityEditor::SetSubject(int c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}

///////////////////////////////////////////////////////////////////////////////
// This checks for valid keystrokes that won't be entered into
// the text of the editor.  This function is defined for these reasons:
// 1) in order to add two keystroke commands you don't have to
//	redefine InsertValidChar() (can use the derived one)
// 2) logically groups the group of non-terminating characters in
//	multiple character cmds
// 3) keeps InsertValidChar() from beeping when you type
//	non-terminating chars in a 2 char cmd.
///////////////////////////////////////////////////////////////////////////////

boolean seeEntityEditor::CheckForCmdChars(char c)
{
//    if(c == CONTROL_X && lastChar != CONTROL_X)
    if(c == CONTROL_X)
    {
	return true;
    }
    else // added else because CenterLine could not handle inline without it
	return false;
}

int seeEntityEditor::TermChar(char c)
{
    int rc = CheckTermChar(c, (const char)lastChar, done);
    if(rc)
	lastChar = rc;
    return rc;
}

///////////////////////////////////////////////////////////////////////////////
//
//  seeEntAggrEditor member functions.
//
///////////////////////////////////////////////////////////////////////////////

seeEntAggrEditor::~seeEntAggrEditor()
{
    delete subInfo; 
}

void seeEntAggrEditor::Handle(Event& e)
{
	if(e.eventType == DownEvent || e.eventType == KeyEvent)
//	if(e.eventType == 2 || e.eventType == 4)
	{
	    UnRead(e);
	    subInfo->SetValue('\001');
	    subject->SetValue(subInfo);  // don't cast this to (void*)
//		call notify for subject (which calls see update)
//	    StringEditor2::Handle(e);
	}
	else
	    StringEditor2::Handle(e);
/*
    if (e.target == this) {
    }
    else
	UnRead(e);
*/
}

void seeEntAggrEditor::NewEdit()
{
    Event e;
    e.target = nil;
    e.eventType = EnterEvent;
    StringEditor2::Handle(e);
}

int seeEntAggrEditor::TypeCheck()
{
#if EDITOR_TYPE_CHECKING
    long int tmpI = 0;
    char tmpstr[3];
    int numFound = sscanf((char *)text->Text(),"#%ld%1s", &tmpI, 
				tmpstr);
    switch(numFound){
	case 0:
	    // 0 => invalid beginning;
		fprintf(stderr, "%c invalid entity aggregate attr value\n" , 
			RingBell);
		fflush(stderr);
		return 0;
	case 2:
	    // 2 => invalid ending
		fprintf(stderr, 
			"%c invalid entity aggregate attr value: '%c'\n",
			RingBell, tmpstr[0]);
		fflush(stderr);
		return 0;
	case 1:
	case EOF:
	    // 1 => success; EOF => blank field
//		printf("edit results returned %d\n", numFound);
//		printf("attr value: %ld\n", tmpI);
		return SETYPE_ENTITY_AGGR;
    }
#else
    return SETYPE_ENTITY_AGGR;
#endif
}

boolean seeEntAggrEditor::InsertValidChar(char c)
{
    switch (c) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		InsertText(&c, 1);
		return true;
	case OpenParen:	// '('
		if(left == 0){
		    InsertText(&OpenParen, 1);
		    return true;
		} else 
		    return false;
	case ' ':
	case '#':
		InsertText(&Pound, 1);
		return true;
	case ',':
		InsertText(CommaSpacePound, 3);
		return true;
	case CloseParen: // ')'
	{
		int lastCharIndex = text->Width();
		if(left == lastCharIndex){
		    InsertText(&CloseParen, 1);
		    return true;
		} else 
		    return false;
	}
	default:
		return false;
    }
}


void seeEntAggrEditor::SetSubject(int c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}

///////////////////////////////////////////////////////////////////////////////
// This checks for valid keystrokes that won't be entered into
// the text of the editor.  This function is defined for these reasons:
// 1) in order to add two keystroke commands you don't have to
//	redefine InsertValidChar() (can use the derived one)
// 2) logically groups the group of non-terminating characters in
//	multiple character cmds
// 3) keeps InsertValidChar() from beeping when you type
//	non-terminating chars in a 2 char cmd.
///////////////////////////////////////////////////////////////////////////////

boolean seeEntAggrEditor::CheckForCmdChars(char c)
{
//    if(c == CONTROL_X && lastChar != CONTROL_X)
    if(c == CONTROL_X)
    {
	return true;
    }
    else // added else because CenterLine could not handle inline without it
	return false;
}

int seeEntAggrEditor::TermChar(char c)
{
    int rc = CheckTermChar(c, (const char)lastChar, done);
    if(rc)
	lastChar = rc;
    return rc;
}
