/*
 * ListItemChooser - a StringChooser for choosing an item from a list.
 */

#ifndef listitemchooser_h
#define listitemchooser_h

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/dialog.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/scroller.h>
#include <mystrbrowser.h>

#include <IV-2_6/_enter.h>

#define CANCEL_VAL ('\007')

class UpMover;
class DownMover;
class Vscroller;

class ListItemChooser : public Dialog {
public:
    ListItemChooser(ButtonState* bs, 
        const char* boxtitle = "Please select one of the following:",
        int rows = 10, int cols = 24,
        const char* acceptLabel = " Choose ", Alignment = Center
    );
    ListItemChooser(const char* name, ButtonState* bs, 
        const char* boxtitle = "Please select one of the following:",
        int rows = 10, int cols = 24,
        const char* acceptLabel = " Choose ", Alignment = Center
    );
    virtual ~ListItemChooser();

    void SetTitle(const char*);

    virtual boolean Accept();	// loop until an item is chosen
    int Choice();		// return the index of the selected line
    char* String(int index);	// return the text for the line at index
    int Count() 		// return the num of entries in the browser
	{ return _browser->Count(); }
    void Select(int index);	// select one of the entries in the list
    void Browse();		// initiate the selecting process for the user

    void Insert(const char* line, int index);
    void Append(const char* line);
    void Remove(int index);
    MyStringBrowser* Browser();

protected:
    void Init (ButtonState* bs, const char* title, int r, int c, 
	       const char* acptlbl);
    Interactor* Interior();
    Interactor* AddScroller(Interactor*);

protected:
    MarginFrame* title;
    MyStringBrowser* _browser;
    PushButton *_cancelBut;
    PushButton *_acceptBut;

    UpMover   *_upMover;
    DownMover *_downMover;
    VScroller *_vScroller;
};

inline MyStringBrowser* ListItemChooser::Browser () { return _browser; }

	// return the index of the selected line
inline int ListItemChooser::Choice()
{   return _browser->Selection(); }

	// return the text for the line at index
inline char* ListItemChooser::String(int index)
{    return _browser->String(index); }

	// initiate the selecting process for the user
inline void ListItemChooser::Browse()
{    _browser->Browse(); }

	// select one of the entries in the list
inline void ListItemChooser::Select(int index)
{    _browser->Select(index); }

inline void ListItemChooser::Insert(const char* line, int index)
{    _browser->Insert(line, index); }

inline void ListItemChooser::Append(const char* line)
{    _browser->Append(line); }

inline void ListItemChooser::Remove(int index)
{    _browser->Remove(index); }

#include <IV-2_6/_leave.h>

#endif
