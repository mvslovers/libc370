/* TIMECLI: ask a time server for the time of day */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mvs/socket.h>
#include "asc.h"

int main(int argc, char **argv)
{
    ASCTAB             asc;
    struct sockaddr_in sin;
    char               buf[128];
    int                got = 0;
    int                n, s;

    if (argc < 2) {
        fprintf(stderr, "usage: TIMECLI host [port]\n");
        return 4;
    }
    asc_init(&asc);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port   = htons(argc > 2 ? atoi(argv[2]) : 7008);
    if (!inet_aton(argv[1], &sin.sin_addr)) {
        /* not an address: ask the host's resolver */
        sin.sin_addr.s_addr = getaddrbyname(argv[1]);
        if (sin.sin_addr.s_addr == 0) {
            fprintf(stderr, "host %s not found\n", argv[1]);
            return 8;
        }
    }

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        perror("socket");
        return 8;
    }
    if (connect(s, &sin, sizeof(sin)) < 0) {
        perror("connect");
        closesocket(s);
        return 8;
    }

    /* The server closes the connection after its line, so a blocking
       recv() returns when the line is complete. */
    while (got < (int)sizeof(buf) - 1) {
        n = recv(s, buf + got, sizeof(buf) - 1 - got, 0);
        if (n <= 0)
            break;
        got += n;
    }
    closesocket(s);

    asc_in(&asc, buf, got);       /* ASCII -> EBCDIC */
    buf[got] = '\0';
    printf("%s", buf);
    return got > 0 ? 0 : 8;
}
