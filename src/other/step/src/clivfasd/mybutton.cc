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
 * Button management.
 */

#include <string.h>

#include <InterViews/bitmap.h>
#include <InterViews/cursor.h>
#include <InterViews/font.h>
#include <InterViews/pattern.h>

#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/sensor.h>
#include <IV-2_6/InterViews/shape.h>
#include <IV-2_6/InterViews/subject.h>

#include <OS/math.h>

#include <mybutton.h>

static Cursor* origCursor = 0;
static Cursor* handCursor = 0;

#include <InterViews/Bitmaps/radioBoth.bm>
#include <InterViews/Bitmaps/radioChosen.bm>
#include <InterViews/Bitmaps/radioHit.bm>
#include <InterViews/Bitmaps/radioMask.bm>
#include <InterViews/Bitmaps/radio.bm>

#include <InterViews/Bitmaps/hand.bm>
#include <InterViews/Bitmaps/handMask.bm>

#include <fasdBitmaps/m.bm>
#include <fasdBitmaps/mHit.bm>
#include <fasdBitmaps/v.bm>
#include <fasdBitmaps/vHit.bm>
#include <fasdBitmaps/mvmask.bm>

#include <fasdBitmaps/pin2mask.bm>   // mask
	// not hit, not pinned - hollow circle with tack
#include <fasdBitmaps/pinout2.bm>
	// hit & not pinned - thick hollow circle with tack
#include <fasdBitmaps/pinout2Hit.bm>
	// not hit, pinned - hollow circle with tack pushed in
#include <fasdBitmaps/pinin2.bm>
	// hit & pinned - thick hollow circle with tack pushed in
#include <fasdBitmaps/pinin2Hit.bm>

#include <IV-2_6/_enter.h>

static const int sep = 3;
static const int pad = 3;

inline int HalfRadioButtonSize (int h) { return Math::round(.4*h); }
inline int HalfCheckBoxSize (int h) { return Math::round(.4*h); }

/*
 * A mybutton has a ButtonState subject that it modifies when pressed.
 * Also, a mybutton may have associated buttons that are enabled/disabled
 * when it is chosen/unchosen.
 */

void MyButton::Init () {
    SetClassName("MyButton");
}

MyButton::MyButton (ButtonState* s, void* v) : Button(s, v) 
{
    Init();
}

MyButton::MyButton (const char* name, ButtonState* s, void* v) : Button(s, v) 
{
    SetInstance(name);
    Init();
}

/*
MyButton::MyButton (Painter* out, ButtonState* s, void* v) : Button(out, s, v) 
{
    Init();
}
*/

MyButton::~MyButton () {
}

void MyButton::Handle (register Event& e) {
    if(!origCursor)
	origCursor = GetCursor();
    if (handCursor == nil) {
	int handHotx = 7; // The Bitmap object seems to have changed how the 
	int handHoty = 1; // coords for hot spot is calculated. DAS
			  // Used to be x = 7, y = 14. i.e. 0,0 is bottom left.
			  // now 0,0 seems to be top left. bm is 16x16

        handCursor = new Cursor(
	    new Bitmap(
		       /* put the hot spot at the finger tip */
		hand_bits, hand_width, hand_height, handHotx, handHoty
//		hand_bits, hand_width, hand_height, hand_x_hot, hand_y_hot
	    ),
	    new Bitmap(hand_mask_bits, hand_mask_width, hand_mask_height),
	    output->GetFgColor(), output->GetBgColor()
	);
    }
    if (e.eventType == EnterEvent)
    {
	SetCursor(handCursor);
	this->Flush();
    }
//    else (e.eventType == LeaveEvent)
//	SetCursor(origCursor);
    if (e.eventType == DownEvent && e.target == this) {
	boolean inside = true;
	do {
	    if (enabled && e.target == this) {
		if (e.eventType == EnterEvent) {
		    SetCursor(handCursor);
		    inside = true;
		} else if (e.eventType == LeaveEvent) {
		    SetCursor(origCursor);
		    inside = false;
		}
		if (inside) {
		    if (!hit) {
			hit = true;
			Refresh();
		    }
		} else {
		    if (hit) {
			hit = false;
			Refresh();
		    }
		}
	    }
	    Read(e);
	} while (e.eventType != UpEvent);
	if (hit) {
	    hit = false;
	    Refresh();
	}
	if (enabled && inside) {
	    Press();
	}
    }
}

////////////////////////////////////////////////////////////////////////

