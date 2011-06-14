
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <tcl.h>


enum P_or_R {P=0,R=1,PP=2};
enum Word_types { NONE_WT,W_WT,BRACE_WT,DQUOTE_WT,BRACKET_WT};
enum braces_history_types { open_BHT,close_BHT };

struct Braces_history
{
  braces_history_types type;
  int level;
  int line;
  int icharline;
  Braces_history* next;

  Braces_history(braces_history_types _type,int _level,int _line,int _icharline):type(_type),
	level(_level),line(_line),icharline(_icharline),next(NULL){}
  ~Braces_history(){
    if(next) delete next;
  }
};

struct InstrumenterState
{
  Tcl_Interp *ip;
  InstrumenterState* stack;
  Tcl_Obj *words;
  Tcl_Obj *currentword;
  Word_types wordtype;
  int wordtypeline;
  int wordtypepos;
  int DoInstrument;
  P_or_R OutputType;
  int NeedsNamespaceClose;
  int braceslevel;
  int snitpackageinserted;
  int level;
  Tcl_Obj *newblock[3];
  char **nonInstrumentingProcs;

  int line;
  Word_types type;

  int nextiscyan;
  Braces_history* braces_hist_1,* braces_hist_end;

  InstrumenterState() : braces_hist_1(NULL),braces_hist_end(NULL) {}
  ~InstrumenterState() {
    if(braces_hist_1) delete braces_hist_1;
  }
  int braces_history_error(int line);
};


int InstrumenterState::braces_history_error(int line)
{
  char buf[1024];
  const char* currentfile=Tcl_GetVar(ip,"RamDebugger::currentfile",TCL_GLOBAL_ONLY);
  if(!currentfile){
    Tcl_SetObjResult(ip,Tcl_NewStringObj("error in InstrumenterState::braces_history_error",-1));
    return TCL_ERROR;
  }
  Tcl_Obj *objPtr=Tcl_NewStringObj("BRACES POSITIONS\n",-1);
  Braces_history* bh=braces_hist_1;
  while(bh){
    if(bh->type==open_BHT)
      sprintf(buf,"%s:%d open brace pos=%d Level after=%d\n",
	      currentfile,bh->line,bh->icharline,bh->level);
    else
      sprintf(buf,"%s:%d close brace pos=%d Level after=%d\n",
	      currentfile,bh->line,bh->icharline,bh->level);
    Tcl_AppendToObj(objPtr,buf,-1);
    bh=bh->next;
  }
  Tcl_EvalEx(ip,"RamDebugger::TextOutClear; RamDebugger::TextOutRaise",-1,TCL_EVAL_GLOBAL);
  Tcl_Obj *listPtr=Tcl_NewListObj(0,NULL);
  Tcl_ListObjAppendElement(ip,listPtr,Tcl_NewStringObj("RamDebugger::TextOutInsert",-1));
  Tcl_ListObjAppendElement(ip,listPtr,objPtr);
  Tcl_IncrRefCount(listPtr);
  Tcl_EvalObjEx(ip,listPtr,TCL_EVAL_GLOBAL);
  Tcl_DecrRefCount(listPtr);
  sprintf(buf,"error in line %d. There is one unmatched closing brace (})",line);
  Tcl_SetObjResult(ip,Tcl_NewStringObj(buf,-1));
  return TCL_ERROR;
}

inline Tcl_Obj *Tcl_CopyIfShared(Tcl_Obj *obj)
{
  Tcl_Obj *objnew=obj;
  if(Tcl_IsShared(obj)){
    objnew=Tcl_DuplicateObj(obj);
    Tcl_DecrRefCount(obj);
    Tcl_IncrRefCount(objnew);
  }
  return objnew;
}

inline Tcl_Obj *Tcl_ResetString(Tcl_Obj *obj)
{
  Tcl_Obj *objnew=obj;
  if(Tcl_IsShared(obj)){
    objnew=Tcl_NewStringObj("",-1);
    Tcl_DecrRefCount(obj);
    Tcl_IncrRefCount(objnew);
  } else {
    Tcl_SetStringObj(objnew,"",-1);
  }
  return objnew;
}

inline Tcl_Obj *Tcl_ResetList(Tcl_Obj *obj)
{
  Tcl_Obj *objnew=obj;
  if(Tcl_IsShared(obj)){
    objnew=Tcl_NewListObj(0,NULL);
    Tcl_DecrRefCount(obj);
    Tcl_IncrRefCount(objnew);
  } else {
    Tcl_SetListObj(objnew,0,NULL);
  }
  return objnew;
}


void RamDebuggerInstrumenterInitState(InstrumenterState* is)
{
  int i,result,len;
  is->stack=(InstrumenterState*) malloc(1000*sizeof(InstrumenterState));
  is->words=Tcl_NewListObj(0,NULL);
  Tcl_IncrRefCount(is->words);
  is->currentword=Tcl_NewStringObj("",-1);
  Tcl_IncrRefCount(is->currentword);
  is->wordtype=NONE_WT;
  is->wordtypeline=-1;
  is->wordtypepos=-1;
  /*   = 0 no instrument, consider data; =1 instrument; =2 special case: switch; */
  /*   = 3 do not instrument but go inside */
  is->DoInstrument=0;
  is->NeedsNamespaceClose=0;
  is->braceslevel=0;
  is->level=0;
  is->snitpackageinserted=0;

  Tcl_EvalEx(is->ip,""
    "foreach i [list return break while eval foreach for if else elseif error switch default continue] {\n"
	     "set ::RamDebugger::Instrumenter::colors($i) magenta\n"
	     "}\n"
	     "foreach i [list variable set global incr lassign] {\n"
	     "set ::RamDebugger::Instrumenter::colors($i) green\n"
	     "}\n"
	     "foreach i [list #include static const if else new delete for return sizeof while continue \
		 break class typedef struct #else #endif #if] {\n"
	     "set ::RamDebugger::Instrumenter::colors_cpp($i) magenta\n"
	     "}\n"
	     "foreach i [list #ifdef #ifndef #define #undef] {\n"
	     "set ::RamDebugger::Instrumenter::colors_cpp($i) magenta2\n"
	     "}\n"
	     "foreach i [list char int double void] {\n"
	     "set ::RamDebugger::Instrumenter::colors_cpp($i) green\n"
	     "}",-1,0);

  result=Tcl_EvalEx(is->ip,"set RamDebugger::options(nonInstrumentingProcs)",-1,TCL_EVAL_GLOBAL);
  if(result==TCL_OK){
    int objc;
    Tcl_Obj** objv;
    result=Tcl_ListObjGetElements(is->ip,Tcl_GetObjResult(is->ip),&objc,&objv);
    if(result==TCL_OK){
      is->nonInstrumentingProcs=new char*[objc+1];
      for(i=0;i<objc;i++){
	char* str=Tcl_GetStringFromObj(objv[i],&len);
	is->nonInstrumentingProcs[i]=new char[len+1];
	strcpy(is->nonInstrumentingProcs[i],str);
      }
      is->nonInstrumentingProcs[i]=NULL;
    }
  }
  if(result!=TCL_OK){
    is->nonInstrumentingProcs=new char*[1];
    is->nonInstrumentingProcs[0]=NULL;
  }
}

int RamDebuggerInstrumenterEndState(InstrumenterState* is)
{
  int i;
  Tcl_Obj* result=NULL;
  char type,buf[1024];

  if(is->wordtype!=NONE_WT && is->wordtype!=W_WT){
    sprintf(buf,"There is a block of type (%c) beginning at line %d "
	    "that is not closed at the end of the file\n",is->wordtype,is->wordtypeline);
    Tcl_SetObjResult(is->ip,Tcl_NewStringObj(buf,-1));
    return TCL_ERROR;
  }
  for(i=0;i<is->level;i++){
    switch(is->stack[i].type){
    case W_WT: type='w'; break;
    case BRACE_WT: type='{'; break;
    case DQUOTE_WT: type='"'; break;
    case BRACKET_WT: type='['; break;
    }
    sprintf(buf,"There is a block of type (%c) beginning at line %d "
	    "that is not closed at the end of the file\n",type,is->stack[i].wordtypeline);
    if(result==NULL) result=Tcl_NewStringObj("",-1);
    Tcl_AppendToObj(result,buf,-1);
  }

  Tcl_DecrRefCount(is->words);
  Tcl_DecrRefCount(is->currentword);

  if(result!=NULL){
    Tcl_SetObjResult(is->ip,result);
    return TCL_ERROR;
  }
  i=0;
  while(is->nonInstrumentingProcs[i]){
    delete [] is->nonInstrumentingProcs[i++];
  }
  delete [] is->nonInstrumentingProcs;
  return TCL_OK;
}

