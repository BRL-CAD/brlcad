/*
 * FileChooser implementation.
 */

#include <InterViews/event.h>

#include <IV-2_6/InterViews/adjuster.h>
#include <IV-2_6/InterViews/border.h>
#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/filechooser.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/scroller.h>
#include <IV-2_6/InterViews/sensor.h>

#include <string.h>
#include <listitemchooser.h>

#include <IV-2_6/_enter.h>

void ListItemChooser::Init (ButtonState* bs, const char* boxtitle, 
			    int r, int c, const char* acptLbl)
{
    input = new Sensor;
    input->Reference();
//    input->Catch(DownEvent);
    input->Catch(KeyEvent);

    state = bs;
    if (*boxtitle == '\0') {
        title = new MarginFrame(new VGlue(0, 0));
    } else {
        title = new MarginFrame(new Message(boxtitle));
    }
    _browser = new MyStringBrowser(bs, r, c);
    _cancelBut = new PushButton("Cancel", state, CANCEL_VAL);
    _acceptBut = new PushButton(acptLbl, state, '\r');
}

ListItemChooser::ListItemChooser (
    ButtonState* bs, const char* boxtitle, int r, int c, 
    const char* acceptLabel, Alignment a) : Dialog(bs, nil, a)
{
    Init(bs, boxtitle, r, c, acceptLabel);
    Scene::Insert(Interior());
}

ListItemChooser::ListItemChooser (
    const char* name, ButtonState* bs, const char* boxtitle, 
    int r, int c, const char* acceptLabel, Alignment a) 
	: Dialog(name, bs, nil, a)
{
    SetInstance(name);
    Init(bs, boxtitle, r, c, acceptLabel);
    Scene::Insert(Interior());
    
}

ListItemChooser::~ListItemChooser() 
{ 
    input->unref();
};

	// loop until an item is chosen
boolean ListItemChooser::Accept () {
    Event e;
    int v;

    state->SetValue(0);
    v = 0;
    do {
	Read(e);
	// this dialog box will hog probe input until a selection is made 
	// (either select or quit) so check to see if an event
	// happened that this dialog box cares about. i.e. did anything 
	// inside the dialog box receive an event.  I added the Movers and
	// the VScroller to this list since before they were being ignored.
	if( (e.target == _cancelBut) || 
	    (e.target == _acceptBut) ||
	    (e.target == _upMover)   ||
	    (e.target == _downMover) ||
	    (e.target == _vScroller)
	)
	    e.target->Handle(e);
	else if( (e.eventType == KeyEvent) || 
		 (e.target == _browser) )
	    _browser->Handle(e);
/*
	if(e.target == _browser)
	    _browser->Handle(e);
	else if(e.target == _cancelBut)
	   _cancelBut->Handle(e);
	else if(e.target == _acceptBut)
	    _acceptBut->Handle(e);
*/
	state->GetValue(v);
    } while (v == 0 && e.target != nil);
    return (v && v != CANCEL_VAL) || e.target == nil;
/*
    boolean accepted, dirSelected;
    do {
        accepted = StringChooser::Accept();
        dirSelected = Browser()->IsADirectory(Choice());
    } while (accepted && dirSelected);
    return accepted;
*/
}

static void ChangeMsg (const char* name, MarginFrame* frame) {
    Interactor* msg;

    if (*name == '\0') {
        msg = new VGlue(0, 0);
    } else {
        msg = new Message(name);
    }
    frame->Insert(msg);
    frame->Change(msg);
}

void ListItemChooser::SetTitle (const char* name) {
    ChangeMsg(name, title);
}

Interactor* ListItemChooser::AddScroller (Interactor* i) {
    return new HBox(
        new MarginFrame(i, 2),
        new VBorder,
        new VBox(
            new UpMover(i, 1),
            new HBorder,
            new VScroller(i),
            new HBorder,
            new DownMover(i, 1)
        )                
    );
}

Interactor* ListItemChooser::Interior () {
    const int space = round(.5*cm);
    HBox* titleblock = new HBox(title, new HGlue);

    _upMover = new UpMover(_browser, 1);
    _vScroller = new VScroller(_browser);
    _downMover = new DownMover(_browser, 1);

    return new MarginFrame(
        new VBox(
            titleblock,
            new VGlue(space, 0),
//            new Frame(AddScroller(_browser)),
            new Frame(
		      new HBox(
			       new MarginFrame(_browser, 2),
			       new VBorder,
			       new VBox(
					_upMover,
					new HBorder,
					_vScroller,
					new HBorder,
					_downMover
					)                
			       )
		      ),
            new VGlue(space, 0),
            new HBox(
//                new VGlue(space, 0),
                new HGlue,
		_cancelBut,
                new HGlue(space, 0),
		_acceptBut
            )
        ), space, space/2, 0
    );
}

#include <IV-2_6/_leave.h>
