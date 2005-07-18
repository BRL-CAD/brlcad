#ifndef _ISST_COMPNET_H
#define _ISST_COMPNET_H

#define ISST_COMPNET_PORT 1983

void isst_compnet_connect(char *host, int port);
void isst_compnet_update(char *string, char status);
void isst_compnet_reset();

#endif
