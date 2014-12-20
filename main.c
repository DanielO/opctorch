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
#include <sysexits.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "config.h"
#include "torch.h"

void
usage(const char *argv0)
{
	fprintf(stderr, "%s server:port\n", argv0);
	fprintf(stderr, "\n");
	fprintf(stderr, "Generate message torch to OPC server:port\n");
}

int
main(int argc, char **argv)
{
	char *srvhost, *srvport;
	const char *cause = NULL;
	int sock, rtn;
	struct sockaddr_in saddr;
	struct addrinfo addrhint, *res, *res0;
	struct config_t conf;

	if (argc != 2) {
		usage(argv[0]);
		exit(EX_USAGE);
	}

	srvport = strchr(argv[1], ':');
	srvport[0] = '\0';
	srvport++;
	srvhost = argv[1];

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
	if ((rtn = getaddrinfo(srvhost, srvport, &addrhint, &res0)) != 0)
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

	default_conf(&conf);
	if ((rtn = run_torch(sock, &conf)) != 0)
		fprintf(stderr, "Failed to start: %d\n", rtn);

	close(sock);
}
