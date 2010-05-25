
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fileImport.cpp
/// \brief  #goblinImport class implementation

#include "fileImport.h"


goblinImport::goblinImport(const char* impFileName,goblinController& thisContext)
    throw(ERFile) :
    impFile(impFileName, ios::in), CT(thisContext)
{
    if (!impFile)
    {
        sprintf(CT.logBuffer,"Could not open import file %s, io_state %d",
            impFileName,impFile.rdstate());
        CT.Error(ERR_FILE,NoHandle,"goblinImport",CT.logBuffer);
    }

    impFile.flags(impFile.flags() | ios::right);
    currentLevel = 0;
    head = tail = false;
    complain = true;

    n = NoNode;
    m = NoArc;
    tuple = NULL;
}


char* goblinImport::Scan(char* token,TOwnership tp)
    throw(ERParse)
{
    impFile.width(255);
    impFile >> ws >> buffer;

    if (buffer[0] == '(')
    {
        if (strlen(buffer)==1)
            CT.Error(ERR_PARSE,NoHandle,"Scan",
                "Misplaced white space behind opening parenthesis");

        head = true;

        if (buffer[strlen(buffer)-1] == ')')
        {
            tail = true;
            buffer[strlen(buffer)-1] = 0;
        }
        else
        {
            tail = false;
            currentLevel++;
        }

        if (token!=NULL && strcmp(token,buffer+1))
        {
            sprintf(CT.logBuffer,"Unexpected token: %s, expected: %s",(buffer+1),token);
            CT.Error(ERR_PARSE,NoHandle,"Scan",CT.logBuffer);
        }

        if (tp==OWNED_BY_RECEIVER) return buffer+1;
        else
        {
            char* ret = new char[strlen(buffer+1)+1];
            sprintf(ret,"%s",buffer+1);
            return ret;
        }
    }
    else
    {
        head = false;
        tail = false;

        if (buffer[strlen(buffer)-1] == ')')
        {
            tail = true;

            if (currentLevel == 0)
                CT.Error(ERR_PARSE,NoHandle,"Scan","Parenthesis mismatch");

            currentLevel--;
            buffer[strlen(buffer)-1] = 0;
        }

        if (buffer[0]=='\"')
        {
            // String handling

            unsigned i = 1;

            while (buffer[i]!='\"' && buffer[i]!=0) i++;

            if (buffer[i]==0)
            {
                char buffer2[256];

                impFile.get(buffer2,255-i,'\"');

                sprintf(buffer,"%s%s",buffer+1,buffer2);

                impFile.get(buffer2[0]);
            }
            else
            {
                buffer[i] = 0;
                sprintf(buffer,"%s",buffer+1);
            }
        }

        if (tp==OWNED_BY_RECEIVER) return buffer;
        else
        {
            char* ret = new char[strlen(buffer)+1];
            sprintf(ret,"%s",buffer);
            return ret;
        }
    }
}


bool goblinImport::Seek(char* token)
    throw(ERParse)
{
    bool found = false;
    char minLevel = currentLevel;
    char* thisToken = Scan();

    while (!found)
    {
        if (strcmp(thisToken,token)) found = true;
        else thisToken = Scan();

        if (minLevel>currentLevel)
        {
            sprintf(CT.logBuffer,"Not in this scope: %s",token);
            CT.Error(ERR_PARSE,NoHandle,"Seek",CT.logBuffer);
        }
    }

    return found;
}


bool goblinImport::Head()
    throw()
{
    return head;
}


bool goblinImport::Tail()
    throw()
{
    return tail;
}


bool goblinImport::Eof()
    throw()
{
    return (tail && (currentLevel == 0));
}


TNode* goblinImport::GetTNodeTuple(unsigned long k)
    throw(ERParse)
{
    TNode* tuple = NULL;

    if (k==0) tuple = new TNode[1];
    else tuple = new TNode[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length==k && length>0)
            {
                delete[] tuple;
                CT.Error(ERR_PARSE,NoHandle,"GetTNodeTuple","Length mismatch");
            }

            #ifdef _BIG_NODES_

            if (strcmp(label,"*")) tuple[length++] = atol(label);

            #else

            if (strcmp(label,"*")) tuple[length++] = atoi(label);

            #endif

            else tuple[length++] = NoNode;
        }
    }

    if (length!=1 && length!=k && k!=0)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTNodeTuple","Length mismatch");
    }

    return tuple;
}