void MyTextButton::Init (const char* str) {
    SetClassName("MyTextButton");
    if (str == nil) {
	text = nil;
    } else {
	text = new char[strlen(str) + 1];
	strcpy(text, str);
    }
    background = nil;
    grayout = nil;
}

MyTextButton::MyTextButton (const char* str, ButtonState* s, void* v) 
 : MyButton(s, v)
{
    Init(str);
}

MyTextButton::MyTextButton (
    const char* name, const char* str, ButtonState* s, void* v) 
 : MyButton(name, s, v)
{
    Init(str);
}

/*
MyTextButton::MyTextButton (
    const char* str, ButtonState* s, void* v, Painter* out) 
 : MyButton(out, s, v) 
{
     Init(str);
}
*/

void MyTextButton::Reconfig() {
    const char* a = GetAttribute("text");
    if (a != nil) {
	delete text;
	text = new char[strlen(a) + 1];
	strcpy(text, a);
    }
}

void MyTextButton::MakeBackground () {
    Unref(background);
    background = new Painter(output);
    background->Reference();
    background->SetColors(output->GetBgColor(), output->GetFgColor());

    static Pattern* gray_pat;
    if (gray_pat == nil) {
	gray_pat = new Pattern(Pattern::gray);
	gray_pat->Reference();
    }
    Unref(grayout);
    grayout = new Painter(background);
    grayout->Reference();
    grayout->SetPattern(gray_pat);
    grayout->FillBg(false);
}

void MyTextButton::MakeShape () {
    if (text != nil) {
	const Font* f = output->GetFont();
	shape->width += f->Width(text);
	shape->height += f->Height();
    }
    shape->Rigid();
}

MyTextButton::~MyTextButton() {
    delete text;
    Unref(background);
    Unref(grayout);
}

////////////////////////////////////////////////////////////////////////

void MyPushButton::Init () {
    SetClassName("MyPushButton");
}

MyPushButton::MyPushButton (
    const char* str, ButtonState* s, int v) : MyTextButton(str, s, (void*)v) 
{
    Init();
}

MyPushButton::MyPushButton (
    const char* str, ButtonState* s, void* v) : MyTextButton(str, s, v) 
{
    Init();
}

MyPushButton::MyPushButton (
    const char* name, const char* str, ButtonState* s, int v) 
 : MyTextButton(name, str, s, (void*)v) 
{
    Init();
}
    
MyPushButton::MyPushButton (
    const char* name, const char* str, ButtonState* s, void* v) 
 : MyTextButton(name, str, s, v) 
{
    Init();
}

/*
MyPushButton::MyPushButton (
    const char* str, ButtonState* s, int v, Painter* out) 
 : MyTextButton(str, s, (void*)v, out) 
{
    Init();
}

MyPushButton::MyPushButton (
    const char* str, ButtonState* s, void* v, Painter* out) 
 : MyTextButton(str, s, v, out) 
{
    Init();
}
*/

MyPushButton::~MyPushButton() { }

void MyPushButton::Reconfig () {
    MyTextButton::Reconfig();
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
	shape->width += output->GetFont()->Width("    ");
	shape->height += 2*pad;
    }
}

void MyPushButton::Redraw (IntCoord x1, IntCoord y1, 
			   IntCoord x2, IntCoord y2)
{
    output->ClearRect(canvas, x1, y1, x2, y2);
    Refresh();
}

