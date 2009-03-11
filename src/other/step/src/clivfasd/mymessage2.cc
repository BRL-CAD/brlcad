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
 * MyMessage class implementation.
 *
 * adapted from Message by David Sauder
 */

#include <mymessage2.h>
#include <InterViews/font.h>
#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/scene.h>
#include <IV-2_6/InterViews/shape.h>
#include <stdlib.h>

extern "C" {
#include <string.h>
}

MyMessage2::MyMessage2(const char* msg, Alignment al, int pad_, 
			int hstretch, int vstretch) 
{
    Init(msg, al, pad_, hstretch, vstretch);
}

MyMessage2::MyMessage2(
    const char* name,
    const char* msg, Alignment al, int Pad, int hstretch, int vstretch) 
 : Interactor(name) 
{
    Init(msg, al, Pad, hstretch, vstretch);
}

void MyMessage2::Init (const char* t, Alignment a, int p, int hstr, int vstr) {
    SetClassName("MyMessage2");
    textLen = strlen(t) + 1;

    text = new char[textLen];
    strcpy(text, t);

    textReturned = new char[textLen];
    strcpy(textReturned, t);

    alignment = a;
    pad = p;
    shape->hstretch = hstr;
    shape->vstretch = vstr;
    highlighted = false;
}

MyMessage2::~MyMessage2() { delete text; }

void MyMessage2::Reconfig () {
    const char* a = GetAttribute("text");
    if (a != nil) {
	delete text;
	text = new char[strlen(a)+1];
	strcpy(text, a);
    }
    a = GetAttribute("padding");
    if (a != nil) {
	pad = atoi(a);
    }
    const Font* f = output->GetFont();
    shape->width = pad + f->Width(text) + pad;
    shape->height = pad + f->Height() + pad;
    shape->hshrink = pad + pad;
    shape->vshrink = pad + pad;
}

void MyMessage2::Realign (Alignment a) {
    alignment = a;
    Draw();
}

void MyMessage2::Redraw (IntCoord l, IntCoord b, IntCoord r, IntCoord t) {
    IntCoord x = 0, y = 0;
    Align(alignment, shape->width, shape->height, x, y);
    output->Clip(canvas, l, b, r, t);
    if (highlighted) {
	output->SetColors(output->GetBgColor(), output->GetFgColor());
    }
    output->ClearRect(canvas, l, b, r, t);
    output->Text(canvas, text, x + pad, y + pad);
    if (highlighted) {
	output->SetColors(output->GetBgColor(), output->GetFgColor());
    }
    output->NoClip();
}

void MyMessage2::Highlight (boolean b) {
    if (highlighted != b) {
	highlighted = b;
	Draw();
    }
}

void MyMessage2::NewMessage (char* t) {
    IntCoord x = 0, y = 0;


/*
    int len_t = strlen(t);
    int len_text = strlen(text);

    if(len_t > len_text){
	delete text;
	text = new char[len_t + 1];
	strcpy(text, t);
    } else {
	strcpy(text, t);
	for(i = len_t - 1; i < len_text; i++)
	    text[i] = ' ';
    }
*/
    int bigger = 0;
    if(strlen(t) > strlen(text)){
	delete text;
	textLen = strlen(t) + 1;
	text = new char[textLen];
	bigger = 1;
    }
    strncpy(text, t, textLen);
    text[textLen - 1] = '\0';

    strncpy(textReturned, t, textLen);
    textReturned[textLen - 1] = '\0';

    if(bigger){
	Reconfig();
    }
    output->Text(canvas, text, x + pad, y + pad);
    if(bigger){
	Draw();
	Parent()->Change();
    }
}

char * MyMessage2::Text () {
    return textReturned;
}