TArc* goblinImport::GetTArcTuple(unsigned long k)
    throw(ERParse)
{
    TArc* tuple = NULL;

    if (k==0) tuple = new TArc[1];
    else tuple = new TArc[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length==k && length>0)
            {
                delete[] tuple;
                CT.Error(ERR_PARSE,NoHandle,"GetTArcTuple","Length mismatch");
            }

            if (k==0) tuple = (TArc *)GoblinRealloc(tuple,sizeof(TArc)*(int)(length+1));

            #ifdef _SMALL_ARCS_

            if (strcmp(label,"*")) tuple[length++] = atoi(label);

            #else

            if (strcmp(label,"*")) tuple[length++] = atol(label);

            #endif

            else tuple[length++] = NoArc;
        }
    }

    if (length!=1 && length!=k && k!=0)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTArcTuple","Length mismatch");
    }

    return tuple;
}


TCap* goblinImport::GetTCapTuple(unsigned long k)
    throw(ERParse)
{
    TCap* tuple = NULL;

    if (k==0) tuple = new TCap[1];
    else tuple = new TCap[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                if (strcmp(label,"*")) tuple[length] = TCap(atof(label));
                else tuple[length] = InfCap;
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTCapTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetTCapTuple","Length exceeded");
    }

    return tuple;
}


TFloat* goblinImport::GetTFloatTuple(unsigned long k)
    throw(ERParse)
{
    TFloat* tuple = NULL;

    if (k==0) tuple = new TFloat[1];
    else tuple = new TFloat[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                if (strcmp(label,"*")) tuple[length] = TFloat(atof(label));
                else tuple[length] = InfFloat;
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTFloatTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetTFloatTuple","Length exceeded");
    }

    return tuple;
}


TIndex* goblinImport::GetTIndexTuple(unsigned long k)
    throw(ERParse)
{
    TIndex* tuple = NULL;

    if (k==0) tuple = new TIndex[1];
    else tuple = new TIndex[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;
        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                if (strcmp(label,"*")) tuple[length] = TIndex(atol(label));
                else tuple[length] = NoIndex;
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTIndexTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetTIndexTuple","Length exceeded");
    }

    return tuple;
}


char* goblinImport::GetCharTuple(unsigned long k)
    throw(ERParse)
{
    char* tuple = NULL;

    if (k==0) tuple = new char[1];
    else tuple = new char[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;
        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                tuple[length] = char(atoi(label));
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetCharTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetCharTuple","Length exceeded");
    }

    return tuple;
}


size_t goblinImport::AllocateTuple(TBaseType arrayType,TArrayDim dim)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (tuple)
        CT.Error(ERR_REJECTED,NoHandle,"AllocateTuple","Previous tuple was not consumed");

    if (n==NoNode || m==NoArc)
        CT.Error(ERR_REJECTED,NoHandle,"AllocateTuple","No dimensions assigned");

    #endif

    size_t length = 0;

    switch (dim)
    {
        case DIM_GRAPH_NODES:
        {
            length = n;
            break;
        }
        case DIM_GRAPH_ARCS:
        {
            length = m;
            break;
        }
        case DIM_ARCS_TWICE:
        {
            length = 2*m;
            break;
        }
        case DIM_LAYOUT_NODES:
        {
            length = n+ni;
            break;
        }
        case DIM_SINGLETON:
        {
            length = 1;
            break;
        }
        case DIM_PAIR:
        {
            length = 2;
            break;
        }
        case DIM_STRING:
        {
            length = 1;
            break;
        }
    }

    switch (arrayType)
    {
        case TYPE_NODE_INDEX:
        {
            tuple = static_cast<void*>(new TNode[length]);
            break;
        }
        case TYPE_ARC_INDEX:
        {
            tuple = static_cast<void*>(new TArc[length]);
            break;
        }
        case TYPE_FLOAT_VALUE:
        {
            tuple = static_cast<void*>(new TFloat[length]);
            break;
        }
        case TYPE_CAP_VALUE:
        {
            tuple = static_cast<void*>(new TCap[length]);
            break;
        }
        case TYPE_INDEX:
        {
            tuple = static_cast<void*>(new TIndex[length]);
            break;
        }
        case TYPE_ORIENTATION:
        {
            tuple = static_cast<void*>(new char[length]);
            break;
        }
        case TYPE_INT:
        {
            tuple = static_cast<void*>(new int[length]);
            break;
        }
        case TYPE_DOUBLE:
        {
            tuple = static_cast<void*>(new double[length]);
            break;
        }
        case TYPE_BOOL:
        {
            tuple = static_cast<void*>(new bool[length]);
            break;
        }
        case TYPE_CHAR:
        {
            tuple = static_cast<void*>(new char[length]);
            break;
        }
        case TYPE_VAR_INDEX:
        {
            tuple = static_cast<void*>(new TVar[length]);
            break;
        }
        case TYPE_RESTR_INDEX:
        {
            tuple = static_cast<void*>(new TRestr[length]);
            break;
        }
        case TYPE_SPECIAL:
        {
            break;
        }
    }

    return length;
}


