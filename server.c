#include<signal.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/select.h>
#include"accepter.h"
#include"deloop.h"
#include"logger/logger.h"
#include"logger/format.h"
#include"mimetype.h"
#include"server.h"
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
int accept_loop(const struct ServerData*s, const char**files,short unsigned port)
{
	int failed = 0;
	SSL_CTX*context = init_ctx(files[0], files[1]);
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
		registerEvent(STDIN_FILENO, NULL, NULL, NULL, NULL);
		registerEvent(rsock, NULL, NULL, NULL, NULL);
		cpcss_socket cli;
		int rd = selectEvent(rsock + 1, &fds);
		while(rd >= 0 && !FD_ISSET(0, &fds))
		{
			if(respondDynamic(rsock + 1, &fds))
			{
				log_message_full("Responding to open dynamic clients failed.");
			}
			if(FD_ISSET(rsock, &fds))
			{
				cli = cpcss_accept_client(sock);
				if(cli)
				{
					log_fmtmsg_full("accepted a client on file descriptor %d\n", *cpcss_get_raw_socket(cli));
					handle_client(context, cli, s);
				}
				else
				{
					log_sys_error("Accepting a client failed");
				}
			}
			FD_ZERO(&fds);
			registerEvent(STDIN_FILENO, NULL, NULL, NULL, NULL);
			registerEvent(rsock, NULL, NULL, NULL, NULL);
			rd = selectEvent(rsock + 1, &fds);
		}
		if(rd < 0)
		{
			log_sys_error("selecting on the socket and stdin failed");
		}
		else if(respondDynamic(rsock + 1, &fds))
		{
			log_message_full("Responding to remaining open dynamic clients failed.");
		}
		finishDynamic(rsock + 1);
		getchar();
		cpcss_close_server(sock);
	}
	SSL_CTX_free(context);
	return failed;
}
int main(int argl, char**argv)
{
	int failed = 0;
	struct ServerData server = {NULL, 443, "hosts.txt", "output.log", "mimetype.txt", "dynamic"};
	const char*keyfile = "key.pem";
	const char*cerfile = "cert.pem";
	puts("01");
	switch(argl)
	{
	default:
		printf("%d unused arguments\n", argl - 6);
	case 6:
		server.proxyfile = argv[5];
	case 5:
		server.typefile = argv[4];
	case 4:
		server.logfile = argv[3];
	case 3:
		server.hostfile = argv[2];
	case 2:
		server.port = atoi(argv[1]) & 0xffff;
	case 1:
		break;
	}
	server.hostlist = load_hosts(server.hostfile);
	if(server.hostlist != NULL)
	{
		puts("Allowing the following hostnames.");
		for(const char*it = server.hostlist; *it != '\0'; it += strlen(it) + 1)
		{
			puts(it);
		}
		if(initialize_logger(server.logfile) == 0)
		{
			if(inittypes(server.typefile) == 0)
			{
				const char*arr[] = {keyfile, cerfile};
				signal(SIGPIPE, SIG_IGN);
				failed = accept_loop(&server, arr, server.port);
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
		free(server.hostlist);
	}
	else
	{
		perror("loading host names failed");
		failed = 1;
	}
	return failed;
}
