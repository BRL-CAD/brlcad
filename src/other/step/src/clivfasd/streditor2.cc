
/*
 * StringEditor2 - interactive editor for character strings
 *			this adds <contrl> K and fixes <cntrl> W
 */

#include <InterViews/bitmap.h>
#include <InterViews/canvas.h>
#include <InterViews/cursor.h>
#include <InterViews/font.h>

// in order to include xcanvas.h you will need to have a -I option 
// for /usr/local/include because <X11/Xlib.h> from /usr/local/include/
// gets included from InterViews/include/IV-X11/Xlib.h
#include <IV-X11/xcanvas.h>
#include <InterViews/window.h>
// xcanvas.h and window.h were included so I could write the 
// function IsMapped()

#include <InterViews/Bitmaps/hand.bm>
#include <InterViews/Bitmaps/handMask.bm>
#include <InterViews/Bitmaps/lfast.bm>
#include <InterViews/Bitmaps/lfastMask.bm>
#include <InterViews/Bitmaps/rfast.bm>
#include <InterViews/Bitmaps/rfastMask.bm>

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/sensor.h>
#include <IV-2_6/InterViews/shape.h>
#include <IV-2_6/InterViews/streditor.h>
#include <IV-2_6/InterViews/textbuffer.h>
#include <IV-2_6/InterViews/textdisplay.h>
#include <IV-2_6/InterViews/world.h>
#include <OS/math.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <streditor2.h>

#include <IV-2_6/_enter.h>

static const int BUFFERSIZE = 1000;

const char RingBell = '\007';   // <CNTRL> G

void 
StringEditor2::Init(const char* samp, int width)
{
    SetClassName("StringEditor2");
/*	// I think this is commented because there is no painter (output) yet
    Font* f = output->GetFont();
    shape->hunits = f->Width("n");
    shape->vunits = f->Height();
    shape->Rect(shape->hunits*strlen(samp), shape->vunits);

    shape->Rigid();
*/
    lastChar = 0;
    boxWidth = width;
    if(boxWidth)
	initStrLen = boxWidth;
    else
	initStrLen = strlen(samp);
    if(!initStrLen)
	initStrLen = DEFAULT_WIDTH;
}

StringEditor2::StringEditor2 (
    ButtonState* s, const char* samp, const char* done_, int width
) : StringEditor(s, samp, done_)
{
    Init(samp, width);
}

StringEditor2::StringEditor2 (
    const char* name, ButtonState* s, const char* samp, const char* done_,
    int width
) : StringEditor(s, samp, done_)
{
    SetInstance(name);
    Init(samp, width);
}

boolean StringEditor2::HandleChar (char c) {
    int rc;
    if (rc = TermChar(c)) {
	if(TypeCheck()){
	    SetSubject(rc);
	    lastChar = rc;
	    return true;
	}
	else{
	    fprintf(stderr, "%c" , RingBell);
	    fflush(stderr);
	    lastChar = c;
	    return false;
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
          case ESCAPE_KEY:
	    lastChar = ESCAPE_KEY;

            break;
          default:
	    if(lastChar == ESCAPE_KEY){
		// if last char was <esc> next char is ignored unless it's:
		switch(c){
		    case 'f':	// jump cursor to beginning of next word
			Select(text->BeginningOfNextWord(right));
			break;
		    case 'b':	// jump cursor to beginning of previous word
			if(text->IsBeginningOfWord(left))
			    Select(text->BeginningOfWord(
					text->PreviousCharacter(left)));
			else
			    Select(text->BeginningOfWord(left));
			break;
		}
//		printf("char: '%0x', '%d'\n", c, c);
//		fflush(stdout);
            }
	    else if(CheckForCmdChars(c)){
		;
	    }
            else if (!InsertValidChar(c)) {
		fprintf(stdout, "%c" , RingBell);
		fflush(stdout);
            }
            break;
        }
	lastChar = c;
        return false;
    }
}

static Cursor* handCursor;
static Cursor* leftCursor;
static Cursor* rightCursor;

