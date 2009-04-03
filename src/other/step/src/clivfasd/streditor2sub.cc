
// ~sauderd/src/iv/cfiles/streditor2sub.c

/*
 * StringEditor2Sub -   Interactive editor for character strings based
 *			on IntStringEditor.  This one is sociable with
 *			a supervisor.  In addition to supplying the 
 *			supervisor with the terminating char it also 
 *			returns a pointer to itself.
 *			It does this by redefining SetSubject() to 
 *			communicate with the supervisor supplying the 
 *			fields contained in class SubordinateInfo.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/textbuffer.h>

#include <streditor2sub.h>	// "~sauderd/src/iv/hfiles/streditor2sub.h"

static const int BUFFERSIZE = 1000;


void StringEditor2Sub::SetSubject(int c)
{
    if (subject != nil) {
	subInfo->SetValue(c);
	subject->SetValue(subInfo);  // don't cast this to (void*)
					//   it makes it not work.
    }
}

/*
 define this if you want the supervisor to be notified before this 
 StringEditor2Sub handles it's own event.  If you do define this you will 
 probably want to call StringEditor2::Handle() for this object when the 
 supervisor has done what it wanted to do before this object handled it's 
 event.  You will need to read the event if you don't call handle for this 
 object otherwise this function will cause an infinite loop (i.e. read the 
 event, unread the event, read the event, unread the event, etc)
*/
/*
void StringEditor2Sub::Handle(Event& e)
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
boolean StringEditor2Sub::HandleChar (char c) {
    if (strchr(done, c) != nil) {
        if (subject != nil) {
	    subInfo->SetValue(c);
//            subject->SetValue((void*)subInfo);
            subject->SetValue(subInfo);
printf("se value: %d,  this: %d\n", (int)subInfo->GetValue(), 
	subInfo->GetSubordinate());
        }
        return true;
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
//		printf("char: '%0x', '%d'\n", c, c);
//		fflush(stdout);
//		meta = false;
            }
            else if (!iscntrl(c)) {
                InsertText(&c, 1);
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