void MyPushButton::Refresh () {
    register int r;
    IntCoord x[16], y[16];
    IntCoord tx, ty;

    r = Math::min(10*pixels, Math::min(xmax+1, ymax+1)/6);
    x[0] = 0; y[0] = r;
    x[1] = 0; y[1] = r + r;
    x[2] = 0; y[2] = ymax - r - r;
    x[3] = 0; y[3] = ymax - r;
    x[4] = r; y[4] = ymax;
    x[5] = r + r; y[5] = ymax;
    x[6] = xmax - r - r; y[6] = ymax;
    x[7] = xmax - r; y[7] = ymax;
    x[8] = xmax; y[8] = ymax - r;
    x[9] = xmax; y[9] = ymax - r - r;
    x[10] = xmax; y[10] = r + r;
    x[11] = xmax; y[11] = r;
    x[12] = xmax - r; y[12] = 0;
    x[13] = xmax - r - r; y[13] = 0;
    x[14] = r + r; y[14] = 0;
    x[15] = r; y[15] = 0;
    tx = (xmax - output->GetFont()->Width(text))/2;
    ty = pad;
    if (chosen || hit) {
	output->FillBSpline(canvas, x, y, 16);
	background->Text(canvas, text, tx, ty);
    } else {
	background->FillRect(canvas, 0, 0, xmax, ymax);
	output->ClosedBSpline(canvas, x, y, 16);
	output->Text(canvas, text, tx, ty);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
}

static Bitmap* myRadioMask;
static Bitmap* myRadioPlain;
static Bitmap* myRadioHit;
static Bitmap* myRadioChosen;
static Bitmap* myRadioBoth;

void MyRadioButton::Init () {
    SetClassName("MyRadioButton");
}

MyRadioButton::MyRadioButton (
    const char* str, ButtonState* s, int v) : MyTextButton(str, s, (void*)v) 
{
    Init();
}

MyRadioButton::MyRadioButton (
    const char* str, ButtonState* s, void* v) : MyTextButton(str, s, v) 
{
    Init();
}

MyRadioButton::MyRadioButton (
    const char* name, const char* str, ButtonState* s, int v) 
 : MyTextButton(name, str, s, (void*)v) 
{
    Init();
}

MyRadioButton::MyRadioButton (
    const char* name, const char* str, ButtonState* s, void* v) 
 : MyTextButton(name, str, s, v) 
{
    Init();
}

/*
MyRadioButton::MyRadioButton (
    const char* str, ButtonState* s, int v, Painter* out) 
 : MyTextButton(str, s, (void*)v, out) 
{
    Init();
}

MyRadioButton::MyRadioButton (
    const char* str, ButtonState* s, void* v, Painter* out) 
 : MyTextButton(str, s, v, out) 
{
    Init();
}
*/

MyRadioButton::~MyRadioButton() { }

void MyRadioButton::Reconfig () {
    MyTextButton::Reconfig();
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
	shape->width += shape->height + sep;
    }
    if (myRadioMask == nil) {
        myRadioMask = new Bitmap(
            radio_mask_bits, radio_mask_width, radio_mask_height
        );
	myRadioMask->Reference();
        myRadioPlain = new Bitmap(
            radio_plain_bits, radio_plain_width, radio_plain_height
        );
	myRadioPlain->Reference();
        myRadioHit = new Bitmap(
            radio_hit_bits, radio_hit_width, radio_hit_height
        );
	myRadioHit->Reference();
        myRadioChosen = new Bitmap(
            radio_chosen_bits, radio_chosen_width, radio_chosen_height
        );
	myRadioChosen->Reference();
        myRadioBoth = new Bitmap(
            radio_both_bits, radio_both_width, radio_both_height
        );
	myRadioBoth->Reference();
    }
}

void MyRadioButton::Redraw (IntCoord x1, IntCoord y1, 
			    IntCoord x2, IntCoord y2) {
    int h = output->GetFont()->Height();
    int r = radio_plain_width;
    output->ClearRect(canvas, x1, y1, x2, y2);
    Coord tx = r + sep;
    Coord ty = (ymax + 1 - h) / 2;
    output->Text(canvas, text, tx, ty);
    Refresh();
}

