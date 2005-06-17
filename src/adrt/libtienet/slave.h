#ifndef _TIENET_SLAVE_H
#define _TIENET_SLAVE_H


#include "tie.h"


extern	void	tienet_slave_init(int port, char *host, void fcb_init(tie_t *tie, void *app_data, int app_size),
                                                        void fcb_work(tie_t *tie, void *data, int size, void **res_data, int *res_size),
                                                        void fcb_free(void),
                                                        void fcb_mesg(void *mesg, int mesg_len),
                                                        int ver_key);
extern	void	tienet_slave_free();
extern	short	tienet_endian;


#endif
