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
 * Implementation of control classes
 */

#include <mycontrol.h>
#include <InterViews/bitmap.h>
#include <InterViews/cursor.h>
#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/sensor.h>

#include <fasdBitmaps/menu16.bm>
#include <fasdBitmaps/menu16_mask.bm>
#include <fasdBitmaps/menu12.bm>
#include <fasdBitmaps/menu12_mask.bm>

static Cursor* menu16Cursor = 0;
static Cursor* menu12Cursor = 0;
static Cursor* origCursor = 0;

/** class MyControl **/

MyControl::MyControl(Interactor* i) { Init(nil, i); }
MyControl::MyControl(const char* name, Interactor* i) { Init(name, i); }

void MyControl::Init(const char* name, Interactor* i) {
    SetClassName("MyControl");
    if (name != nil) {
	SetInstance(name);
    }
    enabled_ = true;
    parent_ = nil;
    state_ = new MyControlState;
    state_->Attach(this);
    input = new Sensor;
    input->Catch(EnterEvent);
    input->Catch(LeaveEvent);
    input->Catch(DownEvent);
    input->Catch(UpEvent);
    if (i != nil) {
	Insert(i);
    }
}

MyControl::~MyControl () {
    state_->Detach(this);
}

void MyControl::Handle(Event& e) {
    switch (e.eventType) {
    case DownEvent:
	Down();
	break;
    case UpEvent:
	Up();
	break;
    case EnterEvent:
	Enter();
	break;
    case LeaveEvent:
	Leave();
	break;
    }
}


void MyControl::Enable(boolean b) {
    if (b != enabled_) {
	enabled_ = b;
    }
}

/*
 * Hitting a control is equivalent to making the state active and
 * then selecting it.
 */

void MyControl::Down() {
    if (Enabled() && !state_->Active()) {
	state_->Activate();
	state_->NotifySelection(this);
    }
}

void MyControl::Enter() {
    if(!origCursor)
	origCursor = GetCursor();
/*    if (menu16Cursor == nil) {
        Bitmap menu16(menu16_bits, menu16_width, menu16_height, 8, 8);
        Bitmap menu16mask(menu16_mask_bits, menu16_mask_width, 
			  menu16_mask_height);
        menu16Cursor = new Cursor(
            &menu16, &menu16mask, output->GetFgColor(), output->GetBgColor()
        );
    }
*/
    int handHotx = 6; // The Bitmap object seems to have changed how the 
    int handHoty = 1; // coords for hot spot is calculated. DAS
			// Used to be x = 6, y = 10. i.e. 0,0 is bottom left.
			// now 0,0 seems to be top left. bm is 12x12
    if (menu12Cursor == nil) {
        Bitmap *menu12 = new Bitmap(menu12_bits, menu12_width, menu12_height, 
				    handHotx, handHoty);
        Bitmap *menu12mask = new Bitmap(menu12_mask_bits, menu12_mask_width, 
					menu12_mask_height);
        menu12Cursor = new Cursor(
            menu12, menu12mask, output->GetFgColor(), output->GetBgColor()
        );
    }

//    SetCursor(menu16Cursor);
    SetCursor(menu12Cursor);
    if (Enabled() && state_->Active()) {
	state_->NotifySelection(this);
    }
}

void MyControl::Leave() {
    SetCursor(origCursor);
    if (Enabled() && state_->Active()) {
	state_->NotifySelection(nil);
    }
}

void MyControl::Select() {
    Highlight(true);
    Open();
    Grab();
}

void MyControl::Unselect() {
    Close();
    Highlight(false);
}

void MyControl::Grab() {
    Event e;
    do {
	Read(e);
	e.target->Handle(e);
	if (e.target == this && e.eventType == LeaveEvent) {
	    Skip();
	    break;
	}
    } while (state_->Active());
}

/*
 * Skip all input events until we see something for another open control.
 */

void MyControl::Skip() {
    Event e;
    for (;;) {
	Read(e);
	if (e.eventType == EnterEvent && IsGrabbing(e.target)) {
	    UnRead(e);
	    break;
	} else if (e.eventType == UpEvent) {
	    Up();
	    break;
	}
    }
}

/*
 * Check whether an interactor (usually an event target) is a grabbing
 * control.  Trivially, the current control is grabbing.  Any other controls
 * attached to the control's state are also grabbing.  Similarly, any controls
 * attached to other control's state up the stack from the current one
 * are also considered to be grabbing.
 */

boolean MyControl::IsGrabbing(Interactor* i) {
    if (i == this) {
	return true;
    }
    for (MyControlState* c = state_; c != nil; c = c->Next()) {
	if (c->IsView(i)) {
	    return true;
	}
    }
    for (c = state_->Prev(); c != nil; c = c->Prev()) {
	if (c->IsView(i)) {
	    return true;
	}
    }
    return false;
}

/*
 * On an up event, deactivate all the control states and then
 * call Do the current selection (if any).
 */

void MyControl::Up() {
    if (state_->Active()) {
	MyControl* target = state_->Selection();
	state_->Action(target);
	for (MyControlState* c = state_; c != nil; c = c->Prev()) {
	    c->Deactivate();
	}
	if (target != nil) {
	    Busy();
	    target->Do();
	    Done();
	}
    }
}

void MyControl::Open() { }
void MyControl::Close() { }
void MyControl::Do() { }
void MyControl::Busy() { }
void MyControl::Done() { }


MyControl* MyControl::RootControl() {
    MyControl* root = this;
    MyControl* Parent = ParentControl();

    while (Parent != nil) {
	root = Parent;
	Parent = Parent->ParentControl();
    }
    return root;
}

void MyControl::SetState(MyControlState* s) {
    state_->Detach(this);
    state_ = s;
    s->Attach(this);
}

void MyControl::Reparent(MyControl* ctrl, MyControl* Parent) {
    ctrl->parent_ = Parent;
}

/** class MyControlState **/

MyControlState::MyControlState(unsigned s) {
    status = s;
    selection = nil;
    action = nil;
    next = nil;
    prev = nil;
}

MyControlState::~MyControlState() { }

void MyControlState::NotifySelection(MyControl* c) {
    if (selection != c) {
	if (selection != nil) {
	    selection->Unselect();
	}
	selection = c;
	if (selection != nil) {
	    selection->Select();
	}
    }
}

void MyControlState::Push(MyControlState* s) {
    next = s;
    s->prev = this;
}

void MyControlState::Pop() {
    if (prev != nil) {
	prev->next = next;
	prev = nil;
    }
    Deactivate();
}

void MyControlState::Deactivate() {
    Set(MyControlActive, false);
    if (selection != nil) {
	selection->Unselect();
	selection = nil;
    }
}
