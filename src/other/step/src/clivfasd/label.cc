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
 * myLabel class implementation.
 */

#define iv2_6_compatible

#include <label.h>

#include <InterViews/font.h>

#include <IV-2_6/InterViews/painter.h>
#include <IV-2_6/InterViews/shape.h>

#include <stdlib.h>
#include <string.h>

void myLabel::Init (Alignment a, int width, int p, 
			int hstr, int vstr) {
    SetClassName("myLabel");
    alignment = a;
    textWidth = width;
    pad = p;
    shape->hstretch = hstr;
    shape->vstretch = vstr;
    highlighted = false;
}

myLabel::myLabel(const char* msg, Alignment al, int width, int pad_, 
		int hstr, int vstr) {
    text = msg;
    Init(al, width, pad_, hstr, vstr);
}

myLabel::myLabel(char* msg, Alignment al, int width, int pad_, 
		int hstr, int vstr) {
    text = new char[strlen(msg) + 1];
    strncpy((char *)text, msg, strlen(msg) + 1);
    Init(al, width, pad_, hstr, vstr);
}

myLabel::myLabel(
    const char* name,
    const char* msg, Alignment al, int width, int pad_, int hstr, int vstr
) : Interactor(name) {
    text = msg;
    Init(al, width, pad_, hstr, vstr);
}

myLabel::myLabel(
    const char* name,
    char* msg, Alignment al, int width, int pad_, int hstr, int vstr
) : Interactor(name) {
    text = new char[strlen(msg) + 1];
    strncpy((char *)text, msg, strlen(msg) + 1);
    Init(al, width, pad_, hstr, vstr);
}

void myLabel::Reconfig () {
    const char* a = GetAttribute("text");
    if (a != nil) {
	text = a;
    }
    a = GetAttribute("padding");
    if (a != nil) {
	pad = atoi(a);
    }
    const Font* f = output->GetFont();
    if(textWidth)
	shape->width = pad + f->Width("n")*textWidth + pad;
    else
	shape->width = pad + f->Width(text) + pad;
    shape->height = pad + f->Height() + pad;
    shape->hshrink = pad + pad;
    shape->vshrink = pad + pad;
}

void myLabel::Realign (Alignment a) {
    alignment = a;
    Draw();
}

void myLabel::Redraw (IntCoord l, IntCoord b, IntCoord r, IntCoord t) {
    IntCoord x = 0, y = 0;
    Align(alignment, shape->width, shape->height, x, y);
    output->Clip(canvas, l, b, r, t);
    if (highlighted) {
	output->SetColors(output->GetBgColor(), output->GetFgColor());
    }
    output->ClearRect(canvas, l, b, r, t);
    const Font *f = output->GetFont();
    if(textWidth - strlen(text) > 0)
    {
	if(alignment == Right){
	    output->Text(canvas, text, 
		 x + ((textWidth - strlen(text)) * f->Width("n")) + pad, 
		 y + pad);
	}
	else if(alignment == Center){
	    output->Text(canvas, text, 
		 x + ((textWidth - strlen(text)) * f->Width("n"))/2 + pad, 
		 y + pad);
	}
	else
	    output->Text(canvas, text, x + pad, y + pad);
    }
    else
	output->Text(canvas, text, x + pad, y + pad);
    if (highlighted) {
	output->SetColors(output->GetBgColor(), output->GetFgColor());
    }
    output->NoClip();
}

void myLabel::Highlight (boolean b) {
    if (highlighted != b) {
	highlighted = b;
	Draw();
    }
}
