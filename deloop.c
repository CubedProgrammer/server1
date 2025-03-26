#include<arpa/inet.h>
#include<stdio.h>
#include<sys/select.h>
#include<unistd.h>
#include<cpcio_istream.h>
#include<cpcio_ostream.h>
#include<cpcss_http.h>
#include"deloop.h"
#include"fetch.h"
#include"logger/format.h"
#include"logger/logger.h"
fd_set DELOOP_SELECT_FDS;
cpcio_ostream DELOOP_OUTPUT_STREAMS[1024];
cpcio_istream DELOOP_INPUT_STREAMS[1024];
cpcss_socket DELOOP_SOCKETS[1024];
SSL*DELOOP_SSL[1024];
int registerEvent(int fd, SSL*ssl, cpcio_istream is, cpcio_ostream os, cpcss_socket socket)
{
	int failed = 1;
	if(fd >= 0 && fd < 1024)
	{
		DELOOP_SSL[fd] = ssl;
		DELOOP_INPUT_STREAMS[fd] = is;
		DELOOP_OUTPUT_STREAMS[fd] = os;
		DELOOP_SOCKETS[fd] = socket;
		FD_SET(fd, &DELOOP_SELECT_FDS);
		failed = 0;
	}
	return failed;
}
int selectEvent(int dynamicstart, fd_set*fdset)
{
	struct timeval tv = {0, 500000};
	*fdset = DELOOP_SELECT_FDS;
	int maxi = sizeof(DELOOP_OUTPUT_STREAMS) / sizeof(cpcio_ostream);
	for(; maxi > dynamicstart && DELOOP_OUTPUT_STREAMS[maxi-1] == NULL; --maxi);
	return select(maxi, fdset, NULL, NULL, &tv);
}
void finishDynamic(int dynamicstart)
{
	static const char response[] = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 3\r\n\r\n500";
	for(int fd = dynamicstart; fd < 1024; ++fd)
	{
		if(FD_ISSET(fd, &DELOOP_SELECT_FDS))
		{
			cpcio_wr(DELOOP_OUTPUT_STREAMS[fd], response, sizeof(response) - 1);
			removeEvent(fd);
		}
	}
}
int respondDynamic(int dynamicstart, const fd_set*fdset)
{
	int failed = 0;
	int maxi = sizeof(DELOOP_OUTPUT_STREAMS) / sizeof(cpcio_ostream);
	char buffer[8192];
	char type;
	uint32_t len;
	size_t c;
	for(int fd = dynamicstart; fd < maxi; ++fd)
	{
		if(FD_ISSET(fd, fdset))
		{
			c = read(fd, &type, sizeof(type));
			c += read(fd, &len, sizeof(len));
			if(c == 5)
			{
				cpcss_http_req res;
				log_fmtmsg_full("dynamic page on file descriptor %i has finished, responding now\n", fd);
				switch(type)
				{
					case'H':
					case'F':
						cpcss_init_http_response(&res, 200, NULL);
						sprintf(buffer, "%u", ntohl(len));
						switch(type)
						{
							case'H':
								cpcss_set_header(&res, "content-type", "text/html");
								break;
							default:
								cpcss_set_header(&res, "content-type", "text/plain");
								break;
						}
						cpcss_set_header(&res, "content-length", buffer);
						send_headers(buffer, DELOOP_OUTPUT_STREAMS[fd], &res);
						cpcss_free_response(&res);
					case'R':
						for(size_t bc = read(fd, buffer, sizeof(buffer)), total = 0; total < len && bc > 0; bc = read(fd, buffer, sizeof(buffer)))
						{
							total += cpcio_wr(DELOOP_OUTPUT_STREAMS[fd], buffer, bc);
						}
						break;
					default:
						cpcss_init_http_response(&res, 404, NULL);
						cpcss_set_header(&res, "connection", "close");
						cpcss_set_header(&res, "content-type", "text/html");
						if(unchecked_respond("404.html", DELOOP_OUTPUT_STREAMS[fd], &res))
						{
							default_response(DELOOP_OUTPUT_STREAMS[fd], 404);
						}
						cpcss_free_response(&res);
						break;
				}
			}
			else
			{
				log_sys_error("could not read five bytes from unix socket");
				failed = 1;
			}
			removeEvent(fd);
		}
	}
	return failed;
}
void removeEvent(int fd)
{
	close(fd);
	cpcio_close_ostream(DELOOP_OUTPUT_STREAMS[fd]);
	cpcio_close_istream(DELOOP_INPUT_STREAMS[fd]);
	SSL_shutdown(DELOOP_SSL[fd]);
	SSL_free(DELOOP_SSL[fd]);
	cpcss_close_server(DELOOP_SOCKETS[fd]);
	FD_CLR(fd, &DELOOP_SELECT_FDS);
	DELOOP_OUTPUT_STREAMS[fd] = NULL;
	DELOOP_INPUT_STREAMS[fd] = NULL;
	DELOOP_SSL[fd] = NULL;
	DELOOP_SOCKETS[fd] = NULL;
}
