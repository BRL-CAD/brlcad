
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, June 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   messenger.h
/// \brief  #goblinMessenger class interface

#if !defined(_MESSENGER_H_)

#define _MESSENGER_H_

#include "goblinController.h"
#include <pthread.h>


typedef unsigned long TLog;


/// \brief  Control of thread communication
///
/// An object of this class manages the interaction of a working thread and its
/// supervisor thread (assumed: the GUI). The operations are protected by
/// semaphores.
class goblinMessenger
{
private:

    goblinController&   CT;

    TLog        qSize;        ///< The number of lines in the transcript buffer
    TLog        firstEntry;   ///< Buffer index of the first message
    TLog        firstFree;    ///< Index of the first unused buffer element
    TLog        lastEntry;    ///< Buffer index of the final message
    TLog        currentEntry; ///< Currently referenced ring buffer element
    msgType*    qMsg;         ///< A buffer of message types
    TModule*    qModule;      ///< A buffer of module specifiers
    THandle*    qHandle;      ///< A buffer of object handles
    int*        qLevel;       ///< A buffer of nesting levels
    char**      qText;        ///< A buffer of message texts
    TLog*       qNext;        ///< An array of buffer indices
    bool*       qHidden;      ///< A buffer of visibility flags

    pthread_mutex_t msgLock;         ///< Mutex to protect the internal message buffer
    pthread_mutex_t traceEventLock;  ///< Mutex to protect the traceEvent flag
    pthread_mutex_t traceLock;       ///< Mutex to protect the cTraceFile string
    pthread_mutex_t solverStateLock; ///< Mutex to protect the signalHalt state

    char*  cTraceFile; ///< The name of the most recent trace file
    bool   traceEvent; ///< A flag indicating an unacknowledged trace event

    /// \brief  Working thread execution states
    enum TThreadState {
        THREAD_STATE_IDLE    = 0, ///< No working thread exists at the time
        THREAD_STATE_RUNNING = 1, ///< A working thread is currently executing
        THREAD_STATE_STOPPED = 2, ///< The working thread has been signaled to end
        THREAD_STATE_PENDING = 3  ///< The working thread has been spawned but does not execute yet
    };

    TThreadState  signalHalt; ///< Current thread execution state

    mutable size_t  cachedLineNumber; ///< Transscript file line number matching cachedFilePos
    mutable long    cachedFilePos;    ///< Transscript file position matching cachedLineNumber

public:

    goblinMessenger(TLog,goblinController &thisContext = goblinDefaultContext) throw();
    ~goblinMessenger() throw();

    unsigned long  Size() throw();

    /// \brief  Reinitialize the tracing and logging handlers
    ///
    /// This calls SweepBuffer(), resets the cached transcript file pointers,
    /// unacknowledged trace events, and also some controller data regarding the
    /// tracing functionality.
    void  Restart() throw();

    /// \brief  Clear the message buffer
    void  SweepBuffer() throw();

    /// \brief  Process messages from the C++ library code
    ///
    /// This calls goblinController::DefaultLogEventHandler() for writing to the
    /// transscript file and MsgAppend() for writing to the internal message buffer.
    void  LogEventHandler(msgType,TModule,THandle,char*) throw();

    /// \brief  Write to the internal message buffer
    void  MsgAppend(msgType,TModule,THandle,int logLevel,char*) throw();

    /// \brief  Reset the pointer to the first buffered message
    void  MsgReset() throw();

    /// \brief  Check if there are unread messages
    bool  MsgEndOfBuffer() throw();

    /// \brief  Check if the message buffer is empty
    bool  MsgVoid() throw();

    /// \brief  Proceed to the next buffered message
    void  MsgSkip() throw(ERRejected);

    /// \brief  Retrieve the currently addressed message text
    size_t  MsgText(char* exportBuffer,size_t bufferSize) throw(ERRejected);

    /// \brief  Retrieve the currently addressed message class
    msgType  MsgClass() throw(ERRejected);

    /// \brief  Retrieve the currently addressed message module
    TModule  MsgModule() throw(ERRejected);

    /// \brief  Retrieve the currently addressed message nesting level
    int  MsgLevel() throw(ERRejected);

    /// \brief  Retrieve the currently addressed message originator
    THandle  MsgHandle() throw(ERRejected);

    /// \brief  Signal a trace event to the supervisor thread
    ///
    /// \param traceFileName  The name of the trace object file just generated or NULL
    ///
    /// This lets TraceEvent() become true until the flag is cleared by the
    /// supervisor thread using TraceUnblock(). The working thread can pend
    /// for this acknowledgement by calling TraceSemTake().
    void  SignalTraceEvent(char* traceFileName) throw();

