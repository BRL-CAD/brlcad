
// ~sauderd/src/iv/hfiles/streditor2.h 

/*
 * StringEditor2 - interactive editor for character strings
 *			this adds:
 *			 <contrl> K -- select rest of line
 *			 <meta/esc> f -- jump to beginning of next word
 *			 <meta/esc> b -- jump to beginning of previous word
 *			and fixes <cntrl> W -- select rest of word.
 *			I
 *			It also creates a generic HandleChar() that
 *			calls the following three virtual functions:
 *			int TypeCheck() -- Does type checking on the 
 *				StringEditor2's text and returns a
 *				constant representing the type or zero
 *				(error).  It is done after a terminating
 *				char is typed.
 *			void SetSubject(int c) -- Sets the member 
 *				variable 'subject' (a ButtonState) 
 *				when a terminating char is typed
 *				if TypeCheck() is successful.
 *			boolean CheckForCmdChar(char) -- Allows you to 
 *				check for valid keystrokes as part of 
 *				multi-character commands. This is a separate
 *				function for inheritance reasons (i.e. this 
 *				function can be defined for a subtype without 
 *				redefining InsertValidChar() which can be used
 *				to do limited type checking.)
 *			boolean InsertValidChar(char c) -- Defines and writes 
 *				all the characters accepted by the 
 *				object that are not defined as <cntrl> 
 *				char commands.  This is part of the default
 *				action of HandleChar().
 */

#ifndef streditor2_h
#define streditor2_h

#include <IV-2_6/_enter.h>

#include <string.h> // put here for inline function that uses strchr()
#include <ctype.h>
//#include <std.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/textdisplay.h>	// needed to define NoCaret
#include <IV-2_6/InterViews/streditor.h>

	// <cntrl> K 
#define SEDeleteToEndOfLine '\v'
#define ESCAPE_KEY '\033'
#define DEFAULT_WIDTH 15

	// return values for TypeCheck()
#define SETYPE_STRING 1
#define SETYPE_UNSIGNED_INTEGER 2
#define SETYPE_SIGNED_INTEGER 3
#define SETYPE_REAL 4
#define SETYPE_ENUM 5
#define SETYPE_OTHER 6
// make your own!! // #define SETYPE_GEEK 1001

class StringEditor2 : public StringEditor {
public:
    StringEditor2(ButtonState*, const char* sample, const char* done_ = SEDone,
		  int width = 0);
    StringEditor2(
        const char* name, ButtonState*, const char*, const char* = SEDone,
	int width = 0
    );
    virtual ~StringEditor2() {};

	// this needed to be a virtual function
    virtual void NewEdit() { StringEditor::Edit(); }
    void GetMark(int& l, int& r) { l = left; r = right; };
    virtual void Handle(Event&);
    virtual void Reconfig ();  // needed to redefine the way size is determined

	// object will become this width (* FontWidth("n")) at next Reconfig()
    void SetBoxWidth(int width);
	// returns type number if success otherwise returns zero.
    virtual int TypeCheck() { return SETYPE_STRING; };
    void RemoveCursor()    { display->CaretStyle(NoCaret); };
    virtual void SetSubject(int c)
	{ if (subject != nil)	{ subject->SetValue(c); } };
    char LastChar()		{ return lastChar; }

    boolean IsMapped();

protected:
    virtual int TermChar(char c) { return ((strchr(done, c) != nil) ? c : 0); }
    virtual boolean HandleChar(char);
    virtual boolean CheckForCmdChars(char c) { return false; }
    virtual boolean InsertValidChar(char c);	// accept all non <cntrl> chars

private:
    void Init(const char* samp, int width);

protected:
    char lastChar;	// last char typed... used to implment 2 keystroke cmds
    int boxWidth;	// Used to limit the size of the box when Reconfig()ing
			//  boxWidth != zero => size is based on 
			//		boxWidth * fontWidth("n")
			//  boxWidth == zero => size is based on 
			//  		initStrLen * fontWidth("n") 
    int initStrLen;	// original size of object ... set to
			// boxWidth if boxWidth > 0 else
			// strlen(constructor param 'sample') > 0 =>
			// 	initStrLen = strlen(sample)
			// strlen(constructor param 'sample') == 0 =>
			// 	initStrLen = DEFAULT_WIDTH
};

inline void StringEditor2::SetBoxWidth(int width) {
    boxWidth = width;
}

#include <IV-2_6/_leave.h>

#endif
