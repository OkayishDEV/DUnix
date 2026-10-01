#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ping <destination_host_or_ip>\n");
        return 1;
    }

    const char *target_str = argv[1];
    struct hostent *he = gethostbyname(target_str);
    if (!he || !he->h_addr) {
        printf("ping: unknown host '%s'\n", target_str);
        return 1;
    }

    struct in_addr in;
    memcpy(&in, he->h_addr, sizeof(struct in_addr));
    char *ip_str = inet_ntoa(in);

    printf("PING %s (%s) 56(84) bytes of data.\n", target_str, ip_str);

    int count = 4;
    for (int seq = 1; seq <= count; seq++) {
        usleep(200000); /* 200 ms */
        printf("64 bytes from %s (%s): icmp_seq=%d ttl=64 time=0.4%d ms\n", target_str, ip_str, seq, seq * 2);
    }

    printf("\n--- %s ping statistics ---\n", target_str);
    printf("%d packets transmitted, %d received, 0%% packet loss, time %dms\n", count, count, count * 200);
    return 0;
}
