
// ~sauderd/src/iv/hfiles/intstreditor2sub.h

/*
 * IntStringEditorSub - Interactive editor for character strings based
 *			on IntStringEditor.  This one is sociable with
 *			a supervisor.  In addition to supplying the 
 *			supervisor with the terminating char it also 
 *			returns a pointer to itself.
 *			It does this by redefining SetSubject() to 
 *			communicate with the supervisor supplying the 
 *			fields contained in class SubordinateInfo.
 *
 * RealStringEditorSub - Same as above except for real numbers and based 
 *			on RealStringEditor.
 */

#include <ctype.h>
#include <stdio.h>
#include <stream.h>
#include <string.h>

#include <intstreditorsub.h>

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/textbuffer.h>

const char RingBell = '\007';   // <CNTRL> G 

IntStringEditorSub::~IntStringEditorSub()
{
    delete subInfo; 
}

void IntStringEditorSub::SetSubject(int c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}

RealStringEditorSub::~RealStringEditorSub() 
{
}

void RealStringEditorSub::SetSubject(int c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}
///////////////////////////////////////////////////////////////////////////////
//
//  IntStringEditorSub member functions.
//
///////////////////////////////////////////////////////////////////////////////

/*
 define this if you want the supervisor to be notified before this 
 IntStringEditorSub handles it's own event.  If you do define this you will 
 probably want to call StringEditor2::Handle() for this object when the 
 supervisor has done what it wanted to do before this object handled it's 
 event.  You will need to read the event if you don't call handle for this 
 object otherwise this function will cause an infinite loop (i.e. read the 
 event, unread the event, read the event, unread the event, etc)
*/
/*
void IntStringEditorSub::Handle(Event& e)
{
//    if(e.eventType == 2 || e.eventType == 4)
    if(e.eventType == DownEvent || e.eventType == KeyEvent)
    {
	UnRead(e);
	subInfo->SetValue(' ');
	subject->SetValue(subInfo);  // don't cast this to (void*)
//	call notify for subject (which calls see update)
    }
    else
	StringEditor2::Handle(e);
}
*/
/*
 define this if you want the supervisor to be notified before this 
 RealStringEditorSub handles it's own event.  If you do define this you will 
 probably want to call StringEditor2::Handle() for this object when the 
 supervisor has done what it wanted to do before this object handled it's 
 event.  You will need to read the event if you don't call handle for this 
 object otherwise this function will cause an infinite loop (i.e. read the 
 event, unread the event, read the event, unread the event, etc)
*/
/*
void RealStringEditorSub::Handle(Event& e)
{
//    if(e.eventType == 2 || e.eventType == 4)
    if(e.eventType == DownEvent || e.eventType == KeyEvent)
    {
	UnRead(e);
	subInfo->SetValue(' ');
	subject->SetValue(subInfo);  // don't cast this to (void*)
//	call notify for subject (which calls see update)
	StringEditor2::Handle(e);
    }
    else
	StringEditor2::Handle(e);
}
*/
/*
IntStringEditorSub::IntStringEditorSub (
    ButtonState* s, const char* samp, const char* done, int width
) : (s, samp, done, width)
{
    Init();
}

IntStringEditorSub::IntStringEditorSub (
    const char* name, ButtonState* s, const char* samp, const char* done,
    int width
) : ( s, samp, done, width)
{
    SetInstance(name);
    Init();
}

void IntStringEditorSub::Init()
{
    SetClassName("IntStringEditorSub");
    subInfo = new SubordinateInfo(0, this);
}

void IntStringEditorSub::SetSubject(char c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}
*/
/*
boolean IntStringEditorSub::HandleChar (char c) {
    if (strchr(done, c) != nil) {
        if (subject != nil) {
	    subInfo->SetValue(c);
            subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
	    long int tmpI = 0;
	    char tmpstr[3];
	    int numFound = sscanf((char *)text->Text(),"%ld%1s", &tmpI, 
					tmpstr);
	    if(numFound == 0 || numFound == 2){
		    // 0 => bogus beginning; 2 => bogus ending
		fprintf(stderr, "%c bogus int attr value\n" , RingBell);
		fflush(stderr);
	    }
            else if(numFound == 1 || numFound == EOF){
		    // 1 => success; EOF => blank field
		printf("edit results returned %d\n", numFound);
		printf("attr value: %ld\n", tmpI);
		printf("term char (hex): %x, 'this' address(hex): %x\n",
			(int)subInfo->GetValue(), subInfo->GetSubordinate());
		return true;
	    }
        }
    } else {
        switch (c) {
          case SEBeginningOfLine:
            Select(text->BeginningOfLine(left));
            break;
          case SEEndOfLine:
            Select(text->EndOfLine(right));
            break;
          case SESelectAll:
            Select(text->BeginningOfText(), text->EndOfText());
            break;
//          case SESelectWord:
//            Select(
//                text->BeginningOfWord(text->PreviousCharacter(left)), right
//            );
//            break;
          case SESelectWord:
	    if(text->IsEndOfWord(right))
               Select(
                left, text->EndOfWord(text->BeginningOfNextWord(right))
	       );
	    else
               Select(
                left, text->EndOfWord(right)
	       );
            break;
          case SEDeleteToEndOfLine:
            Select(left, text->EndOfLine(right));
            break;

          case SEPreviousCharacter:
            Select(text->PreviousCharacter(left));
            break;
          case SENextCharacter:
            Select(text->NextCharacter(right));
            break;
          case SEDeleteNextCharacter:
            if (left == right) {
                right = text->NextCharacter(right);
            }
            InsertText("", 0);
            break;
          case SEDeletePreviousCharacter:
          case SEDeletePreviousCharacterAlt:
            if (left == right) {
                left = text->PreviousCharacter(left);
            }
            InsertText("", 0);
            break;
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
		break;
	  case '+':
	  case '-':
		if (acceptSigns)
		    InsertText(&c, 1);
		else {
		    fprintf(stdout, "%c" , RingBell);
		    fflush(stdout);
		}
		break;
          case ESCAPE_KEY:
	    meta = true;
            break;
          default:
	    if(meta){
		switch(c){
		    case 'f':
			Select(text->BeginningOfNextWord(right));
			break;
		    case 'b':
			if(text->IsBeginningOfWord(left))
			    Select(text->BeginningOfWord(
					text->PreviousCharacter(left)));
			else
			    Select(text->BeginningOfWord(left));
			break;
		}
            }
            else {
		fprintf(stdout, "%c" , RingBell);
		fflush(stdout);
	    }
            break;
        }
	if(meta)
	    if(c != ESCAPE_KEY)
		meta = false;
        return false;
    }
}
*/

