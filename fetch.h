#ifndef Included_fetch_h
#define Included_fetch_h
#include<cpcss_http.h>
#include"server.h"
struct Connection
{
	cpcio_istream is;
	cpcio_ostream os;
	const char*host;
	const char*path;
	size_t bodylen;
};
int servefile(const struct ServerData*server, const struct Connection*conn);
int respond(const struct ServerData*server,const struct Connection*conn,const char*first,const char*last);
int redirect(cpcio_ostream os, const char*destination);
void send_headers(char*buffer, cpcio_ostream os, cpcpcss_http_req res);
int fetch_dynamic(const struct Connection*connection, const char*restrict socketpath, const char*restrict path, size_t pathlen);
ssize_t deepreadlink(const char*restrict path,char*restrict buf,size_t size);
int validate(const char*path);
#endif