void MyRadioButton::Refresh () {
    IntCoord x = 0;
    IntCoord y = (ymax+1 - radio_plain_height)/2;
    if (!hit && !chosen) {
        output->Stencil(canvas, x, y, myRadioPlain, myRadioMask);
    } else if (hit && !chosen) {
        output->Stencil(canvas, x, y, myRadioHit, myRadioMask);
    } else if (!hit && chosen) {
        output->Stencil(canvas, x, y, myRadioChosen, myRadioMask);
    } else if (hit && chosen) {
        output->Stencil(canvas, x, y, myRadioBoth, myRadioMask);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
}

/*
static Bitmap* myCheckBoxMask;
static Bitmap* myCheckBoxPlain;
static Bitmap* myCheckBoxHit;
static Bitmap* myCheckBoxChosen;
static Bitmap* myCheckBoxBoth;
*/

void MyCheckBox::Init (void* v) {
    SetClassName("MyCheckBox");
    offvalue = v;
}

MyCheckBox::MyCheckBox (
    const char* str, ButtonState* s, int on, int off) 
 : MyTextButton(str, s, (void*)on) 
{
    Init((void*)off);
}

MyCheckBox::MyCheckBox (
    const char* str, ButtonState* s, void* on, void* off) 
 : MyTextButton(str, s, on) 
{
    Init(off);
}

MyCheckBox::MyCheckBox (
    const char* name, const char* str, ButtonState* s, int on, int off) 
 : MyTextButton(name, str, s, (void*)on) 
{
    Init((void*)off);
}

MyCheckBox::MyCheckBox (
    const char* name, const char* str, ButtonState* s, void* on, void* off) 
 : MyTextButton(name, str, s, on) 
{
    Init(off);
}

/*
MyCheckBox::MyCheckBox (
    const char* str, ButtonState* s, int on, int off, Painter* out) 
 : MyTextButton(str, s, (void*)on, out) 
{
    Init((void*)off);
}

MyCheckBox::MyCheckBox (
    const char* str, ButtonState* s, void* on, void* off, Painter* out) 
 : MyTextButton(str, s, on, out) 
{
    Init(off);
}
*/

MyCheckBox::~MyCheckBox() { }

void MyCheckBox::Reconfig () {
    MyTextButton::Reconfig();
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
	shape->width += shape->height + sep;
    }
    Update();
/*
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
	shape->width += shape->height + sep;
    }
    if (myCheckBoxMask == nil) {
        myCheckBoxMask = new Bitmap(radio_mask_bits, radio_mask_width, 
				    radio_mask_height);
	myCheckBoxMask->Reference();
        myCheckBoxPlain = new Bitmap(
            radio_plain_bits, radio_plain_width, radio_plain_height
        );
	myCheckBoxPlain->Reference();
        myCheckBoxHit = new Bitmap(
            radio_hit_bits, radio_hit_width, radio_hit_height
        );
	myCheckBoxHit->Reference();
        myCheckBoxChosen = new Bitmap(
            radio_chosen_bits, radio_chosen_width, radio_chosen_height
        );
	myCheckBoxChosen->Reference();
        myCheckBoxBoth = new Bitmap(
            radio_both_bits, radio_both_width, radio_both_height
        );
	myCheckBoxBoth->Reference();
    }
*/
}

void MyCheckBox::Press () {
    if (chosen) {
	subject->GetValue(value);
	subject->SetValue(offvalue);
    } else {
	subject->SetValue(value);
    }
}

void MyCheckBox::Update () {
    void* v;

    subject->GetValue(v);
    if (v != offvalue) {
	Choose();
	value = v;
    } else {
	UnChoose();
    }
}

void MyCheckBox::Redraw (IntCoord x1, IntCoord y1, IntCoord x2, IntCoord y2) {
    int h = output->GetFont()->Height();
    int t = HalfCheckBoxSize(h);
    output->ClearRect(canvas, x1, y1, x2, y2);
    IntCoord tx = 2*t + sep;
    IntCoord ty = (ymax + 1 - h) / 2;
    output->Text(canvas, text, tx, ty);
    Refresh();
/*
    int h = output->GetFont()->Height();
    int r = radio_plain_width;
    output->ClearRect(canvas, x1, y1, x2, y2);
    Coord tx = r + sep;
    Coord ty = (ymax + 1 - h) / 2;
    output->Text(canvas, text, tx, ty);
    Refresh();
*/
}

