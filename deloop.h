#ifndef Included_deloop_h
#define Included_deloop_h
#include<sys/select.h>
#include<cpcio_istream.h>
#include<cpcio_ostream.h>
#include<cpcss_socket.h>
#include<openssl/ssl.h>
int registerEvent(int fd, SSL*ssl, cpcio_istream is, cpcio_ostream os, cpcss_socket socket);
int selectEvent(int dynamicstart, fd_set*fdset);
void finishDynamic(int dynamicstart);
int respondDynamic(int dynamicstart, const fd_set*fdset);
void removeEvent(int fd);
#endif
