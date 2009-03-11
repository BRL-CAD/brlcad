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
 * A button is a view of some value that is normally set when
 * the button is pressed.
 */

#ifndef mybutton_h
#define mybutton_h

#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/subject.h>
#include <IV-2_6/InterViews/button.h>

#include <IV-2_6/_enter.h>

class MyButton : public Button {
public:
    virtual void Handle(Event&);
protected:
    MyButton(ButtonState*, void*);
    MyButton(const char*, ButtonState*, void*);
//    MyButton(Painter*, ButtonState*, void*);
    virtual ~MyButton();

private:
    void Init();
};

class MyTextButton : public MyButton {
protected:
    char* text;
    Painter* background;
    Painter* grayout;		/* for disabled buttons */

    MyTextButton(const char*, const char*, ButtonState*, void*);
    MyTextButton(const char*, ButtonState*, void*);
//    MyTextButton(const char*, ButtonState*, void*, Painter*);
    virtual ~MyTextButton();

    virtual void Reconfig();

    void MakeBackground();
    void MakeShape();
private:
    void Init(const char*);
};

class MyPushButton : public MyTextButton {
public:
    MyPushButton(const char*, ButtonState*, int);
    MyPushButton(const char*, ButtonState*, void*);
    MyPushButton(const char*, const char*, ButtonState*, int);
    MyPushButton(const char*, const char*, ButtonState*, void*);
//    MyPushButton(const char*, ButtonState*, int, Painter*);
//    MyPushButton(const char*, ButtonState*, void*, Painter*);
    virtual ~MyPushButton();

    virtual void Refresh();
protected:
    virtual void Reconfig();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
private:
    void Init();
};

class MyRadioButton : public MyTextButton {
public:
    MyRadioButton(const char*, ButtonState*, int);
    MyRadioButton(const char*, ButtonState*, void*);
    MyRadioButton(const char*, const char*, ButtonState*, int);
    MyRadioButton(const char*, const char*, ButtonState*, void*);
//    MyRadioButton(const char*, ButtonState*, int, Painter*);
//    MyRadioButton(const char*, ButtonState*, void*, Painter*);
    virtual ~MyRadioButton();

    virtual void Refresh();
protected:
    virtual void Reconfig();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
private:
    void Init();
};

class MyCheckBox : public MyTextButton {
public:
    MyCheckBox(const char*, ButtonState*, int, int);
    MyCheckBox(const char*, ButtonState*, void*, void*);
    MyCheckBox(const char*, const char*, ButtonState*, int, int);
    MyCheckBox(const char*, const char*, ButtonState*, void*, void*);
//    MyCheckBox(const char*, ButtonState*, int, int, Painter*);
//    MyCheckBox(const char*, ButtonState*, void*, void*, Painter*);
    virtual ~MyCheckBox();

    virtual void Press();
    virtual void Refresh();
    virtual void Update();
protected:
    virtual void Reconfig();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
private:
    void* offvalue;

    void Init(void*);
};

class PinCheckBox : public MyCheckBox {
public:
    PinCheckBox(const char*, ButtonState*, int, int);
    PinCheckBox(const char*, ButtonState*, void*, void*);
    PinCheckBox(const char*, const char*, ButtonState*, int, int);
    PinCheckBox(const char*, const char*, ButtonState*, void*, void*);
//    PinCheckBox(const char*, ButtonState*, int, int, Painter*);
//    PinCheckBox(const char*, ButtonState*, void*, void*, Painter*);
    virtual ~PinCheckBox();

    virtual void Refresh();
protected:
    virtual void Reconfig();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
private:

    void Init();
};

class ModifyViewCheckBox : public MyCheckBox {
public:
    ModifyViewCheckBox(const char*, ButtonState*, int, int);
    ModifyViewCheckBox(const char*, ButtonState*, void*, void*);
    ModifyViewCheckBox(const char*, const char*, ButtonState*, int, int);
    ModifyViewCheckBox(const char*, const char*, ButtonState*, void*, void*);
//    ModifyViewCheckBox(const char*, ButtonState*, int, int, Painter*);
//    ModifyViewCheckBox(const char*, ButtonState*, void*, void*, Painter*);
    virtual ~ModifyViewCheckBox();

    virtual void Refresh();
protected:
    virtual void Reconfig();
    virtual void Redraw(IntCoord, IntCoord, IntCoord, IntCoord);
private:

    void Init();
};

#include <IV-2_6/_leave.h>

#endif
