#ifndef Included_server_h
#define Included_server_h
#include<cpcss_socket.h>
struct ServerData
{
	char*hostlist;
	unsigned short port;
	const char*hostfile;
	const char*logfile;
	const char*typefile;
	const char*proxyfile;
};
#endif