    /// \brief  Wait for the supervisor thread to clear the trace event flag
    void  TraceSemTake() throw();

    /// \brief  Retrieve the file name of the most recent trace file
    ///
    /// \return    If this is less than bufferSize, the trace file name length
    /// \retval 0  No trace file exists
    size_t  TraceFilename(char* exportBuffer,size_t bufferSize) throw();

    /// \brief  Check for an unacknowledged trace event
    bool  TraceEvent() throw();

    /// \brief  Let the working thread continue after a trace event
    void  TraceUnblock() throw();

    /// \brief  The current thread execution state is THREAD_STATE_IDLE
    bool  SolverIdle() throw();

    /// \brief  The current thread execution state is THREAD_STATE_RUNNING
    bool  SolverRunning() throw();


    /// \brief  Signal the start of a working thread
    void  SolverSignalPending() throw();

    /// \brief  Acknowledge the start by the working thread
    void  SolverSignalStarted() throw();

    /// \brief  Try to stop the working thread prematurely
    ///
    /// This only works if the code executed by the working thread cyclically
    /// calls goblinController::SolverRunning() and a stop signal handler -
    /// registered in the controller object - calls this function. Assume that
    /// the working thread will end only after some delay.
    void  SolverSignalStop() throw();

    /// \brief  Signal the working thread completion
    void  SolverSignalIdle() throw();

    /// \brief  Retrieve the maximum number of lines in the transcript buffer
    TLog  GetBufferSize() const throw();

    /// \brief  Completely scan the transscript file for the number of lines
    ///
    /// \param fileName    The transscript file name
    /// \return            The number of lines in file
    ///
    /// This scans the entire file!
    unsigned long  GetNumLines(const char* fileName) const throw(ERFile);

    /// \brief  Load a line of the transscript file into a specified buffer
    ///
    /// \param fileName    The transscript file name
    /// \param lineBuffer  A buffer to store the line content
    /// \param bufferSize  The size of the line buffer
    /// \param lineNumber  The number of the addressed line. Line numbers count from zero
    /// \return            The start position of the subsequent line in file
    ///
    /// If the line content exceeds the buffer size, the line is truncated
    /// and marked with a " <..>" sequence. The returned position is even then
    /// the position of the first character on the subsequent line.
    long  GetLineByNumber(
        const char* fileName,char* lineBuffer,size_t bufferSize,size_t lineNumber) const throw(ERFile);

    /// \brief  Load a line of the transscript file into a specified buffer
    ///
    /// \param fileName    The transscript file name
    /// \param lineBuffer  A buffer to store the line content
    /// \param bufferSize  The size of the line buffer
    /// \param filePos     The file position of the first character in the addressed line
    /// \return            The file position of the first character in the subsequent line
    ///
    /// If the line content exceeds the buffer size, the line is truncated
    /// and marked with a " <..>" sequence. The returned position is even then
    /// the start position of the subsequent line.
    long  GetLineByPos(
        const char* fileName,char* lineBuffer,size_t bufferSize,long filePos) const throw(ERFile);

    /// \brief  Search back from a specified position
    ///
    /// \param fileName    The transscript file name
    /// \param filePos     A position in the transcript file
    /// \param numLines    The number of line feeds to overrun
    /// \return            The file position of the first character in target line
    ///
    /// Taking numLines==0, searches for the start position of the same line.
    long  SeekLinesBack(
        const char* fileName,long filePos,unsigned long numLines=1) const throw(ERFile);

    /// \brief  Search onwards from a specified position
    ///
    /// \param fileName    The transscript file name
    /// \param filePos     A position in the transcript file
    /// \param numLines    The number of line feeds to overrun
    /// \return            The file position of the first character in target line
    ///
    /// Taking numLines==0, searches for the start position of the same line.
    long  SeekLinesAhead(
        const char* fileName,long filePos,unsigned long numLines=1) const throw(ERFile);

    /// \brief  Load a part of the transscript file into the message buffer
    ///
    /// \param fileName    The transscript file name
    /// \param lineNumber  The final (!) line number in the buffer. Line numbers start by zero
    ///
    /// This fills message buffer with qSize consecutive lines of the transcript
    /// file. Lines are truncated to 500 characters. For sake of performance of
    /// iterated calls, the position in file and the line number are memorized.
    /// When the line number of the subsequent call is differs by at most 2*qSize,
    /// or is higher, the file is searched from the memorized position.
    void  LoadBuffer(const char* fileName,size_t lineNumber) throw(ERFile);
};


#endif
