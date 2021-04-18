/*=============================================================================
                                 pm_system
===============================================================================
   This is the library subroutine pm_system().  It is just like Standard C
   Library system(), except that you can supply routines for it to run to
   generate the Standard Input for the executed shell command and to accept
   the Standard Output from it.  system(), by contrast, always sets up the
   current Standard Input and Standard Output as the Standard Input and
   Standard Output of the shell command.

   By Bryan Henderson, San Jose CA  2002.12.14.

   Contributed to the public domain.
=============================================================================*/
#define _XOPEN_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include "pm.h"
#include "pm_system.h"

#define STDIN 0
#define STDOUT 1


static void
execProgram(const char * const shellCommand,
            int          const stdinFd,
            int          const stdoutFd) {
/*----------------------------------------------------------------------------
   Run the shell command 'shellCommand', supplying to the shell
   'inputPipeFd' as its Standard Input and 'outputPipeFd' as its 
   Standard Output.

   But leave Standard Input and Standard Output as we found them.
-----------------------------------------------------------------------------*/
    int stdinSaveFd, stdoutSaveFd;
    int rc;
    int execErrno;

    /* Make inputPipeFd Standard Input.
       Make outputPipeFd Standard Output.
    */
    if (stdinFd != STDIN) {
        stdinSaveFd  = dup(STDIN);
        close(STDIN);
        dup2(stdinFd, STDIN);
    }
    if (stdoutFd != STDOUT) {
        stdoutSaveFd = dup(STDOUT);
        close(STDOUT);
        dup2(stdoutFd, STDOUT);
    }

    rc = execl("/bin/sh", "sh", "-c", shellCommand, NULL);

    execErrno = errno;

    if (stdinFd != STDIN) {
        close(STDIN);
        dup2(stdinSaveFd, STDIN);
        close(stdinSaveFd);
    }
    if (stdoutFd != STDOUT) {
        close(STDOUT);
        dup2(stdoutSaveFd, STDOUT);
        close(stdoutSaveFd);
    }
    if (rc < 0)
        pm_error("Unable to exec the shell.  Errno=%d (%s)",
                 execErrno, strerror(execErrno));
    else
        pm_error("INTERNAL ERROR.  execl() returns, but does not fail.");
}



static void
createPipeFeeder(void          pipeFeederRtn(int, void *), 
                 void *  const feederParm, 
                 int *   const fdP,
                 pid_t * const pidP) {
/*----------------------------------------------------------------------------
   Create a process and a pipe.  Have the process run program
   'pipeFeederRtn' to fill the pipe and return the file descriptor of the
   other end of the pipe as *fdP.
-----------------------------------------------------------------------------*/
    int pipeToFeed[2];
    pid_t feederPid;

    pipe(pipeToFeed);
    feederPid = fork();
    if (feederPid < 0) {
        pm_error("fork() of stdin feeder failed.  errno=%d (%s)", 
                 errno, strerror(errno));
    } else if (feederPid == 0) {
        /* This is the child -- the stdin feeder process */
        close(pipeToFeed[0]);
        (*pipeFeederRtn)(pipeToFeed[1], feederParm);
        exit(0);
    }
    else {
        /* This is the parent */
        close(pipeToFeed[1]);
        *fdP = pipeToFeed[0];
        *pidP = feederPid;
    }
}



static void
spawnProcessor(const char * const shellCommand, 
               int          const stdinFd,
               int *        const stdoutFdP,
               pid_t *      const pidP) {
/*----------------------------------------------------------------------------
   Create a process to run a shell that runs command 'shellCommand'.
   Pass file descriptor 'stdinFd' to the shell as Standard Input.
   Set up a pipe and pass it to the shell as Standard Output.  Return
   as *stdoutFdP the file descriptor of the other end of that pipe,
   from which Caller can suck the shell's Standard Output.
-----------------------------------------------------------------------------*/
    int stdoutpipe[2];
    pid_t processorpid;
        
    pipe(stdoutpipe);

    processorpid = fork();
    if (processorpid < 0) {
        pm_error("fork() of processor process failed.  errno=%d (%s)", 
                 errno, strerror(errno));
    } else if (processorpid == 0) {
        /* The second child */
        close(stdoutpipe[0]);

        execProgram(shellCommand, stdinFd, stdoutpipe[1]);

        close(stdinFd);
        close(stdoutpipe[1]);
        pm_error("INTERNAL ERROR: execProgram() returns.");
    } else {
        /* The parent */
        close(stdoutpipe[1]);
        *stdoutFdP = stdoutpipe[0];
        *pidP = processorpid;
    }
}



