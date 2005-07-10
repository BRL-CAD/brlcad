#ifndef _IVAT_COMPNET_H
#define _IVAT_COMPNET_H

#define IVAT_COMPNET_PORT 1983

void ivat_compnet_connect(char *host, int port);
void ivat_compnet_update(char *string, char status);
void ivat_compnet_reset();

#endif
