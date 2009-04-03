/*
 * Copyright (c) 1987, 1988, 1989 Stanford University
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Stanford not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Stanford makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * STANFORD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * MyStringBrowser - a Mac minifinder-like object for perusing and
 * choosing from list of strings.  I rebound the key bindings.  I also
 * wanted to add a function ReplaceText() so I could replace the text
 * on an existing line.  However, in order to do this I needed to use
 * the private member variable 'display'; so I had to copy all of
 * StringBrowser since I couldn't use it's private variable 'display'.
 */

#ifndef mystrbrowser_h
#define mystrbrowser_h

#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/textdisplay.h>
#include <IV-2_6/InterViews/textstyle.h>

#include <IV-2_6/_enter.h>

static const char* mySBDone = "\r\t\007\033";

/* these are new key bindings for MyStringBrowser */
static const char mySBFirstString = '\001';	// <ctrl> a
static const char mySBLastString = '\005';	// <ctrl> e
//static const char mySBSelectAll = '\023';	// <ctrl> s
static const char mySBSelectAll = '\027';	// <ctrl> w
static const char mySBUnselectAll = '\177';	// <delete>
static const char mySBUnselectAllAlt = '\010';	// <ctrl> h
static const char mySBSelectPreviousString = '\020';  // <ctrl> p
static const char mySBSelectNextString = '\016';	// <ctrl> n
static const char mySBSelectTopString = '<';
static const char mySBSelectBottomString = '>';
static const char mySBScrollUpb = '\002';	// <ctrl> b
static const char mySBScrollUpk = '\v';		// <ctrl> k
static const char mySBScrollDownf = '\006';	// <ctrl> f
static const char mySBScrollDownj = '\n';	// <ctrl> j
static const char mySBPageDownSp = ' ';
static const char mySBPageDown = '\026';		// <ctrl> v
//static const char mySBPageUp = 'v';
static const char mySBHalfPageDown = '\004';	// <ctrl> d
static const char mySBHalfPageUp = '\025';	// <ctrl> u

class ButtonState;
class TextDisplay;

class MyStringBrowser : public Interactor {
public:
    MyStringBrowser(
        ButtonState*, int rows, int cols, 
        boolean uniqueSel = true, int highlight = Reversed,
        const char* done = mySBDone
    );
    MyStringBrowser(
        const char* name, ButtonState*, int, int,
        boolean = true, int = Reversed, const char* = mySBDone
    );
    virtual ~MyStringBrowser();

    void Browse();
    virtual void Insert(const char*, int index);
    virtual void Append(const char*);
    virtual void Remove(int index);
    virtual void ReplaceText(const char* s, int index);

    int SearchForward(char *substr, int startIndex, int ignoreCase = 1);
    int SearchBackward(char *substr, int startIndex, int ignoreCase = 1);
    void Bold(int index);

    int Index(const char*);
    char* String(int);
    int Count();
    void Clear();

    void Select(int index);
    void SelectAll();
    void Unselect(int index);
    void UnselectAll();
    int Selection(int selindex = 0);
    int SelectionIndex(int index);
    int Selections();
    boolean Selected(int index);
    void ScrollTo(int index);

    virtual void Handle(Event&);
    virtual void Adjust(Perspective&);
protected:
    void Select(int dot, int mark);
    void Unselect(int dot, int mark);
    void ScrollBy(int, int);
    void ScrollBy(int lines);
    void ScrollTo(int, int);
    void ScrollToView(IntCoord, IntCoord);
    void GrabScroll(Event&);
    void RateScroll(Event&);

    int Locate(IntCoord, IntCoord);
    boolean DoubleClicked(Event&);

    virtual boolean HandleChar(char);
	// used to implement extra char bindings in subclasses
    virtual boolean HandleCharExtra(char c) { return false; }
    virtual boolean LeftButtonDown(Event&);

    virtual void Reconfig();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
    virtual void Resize();
protected:
    int rows;
    int columns;
    boolean uniqueSel;
    boolean singleClick;
    int clickDelay;
    int highlight;
    ButtonState* subject;
    const char* done;

// used to be private when this was a StringBrowser
    char** strbuf;
    int strbufsize;
    int strcount;
    char** selbuf;
    int selbufsize;
    int selcount;

    TextDisplay* display;
    int lineheight;
    unsigned long lasttime;
    IntCoord lastx, lasty;
    int lastdot, lastmark;
    boolean firstResize;
private:
    void Init(ButtonState*, int, int, boolean, int, const char*);
    void InitTextDisplay();
    void InitPerspective(boolean scroll_to_top);

    void UpdateSelection(int dot, int mark, int Style);
    void UpdateWidth();
    void Note(Event&);

    boolean HandleDownEvent(Event&);
    boolean HandleKeyEvent(Event&);
private:

};

inline int MyStringBrowser::Count () { return strcount; }
inline int MyStringBrowser::Selections () { return selcount; }
inline boolean MyStringBrowser::Selected (int i) 
					{return SelectionIndex(i) >= 0;}

#include <IV-2_6/_leave.h>

#endif