static const char *
signalName(unsigned int const signalClass) {

    switch (signalClass) {
    case SIGHUP: /* POSIX.1 */
        return "SIGHUP";
    case SIGINT: /* POSIX.1 */
        return "SIGINT";
    case SIGQUIT: /* POSIX.1 */
        return "SIGQUIT";
    case SIGILL: /* POSIX.1 */
        return "SIGILL";
    case SIGTRAP:
        return "SIGTRAP";
    case SIGABRT: /* POSIX.1 */
        return "SIGABRT";
    case SIGBUS:
        return "SIGBUS";
    case SIGFPE: /* POSIX.1 */
        return "SIGFPE";
    case SIGKILL: /* POSIX.1 */
        return "SIGKILL";
    case SIGUSR1: /* POSIX.1 */
        return "SIGUSR1";
    case SIGSEGV: /* POSIX.1 */
        return "SIGSEGV";
    case SIGUSR2: /* POSIX.1 */
        return "SIGUSR2";
    case SIGPIPE: /* POSIX.1 */
        return "SIGPIPE";
    case SIGALRM: /* POSIX.1 */
        return "SIGALRM";
    case SIGTERM: /* POSIX.1 */
        return "SIGTERM";
    case SIGCHLD: /* POSIX.1 */
        return "SIGCHLD";
    case SIGCONT: /* POSIX.1 */
        return "SIGCONT";
    case SIGSTOP: /* POSIX.1 */
        return "SIGSTOP";
    case SIGTSTP: /* POSIX.1 */
        return "SIGTSTP";
    case SIGTTIN: /* POSIX.1 */
        return "SIGTTIN";
    case SIGTTOU: /* POSIX.1 */
        return "SIGTTOU";
    case SIGXCPU:
        return "SIGXCPU";
    case SIGXFSZ:
        return "SIGXFSZ";
    case SIGVTALRM:
        return "SIGVTALRM";
    case SIGPROF:
        return "SIGPROF";
    case SIGSYS:
        return "SIGSYS";
    default:
        return "???";

        /* There are various other signal classes on some systems, but
           not defined by POSIX and not on at least one system we
           know of for which someone wanted to compile Netpbm.  The
           list includes: SIGPWR, SIGLOST, SIGINFO, SIGRTxx,
           SIGURG, SIGWINCH, SIGIO.
        */
    }
}



static void
cleanupProcessorProcess(pid_t const processorPid) {

    int terminationStatus;
    waitpid(processorPid, &terminationStatus, 0);

    if (WIFEXITED(terminationStatus)) {
        int const exitStatus = WEXITSTATUS(terminationStatus);

        if (exitStatus != 0)
            pm_message("Shell process exited with abnormal exist status %u.  ",
                       exitStatus);
    } else if (WIFSIGNALED(terminationStatus)) {
        pm_message("Shell process was killed by a Class %u (%s) signal.",
                   WTERMSIG(terminationStatus),
                   signalName(WTERMSIG(terminationStatus)));
    } else {
        pm_message("Shell process died, but its termination status "
                   "0x%x  doesn't make sense", terminationStatus);
    }
}



static void
cleanupFeederProcess(pid_t const feederPid) {
    int status;

    waitpid(feederPid, &status, 0);

    if (WIFSIGNALED(status)) {
        if (WTERMSIG(status) == SIGPIPE)
            pm_message("WARNING: "
                       "Standard Input feeder process was terminated by a "
                       "SIGPIPE signal because the shell command closed its "
                       "Standard Input before the Standard Input feeder was "
                       "through feeding it.");
        else
            pm_message("WARNING: "
                       "Standard Input feeder was terminated by a Signal %d.",
                       WTERMSIG(status));
    }
    else if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0)
            pm_message("WARNING: "
                       "Standard Input feeder process ended abnormally.  "
                       "exit status = %d", WEXITSTATUS(status));
    } else
        pm_message("WARNING: "
                   "Unrecognized process completion status from "
                   "Standard Input feeder: %d", status);
}



