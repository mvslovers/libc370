#ifndef NETINET_IN_H
#define NETINET_IN_H
#include <sys/_cc370.h>
/* netinet/in.h - POSIX <netinet/in.h>: IPv4 addresses, ports, protocols.
**
** libc370 2.0 splits this out of socket.h (#256).
*/

/* in_addr_t - an IPv4 address in network byte order (POSIX: an unsigned
** integer of 32 bits).  unsigned long is 32 bits here and is the type s_addr
** has always had; 1.x made in_addr_t the struct itself (#51). */
typedef unsigned long   in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

#define INET_ADDRSTRLEN 16  /* "255.255.255.255" and its NUL            */

struct sockaddr_in {
    short   sin_family;
    unsigned short sin_port;
    struct  in_addr sin_addr;
    char    sin_zero [8];
};

#define IPPROTO_IP      0   /* Dummy protocol for TCP               */
#define IPPROTO_ICMP    1   /* Internet Control Message Protocol    */
#define IPPROTO_IGMP    2   /* Internet Group Management Protocol   */
#define IPPROTO_IPIP    4   /* IPIP tunnels (older KA9Q tunnels use 94) */
#define IPPROTO_TCP     6   /* Transmission Control Protocol        */
#define IPPROTO_EGP     8   /* Exterior Gateway Protocol            */
#define IPPROTO_PUP     12  /* PUP protocol                         */
#define IPPROTO_UDP     17  /* User Datagram Protocol               */
#define IPPROTO_IDP     22  /* XNS IDP protocol                     */
#define IPPROTO_TP      29  /* SO Transport Protocol Class 4        */
#define IPPROTO_DCCP    33  /* Datagram Congestion Control Protocol */
#define IPPROTO_IPV6    41  /* IPv6-in-IPv4 tunnelling              */
#define IPPROTO_RSVP    46  /* RSVP Protocol                        */
#define IPPROTO_GRE     47  /* Cisco GRE tunnels (rfc 1701,1702)    */
#define IPPROTO_ESP     50  /* Encapsulation Security Payload protocol */
#define IPPROTO_AH      51  /* Authentication Header protocol       */
#define IPPROTO_MTP     92  /* Multicast Transport Protocol         */
#define IPPROTO_BEETPH  94  /* IP option pseudo header for BEET     */
#define IPPROTO_ENCAP   98  /* Encapsulation Header                 */
#define IPPROTO_PIM     103 /* Protocol Independent Multicast       */
#define IPPROTO_COMP    108 /* Compression Header Protocol          */
#define IPPROTO_SCTP    132 /* Stream Control Transport Protocol    */
#define IPPROTO_UDPLITE 136 /* UDP-Lite (RFC 3828)                  */
#define IPPROTO_MPLS    137 /* MPLS in IP (RFC 4023)                */
#define IPPROTO_RAW     255 /* Raw IP packets                       */

#define INADDR_ANY  (unsigned)0x00000000
#define INADDR_NONE (unsigned)0xFFFFFFFF

#define ntohl(x) (x)
#define ntohs(x) (x)
#define htonl(x) (x)
#define htons(x) (x)
#define NTOHL(x) (x)
#define NTOHS(x) (x)
#define HTONL(x) (x)
#define HTONS(x) (x)

#endif /* NETINET_IN_H */
