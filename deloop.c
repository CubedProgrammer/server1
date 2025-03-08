#include<arpa/inet.h>
#include<stdio.h>
#include<sys/select.h>
#include<unistd.h>
#include<cpcio_istream.h>
#include<cpcio_ostream.h>
#include<cpcss_http.h>
#include"fetch.h"
#include"logger/logger.h"
fd_set DELOOP_SELECT_FDS;
cpcio_ostream DELOOP_OUTPUT_STREAMS[1024];
cpcio_istream DELOOP_INPUT_STREAMS[1024];
cpcss_socket DELOOP_SOCKETS[1024];
int registerEvent(int fd, cpcio_istream is, cpcio_ostream os, cpcss_socket socket)
{
	int failed = 1;
	if(fd >= 0 && fd < 1024)
	{
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
int respondDynamic(int dynamicstart, const fd_set*fdset)
{
	int failed = 0;
	int maxi = sizeof(DELOOP_OUTPUT_STREAMS) / sizeof(cpcio_ostream);
	cpcss_http_req res;
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
				switch(type)
				{
					case'F':
						cpcss_init_http_response(&res, 200, NULL);
						sprintf(buffer, "%u", ntohl(len));
						cpcss_set_header(&res, "content-length", buffer);
						cpcss_set_header(&res, "content-type", "text/html");
						send_headers(buffer, DELOOP_OUTPUT_STREAMS[fd], &res);
						cpcss_free_response(&res);
					case'R':
						for(size_t bc = read(fd, buffer, sizeof(buffer)); bc > 0; bc = read(fd, buffer, sizeof(buffer)))
						{
							cpcio_wr(DELOOP_OUTPUT_STREAMS[fd], buffer, bc);
						}
						break;
					default:
						break;
				}
			}
			else
			{
				log_sys_error("could not read one byte from unix socket");
				failed = 1;
			}
			close(fd);
			cpcio_close_ostream(DELOOP_OUTPUT_STREAMS[fd]);
			cpcio_close_istream(DELOOP_INPUT_STREAMS[fd]);
			cpcss_close_server(DELOOP_SOCKETS[fd]);
			DELOOP_OUTPUT_STREAMS[fd] = NULL;
			DELOOP_INPUT_STREAMS[fd] = NULL;
			DELOOP_SOCKETS[fd] = NULL;
		}
	}
	return failed;
}
