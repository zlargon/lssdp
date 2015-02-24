#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "lssdp.h"

/* interface_monitor.c
 *
 * 1. update interface per 3 seconds
 * 2. if network interface is changed, show the changed message
 */

int log_callback(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) {
    printf("[%s][%s] %s: %s", level, tag, func, message);
    return 0;
}

int main() {
    lssdp_set_log_callback(log_callback);

    lssdp_ctx lssdp = {};
    for (;; sleep(3)) {

        const size_t SIZE_OF_LIST = sizeof(struct lssdp_interface) * LSSDP_INTERFACE_LIST_SIZE;

        // copy ssdp interface
        struct lssdp_interface interface[LSSDP_INTERFACE_LIST_SIZE] = {};
        memcpy(interface, lssdp.interface, SIZE_OF_LIST);

        // get network interface
        if (lssdp_get_network_interface(&lssdp) != 0) {
            puts("get network interface failed");
            continue;
        }

        // compare interface
        if (memcmp(interface, lssdp.interface, SIZE_OF_LIST) != 0) {
            puts("* Network Interface is changed!");
        }

        // show network interface
        size_t i;
        for (i = 0; i < LSSDP_INTERFACE_LIST_SIZE; i++) {
            // no more network interface
            if (strlen(lssdp.interface[i].name) == 0) {
                break;
            }

            printf("%s : %s\n", lssdp.interface[i].name, lssdp.interface[i].ip);
        }
        puts("");
    }

    return EXIT_SUCCESS;
}