void
pm_system(void stdinFeeder(int, void *),
          void *          const feederParm,
          void stdoutAccepter(int, void *),
          void *          const accepterParm,
          const char *    const shellCommand) {
/*----------------------------------------------------------------------------
   Run a shell and have it run command 'shellCommand'.  Feed its
   Standard Input with a pipe, which is fed by the routine
   'stdinFeeder' with parameter 'feederParm'.  Process its Standard
   Output with the routine 'stdoutAccepter' with parameter 'accepterParm'.

   But if 'stdinFeeder' is NULL, just feed the shell our own Standard
   Input.  And if 'stdoutFeeder' is NULL, just send its Standard Output
   to our own Standard Output.

   Run the program 'progName' with arguments argArray[] (terminated by NULL
   element).  That includes arg0.
-----------------------------------------------------------------------------*/
    /* If 'stdinFeeder' is non-NULL, we create a child process to run
       'stdinFeeder' and create a pipe between from that process as the
       shell's Standard Input.

       If 'stdoutFeeder' is non-NULL, we create a child process to run
       the shell and create a pipe between the shell's Standard Output
       and this process, and then this process runs 'stdoutAccepter'
       to read the data from that pipe.
       
       But if 'stdoutFeeder' is NULL, we just run the shell in this
       process.

       So there can be 1, 2, or 3 processes involved depending on 
       parameters.
    */
    
    int shellStdinFd;
    pid_t feederPid;

    if (stdinFeeder) 
        createPipeFeeder(stdinFeeder, feederParm, &shellStdinFd, &feederPid);
    else {
        shellStdinFd = STDIN;
        feederPid = 0;
    }

    if (stdoutAccepter) {
        int shellStdoutFd;
        pid_t processorPid;

        /* Make a child process to run the shell and pipe back to us its
           Standard Output 
        */
        spawnProcessor(shellCommand, shellStdinFd, 
                       &shellStdoutFd, &processorPid);

        /* The shell process has cloned our 'shellStdinFd'; we have no
           more use for our copy.
        */
        close(shellStdinFd);
        /* Dispose of the stdout from that shell */
        (*stdoutAccepter)(shellStdoutFd, accepterParm);
        close(shellStdoutFd);

        cleanupProcessorProcess(processorPid);
    } else {
        /* Run a child process for the shell that sends its Standard Output
           to our Standard Output
        */
        int const stdinSaveFd = dup(STDIN);
        int rc;

        dup2(shellStdinFd, STDIN);
        
        rc = system(shellCommand);

        close(STDIN);
        dup2(stdinSaveFd, STDIN);
        
        if (rc < 0)
            pm_error("Unable to invoke the shell.  Errno=%d (%s)",
                     errno, strerror(errno));
        else if (rc != 0)
            pm_message("WARNING: Shell process completion code = %d", rc);
    }

    if (feederPid) 
        cleanupFeederProcess(feederPid);
}




void
pm_feed_from_memory(int    const pipeToFeedFd,
                    void * const feederParm) {

    struct bufferDesc * const inputBufferP = feederParm;
    
    FILE * const outfile = fdopen(pipeToFeedFd, "w");
    
    int bytesTransferred;

    /* The following signals (and normally kills) the process with
       SIGPIPE if the pipe does not take all 'size' bytes.
    */
    bytesTransferred = 
        fwrite(inputBufferP->buffer, 1, inputBufferP->size, outfile);

    if (inputBufferP->bytesTransferredP)
        *(inputBufferP->bytesTransferredP) = bytesTransferred;

    fclose(outfile);
}



void
pm_accept_to_memory(int             const pipetosuckFd,
                    void *          const accepterParm ) {

    struct bufferDesc * const outputBufferP = accepterParm;
    
    FILE * const infile = fdopen(pipetosuckFd, "r");

    int bytesTransferred;

    bytesTransferred =
        fread(outputBufferP->buffer, 1, outputBufferP->size, infile);

    fclose(infile);

    if (outputBufferP->bytesTransferredP)
        *(outputBufferP->bytesTransferredP) = bytesTransferred;
}