void MyCheckBox::Refresh () {
    int h = output->GetFont()->Height();
    int t = HalfCheckBoxSize(h);
    IntCoord cx = t;
    IntCoord cy = (ymax + 1)/2;
    IntCoord left = cx - t;
    IntCoord right = cx + t;
    IntCoord bottom = cy - t;
    IntCoord top = cy + t;
    output->Rect(canvas, left, bottom, right, top);
    background->FillRect(canvas, left+1, bottom+1, right-1, top-1);
    if (hit) {
	output->Rect(canvas, left+1, bottom+1, right-1, top-1);
    }
    if (chosen) {
	output->Line(canvas, left, bottom, right, top);
	output->Line(canvas, left, top, right, bottom);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
/*
    Coord x = 0;
    Coord y = (ymax+1 - radio_plain_height)/2;
    if (!hit && !chosen) {
        output->Stencil(canvas, x, y, myCheckBoxPlain, myCheckBoxMask);
    } else if (hit && !chosen) {
        output->Stencil(canvas, x, y, myCheckBoxHit, myCheckBoxMask);
    } else if (!hit && chosen) {
        output->Stencil(canvas, x, y, myCheckBoxChosen, myCheckBoxMask);
    } else if (hit && chosen) {
        output->Stencil(canvas, x, y, myCheckBoxBoth, myCheckBoxMask);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
*/
}
///////////////////////////////////////////////////////////////////////////////

static Bitmap* pin2mask;
static Bitmap* pinin2;
static Bitmap* pinin2Hit;
static Bitmap* pinout2;
static Bitmap* pinout2Hit;

void PinCheckBox::Init () {
    SetClassName("PinCheckBox");
}

PinCheckBox::PinCheckBox (
    const char* str, ButtonState* s, int on, int off) 
 : MyCheckBox(str, s, (void*)on, (void*)off) 
{
    Init ();
}

PinCheckBox::PinCheckBox (
    const char* str, ButtonState* s, void* on, void* off) 
 : MyCheckBox(str, s, on, off) 
{
    Init ();
}

PinCheckBox::PinCheckBox (
    const char* name, const char* str, ButtonState* s, int on, int off) 
 : MyCheckBox(name, str, s, (void*)on, (void*)off) 
{
    Init ();
}

PinCheckBox::PinCheckBox (
    const char* name, const char* str, ButtonState* s, void* on, void* off) 
 : MyCheckBox(name, str, s, on, off) 
{
    Init ();
}

/*
PinCheckBox::PinCheckBox (
    const char* str, ButtonState* s, int on, int off, Painter* out) 
 : MyCheckBox(str, s, (void*)on, (void*)off, out) 
{
    Init ();
}

PinCheckBox::PinCheckBox (
    const char* str, ButtonState* s, void* on, void* off, Painter* out) 
 : MyCheckBox(str, s, on, off, out) 
{
    Init ();
}
*/

PinCheckBox::~PinCheckBox() { }

void PinCheckBox::Reconfig () {
    MyTextButton::Reconfig();
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
//	shape->width += shape->height + sep;
	shape->width += pinin2_width + sep;
	shape->height = pinin2_height;
    }
    if (pin2mask == nil) {
        pin2mask = new Bitmap(pin2mask_bits, pin2mask_width, 
				    pin2mask_height);
	pin2mask->Reference();
        pinout2 = new Bitmap(
            pinout2_bits, pinout2_width, pinout2_height
        );
	pinout2->Reference();
        pinout2Hit = new Bitmap(
            pinout2Hit_bits, pinout2Hit_width, pinout2Hit_height
        );
	pinout2Hit->Reference();
        pinin2 = new Bitmap(
            pinin2_bits, pinin2_width, pinin2_height
        );
	pinin2->Reference();
        pinin2Hit = new Bitmap(
            pinin2Hit_bits, pinin2Hit_width, pinin2Hit_height
        );
	pinin2Hit->Reference();
    }
    MakeBackground();
    Update();
}

void PinCheckBox::Redraw (IntCoord x1, IntCoord y1, IntCoord x2, IntCoord y2) {
//    int h = output->GetFont()->Height();
    int h = pinout2_height;
    int r = pinout2_width;
    output->ClearRect(canvas, x1, y1, x2, y2);
    IntCoord tx = r + sep;
    IntCoord ty = (ymax + 1 - h) / 2;
    output->Text(canvas, text, tx, ty);
    Refresh();
}

