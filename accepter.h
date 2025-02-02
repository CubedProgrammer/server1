#ifndef Included_accepter_h
#define Included_accepter_h
#include<openssl/ssl.h>
SSL_CTX* init_ctx(const char*key,const char*cert);
void handle_client(SSL_CTX* ctx,cpcss_socket client,const char*restrict proxy,const char*restrict hostls);
#endif
