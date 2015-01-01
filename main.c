/*
 * Open Pixel Control Torch
 *
 * OCP version of message torch (copyright Lukas Zeller)
 * Original: https://community.spark.io/t/messagetorch-torch-fire-animation-with-ws2812-leds-message-display/2551
 *
 * Daniel O'Connor <darius@dons.net.au>
 */

#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ccan/ciniparser/ciniparser.h>

#include "config.h"
#include "torch.h"

/* Decl for list of clients */
SLIST_HEAD(clientshead, clentry);
struct clentry {
	int			fd;
	char			addrtxt[INET6_ADDRSTRLEN];
	char			buf[1024];
	int			amt;
	SLIST_ENTRY(clentry)	entries;
};

static int doquit;

static void *		thr_torch(void *arg);
static int		opcconnect(const char *host, const char *port);
static int		createlisten(int listenport, int *listensock4, int *listensock6);
static char *		get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen);
static struct clentry *	findsock(int fd, struct clientshead *head);
static void		parseline(struct config_t *conf,char *cmd, const char *from);
static void		readfromsock(struct config_t *conf, int fd, struct clientshead *head, int *numclients);
static void		closesock(int fd, struct clientshead *head, int *numclients);

void
usage(const char *argv0)
{
	fprintf(stderr, "%s [-s server:port] [-c config]\n", argv0);
	fprintf(stderr, "\n");
	fprintf(stderr, "Generate message torch to OPC server:port\n");

	exit(EX_USAGE);
}

static void *
thr_torch(void *arg)
{
	static int rtn;
	sigset_t sigs;

	sigfillset(&sigs);
	sigdelset(&sigs, SIGTERM);
	if (pthread_sigmask(SIG_BLOCK, &sigs, NULL) != 0)
		warn("Unable to block signals");

	rtn = run_torch((struct config_t *)arg);

	return(&rtn);
}

static int
opcconnect(const char *host, const char *port)
{
	int opcsock, rtn;
	struct addrinfo addrhint, *res, *res0;
	char *cause;

	memset(&addrhint, 0, sizeof(addrhint));
	addrhint.ai_family = PF_UNSPEC;
#if 1
	addrhint.ai_socktype = SOCK_STREAM;
	addrhint.ai_protocol = IPPROTO_TCP;
#else
	addrhint.ai_socktype = SOCK_DGRAM;
	addrhint.ai_protocol = IPPROTO_UDP;
#endif
	if ((rtn = getaddrinfo(host, port, &addrhint, &res0)) != 0)
		err(EX_NOHOST, "Unable to resolve host: %s", gai_strerror(rtn));

	opcsock = -1;
	for (res = res0; res; res = res->ai_next) {
		opcsock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (opcsock < 0) {
			cause = "create socket";
			continue;
		}

		if (connect(opcsock, res->ai_addr, res->ai_addrlen) < 0) {
			cause = "connect";
			close(opcsock);
			opcsock = -1;
			continue;
		}
		break; /* Got one */
	}
	freeaddrinfo(res0);

	if (opcsock < 0)
		err(EX_NOHOST, "Unable to %s", cause);

	return(opcsock);
}

