#ifndef _LIBC_NETDB_H
#define _LIBC_NETDB_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct hostent {
    char  *h_name;
    char **h_aliases;
    int    h_addrtype;
    int    h_length;
    char **h_addr_list;
};

#define h_addr h_addr_list[0]

struct hostent *gethostbyname(const char *name);

#endif /* _LIBC_NETDB_H */
