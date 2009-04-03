
/* ivfasd/listmgrcore.c */

/*
 * ListMgrCore - an object that let's you manage a list.  Meaning that you
 * can mark an entry on the list for:
 *    1) v(iew) or m(odify)
 *    2) r(eplicate) or d(elete)
 *    3) p(rint)
 *    4) c(hoose)
 *    5) h(ide)
 *    6) s(ave complete)
 *    7) i(save incomplete)
 */

#ifndef listmgrcore_h
#define listmgrcore_h

#include <stdlib.h>
#include <string.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/interactor.h>

#include <mystrbrowser.h>

#include <IV-2_6/_enter.h>

//typedef void (*ListMgrCore::lmFunc)();

///////////////////////////////////////////////////////////////////////////////
//
// lmCommand class
//
///////////////////////////////////////////////////////////////////////////////

class lmCommand {
public:
    lmCommand(char ch, int col) { cmdChar = ch; cmdCol = col; }
    ~lmCommand() {};

    char cmdChar;
    int  cmdCol;
};

///////////////////////////////////////////////////////////////////////////////
//
// ListMgrCore class
//
///////////////////////////////////////////////////////////////////////////////

class ListMgrCore : public MyStringBrowser {
public:
    ListMgrCore(ButtonState* bs, int Rows, int Cols, 
		lmCommand *CmdArray, int arraySize,
		const char* Done = mySBDone);
    ListMgrCore (const char* name, ButtonState* bs, int Rows, int Cols, 
		lmCommand *CmdArray, int arraySize, 
		const char* Done = mySBDone);
    virtual ~ListMgrCore() { };

    virtual void Insert(const char*, int index);
    virtual void Append(const char*);
    virtual void ReplaceText(const char* s, int index);
    virtual char* String(int index);	// returns the data portion of the line
			// returns the command portion of the line
    virtual char* CmdString(int index, char *str, int strSize);
    virtual int InCmdString(char ch, int index);

    virtual void DoCommand(lmCommand lm, int index) ;
    virtual void WriteChar(char c, int col, int index);
    virtual int MaxIndex() { return strcount - 1; }
    virtual int Count() { return strcount; }

protected:
	// used to implement extra char bindings in subclasses
    virtual boolean HandleCharExtra(char);
    void DoNothing() { printf("did nothing\n"); };

protected:
	// the number of columns in Cmd portion of the display
    int opColumns;
	// a string that has all the keystrokes accepted as commands;
	// it's used for querying for valid keystrokes on key events;
	// the position of the char in this string corresponds to the
	//	position of the corresponding lmCommand in lmCmdArray
    char *lmCmdStr;
	// an array of entries containing valid keystrokes and where
	//	they will be written in the Cmd portion of the display
    lmCommand *lmCmdArray;
private:
    void Init(lmCommand *CmdArray, int arraySize);
};

///////////////////////////////////////////////////////////////////////////////
//
// ListMgrCore inline member functions
//
///////////////////////////////////////////////////////////////////////////////

inline ListMgrCore::ListMgrCore(
	ButtonState* bs, int Rows, int Cols, 
	lmCommand *LmCmdArray, int arraySize, const char* Done) :
		MyStringBrowser(bs, Rows, Cols, true, Reversed, Done)
{
    Init(LmCmdArray, arraySize);
}
              //////////////////////////////////////////

inline ListMgrCore::ListMgrCore (
	const char* name, ButtonState* bs, int Rows, int Cols, 
	lmCommand *LmCmdArray, int arraySize, const char* Done) :
		MyStringBrowser(bs, Rows, Cols, true, Reversed, Done)
{
    SetInstance(name);
    Init(LmCmdArray, arraySize);
}
              //////////////////////////////////////////

              //////////////////////////////////////////

#include <IV-2_6/_leave.h>

#endif
