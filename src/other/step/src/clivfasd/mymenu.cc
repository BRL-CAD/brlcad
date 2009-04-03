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
 * Implementation of common menus.
 */

#include <InterViews/event.h>
#include <InterViews/pattern.h>
#include <InterViews/window.h>

#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>
//#include <IV-2_6/InterViews/menu.h>
#include <IV-2_6/InterViews/message.h>
#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/shape.h>
#include <IV-2_6/InterViews/world.h>
#include <OS/math.h>
#include <stdlib.h>

#include <mymenu.h>

#include <IV-2_6/_enter.h>

/** class MyMenuItem **/

MyMenuItem::MyMenuItem(Interactor* i) : MyControl(i) 
{
    Init(); 
}

MyMenuItem::MyMenuItem(const char* name, Interactor* i) : MyControl(name, i) 
{ 
    Init(); 
}

MyMenuItem::MyMenuItem(
    const char* str, Alignment al) 
 : MyControl(new Message(str, al, 2, hfil, 0)) 
// : MyControl(str, new Message(str, al, 2, hfil, 0)) 
{
    Init();
}

MyMenuItem::MyMenuItem(
    const char* name,
    const char* str, Alignment al) 
 : MyControl(name, new Message(str, al, 2, hfil, 0)) 
{
    Init();
}

MyMenuItem::~MyMenuItem() { }

void MyMenuItem::Init() {
    SetClassName("MyMenuItem");
}

void MyMenuItem::Busy() {
    RootControl()->Highlight(true);
}

void MyMenuItem::Done() {
    RootControl()->Highlight(false);
}

/** class MyMenuShadow **/

class MyMenuShadow : public MonoScene {
public:
    static Pattern* halftone;
    int depth_;

    MyMenuShadow(Interactor*, int depth = 4);
    virtual ~MyMenuShadow();

    virtual void Reconfig();
    virtual void Resize();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
};

Pattern* MyMenuShadow::halftone;

MyMenuShadow::MyMenuShadow(Interactor* i, int d) {
    SetClassName("MyMenuShadow");
    depth_ = d;
    if (halftone == nil) {
	halftone = new Pattern(Pattern::gray);
	halftone->ref();
    }
    if (i != nil) {
	Insert(new Frame(i));
    }
}

MyMenuShadow::~MyMenuShadow() { }

void MyMenuShadow::Reconfig() {
    MonoScene::Reconfig();
    const char* d = GetAttribute("depth");
    if (d != nil) {
	depth_ = atoi(d);
    }
    shape->width += depth_;
    shape->height += depth_;
}

void MyMenuShadow::Resize() {
//    if (component != nil) {
    if (interior() != nil) {
//	Place(component, 0, depth_, xmax - depth_, ymax);
	Place(interior(), 0, depth_, xmax - depth_, ymax);
    }
}

void MyMenuShadow::Redraw(IntCoord x1, IntCoord y1, IntCoord x2, IntCoord y2) {
    if (x2 >= depth_ && y1 <= ymax - depth_) {
	const Pattern* p = output->GetPattern();
	Resource::ref(p);
	boolean b = output->BgFilled();
	output->SetPattern(halftone);
	output->FillBg(false);
	IntCoord left = Math::max(x1, depth_);
	IntCoord top = Math::min(y2, ymax - depth_);
	output->FillRect(canvas, left, y1, x2, top);
	output->FillBg(b);
	output->SetPattern(p);
	Resource::unref(p);
    }
}

/** class MyMenu **/

MyMenu::MyMenu(Interactor* i) : MyControl(i) 
{
    Init(); 
}

MyMenu::MyMenu(const char* name, Interactor* i) : MyControl(name, i) 
{
    Init(); 
}

void MyMenu::Init() {
    SetClassName("MyMenu");
    state_ = new MyControlState;
    state_->Reference();
    scene_ = new VBox;
    body_ = new MyMenuShadow(scene_);
    depth_ = 3;
    align_ = BottomLeft;
    world_ = nil;
    rel_x_ = 0;
    rel_y_ = 0;
    ins_x_ = 0;
    ins_y_ = 0;
}

MyMenu::~MyMenu() {
    delete body_;
    Unref(state_);
}

void MyMenu::SetBody(Interactor* i) { body_ = i; }
void MyMenu::SetAlign(Alignment a) { align_ = a; }
void MyMenu::SetDepth(int d) { depth_ = d; }

void MyMenu::SetBodyState(MyControlState* s) {
    Unref(state_);
    state_ = s;
    s->Reference();
}

