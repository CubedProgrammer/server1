#include<arpa/inet.h>
#include<errno.h>
#include<fcntl.h>
#include<limits.h>
#include<stdint.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<sys/un.h>
#include<cpcss_http.h>
#include"deloop.h"
#include"fetch.h"
#include"logger/logger.h"
#include"logger/format.h"
#include"mimetype.h"
#include"utils/str.h"
int servefile(const struct ServerData*server, const struct Connection*conn)
{
	int fail = 0;
	char destroy = 1;
	if(validate(conn->path))
	{
		char buf[PATH_MAX];
		size_t pathlen = strlen(conn->path);
		ssize_t hostlen = deepreadlink(conn->host, buf, sizeof(buf));
		if(hostlen > 0)
		{
			buf[hostlen] = '\0';
		}
		if(hostlen > 0 && *double_null_list_search(server->hostlist, buf) != '\0')
		{
			if(conn->path[0] != '/')
			{
				buf[hostlen++] = '/';
			}
			if(hostlen + pathlen + 1 > sizeof(buf))
			{
				log_message_full("client requested a path that is far too long");
				fail = 1;
			}
			else
			{
				memcpy(buf + hostlen, conn->path, pathlen + 1);
				struct stat fdat;
				char toredirect = 0;
				if(stat(buf, &fdat) == 0 && S_ISDIR(fdat.st_mode))
				{
					if(hostlen + pathlen + 11 < sizeof(buf))
					{
						if(conn->path[pathlen - 1] == '/')
						{
							log_message_full("client requested a directory, fetching the corresponding index.html");
							strcpy(buf + hostlen + pathlen, "/index.html");
							pathlen += 11;
						}
						else if(hostlen + pathlen + 1 < sizeof(buf))
						{
							strcpy(buf + hostlen + pathlen, "/");
							++pathlen;
							toredirect = 1;
						}
					}
					else
					{
						log_message_full("client requested a path that was just under the length limit, but it was a directory");
						fail = 1;
					}
				}
				if(!fail)
				{
					if(toredirect)
					{
						fail = redirect(conn->os, buf + hostlen);
					}
					else
					{
						fail = respond(server, conn, buf, buf + hostlen + pathlen);
						destroy = 0;
					}
				}
			}
		}
		else
		{
			log_message_full("client requested a non-existant host");
			fail = 1;
		}
	}
	else
	{
		log_message_full("client sent a request to a path with negative depth");
		fail = 1;
	}
	if(destroy)
	{
		cpcio_close_ostream(conn->os);
		cpcio_close_istream(conn->is);
		cpcss_close_server(conn->client);
	}
	return fail;
}
int unchecked_respond(const char*filename, cpcio_ostream os, cpcpcss_http_req res)
{
	int f = 1;
	FILE*fh = fopen(filename, "rb");
	if(fh != NULL)
	{
		char buffer[8192];
		send_headers(buffer, os, res);
		for(size_t r = fread(buffer, 1, sizeof(buffer), fh); r > 0; r = fread(buffer, 1, sizeof(buffer), fh))
		{
			cpcio_wr(os, buffer, r);
		}
		f = 0;
		fclose(fh);
	}
	return f;
}
int respond(const struct ServerData*server,const struct Connection*conn,const char*first,const char*last)
{
	cpcss_http_req res;
	int fail = cpcss_init_http_response(&res, 200, NULL);
	char destroy = 1;
	if(!fail)
	{
		if(access(first, X_OK))
		{
			struct stat fdat;
			const char*period = strrchr(first, '.');
			int check = cpcss_set_header(&res, "connection", "Close");
			const char*mimetype = "text/plain";
			if(period != NULL)
			{
				mimetype = mimetype_get(period + 1);
			}
			check += cpcss_set_header(&res, "content-type", mimetype);
			if(stat(first, &fdat) == 0)
			{
				char lenstr[31];
				sprintf(lenstr, "%zu", fdat.st_size);
				check += cpcss_set_header(&res, "content-length", lenstr);
			}
			fail = check != 0;
			if(!fail)
			{
				if(unchecked_respond(first, conn->os, &res))
				{
					int defaultresp = 0;
					log_sys_error("opening file failed");
					cpcss_set_header(&res, "content-type", "text/html");
					if(errno == ENOENT)
					{
						int proxyres = fetch_dynamic(conn, server->proxyfile, first, last - first);
						if(proxyres)
						{
							destroy = 0;
						}
						else
						{
							res.rru.res = 404;
							if(unchecked_respond("404.html", conn->os, &res))
							{
								defaultresp = 404;
							}
							fail = 1;
						}
					}
					else
					{
						res.rru.res = 403;
						if(unchecked_respond("403.html", conn->os, &res))
						{
							defaultresp = 403;
						}
						fail = 1;
					}
					if(defaultresp)
					{
						char rstr[] = "HTTP/1.1    \r\ncontent-type: text/plain\r\nconnection: close\r\n\r";
						sprintf(rstr + 9, "%d", defaultresp);
						rstr[12] = '\r';
						log_fmtmsg_full("Sending the following response\n\n%s\n", rstr);
						cpcio_putln_os(conn->os, rstr);
						cpcio_putint_os(conn->os, defaultresp);
						cpcio_flush_os(conn->os);
					}
				}
			}
			else
			{
				log_sys_error("setting response headers failed");
				fail = 1;
			}
		}
		cpcss_free_response(&res);
	}
	if(destroy)
	{
		cpcio_close_ostream(conn->os);
		cpcio_close_istream(conn->is);
		cpcss_close_server(conn->client);
	}
	return fail;
}
int redirect(cpcio_ostream os, const char*destination)
{
	cpcss_http_req res;
	char buffer[8192];
	int fail = cpcss_init_http_response(&res, 308, NULL);
	if(!fail)
	{
		cpcss_set_header(&res, "location", destination);
		log_fmtmsg_full("redirecting to %s\n", destination);
		send_headers(buffer, os, &res);
		cpcss_free_response(&res);
	}
	return fail != 0;
}
void send_headers(char*buffer, cpcio_ostream os, cpcpcss_http_req res)
{
	cpcss_response_str(buffer, res);
	cpcio_puts_os(os, buffer);
	cpcio_flush_os(os);
}
int fetch_dynamic(const struct Connection*connection, const char*restrict socketpath, const char*restrict path, size_t pathlen)
{
	struct sockaddr_un saddr;
	int succ = 1;
	char destroy = 1;
	saddr.sun_family = AF_UNIX;
	strcpy(saddr.sun_path, socketpath);
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd > 0)
	{
		int res = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
		if(res == 0)
		{
			uint32_t totallen = connection->bodylen + pathlen + 1;
			totallen = htonl(totallen);
			log_fmtmsg_full("fetching file %s from proxy socket\n", path);
			write(fd, &totallen, sizeof(totallen));
			write(fd, path, pathlen + 1);
			if(connection->bodylen)
			{
				char buffer[8192];
				size_t tot = 0;
				for(size_t bc = cpcio_rd(connection->is, buffer, sizeof(buffer)); bc > 0 && tot < connection->bodylen; bc = cpcio_rd(connection->is, buffer, sizeof(buffer)))
				{
					write(fd, buffer, bc);
					tot += bc;
				}
			}
			registerEvent(fd, connection->is, connection->os, connection->client);
			destroy = 0;
			succ = fd;
		}
		else
		{
			log_sys_error("connecting to unix socket failed");
			succ = 0;
		}
	}
	else
	{
		log_sys_error("creating unix socket failed");
		succ = 0;
	}
	if(destroy)
	{
		cpcio_close_ostream(connection->os);
		cpcio_close_istream(connection->is);
		cpcss_close_server(connection->client);
	}
	if(!succ)
	{
		close(fd);
	}
	return succ;
}
ssize_t deepreadlink(const char*restrict path,char*restrict buf,size_t size)
{
	char otherbuf[PATH_MAX];
	ssize_t cnt = readlink(path, buf, size), lastcnt;
	char*from = buf;
	char*to = otherbuf;
	char*tmp;
	while(cnt > 0)
	{
		lastcnt = cnt;
		cnt = readlink(from, to, size);
		tmp = from;
		from = to;
		to = tmp;
	}
	if(errno == EINVAL)
	{
		cnt = lastcnt;
		if(to == buf)
		{
			memcpy(from, to, cnt);
		}
	}
	return cnt;
}
int validate(const char*path)
{
	static const char up[] = "..";
	int depth = 0;
	const char*it,*last;
	path += *path == '/';
	for(last = it = path; depth >= 0 && *it != '\0'; ++it)
	{
		if(*it == '/' && it - last == 2)
		{
			depth -= (memcmp(last, up, 2) == 0) * 2 - 1;
			last = it + 1;
		}
	}
	return depth >= 0;
}
