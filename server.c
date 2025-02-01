#include<signal.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/select.h>
#include<cpcss_socket.h>
#include"accepter.h"
#include"logger/logger.h"
#include"logger/format.h"
#include"mimetype.h"
char*load_hosts(const char*fn)
{
	char*hostls = NULL;
	FILE*fh = fopen(fn, "rb");
	if(fh != NULL)
	{
		fseek(fh, 0, SEEK_END);
		long len = ftell(fh);
		fseek(fh, 0, SEEK_SET);
		hostls = malloc(len + 1);
		if(hostls != NULL)
		{
			char*it = hostls;
			for(; fscanf(fh, "%s", it) == 1; it += strlen(it) + 1);
			*it = '\0';
		}
		fclose(fh);
	}
	return hostls;
}
int accept_loop(const char*hosts,const char**files,short unsigned port)
{
	int failed = 0;
	SSL_CTX*context = init_ctx(files[0], files[1]);
	struct timeval quartersec = {0, 250000};
	struct timeval timeout = quartersec;
	fd_set fds;
	cpcss_socket sock = cpcss_open_server(port);
	if(sock == NULL)
	{
		log_sys_error("creating socket failed");
		failed = 1;
	}
	else
	{
		int rsock = *cpcss_get_raw_socket(sock);
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(rsock, &fds);
		cpcss_socket cli;
		int rd = select(rsock + 1, &fds, NULL, NULL, &timeout);
		while(rd >= 0 && !FD_ISSET(0, &fds))
		{
			if(FD_ISSET(rsock, &fds))
			{
				cli = cpcss_accept_client(sock);
				if(cli)
				{
					log_fmtmsg_full("accepted a client on file descriptor %d\n", *cpcss_get_raw_socket(cli));
					handle_client(context, cli, hosts);
					cpcss_close_server(cli);
				}
				else
				{
					log_sys_error("Accepting a client failed");
				}
			}
			FD_ZERO(&fds);
			FD_SET(0, &fds);
			FD_SET(rsock, &fds);
			timeout = quartersec;
			rd = select(rsock + 1, &fds, NULL, NULL, &timeout);
		}
		if(rd < 0)
		{
			log_sys_error("selecting on the socket and stdin failed");
		}
		getchar();
	}
	SSL_CTX_free(context);
	return failed;
}
int main(int argl, char**argv)
{
	int failed = 0;
	const char*hostfile = "hosts.txt";
	const char*logfile = "output.log";
	const char*typefile = "mimetype.txt";
	const char*keyfile = "key.pem";
	const char*cerfile = "cert.pem";
	short unsigned port = 443;
	puts("01");
	switch(argl)
	{
	case 5:
		typefile = argv[4];
	case 4:
		logfile = argv[3];
	case 3:
		hostfile = argv[2];
	case 2:
		port = atoi(argv[1]) & 0xffff;
	}
	char*hostlist = load_hosts(hostfile);
	if(hostlist != NULL)
	{
		if(initialize_logger(logfile) == 0)
		{
			if(inittypes(typefile) == 0)
			{
				const char*arr[] = {keyfile, cerfile};
				signal(SIGPIPE, SIG_IGN);
				failed = accept_loop(hostlist, arr, port);
				freetypes();
			}
			else
			{
				perror("could not initialize mimetypes");
				failed = 1;
			}
			finalize_logger();
		}
		else
		{
			perror("initialize_logger failed");
			failed = 1;
		}
	}
	else
	{
		perror("loading host names failed");
		failed = 1;
	}
	return failed;
}
