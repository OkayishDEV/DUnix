#ifndef _LIBC_NETINET_IN_H
#define _LIBC_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

#define IPPROTO_IP   0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17
#define IPPROTO_RAW  255

#define INADDR_ANY       0x00000000
#define INADDR_LOOPBACK  0x7F000001
#define INADDR_BROADCAST 0xFFFFFFFF
#define INADDR_NONE      0xFFFFFFFF

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    uint16_t       sin_family;
    in_port_t      sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};

#endif /* _LIBC_NETINET_IN_H */