void goblinImport::ReadTupleValues(TBaseType arrayType,size_t reqLength,void* _array)
    throw(ERParse)
{
    if (!_array) _array = tuple;

    length = 0;
    char* label = NULL;

    while (!tail)
    {
        label = Scan();

        if (strlen(label)>0)
        {
            if (length==reqLength && length>0)
            {
                length++;
                continue;
            }

            switch (arrayType)
            {
                case TYPE_NODE_INDEX:
                {
                    #ifdef _BIG_NODES_

                    if (strcmp(label,"*"))
                        static_cast<TNode*>(_array)[length++] = TNode(atol(label));

                    #else

                    if (strcmp(label,"*"))
                        static_cast<TNode*>(_array)[length++] = TNode(atoi(label));

                    #endif

                    else static_cast<TNode*>(_array)[length++] = NoNode;

                    break;
                }
                case TYPE_ARC_INDEX:
                {
                    #ifdef _SMALL_ARCS_

                    if (strcmp(label,"*"))
                        static_cast<TArc*>(_array)[length++] = TArc(atoi(label));

                    #else

                    if (strcmp(label,"*"))
                        static_cast<TArc*>(_array)[length++] = TArc(atol(label));

                    #endif

                    else static_cast<TArc*>(_array)[length++] = NoArc;

                    break;
                }
                case TYPE_FLOAT_VALUE:
                {
                    if (strcmp(label,"*"))
                        static_cast<TFloat*>(_array)[length++] = TFloat(atof(label));

                    else static_cast<TFloat*>(_array)[length++] = InfFloat;

                    break;
                }
                case TYPE_CAP_VALUE:
                {
                    if (strcmp(label,"*"))
                        static_cast<TCap*>(_array)[length++] = TCap(atof(label));

                    else static_cast<TCap*>(_array)[length++] = InfCap;

                    break;
                }
                case TYPE_INDEX:
                {
                    static_cast<TIndex*>(_array)[length++] = TIndex(atol(label));
                    break;
                }
                case TYPE_ORIENTATION:
                {
                    static_cast<char*>(_array)[length++] = char(atoi(label));
                    break;
                }
                case TYPE_INT:
                {
                    static_cast<int*>(_array)[length++] = int(atoi(label));
                    break;
                }
                case TYPE_DOUBLE:
                {
                    static_cast<double*>(_array)[length++] = double(atof(label));
                    break;
                }
                case TYPE_BOOL:
                {
                    static_cast<bool*>(_array)[length++] = bool(atoi(label));
                    break;
                }
                case TYPE_CHAR:
                {
                    static_cast<char*>(_array)[length++] = char(atoi(label));
                    break;
                }
                case TYPE_VAR_INDEX:
                {
                    static_cast<TVar*>(_array)[length++] = TVar(atol(label));
                    break;
                }
                case TYPE_RESTR_INDEX:
                {
                    static_cast<TRestr*>(_array)[length++] = TRestr(atol(label));
                    break;
                }
                case TYPE_SPECIAL:
                {
                    break;
                }
            }
        }
    }

    if (length<reqLength && length!=1)
    {
        delete[] (char*)tuple;
        CT.Error(ERR_PARSE,NoHandle,"ReadTupleValues","Length mismatch");
    }
    else if ((length>1 && reqLength==0) || (length>reqLength && reqLength!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"ReadTupleValues","Length exceeded");
    }
}