int RamDebuggerInstrumenterPushState(InstrumenterState* is,Word_types type,int line)
{
  int i,NewDoInstrument=0,PushState=0,wordslen,intbuf,index;
  P_or_R NewOutputType;
  Tcl_Obj *word0,*word1,*wordi,*tmpObj;
  char* pword0,*pword1,*pchar,buf[1024];

  if(is->OutputType==P){
    NewOutputType=PP;
  } else {
    NewOutputType=is->OutputType;
  }

  if(type==BRACKET_WT){
    PushState=1;
    if(is->DoInstrument == 1) NewDoInstrument=1;
    else NewDoInstrument=0;
  }
  else {
    Tcl_ListObjLength(is->ip,is->words,&wordslen);
    if(wordslen){
      Tcl_ListObjIndex(is->ip,is->words,0,&word0);
      pword0=Tcl_GetStringFromObj(word0,NULL);
      if(*pword0==':' && *(pword0+1)==':') pword0+=2;
      if(wordslen>1){
	Tcl_ListObjIndex(is->ip,is->words,1,&word1);
	pword1=Tcl_GetStringFromObj(word1,NULL);
      }
    }

    if(wordslen==2 && strcmp(pword0,"constructor")==0)
      NewDoInstrument=1;
    else if(wordslen==1 && strcmp(pword0,"destructor")==0)
      NewDoInstrument=1;
    else if(wordslen==2 && strcmp(pword0,"oncget")==0) {
      int result=Tcl_GetIndexFromObj(NULL,word1,(const char**) is->nonInstrumentingProcs,"",TCL_EXACT,&index);
      if(result!=TCL_OK){
	NewDoInstrument=1;
      }
    }
    else if(wordslen==3 && (strcmp(pword0,"proc")==0 || strcmp(pword0,"method")==0 || 
	     strcmp(pword0,"typemethod")==0 || strcmp(pword0,"onconfigure")==0)){
      int result=Tcl_GetIndexFromObj(NULL,word1,(const char**) is->nonInstrumentingProcs,"",TCL_EXACT,&index);
      if(result!=TCL_OK){
	NewDoInstrument=1;
      }
    }
    else if(is->DoInstrument==0){
      if(wordslen==2 && (strcmp(pword0,"snit::type")==0 || strcmp(pword0,"snit::widget")==0 || 
		         strcmp(pword0,"snit::widgetadaptor")==0))
	PushState=1;
      else if(wordslen>=3 && strcmp(pword0,"namespace")==0 && strcmp(pword1,"eval")==0){
	PushState=1;
	/*                  if { $OutputType == "R" } { */
	/*                      upvar 2 $newblocknameP newblock */
	/*                  } else { upvar 2 $newblocknameR newblock } */
	/*                  append newblock "namespace eval [lindex $words 2] \{\n" */
	/*                  set NeedsNamespaceClose 1 */
      }
    }
    else if(is->DoInstrument==1){
      if(wordslen>0 && strcmp(pword0,"if")==0){
	if(wordslen==2) NewDoInstrument=1;
	else if(wordslen>2 && Tcl_ListObjIndex(is->ip,is->words,wordslen-1,&wordi)==TCL_OK &&
		(strcmp(Tcl_GetStringFromObj(wordi,NULL),"then")==0 ||
		 strcmp(Tcl_GetStringFromObj(wordi,NULL),"else")==0))
	  NewDoInstrument=1;
	else if(wordslen>2 && Tcl_ListObjIndex(is->ip,is->words,wordslen-2,&wordi)==TCL_OK &&
		strcmp(Tcl_GetStringFromObj(wordi,NULL),"elseif")==0)
	  NewDoInstrument=1;
      }
      else if(wordslen>0 && strcmp(pword0,"db")==0){
	if(wordslen>=3 && strcmp(pword1,"eval")==0){
	  // this is not a very correct one. It assumes that db is a sqlite3 handle
	  // can fail if db is an interp
	  NewDoInstrument=1;
	}
      }
      else if(wordslen>0 && strcmp(pword0,"dict")==0){
	if(wordslen==4 && strcmp(pword1,"for")==0){
	  NewDoInstrument=1;
	}
      }
      else if(wordslen>0 && strcmp(pword0,"namespace")==0){
	if(wordslen>=3 && strcmp(pword1,"eval")==0){
	  NewDoInstrument=1;
	  if(is->OutputType==R){
	    Tcl_ListObjIndex(is->ip,is->words,2,&wordi);
	    tmpObj=Tcl_NewListObj(1,&wordi);
	    Tcl_IncrRefCount(tmpObj);
	    sprintf(buf,"namespace eval %s {\n",Tcl_GetStringFromObj(tmpObj,NULL));
	    Tcl_DecrRefCount(tmpObj);
	    Tcl_AppendToObj(is->newblock[P],buf,-1);
	    is->NeedsNamespaceClose=1;
	  }
	}
      }
      else if(wordslen==2 && (strcmp(pword0,"snit::type")==0 || strcmp(pword0,"snit::widget")==0 || 
	       strcmp(pword0,"snit::widgetadaptor")==0)){
	NewDoInstrument=3;
	if(is->OutputType==R){
	  tmpObj=Tcl_NewListObj(0,NULL);
	  Tcl_IncrRefCount(tmpObj);
	  Tcl_ListObjAppendElement(is->ip,tmpObj,word0);
	  Tcl_ListObjAppendElement(is->ip,tmpObj,word1);
	  sprintf(buf,"%s {\n",Tcl_GetStringFromObj(tmpObj,NULL));
	  Tcl_DecrRefCount(tmpObj);
	  is->NeedsNamespaceClose=1;
	}
      }
      else if((wordslen==1 && strcmp(pword0,"catch")==0) ||
	(wordslen==2 && strcmp(pword0,"while")==0) ||
	(wordslen>=3 && strcmp(pword0,"foreach")==0) ||
	(wordslen>=3 && strcmp(pword0,"mk::loop")==0) ||
	(wordslen>=1 && wordslen<=4 && strcmp(pword0,"for")==0) ||
	(wordslen>1 && strcmp(pword0,"eval")==0) ||
	(wordslen>1 && strcmp(pword0,"html::eval")==0) ||
	(wordslen==3 && strcmp(pword0,"bind")==0) ||
	(wordslen==4 && strcmp(pword1,"sql")==0) &&
	Tcl_ListObjIndex(is->ip,is->words,2,&wordi)==TCL_OK &&
	strcmp(Tcl_GetStringFromObj(wordi,NULL),"maplist")==0) 
	NewDoInstrument=1;
      else if(wordslen>1 && strcmp(pword0,"uplevel")==0){
	int len=wordslen;
	pchar=pword1;
	if(*pchar=='#') pchar++;
	if(Tcl_GetInt(is->ip,pchar,&intbuf)==TCL_OK) len--;
	if(len>0) NewDoInstrument=1;
      }
      else if(wordslen>0 && strcmp(pword0,"switch")==0){
	for(i=1;i<wordslen;i++){
	  Tcl_ListObjIndex(is->ip,is->words,i,&wordi);
	  if(strcmp(Tcl_GetStringFromObj(wordi,NULL),"--")==0){
	    i++;
	    break;
	  } else if( *(Tcl_GetStringFromObj(wordi,NULL))!='-') break;
	}
	if(wordslen-i==1) NewDoInstrument=2;
      }
    } else if(is->DoInstrument == 2){
      if(wordslen%2) NewDoInstrument=1;
    }
  }
  if(!PushState && !NewDoInstrument) { return 1; }
  
  if(is->level>=0){
    is->stack[is->level].words=is->words;
    Tcl_IncrRefCount(is->stack[is->level].words);
    is->stack[is->level].currentword=is->currentword;
    Tcl_IncrRefCount(is->stack[is->level].currentword);
    is->stack[is->level].wordtype=is->wordtype;
    is->stack[is->level].wordtypeline=is->wordtypeline;
    is->stack[is->level].wordtypepos=is->wordtypepos;
    is->stack[is->level].DoInstrument=is->DoInstrument;
    is->stack[is->level].OutputType=is->OutputType;
    is->stack[is->level].NeedsNamespaceClose=is->NeedsNamespaceClose;
    is->stack[is->level].braceslevel=is->braceslevel;
    is->stack[is->level].line=line;
    is->stack[is->level].type=type;
  }
  is->level++;
    
  is->words=Tcl_ResetList(is->words);
  is->currentword=Tcl_ResetString(is->currentword);
  is->wordtype=NONE_WT;
  is->wordtypeline=-1;
  is->wordtypepos=-1;
  is->DoInstrument=NewDoInstrument;
  is->OutputType=NewOutputType;
  is->NeedsNamespaceClose=0;
  is->braceslevel=0;

  return 0;
}

int RamDebuggerInstrumenterPopState(InstrumenterState* is,Word_types type,int line) {
  
  char buf[1024];
  Word_types last_type;
  int i;

  if(is->level>0){
    last_type=is->stack[is->level-1].type;
    if(type==BRACKET_WT && last_type!=BRACKET_WT) return 1;
  }

  if(type==BRACE_WT){
    if(is->wordtype==W_WT){
      int numopen=0,len,i;
      len=Tcl_GetCharLength(is->currentword);
      for(i=0;i<len;i++){
	switch(Tcl_GetString(is->currentword)[i]){
	case '\\': i++; break;
	case '{': { numopen++; }
	case '}': { numopen--; }
	}
      }
      if(numopen) return 1;
    }
    if(last_type != BRACE_WT){
      for(i=0;i<is->level;i++){
	if(is->stack[i].type==BRACE_WT){
	  sprintf(buf,"Using a close brace (}) in line %d when there is an open brace "
		  "in line %d and an open bracket ([) in line %d"
		  ,line,is->stack[i].line,is->stack[is->level-1].line);
	  Tcl_SetObjResult(is->ip,Tcl_NewStringObj(buf,-1));
	  return -1;
	}
	return 1;
      } 
    }
  }

  int wordslen,isexpand=0;
  Tcl_ListObjLength(is->ip,is->words,&wordslen);
  if(!wordslen && strcmp(Tcl_GetString(is->currentword),"*")==0) isexpand=1;

  is->level--;
  if(is->level<0) return 0;
  Tcl_DecrRefCount(is->words);
  is->words=is->stack[is->level].words;
  Tcl_DecrRefCount(is->currentword);
  is->currentword=is->stack[is->level].currentword;
  is->wordtype=is->stack[is->level].wordtype;
  is->wordtypeline=is->stack[is->level].wordtypeline;
  is->wordtypepos=is->stack[is->level].wordtypepos;
  is->DoInstrument=is->stack[is->level].DoInstrument;
  is->OutputType=is->stack[is->level].OutputType;
  is->NeedsNamespaceClose=is->stack[is->level].NeedsNamespaceClose;
  is->braceslevel=is->stack[is->level].braceslevel;

  is->words=Tcl_CopyIfShared(is->words);
  if(isexpand) Tcl_ListObjAppendElement(is->ip,is->words,Tcl_NewStringObj("*",-1));
//   else Tcl_ListObjAppendElement(is->ip,is->words,Tcl_NewStringObj("",-1));

  if(is->NeedsNamespaceClose){
    Tcl_AppendToObj(is->newblock[P],"}\n",-1);
    is->NeedsNamespaceClose=0;
  }
  return 0;
}

