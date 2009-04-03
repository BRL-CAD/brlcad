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
 * Controls provide an interface to selecting and executing some action.
 */

#ifndef mycontrol_h
#define mycontrol_h

#include <InterViews/resource.h>
#include <IV-2_6/InterViews/scene.h>
#include <IV-2_6/InterViews/subject.h>

#include <InterViews/_enter.h>

class MyControl;
class MyControlState;

class MyControl : public MonoScene {
public:
    MyControl(Interactor*);
    MyControl(const char* name, Interactor*);
    ~MyControl();

    MyControlState* State() { return state_; }
    void SetState(MyControlState*);

    virtual void Handle(Event&);

    virtual void Enable(boolean);
    boolean Enabled();

    virtual void Select();	/* highlight, open, and grab */
    virtual void Unselect();	/* close, unhighlight */

    virtual void Do();		/* action for selection */

    MyControl* ParentControl();
    MyControl* RootControl();
protected:
    virtual void Down();	/* activate control */
    virtual void Enter();	/* select a control if active */
    virtual void Open();	/* open subviews, if any */
    virtual void Grab();	/* read input events */
    virtual void Skip();	/* ignore input until enters active control */
    virtual void Leave();	/* unselect if active */
    virtual void Close();	/* close subviews, if any */
    virtual void Up();		/* deactivate control, exec selection */

    virtual void Busy();
    virtual void Done();

    virtual boolean IsGrabbing(Interactor*);

    void Reparent(MyControl*, MyControl* parent);
private:
    MyControlState* state_;
    boolean enabled_;
    MyControl* parent_;

private:
    void Init(const char*, Interactor*);
};

inline boolean MyControl::Enabled() { return enabled_; }
inline MyControl* MyControl::ParentControl() { return parent_; }

/*
 * MyControlState is a subject that several controls share to exchange
 * common information.
 */

enum MyControlStatus { MyControlActive = 0x1 };

class MyControlState : virtual public Subject {
public:
    MyControlState(unsigned status = 0);
    ~MyControlState();

    boolean Active() { return Set(MyControlActive); }
    void Activate() { Set(MyControlActive, true); }
    virtual void Deactivate();

    MyControl* Selection() { return selection; }
    void Selection(MyControl* c) { selection = c; }
    virtual void NotifySelection(MyControl*);

    MyControl* Action() { return action; }
    void Action(MyControl* c) { action = c; }

    void Push(MyControlState*);
    void Pop();
    MyControlState* Next() { return next; }
    MyControlState* Prev() { return prev; }
protected:
    unsigned status;
    MyControl* selection;
    MyControl* action;
    MyControlState* next;
    MyControlState* prev;

    boolean Set(MyControlStatus s) { return (status & s) != 0; }
    void Set(MyControlStatus s, boolean b) {
	status = b ? (status | s) : (status & ~s);
    }
};

#include <InterViews/_leave.h>

#endif