void StringEditor2::Reconfig () {
    if (handCursor == nil) {
        handCursor = new Cursor(
	    new Bitmap(
		hand_bits, hand_width, hand_height, hand_x_hot, hand_y_hot
	    ),
	    new Bitmap(hand_mask_bits, hand_mask_width, hand_mask_height),
	    output->GetFgColor(), output->GetBgColor()
	);

        leftCursor = new Cursor(
	    new Bitmap(
		lfast_bits, lfast_width, lfast_height, lfast_x_hot, lfast_y_hot
	    ),
	    new Bitmap(lfast_mask_bits, lfast_mask_width, lfast_mask_height),
	    output->GetFgColor(), output->GetBgColor()
        );

        rightCursor = new Cursor(
	    new Bitmap(
		rfast_bits, rfast_width, rfast_height, rfast_x_hot, rfast_y_hot
	    ),
	    new Bitmap(rfast_mask_bits, rfast_mask_width, rfast_mask_height),
            output->GetFgColor(), output->GetBgColor()
        );
    }

    const Font* f = output->GetFont();

    shape->hunits = f->Width("n");
    shape->vunits = f->Height();
    if(boxWidth > 0)	// resize to setting of boxwidth
	shape->Rect(f->Width("n")*boxWidth, f->Height());
    else		// or width of original size
	shape->Rect(f->Width("n")*initStrLen, f->Height());
    shape->Rigid();
    display->LineHeight(f->Height());
}

void StringEditor2::Handle (Event& e) {
    boolean Done = false;
    World* World = GetWorld();
    display->Draw(output, canvas);
    display->CaretStyle(BarCaret);
    do {
	if(e.meta)
	    lastChar = ESCAPE_KEY;
        switch (e.eventType) {
          case KeyEvent:
            if (e.len != 0) {
                Done = HandleChar(e.keystring[0]);
            }
            break;
          case DownEvent:
            if (e.target == this) {
                int origin = display->Left(0, 0);
                int width = display->Width();
                if (e.button == LEFTMOUSE) {
                    int start = display->LineIndex(0, e.x);
                    do {
                        if (e.x < 0) {
                            origin = Math::min(0, origin - e.x);
                        } else if (e.x > xmax) {
                            origin = Math::max(
					   xmax - width, origin - (e.x - xmax)
			    );
                        }
                        display->Scroll(0, origin, ymax);
                        DoSelect(start, display->LineIndex(0, e.x));
                        Poll(e);
                    } while (e.leftmouse);
                } else if (e.button == MIDDLEMOUSE) {
                    Cursor* origCursor = GetCursor();
                    SetCursor(handCursor);
                    int x = e.x;
                    do {
                        origin += e.x - x;
                        origin = Math::min(
					0, max(min(0, xmax - width), origin)
					);
                        display->Scroll(0, origin, ymax);
                        x = e.x;
                        Poll(e);
                    } while (e.middlemouse);
                    SetCursor(origCursor);
                } else if (e.button == RIGHTMOUSE) {
                    Cursor* origCursor = GetCursor();
                    int x = e.x;
                    do {
                        origin += x - e.x;
                        origin = Math::min(
			    0, Math::max(Math::min(0, xmax - width), origin)
					   );
                        display->Scroll(0, origin, ymax);
                        if (e.x - x < 0) {
                            SetCursor(leftCursor);
                        } else {
                            SetCursor(rightCursor);
                        }
                        Poll(e);
                    } while (e.rightmouse);
                    SetCursor(origCursor);
                }
            } else {
                UnRead(e);
                Done = true;
            }
            break;
        }
        if (!Done) {
            Read(e);
        }
    } while (!Done);
    int ismapped = 0;
    ismapped = IsMapped();
    if(ismapped)
	display->CaretStyle(NoCaret);
}

// boolean i.e. true = 1 and false = 0
boolean StringEditor2::IsMapped()
{
    return (canvas != nil && canvas->status() == Canvas::mapped);
/*
    if(GetTopLevelWindow())
    {
	// this doesn't work... seems to always return false DAS
	int bool = GetTopLevelWindow()->is_mapped();
//	int bool canvas->rep()->window_->is_mapped();
	return bool;
    }
    else
	return false;
*/
/* // this doesn't work
    if(canvas)
    {
	int bool = canvas->window()->is_mapped();
//	int bool canvas->rep()->window_->is_mapped();
	return bool;
    }
    else
	return false;
*/
}

// this used to be inline but CenterLine CC won't expand it as inline because
// a statement follows the first return statment.
	// accept all non <cntrl>
boolean StringEditor2::InsertValidChar(char c)
{
    if (!iscntrl(c))
    {
	InsertText(&c, 1);
	return true;
    }
    return false;
}

#include <IV-2_6/_leave.h>
