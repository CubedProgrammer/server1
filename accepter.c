#include<fcntl.h>
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
int ssl_select_cpcio_callback(void**first,void**last,long*ms)
{
	SSL**truefirst = (SSL**)first;
	SSL**truelast = (SSL**)last;
	struct timeval tv = {*ms / 1000, *ms % 1000 * 1000};
	fd_set fds;
	int socket, maxi = 0;
	FD_ZERO(&fds);
	for(SSL**it = truefirst; it != truelast; ++it)
	{
		socket = SSL_get_fd(*it);
		maxi = socket > maxi ? socket : maxi;
		FD_SET(socket, &fds);
	}
	int ready = select(maxi + 1, &fds, NULL, NULL, &tv);
	for(SSL**it = truefirst; it != truelast; ++it)
	{
		socket = SSL_get_fd(*it);
		if(!FD_ISSET(socket, &fds))
		{
			*it = NULL;
		}
	}
	*ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return ready;
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
void handle_client(SSL_CTX*ctx, cpcss_socket client, const struct ServerData*server)
{
	SSL*ssl = SSL_new(ctx);
	char destroy = 1;
	char ipstr[17];
	cpcss_address_s(client, ipstr);
	log_header();
	log_fmtmsg_partial("client of address %s has established connection\n", ipstr);
	if(!SSL_set_fd(ssl, *cpcss_get_raw_socket(client)))
	{
		fputs("could not set file descriptor\n", stderr);
		ERR_print_errors_fp(stderr);
	}
	else
	{
		int fd = SSL_get_fd(ssl);
		struct timeval timeout = {3, 0};
		fd_set fds, cpy;
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
		FD_ZERO(&fds);
		int ready = 0;
		int acceptret = 0;
		int errorcode = SSL_ERROR_WANT_READ;
		while(ready >= 0 && (timeout.tv_sec != 0 || timeout.tv_usec != 0) && acceptret < 1 && (errorcode == SSL_ERROR_WANT_READ || errorcode == SSL_ERROR_WANT_WRITE))
		{
			FD_SET(fd, &fds);
			cpy = fds;
			ready = select(fd + 1, &fds, &cpy, NULL, &timeout);
			if(ready > 0)
			{
				acceptret = SSL_accept(ssl);
				if(acceptret < 1)
				{
					errorcode = SSL_get_error(ssl, acceptret);
				}
			}
		}
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
		if(acceptret == 1)
		{
			log_message_partial("that client has completed SSL handshake\n");
			log_flush();
			const struct cpcss_transform_io ssl_transformer=
			{
				ssl,
				&ssl_init_cpcio_callback,
				&ssl_i_cpcio_callback,
				&ssl_o_cpcio_callback,
				&ssl_ready_cpcio_callback,
				&ssl_select_cpcio_callback,
				&ssl_close_cpcio_callback
			};
			cpcio_istream is = cpcss_open_istream_ex(client, &ssl_transformer);
			cpcio_ostream os = cpcss_open_ostream_ex(client, &ssl_transformer);
			cpcss_http_req req;
			cpcio_toggle_buf_is(is);
			cpcio_toggle_buf_os(os);
			int psucc = cpcss_parse_request_ex(is, &req, 5000, 8192, NULL);
			log_header();
			log_message_partial("request has been parsed");
			if(psucc == 0)
			{
				const char*host = cpcss_get_header(&req, "host");
				const char*path = req.rru.req.requrl;
				if(host != NULL)
				{
					struct Connection connection = {ssl, client, is, os, host, path, 0};
					const char*contlen = cpcss_get_header(&req, "content-length");
					if(contlen != NULL)
					{
						connection.bodylen = strtoul(contlen, NULL, 10);
					}
					log_fmtmsg_partial("client %s requested host %s for file %s\n\n", ipstr, host, path);
					destroy = 0;
					servefile(server, &connection);
				}
				else
				{
					log_fmtmsg_partial("client %s sent a request with no host\n\n", ipstr);
					cpcio_close_ostream(os);
					cpcio_close_istream(is);
				}
			}
			else
			{
				log_sys_error_partial("parsing stream failed");
				log_cstr("\n");
				cpcio_close_ostream(os);
				cpcio_close_istream(is);
			}
		}
		else if(ready < 0)
		{
			log_sys_error_partial("select failed");
			log_cstr("\n");
		}
		else if(timeout.tv_sec == 0 && timeout.tv_usec == 0)
		{
			log_message_partial("client did not send a message in time\n");
		}
		else
		{
			ERR_print_errors_fp(log_file_handle());
			log_message_partial("SSL_accept failed, see above\n");
		}
	}
	log_flush();
	if(destroy)
	{
		SSL_shutdown(ssl);
		SSL_free(ssl);
		cpcss_close_server(client);
	}
}