void PinCheckBox::Refresh () {
    IntCoord x = 0;
    IntCoord y = (ymax+1 - pinout2_height)/2;
    if (!hit && !chosen) {
        output->Stencil(canvas, x, y, pinout2, pin2mask);
    } else if (hit && !chosen) {
        output->Stencil(canvas, x, y, pinin2Hit, pin2mask);
//        output->Stencil(canvas, x, y, pinout2Hit, pin2mask);
    } else if (!hit && chosen) {
        output->Stencil(canvas, x, y, pinin2, pin2mask);
    } else if (hit && chosen) {
        output->Stencil(canvas, x, y, pinout2Hit, pin2mask);
//        output->Stencil(canvas, x, y, pinin2Hit, pin2mask);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
}

///////////////////////////////////////////////////////////////////////////////

static Bitmap* MVCheckBoxMask;
static Bitmap* ViewCheckBox;
static Bitmap* ViewCheckBoxHit;
static Bitmap* ModifyCheckBox;
static Bitmap* ModifyCheckBoxHit;

void ModifyViewCheckBox::Init () {
    SetClassName("ModifyViewCheckBox");
}

ModifyViewCheckBox::ModifyViewCheckBox (
    const char* str, ButtonState* s, int on, int off) 
 : MyCheckBox(str, s, (void*)on, (void*)off) 
{
    Init ();
}

ModifyViewCheckBox::ModifyViewCheckBox (
    const char* str, ButtonState* s, void* on, void* off) 
 : MyCheckBox(str, s, on, off) 
{
    Init ();
}

ModifyViewCheckBox::ModifyViewCheckBox (
    const char* name, const char* str, ButtonState* s, int on, int off) 
 : MyCheckBox(name, str, s, (void*)on, (void*)off) 
{
    Init ();
}

ModifyViewCheckBox::ModifyViewCheckBox (
    const char* name, const char* str, ButtonState* s, void* on, void* off) 
 : MyCheckBox(name, str, s, on, off) 
{
    Init ();
}

/*
ModifyViewCheckBox::ModifyViewCheckBox (
    const char* str, ButtonState* s, int on, int off, Painter* out) 
 : MyCheckBox(str, s, (void*)on, (void*)off, out) 
{
    Init ();
}

ModifyViewCheckBox::ModifyViewCheckBox (
    const char* str, ButtonState* s, void* on, void* off, Painter* out) 
 : MyCheckBox(str, s, on, off, out) 
{
    Init ();
}
*/

ModifyViewCheckBox::~ModifyViewCheckBox() { }

void ModifyViewCheckBox::Reconfig () {
    MyTextButton::Reconfig();
    MakeBackground();
    if (shape->Undefined()) {
	MakeShape();
//	shape->width += shape->height + sep;
	shape->width += m_width + sep;
	shape->height = m_height;
    }
//    printf("shape: \nwidth: %d height: %d \n", shape->width, shape->height);
    
    if (MVCheckBoxMask == nil) {

        MVCheckBoxMask = new Bitmap(mvmask_bits, mvmask_width, mvmask_height);
	MVCheckBoxMask->Reference();
        ViewCheckBox = new Bitmap(v_bits, v_width, v_height);
	ViewCheckBox->Reference();
        ViewCheckBoxHit = new Bitmap(vHit_bits, vHit_width, vHit_height);
	ViewCheckBoxHit->Reference();
        ModifyCheckBox = new Bitmap(m_bits, m_width, m_height);
	ModifyCheckBox->Reference();
        ModifyCheckBoxHit = new Bitmap(mHit_bits, mHit_width, mHit_height);
	ModifyCheckBoxHit->Reference();
    }
    MakeBackground();
/*
    if (shape->Undefined()) {
	MakeShape();
	shape->width += shape->height + sep;
    }
*/
    Update();
}

void ModifyViewCheckBox::Redraw (IntCoord x1, IntCoord y1, 
				 IntCoord x2, IntCoord y2) {
//    int h = output->GetFont()->Height();
    int h = m_height;
    int r = m_width;
    output->ClearRect(canvas, x1, y1, x2, y2);
    IntCoord tx = r + sep;
    IntCoord ty = (ymax + 1 - h) / 2;
    output->Text(canvas, text, tx, ty);
    Refresh();
/*
    int h = output->GetFont()->Height();
    int t = HalfCheckBoxSize(h);
    output->ClearRect(canvas, x1, y1, x2, y2);
    Coord tx = 2*t + sep;
    Coord ty = (ymax + 1 - h) / 2;
    output->Text(canvas, text, tx, ty);
    Refresh();
*/
}

void ModifyViewCheckBox::Refresh () {
    IntCoord x = 0;
    IntCoord y = (ymax+1 - m_height)/2;
    if (!hit && !chosen) {
        output->Stencil(canvas, x, y, ViewCheckBox, MVCheckBoxMask);
    } else if (hit && !chosen) {
        output->Stencil(canvas, x, y, ViewCheckBoxHit, MVCheckBoxMask);
    } else if (!hit && chosen) {
        output->Stencil(canvas, x, y, ModifyCheckBox, MVCheckBoxMask);
    } else if (hit && chosen) {
        output->Stencil(canvas, x, y, ModifyCheckBoxHit, MVCheckBoxMask);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
/*
    int h = output->GetFont()->Height();
    int t = HalfCheckBoxSize(h);
    Coord cx = t;
    Coord cy = (ymax + 1)/2;
    Coord left = cx - t;
    Coord right = cx + t;
    Coord bottom = cy - t;
    Coord top = cy + t;
    output->Rect(canvas, left, bottom, right, top);
    background->FillRect(canvas, left+1, bottom+1, right-1, top-1);
    if (chosen) {
	output->Line(canvas, left, bottom, right, top);
	output->Line(canvas, left, top, right, bottom);
    }
    if (hit) {
	output->Rect(canvas, left+1, bottom+1, right-1, top-1);
    }
    if (!enabled) {
	grayout->FillRect(canvas, 0, 0, xmax, ymax);
    }
*/
}