int RamDebuggerInstrumenterIsProc(InstrumenterState* is)
{
  int wordslen;
  Tcl_Obj *word0;
  char* pword0;
  Tcl_ListObjLength(is->ip,is->words,&wordslen);
  if(wordslen==0) return 0;
  Tcl_ListObjIndex(is->ip,is->words,0,&word0);
  pword0=Tcl_GetStringFromObj(word0,NULL);
  if(*pword0==':' && *(pword0+1)==':') pword0+=2;

  if(strcmp(pword0,"snit::type")==0 || strcmp(pword0,"snit::widget")==0 || 
     strcmp(pword0,"snit::widgetadaptor")==0 || strcmp(pword0,"proc")==0 || 
     strcmp(pword0,"method")==0 || strcmp(pword0,"typemethod")==0 || 
     strcmp(pword0,"constructor")==0 || strcmp(pword0,"destructor")==0)
    return 1;
  return 0;
}

int RamDebuggerInstrumenterIsProcUpLevel(InstrumenterState* is)
{
  int wordslen;
  Tcl_Obj *word0;
  char* pword0;
  if(is->level<=0)
    return 0;
  Tcl_ListObjLength(is->ip,is->stack[is->level-1].words,&wordslen);
  if(wordslen==0) return 0;
  Tcl_ListObjIndex(is->ip,is->stack[is->level-1].words,0,&word0);
  pword0=Tcl_GetStringFromObj(word0,NULL);
  if(*pword0==':' && *(pword0+1)==':') pword0+=2;

  if(strcmp(pword0,"snit::type")==0 || strcmp(pword0,"snit::widget")==0 || 
     strcmp(pword0,"snit::widgetadaptor")==0 || strcmp(pword0,"proc")==0 || 
     strcmp(pword0,"method")==0 || strcmp(pword0,"typemethod")==0 || 
     strcmp(pword0,"constructor")==0 || strcmp(pword0,"destructor")==0)
    return 1;
  return 0;
}

void RamDebuggerInstrumenterInsertSnitPackage_ifneeded(InstrumenterState* is)
{
  Tcl_Obj *word0;
  char* pword0;
  Tcl_ListObjIndex(is->ip,is->words,0,&word0);
  pword0=Tcl_GetStringFromObj(word0,NULL);
  if(*pword0==':' && *(pword0+1)==':') pword0+=2;

  if(!is->snitpackageinserted &&
     (strcmp(pword0,"snit::type")==0 || strcmp(pword0,"snit::widget")==0 || 
      strcmp(pword0,"snit::widgetadaptor")==0)){
    is->snitpackageinserted=1;
    Tcl_AppendToObj(is->newblock[P],"package require snit\n",-1);
  }
}


