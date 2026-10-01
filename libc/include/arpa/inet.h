#ifndef _LIBC_ARPA_INET_H
#define _LIBC_ARPA_INET_H

#include <stdint.h>
#include <netinet/in.h>

static inline uint16_t htons(uint16_t hostshort) {
    return (uint16_t)((hostshort << 8) | (hostshort >> 8));
}

static inline uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);
}

static inline uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0x000000FF) << 24) |
           ((hostlong & 0x0000FF00) << 8)  |
           ((hostlong & 0x00FF0000) >> 8)  |
           ((hostlong & 0xFF000000) >> 24);
}

static inline uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

in_addr_t inet_addr(const char *cp);
char *inet_ntoa(struct in_addr in);

#endif /* _LIBC_ARPA_INET_H */