void goblinImport::ReadConfiguration()
    throw(ERParse)
{
    int ParamCount = 0;
    char** ParamStr = new char*[500];
    unsigned offset = 0;
    char* ParamBuffer = new char[5000];
    bool truncated = false;

    try
    {
        while (!Tail())
        {
            ParamCount++;
            char* token = Scan();

            if (ParamCount==500 || strlen(token)+1>2000-offset)
            {
                truncated = true;
                break;
            }

            ParamStr[ParamCount] = &ParamBuffer[offset];
            strcpy(ParamStr[ParamCount],token);
            offset += strlen(token)+1;
        }

        while (!Tail()) Scan();
    }
    catch (ERParse)
    {
        delete[] ParamStr;
        delete[] ParamBuffer;
    }


    CT.Configure(ParamCount+1,(const char**)ParamStr);

    delete[] ParamStr;
    delete[] ParamBuffer;

    if (truncated)
        CT.Error(MSG_WARN,NoHandle,"ReadConfiguration",
            "Buffer overflow: Configuration truncated");
}


bool goblinImport::Constant()
    throw()
{
    return (length == 1);
}


unsigned long goblinImport::Length()
    throw()
{
    return length;
}


void goblinImport::DontComplain()
    throw()
{
    complain = false;
}


goblinImport::~goblinImport()
    throw(ERParse)
{
    impFile.close();

    if (currentLevel>0 && complain)
        CT.Error(MSG_WARN,NoHandle,"goblinImport","Parenthesis mismatch");
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fileImport.cpp
/// \brief  #goblinImport class implementation

#include "fileImport.h"


goblinImport::goblinImport(const char* impFileName,goblinController& thisContext)
    throw(ERFile) :
    impFile(impFileName, ios::in), CT(thisContext)
{
    if (!impFile)
    {
        sprintf(CT.logBuffer,"Could not open import file %s, io_state %d",
            impFileName,impFile.rdstate());
        CT.Error(ERR_FILE,NoHandle,"goblinImport",CT.logBuffer);
    }

    impFile.flags(impFile.flags() | ios::right);
    currentLevel = 0;
    head = tail = false;
    complain = true;

    n = NoNode;
    m = NoArc;
    tuple = NULL;
}


char* goblinImport::Scan(char* token,TOwnership tp)
    throw(ERParse)
{
    impFile.width(255);
    impFile >> ws >> buffer;

    if (buffer[0] == '(')
    {
        if (strlen(buffer)==1)
            CT.Error(ERR_PARSE,NoHandle,"Scan",
                "Misplaced white space behind opening parenthesis");

        head = true;

        if (buffer[strlen(buffer)-1] == ')')
        {
            tail = true;
            buffer[strlen(buffer)-1] = 0;
        }
        else
        {
            tail = false;
            currentLevel++;
        }

        if (token!=NULL && strcmp(token,buffer+1))
        {
            sprintf(CT.logBuffer,"Unexpected token: %s, expected: %s",(buffer+1),token);
            CT.Error(ERR_PARSE,NoHandle,"Scan",CT.logBuffer);
        }

        if (tp==OWNED_BY_RECEIVER) return buffer+1;
        else
        {
            char* ret = new char[strlen(buffer+1)+1];
            sprintf(ret,"%s",buffer+1);
            return ret;
        }
    }
    else
    {
        head = false;
        tail = false;

        if (buffer[strlen(buffer)-1] == ')')
        {
            tail = true;

            if (currentLevel == 0)
                CT.Error(ERR_PARSE,NoHandle,"Scan","Parenthesis mismatch");

            currentLevel--;
            buffer[strlen(buffer)-1] = 0;
        }

        if (buffer[0]=='\"')
        {
            // String handling

            unsigned i = 1;

            while (buffer[i]!='\"' && buffer[i]!=0) i++;

            if (buffer[i]==0)
            {
                char buffer2[256];

                impFile.get(buffer2,255-i,'\"');

                sprintf(buffer,"%s%s",buffer+1,buffer2);

                impFile.get(buffer2[0]);
            }
            else
            {
                buffer[i] = 0;
                sprintf(buffer,"%s",buffer+1);
            }
        }

        if (tp==OWNED_BY_RECEIVER) return buffer;
        else
        {
            char* ret = new char[strlen(buffer)+1];
            sprintf(ret,"%s",buffer);
            return ret;
        }
    }
}


bool goblinImport::Seek(char* token)
    throw(ERParse)
{
    bool found = false;
    char minLevel = currentLevel;
    char* thisToken = Scan();

    while (!found)
    {
        if (strcmp(thisToken,token)) found = true;
        else thisToken = Scan();

        if (minLevel>currentLevel)
        {
            sprintf(CT.logBuffer,"Not in this scope: %s",token);
            CT.Error(ERR_PARSE,NoHandle,"Seek",CT.logBuffer);
        }
    }

    return found;
}


bool goblinImport::Head()
    throw()
{
    return head;
}


bool goblinImport::Tail()
    throw()
{
    return tail;
}


bool goblinImport::Eof()
    throw()
{
    return (tail && (currentLevel == 0));
}


TNode* goblinImport::GetTNodeTuple(unsigned long k)
    throw(ERParse)
{
    TNode* tuple = NULL;

    if (k==0) tuple = new TNode[1];
    else tuple = new TNode[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length==k && length>0)
            {
                delete[] tuple;
                CT.Error(ERR_PARSE,NoHandle,"GetTNodeTuple","Length mismatch");
            }

            #ifdef _BIG_NODES_

            if (strcmp(label,"*")) tuple[length++] = atol(label);

            #else

            if (strcmp(label,"*")) tuple[length++] = atoi(label);

            #endif

            else tuple[length++] = NoNode;
        }
    }

    if (length!=1 && length!=k && k!=0)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTNodeTuple","Length mismatch");
    }

    return tuple;
}