/*  newblocknameP is for procs */
/*  newblocknameR is for the rest */
int RamDebuggerInstrumenterDoWork_do(Tcl_Interp *ip,char* block,int filenum,char* newblocknameP,
		                   char* newblocknameR,char* blockinfoname,int progress) {

  int i,length,braceslevelNoEval,lastinstrumentedline,
    line,ichar,icharline,consumed,instrument_proc_last_line,wordslen=0,
    quoteintobraceline=-1,quoteintobracepos,fail,commentpos,result;
  Word_types checkExtraCharsAfterCQB;
  Tcl_Obj *blockinfo,*blockinfocurrent,*word0,*wordi,*tmpObj;
  char c,lastc,buf[1024],*pword0=NULL;

  length = ( int)strlen(block);
  if(length>1000 && progress){
/*     RamDebugger::ProgressVar 0 1 */
  }

  InstrumenterState instrumenterstate,*is;
  is=&instrumenterstate;
  is->ip=ip;

  is->newblock[P]=Tcl_NewStringObj("",-1);
  Tcl_IncrRefCount(is->newblock[P]);
  is->newblock[PP]=is->newblock[P];
  is->newblock[R]=Tcl_NewStringObj("",-1);
  Tcl_IncrRefCount(is->newblock[R]);
  blockinfo=Tcl_NewListObj(0,NULL);
  Tcl_IncrRefCount(blockinfo);
  blockinfocurrent=Tcl_NewListObj(0,NULL);
  Tcl_IncrRefCount(blockinfocurrent);
  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(0));
  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
  RamDebuggerInstrumenterInitState(is);


  is->DoInstrument=1;
  is->OutputType=R;

  Tcl_AppendToObj(is->newblock[P],"# RamDebugger instrumented file. InstrumentProcs=1\n",-1);
  Tcl_AppendToObj(is->newblock[R],"# RamDebugger instrumented file. InstrumentProcs=0\n",-1);

  if(Tcl_ExprBoolean(is->ip,"$::RamDebugger::options(instrument_proc_last_line)",
		     &instrument_proc_last_line)!=TCL_OK) instrument_proc_last_line=0;

  braceslevelNoEval=0;
  checkExtraCharsAfterCQB=NONE_WT;
  lastc=0;
  lastinstrumentedline=-1;
  line=1;
  ichar=0;
  icharline=0;

  for(i=0;i<length;i++){
    c=block[i];

    if(ichar%1000 == 0 && progress){
/*       RamDebugger::ProgressVar [expr {$ichar*100/$length}] */
    }
    if(checkExtraCharsAfterCQB!=NONE_WT){
      if(!strchr(" \t\n}]\\;",c)){
	if(c=='"' && checkExtraCharsAfterCQB==BRACE_WT){
	  /* # nothing */
	} else {
	  char cblocktype;
	  switch(checkExtraCharsAfterCQB){
	  case BRACE_WT: cblocktype='}'; break;
	  case DQUOTE_WT: cblocktype='"'; break;
	  }
	  sprintf(buf,"There is a non valid character (%c) in line %d "
		  "after a closing block with (%c)",c,line,cblocktype);
	  Tcl_SetObjResult(ip,Tcl_NewStringObj(buf,-1));
	  Tcl_DecrRefCount(is->newblock[P]);
	  Tcl_DecrRefCount(is->newblock[R]);
	  Tcl_DecrRefCount(blockinfo);
	  Tcl_DecrRefCount(blockinfocurrent);
	  return TCL_ERROR;
	}
      }
      checkExtraCharsAfterCQB=NONE_WT;
    }
    if(is->DoInstrument==1 && lastinstrumentedline!=line && !strchr(" \t\n#",c) &&
       wordslen==0){
      if(c!='}' || !RamDebuggerInstrumenterIsProcUpLevel(is) || instrument_proc_last_line){
	sprintf(buf,"RDC::F %d %d ; ",filenum,line);
	Tcl_AppendToObj(is->newblock[is->OutputType],buf,-1);
	lastinstrumentedline=line;
      }
    }
    consumed=0;
    switch(c){
    case '"':
      if(lastc != '\\' && wordslen> 0 && *pword0!='#') {
	switch(is->wordtype){
	case NONE_WT:
	  is->wordtype=DQUOTE_WT;
	  is->wordtypeline=line;
	  is->wordtypepos=icharline;
	  consumed=1;
	  break;
	case DQUOTE_WT:
	  is->wordtype=NONE_WT;
	  is->words=Tcl_CopyIfShared(is->words);
	  Tcl_ListObjAppendElement(is->ip,is->words,is->currentword);
	  wordslen++;
	  Tcl_ListObjIndex(is->ip,is->words,0,&word0);
	  pword0=Tcl_GetStringFromObj(word0,NULL);
	  if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	  is->currentword=Tcl_ResetString(is->currentword);
	  consumed=1;
	  checkExtraCharsAfterCQB=DQUOTE_WT;
	  
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("grey",-1));
	  if(is->wordtypeline==line)
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(is->wordtypepos));
	  else Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(0));
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharline+1));
	  is->wordtypeline=0;

	  if(is->OutputType==R && RamDebuggerInstrumenterIsProc(is)){
	    int len,newllen;
	    Tcl_GetStringFromObj(is->newblock[R],&len);
	    newllen=len;
	    Tcl_GetStringFromObj(is->words,&len);
	    newllen-=len;
	    if(lastinstrumentedline==line){
	      sprintf(buf,"RDC::F %d %d ; ",filenum,line);
	      newllen -= ( int)strlen(buf);
	    }
	    Tcl_SetObjLength(is->newblock[R],newllen);
	    RamDebuggerInstrumenterInsertSnitPackage_ifneeded(is);
	    Tcl_AppendObjToObj(is->newblock[P],is->words);
	    is->OutputType=P;
	  }
	  break;
	case BRACE_WT:
	  if(quoteintobraceline==-1){
	    quoteintobraceline=line;
	    quoteintobracepos=icharline;
	  } else {
	    if(line==quoteintobraceline){
	      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	      Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("grey",-1));
	      Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(quoteintobracepos));
	      Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharline+1));
	    }
	    quoteintobraceline=-1;
	  }
	  break;
	}
      }
      break;
      case '{':
	if(lastc != '\\'){
	  switch(is->wordtype){
	  case BRACE_WT:
	    braceslevelNoEval++;
	    break;
	  case DQUOTE_WT: case W_WT:
	    is->braceslevel++;
	    break;
	  default:
	    consumed=1;
	    fail=RamDebuggerInstrumenterPushState(is,BRACE_WT,line);
	    if(fail){
	      is->wordtype=BRACE_WT;
	      is->wordtypeline=line;
	      braceslevelNoEval=1;
	    } else{
	      lastinstrumentedline=line;
	      Tcl_ListObjLength(is->ip,is->words,&wordslen);
	      if(wordslen){
		Tcl_ListObjIndex(is->ip,is->words,0,&word0);
		pword0=Tcl_GetStringFromObj(word0,NULL);
		if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	      }
	    }
	    break;
	  }
	}
	break;
      case '}':
	if(lastc != '\\'){
	  if(is->wordtype==BRACE_WT){
	    braceslevelNoEval--;
	    if(braceslevelNoEval==0){
	      is->wordtype=NONE_WT;
	      is->words=Tcl_CopyIfShared(is->words);
	      Tcl_ListObjAppendElement(is->ip,is->words,is->currentword);
	      wordslen++;
	      Tcl_ListObjIndex(is->ip,is->words,0,&word0);
	      pword0=Tcl_GetStringFromObj(word0,NULL);
	      if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	      if(*pword0!='#' && strcmp(Tcl_GetString(is->currentword),"*")!=0)
		checkExtraCharsAfterCQB=BRACE_WT;
	      is->currentword=Tcl_ResetString(is->currentword);
	      consumed=1;
	      if(is->OutputType==R && RamDebuggerInstrumenterIsProc(is)){
		int len,newllen;
		Tcl_GetStringFromObj(is->newblock[R],&len);
		newllen=len;
		Tcl_GetStringFromObj(is->words,&len);
		newllen-=len;
		if(lastinstrumentedline==line){
		  sprintf(buf,"RDC::F %d %d ; ",filenum,line);
		  newllen -= ( int)strlen(buf);
		}
		Tcl_SetObjLength(is->newblock[R],newllen);
		RamDebuggerInstrumenterInsertSnitPackage_ifneeded(is);
		Tcl_AppendObjToObj(is->newblock[P],is->words);
		is->OutputType=P;
	      }
	    }
	  } else if(is->braceslevel>0) is->braceslevel--;
	  else {
	    Word_types wordtype_before=is->wordtype;
	    fail=RamDebuggerInstrumenterPopState(is,BRACE_WT,line);
	    if(fail==-1){
	      Tcl_DecrRefCount(is->newblock[P]);
	      Tcl_DecrRefCount(is->newblock[R]);
	      Tcl_DecrRefCount(blockinfo);
	      Tcl_DecrRefCount(blockinfocurrent);
	      return TCL_ERROR;
	    }
	    if(!fail){
	      Tcl_ListObjLength(is->ip,is->words,&wordslen);
	      if(wordslen){
		Tcl_ListObjIndex(is->ip,is->words,0,&word0);
		pword0=Tcl_GetStringFromObj(word0,NULL);
		if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	      }
	      if(wordtype_before==DQUOTE_WT){
		sprintf(buf,"Quoted text (\") in line %d contains and invalid brace (})",line);
		Tcl_SetObjResult(is->ip,Tcl_NewStringObj(buf,-1));
		Tcl_DecrRefCount(is->newblock[P]);
		Tcl_DecrRefCount(is->newblock[R]);
		Tcl_DecrRefCount(blockinfo);
		Tcl_DecrRefCount(blockinfocurrent);
		return TCL_ERROR;
	      }
	      consumed=1;
	      if(wordslen){
		Tcl_ListObjIndex(is->ip,is->words,wordslen-1,&wordi);
		if(*pword0!='#' && strcmp(Tcl_GetString(wordi),"*")!=0)
		  checkExtraCharsAfterCQB=BRACE_WT;
	      }
	    }
	  }
	}
	break;
      case ' ': case '\t':
	if(lastc != '\\'){
	  if(is->wordtype==W_WT){
	    consumed=1;
	    is->wordtype=NONE_WT;
	    is->words=Tcl_CopyIfShared(is->words);
	    Tcl_ListObjAppendElement(is->ip,is->words,is->currentword);
	    wordslen++;
	    Tcl_ListObjIndex(is->ip,is->words,0,&word0);
	    pword0=Tcl_GetStringFromObj(word0,NULL);
	    if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	    is->currentword=Tcl_ResetString(is->currentword);
	    if(is->OutputType==R && RamDebuggerInstrumenterIsProc(is)){
	      int len,newllen;
	      Tcl_GetStringFromObj(is->newblock[R],&len);
	      newllen=len;
	      Tcl_GetStringFromObj(is->words,&len);
	      newllen-=len;
	      if(lastinstrumentedline==line){
		sprintf(buf,"RDC::F %d %d ; ",filenum,line);
		newllen -= ( int)strlen(buf);
	      }
	      Tcl_SetObjLength(is->newblock[R],newllen);
	      RamDebuggerInstrumenterInsertSnitPackage_ifneeded(is);
	      
	      Tcl_AppendObjToObj(is->newblock[P],is->words);
	      is->OutputType=P;
	    }
	    Tcl_ListObjIndex(is->ip,is->words,wordslen-1,&wordi);
	    if(RamDebuggerInstrumenterIsProc(is)){
	      int icharlineold=icharline-Tcl_GetCharLength(wordi);
	      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	      if(wordslen==1)
		Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("magenta",-1));
	      else
		Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("blue",-1));
	      Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharlineold));
	      Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	    } else{
	      tmpObj=Tcl_GetVar2Ex(is->ip,"::RamDebugger::Instrumenter::colors",
		   Tcl_GetStringFromObj(wordi,NULL),TCL_GLOBAL_ONLY);
	      if(tmpObj){
		int icharlineold=icharline-Tcl_GetCharLength(wordi);
		blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
		Tcl_ListObjAppendElement(is->ip,blockinfocurrent,tmpObj);
		Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharlineold));
		Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	      }
	    }
	  } else if(is->wordtype==NONE_WT) consumed=1;
	}
	break;
      case '[':
	if(lastc != '\\' && is->wordtype!=BRACE_WT && (wordslen==0 || *pword0!='#')){
	  if(is->wordtype==NONE_WT) is->wordtype=W_WT;
	  consumed=1;
	  RamDebuggerInstrumenterPushState(is,BRACKET_WT,line);
	  Tcl_ListObjLength(is->ip,is->words,&wordslen);
	  if(wordslen){
	    Tcl_ListObjIndex(is->ip,is->words,0,&word0);
	    pword0=Tcl_GetStringFromObj(word0,NULL);
	    if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	  }
	  lastinstrumentedline=line;
	}
	break;
      case ']':
	if(lastc != '\\' && is->wordtype!=BRACE_WT && is->wordtype!=DQUOTE_WT && 
	   (wordslen==0 || *pword0!='#')){
	  fail=RamDebuggerInstrumenterPopState(is,BRACKET_WT,line);
	  if(fail==-1){
	    Tcl_DecrRefCount(is->newblock[P]);
	    Tcl_DecrRefCount(is->newblock[R]);
	    Tcl_DecrRefCount(blockinfo);
	    Tcl_DecrRefCount(blockinfocurrent);
	    return TCL_ERROR;
	  }
	  if(!fail){
	    consumed=1;
	    Tcl_ListObjLength(is->ip,is->words,&wordslen);
	    if(wordslen){
	      Tcl_ListObjIndex(is->ip,is->words,0,&word0);
	      pword0=Tcl_GetStringFromObj(word0,NULL);
	      if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	    }
	  }
	  /*         note: the word inside words is not correct when using [] */
	}
	break;
      case '\n':
	if(wordslen> 0 && *pword0=='#'){
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("red",-1));
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(commentpos));
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	} else if(is->wordtype==DQUOTE_WT){
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("grey",-1));
	  if(is->wordtypeline==line)
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(is->wordtypepos));
	  else
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(0));
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	} else if(is->wordtype==W_WT){
	  tmpObj=Tcl_GetVar2Ex(is->ip,"::RamDebugger::Instrumenter::colors",
		               Tcl_GetStringFromObj(is->currentword,NULL),TCL_GLOBAL_ONLY);
	  if(tmpObj){
	    int icharlineold=icharline-Tcl_GetCharLength(is->currentword);
	    blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,tmpObj);
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharlineold));
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	  }
	}
	blockinfo=Tcl_CopyIfShared(blockinfo);
	Tcl_ListObjAppendElement(is->ip,blockinfo,blockinfocurrent);
	line++;
	tmpObj=Tcl_NewIntObj(is->level+braceslevelNoEval);
	Tcl_IncrRefCount(tmpObj);
	blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	Tcl_SetListObj(blockinfocurrent,1,&tmpObj);
	Tcl_DecrRefCount(tmpObj);

	if((is->wordtype==W_WT || is->wordtype==DQUOTE_WT) && is->braceslevel>0){
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
	}
	else if(is->wordtype!=BRACE_WT){
	  consumed=1;
	  if(lastc != '\\' && is->wordtype!=DQUOTE_WT){
	    is->words=Tcl_ResetList(is->words);
	    wordslen=0;
	    is->currentword=Tcl_ResetString(is->currentword);
	    is->wordtype=NONE_WT;

	    if(is->OutputType==P){
	      Tcl_AppendToObj(is->newblock[P],&c,1);
	      is->OutputType=R;
	    }
	    blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
	  } else {
	    if(is->wordtype==W_WT){
	      is->wordtype=NONE_WT;
	      if(strcmp(Tcl_GetStringFromObj(is->currentword,NULL),"\\")!=0) {
		is->words=Tcl_CopyIfShared(is->words);
		Tcl_ListObjAppendElement(is->ip,is->words,is->currentword);
		wordslen++;
		Tcl_ListObjIndex(is->ip,is->words,0,&word0);
		pword0=Tcl_GetStringFromObj(word0,NULL);
		if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	      }
	    }
	    blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	    Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("c",-1));
	  }
	} else{
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
	}
	break;
      case '#':
	if(wordslen==0 && strcmp(Tcl_GetStringFromObj(is->currentword,NULL),"")==0 &&
	   is->wordtype==NONE_WT){
	  consumed=1;
	  is->words=Tcl_CopyIfShared(is->words);
	  Tcl_ListObjAppendElement(is->ip,is->words,Tcl_NewStringObj("#",-1));
	  wordslen++;
	  Tcl_ListObjIndex(is->ip,is->words,0,&word0);
	  pword0=Tcl_GetStringFromObj(word0,NULL);
	  if(*pword0==':' && *(pword0+1)==':') pword0+=2;
	  commentpos=icharline;
	}
	break;
      case ';':
	if(lastc != '\\' && is->wordtype!=BRACE_WT && is->wordtype!=DQUOTE_WT &&
	   wordslen> 0 && *pword0!='#'){
	  consumed=1;
	  is->words=Tcl_ResetList(is->words);
	  wordslen=0;
	  Tcl_ListObjIndex(is->ip,is->words,0,&word0);
	  is->currentword=Tcl_ResetString(is->currentword);
	  is->wordtype=NONE_WT;
	  
	  if(is->OutputType==P){
	    Tcl_AppendToObj(is->newblock[P],&c,1);
	    is->OutputType=R;
	  }
	}
	break;
    }
    
    Tcl_AppendToObj(is->newblock[is->OutputType],&c,1);
    if(!consumed){
      if(is->wordtype==NONE_WT){
	is->wordtype=W_WT;
	is->wordtypeline=line;
      }
      is->currentword=Tcl_CopyIfShared(is->currentword);
      Tcl_AppendToObj(is->currentword,&c,1);
    }
    if(lastc == '\\' && c=='\\')
      lastc=0;
    else lastc=c;
    ichar++;

    if(c=='\t') icharline+=8;
    else if(c!='\n') icharline++;
    else icharline=0;
  }
  blockinfo=Tcl_CopyIfShared(blockinfo);
  Tcl_ListObjAppendElement(is->ip,blockinfo,blockinfocurrent);

  result=RamDebuggerInstrumenterEndState(is);

  Tcl_UpVar(ip,"1",newblocknameP,"newblockP",0);
  Tcl_SetVar2Ex(is->ip,"newblockP",NULL,is->newblock[P],0);
  Tcl_UpVar(ip,"1",newblocknameR,"newblockR",0);
  Tcl_SetVar2Ex(is->ip,"newblockR",NULL,is->newblock[R],0);
  Tcl_UpVar(ip,"1",blockinfoname,"blockinfo",0);
  Tcl_SetVar2Ex(is->ip,"blockinfo",NULL,blockinfo,0);

