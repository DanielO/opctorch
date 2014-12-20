/*
 * Open Pixel Control Torch
 *
 * OCP version of message torch (copyright Lukas Zeller)
 * Original: https://community.spark.io/t/messagetorch-torch-fire-animation-with-ws2812-leds-message-display/2551
 *
 * Daniel O'Connor <darius@dons.net.au>
 */

#include <arpa/inet.h>
#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sysexits.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ccan/ciniparser/ciniparser.h>

#include "config.h"
#include "torch.h"

void
usage(const char *argv0)
{
	fprintf(stderr, "%s [-s server:port] [-c config]\n", argv0);
	fprintf(stderr, "\n");
	fprintf(stderr, "Generate message torch to OPC server:port\n");

	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	char *server = NULL;
	const char *argv0, *cause = NULL;
	int sock, rtn, ch;
	struct sockaddr_in saddr;
	struct addrinfo addrhint, *res, *res0;
	struct config_t conf;
	dictionary *ini;
	static struct option longopts[] = {
		{ "config",	required_argument,	NULL, 	'c' },
		{ "server",	required_argument,	NULL,	's' },
		{ NULL,		0,			NULL,	0 }
	};

	argv0 = argv[0];

	default_conf(&conf);

	while ((ch = getopt_long(argc, argv, "c:s:", longopts, NULL)) != -1) {
		switch (ch) {
			case 'c':
				if ((ini = ciniparser_load(optarg)) == NULL)
					exit(EX_DATAERR);
				if (ini2conf(ini, &conf) != 0)
					exit(EX_DATAERR);
				break;

			case 's':
				server = optarg;
				break;

			default:
				usage(argv0);
			}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage(argv0);

	/* Override the server host/port in config from the command line */
	if (server != NULL) {
		conf.srvport = strchr(server, ':');
		conf.srvport[0] = '\0';
		conf.srvport++;
		conf.srvhost = server;
	}

	/* Check a server was specified somewhere */
	if (conf.srvhost == NULL || conf.srvport == NULL) {
		fprintf(stderr, "A server name and port must be specified in the configuration file or on the command line\n");
		exit(EX_DATAERR);
	}

	saddr.sin_family = AF_INET;

	memset(&addrhint, 0, sizeof(addrhint));
	addrhint.ai_family = PF_UNSPEC;
#if 1
	addrhint.ai_socktype = SOCK_STREAM;
	addrhint.ai_protocol = IPPROTO_TCP;
#else
	addrhint.ai_socktype = SOCK_DGRAM;
	addrhint.ai_protocol = IPPROTO_UDP;
#endif
	if ((rtn = getaddrinfo(conf.srvhost, conf.srvport, &addrhint, &res0)) != 0)
		errx(EX_NOHOST, "Unable to resolve host: %s", gai_strerror(rtn));

	sock = -1;
	for (res = res0; res; res = res->ai_next) {
		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock < 0) {
			cause = "create socket";
			continue;
		}

		if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
			cause = "connect";
			close(sock);
			sock = -1;
			continue;
		}
		break; /* Got one */
	}
	if (sock < 0)
		err(EX_NOHOST, "Unable to %s", cause);
	freeaddrinfo(res0);

	if ((rtn = run_torch(sock, &conf)) != 0)
		fprintf(stderr, "Failed to start: %d\n", rtn);

	ciniparser_freedict(ini);
	close(sock);
}
