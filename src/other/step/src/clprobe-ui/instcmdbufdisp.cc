
/*
* NIST Data Probe Class Library
* clprobe-ui/instcmdbufdisp.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: instcmdbufdisp.cc,v 3.0.1.1 1997/11/05 23:01:06 sauderd DP3.1 $ */

/*
 * InstCmdBufDisp - an object that let's you manage a list.  Meaning that you
 * can mark an entry on the list for:
 *    1) v(iew) or m(odify)
 *    2) r(eplicate) or d(elete)
 *    3) p(rint)
 *    4) c(hoose)
 *    5) h(ide)
 *    6) s(ave complete)
 *    7) i(save incomplete)
 */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>

#include <listmgrcore.h>	// ivfasd/listmgrcore.h
#include <instcmdbufdisp.h>	// probe-ui/instcmdbufdisp.h
#include <probe.h>

const int defaultCmdSize = 18;
lmCommand defaultCmds[defaultCmdSize] =
{
 lmCommand('s', SAVE_COMPLETE_CMD_COL),
 lmCommand('S', SAVE_COMPLETE_CMD_COL),
 lmCommand('i', SAVE_INCOMPLETE_CMD_COL),
 lmCommand('I', SAVE_INCOMPLETE_CMD_COL),
 lmCommand('m', MODIFY_CMD_COL),
 lmCommand('M', MODIFY_CMD_COL),
 lmCommand('v', VIEW_CMD_COL),
 lmCommand('V', VIEW_CMD_COL),
 lmCommand('d', DELETE_CMD_COL),
 lmCommand('D', DELETE_CMD_COL),
 lmCommand('c', CLOSE_CMD_COL),
 lmCommand('C', CLOSE_CMD_COL),
 lmCommand('r', REPLICATE_CMD_COL),
 lmCommand('R', REPLICATE_CMD_COL),
 lmCommand('x', EXECUTE_CMD_COL),
 lmCommand('X', EXECUTE_CMD_COL),
 lmCommand('u', UNMARK_CMD_COL),
 lmCommand('U', UNMARK_CMD_COL)
};

extern Probe *dp;

InstCmdBufDisp::InstCmdBufDisp(ButtonState* bs, int Rows, int Cols, 
		const char* Done) :
	ListMgrCore(bs, Rows, Cols, defaultCmds, defaultCmdSize, Done)
{
    Init();
}
              //////////////////////////////////////////

/*
InstCmdBufDisp::InstCmdBufDisp(const char* name, ButtonState* bs, 
			int rows, int cols, const char* done) :
	ListMgrCore(bs, rows, cols, defaultCmds, defaultCmdSize, done)
{
    Init();
}
*/
              //////////////////////////////////////////

void InstCmdBufDisp::Init()
{
    SetClassName("InstCmdBufDisp");
}

void InstCmdBufDisp::Choose()
{
    printf("choose command\n");
}

void InstCmdBufDisp::AdvanceSelection(int index)
{
    Unselect(index);
    index = max(0, min(++index, strcount-1));
    Select(index);
    ScrollTo(index);
}


void InstCmdBufDisp::DoCommand(lmCommand lm, int index)
{
//printf("InstCmdBufDisp doCommand() - char '%c', col: %d, index: %d\n", 
//						lm.cmdChar, lm.cmdCol, index);

    char ch = tolower(lm.cmdChar);
    if(index >= 0){
	dp->UpdateCmdList(index, ch, lm.cmdCol);
	if (ch != 'x' && ch != 'X') {
	    AdvanceSelection(index);
	}
    }
}
