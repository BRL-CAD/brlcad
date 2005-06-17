#ifndef _TIENET_UTIL_H
#define _TIENET_UTIL_H


#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>


typedef struct tienet_sem_s {
  int val;
  pthread_mutex_t mut;
  pthread_cond_t cond;
} tienet_sem_t;


extern	void	tienet_flip(void *src, void *dest, int size);
extern	int	tienet_send(int socket, void *data, int size, int flip);
extern	int	tienet_recv(int socket, void *data, int size, int flip);

extern	void	tienet_sem_init(tienet_sem_t *sem, int val);
extern	void	tienet_sem_free(tienet_sem_t *sem);
extern	void	tienet_sem_wait(tienet_sem_t *sem);
extern	void	tienet_sem_post(tienet_sem_t *sem);

#endif
