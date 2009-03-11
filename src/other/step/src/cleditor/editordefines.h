
#ifndef editordefines_h
#define editordefines_h

/*
* NIST STEP Editor Class Library
* cleditor/editordefines.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: editordefines.h,v 3.0.1.2 1997/11/05 22:11:46 sauderd DP3.1 $ */ 

class GenericNode;
class GenNodeList;

class MgrNode;
class MgrNodeList;

class DisplayNode;
class DisplayNodeList;

//////////////////////////////////////////////////////////////////////////////

enum displayStateEnum {
		 mappedWrite,		// has a writable SEE on the screen
		 mappedView,		// has a view only SEE on the screen
		 notMapped,
		 noMapState
		}  ;

//////////////////////////////////////////////////////////////////////////////

enum stateEnum {
		 noStateSE,	// state undefined, not on a list
		 completeSE,	// on saved complete list
		 incompleteSE,	// on saved incomplete list
		 deleteSE,	// on delete list
		 newSE		// on newly created list
		}  ;

/*
   these variable are used by the STEPfile for reading and writing
   files in working session format.
   None of these variable should be set to 'E' as it will disrupt
   the way the read function finds the "ENDSEC;" token.
*/
static const char wsSaveComplete = 'C';
static const char wsSaveIncomplete = 'I';
static const char wsDelete = 'D';
static const char wsNew = 'N';

#endif
