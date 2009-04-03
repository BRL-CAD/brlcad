
// ~sauderd/src/iv/hfiles/streditor2sub.h

/*
 * StringEditor2Sub -   Interactive editor for character strings based
 *			on StringEditor2.  This one is sociable with
 *			a supervisor.  In addition to supplying the 
 *			supervisor with the terminating char it also 
 *			returns a pointer to itself.
 *			It does this by redefining SetSubject() to 
 *			communicate with the supervisor supplying the 
 *			fields contained in class SubordinateInfo.
 */

#ifndef streditor2sub_h
#define streditor2sub_h

#include <InterViews/event.h>
#include <streditor2.h>
#include <subinfo.h>

#include <IV-2_6/_enter.h>

class StringEditor2Sub : public StringEditor2 {
public:
    StringEditor2Sub(ButtonState*, const char* sample, 
		const char* Done, int width = 0);
    StringEditor2Sub(
        const char* name, ButtonState*, const char*, const char*, int width = 0
    );
    ~StringEditor2Sub() { delete subInfo; };

    virtual void SetSubject(int c);

protected:
    SubordinateInfo *subInfo;

private:
    void Init();
};

inline void StringEditor2Sub::Init()
{
    subInfo = new SubordinateInfo(0, this);
}

inline StringEditor2Sub::StringEditor2Sub (
    ButtonState* s, const char* samp, const char* Done, int width
) : StringEditor2(s, samp, Done, width)
{
    Init();
}

inline StringEditor2Sub::StringEditor2Sub (
    const char* name, ButtonState* s, const char* samp, const char* Done,
    int width
) : StringEditor2(s, samp, Done, width)
{
    SetInstance(name);
    Init();
}

#include <IV-2_6/_leave.h>

#endif
