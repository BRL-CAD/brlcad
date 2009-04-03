
// ~sauderd/src/iv/hfiles/intstreditor.h

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

#ifndef intstreditor_h
#define intstreditor_h

#include <streditor2.h>

#include <IV-2_6/_enter.h>

///////////////////////////////////////////////////////////////////////////////
//
// IntStringEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class IntStringEditor : public StringEditor2 {
public:
    IntStringEditor(ButtonState*, const char* sample, 
			const char* Done = SEDone, int width = 0);
    IntStringEditor(
        const char* name, ButtonState*, const char*, const char* = SEDone,
	int width = 0
    );
    virtual ~IntStringEditor() {};

	// decide if signs are accepted and if they pass type checking.
    void AcceptSigns(boolean boolvar = true) { acceptSigns = boolvar; };

	// returns SETYPE_UNSIGNED_INTEGER or SETYPE_SIGNED_INTEGER if success
	// otherwise returns zero.
    virtual int TypeCheck();

    virtual void SetSubject(int c)
	{ if (subject != nil) { subject->SetValue(c); } };

protected:
    virtual boolean InsertValidChar(char c);
    boolean acceptSigns;
private:
};

///////////////////////////////////////////////////////////////////////////////
//
// RealStringEditor class definition
//
///////////////////////////////////////////////////////////////////////////////

class RealStringEditor : public StringEditor2 {
public:
    RealStringEditor(ButtonState*, const char* sample, 
			const char* Done = SEDone, int width = 0);
    RealStringEditor(
        const char* name, ButtonState*, const char*, const char* = SEDone,
	int width = 0
    );
    virtual ~RealStringEditor() {};

	// returns SETYPE_REAL if success otherwise returns zero.
    virtual int TypeCheck();

    virtual void SetSubject(int c)
	{ if (subject != nil) { subject->SetValue(c); } };

protected:
    virtual boolean InsertValidChar(char c);
private:
};

#include <IV-2_6/_leave.h>

#endif