#ifdef _DEBUG
  char* tmpblockinfo=Tcl_GetString(blockinfo);
#endif
  if(length>1000 && progress){
    /*     RamDebugger::ProgressVar 100 */
  }
  Tcl_DecrRefCount(is->newblock[P]);
  Tcl_DecrRefCount(is->newblock[R]);
  Tcl_DecrRefCount(blockinfo);
  Tcl_DecrRefCount(blockinfocurrent);
  return result;
}


Tcl_Obj* append_block_info(InstrumenterState* is,Tcl_Obj *blockinfocurrent,Tcl_Obj* colorPtr,int p1,int p2)
{
  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,colorPtr);
  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(p1));
  Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewIntObj(p2));
  
  return blockinfocurrent;
}

Tcl_Obj* append_block_infoS(InstrumenterState* is,Tcl_Obj *blockinfocurrent,const char* color,
		            int p1,int p2)
{
  return append_block_info(is,blockinfocurrent,Tcl_NewStringObj(color,-1),p1,p2);
}

Tcl_Obj* check_word_color_cpp(InstrumenterState* is,Tcl_Obj *blockinfocurrent,int icharline,
		              int also_magenta2)
{
  Tcl_Obj *tmpObj;
  if(is->wordtype==W_WT){
    tmpObj=Tcl_GetVar2Ex(is->ip,"::RamDebugger::Instrumenter::colors_cpp",
		         Tcl_GetStringFromObj(is->currentword,NULL),TCL_GLOBAL_ONLY);
    if(tmpObj){
      int icharlineold=icharline-Tcl_GetCharLength(is->currentword);
      blockinfocurrent=append_block_info(is,blockinfocurrent,tmpObj,icharlineold,icharline);
      if(strcmp(Tcl_GetStringFromObj(tmpObj,NULL),"green")==0){
	is->nextiscyan=1;
      } else if(also_magenta2 && strcmp(Tcl_GetStringFromObj(tmpObj,NULL),"also_magenta2")==0){
	is->nextiscyan=1;
      }
    } else if(is->nextiscyan){
      int icharlineold=icharline-Tcl_GetCharLength(is->currentword);
      blockinfocurrent=append_block_infoS(is,blockinfocurrent,"cyan",icharlineold,icharline);
      is->nextiscyan=0;
    }
    is->wordtype=NONE_WT;
  }
  return blockinfocurrent;
}

int RamDebuggerInstrumenterDoWorkForCpp_do(Tcl_Interp *ip,char* block,char* blockinfoname,int progress,int indentlevel_ini) {

  int i,p1,length,
    line,ichar,icharline,simplechar_line,simplechar_pos,
    commentlevel,startofline,finishedline,result;
  Tcl_Obj *blockinfo,*blockinfocurrent,*tmpObj;
  char c,lastc,buf[1024];
  
  length = ( int)strlen(block);
  if(length>5000 && progress){
/*     RamDebugger::ProgressVar 0 1 */
  } else {
    progress=0;
  }

  InstrumenterState instrumenterstate,*is;
  is=&instrumenterstate;
  is->ip=ip;

  blockinfo=Tcl_NewListObj(0,NULL);
  Tcl_IncrRefCount(blockinfo);
  blockinfocurrent=Tcl_NewListObj(0,NULL);
  Tcl_IncrRefCount(blockinfocurrent);
  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(indentlevel_ini));
  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
  RamDebuggerInstrumenterInitState(is);
  is->braceslevel=indentlevel_ini;

  commentlevel=0;
//   braceslevelNoEval=0;
//   checkExtraCharsAfterCQB=NONE_WT;
//   wordslen=0;
//   quoteintobraceline=-1;
//   pword0=NULL
  lastc=0;
