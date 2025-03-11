#ifndef Included_fetch_h
#define Included_fetch_h
#include<cpcss_http.h>
#include<openssl/ssl.h>
#include"server.h"
struct Connection
{
	SSL*ssl;
	cpcss_socket client;
	cpcio_istream is;
	cpcio_ostream os;
	const char*host;
	const char*path;
	size_t bodylen;
};
int servefile(const struct ServerData*server, const struct Connection*conn);
int unchecked_respond(const char*filename, cpcio_ostream os, pcpcss_http_req res);
int respond(const struct ServerData*server,const struct Connection*conn,const char*first,const char*last);
void default_response(cpcio_ostream os, int status);
int redirect(cpcio_ostream os, const char*destination);
void send_headers(char*buffer, cpcio_ostream os, cpcpcss_http_req res);
int fetch_dynamic(const struct Connection*connection, const char*restrict socketpath, const char*restrict path, size_t pathlen);
ssize_t deepreadlink(const char*restrict path,char*restrict buf,size_t size);
int validate(const char*path);
void destroyConnection(const struct Connection*connection);
#endif
