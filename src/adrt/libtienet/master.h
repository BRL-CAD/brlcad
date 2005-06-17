#ifndef _TIENET_MASTER_H
#define _TIENET_MASTER_H

#include <inttypes.h>

extern void   	tienet_master_init(int port, void RDC(void *res_buf, int res_len), char *list, char *exec, int buffer_size, int ver_key);
extern void	tienet_master_free();
extern void	tienet_master_prep(void *app_data, int app_size);
extern void	tienet_master_push(void *data, int size);
extern void	tienet_master_begin();
extern void	tienet_master_end();
extern void	tienet_master_wait();
extern void	tienet_master_shutdown();
extern void	tienet_master_broadcast(void *mesg, int mesg_len);
extern uint64_t	tienet_master_get_rays_fired();

extern int	tienet_master_socket_num;
extern int	tienet_master_transfer;

#endif