TArc* goblinImport::GetTArcTuple(unsigned long k)
    throw(ERParse)
{
    TArc* tuple = NULL;

    if (k==0) tuple = new TArc[1];
    else tuple = new TArc[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length==k && length>0)
            {
                delete[] tuple;
                CT.Error(ERR_PARSE,NoHandle,"GetTArcTuple","Length mismatch");
            }

            if (k==0) tuple = (TArc *)GoblinRealloc(tuple,sizeof(TArc)*(int)(length+1));

            #ifdef _SMALL_ARCS_

            if (strcmp(label,"*")) tuple[length++] = atoi(label);

            #else

            if (strcmp(label,"*")) tuple[length++] = atol(label);

            #endif

            else tuple[length++] = NoArc;
        }
    }

    if (length!=1 && length!=k && k!=0)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTArcTuple","Length mismatch");
    }

    return tuple;
}


TCap* goblinImport::GetTCapTuple(unsigned long k)
    throw(ERParse)
{
    TCap* tuple = NULL;

    if (k==0) tuple = new TCap[1];
    else tuple = new TCap[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                if (strcmp(label,"*")) tuple[length] = TCap(atof(label));
                else tuple[length] = InfCap;
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTCapTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetTCapTuple","Length exceeded");
    }

    return tuple;
}


TFloat* goblinImport::GetTFloatTuple(unsigned long k)
    throw(ERParse)
{
    TFloat* tuple = NULL;

    if (k==0) tuple = new TFloat[1];
    else tuple = new TFloat[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;

        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                if (strcmp(label,"*")) tuple[length] = TFloat(atof(label));
                else tuple[length] = InfFloat;
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTFloatTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetTFloatTuple","Length exceeded");
    }

    return tuple;
}


TIndex* goblinImport::GetTIndexTuple(unsigned long k)
    throw(ERParse)
{
    TIndex* tuple = NULL;

    if (k==0) tuple = new TIndex[1];
    else tuple = new TIndex[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;
        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                if (strcmp(label,"*")) tuple[length] = TIndex(atol(label));
                else tuple[length] = NoIndex;
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetTIndexTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetTIndexTuple","Length exceeded");
    }

    return tuple;
}


