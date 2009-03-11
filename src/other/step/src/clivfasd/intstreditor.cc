
// ~sauderd/src/iv/cfiles/intstreditor.c

/*
 * IntStringEditor -    Interactive editor for character strings based on
 *			on StringEditor2.  This object accepts only integer
 *			characters and possibly an optional sign. This is
 *			done by redefining InsertValidChar(char c).  It 
 *			performs type checking when a terminating char is
 *			typed by redefining TypeCheck().
 *
 * RealStringEditor -   Interactive editor for character strings based on
 *			on StringEditor2.  This object accepts only chars
 *			that would be found in a real number 
 *			(i.e. 0-9, '+', '-', '.', 'e', and 'E').  This is
 *			done by redefining InsertValidChar(char c).
 *			It performs type checking by redefining TypeCheck().
 *			Type checking is done when a terminating char is 
 *			typed.  Valid real numbers are defined as following:
 *			optional sign ,
 *			zero or more decimal digits,
 *			optional decimal point,
 *			zero or more decimal digits,
 *			then possibly:
 *			the letter e or E which may be followed by
 *			an optional sign and then
 *			zero or more decimal digits.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <IV-2_6/InterViews/textbuffer.h>
#include <intstreditor.h>

const char RingBell = '\007';   // <CNTRL> G 

///////////////////////////////////////////////////////////////////////////////
//
//  IntStringEditor member functions.
//
///////////////////////////////////////////////////////////////////////////////

IntStringEditor::IntStringEditor (
    ButtonState* s, const char* samp, const char* Done, int width
) : StringEditor2(s, samp, Done, width)
{
    SetClassName("IntStringEditor");
    acceptSigns = true;
}

IntStringEditor::IntStringEditor (
    const char* name, ButtonState* s, const char* samp, const char* Done,
    int width
) : StringEditor2( s, samp, Done, width)
{
    SetInstance(name);
    SetClassName("IntStringEditor");
    acceptSigns = true;
}

int IntStringEditor::TypeCheck()
{
    long int tmpI = 0;
    char tmpstr[3];
    int numFound = sscanf((char *)text->Text(),"%ld%1s", &tmpI, 
				tmpstr);
    switch(numFound){
	case 0:
	    // 0 => bogus beginning;
		fprintf(stderr, "%c invalid int attr value\n" , RingBell);
		fflush(stderr);
		return 0;
	case 2:
	    // 2 => bogus ending
		fprintf(stderr, "%c invalid int attr value: '%c'\n" , RingBell,
			tmpstr[0]);
		fflush(stderr);
		return 0;
	case 1:
	case EOF:
	    // 1 => success; EOF => blank field
//		printf("edit results returned %d\n", numFound);
//		printf("attr value: %ld\n", tmpI);
//		printf("term char (hex): %x, 'this' address(hex): %x\n",
//		(int)subInfo->GetValue(), subInfo->GetSubordinate());
		if(acceptSigns)
		    return SETYPE_SIGNED_INTEGER;
		else
		    return SETYPE_UNSIGNED_INTEGER;
	default:
		return 0;
    }
}

boolean IntStringEditor::InsertValidChar(char c)
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
	case '+':
	case '-':
		if(acceptSigns){
		    InsertText(&c, 1);
		    return true;
		} else
		    return false;
	default:
		return false;
    }
}

/*
boolean IntStringEditor::HandleChar (char c) {
    if (strchr(done, c) != nil) {
        if (subject != nil) {
            subject->SetValue(c);
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

///////////////////////////////////////////////////////////////////////////////
//
//  RealStringEditor member functions.
//
///////////////////////////////////////////////////////////////////////////////


RealStringEditor::RealStringEditor (
    ButtonState* s, const char* samp, const char* Done, int width
) : StringEditor2(s, samp, Done, width)
{
    SetClassName("RealStringEditor");
}

RealStringEditor::RealStringEditor (
    const char* name, ButtonState* s, const char* samp, const char* Done,
    int width
) : StringEditor2( s, samp, Done, width)
{
    SetInstance(name);
    SetClassName("RealStringEditor");
}

int RealStringEditor::TypeCheck()
{
    double tmpD = 0;
    char tmpstr[3];

    int numFound = sscanf((char *)text->Text(),"%lf%1s", &tmpD,
				tmpstr);
    switch(numFound){
	case 0:
	    // 0 => bogus beginning;
		fprintf(stderr, "%c invalid real attr value\n" , RingBell);
		fflush(stderr);
		return 0;
	case 2:
	    // 2 => bogus ending
		fprintf(stderr, "%c invalid real attr value: '%c'\n" ,RingBell,
			tmpstr[0]);
		fflush(stderr);
		return 0;
	case 1:
	case EOF:
	    // 1 => success; EOF => blank field
//		printf("edit results returned %d\n", numFound);
//		printf("attr value: %e\n", tmpD);
//		printf("term char (hex): %x, 'this' address(hex): %x\n",
//		(int)subInfo->GetValue(), subInfo->GetSubordinate());
		return SETYPE_REAL;
	default:
		return 0;
    }
}

boolean RealStringEditor::InsertValidChar(char c)
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
	case '+':
	case '-':
		InsertText(&c, 1);
		return true;
	default:
		return false;
    }
}
