/* TIMESRV: send the time of day to each client, then close */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mvs/socket.h>
#include "asc.h"

#define PORT 7008

static int send_all(int s, const char *buf, int len)
{
    int n;

    while (len > 0) {
        n = send(s, buf, len, 0);
        if (n < 0)
            return -1;
        buf += n;
        len -= n;
    }
    return 0;
}

int main(int argc, char **argv)
{
    ASCTAB             asc;        /* 512 bytes, on the stack */
    struct sockaddr_in sin;
    struct sockaddr_in peer;
    char               addr[INET_ADDRSTRLEN];
    char               line[64];
    int                count = argc > 1 ? atoi(argv[1]) : 1;
    int                served, len, ls, s;
    time_t             now;

    asc_init(&asc);

    ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls < 0) {
        perror("socket");
        return 8;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(PORT);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ls, &sin, sizeof(sin)) < 0 || listen(ls, 5) < 0) {
        perror("bind/listen");
        closesocket(ls);           /* the port stays taken otherwise */
        return 8;
    }
    printf("TIMESRV listening on port %d\n", PORT);

    for (served = 0; served < count; served++) {
        len = sizeof(peer);
        s = accept(ls, &peer, &len);
        if (s < 0) {
            perror("accept");
            break;
        }
        inet_ntop(AF_INET, &peer.sin_addr, addr, sizeof(addr));
        printf("connection from %s\n", addr);

        time(&now);
        len = strftime(line, sizeof(line), "%Y-%m-%d %H:%M:%S\r\n",
                       localtime(&now));
        asc_out(&asc, line, len);  /* EBCDIC -> ASCII, CR LF included */
        if (send_all(s, line, len) < 0)
            perror("send");
        closesocket(s);
    }

    closesocket(ls);
    return served == count ? 0 : 8;
}