static int
createlisten(int listenport, int *listensock4, int *listensock6)
{
	struct sockaddr_in laddr4;
	struct sockaddr_in6 laddr6;
	int one = 1;

	if ((*listensock4 = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		warn("Unable to create IPv4 listen socket");
		return(-1);
	}
	if ((*listensock6 = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
		warn("Unable to create IPv6 listen socket");
		return(-1);
	}

	if (setsockopt (*listensock4, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
		warn("Unable to set SO_REUSEADDR on v4 socket");
	memset(&laddr4, 0, sizeof(laddr4));
	laddr4.sin_family = AF_INET;
	laddr4.sin_port = htons(listenport);
	laddr4.sin_addr.s_addr = INADDR_ANY;
	if ((bind(*listensock4, (struct sockaddr *)&laddr4, sizeof(laddr4))) == -1) {
		warn("Unable to bind to IPv4 address");
		return(-1);
	}

	if (setsockopt (*listensock6, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
		warn("Unable to set SO_REUSEADDR on v6 socket");
	memset(&laddr6, 0, sizeof(laddr6));
	laddr6.sin6_family = AF_INET6;
	laddr6.sin6_port = htons(listenport);
	laddr6.sin6_addr = in6addr_any;
	if ((bind(*listensock6, (struct sockaddr *)&laddr6, sizeof(laddr6))) == -1) {
		warn("Unable to bind to IPv6 address");
		return(-1);
	}
	if (listen(*listensock4, 5) < 0) {
		warn("Unable to listen to IPv4 socket");
		return(-1);
	}
	if (listen(*listensock6, 5) < 0) {
		/* If the system is set to bind v6 addresses when you bind v4 so just ignore the error */
		if (errno != EADDRINUSE) {
			warn("Unable to listen to IPv6 socket");
			return(-1);
		}
		close(*listensock6);
		*listensock6 = -1;
	}

	return(0);
}

/* Stolen from http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html */
static char *
get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{

	switch(sa->sa_family) {
		case AF_INET:
			inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
				  s, maxlen);
			break;

		case AF_INET6:
			inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
				  s, maxlen);
			break;

		default:
			strncpy(s, "Unknown AF", maxlen);
	}

	return s;
}

/* Search the list for the fd */
static struct clentry *
findsock(int fd, struct clientshead *head)
{
	struct clentry *clp;

	SLIST_FOREACH(clp, head, entries) {
		if (clp->fd == fd)
			return(clp);
	}

	return(NULL);
}

static void
parseline(struct config_t *conf, char *cmd, const char *from)
{
	char *t;

	t = strchr(cmd, '\n');
	if (t != NULL)
		*t = '\0';
	t = strchr(cmd, '\r');
	if (t != NULL)
		*t = '\0';

	if (!strcmp(cmd, "quit")) {
		doquit = 1;
		return;
	}
	cmd_torch(conf, from, cmd);
}

/* Add data to buffer for given fd */
static void
readfromsock(struct config_t *conf, int fd, struct clientshead *head, int *numclients)
{
	struct clentry *clp;
	int amt, r;

	clp = findsock(fd, head);
	assert(clp != NULL);
	amt = sizeof(clp->buf) - 1 - clp->amt;
	if ((r = read(fd, clp->buf + clp->amt, amt)) == -1) {
		warn("Unable to read from %s", clp->addrtxt);
		closesock(fd, head, numclients);
		return;
	}
	clp->amt += r;
	if (clp->amt == sizeof(clp->buf) - 1) {
		warnx("Line too long");
		closesock(fd, head, numclients);
		return;
	}
	if (strchr(clp->buf, '\n') != NULL) {
		parseline(conf, clp->buf, clp->addrtxt);
		closesock(fd, head, numclients);
	}
}

static void
closesock(int fd, struct clientshead *head, int *numclients)
{
	struct clentry *clp;

	clp = findsock(fd, head);
	if (clp == NULL)
		return;

	SLIST_REMOVE(head, clp, clentry, entries);
	close(clp->fd);
	(*numclients)--;
	warnx("Closed connection from %s", clp->addrtxt);

	free(clp);
}

int
main(int argc, char **argv)
{
	char *server = NULL;
	const char *argv0;
	int ch, i, j, listenport, listensock4, listensock6, opcsock, rtn;
	struct config_t conf;
	dictionary *ini;
	pthread_t torchthr;
	void *thrrtn;
	struct pollfd *fds;
	struct clentry *clp;
	int numclients = 0;
	struct option longopts[] = {
		{ "config",	required_argument,	NULL, 	'c' },
		{ "listen",	required_argument,	NULL,	'l' },
		{ "server",	required_argument,	NULL,	's' },
		{ NULL,		0,			NULL,	0 }
	};
	struct clientshead clients = SLIST_HEAD_INITIALIZER(clients);

	SLIST_INIT(&clients);
	rtn = 0;
	listenport = listensock4 = listensock6 = -1;
	argv0 = argv[0];

	default_conf(&conf);

	while ((ch = getopt_long(argc, argv, "c:l:s:", longopts, NULL)) != -1) {
		switch (ch) {
			case 'c':
				if ((ini = ciniparser_load(optarg)) == NULL)
					exit(EX_DATAERR);
				if (ini2conf(ini, &conf) != 0)
					exit(EX_DATAERR);
				break;

			case 'l':
				listenport = atoi(optarg);
				if (listenport <= 0 || listenport > 65535)
					errx(EX_DATAERR, "Listen port out of range");
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
		errx(EX_DATAERR, "A server name and port must be specified in the configuration file or on the command line\n");
	}
	if ((opcsock = opcconnect(conf.srvhost, conf.srvport)) == -1) {
		rtn = EX_OSERR;
		goto out;
	}

	if (listenport > 0) {
		if (createlisten(listenport, &listensock4, &listensock6) != 0) {
			rtn = EX_OSERR;
			goto out;
		}
	}

	if ((rtn = create_torch(opcsock, &conf)) != 0) {
		warnx("Failed to create: %d\n", rtn);
		rtn = EX_OSERR;
		goto out;
	}

	if (pthread_create(&torchthr, NULL, &thr_torch, &conf) != 0) {
		warnx("Failed to start thread\n");
		rtn = EX_OSERR;
		goto out;
	}

	/* Nothing to listen for so just wait for the thread to exit
	 * (which will never happen, so just sleep) */
	if (listenport == -1)
		goto wait;

	fds = NULL;
	doquit = 0;
	while (!doquit) {
		int numfds;
		numfds = numclients;
		if (listensock4 != -1)
			numfds++;
		if (listensock6 != -1)
			numfds++;
		fds = realloc(fds, sizeof(fds[0]) * numfds);
		j = 0;
		if (listensock4 != -1) {
			fds[j].fd = listensock4;
			fds[j].events = POLLRDNORM;
			fds[j++].revents = 0;
		}
		if (listensock6 != -1) {
			fds[j].fd = listensock6;
			fds[j].events = POLLRDNORM;
			fds[j++].revents = 0;
		}

		SLIST_FOREACH(clp, &clients, entries) {
			fds[j].fd = clp->fd;
			fds[j].events = POLLRDNORM;
			fds[j++].revents = 0;
		}
		assert(numfds == j);
		if (poll(fds, j, -1) == -1) {
			if (errno == EINTR)
				continue;
			warn("poll failed");
			break;
		}
		for (i = 0; i < numfds; i++) {
			/* Slot 0 & 1 are listen sockets, check for new connections */
			if (i < 2) {
				int tmpfd;
				socklen_t addrlen;
				struct sockaddr saddr;

				if (fds[i].revents & POLLRDNORM) {
					addrlen = sizeof(saddr);
					memset(&saddr, 0, sizeof(saddr));
					if ((tmpfd = accept(fds[i].fd, &saddr, &addrlen)) == -1) {
						warn("Unable to accept new connection");
						continue;
					}
					if ((clp = calloc(1, sizeof(*clp))) == NULL) {
						warnx("Can't allocate listener");
						continue;
					}

					clp->fd = tmpfd;
					get_ip_str(&saddr, clp->addrtxt, sizeof(clp->addrtxt));
					warnx("Accepted new connection from %s",
						clp->addrtxt);
					SLIST_INSERT_HEAD(&clients, clp, entries);
					numclients++;
				}
				if (fds[i].revents & (POLLERR | POLLHUP)) {
					warnx("v%s socket error", i == 0 ? "4" : "6");
					continue;
				}
			} else {
				/* See if our clients have anything to say */
				if (fds[i].revents & POLLRDNORM) {
					readfromsock(&conf, fds[i].fd, &clients, &numclients);
				}
				if (fds[i].revents & (POLLERR | POLLHUP)) {
					closesock(fds[i].fd, &clients, &numclients);
				}
			}
		}
	}
	fprintf(stderr, "before\n");
	pthread_kill(torchthr, SIGTERM);
	fprintf(stderr, "after\n");
	
  wait:
	pthread_join(torchthr, &thrrtn);
	fprintf(stderr, "Torch thread returned %p\n", thrrtn);

  out:
	free_torch();
	ciniparser_freedict(ini);
	close(opcsock);
	close(listensock4);
	close(listensock6);

	return(rtn);
}
