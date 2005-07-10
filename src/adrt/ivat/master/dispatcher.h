#ifndef _IVAT_DISPATCHER_H
#define _IVAT_DISPATCHER_H

#include "cdb.h"

extern	void	ivat_dispatcher_init();
extern	void	ivat_dispatcher_free();
extern	void	ivat_dispatcher_generate(common_db_t *db, void *data, int data_len);


#endif
