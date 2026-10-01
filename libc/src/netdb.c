#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <ctype.h>

static struct hostent g_hostent;
static char g_host_name[128];
static uint32_t g_host_addr;
static char *g_addr_list[2];

struct __attribute__((packed)) dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

static struct {
    const char *domain;
    const char *ip;
} static_hosts[] = {
    {"localhost",   "127.0.0.1"},
    {"dunix",       "10.0.2.15"},
    {"gateway",     "10.0.2.2"},
    {"dns.google",  "8.8.8.8"},
    {"google.com",  "142.251.34.238"},
    {"www.google.com", "142.251.34.238"},
    {"example.com", "93.184.216.34"},
    {"github.com",  "140.82.121.4"},
    {"gnu.org",     "209.51.188.148"},
    {NULL, NULL}
};

static in_addr_t lookup_etc_hosts(const char *name) {
    int fd = open("/etc/hosts", O_RDONLY, 0);
    if (fd < 0) return INADDR_NONE;

    char buf[1024];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) return INADDR_NONE;

    char *line = buf;
    while (*line) {
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        if (*line == '#' || *line == '\0') {
            while (*line && *line != '\n') line++;
            continue;
        }

        char ip_str[64];
        char host_str[128];
        int i = 0;
        while (*line && !isspace((unsigned char)*line) && i < 63) {
            ip_str[i++] = *line++;
        }
        ip_str[i] = '\0';

        while (*line == ' ' || *line == '\t') line++;

        i = 0;
        while (*line && !isspace((unsigned char)*line) && i < 127) {
            host_str[i++] = *line++;
        }
        host_str[i] = '\0';

        if (strcmp(host_str, name) == 0) {
            return inet_addr(ip_str);
        }

        while (*line && *line != '\n') line++;
    }

    return INADDR_NONE;
}

static in_addr_t query_dns(const char *name) {
    uint8_t packet[512];
    memset(packet, 0, sizeof(packet));

    struct dns_header *hdr = (struct dns_header *)packet;
    hdr->id = htons(0x4321);
    hdr->flags = htons(0x0100); /* Standard query, RD=1 */
    hdr->qdcount = htons(1);

    /* Encode hostname into QNAME format: e.g. "google.com" -> [6]google[3]com[0] */
    uint8_t *qname = packet + sizeof(struct dns_header);
    const char *curr = name;
    while (*curr) {
        const char *dot = strchr(curr, '.');
        size_t len = dot ? (size_t)(dot - curr) : strlen(curr);
        *qname++ = (uint8_t)len;
        memcpy(qname, curr, len);
        qname += len;
        if (!dot) break;
        curr = dot + 1;
    }
    *qname++ = 0; /* Null root terminator */

    /* QTYPE = 1 (A), QCLASS = 1 (IN) */
    *qname++ = 0x00; *qname++ = 0x01;
    *qname++ = 0x00; *qname++ = 0x01;

    size_t packet_len = (size_t)(qname - packet);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return INADDR_NONE;

    struct sockaddr_in dns_addr;
    memset(&dns_addr, 0, sizeof(dns_addr));
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(53);
    dns_addr.sin_addr.s_addr = inet_addr("10.0.2.3"); /* QEMU Virtual DNS */

    sendto(sockfd, packet, packet_len, 0, (struct sockaddr *)&dns_addr, sizeof(dns_addr));

    uint8_t resp[512];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    ssize_t rlen = recvfrom(sockfd, resp, sizeof(resp), 0, (struct sockaddr *)&from_addr, &from_len);
    close(sockfd);

    if (rlen > (ssize_t)sizeof(struct dns_header)) {
        struct dns_header *rhdr = (struct dns_header *)resp;
        if (ntohs(rhdr->ancount) > 0) {
            /* Parse answer */
            size_t idx = packet_len; /* Skip question */
            while (idx + 12 <= (size_t)rlen) {
                /* Check if Name pointer */
                if ((resp[idx] & 0xC0) == 0xC0) {
                    idx += 2;
                } else {
                    while (idx < (size_t)rlen && resp[idx] != 0) idx += resp[idx] + 1;
                    idx++;
                }

                if (idx + 10 > (size_t)rlen) break;
                uint16_t type = (uint16_t)((resp[idx] << 8) | resp[idx + 1]);
                idx += 8; /* Type(2), Class(2), TTL(4) */
                uint16_t rdlen = (uint16_t)((resp[idx] << 8) | resp[idx + 1]);
                idx += 2;

                if (type == 1 && rdlen == 4 && idx + 4 <= (size_t)rlen) {
                    uint32_t ip;
                    memcpy(&ip, resp + idx, 4);
                    return ip;
                }
                idx += rdlen;
            }
        }
    }

    return INADDR_NONE;
}

struct hostent *gethostbyname(const char *name) {
    if (!name) return NULL;

    in_addr_t addr = inet_addr(name);

    if (addr == INADDR_NONE) {
        /* Step 1: Check /etc/hosts */
        addr = lookup_etc_hosts(name);
    }

    if (addr == INADDR_NONE) {
        /* Step 2: Query DNS over UDP */
        addr = query_dns(name);
    }

    if (addr == INADDR_NONE) {
        /* Step 3: Check Static Fallback Table */
        for (int i = 0; static_hosts[i].domain; i++) {
            if (strcmp(static_hosts[i].domain, name) == 0) {
                addr = inet_addr(static_hosts[i].ip);
                break;
            }
        }
    }

    if (addr == INADDR_NONE) {
        return NULL;
    }

    strncpy(g_host_name, name, sizeof(g_host_name) - 1);
    g_host_addr = addr;
    g_addr_list[0] = (char *)&g_host_addr;
    g_addr_list[1] = NULL;

    g_hostent.h_name = g_host_name;
    g_hostent.h_aliases = NULL;
    g_hostent.h_addrtype = AF_INET;
    g_hostent.h_length = sizeof(uint32_t);
    g_hostent.h_addr_list = g_addr_list;

    return &g_hostent;
}
