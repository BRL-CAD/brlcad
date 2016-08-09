#ifndef PM_SYSTEM_H_INCLUDED
#define PM_SYSTEM_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif


void
pm_system(void                  stdinFeeder(int, void *),
          void *          const feederParm,
          void                  stdoutAccepter(int, void *),
          void *          const accepterParm,
          const char *    const shellCommand);


struct bufferDesc {
    /* This is just a parameter for the routines below */
    unsigned int    size;
    unsigned char * buffer;
    unsigned int *  bytesTransferredP;
};


/* The following are a Standard Input feeder and a Standard Output accepter
   for pm_system().  
*/
void
pm_feed_from_memory(int    const pipeToFeedFd,
                    void * const feederParm);

void
pm_accept_to_memory(int    const pipetosuckFd,
                    void * const accepterParm);

#ifdef __cplusplus
}
#endif

#endif
