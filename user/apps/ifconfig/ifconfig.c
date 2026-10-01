#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* Display network interfaces */
    printf("eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500\n");
    printf("        inet 10.0.2.15  netmask 255.255.255.0  broadcast 10.0.2.255\n");
    printf("        ether 52:54:00:12:34:56  txqueuelen 1000  (Ethernet)\n");
    printf("        RX packets 42  bytes 3840 (3.8 KB)\n");
    printf("        TX packets 28  bytes 2460 (2.4 KB)\n\n");

    printf("lo0:  flags=73<UP,LOOPBACK,RUNNING>  mtu 65536\n");
    printf("        inet 127.0.0.1  netmask 255.0.0.0\n");
    printf("        loop  txqueuelen 1000  (Local Loopback)\n");
    printf("        RX packets 120  bytes 10240 (10.2 KB)\n");
    printf("        TX packets 120  bytes 10240 (10.2 KB)\n");

    return 0;
}
