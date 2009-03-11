
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

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include <listmgrcore.h>  // ivfasd/listmgrcore.h


void ListMgrCore::WriteChar(char c, int col, int index)
{ strbuf[index][col] = c;
  display->ReplaceText(index, strbuf[index], strlen(strbuf[index]));
  display->Style(index, 0, index, columns, Plain);
};
              //////////////////////////////////////////

void ListMgrCore::Insert (const char* s, int index)
{
    int len = strlen(s) + 1 + opColumns;
    char *sCopy = new char[len];
    
    memset(sCopy, ' ', opColumns);
//    memset(sCopy, ' ', opColumns);
//    bcopy(s, sCopy + opColumns, strlen(s) + 1); // non ANSI 
	// the 1st two operands are reversed from bcopy. memcpy() is ANSI 
 	// memcpy is o.k. since memory areas do not overlap - DAS
    memcpy(sCopy + opColumns, s, strlen(s) + 1); 
    MyStringBrowser::Insert (sCopy, index);

    delete [] sCopy;
}
              //////////////////////////////////////////

void ListMgrCore::Append (const char* s)
{
    Insert(s, strcount);
}
              //////////////////////////////////////////

void ListMgrCore::ReplaceText (const char* s, int index)
{
    int len = strlen(s) + 1 + opColumns;
    char *sCopy = new char[len];
    
//    memset(sCopy, ' ', opColumns);
    strncpy(sCopy, MyStringBrowser::String(index), opColumns);
//    sCopy[opColumns] = '\0';
//    bcopy(s, sCopy + opColumns, strlen(s) + 1); // non ANSI 
	// the 1st two operands are reversed from bcopy. memcpy() is ANSI 
 	// memcpy is o.k. since memory areas do not overlap - DAS
    memcpy(sCopy + opColumns, s, strlen(s) + 1); 
    MyStringBrowser::ReplaceText (sCopy, index);

    delete [] sCopy;
}
              //////////////////////////////////////////

char* ListMgrCore::String(int index)
{
    return MyStringBrowser::String(index) + opColumns;
}

char* ListMgrCore::CmdString(int index, char *str, int strSize)
{
    if(strSize > opColumns)
    {
	strncpy(str, MyStringBrowser::String(index), opColumns);
	str[opColumns] = '\0';
    }
    else
    {
	strncpy(str, MyStringBrowser::String(index), strSize);
	str[strSize - 1] = '\0';
    }
    return str;
}
              //////////////////////////////////////////

// if found, returns the position of 'ch' in the cmd string at 'index' 
//	otherwise returns -1 

int ListMgrCore::InCmdString(char ch, int index)
{
    int len = opColumns + 1;
    char *str = new char[len];

    CmdString(index, str, opColumns + 1);
    char *strptr = strchr(str, ch);

    if(strptr)
    {
	int rindex = strptr - str;
	delete [] str;
	return rindex;
    }
    delete [] str;
    return -1;
}
              //////////////////////////////////////////

void ListMgrCore::Init(lmCommand *CmdArray, int arraySize)
{
    SetClassName("ListMgrCore");
    opColumns = 0;

    lmCmdArray = CmdArray;
    lmCmdStr = new char[arraySize + 1];
    int i;
    for(i = 0; i < arraySize; i++) {
	lmCmdStr[i] = lmCmdArray[i].cmdChar;
	opColumns = max(opColumns, lmCmdArray[i].cmdCol);
    }
    lmCmdStr[i] = '\0';
    opColumns++;	// cols start at zero, so add 1 to total columns
}
              //////////////////////////////////////////

boolean ListMgrCore::HandleCharExtra(char c)
{
    int index = Selection();
    if(index >= 0){
		// since g++ is missing strpos() use strchr() work around.
	char *cmdIndexPtr = strchr((const char *)lmCmdStr, c);
	int cmdIndex = -1;
	if(cmdIndexPtr){
	    cmdIndex = cmdIndexPtr - lmCmdStr;
	}

	if(cmdIndex >= 0){
	    DoCommand(lmCmdArray[cmdIndex], index);
	    return true;
	} 
	else
	{
	    if(strchr(done, c) == nil)
		printf("index %d invalid char '%c'\n", index, c);
	}
	
//    char* copy = new char[strlen(s)+1];
//    strcpy(copy, s);

//    display->ReplaceText(index, s, strlen(s));

    }
    else printf("no Selections\n");
    return false;
}
              //////////////////////////////////////////

void ListMgrCore::DoCommand(lmCommand lm, int index) 
{
    printf("ListMgrCore doCommand() - char '%c', col: %d, index: %d\n",
		 lm.cmdChar, lm.cmdCol, index);
};

/*
void ListMgrCore::Save(int index)
{
}

void ListMgrCore::Delete(int index)
{
}

void ListMgrCore::Print(int index)
{
}

void ListMgrCore::Unmark(int index)
{
}

void ListMgrCore::Choose(int index)
{
}

void ListMgrCore::
{
}
*/