char* goblinImport::GetCharTuple(unsigned long k)
    throw(ERParse)
{
    char* tuple = NULL;

    if (k==0) tuple = new char[1];
    else tuple = new char[k];

    length = 0;

    while (!tail)
    {
        char* label = NULL;
        try {label = Scan();} catch(ERParse)
        {
            delete[] tuple;
            throw ERParse();
        }

        if (strlen(label)>0)
        {
            if (length<k || (length==0 && k==0))
            {
                tuple[length] = char(atoi(label));
            }

            length++;
        }
    }

    if (length<k && length!=1)
    {
        delete[] tuple;
        CT.Error(ERR_PARSE,NoHandle,"GetCharTuple","Length mismatch");
    }
    else if ((length>1 && k==0) || (length>k && k!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"GetCharTuple","Length exceeded");
    }

    return tuple;
}


size_t goblinImport::AllocateTuple(TBaseType arrayType,TArrayDim dim)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (tuple)
        CT.Error(ERR_REJECTED,NoHandle,"AllocateTuple","Previous tuple was not consumed");

    if (n==NoNode || m==NoArc)
        CT.Error(ERR_REJECTED,NoHandle,"AllocateTuple","No dimensions assigned");

    #endif

    size_t length = 0;

    switch (dim)
    {
        case DIM_GRAPH_NODES:
        {
            length = n;
            break;
        }
        case DIM_GRAPH_ARCS:
        {
            length = m;
            break;
        }
        case DIM_ARCS_TWICE:
        {
            length = 2*m;
            break;
        }
        case DIM_LAYOUT_NODES:
        {
            length = n+ni;
            break;
        }
        case DIM_SINGLETON:
        {
            length = 1;
            break;
        }
        case DIM_PAIR:
        {
            length = 2;
            break;
        }
        case DIM_STRING:
        {
            length = 1;
            break;
        }
    }

    switch (arrayType)
    {
        case TYPE_NODE_INDEX:
        {
            tuple = static_cast<void*>(new TNode[length]);
            break;
        }
        case TYPE_ARC_INDEX:
        {
            tuple = static_cast<void*>(new TArc[length]);
            break;
        }
        case TYPE_FLOAT_VALUE:
        {
            tuple = static_cast<void*>(new TFloat[length]);
            break;
        }
        case TYPE_CAP_VALUE:
        {
            tuple = static_cast<void*>(new TCap[length]);
            break;
        }
        case TYPE_INDEX:
        {
            tuple = static_cast<void*>(new TIndex[length]);
            break;
        }
        case TYPE_ORIENTATION:
        {
            tuple = static_cast<void*>(new char[length]);
            break;
        }
        case TYPE_INT:
        {
            tuple = static_cast<void*>(new int[length]);
            break;
        }
        case TYPE_DOUBLE:
        {
            tuple = static_cast<void*>(new double[length]);
            break;
        }
        case TYPE_BOOL:
        {
            tuple = static_cast<void*>(new bool[length]);
            break;
        }
        case TYPE_CHAR:
        {
            tuple = static_cast<void*>(new char[length]);
            break;
        }
        case TYPE_VAR_INDEX:
        {
            tuple = static_cast<void*>(new TVar[length]);
            break;
        }
        case TYPE_RESTR_INDEX:
        {
            tuple = static_cast<void*>(new TRestr[length]);
            break;
        }
        case TYPE_SPECIAL:
        {
            break;
        }
    }

    return length;
}


