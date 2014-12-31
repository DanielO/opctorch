PROG=	opctorch

SRCS=	main.c \
	torch.c

CINIPARSER= ${.CURDIR}/ccan/ciniparser
.PATH:	${CINIPARSER}
SRCS+=	dictionary.c \
	ciniparser.c
CFLAGS+=-I${.CURDIR}

CFLAGS+=-g -Wall -Werror -O0
LDFLAGS+=-lpthread
NO_MAN=

.include <bsd.prog.mk>
