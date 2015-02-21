#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // select
#include <arpa/inet.h>  // sockaddr_in
#include "lssdp.h"

/* socket_listener.c
 *
 * 1. create SSDP socket with port 1900
 * 2. select SSDP socket with timeout 1 seconds
 * 3. when select return value > 0, read socket and show received data
 */

int log_callback(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) {
    printf("[%s][%s] %s: %s", level, tag, func, message);
    return 0;
}

int main() {
    lssdp_set_log_callback(log_callback);

    lssdp_ctx lssdp = {
        .sock = -1,
        .port = 1900
    };

    if (lssdp_create_socket(&lssdp) != 0) {
        puts("SSDP create socket failed");
        return -1;
    }

    printf("SSDP socket = %d\n", lssdp.sock);

    for (;;) {
        fd_set fs;
        FD_ZERO(&fs);
        FD_SET(lssdp.sock, &fs);
        struct timeval tv = {
            .tv_sec = 1     // 1 seconds
        };

        int ret = select(lssdp.sock + 1, &fs, NULL, NULL, &tv);
        if (ret < 0) {
            printf("select error, ret = %d\n", ret);
            break;
        }

        if (ret == 0) {
            puts("select timeout");
            continue;
        }

        char buffer[2048] = {};
        struct sockaddr_in address = {};
        socklen_t address_len = sizeof(struct sockaddr_in);
        ssize_t recv_len = recvfrom(lssdp.sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&address, &address_len);

        printf("Received Packet (%zd):\n%s\n", recv_len, buffer);
    }

    return EXIT_SUCCESS;
}