void MyMenu::SetScene(Scene* s) {
    delete body_;
    scene_ = s;
    body_ = nil;
    for (Scene* p = scene_; p != nil; p = p->Parent()) {
	body_ = p;
    }
/*
    if (scene_ != nil) {
	delete scene_;
    }
    scene_ = s;
    body_ = scene_->Root();
*/
}

void MyMenu::Include(MyControl* c) {
/*
    scene_->Insert(c);
    if (scene_ == nil) {
	scene_ = new VBox;
	body_ = new MyMenuShadow(scene_);
    }
*/
    scene_->Insert(c);
    c->SetState(GetBodyState());
    Reparent(c, this);
}

void MyMenu::Reconfig() {
    MyControl::Reconfig();
    world_ = GetWorld();
    Setup();
}

void MyMenu::Setup() {
    body_->Config(world_);
    Shape* s = body_->GetShape();
    rel_x_ = s->width / 2;
    rel_y_ = s->height / 2;
}

void MyMenu::Popup(Event& e) {
    if (Enabled()) {
	World* w;
	IntCoord wx, wy;
	e.GetAbsolute(w, wx, wy);
	if (w != world_) {
	    world_ = w;
	    Setup();
	}
	InsertBody(wx - rel_x_, wy - rel_y_);
	State()->Selection(this);
    }
}

void MyMenu::Leave() { }

void MyMenu::Open() {
    IntCoord x, y;
    Align(align_, 0, 0, x, y);
    GetRelative(x, y);
    InsertBody(x, y - body_->GetShape()->height);
}

void MyMenu::InsertBody(IntCoord x, IntCoord y) {
    ins_x_ = x;
    ins_y_ = y;
    world_->InsertPopup(body_, x, y);
    State()->Push(state_);
    state_->Activate();
    world_->Flush();
}

void MyMenu::Close() {
    if (body_->GetCanvas() != nil) {
	MyControl* action = state_->Action();
	state_->Action(nil);
	if (action != nil) {
	    action->Align(Center, 0, 0, rel_x_, rel_y_);
	    action->GetRelative(rel_x_, rel_y_, body_);
	}
    }
    state_->Pop();
    world_->Remove(body_);
    world_->Flush();
}

/** class MyMenuBar **/

MyMenuBar::MyMenuBar() { Init(); }
MyMenuBar::MyMenuBar(const char* name) { SetInstance(name); Init(); }

void MyMenuBar::Init() {
    SetClassName("MyMenuBar");
    state_ = new MyControlState;
    state_->Reference();
}

MyMenuBar::~MyMenuBar() {
    Unref(state_);
}

void MyMenuBar::Include(MyControl* c) {
    Insert(c);
    c->SetState(state_);
}

/** class MyPulldownMenu **/

MyPulldownMenu::MyPulldownMenu(Interactor* i) : MyMenu(i) 
{
    Init(); 
}

MyPulldownMenu::MyPulldownMenu(
    const char* name, Interactor* i) : MyMenu(name, i) 
{
    Init();
}

MyPulldownMenu::MyPulldownMenu(
    const char* str) : MyMenu(str, new Message(str, Center, 2)) 
{
    Init();
}

MyPulldownMenu::MyPulldownMenu(
    const char* name, const char* str) 
 : MyMenu(name, new Message(str, Center, 2)) 
{
    Init();
}

MyPulldownMenu::~MyPulldownMenu() { }

void MyPulldownMenu::Init() {
    SetClassName("MyPulldownMenu");
    SetAlign(BottomLeft);
}

/** class MyPullrightMenu **/

MyPullrightMenu::MyPullrightMenu(Interactor* i) : MyMenu(i) 
{
    Init(); 
}

MyPullrightMenu::MyPullrightMenu(
    const char* name, Interactor* i) : MyMenu(name, i) 
{
    Init();
}

MyPullrightMenu::MyPullrightMenu(
    const char* str) : MyMenu(str, new Message(str, Left, 2)) 
{
    Init();
}

MyPullrightMenu::MyPullrightMenu(
    const char* name, const char* str) 
 : MyMenu(name, new Message(str, Left, 2)) 
{
    Init();
}

MyPullrightMenu::~MyPullrightMenu() { }

void MyPullrightMenu::Init() {
    SetClassName("MyPullrightMenu");
    SetAlign(TopRight);
}

/** class PopupMenu **/

MyPopupMenu::MyPopupMenu() : MyMenu((Interactor*)nil) 
{
    Init(); 
}

MyPopupMenu::MyPopupMenu(const char* name) : MyMenu(name, (Interactor*)nil) {
    Init();
}

MyPopupMenu::~MyPopupMenu() { }

void MyPopupMenu::Init() {
    SetClassName("MyPopupMenu");
    SetState(new MyControlState);
    SetAlign(Center);
}
