#include<stdio.h>
#include<sys/select.h>
#include<cpcss_sockstream.h>
#include<cpcss_http.h>
#include<openssl/err.h>
#include"accepter.h"
#include"fetch.h"
#include"logger/logger.h"
#include"logger/format.h"
void ssl_init_cpcio_callback(void*i,cpcss_socket s)
{
	SSL*ssl = i;
	SSL_set_fd(ssl, *cpcss_get_raw_socket(s));
}
size_t ssl_i_cpcio_callback(void*i,void*buf,size_t size)
{
	size_t cnt;
	int res = SSL_read_ex(i, buf, size, &cnt);
	return res ? cnt : -1;
}
size_t ssl_o_cpcio_callback(void*i,const void*buf,size_t size)
{
	size_t cnt;
	int res = SSL_write_ex(i, buf, size, &cnt);
	return res ? cnt : -1;
}
int ssl_ready_cpcio_callback(void*i)
{
	return SSL_pending(i) > 0;
}
int ssl_close_cpcio_callback(void*i)
{
	return 0;
}
int check_client_ready(cpcss_socket client)
{
	return 1;
}
SSL_CTX* init_ctx(const char* key,const char* cert)
{
	SSL_CTX*context = SSL_CTX_new(TLS_server_method());
	if(context == NULL)
	{
		ERR_print_errors_fp(stderr);
	}
	else if(SSL_CTX_use_PrivateKey_file(context, key, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
	}
	else if(SSL_CTX_use_certificate_file(context, cert, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
	}
	return context;
}
void handle_client(SSL_CTX*ctx, cpcss_socket client, const char*hostls)
{
	SSL*ssl = SSL_new(ctx);
	if(!SSL_set_fd(ssl, *cpcss_get_raw_socket(client)))
	{
		fputs("could not set file descriptor\n", stderr);
		ERR_print_errors_fp(stderr);
	}
	else
	{
		int fd = SSL_get_fd(ssl);
		struct timeval timeout = {15, 0};
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		int ready = select(fd + 1, &fds, NULL, NULL, &timeout);
		if(ready > 0)
		{
			if(SSL_accept(ssl) <= 0)
			{
				log_header();
				ERR_print_errors_fp(log_file_handle());
				log_message_partial("SSL_accept failed, see above\n");
				log_flush();
			}
			else
			{
				const struct cpcss_transform_io ssl_transformer=
				{
					ssl,
					&ssl_init_cpcio_callback,
					&ssl_i_cpcio_callback,
					&ssl_o_cpcio_callback,
					&ssl_ready_cpcio_callback,
					&ssl_close_cpcio_callback
				};
				cpcio_istream is = cpcss_open_istream_ex(client, &ssl_transformer);
				cpcio_ostream os = cpcss_open_ostream_ex(client, &ssl_transformer);
				cpcss_http_req req;
				cpcio_toggle_buf_is(is);
				cpcio_toggle_buf_os(os);
				int psucc = cpcss_parse_request(is, &req);
				if(psucc == 0)
				{
					char ipstr[17];
					const char*host = cpcss_get_header(&req, "host");
					const char*path = req.rru.req.requrl;
					cpcss_address_s(client, ipstr);
					if(host != NULL)
					{
						log_fmtmsg_full("client %s requested host %s for file %s\n", ipstr, host, path);
						servefile(os, hostls, host, path);
					}
					else
					{
						log_fmtmsg_full("client %s sent a request with no host\n", ipstr);
					}
				}
				else
				{
					log_sys_error("parsing stream failed");
				}
				cpcio_close_ostream(os);
				cpcio_close_istream(is);
			}
		}
		else if(ready < 0)
		{
			log_sys_error("select failed");
		}
		else
		{
			log_message_full("client did not send a message in time");
		}
	}
	SSL_shutdown(ssl);
	SSL_free(ssl);
}
