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
#if 0
	int i;
	uint8_t buf[4 + NLEDS * 3];
#endif
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
#if 0
	memset(buf, 0, sizeof(buf));

	i = 0;
	buf[i++] = 0;			/* Channel (0 == all) */
	buf[i++] = 0;			/* Command (0 = set LEDs) */
	buf[i++] = (NLEDS * 3) >> 8;	/* Length of data to follow (MSB) */
	buf[i++] = (NLEDS * 3) & 0xff;	/* Length of data to follow (LSB) */
	for (n = 0; n < NLEDS; n++) {
		if (n % 3 == 0) {
			buf[i++] = 255;	/* Red */
			buf[i++] = 0;	/* Green */
			buf[i++] = 0;	/* Blue */
		}
		if (n % 3 == 1) {
			buf[i++] = 0;	/* Red */
			buf[i++] = 255;	/* Green */
			buf[i++] = 0;	/* Blue */
		}

		if (n % 3 == 2) {
			buf[i++] = 0;	/* Red */
			buf[i++] = 0;	/* Green */
			buf[i++] = 255;	/* Blue */
		}
	}
	if ((rtn = send(sock, buf, sizeof(buf), 0)) < 0)
		err(EX_PROTOCOL, "Unable to send data");

	fprintf(stderr, "Sent %d bytes\n", rtn);
#else
	default_conf(&conf);
	if ((rtn = run_torch(sock, &conf)) != 0)
		fprintf(stderr, "Failed to start: %d\n", rtn);
#endif
	close(sock);
}