//   lastinstrumentedline=-1;
  line=1;
  ichar=0;
  icharline=0;
  simplechar_line=0;
  simplechar_pos=0;
  finishedline=1;
  startofline=1;
  is->nextiscyan=0;

  for(i=0;i<length;i++){
    c=block[i];

    if(ichar%5000 == 0 && progress){
      /*       RamDebugger::ProgressVar [expr {$ichar*100/$length}] */
    }
    if(simplechar_line>0){
      if(line>simplechar_line){
	sprintf(buf,"error in line %d, position %d. There is no closing (')",simplechar_line,
		simplechar_pos);
	Tcl_SetObjResult(is->ip,Tcl_NewStringObj(buf,-1));
	Tcl_DecrRefCount(blockinfo);
	Tcl_DecrRefCount(blockinfocurrent);
	return TCL_ERROR;
      }
      if(c=='\'' && lastc != '\\'){
	simplechar_line=0;
      }
      if(lastc == '\\' && c=='\\')
	lastc=0;
      else lastc=c;
      ichar++;

      if(c=='\t') icharline+=8;
      else if(c!='\n') icharline++;
      else icharline=0;
      continue;
    }
    switch(c){
    case '"':
      if(commentlevel){
	// nothing
      } else if(is->wordtype!=DQUOTE_WT){
	is->wordtype=DQUOTE_WT;
	is->wordtypeline=line;
	is->wordtypepos=icharline;
	finishedline=0;
	break;
      } else if(lastc != '\\'){
	is->wordtype=NONE_WT;
	if(is->wordtypeline==line) p1=is->wordtypepos;
	else p1=0;
	blockinfocurrent=append_block_infoS(is,blockinfocurrent,"grey",p1,icharline+1);
	is->wordtypeline=0;
      }
      startofline=0;
      break;
    case '\'':
      if(!commentlevel && is->wordtype!=DQUOTE_WT && lastc != '\\'){
	simplechar_line=line;
	simplechar_pos=icharline;
      }
      startofline=0;
      break;
    case '{':
      if(!commentlevel && is->wordtype!=DQUOTE_WT){
	blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,0);
	is->braceslevel++;
	Braces_history* bh=new Braces_history(open_BHT,is->braceslevel,line,icharline);
	if(!is->braces_hist_1)  is->braces_hist_1=bh;
	else is->braces_hist_end->next=bh;
	is->braces_hist_end=bh;
	finishedline=1;
      }
      startofline=0;
      break;
    case '}':
      if(!commentlevel && is->wordtype!=DQUOTE_WT){
	blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,1);
	is->braceslevel--;
	Braces_history* bh=new Braces_history(close_BHT,is->braceslevel,line,icharline);
	if(!is->braces_hist_1)  is->braces_hist_1=bh;
	else is->braces_hist_end->next=bh;
	is->braces_hist_end=bh;
	if(is->braceslevel<0){
	  return is->braces_history_error(line);
	}
	finishedline=1;
      }
      startofline=0;
      break;
    case '*':
      if(commentlevel==-1 || is->wordtype==DQUOTE_WT){
	// nothing
      } else if(lastc=='/'){
	if(commentlevel==0){
	  if(startofline){
	    Tcl_Obj *objPtr;
	    Tcl_ListObjIndex(is->ip,blockinfocurrent,0,&objPtr);
	    Tcl_SetIntObj(objPtr,0);
	  }
	  is->wordtype=NONE_WT;
	  is->wordtypepos=icharline-1;
	}
	commentlevel++;
      } else if(!commentlevel){
	blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,1);
      }
      startofline=0;
      break;
    case '/':
      if(commentlevel==-1 || is->wordtype==DQUOTE_WT){
	// nothing
      } else if(!commentlevel && lastc=='/'){
	if(startofline){
	  Tcl_Obj *objPtr;
	  Tcl_ListObjIndex(is->ip,blockinfocurrent,0,&objPtr);
	  Tcl_SetIntObj(objPtr,0);
	}
	is->wordtype=NONE_WT;
	is->wordtypepos=icharline-1;
	commentlevel=-1;
	startofline=0;
      } else if(lastc=='*'){
	is->wordtype=NONE_WT;
	if(commentlevel>=1){
	  commentlevel--;
	  if(commentlevel==0){
	    blockinfocurrent=append_block_infoS(is,blockinfocurrent,"red",is->wordtypepos,icharline+1);
	  }
	}
	startofline=0;
      } else if(!commentlevel){
	blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,1);
      }
      break;
    case '(':
	if(!commentlevel && is->braceslevel==0 && is->wordtype!=DQUOTE_WT){
	char* cw=Tcl_GetString(is->currentword);
	char* cipos=strstr(cw,"::");
	if(!cipos){
	  blockinfocurrent=append_block_infoS(is,blockinfocurrent,"blue",is->wordtypepos,icharline);
	} else {
	  blockinfocurrent=append_block_infoS(is,blockinfocurrent,"green",is->wordtypepos,
	    is->wordtypepos+cipos-cw+2);
	  blockinfocurrent=append_block_infoS(is,blockinfocurrent,"blue",
	    is->wordtypepos+cipos-cw+2,icharline);
	}
	is->nextiscyan=0;
      } else if(!commentlevel){
	blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,1);
      }
      startofline=0;
      break;
      case ';':
      if(!commentlevel){
	blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,1);
	if(is->wordtype!=DQUOTE_WT){
	  finishedline=1;
	}
      }
      startofline=0;
      break;
    case '\n':
      if(commentlevel){
	blockinfocurrent=append_block_infoS(is,blockinfocurrent,"red",is->wordtypepos,icharline);
	is->wordtypepos=0;
	if(commentlevel==-1) commentlevel=0;
	finishedline=1;
      } else if(is->wordtype==DQUOTE_WT){
	blockinfocurrent=append_block_infoS(is,blockinfocurrent,"grey",is->wordtypepos,icharline);
	is->wordtypepos=0;
      } else {
	blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,1);
      }
      blockinfo=Tcl_CopyIfShared(blockinfo);
      Tcl_ListObjAppendElement(is->ip,blockinfo,blockinfocurrent);
      line++;
      tmpObj=Tcl_NewIntObj(is->braceslevel);
      Tcl_IncrRefCount(tmpObj);
      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
      Tcl_SetListObj(blockinfocurrent,1,&tmpObj);
      Tcl_DecrRefCount(tmpObj);
      
      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
      if(finishedline){
	Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
	startofline=1;
      } else {
	Tcl_ListObjAppendElement(is->ip,blockinfocurrent,Tcl_NewStringObj("c",-1));
	startofline=0;
      }
      break;
    default:
      if(commentlevel || is->wordtype==DQUOTE_WT){
	// nothing
      } else if(is->wordtype==NONE_WT){
	if(startofline && c=='#'){
	  Tcl_Obj *objPtr;
	  Tcl_ListObjIndex(is->ip,blockinfocurrent,0,&objPtr);
	  Tcl_SetIntObj(objPtr,0);
	}
	if(isalnum(c) || c=='_' || c=='#' || c==':' || c==','){
	  is->wordtype=W_WT;
	  is->wordtypepos=icharline;
	  is->currentword=Tcl_ResetString(is->currentword);
	  is->currentword=Tcl_CopyIfShared(is->currentword);
	  Tcl_AppendToObj(is->currentword,&c,1);
	  //finishedline=0;
	  startofline=0;
	}
      } else if(is->wordtype==W_WT){
	if(isalnum(c) || c=='_' || c=='#' || c==':' || c==','){
	  is->currentword=Tcl_CopyIfShared(is->currentword);
	  Tcl_AppendToObj(is->currentword,&c,1);
	} else {
	  blockinfocurrent=check_word_color_cpp(is,blockinfocurrent,icharline,1);
	  finishedline=0;
	}
      }
    }
    if(lastc == '\\' && c=='\\')
      lastc=0;
    else lastc=c;
    ichar++;

    if(c=='\t') icharline+=8;
    else if(c!='\n') icharline++;
    else icharline=0;
  }
  blockinfo=Tcl_CopyIfShared(blockinfo);
  Tcl_ListObjAppendElement(is->ip,blockinfo,blockinfocurrent);

  if(commentlevel>0){
    sprintf(buf,"error: There is a non-closed comment beginning at line %d",is->wordtypeline);
    Tcl_SetObjResult(is->ip,Tcl_NewStringObj(buf,-1));
    Tcl_UpVar(ip,"1",blockinfoname,"blockinfo",0);
    Tcl_SetVar2Ex(is->ip,"blockinfo",NULL,blockinfo,0);
    Tcl_DecrRefCount(blockinfo);
    Tcl_DecrRefCount(blockinfocurrent);
    return TCL_ERROR;
  }
  if(is->braceslevel){
    Tcl_UpVar(ip,"1",blockinfoname,"blockinfo",0);
    Tcl_SetVar2Ex(is->ip,"blockinfo",NULL,blockinfo,0);
    return is->braces_history_error(line);
  }
  result=RamDebuggerInstrumenterEndState(is);

  Tcl_UpVar(ip,"1",blockinfoname,"blockinfo",0);
  Tcl_SetVar2Ex(is->ip,"blockinfo",NULL,blockinfo,0);

#ifdef _DEBUG
  char* tmpblockinfo=Tcl_GetString(blockinfo);
#endif
  if(progress){
    /*     RamDebugger::ProgressVar 100 */
  }
  Tcl_DecrRefCount(blockinfo);
  Tcl_DecrRefCount(blockinfocurrent);
  return result;
}

struct Xml_state_tag
{
  char* tag;
  int charlen;
  int line;
};

enum Xml_states_names {
  NONE_XS,
  doctype_XS,
  doctypeM_XS,
  pi_XS,
  comment_XS,
  cdata_XS,
  tag_XS,
  tag_end_XS,
  enter_tagtext_XS,
  entered_tagtext_XS,
  text_XS,
  att_text_XS,
  att_after_equal_XS,
  att_quote_XS,
  att_dquote_XS,
  att_after_name_XS,
  att_entername_XS
};

const int MaxStackLen=1000;

struct Xml_state
{
  Tcl_Interp *ip;
  int indentlevel;
  int xml_state_tag_listNum;
  Xml_state_tag xml_state_tag_list[MaxStackLen];
  int xml_states_names_listNum;
  Xml_states_names xml_states_names_list[MaxStackLen];
  int iline,icharline;

  Xml_state(Tcl_Interp *_ip,int _indentlevel): ip(_ip),indentlevel(_indentlevel),
		                               xml_state_tag_listNum(0),xml_states_names_listNum(0),
		                               iline(0),icharline(0) {}
  void enter_line_icharline(int _iline,int _icharline) { iline=_iline ; icharline=_icharline; }

  void push_state(Xml_states_names state);
  void pop_state();
  int state_is(Xml_states_names state,int idx=-1);

  void push_tag(char* tag,int len);
  void pop_tag(char* tag,int raiseerror,int len);
  void raise_error_if_tag_stack();

  void raise_error(const char* txt,int raiseerror);
};

void Xml_state::push_state(Xml_states_names state)
{
  if(xml_states_names_listNum>=MaxStackLen){
    Tcl_SetObjResult(ip,Tcl_NewStringObj("error in push_state. Stack full",-1));
    throw 1;
  }
  xml_states_names_list[xml_states_names_listNum++]=state;
}

void Xml_state::pop_state()
{
  xml_states_names_listNum--;
  if(xml_states_names_listNum<0){
    Tcl_SetObjResult(ip,Tcl_NewStringObj("error in pop_state. Stack empty",-1));
    throw 1;
  }
}

int Xml_state::state_is(Xml_states_names state,int idx)
{
  if(idx<0) idx=xml_states_names_listNum+idx;
  if(idx<0 || idx>=xml_states_names_listNum){
    return state==NONE_XS;
  }
  return state==xml_states_names_list[idx];
}

void Xml_state::push_tag(char* tag,int charlen)
{
  if(xml_state_tag_listNum>=MaxStackLen){
    Tcl_SetObjResult(ip,Tcl_NewStringObj("error in push_tag. Stack full",-1));
    throw 1;
  }
  xml_state_tag_list[xml_state_tag_listNum].tag=tag;
  xml_state_tag_list[xml_state_tag_listNum].charlen=charlen;
  xml_state_tag_list[xml_state_tag_listNum].line=iline;
  xml_state_tag_listNum++;
  indentlevel++;
}