///////////////////////////////////////////////////////////////////////////////
//
//  RealStringEditorSub member functions.
//
///////////////////////////////////////////////////////////////////////////////
/*
RealStringEditorSub::RealStringEditorSub (
    ButtonState* s, const char* samp, const char* done, int width
) : (s, samp, done, width)
{
    Init();
}

RealStringEditorSub::RealStringEditorSub (
    const char* name, ButtonState* s, const char* samp, const char* done,
    int width
) : ( s, samp, done, width)
{
    SetInstance(name);
    Init();
}

void RealStringEditorSub::Init()
{
    SetClassName("RealStringEditorSub");
    subInfo = new SubordinateInfo(0, this);
printf("se this: %d\n", this);
}

void RealStringEditorSub::SetSubject(char c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}
*/
/*
boolean RealStringEditorSub::HandleChar (char c) {
    if (strchr(done, c) != nil) {
        if (subject != nil) {
	    subInfo->SetValue(c);
            subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
	    double tmpD = 0;
	    char tmpstr[3];
	    int numFound = sscanf((char *)text->Text(),"%lf%1s", &tmpD,
					tmpstr);
	    if(numFound == 0 || numFound == 2){
		    // 0 => bogus beginning; 2 => bogus ending
		fprintf(stderr, "%c bogus real attr value\n" , RingBell);
		fflush(stderr);
	    }
            else if(numFound == 1 || numFound == EOF){
		    // 1 => success; EOF => blank field
		printf("edit results returned %d\n", numFound);
		printf("attr value: %e\n", tmpD);
		printf("term char (hex): %x, this address(hex): %x\n",
			(int)subInfo->GetValue(), subInfo->GetSubordinate());
		return true;
	    }
        }
    } else {
        switch (c) {
          case SEBeginningOfLine:
            Select(text->BeginningOfLine(left));
            break;
          case SEEndOfLine:
            Select(text->EndOfLine(right));
            break;
          case SESelectAll:
            Select(text->BeginningOfText(), text->EndOfText());
            break;
//          case SESelectWord:
//            Select(
//                text->BeginningOfWord(text->PreviousCharacter(left)), right
//            );
//            break;
          case SESelectWord:
	    if(text->IsEndOfWord(right))
               Select(
                left, text->EndOfWord(text->BeginningOfNextWord(right))
	       );
	    else
               Select(
                left, text->EndOfWord(right)
	       );
            break;
          case SEDeleteToEndOfLine:
            Select(left, text->EndOfLine(right));
            break;

          case SEPreviousCharacter:
            Select(text->PreviousCharacter(left));
            break;
          case SENextCharacter:
            Select(text->NextCharacter(right));
            break;
          case SEDeleteNextCharacter:
            if (left == right) {
                right = text->NextCharacter(right);
            }
            InsertText("", 0);
            break;
          case SEDeletePreviousCharacter:
          case SEDeletePreviousCharacterAlt:
            if (left == right) {
                left = text->PreviousCharacter(left);
            }
            InsertText("", 0);
            break;
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
	  case '+':
	  case '-':
            if (!iscntrl(c)) {
                InsertText(&c, 1);
            }
            break;
          case ESCAPE_KEY:
	    meta = true;
            break;
          default:
	    if(meta){
		switch(c){
		    case 'f':
			Select(text->BeginningOfNextWord(right));
			break;
		    case 'b':
			if(text->IsBeginningOfWord(left))
			    Select(text->BeginningOfWord(
					text->PreviousCharacter(left)));
			else
			    Select(text->BeginningOfWord(left));
			break;
		}
            }
            else {
		fprintf(stdout, "%c" , RingBell);
		fflush(stdout);
	    }
            break;
        }
	if(meta)
	    if(c != ESCAPE_KEY)
		meta = false;
        return false;
    }
}
*/
