
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fileExport.h
/// \brief  #goblinExport class interface

#ifndef _FILE_EXPORT_H_
#define _FILE_EXPORT_H_

#include "goblinController.h"


/// \addtogroup groupObjectExport
/// @{

/// \brief  Class for writing data objects to file

class goblinExport : public goblinRootObject
{
private:

    char                currentLevel;
    char                currentPos;
    char                currentType;
    ofstream            expFile;
    goblinController&   CT;

public:

    goblinExport(const char* expFileName,
        goblinController &thisContext = goblinDefaultContext) throw(ERFile);
    ~goblinExport() throw();

    char*           Display() const throw() {return NULL;};
    const char*     Label() const throw() {return NULL;};
    unsigned long   Size() const throw() {return 0;};
    ofstream&       Stream() throw() {return expFile;};

    void    StartTuple(const char* header,char type) throw(ERRejected);
    void    StartTuple(unsigned long k,char type) throw(ERRejected);
    void    EndTuple() throw(ERRejected);
    template <typename T> void MakeItem(T value,int length) throw();
    void    MakeNoItem(int length) throw();

    template <typename T> void WriteAttribute(const T*,const char*,size_t,T) throw();

    enum    TConfig {CONF_DIFF=0,CONF_FULL=1};

    void    WriteConfiguration(const goblinController&,TConfig = CONF_DIFF) throw();

};

/// @}


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   fileExport.h
/// \brief  #goblinExport class interface

#ifndef _FILE_EXPORT_H_
#define _FILE_EXPORT_H_

#include "goblinController.h"


/// \addtogroup groupObjectExport
/// @{

/// \brief  Class for writing data objects to file

class goblinExport : public goblinRootObject
{
private:

    char                currentLevel;
    char                currentPos;
    char                currentType;
    ofstream            expFile;
    goblinController&   CT;

public:

    goblinExport(const char* expFileName,
        goblinController &thisContext = goblinDefaultContext) throw(ERFile);
    ~goblinExport() throw();

    char*           Display() const throw() {return NULL;};
    const char*     Label() const throw() {return NULL;};
    unsigned long   Size() const throw() {return 0;};
    ofstream&       Stream() throw() {return expFile;};

    void    StartTuple(const char* header,char type) throw(ERRejected);
    void    StartTuple(unsigned long k,char type) throw(ERRejected);
    void    EndTuple() throw(ERRejected);
    template <typename T> void MakeItem(T value,int length) throw();
    void    MakeNoItem(int length) throw();

    template <typename T> void WriteAttribute(const T*,const char*,size_t,T) throw();

    enum    TConfig {CONF_DIFF=0,CONF_FULL=1};

    void    WriteConfiguration(const goblinController&,TConfig = CONF_DIFF) throw();

};

/// @}


#endif