void Xml_state::pop_tag(char* tag,int raiseerror,int charlen)
{
  int idx=xml_state_tag_listNum-1;
  if(tag && (charlen!=xml_state_tag_list[idx].charlen ||
    strncmp(tag,xml_state_tag_list[idx].tag,charlen)!=0)){
    if(!raiseerror) return;
    char buf[1024];
    if(charlen>800) charlen=800;
    sprintf(buf,"closing tag '%.*s' is not correct. line=%d position=%d. tags stack=\n",charlen,tag,
	    iline,icharline+1);
    Tcl_Obj *resPtr=Tcl_NewStringObj(buf,-1);
    for(int i=0;i<xml_state_tag_listNum;i++){
      sprintf(buf,"\t%.*s\tLine: %d\n",xml_state_tag_list[i].charlen,
	      xml_state_tag_list[i].tag,xml_state_tag_list[i].line);
      Tcl_AppendToObj(resPtr,buf,-1);
    }
    Tcl_SetObjResult(ip,resPtr);
    throw 1;
  }
  xml_state_tag_listNum--;
  indentlevel--;
  if(xml_state_tag_listNum<0){
    char buf[1024];
    sprintf(buf,"error in pop_tag. Stack empty. line=%d position=%d",iline,icharline+1);
    Tcl_SetObjResult(ip,Tcl_NewStringObj(buf,-1));
    throw 1;
  }
}

void Xml_state::raise_error_if_tag_stack()
{
  if(xml_state_tag_listNum){
    char buf[1024];
    Tcl_Obj *resPtr=Tcl_NewStringObj("There are non-closed tags. tags stack=\n",-1);
    for(int i=0;i<xml_state_tag_listNum;i++){
      sprintf(buf,"\t%.*s\tLine: %d\n",xml_state_tag_list[i].charlen,
	xml_state_tag_list[i].tag,xml_state_tag_list[i].line);
      Tcl_AppendToObj(resPtr,buf,-1);
    }
    Tcl_SetObjResult(ip,resPtr);
    throw 1;
  }
}

void Xml_state::raise_error(const char* txt,int raiseerror)
{
  if(!raiseerror) return;
  char buf[1024];
  int charlen=(int)strlen(txt);
  if(charlen>800) charlen=800;
  sprintf(buf,"error in line=%d position=%d. %.*s",iline,icharline+1,charlen,txt);
  Tcl_SetObjResult(ip,Tcl_NewStringObj(buf,-1));
  throw 1;
}

int RamDebuggerInstrumenterDoWorkForXML_do(Tcl_Interp *ip,char* block,char* blockinfoname,int progress,
		                           int indentlevel_ini,int raiseerror) {

  int i,length,state_start,state_start_global;
  char c,buf[1024];
  Tcl_Obj *blockinfo,*blockinfocurrent;

  length = ( int)strlen(block);
  if(length>1000 && progress){
    /*     RamDebugger::ProgressVar 0 1 */
  }
  blockinfo=Tcl_NewListObj(0,NULL);
  Tcl_IncrRefCount(blockinfo);
  blockinfocurrent=Tcl_NewListObj(0,NULL);
  Tcl_IncrRefCount(blockinfocurrent);
  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(indentlevel_ini));
  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
  int indentLevel0=indentlevel_ini;

  // colors: magenta magenta2 green red
  
  Xml_state xml_state(ip,indentlevel_ini);
  int icharline=0;
  int iline=1;
  xml_state.enter_line_icharline(iline,icharline);
  try {
    for(i=0;i<length;i++){
      c=block[i];

      if(i%5000 == 0 && progress){
	/* RamDebugger::ProgressVar [expr {$ichar*100/$length}] */
      }
      if(xml_state.state_is(cdata_XS)){
	if(strncmp(&block[i-2],"]]>",3)==0){
	  xml_state.pop_state();
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("green",-1));
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline-2));
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline+1));
	}
      } else if(xml_state.state_is(comment_XS)){
	if(strncmp(&block[i-2],"-->",3)==0){
	  xml_state.pop_state();
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("red",-1));
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline+1));
	} else if(c=='\n'){
	  blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("red",-1));
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
	  Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	  state_start=0;
	}
      } else if(xml_state.state_is(doctype_XS)){
	if(c=='['){
	  xml_state.pop_state();
	  xml_state.push_state(doctypeM_XS);
	} else if(c=='>'){
	  xml_state.pop_state();
	}
      } else if(xml_state.state_is(doctypeM_XS)){
	if(strncmp(&block[i-1],"]>",2)==0){
	  xml_state.pop_state();
	}
      } else {
	switch(c){
	case '<':
	  if(xml_state.state_is(att_dquote_XS) || xml_state.state_is(att_quote_XS)){
	    //nothing
	  } else if(!xml_state.state_is(NONE_XS) && !xml_state.state_is(text_XS)){
	    Tcl_SetObjResult(ip,Tcl_NewStringObj("Not valid <",-1));
	  } else if(strncmp(&block[i],"<?",2)==0){
	    xml_state.push_state(pi_XS);
	  } else if(strncmp(&block[i],"<!DOCTYPE",9)==0){
	    xml_state.push_state(doctype_XS);
	  } else if(strncmp(&block[i],"<!--",4)==0){
	    xml_state.push_state(comment_XS);
	    state_start=icharline;
	    state_start_global=i;
	  } else if(strncmp(&block[i],"<![CDATA[",9)==0){
	    xml_state.push_state(cdata_XS);
	    blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("green",-1));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline+9));
	  } else {
	    if(xml_state.state_is(text_XS)) { xml_state.pop_state(); }
	    xml_state.push_state(tag_XS);
	    blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("magenta",-1));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	  }
	  break;
	case '>':
	  if(xml_state.state_is(enter_tagtext_XS)){
	    xml_state.pop_state();
	    int isend;
	    if(xml_state.state_is(tag_end_XS)){
	      isend=1;
	      xml_state.pop_state();
	    } else { isend=0; }
	    if(xml_state.state_is(tag_XS)){
	      if(isend){
		xml_state.pop_tag(&block[state_start_global],raiseerror,i-state_start_global);
	      } else {
		xml_state.push_tag(&block[state_start_global],i-state_start_global);
	      }
	      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("magenta",-1));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	    } else {
	      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("green",-1));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	    }
	    xml_state.push_state(entered_tagtext_XS);
	  }
	  if(xml_state.state_is(att_dquote_XS) || xml_state.state_is(att_quote_XS)){
	    // nothing
	  } else {
	    if(!xml_state.state_is(entered_tagtext_XS)){
	      xml_state.raise_error("Not valid >",raiseerror);
	    }
	    xml_state.pop_state();
	    if(xml_state.state_is(pi_XS) && strncmp(&block[i-1],"?>",2)==0){
	      xml_state.pop_state();
	    } else if(xml_state.state_is(tag_XS)){
	      xml_state.pop_state();
	      xml_state.push_state(text_XS);
	    } else {
	      xml_state.raise_error("Not valid > (2)",raiseerror);
	    }
	  }
	  break;
	case '/':
	  if(xml_state.state_is(tag_XS) || xml_state.state_is(pi_XS)){
	    xml_state.push_state(tag_end_XS);
	    blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("red",-1));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline+1));
	  } else if(xml_state.state_is(enter_tagtext_XS)){
	    xml_state.pop_state();
	    if(xml_state.state_is(tag_XS)){
	      xml_state.push_tag(&block[state_start_global],i-state_start_global);
	      xml_state.pop_tag(&block[state_start_global],raiseerror,i-state_start_global);
	    }
	    blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("magenta",-1));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
	    Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	    xml_state.push_state(entered_tagtext_XS);
	  } else if(xml_state.state_is(entered_tagtext_XS)){
	     xml_state.pop_tag(NULL,raiseerror,0);
	  } else if(!xml_state.state_is(text_XS) && !xml_state.state_is(att_text_XS) &&
	     !xml_state.state_is(att_quote_XS) && !xml_state.state_is(att_dquote_XS)){
	       xml_state.raise_error("Not valid /",raiseerror);
	  }
	  break;
	  case '=':
	    if(xml_state.state_is(att_entername_XS) || xml_state.state_is(att_after_name_XS)){
	      xml_state.pop_state();
	      xml_state.push_state(att_after_equal_XS);
	    } else if(!xml_state.state_is(text_XS) &&
	      !xml_state.state_is(att_quote_XS) && !xml_state.state_is(att_dquote_XS)){
		xml_state.raise_error("Not valid =",raiseerror);
	    }
	    break;
	  case '\'':
	    if(xml_state.state_is(att_after_equal_XS)){
	      xml_state.pop_state();
	      xml_state.push_state(att_quote_XS);
	      state_start=icharline;
	      state_start_global=i;
	    } else if(xml_state.state_is(att_quote_XS)){
	      char cm1=block[i+1];
	      if(cm1!=' ' && cm1!='\t' && cm1!='\n' && cm1!='?' && cm1!='/' && cm1!='>'){
		xml_state.raise_error("Not valid close '",raiseerror);
	      }
	      xml_state.pop_state();
	      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("grey",-1));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline+1));
	    } else if(!xml_state.state_is(text_XS) && !xml_state.state_is(att_dquote_XS)){
		xml_state.raise_error("Not valid '",raiseerror);
	    }
	    break;
	  case '"':
	    if(xml_state.state_is(att_after_equal_XS)){
	      xml_state.pop_state();
	      xml_state.push_state(att_dquote_XS);
	      state_start=icharline;
	      state_start_global=i;
	    } else if(xml_state.state_is(att_dquote_XS)){
	      char cm1=block[i+1];
	      if(cm1!=' ' && cm1!='\t' && cm1!='\n' && cm1!='?' && cm1!='/' && cm1!='>'){
		xml_state.raise_error("Not valid close \"",raiseerror);
	      }
	      xml_state.pop_state();
	      blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("grey",-1));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
	      Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline+1));
	    } else if(!xml_state.state_is(text_XS) && !xml_state.state_is(att_quote_XS)){
		xml_state.raise_error("Not valid \"",raiseerror);
	    }
	    break;
	case ' ': case '\t': case '\n':
	    if(xml_state.state_is(enter_tagtext_XS)){
	       xml_state.pop_state();
	       int isend;
	       if(xml_state.state_is(tag_end_XS)){
		 isend=1;
		 xml_state.pop_state();
	       } else {
		 isend=0;
	       }
	       if(xml_state.state_is(tag_XS)){
		 if(isend){
		   xml_state.pop_tag(&block[state_start_global],raiseerror,i-state_start_global);
		 } else {
		   xml_state.push_tag(&block[state_start_global],i-state_start_global);
		 }
		 blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
		 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("magenta",-1));
		 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
		 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	       } else {
		 blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
		 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("green",-1));
		 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(state_start));
		 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(icharline));
	       }
	       xml_state.push_state(entered_tagtext_XS);
	    } else if(xml_state.state_is(att_entername_XS)){
	      xml_state.pop_state();
	      xml_state.push_state(att_after_name_XS);

	    }
	    break;
	  default:
	    if(xml_state.state_is(tag_XS) || xml_state.state_is(pi_XS) ||
	      xml_state.state_is(tag_end_XS)){
		xml_state.push_state(enter_tagtext_XS);
		state_start=icharline;
		state_start_global=i;
	    } else if(xml_state.state_is(entered_tagtext_XS)){
	      if(c!='?'){
		xml_state.push_state(att_entername_XS);
		state_start=icharline;
		state_start_global=i;
	      }
	    } else if(!xml_state.state_is(text_XS) && !xml_state.state_is(att_quote_XS) &&
	      !xml_state.state_is(att_dquote_XS) && !xml_state.state_is(enter_tagtext_XS) &&
	      !xml_state.state_is(att_entername_XS)){
		sprintf(buf,"Not valid character '%c'",c);
		xml_state.raise_error(buf,raiseerror);
	    }
	    break;
	 }
       }
       if(c=='\t'){
	 icharline+=8;
       } else if(c=='\n'){
	 iline++;
	 icharline=0;
	 if(xml_state.indentlevel<indentLevel0){
	   if(iline==2){
	     xml_state.indentlevel++;
	   } else {
	     Tcl_Obj *objPtr;
	     Tcl_ListObjIndex(ip,blockinfocurrent,0,&objPtr);
	     Tcl_SetIntObj(objPtr,xml_state.indentlevel);
	   }
	 }
	 blockinfo=Tcl_CopyIfShared(blockinfo);
	 Tcl_ListObjAppendElement(ip,blockinfo,blockinfocurrent);
	 blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);
	 Tcl_SetListObj(blockinfocurrent,0,NULL);
	 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewIntObj(xml_state.indentlevel));
	 Tcl_ListObjAppendElement(ip,blockinfocurrent,Tcl_NewStringObj("n",-1));
	 indentLevel0=xml_state.indentlevel;
       } else {
	 icharline++;
       }
       xml_state.enter_line_icharline(iline,icharline);
     }
     if(xml_state.indentlevel<indentLevel0){
       if(iline==2){
	 xml_state.indentlevel++;
       } else {
	 Tcl_Obj *objPtr;
	 Tcl_ListObjIndex(ip,blockinfocurrent,0,&objPtr);
	 Tcl_SetIntObj(objPtr,xml_state.indentlevel);
       }
     }
     blockinfo=Tcl_CopyIfShared(blockinfo);
     Tcl_ListObjAppendElement(ip,blockinfo,blockinfocurrent);
     blockinfocurrent=Tcl_CopyIfShared(blockinfocurrent);

     xml_state.raise_error_if_tag_stack();

     Tcl_UpVar(ip,"1",blockinfoname,"blockinfo",0);
     Tcl_SetVar2Ex(ip,"blockinfo",NULL,blockinfo,0);