void goblinImport::ReadTupleValues(TBaseType arrayType,size_t reqLength,void* _array)
    throw(ERParse)
{
    if (!_array) _array = tuple;

    length = 0;
    char* label = NULL;

    while (!tail)
    {
        label = Scan();

        if (strlen(label)>0)
        {
            if (length==reqLength && length>0)
            {
                length++;
                continue;
            }

            switch (arrayType)
            {
                case TYPE_NODE_INDEX:
                {
                    #ifdef _BIG_NODES_

                    if (strcmp(label,"*"))
                        static_cast<TNode*>(_array)[length++] = TNode(atol(label));

                    #else

                    if (strcmp(label,"*"))
                        static_cast<TNode*>(_array)[length++] = TNode(atoi(label));

                    #endif

                    else static_cast<TNode*>(_array)[length++] = NoNode;

                    break;
                }
                case TYPE_ARC_INDEX:
                {
                    #ifdef _SMALL_ARCS_

                    if (strcmp(label,"*"))
                        static_cast<TArc*>(_array)[length++] = TArc(atoi(label));

                    #else

                    if (strcmp(label,"*"))
                        static_cast<TArc*>(_array)[length++] = TArc(atol(label));

                    #endif

                    else static_cast<TArc*>(_array)[length++] = NoArc;

                    break;
                }
                case TYPE_FLOAT_VALUE:
                {
                    if (strcmp(label,"*"))
                        static_cast<TFloat*>(_array)[length++] = TFloat(atof(label));

                    else static_cast<TFloat*>(_array)[length++] = InfFloat;

                    break;
                }
                case TYPE_CAP_VALUE:
                {
                    if (strcmp(label,"*"))
                        static_cast<TCap*>(_array)[length++] = TCap(atof(label));

                    else static_cast<TCap*>(_array)[length++] = InfCap;

                    break;
                }
                case TYPE_INDEX:
                {
                    static_cast<TIndex*>(_array)[length++] = TIndex(atol(label));
                    break;
                }
                case TYPE_ORIENTATION:
                {
                    static_cast<char*>(_array)[length++] = char(atoi(label));
                    break;
                }
                case TYPE_INT:
                {
                    static_cast<int*>(_array)[length++] = int(atoi(label));
                    break;
                }
                case TYPE_DOUBLE:
                {
                    static_cast<double*>(_array)[length++] = double(atof(label));
                    break;
                }
                case TYPE_BOOL:
                {
                    static_cast<bool*>(_array)[length++] = bool(atoi(label));
                    break;
                }
                case TYPE_CHAR:
                {
                    static_cast<char*>(_array)[length++] = char(atoi(label));
                    break;
                }
                case TYPE_VAR_INDEX:
                {
                    static_cast<TVar*>(_array)[length++] = TVar(atol(label));
                    break;
                }
                case TYPE_RESTR_INDEX:
                {
                    static_cast<TRestr*>(_array)[length++] = TRestr(atol(label));
                    break;
                }
                case TYPE_SPECIAL:
                {
                    break;
                }
            }
        }
    }

    if (length<reqLength && length!=1)
    {
        delete[] (char*)tuple;
        CT.Error(ERR_PARSE,NoHandle,"ReadTupleValues","Length mismatch");
    }
    else if ((length>1 && reqLength==0) || (length>reqLength && reqLength!=0))
    {
        CT.Error(MSG_WARN,NoHandle,"ReadTupleValues","Length exceeded");
    }
}


void goblinImport::ReadConfiguration()
    throw(ERParse)
{
    int ParamCount = 0;
    char** ParamStr = new char*[500];
    unsigned offset = 0;
    char* ParamBuffer = new char[5000];
    bool truncated = false;

    try
    {
        while (!Tail())
        {
            ParamCount++;
            char* token = Scan();

            if (ParamCount==500 || strlen(token)+1>2000-offset)
            {
                truncated = true;
                break;
            }

            ParamStr[ParamCount] = &ParamBuffer[offset];
            strcpy(ParamStr[ParamCount],token);
            offset += strlen(token)+1;
        }

        while (!Tail()) Scan();
    }
    catch (ERParse)
    {
        delete[] ParamStr;
        delete[] ParamBuffer;
    }


    CT.Configure(ParamCount+1,(const char**)ParamStr);

    delete[] ParamStr;
    delete[] ParamBuffer;

    if (truncated)
        CT.Error(MSG_WARN,NoHandle,"ReadConfiguration",
            "Buffer overflow: Configuration truncated");
}


bool goblinImport::Constant()
    throw()
{
    return (length == 1);
}


unsigned long goblinImport::Length()
    throw()
{
    return length;
}


void goblinImport::DontComplain()
    throw()
{
    complain = false;
}


goblinImport::~goblinImport()
    throw(ERParse)
{
    impFile.close();

    if (currentLevel>0 && complain)
        CT.Error(MSG_WARN,NoHandle,"goblinImport","Parenthesis mismatch");
}
