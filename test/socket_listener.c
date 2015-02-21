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
 * 3. when select return value > 0, call function lssdp_read_socket
 * 4. data will be return in sspd_data_callback
 */

int log_callback(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) {
    printf("[%s][%s] %s: %s", level, tag, func, message);
    return 0;
}

int ssdp_data_callback(const lssdp_ctx * lssdp, const char * data, size_t data_len) {
    printf("%s\n", data);
    return 0;
}

int main() {
    lssdp_set_log_callback(log_callback);

    lssdp_ctx lssdp = {
        .sock = -1,
        .port = 1900,
        .data_callback = ssdp_data_callback
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

        lssdp_read_socket(&lssdp);
    }

    return EXIT_SUCCESS;
}
