#include<arpa/inet.h>
#include<errno.h>
#include<fcntl.h>
#include<limits.h>
#include<stdint.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<sys/un.h>
#include<unistd.h>
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
		hostlen *= hostlen > 0;
		buf[hostlen] = '\0';
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
			log_fmtmsg_full("client requested a non-existant host %s, which resolved to %s", conn->host, buf);
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
		destroyConnection(conn);
	}
	return fail;
}
int unchecked_respond(const char*filename, cpcio_ostream os, pcpcss_http_req res)
{
	int f = 1;
	struct stat fdat;
	FILE*fh = fopen(filename, "rb");
	if(fh != NULL)
	{
		if(stat(filename, &fdat) == 0)
		{
			char lenstr[31];
			sprintf(lenstr, "%zu", fdat.st_size);
			if(cpcss_set_header(res, "content-length", lenstr) == 0)
			{
				char buffer[8192];
				send_headers(buffer, os, res);
				for(size_t r = fread(buffer, 1, sizeof(buffer), fh); r > 0; r = fread(buffer, 1, sizeof(buffer), fh))
				{
					cpcio_wr(os, buffer, r);
				}
				f = 0;
			}
		}
		fclose(fh);
	}
	else if(lstat(filename, &fdat) == 0)
	{
		char link[8192];
		ssize_t cnt = readlink(filename, link, PATH_MAX);
		cnt -= cnt == PATH_MAX;
		link[cnt] = '\0';
		cpcss_free_response(res);
		cpcss_init_http_response(res, 308, NULL);
		cpcss_set_header(res, "Location", link);
		send_headers(link, os, res);
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
			const char*period = strrchr(first, '.');
			int check = cpcss_set_header(&res, "connection", "Close");
			const char*mimetype = "text/plain";
			if(period != NULL)
			{
				mimetype = mimetype_get(period + 1);
			}
			check += cpcss_set_header(&res, "content-type", mimetype);
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
						destroy = !proxyres;
						if(!proxyres)
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
						default_response(conn->os, defaultresp);
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
		destroyConnection(conn);
	}
	return fail;
}
void default_response(cpcio_ostream os, int status)
{
	char rstr[] = "HTTP/1.1    \r\ncontent-length: 3\r\ncontent-type: text/plain\r\nconnection: close\r\n\r";
	sprintf(rstr + 9, "%d", status);
	rstr[12] = '\r';
	log_fmtmsg_full("Sending the following response\n\n%s\n", rstr);
	cpcio_putln_os(os, rstr);
	cpcio_putint_os(os, status);
	cpcio_flush_os(os);
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
	saddr.sun_family = AF_UNIX;
	strcpy(saddr.sun_path, socketpath);
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd > 0)
	{
		int res = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
		if(res == 0)
		{
			uint32_t totallen = connection->bodylen + pathlen + 1;
			uint32_t address = cpcss_address_n(connection->client);
			totallen = htonl(totallen);
			log_fmtmsg_full("fetching file %s from proxy socket %d\n", path, fd);
			write(fd, &address, sizeof(address));
			write(fd, &totallen, sizeof(totallen));
			write(fd, path, pathlen + 1);
			if(connection->bodylen)
			{
				char buffer[8192];
				size_t bufsz = sizeof(buffer);
				size_t tot = 0;
				bufsz = bufsz > connection->bodylen ? connection->bodylen : bufsz;
				size_t bc = cpcio_rd(connection->is, buffer, bufsz);
				for(tot += bc; bc > 0 && tot < connection->bodylen; bc = cpcio_rd(connection->is, buffer, bufsz))
				{
					write(fd, buffer, bc);
					tot += bc;
				}
				if(bc)
				{
					write(fd, buffer, bc);
				}
			}
			registerEvent(fd, connection->ssl, connection->is, connection->os, connection->client);
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
	if(!succ)
	{
		close(fd);
	}
	return succ;
}
ssize_t deepreadlink(const char*restrict path,char*restrict buf,size_t size)
{
	char otherbuf[PATH_MAX];
	ssize_t cnt = readlink(path, buf, size), lastcnt = -1;
	char*from = buf;
	char*to = otherbuf;
	char*tmp;
	while(cnt > 0)
	{
		from[cnt] = '\0';
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
void destroyConnection(const struct Connection*connection)
{
	cpcio_close_ostream(connection->os);
	cpcio_close_istream(connection->is);
	SSL_shutdown(connection->ssl);
	SSL_free(connection->ssl);
	cpcss_close_server(connection->client);
}