#ifdef _DEBUG
     char* tmpblockinfo=Tcl_GetString(blockinfo);
#endif
     if(length>1000 && progress){
       /*     RamDebugger::ProgressVar 100 */
     }
     Tcl_DecrRefCount(blockinfo);
     Tcl_DecrRefCount(blockinfocurrent);
     return TCL_OK;
   }   
   catch(...){
     Tcl_UpVar(ip,"1",blockinfoname,"blockinfo",0);
     Tcl_SetVar2Ex(ip,"blockinfo",NULL,blockinfo,0);

     Tcl_DecrRefCount(blockinfo);
     Tcl_DecrRefCount(blockinfocurrent);
     return TCL_ERROR;
   }
}


int RamDebuggerInstrumenterDoWork(ClientData clientData, Tcl_Interp *ip, int objc,
		                  Tcl_Obj *CONST objv[])
{
  int result,filenum,progress=1;
  if (objc<6) {
    Tcl_WrongNumArgs(ip, 1, objv,
      "block filenum newblocknameP newblocknameR blockinfoname ?progress?");
    return TCL_ERROR;
  }
  result=Tcl_GetIntFromObj(ip,objv[2],&filenum);
  if(result) return TCL_ERROR;
  if (objc==7){
    result=Tcl_GetIntFromObj(ip,objv[6],&progress);
    if(result) return TCL_ERROR;
  }
  result=RamDebuggerInstrumenterDoWork_do(ip,Tcl_GetString(objv[1]),filenum,Tcl_GetString(objv[3]),
		                        Tcl_GetString(objv[4]),Tcl_GetString(objv[5]),progress);
  return result;
}

int RamDebuggerInstrumenterDoWorkForCpp(ClientData clientData, Tcl_Interp *ip, int objc,
		                  Tcl_Obj *CONST objv[])
{
  int result,progress=1,indentlevel_ini=0,raiseerror=1;
  if (objc<3) {
    Tcl_WrongNumArgs(ip, 1, objv,
		     "block blockinfoname ?progress? ?indent_level_ini?");
    return TCL_ERROR;
  }
  if (objc>=4){
    result=Tcl_GetIntFromObj(ip,objv[3],&progress);
    if(result) return TCL_ERROR;
  }
  if (objc>=5){
    result=Tcl_GetIntFromObj(ip,objv[4],&indentlevel_ini);
    if(result) return TCL_ERROR;
  }
  if (objc==6){
    result=Tcl_GetBooleanFromObj(ip,objv[5],&raiseerror);
    if(result) return TCL_ERROR;
  }
  result=RamDebuggerInstrumenterDoWorkForCpp_do(ip,Tcl_GetString(objv[1]),Tcl_GetString(objv[2]),
    progress,indentlevel_ini);
  return result;
}

int RamDebuggerInstrumenterDoWorkForXML(ClientData clientData, Tcl_Interp *ip, int objc,
		                  Tcl_Obj *CONST objv[])
{
  int result,progress=1,indentlevel_ini=0,raiseerror=1;
  if (objc<3) {
    Tcl_WrongNumArgs(ip, 1, objv,
		     "block blockinfoname ?progress? ?indentlevel_ini? ?raiseerror?");
    return TCL_ERROR;
  }
  if (objc>=4){
    result=Tcl_GetIntFromObj(ip,objv[3],&progress);
    if(result) return TCL_ERROR;
  }
  if (objc>=5){
    result=Tcl_GetIntFromObj(ip,objv[4],&indentlevel_ini);
    if(result) return TCL_ERROR;
  }
  if (objc==6){
    result=Tcl_GetBooleanFromObj(ip,objv[5],&raiseerror);
    if(result) return TCL_ERROR;
  }
  result=RamDebuggerInstrumenterDoWorkForXML_do(ip,Tcl_GetString(objv[1]),Tcl_GetString(objv[2]),
		                                progress,indentlevel_ini,raiseerror);
  return result;
}


extern "C" DLLEXPORT int Ramdebuggerinstrumenter_Init(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
  Tcl_InitStubs(interp,"8.5",0);
  //Tk_InitStubs(interp,"8.5",0);
#endif

  Tcl_CreateObjCommand( interp, "RamDebuggerInstrumenterDoWork",RamDebuggerInstrumenterDoWork,
		        ( ClientData)0, NULL);
  Tcl_CreateObjCommand( interp, "RamDebuggerInstrumenterDoWorkForCpp",RamDebuggerInstrumenterDoWorkForCpp,
		        ( ClientData)0, NULL);


  Tcl_CreateObjCommand( interp, "RamDebuggerInstrumenterDoWorkForXML",RamDebuggerInstrumenterDoWorkForXML,
		        ( ClientData)0, NULL);
  return TCL_OK;
}

extern "C" DLLEXPORT int Ramdebuggerinstrumenter_SafeInit(Tcl_Interp *interp)
{
  return Ramdebuggerinstrumenter_Init(interp);
}

extern "C" DLLEXPORT int Ramdebuggerinstrumenter_Unload(Tcl_Interp *interp,int flags)
{
  return TCL_ERROR;
}

extern "C" DLLEXPORT int Ramdebuggerinstrumenter_SafeUnload(Tcl_Interp *interp,int flags)
{
  return TCL_ERROR;
}






