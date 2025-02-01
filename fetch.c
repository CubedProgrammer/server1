#include<errno.h>
#include<fcntl.h>
#include<limits.h>
#include<sys/stat.h>
#include<cpcss_http.h>
#include"utils/str.h"
#include"fetch.h"
#include"logger/logger.h"
#include"logger/format.h"
#include"mimetype.h"
int servefile(cpcio_ostream os,const char*restrict hostls,const char*restrict host,const char*restrict path)
{
	int fail = 0;
	if(validate(path))
	{
		if(*double_null_list_search(hostls, host) != '\0')
		{
			char buf[PATH_MAX];
			size_t hostlen = strlen(host), pathlen = strlen(path);
			memcpy(buf, host, hostlen);
			if(path[0] != '/')
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
				memcpy(buf + hostlen, path, pathlen + 1);
				struct stat fdat;
				if(stat(buf, &fdat) == 0 && S_ISDIR(fdat.st_mode))
				{
					if(hostlen + pathlen + 11 < sizeof(buf))
					{
						log_message_full("client requested a directory, fetching the corresponding index.html");
						strcpy(buf + hostlen + pathlen, "/index.html");
						pathlen += 11;
					}
					else
					{
						log_message_full("client requested a path that was just under the length limit, but it was a directory");
						fail = 1;
					}
				}
				if(!fail)
				{
					fail = respond(os, buf, buf + hostlen + pathlen);
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
	return fail;
}
int unchecked_respond(const char*filename, cpcio_ostream os, pcpcss_http_req res)
{
	int f = 1;
	FILE*fh = fopen(filename, "rb");
	if(fh != NULL)
	{
		char buffer[8192];
		cpcss_response_str(buffer, res);
		cpcio_puts_os(os, buffer);
		cpcio_flush_os(os);
		for(size_t r = fread(buffer, 1, sizeof(buffer), fh); r > 0; r = fread(buffer, 1, sizeof(buffer), fh))
		{
			cpcio_wr(os, buffer, r);
		}
		f = 0;
		fclose(fh);
	}
	return f;
}
int respond(cpcio_ostream os,const char*first,const char*last)
{
	cpcss_http_req res;
	int fail = cpcss_init_http_response(&res, 200, NULL);
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
				if(unchecked_respond(first, os, &res))
				{
					int defaultresp = 0;
					log_sys_error("opening file failed, sending error response");
					cpcss_set_header(&res, "content-type", "text/html");
					if(errno == ENOENT)
					{
						res.rru.res = 404;
						if(unchecked_respond("404.html", os, &res))
						{
							defaultresp = 404;
						}
					}
					else
					{
						res.rru.res = 403;
						if(unchecked_respond("403.html", os, &res))
						{
							defaultresp = 403;
						}
					}
					if(defaultresp)
					{
						char rstr[] = "HTTP/1.1    \r\ncontent-type: text/plain\r\nconnection: close\r\n\r";
						sprintf(rstr + 9, "%d", defaultresp);
						rstr[12] = '\r';
						log_fmtmsg_full("Sending the following response\n\n%s\n", rstr);
						cpcio_putln_os(os, rstr);
						cpcio_putint_os(os, defaultresp);
						cpcio_flush_os(os);
					}
					fail = 1;
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
	return fail;
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
