/* ECHO: accept one connection on port 7007 and send back what arrives */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mvs/socket.h>

int main(void)
{
    struct sockaddr_in  sin;
    struct sockaddr_in  peer;
    int                 len = sizeof(peer);
    int                 on  = 1;
    int                 ls, s, n;
    fd_set              rd;
    char                buf[256];

    ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls < 0) {
        perror("socket");
        return 8;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(7007);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ls, &sin, sizeof(sin)) < 0 || listen(ls, 5) < 0) {
        perror("bind/listen");
        closesocket(ls);
        return 8;
    }

    s = accept(ls, &peer, &len);
    if (s < 0) {
        perror("accept");
        closesocket(ls);
        return 8;
    }
    printf("connection from %s\n", inet_ntoa(peer.sin_addr));

    /* non-blocking: recv() returns what has arrived */
    ioctlsocket(s, FIONBIO, &on);

    for (;;) {
        memset(&rd, 0, sizeof(rd));
        FD_SET(s, &rd);
        if (select(s + 1, &rd, NULL, NULL, NULL) < 0)
            break;
        n = recv(s, buf, sizeof(buf), 0);
        if (n < 0 && errno == EWOULDBLOCK)
            continue;
        if (n <= 0)
            break;                      /* closed by the peer, or an error */
        if (send(s, buf, n, 0) < 0)
            break;
    }

    closesocket(s);
    closesocket(ls);
    return 0;
}
